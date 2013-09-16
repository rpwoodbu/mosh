// pepper_posix_udp.h - UDP Pepper POSIX adapters.
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

#ifndef PEPPER_POSIX_UDP_HPP
#define PEPPER_POSIX_UDP_HPP

#include "pepper_posix_selector.h"

#include <string>
#include <vector>
#include <sys/socket.h>
#include <sys/types.h>

namespace PepperPOSIX {

using std::string;

// UDP implements the basic POSIX emulation logic for UDP communication. It is
// not fully implemented. An implementation should fully implement Send() and
// insert received packets using AddPacket(). It is expected that AddPacket()
// will be called from a different thread than the one calling the other
// methods; no other thread safety is provided.
class UDP {
 public:
  // UDP constructs with a Target, from Selector::GetTarget().
  UDP(Target* target) : target_(target), next_fd_(42) {}
  virtual ~UDP() { delete target_; }

  // Socket replaces socket(), and returns a new fd.
  int Socket();

  /* Not sure if this is necessary.
  // Bind replaces bind().
  int Bind(int fd, const string& address);
  */

  // Dup replaces dup(), and returns a new fd.
  int Dup(int fd);

  // Receive replaces recvmsg(); see its documentation for usage.
  ssize_t Receive(int fd, struct ::msghdr* message, int flags);

  // Send replaces sendto(). Usage is similar, but tweaked for C++.
  virtual ssize_t Send(
    int fd, const vector<char>& buf, int flags, const string& address) = 0;

  // Close replaces close().
  int Close(int fd);

 protected:
  // AddPacket is used by the subclass to add a packet to the incoming queue.
  // This method can be called from another thread than the one used to call
  // the other methods.
  void AddPacket(struct ::msghdr* message);
 
 private:
  Target* target_;
  int next_fd_;

  // Disable copy and assignment.
  UDP(const UDP&);
  UDP& operator=(const UDP&);
};

// StubUDP is an instantiatable stubbed subclass of UDP for debugging.
class StubUDP : public UDP {
 public:
  // StubUDP constructs with a Target, from Selector::GetTarget().
  StubUDP(Target* target) : UDP(target) {}
  virtual ~StubUDP() {}

  // Send replaces sendto. Usage is similar, but tweaked for C++.
  virtual ssize_t Send(
    int fd, const vector<char>& buf, int flags, const string& address);

 private:
  // Disable copy and assignment.
  StubUDP(const StubUDP&);
  StubUDP& operator=(const StubUDP&);
};

} // namespace PepperPOSIX

#endif // PEPPER_POSIX_UDP_HPP
