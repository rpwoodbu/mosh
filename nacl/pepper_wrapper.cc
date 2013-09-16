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
#include "pepper_posix_udp.h"

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
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

using ::std::string;
using ::std::vector;

// Forward declaration of mosh_main(), as it has no header file.
int mosh_main(int argc, char* argv[]);

// Make the singleton MoshClientInstance available to C functions in this
// module.
static class MoshClientInstance* instance = NULL;

class MoshClientInstance : public pp::Instance {
 public:
  explicit MoshClientInstance(PP_Instance instance) :
      pp::Instance(instance), addr_(NULL), port_(NULL),
      udp(NULL), selector(NULL) {
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
    delete udp;
    delete selector;
  }

  virtual void HandleMessage(const pp::Var& var) {
    if (!var.is_string()) {
      return;
    }
    if (var.AsString() == "hello") {
      PostMessage(pp::Var("Greetings from the Mosh Native Client!"));
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
    selector = new PepperPOSIX::Selector();
    udp = new PepperPOSIX::StubUDP(selector->NewTarget());

    // Launch mosh-client.
    pthread_create(&thread_, NULL, &Launch, this);
    return true;
  }

  static void* Launch(void* data) {
    MoshClientInstance* thiz = reinterpret_cast<MoshClientInstance*>(data);

    setenv("TERM", "vt100", 1);
    char* argv[] = { "mosh-client", thiz->addr_, thiz->port_ };
    mosh_main(sizeof(argv) / sizeof(argv[0]), argv);
    thiz->PostMessage(pp::Var("Mosh has exited."));
    return 0;
  }

  // Selector object that stubs will use.
  PepperPOSIX::Selector *selector;
  // UDP object that stubs will use.
  PepperPOSIX::UDP *udp;

 private:
  static int num_instances_; // This needs to be a singleton.
  pthread_t thread_;
  // Non-const params for mosh_main().
  char* addr_;
  char* port_;
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
  instance->PostMessage(pp::Var(buf));
}

// Make an address string from sockaddr.
string MakeAddress(const struct sockaddr* addr, socklen_t addrlen) {
  char host[NI_MAXHOST];
  char port[NI_MAXSERV];
  if (getnameinfo(addr, addrlen, host, sizeof(host), port, sizeof(port),
        NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
    string address = string(host) + ":" + port;
    NaClDebug("MakeAddress: %s", address.c_str());
    return address;
  }
  NaClDebug("MakeAddress: failed");
  return "";
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

// TODO: Wire this into hterm.
int ioctl(int d, long unsigned int request, ...) {
  if (d != STDIN_FILENO || request != TIOCGWINSZ) {
    errno = EPROTO;
    return -1;
  }
  va_list argp;
  va_start(argp, request);
  struct winsize* ws = va_arg(argp, struct winsize*);
  ws->ws_row = 24;
  ws->ws_col = 80;
  va_end(argp);
  return 0;
}

/* TODO: Intercept ifstream; this doesn't seem to work.
// Emulate /dev/urandom, but don't interfere with closing of sockets.
const int RANDOM_FD = 24;
int open(const char* pathname, int flags, ...) {
  string path = pathname;
  if (path != "/dev/urandom") {
    NaClDebug("opening something NOT /dev/urandom! (%s)", pathname);
    errno = EACCES;
    return -1;
  }
  NaClDebug("open stub called for /dev/urandom");
  return RANDOM_FD;
}
ssize_t read(int fd, void* buf, size_t count) {
  if (fd != RANDOM_FD) {
    NaClDebug("read stub called for some other FD! (%d)", fd);
    errno = EIO;
    return -1;
  }
  // TODO: Properly emulate /dev/urandom.
  return count;
}
*/
int close(int fd) {
  return instance->udp->Close(fd);
}

//
// Wrap all networking functions to communicate via JavaScript, where UDP can
// be done.
//

int socket(int domain, int type, int protocol) {
  if (domain != AF_INET || type != SOCK_DGRAM || protocol != 0) {
    errno = EPROTO;
    return -1;
  }
  return instance->udp->Socket();
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

  return instance->udp->Bind(sockfd, MakeAddress(addr, addrlen));
}
*/

int setsockopt(int sockfd, int level, int optname,
    const void* optval, socklen_t optlen) {
  NaClDebug("setsockopt stub called; fd=%d", sockfd);
  return 0;
}
int dup(int oldfd) {
  return instance->udp->Dup(oldfd);
}
int pselect(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds,
    const struct timespec* timeout, const sigset_t* sigmask) {
  const vector<PepperPOSIX::Target*> targets =
    instance->selector->SelectAll(timeout);
  if (targets.size() == 0) {
    // TODO: Consider how to break this down to individual descriptors.
    FD_ZERO(readfds);
  }
  FD_ZERO(exceptfds);
  return 0;
}
ssize_t recvmsg(int sockfd, struct msghdr* msg, int flags) {
  return instance->udp->Receive(sockfd, msg, flags);
}
ssize_t sendto(int sockfd, const void* buf, size_t len, int flags,
    const struct sockaddr* dest_addr, socklen_t addrlen) {
  vector<char> buffer((const char*)buf, (const char*)buf+len);
  return instance->udp->Send(
      sockfd, buffer, flags, MakeAddress(dest_addr, addrlen));
}

} // extern "C"

namespace pp {
Module* CreateModule() {
  return new MoshClientModule();
}
} // namespace pp
