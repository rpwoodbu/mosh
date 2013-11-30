#!/bin/bash -e

# Build the Native Client port of the Mosh client for running in the Chrome
# browser. If you have already built once and are doing active development on
# the Native Client port, invoke with the parameter "fast".

FAST=""
if [[ $# -gt 0 ]]; then
  FAST="$1"
  shift 1
fi

# Copyright 2013 Richard Woodbury
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# TODO: Get NaCl SDK.

# TODO: Get NaCl Ports (and depot_tools).
if [[ ${NACL_PORTS} == "" ]]; then
  echo "Please set NACL_PORTS."
  exit 1
fi

# TODO: Consider getting and building protoc to match what's in NaCl Ports.

export NACL_GLIBC="1"

make clean
for arch in x86_64 i686; do ( # Do all this in a separate subshell.
  export NACL_ARCH="${arch}"
  echo "Building packages in NaCl Ports..."
  pushd ${NACL_PORTS}/src > /dev/null
  make zlib openssl ncurses protobuf
  popd > /dev/null

  echo "Updating submodules..."
  pushd .. > /dev/null
  git submodule init
  git submodule update
  popd > /dev/null
  pushd chromium_assets/chromeapps/hterm > /dev/null
  if [[ ! -d dist ]]; then
    bin/mkdist.sh
  fi
  popd > /dev/null

  echo "Loading naclports environment..."
  # For some reason I have to build NACLPORTS_LIBDIR myself, and I need vars to
  # do this that nacl_env.sh generates, so I end up calling that guy twice.
  . ${NACL_PORTS}/src/build_tools/nacl_env.sh
  export NACLPORTS_LIBDIR=${NACL_TOOLCHAIN_ROOT}/${NACL_CROSS_PREFIX}/usr/lib
  eval $(${NACL_PORTS}/src/build_tools/nacl_env.sh --print)

  export CC=${NACLCC}
  export CXX=${NACLCXX}
  export AR=${NACLAR}
  export RANLIB=${NACLRANLIB}
  export PKG_CONFIG_LIBDIR=${NACLPORTS_LIBDIR}
  export PKG_CONFIG_PATH=${PKG_CONFIG_LIBDIR}/pkgconfig
  export PATH=${NACL_BIN_PATH}:${PATH};
  # TODO: See if there's a way to get the linker to prefer main() in libppapi
  # instead of using this hack.
  export CXXFLAGS="-D__NACL__"

  if [[ ${FAST} != "fast" ]]; then
    pushd .. > /dev/null
    if [[ ! -f configure ]]; then
      echo "Running autogen."
      ./autogen.sh
    fi
    popd > /dev/null # ..

    build_dir="build/${NACL_ARCH}"
    mkdir -p "${build_dir}"
    pushd "${build_dir}" > /dev/null
    echo "Configuring..."
    configure_options="--host=nacl --enable-client=yes --enable-server=no"
    if [[ "${arch}" == "i686" ]]; then
      # The i686 build doesn't seem to have stack protection, even though
      # "configure" finds it, so disabling hardening. :(
      configure_options="${configure_options} --disable-hardening"
    fi
    ../../../configure ${configure_options}
    echo "Building Mosh with NaCl compiler..."
    make clean
    make || echo "Ignore error."
    popd > /dev/null # ${build_dir}
  fi

  target="app/mosh_client_${NACL_ARCH}.nexe"
  echo "Building ${target}..."
  make "${target}"

) done

# Copy hterm dist files into app directory.
mkdir -p app/hterm
cp -f chromium_assets/chromeapps/hterm/dist/js/* app/hterm

if [[ ${FAST} == "fast" ]]; then
  make nmf
else
  make all
fi

echo "Done."
