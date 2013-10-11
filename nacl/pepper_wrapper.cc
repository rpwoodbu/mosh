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

#include "pepper_posix_selector.h"
#include "pepper_posix_native_udp.h"

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

#include "ppapi/c/ppb_net_address.h"
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

// Enum for identifying different Targets.
enum TARGETS {
  TARGET_KEYBOARD,
  TARGET_UDP,
  TARGET_WINDOW_CHANGE,
};

class MoshClientInstance : public pp::Instance {
 public:
  explicit MoshClientInstance(PP_Instance instance) :
      pp::Instance(instance), addr_(NULL), port_(NULL),
      udp_(NULL), selector_(NULL), instance_handle_(this),
      sigwinch_handler_(NULL), width_(80), height_(24) {
    ++num_instances_;
    assert (num_instances_ == 1);
    ::instance = this;
    pthread_mutex_init(&keypresses_lock_, NULL);
  }

  virtual ~MoshClientInstance() {
    // Wait for thread to finish.
    if (thread_) {
      pthread_join(thread_, NULL);
    }
    delete[] addr_;
    delete[] port_;
    delete udp_;
    delete keyboard_target_;
    delete selector_;
    pthread_mutex_destroy(&keypresses_lock_);
  }

  virtual void HandleMessage(const pp::Var& var) {
    if (var.is_string()) {
      string s = var.AsString();
      pthread_mutex_lock(&keypresses_lock_);
      for (int i = 0; i < s.size(); ++i) {
        keypresses_.push_back(s[i]);
      }
      pthread_mutex_unlock(&keypresses_lock_);
      keyboard_target_->Update(true);
    } else if (var.is_number()) {
      int32_t num = var.AsInt();
      width_ = num >> 16;
      height_ = num & 0xffff;
      window_change_target_->Update(true);
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
    selector_ = new PepperPOSIX::Selector();
    udp_ = new PepperPOSIX::NativeUDP(
        instance_handle_, selector_->NewTarget(TARGET_UDP));
    keyboard_target_ = selector_->NewTarget(TARGET_KEYBOARD);
    window_change_target_ = selector_->NewTarget(TARGET_WINDOW_CHANGE);

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

  // Selector object that stubs will use.
  PepperPOSIX::Selector *selector_;
  // UDP object that stubs will use.
  PepperPOSIX::UDP *udp_;
  // Target for managing keyboard input.
  PepperPOSIX::Target *keyboard_target_;
  // Target for handling window size changes.
  PepperPOSIX::Target *window_change_target_;
  // Queue of keyboard keypresses.
  std::deque<char> keypresses_; // Guard with keypresses_lock_.
  pthread_mutex_t keypresses_lock_;
  // Handler for window size changes.
  void (*sigwinch_handler_)(int);
  // Size of window.
  int width_, height_;

 private:
  static int num_instances_; // This needs to be a singleton.
  pthread_t thread_;
  // Non-const params for mosh_main().
  char* addr_;
  char* port_;
  pp::InstanceHandle instance_handle_;
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

// TODO: Eliminate this debugging hack.
void NaClDebug(const char* fmt, ...) {
  char buf[256];
  va_list argp;
  va_start(argp, fmt);
  vsnprintf(buf, sizeof(buf), fmt, argp);
  buf[sizeof(buf)-1] = 0;
  //instance->PostMessage(pp::Var(buf));
}

// Make a PP_NetAddress_IPv4 from sockaddr.
void MakeAddress(const struct sockaddr* addr, socklen_t addrlen,
    PP_NetAddress_IPv4* pp_addr) {
  // TODO: Make an IPv6 version, but since mosh doesn't support it now, this
  // will do.
  assert(addr->sa_family == AF_INET);
  assert(addrlen >= 4);
  const struct sockaddr_in* in_addr = (struct sockaddr_in*)addr;
  uint32_t a = in_addr->sin_addr.s_addr;
  for (int i = 0; i < 4; ++i) {
    pp_addr->addr[i] = a & 0xff;
    a >>= 8;
  }
  pp_addr->port = in_addr->sin_port;
}

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

// TODO: sigaction is used for important things. This will need to be
// rethought. For now, stub it out to see how far it'll go without it.
int sigaction(int signum, const struct sigaction* act,
    struct sigaction* oldact) {
  NaClDebug("sigaction(%d, ...)", signum);
  assert(oldact == NULL);
  switch (signum) {
  case SIGWINCH:
    instance->sigwinch_handler_ = act->sa_handler;
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
  ws->ws_row = instance->height_;
  ws->ws_col = instance->width_;
  va_end(argp);
  return 0;
}

// TODO: Determine if it is necessary to emulate /dev/urandom.

ssize_t read(int fd, void* buf, size_t count) {
  switch (fd) {
  case 0: // STDIN
    if (count < 1) {
      // Cannot do anything with an empty buffer.
      return 0;
    }
    // TODO: Could submit in batches, but rarely will get in batches.
    int result = 0;
    pthread_mutex_lock(&instance->keypresses_lock_);
    if (instance->keypresses_.size() > 0) {
      ((char *)buf)[0] = instance->keypresses_.front();
      instance->keypresses_.pop_front();
      instance->keyboard_target_->Update(instance->keypresses_.size() > 0);
      result = 1;
    } else {
      NaClDebug("read(): From STDIN, no data, treat as nonblocking.");
    }
    pthread_mutex_unlock(&instance->keypresses_lock_);
    return result;
  }

  NaClDebug("read(%d, ...): Unexpected read.", fd);
  errno = EIO;
  return -1;
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
  return instance->udp_->Close(fd);
}

//
// Wrap all networking functions to communicate via the Pepper API.
//

int socket(int domain, int type, int protocol) {
  if (domain != AF_INET || type != SOCK_DGRAM || protocol != 0) {
    errno = EPROTO;
    return -1;
  }
  return instance->udp_->Socket();
}

/* bind() doesn't seem to be necessary after all, but keeping it just in case.
int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
  if (addr->sa_family != AF_INET || addrlen != 4) {
    // Only IPv4 currently supported.
    // TODO: When Mosh supports IPv6, include support for it here.
    NaClDebug("bind: Bad input. sa_family=%d, addrlen=%d",
        addr->sa_family, addrlen);
    errno = EPROTO;
    return -1;
  }
  PP_NetAddress_IPv4 pp_addr;
  MakeAddress(addr, addrlen, &addr);
  return instance->udp_->Bind(sockfd, pp_addr);
}
*/

int setsockopt(int sockfd, int level, int optname,
    const void* optval, socklen_t optlen) {
  NaClDebug("setsockopt stub called; fd=%d", sockfd);
  return 0;
}
int dup(int oldfd) {
  return instance->udp_->Dup(oldfd);
}
int pselect(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds,
    const struct timespec* timeout, const sigset_t* sigmask) {
  /*
  // Debugging output only:
  for (int fd = 0; fd < nfds; ++fd) {
    if (readfds != NULL && FD_ISSET(fd, readfds)) {
      NaClDebug("pselect(): read %d", fd);
    }
    if (writefds != NULL && FD_ISSET(fd, writefds)) {
      NaClDebug("pselect(): write %d", fd);
    }
  }
  */
  bool got_udp = false;
  bool got_keyboard = false;
  bool got_window_change = false;

  vector<PepperPOSIX::Target *> targets =
    instance->selector_->SelectAll(timeout);
  for (vector<PepperPOSIX::Target *>::iterator i = targets.begin();
      i != targets.end();
      ++i) {
    switch ((*i)->id()) {
    case TARGET_UDP:
      got_udp = true;
      break;
    case TARGET_KEYBOARD:
      got_keyboard = true;
      break;
    case TARGET_WINDOW_CHANGE:
      got_window_change = true;
      break;
    }
  }

  if (!got_udp) {
    // TODO: Clean this up; not dealing with file descriptors at all, and just
    // assuming any file descriptor != 0 is UDP.
    FD_ZERO(readfds);
  }

  // Just assuming the caller cares about STDIN. Must explicitly set and clear,
  // to ensure no damage from hack above.
  if (got_keyboard) {
    FD_SET(0, readfds);
  } else {
    FD_CLR(0, readfds);
  }

  FD_ZERO(exceptfds);

  // Send a SIGWINCH if pending. This has to be done on the native Mosh thread,
  // which is why we do it here in the pselect().
  if (got_window_change) {
    instance->sigwinch_handler_(SIGWINCH);
    instance->window_change_target_->Update(false);
  }

  return 0;
}
ssize_t recvmsg(int sockfd, struct msghdr* msg, int flags) {
  return instance->udp_->Receive(sockfd, msg, flags);
}
ssize_t sendto(int sockfd, const void* buf, size_t len, int flags,
    const struct sockaddr* dest_addr, socklen_t addrlen) {
  vector<char> buffer((const char*)buf, (const char*)buf+len);
  PP_NetAddress_IPv4 addr;
  MakeAddress(dest_addr, addrlen, &addr);
  return instance->udp_->Send(sockfd, buffer, flags, addr);
}

} // extern "C"

namespace pp {
Module* CreateModule() {
  return new MoshClientModule();
}
} // namespace pp
