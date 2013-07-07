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

#include <stdlib.h>

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

// Implement stubs for functions not in NaCl's libc.
extern "C" {

// These are used to avoid CORE dumps. Should be OK to stub them out.
int getrlimit(int resource, struct rlimit *rlim) {
  return 0;
}
int setrlimit(int resource, const struct rlimit *rlim) {
  return 0;
}

// TODO: sigaction is used for things like catching window size changes. This
// will need to be redirected.
int sigaction(int signum, const struct sigaction *act,
    struct sigaction *oldact) {
  return 0;
}

// kill() is used to send a SIGSTOP on Ctrl-Z, which is not useful for NaCl.
int kill(pid_t pid, int sig) {
  return 0;
}

} // extern "C"

// TODO: Implement something useful.

// Forward declaration of mosh_main(), as it has no header file.
int mosh_main(int argc, char* argv[]);

class MoshClientInstance : public pp::Instance {
 public:
  explicit MoshClientInstance(PP_Instance instance) : pp::Instance(instance) {
    setenv("MOSH_KEY", "somekeyhere", 1);
    char* argv[] = { "mosh-client", "192.168.11.125", "60001" };
    mosh_main(sizeof(argv) / sizeof(argv[0]), argv);
  }

  virtual ~MoshClientInstance() {}

  virtual void HandleMessage(const pp::Var& var) {
    if (!var.is_string()) {
      return;
    }
    if (var.AsString() == "hello") {
      PostMessage(pp::Var("Greetings from the Mosh Native Client!"));
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
