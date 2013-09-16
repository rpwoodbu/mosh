// pepper_posix_udp.cc - UDP Pepper POSIX adapters.
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

#include "pepper_posix_udp.h"

// TODO: Eliminate this debugging hack.
void NaClDebug(const char* fmt, ...);

namespace PepperPOSIX {

int UDP::Socket() {
  // TODO: Implement.
  NaClDebug("Socket(): fd=%d", next_fd_);
  return next_fd_++;
}

int UDP::Dup(int fd) {
  // TODO: Implement.
  NaClDebug("Dup(): oldfd=%d, fd=%d", fd, next_fd_);
  return next_fd_++;
}

ssize_t UDP::Receive(int fd, struct ::msghdr* message, int flags) {
  // TODO: Implement.
  NaClDebug("Receive(): fd=%d", fd);
  target_->Update(false);
  return 0;
}

void UDP::AddPacket(struct ::msghdr* message) {
  // TODO: Implement.
  NaClDebug("AddPacket()");
  target_->Update(true);
}

int UDP::Close(int fd) {
  // TODO: Implement.
  NaClDebug("Close(): fd=%d", fd);
  return 0;
}

ssize_t StubUDP::Send(
    int fd, const vector<char>& buf, int flags, const PP_NetAddress_IPv4& addr) {
  NaClDebug("StubUDP::Send(): fd=%d, size=%d", fd, buf.size());
  NaClDebug("StubUDP::Send(): Pretending we received something.");
  AddPacket(NULL);
  return buf.size();
}

int StubUDP::Bind(int fd, const PP_NetAddress_IPv4& address) {
  NaClDebug("StubBind(): fd=%d", fd);
  return 0;
}

} // namespace PepperPOSIX
