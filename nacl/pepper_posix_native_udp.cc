// pepper_posix_native_udp.cc - Native Pepper UDP implementation.

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

#include "pepper_posix_native_udp.h"

#include <string.h>

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

// TODO: Eliminate this debugging hack.
void NaClDebug(const char* fmt, ...);

namespace PepperPOSIX {

NativeUDP::NativeUDP(
    const pp::InstanceHandle& instance_handle, Target* target) :
    UDP(target), instance_handle_(instance_handle), factory_(this) {
  socket_ = new pp::UDPSocket(instance_handle);
  bound_ = false;
}

NativeUDP::~NativeUDP() {
  delete socket_;
}

int NativeUDP::Bind(int fd, const PP_NetAddress_IPv4& address) {
  pp::NetAddress net_address(instance_handle_, address);
  pp::Var string_address = net_address.DescribeAsString(true);
  if (string_address.is_undefined()) {
    NaClDebug("NativeUDP::Bind() Address is bogus.");
    // TODO: Return something appropriate.
    return false;
  } else {
    NaClDebug("NativeUDP::Bind() Address is %s", string_address.AsString().c_str());
  }

  int32_t result = socket_->Bind(net_address, pp::CompletionCallback());
  NaClDebug("NativeUDP::Bind() fd=%d, result=%d", fd, result);
  if (result == PP_OK) {
    bound_ = true;
    pp::Module::Get()->core()->CallOnMainThread(
        0, factory_.NewCallback(&NativeUDP::StartReceive));
  }
  // TODO: Flesh out error mapping.
  return result;
}

ssize_t NativeUDP::Send(
    int fd, const vector<char>& buf, int flags,
    const PP_NetAddress_IPv4& address) {
  if (!bound_) {
    PP_NetAddress_IPv4 any_address;
    memset(&any_address, 0, sizeof(any_address));
    int result = Bind(fd, any_address);
    if (result != 0) {
      NaClDebug("NativeUDP::Send(): Bind failed with %d", result);
      return 0;
    }
  }

  pp::NetAddress net_address(instance_handle_, address);
  return socket_->SendTo(
      buf.data(), buf.size(), net_address, pp::CompletionCallback());
}

// StartReceive prepares to receive another packet, and returns without
// blocking.
void NativeUDP::StartReceive(int32_t unused) {
  int32_t result = socket_->RecvFrom(
      receive_buffer_, sizeof(receive_buffer_),
      factory_.NewCallbackWithOutput(&NativeUDP::Received));
  if (result != PP_OK_COMPLETIONPENDING) {
    NaClDebug("NativeUDP::StartReceive(): RecvFrom returned %d", result);
    // TODO: Perhaps crash here?
  }
}

// Received is the callback result of StartReceive().
void NativeUDP::Received(int32_t result, const pp::NetAddress &address) {
  NaClDebug("NativeUDP::Received(): Got %d from %s",
      result, address.DescribeAsString(true).AsString().c_str());
  // TODO: Implement.
  AddPacket(NULL);
  // Await another packet.
  StartReceive(0);
}

} // namespace PepperPOSIX
