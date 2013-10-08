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
# TODO: Make other architectures.
export NACL_ARCH="x86_64" 

echo "Building packages in NaCl Ports..."
pushd ${NACL_PORTS}/src > /dev/null
make zlib openssl ncurses protobuf
popd > /dev/null

source ${NACL_PORTS}/src/build_tools/common.sh

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
  echo "Configuring..."
  ./configure --host=nacl --prefix=${NACLPORTS_PREFIX} \
    --enable-client=yes --enable-server=no # --disable-silent-rules
  echo "Building Mosh with NaCl compiler..."
  make clean
  make || echo "Ignore error."
  popd > /dev/null # ..
fi

echo "Building .nexe..."
make clean
if [[ ${FAST} == "fast" ]]; then
  make nmf
else
  make all
fi

echo "Done."
