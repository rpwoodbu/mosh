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

#include "pepper_wrapper.h"

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <langinfo.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

// Implement stubs for functions not in NaCl's libc.
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

// TODO: Wire the socket calls to talk to JS, and do the comm from there.
static int stub_fd = 42;
int socket(int domain, int type, int protocol) {
  if (domain != AF_INET || type != SOCK_DGRAM || protocol != 0) {
    errno = EPROTO;
    return -1;
  }
  fprintf(stderr, "socket stub called; fd=%d\n", stub_fd);
  return stub_fd++;
}
int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
  fprintf(stderr, "bind stub called; fd=%d\n", sockfd);
  return 0;
}
int close(int fd) {
  fprintf(stderr, "close stub called; fd=%d\n", fd);
  return 0;
}
int setsockopt(int sockfd, int level, int optname,
    const void* optval, socklen_t optlen) {
  fprintf(stderr, "setsockopt stub called; fd=%d\n", sockfd);
  return 0;
}
int dup(int oldfd) {
  fprintf(stderr, "dup stub called; oldfd=%d, fd=%d\n", oldfd, stub_fd);
  return stub_fd++;
}
int pselect(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds,
    const struct timespec* timeout, const sigset_t* sigmask) {
  //fprintf(stderr, "pselect stub called; nfds=%d, timeout=(%ld,%ld)\n",
      //nfds, timeout->tv_sec, timeout->tv_nsec);
  return 0;
}
ssize_t recvmsg(int sockfd, struct msghdr* msg, int flags) {
  //fprintf(stderr, "recvmsg stub called; fd=%d, MSG_DONTWAIT=%s\n",
      //sockfd, flags & MSG_DONTWAIT ? "true" : "false");
  sleep(1);
  return 0;
}
ssize_t sendto(int sockfd, const void* buf, size_t len, int flags,
    const struct sockaddr* dest_addr, socklen_t addrlen) {
  fprintf(stderr, "sendto stub called; fd=%d, len=%d\n", sockfd, len);
  return 0;
}

} // extern "C"

// TODO: Implement something useful.

// Forward declaration of mosh_main(), as it has no header file.
int mosh_main(int argc, char* argv[]);

class MoshClientInstance : public pp::Instance {
 public:
  explicit MoshClientInstance(PP_Instance instance) : pp::Instance(instance) {
    setenv("MOSH_KEY", "WT+MdvOOXtwp8+RJn5KPfg", 1);
    setenv("TERM", "vt100", 1);
    char* argv[] = { "mosh-client", "192.168.11.125", "60001" };
    // TODO: This call probably shouldn't be in the constructor, and/or should
    // be in another thread.
    mosh_main(sizeof(argv) / sizeof(argv[0]), argv);
    fprintf(stderr, "Mosh has exited.\n");
  }

  virtual ~MoshClientInstance() {}

  virtual void HandleMessage(const pp::Var& var) {
    if (!var.is_string()) {
      return;
    }
    if (var.AsString() == "hello") {
      //PostMessage(pp::Var("Greetings from the Mosh Native Client!"));
      fprintf(stderr, "Greetings from the Mosh Native Client!\n");
    }
  }
};

class MoshClientModule : public pp::Module {
 public:
  MoshClientModule() : pp::Module() {}
  virtual ~MoshClientModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new MoshClientInstance(instance);
  }
};

namespace pp {
Module* CreateModule() {
  return new MoshClientModule();
}
} // namespace pp
