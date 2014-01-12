// pepper_posix.cc - Pepper POSIX adapters.
//
// Pepper POSIX is a set of adapters to enable POSIX-like APIs to work with the
// callback-based APIs of Pepper (and transitively, JavaScript).

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
#include "pepper_posix_native_udp.h"

#include <errno.h>
#include <netinet/in.h>

namespace PepperPOSIX {

POSIX::POSIX(const pp::InstanceHandle &instance_handle,
    Reader *std_in, Writer *std_out, Writer *std_err, Signal *signal)
  : instance_handle_(instance_handle) {
  files_[STDIN_FILENO] = std_in;
  if (std_in != NULL) {
    std_in->target_ = selector_.NewTarget(STDIN_FILENO);
  }
  files_[STDOUT_FILENO] = std_out;
  if (std_out != NULL) {
    std_out->target_ = selector_.NewTarget(STDOUT_FILENO);
  }
  files_[STDERR_FILENO] = std_err;
  if (std_err != NULL) {
    std_err->target_ = selector_.NewTarget(STDERR_FILENO);
  }
  // This file descriptor isn't used, so place it out of the issuance range.
  files_[-1] = signal;
  if (signal != NULL) {
    signal->target_ = selector_.NewTarget(-1);
  }
}

POSIX::~POSIX() {
  for (::std::map<int, File *>::iterator i = files_.begin();
      i != files_.end();
      ++i) {
    Close(i->first);
  }
}

int POSIX::Open(const char *pathname, int flags, mode_t mode) {
  ::std::map<string, File *(*)()>::iterator factories_iter =
      factories_.find(string(pathname));
  if (factories_iter == factories_.end()) {
    errno = EACCES;
    return -1;
  }
  File *file = factories_iter->second();
  // TODO: Error out if |file|'s type doesn't match |flags| (i.e., Reader
  // cannot be O_WRONLY).
  int fd = NextFileDescriptor();
  files_[fd] = file;
  file->target_ = selector_.NewTarget(fd);
  return fd;
}

int POSIX::Close(int fd) {
  if (files_.count(fd) == 0) {
    errno = EBADF;
    return -1;
  }

  int result = files_[fd]->Close();
  delete files_[fd];
  files_.erase(fd);

  return result;
}

ssize_t POSIX::Read(int fd, void *buf, size_t count) {
  if (files_.count(fd) == 0) {
    errno = EBADF;
    return -1;
  }
  Reader *reader = dynamic_cast<Reader *>(files_[fd]);
  if (reader == NULL) {
    errno = EBADF;
    return -1;
  }

  return reader->Read(buf, count);
}

ssize_t POSIX::Write(int fd, const void *buf, size_t count) {
  if (files_.count(fd) == 0) {
    errno = EBADF;
    return -1;
  }
  Writer *writer = dynamic_cast<Writer *>(files_[fd]);
  if (writer == NULL) {
    errno = EBADF;
    return -1;
  }

  return writer->Write(buf, count);
}

int POSIX::NextFileDescriptor() {
  for (int fd = 0; ; ++fd) {
    if (files_.count(fd) == 0) {
      return fd;
    }
  }
}

int POSIX::Socket(int domain, int type, int protocol) {
  if (domain != AF_INET) {
    errno = EINVAL;
    return -1;
  }

  if (type == SOCK_DGRAM && (protocol == 0 || protocol == IPPROTO_UDP)) {
    NativeUDP *udp = new NativeUDP(instance_handle_);
    int fd = NextFileDescriptor();
    udp->target_ = selector_.NewTarget(fd);
    files_[fd] = udp;
    return fd;
  }

  if (type == SOCK_STREAM && (protocol == 0 || protocol == IPPROTO_TCP)) {
    // TODO: Implement.
    errno = EPROTONOSUPPORT;
    return -1;
  }

  errno = EINVAL;
  return -1;
}

int POSIX::Dup(int oldfd) {
  if (files_.count(oldfd) == 0) {
    errno = EBADF;
    return -1;
  }
  // Currently can only dup UDP sockets.
  UDP *udp = dynamic_cast<UDP *>(files_[oldfd]);
  if (udp == NULL) {
    errno = EBADF;
    return -1;
  }

  return Socket(AF_INET, SOCK_DGRAM, 0);
}

int POSIX::PSelect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
    const struct timespec *timeout, const sigset_t *sigmask) {
  int result = 0;
  // This code assumes that the caller cares about all open files. This is not
  // perfectly compliant, but works for the initial use case.
  if (readfds != NULL) {
    FD_ZERO(readfds);
  }
  if (writefds != NULL) {
    FD_ZERO(writefds);
  }
  if (exceptfds != NULL) {
    FD_ZERO(exceptfds);
  }

  vector<Target *> targets = selector_.SelectAll(timeout);
  for (vector<Target *>::iterator i = targets.begin();
      i != targets.end();
      ++i) {
    int fd = (*i)->id();
    File *file = files_[fd];

    Reader *reader = dynamic_cast<Reader *>(file);
    if (reader != NULL) {
      FD_SET(fd, readfds);
      ++result;
      continue;
    }
    Writer *writer = dynamic_cast<Writer *>(file);
    if (writer != NULL) {
      FD_SET(fd, writefds);
      ++result;
      continue;
    }
    UDP *udp = dynamic_cast<UDP *>(file);
    if (udp != NULL) {
      FD_SET(fd, readfds);
      ++result;
      continue;
    }
    Signal *signal = dynamic_cast<Signal *>(file);
    if (signal != NULL) {
      signal->Handle();
      ++result;
      continue;
    }

    assert(false); // Should not get here.
  }

  return result;
}

ssize_t POSIX::RecvMsg(int sockfd, struct msghdr *msg, int flags) {
  if (files_.count(sockfd) == 0) {
    errno = EBADF;
    return -1;
  }
  UDP *udp = dynamic_cast<UDP *>(files_[sockfd]);
  if (udp == NULL) {
    errno = EBADF;
    return -1;
  }

  return udp->Receive(msg, flags);
}

// Make a PP_NetAddress_IPv4 from sockaddr.
void MakeAddress(const struct sockaddr *addr, socklen_t addrlen,
    PP_NetAddress_IPv4 *pp_addr) {
  // TODO: Make an IPv6 version, but since mosh doesn't support it now, this
  // will do.
  assert(addr->sa_family == AF_INET);
  assert(addrlen >= 4);
  const struct sockaddr_in *in_addr = (struct sockaddr_in*)addr;
  uint32_t a = in_addr->sin_addr.s_addr;
  for (int i = 0; i < 4; ++i) {
    pp_addr->addr[i] = a & 0xff;
    a >>= 8;
  }
  pp_addr->port = in_addr->sin_port;
}

ssize_t POSIX::SendTo(int sockfd, const void *buf, size_t len, int flags,
    const struct sockaddr *dest_addr, socklen_t addrlen) {
  if (files_.count(sockfd) == 0) {
    return EBADF;
  }
  UDP *udp = dynamic_cast<UDP *>(files_[sockfd]);
  if (udp == NULL) {
    errno = EBADF;
    return -1;
  }

  vector<char> buffer((const char*)buf, (const char*)buf+len);
  PP_NetAddress_IPv4 addr;
  MakeAddress(dest_addr, addrlen, &addr);

  return udp->Send(buffer, flags, addr);
}

} // namespace PepperPOSIX
