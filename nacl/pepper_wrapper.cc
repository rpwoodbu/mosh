// Copyright 2013 Richard Woodbury
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "pepper_posix.h"

#include <deque>
#include <string>
#include <vector>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <langinfo.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

using ::std::string;
using ::std::vector;

// Forward declaration of mosh_main(), as it has no header file.
int mosh_main(int argc, char* argv[]);

// Make the singleton MoshClientInstance available to C functions in this
// module.
static class MoshClientInstance* instance = NULL;

// TODO: Eliminate this debugging hack.
void NaClDebug(const char* fmt, ...) {
  char buf[256];
  va_list argp;
  va_start(argp, fmt);
  vsnprintf(buf, sizeof(buf), fmt, argp);
  buf[sizeof(buf)-1] = 0;
  fprintf(stderr, "%s\n", buf);
  //instance->PostMessage(pp::Var(buf));
}

// Implements most of the plumbing to get keystrokes to Mosh. A tiny amount of
// plumbing is in the MoshClientInstance::HandleMessage().
class Keyboard : public PepperPOSIX::Reader {
 public:
  Keyboard() : Reader(NULL) {
    pthread_mutex_init(&keypresses_lock_, NULL);
  }
  virtual ~Keyboard() {
    pthread_mutex_destroy(&keypresses_lock_);
  }

  virtual ssize_t Read(void* buf, size_t count) {
    // TODO: Could submit in batches, but rarely will get in batches.
    int result = 0;
    pthread_mutex_lock(&keypresses_lock_);
    if (keypresses_.size() > 0) {
      ((char *)buf)[0] = keypresses_.front();
      keypresses_.pop_front();
      target_->Update(keypresses_.size() > 0);
      result = 1;
    } else {
      NaClDebug("read(): From STDIN, no data, treat as nonblocking.");
    }
    pthread_mutex_unlock(&keypresses_lock_);
    return result;
  }

  // Handle input from the keyboard.
  void HandleInput(string input) {
    pthread_mutex_lock(&keypresses_lock_);
    for (int i = 0; i < input.size(); ++i) {
      keypresses_.push_back(input[i]);
    }
    pthread_mutex_unlock(&keypresses_lock_);
    target_->Update(true);
  }

  // Used as STDIN; should not be closed.
  virtual int Close() { return 0; };

 private:
  // Queue of keyboard keypresses.
  std::deque<char> keypresses_; // Guard with keypresses_lock_.
  pthread_mutex_t keypresses_lock_;
};

// Implements the plumbing to get SIGWINCH to Mosh. A tiny amount of plumbing
// is in MoshClientInstance::HandleMessage();
class WindowChange : public PepperPOSIX::Signal {
 public:
  WindowChange() : Signal(NULL), sigwinch_handler_(NULL),
      width_(80), height_(24) {}

  // Update geometry and send SIGWINCH.
  void Update(int width, int height) {
    width_ = width;
    height_ = height;
    target_->Update(true);
  }

  void SetHandler(void (*sigwinch_handler)(int)) {
    sigwinch_handler_ = sigwinch_handler;
  }

  virtual void Handle() {
    sigwinch_handler_(SIGWINCH);
    target_->Update(false);
  }

  // Doesn't really make sense to close this, but required to implemenet.
  virtual int Close() { return 0; };

  int height() { return height_; }
  int width() { return width_; }

 private:
  int width_;
  int height_;
  void (*sigwinch_handler_)(int);
};

class MoshClientInstance : public pp::Instance {
 public:
  explicit MoshClientInstance(PP_Instance instance) :
      pp::Instance(instance), addr_(NULL), port_(NULL),
      posix_(NULL), keyboard_(NULL), instance_handle_(this),
      window_change_(NULL) {
    ++num_instances_;
    assert (num_instances_ == 1);
    ::instance = this;
  }

  virtual ~MoshClientInstance() {
    // Wait for thread to finish.
    if (thread_) {
      pthread_join(thread_, NULL);
    }
    delete[] addr_;
    delete[] port_;
    delete posix_;
  }

  virtual void HandleMessage(const pp::Var& var) {
    if (var.is_string()) {
      string s = var.AsString();
      keyboard_->HandleInput(s);
    } else if (var.is_number()) {
      int32_t num = var.AsInt();
      window_change_->Update(num >> 16, num & 0xffff);
    } else {
      fprintf(stderr, "Got a message of an unexpected type.\n");
    }
  }

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    for (int i = 0; i < argc; ++i) {
      string name = argn[i];
      int len = strlen(argv[i]) + 1;
      if (name == "key") {
        setenv("MOSH_KEY", argv[i], 1);
      } else if (name == "addr" && addr_ == NULL) {
        addr_ = new char[len];
        strncpy(addr_, argv[i], len);
      } else if (name == "port" && port_ == NULL) {
        port_ = new char[len];
        strncpy(port_, argv[i], len);
      }
    }

    if (addr_ == NULL || port_ == NULL) {
      fprintf(stderr, "Must supply addr and port attributes.\n");
      return false;
    }

    // Setup communications.
    keyboard_ = new Keyboard();
    window_change_ = new WindowChange();
    posix_ = new PepperPOSIX::POSIX(
        instance_handle_, keyboard_, NULL, NULL, window_change_);

    // Launch mosh-client.
    pthread_create(&thread_, NULL, &Launch, this);
    return true;
  }

  static void* Launch(void* data) {
    MoshClientInstance* thiz = reinterpret_cast<MoshClientInstance*>(data);

    setenv("TERM", "xterm-256color", 1);
    char* argv[] = { "mosh-client", thiz->addr_, thiz->port_ };
    mosh_main(sizeof(argv) / sizeof(argv[0]), argv);
    thiz->PostMessage(pp::Var("Mosh has exited."));
    return 0;
  }

  // Pepper POSIX emulation.
  PepperPOSIX::POSIX *posix_;
  // Window change "file"; must be visible to sigaction().
  WindowChange *window_change_;

 private:
  static int num_instances_; // This needs to be a singleton.
  pthread_t thread_;
  // Non-const params for mosh_main().
  char* addr_;
  char* port_;
  pp::InstanceHandle instance_handle_;
  Keyboard *keyboard_;
};

// Initialize static data for MoshClientInstance.
int MoshClientInstance::num_instances_ = 0;

class MoshClientModule : public pp::Module {
 public:
  MoshClientModule() : pp::Module() {}
  virtual ~MoshClientModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new MoshClientInstance(instance);
  }
};

//
// Implement stubs and overrides for various C library functions.
//
extern "C" {

// These are used to avoid CORE dumps. Should be OK to stub them out.
int getrlimit(int resource, struct rlimit* rlim) {
  return 0;
}
int setrlimit(int resource, const struct rlimit* rlim) {
  return 0;
}

int sigaction(int signum, const struct sigaction* act,
    struct sigaction* oldact) {
  NaClDebug("sigaction(%d, ...)", signum);
  assert(oldact == NULL);
  switch (signum) {
  case SIGWINCH:
    instance->window_change_->SetHandler(act->sa_handler);
    break;
  }

  return 0;
}

int sigprocmask(int how, const sigset_t* set, sigset_t* oldset) {
  return 0;
}

// kill() is used to send a SIGSTOP on Ctrl-Z, which is not useful for NaCl.
int kill(pid_t pid, int sig) {
  return 0;
}

// getpid() isn't actually called, but it is annoying to see a linker warning
// about it not being implemented.
pid_t getpid(void) {
  return 0;
}

// TODO: Determine if there's a better way than stubbing these out.
char* setlocale(int category, const char* locale) {
  return "NaCl";
}
char* nl_langinfo(nl_item item) {
  switch (item) {
    case CODESET:
      return "UTF-8";
    default:
      return "Error";
  }
}

// We don't really care about terminal attributes.
int tcgetattr(int fd, struct termios* termios_p) {
  return 0;
}
int tcsetattr(int fd, int optional_sctions, const struct termios* termios_p) {
  return 0;
}

int ioctl(int d, long unsigned int request, ...) {
  if (d != STDIN_FILENO || request != TIOCGWINSZ) {
    NaClDebug("ioctl(%d, %u, ...): Got unexpected call", d, request);
    errno = EPROTO;
    return -1;
  }
  va_list argp;
  va_start(argp, request);
  struct winsize* ws = va_arg(argp, struct winsize*);
  ws->ws_row = instance->window_change_->height();
  ws->ws_col = instance->window_change_->width();
  va_end(argp);
  return 0;
}

//
// Wrap all unistd functions to communicate via the Pepper API.
//

// TODO: Determine if it is necessary to emulate /dev/urandom.

ssize_t read(int fd, void *buf, size_t count) {
  return instance->posix_->Read(fd, buf, count);
}

ssize_t write(int fd, const void *buf, size_t count) {
 switch (fd) {
 case 1: // STDOUT
   string b((const char *)buf, count);
   instance->PostMessage(pp::Var(b));
   return count;
 }

 NaClDebug("write(%d, ...): Unexpected write.", fd);
 errno = EIO;
 return -1;
}

int close(int fd) {
  return instance->posix_->Close(fd);
}

int socket(int domain, int type, int protocol) {
  return instance->posix_->Socket(domain, type, protocol);
}

int setsockopt(int sockfd, int level, int optname,
    const void* optval, socklen_t optlen) {
  NaClDebug("setsockopt stub called; fd=%d", sockfd);
  return 0;
}

int dup(int oldfd) {
  return instance->posix_->Dup(oldfd);
}

int pselect(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds,
    const struct timespec* timeout, const sigset_t* sigmask) {
  return instance->posix_->PSelect(
     nfds, readfds, writefds, exceptfds, timeout, sigmask);
}

ssize_t recvmsg(int sockfd, struct msghdr* msg, int flags) {
  return instance->posix_->RecvMsg(sockfd, msg, flags);
}

ssize_t sendto(int sockfd, const void* buf, size_t len, int flags,
    const struct sockaddr* dest_addr, socklen_t addrlen) {
  return instance->posix_->SendTo(sockfd, buf, len, flags, dest_addr, addrlen);
}

} // extern "C"

namespace pp {
Module* CreateModule() {
  return new MoshClientModule();
}
} // namespace pp
