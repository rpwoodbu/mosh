// pepper_posix_native_udp.h - Native Pepper UDP implementation.

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

#ifndef PEPPER_POSIX_NATIVE_UDP_HPP
#define PEPPER_POSIX_NATIVE_UDP_HPP

#include "pepper_posix_udp.h"

namespace PepperPOSIX {

// NativeUDP implements UDP using the native Pepper UDPSockets API.
class NativeUDP : public UDP {
 public:
  // NativeUDP constructs with a Target, from Selector::GetTarget().
  NativeUDP(Target* target) : UDP(target) {}
  virtual ~NativeUDP() {}

  // Send replaces sendto. Usage is similar, but tweaked for C++.
  virtual ssize_t Send(
      int fd, const vector<char>& buf, int flags, const string& address);

 private:
  // Disable copy and assignment.
  NativeUDP(const NativeUDP&);
  NativeUDP& operator=(const NativeUDP&);
};

} // namespace PepperPOSIX

#endif // PEPPER_POSIX_NATIVE_UDP_HPP
