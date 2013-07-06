#!/bin/bash -e

# Build the Native Client port of the Mosh client for running in the Chrome
# browser.

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

pushd .. > /dev/null

if [[ ! -f configure ]]; then
  echo "Running autogen."
  ./autogen.sh
fi

echo "Configuring..."
./configure --host=nacl --prefix=${NACLPORTS_PREFIX}
#./configure --host=nacl --prefix=${NACLPORTS_PREFIX} --disable-silent-rules
make clean

echo "Building Mosh with NaCl compiler..."
make || echo "Ignore error."

pushd src/frontend > /dev/null
$AR rcs libmoshclient-${NACL_ARCH}.a \
  mosh-client.o stmclient.o terminaloverlay.o ../crypto/libmoshcrypto.a \
  ../network/libmoshnetwork.a ../statesync/libmoshstatesync.a \
  ../terminal/libmoshterminal.a ../util/libmoshutil.a \
  ../protobufs/libmoshprotos.a
popd > /dev/null

# TODO: Actually build the .nexe, probably using a Makefile.

popd > /dev/null # ..

echo "Done building (WARNING: incomplete script)."
