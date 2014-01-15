// pepper_wrapper.cc - C wrapper functions to interface to PepperPOSIX.

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

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <langinfo.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

extern "C" {

// These are used to avoid CORE dumps. Should be OK to stub them out. However,
// it seems that on x86_32 with glibc, pthread_create() calls this with
// RLIMIT_STACK. It needs to return an error at least, otherwise the thread
// cannot be created. This does not seem to be an issue on x86_64 nor with
// newlib (which doesn't have RLIMIT_STACK in the headers).
int getrlimit(int resource, struct rlimit *rlim) {
#ifndef USE_NEWLIB
  if (resource == RLIMIT_STACK) {
    errno = EAGAIN;
    return -1;
  }
#endif
  return 0;
}
int setrlimit(int resource, const struct rlimit *rlim) {
  return 0;
}

// sigprocmask() isn't meaningful in NaCl; stubbing out.
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
  Log("sigprocmask(%d, ...)", how);
  return 0;
}

// kill() is used to send a SIGSTOP on Ctrl-Z, which is not useful for NaCl.
// This shouldn't be called, but it is annoying to see a linker warning about
// it not being implemented.
int kill(pid_t pid, int sig) {
  Log("kill(%d, %d)", pid, sig);
  return 0;
}

// getpid() isn't actually called, but it is annoying to see a linker warning
// about it not being implemented.
pid_t getpid(void) {
  Log("getpid()");
  return 0;
}

// Stub these out. In the NaCl glibc, locale support is terrible (and we don't
// get UTF-8 because of it). In newlib, there seems to be some crashiness with
// nl_langinfo(). This will do for both cases (although no UTF-8 in glibc can
// cause a bit of a mess).
#ifndef USE_NEWLIB
char *setlocale(int category, const char *locale) {
  Log("setlocale(%d, \"%s\")", category, locale);
  return "NaCl";
}
#endif
char *nl_langinfo(nl_item item) {
  switch (item) {
    case CODESET:
      Log("nl_langinfo(CODESET)");
      return "UTF-8";
    default:
      Log("nl_langinfo(%d)", item);
      return "Error";
  }
}

// We don't really care about terminal attributes.
int tcgetattr(int fd, struct termios *termios_p) {
  Log("tcgetattr(%d, ...)", fd);
  return 0;
}
int tcsetattr(int fd, int optional_actions, const struct termios *termios_p) {
  Log("tcsetattr(%d, %d, ...)", fd, optional_actions);
  return 0;
}

// The getopt() in newlib crashes. We don't really need it anyway. Stub it out.
// getopt() is redirected to mygetopt() using the magic include file.
#ifdef USE_NEWLIB
int mygetopt(int argc, char * const argv[], const char *optstring) {
  optind = 1;
  return -1;
}
#endif

//
// Wrap fopen() and friends to capture access to stderr and /dev/urandom.
//

FILE *fopen(const char *path, const char *mode) {
  int flags = 0;
  if (mode[1] == '+') {
    flags = O_RDWR;
  } else if (mode[0] == 'r') {
    flags = O_RDONLY;
  } else if (mode[0] == 'w' || mode[0] == 'a') {
    flags = O_WRONLY;
  } else {
    errno = EINVAL;
    return NULL;
  }

  FILE *stream = new FILE;
  memset(stream, 0, sizeof(*stream));
  // TODO: Consider the mode param of open().
#ifdef USE_NEWLIB
  stream->_file = open(path, flags);
#else
  stream->_fileno = open(path, flags);
#endif
  return stream;
}

int fprintf(FILE *stream, const char *format, ...) {
  char buf[1024];
  va_list argp;
  va_start(argp, format);
  int size = vsnprintf(buf, sizeof(buf), format, argp);
  va_end(argp);
  return write(fileno(stream), buf, size);
}

int fileno(FILE *stream) {
#ifdef USE_NEWLIB
  return stream->_file;
#else
  return stream->_fileno;
#endif
}

int fclose(FILE *stream) {
  int result = close(fileno(stream));
  if (result == 0) {
    delete stream;
    return 0;
  }
  return result;
}

//
// Wrap all unistd functions to communicate via the Pepper API.
//

// There is a pseudo-overload that includes a third param |mode_t|.
int open(const char *pathname, int flags, ...) {
  // TODO: For now, ignoring |mode_t| param.
  return GetPOSIX()->Open(pathname, flags, 0);
}

ssize_t read(int fd, void *buf, size_t count) {
  return GetPOSIX()->Read(fd, buf, count);
}

ssize_t write(int fd, const void *buf, size_t count) {
  return GetPOSIX()->Write(fd, buf, count);
}

// printf is used rarely (only once at the time of this writing).
int printf(const char *format, ...) {
  char buf[1024];
  va_list argp;
  va_start(argp, format);
  int size = vsnprintf(buf, sizeof(buf), format, argp);
  va_end(argp);
  return write(STDOUT_FILENO, buf, size);
}

int close(int fd) {
  return GetPOSIX()->Close(fd);
}

int socket(int domain, int type, int protocol) {
  return GetPOSIX()->Socket(domain, type, protocol);
}

// Most socket options aren't supported by PPAPI, so just stubbing out.
int setsockopt(int sockfd, int level, int optname,
    const void *optval, socklen_t optlen) {
  Log("setsockopt(%d, %d, %d, ...", sockfd, level, optname);
  return 0;
}

int dup(int oldfd) {
  return GetPOSIX()->Dup(oldfd);
}

int pselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
    const struct timespec *timeout, const sigset_t *sigmask) {
  return GetPOSIX()->PSelect(
     nfds, readfds, writefds, exceptfds, timeout, sigmask);
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
  return GetPOSIX()->RecvMsg(sockfd, msg, flags);
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
    const struct sockaddr *dest_addr, socklen_t addrlen) {
  return GetPOSIX()->SendTo(sockfd, buf, len, flags, dest_addr, addrlen);
}

} // extern "C"
