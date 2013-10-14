// pepper_posix.h - Pepper POSIX adapters.
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

#ifndef PEPPER_POSIX
#define PEPPER_POSIX

#include "pepper_posix_selector.h"

#include <map>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "ppapi/c/ppb_net_address.h"
#include "ppapi/cpp/instance_handle.h"

namespace PepperPOSIX {

// Abstract class representing a POSIX file.
class File {
 public:
  File(Target *target) : target_(target) {}
  virtual ~File() {};

  virtual int Close() = 0;
  int fd() { return target_->id(); }

  Target *target_;

 private:
  // Disable copy and assignment.
  File(const File &);
  File &operator=(const File &);
};

// Abstract class defining a file that is read-only.
class Reader : public File {
 public:
  Reader(Target *target) : File(target) {}
  virtual ~Reader() {}
  virtual ssize_t Read(void *buf, size_t count) = 0;
};

// Abstract class defining a file that is write-only.
class Writer : public File {
 public:
  Writer(Target *target) : File(target) {}
  virtual ~Writer() {}
  virtual ssize_t Write(const void *buf, size_t count) = 0;
};

// Special File to handle signals. Write a method in your implementation that
// calls target_->Update(true) when there's an outstanding signal. Handle()
// will get called when there is.
class Signal : public File {
 public:
  Signal(Target *target) : File(target) {}
  virtual ~Signal() {}
  // Implement this to handle a signal. It will be called from PSelect. It is
  // the responsibility of the implementer to track what signals are
  // outstanding. Call target_->Update(false) from this method when there are
  // no more outstanding signals.
  virtual void Handle() = 0;
  virtual int Close() { return 0; };
};


// POSIX implements the top-level POSIX file functionality, allowing easy
// binding to "unistd.h" functions. As such, the methods bear a great
// resemblance to those functions.
class POSIX {
 public:
  // Provide implementations of Reader and Writer which emulate STDIN, STDOUT,
  // and STDERR. Provide an implementation of Signal to handle signals, which
  // will be called from PSelect(). Construct them with NULL targets; this
  // constructor will generate targets for them.
  //
  // Set any of these to NULL if not used. Takes ownership of all.
  POSIX(const pp::InstanceHandle& instance_handle,
      Reader *std_in, Writer *std_out, Writer *std_err, Signal *signal);
  ~POSIX();

  ssize_t Read(int fd, void *buf, size_t count);

  ssize_t Write(int fd, const void *buf, size_t count);

  int Close(int fd);

  int Socket(int domain, int type, int protocol);

  int Dup(int oldfd);

  int PSelect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
      const struct timespec *timeout, const sigset_t *sigmask);

  ssize_t RecvMsg(int sockfd, struct msghdr *msg, int flags);

  ssize_t SendTo(int sockfd, const void *buf, size_t len, int flags,
      const struct sockaddr *dest_addr, socklen_t addrlen);

 private:
  // Map of file descriptors and the File objects they represent.
  ::std::map<int, File *> files_;
  Selector selector_;
  const pp::InstanceHandle& instance_handle_;

  // Returns the next available file descriptor.
  int NextFileDescriptor_();

  // Disable copy and assignment.
  POSIX(const POSIX &);
  POSIX &operator=(const POSIX &);
};

} // namespace PepperPOSIX

#endif // PEPPER_POSIX
