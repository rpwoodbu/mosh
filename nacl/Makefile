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

PROJECT:=mosh_client
CXX_SOURCES:=\
	pepper_wrapper.cc

CXX_HEADERS:=\
	pepper_wrapper.h

OBJECTS:=\
	../src/frontend/mosh-client.o \
	../src/frontend/stmclient.o \
	../src/frontend/terminaloverlay.o

LIBDIRS:=\
	-L../src/frontend \
	-L../src/crypto \
	-L../src/network \
	-L../src/statesync \
	-L../src/terminal \
	-L../src/util \
	-L../src/protobufs \

LIBS:=\
	-lmoshcrypto \
	-lmoshnetwork \
	-lmoshstatesync \
	-lmoshterminal \
	-lmoshutil \
	-lmoshprotos

OSNAME:=$(shell python $(NACL_SDK_ROOT)/tools/getos.py)
TC_PATH:=$(abspath $(NACL_SDK_ROOT)/toolchain/$(OSNAME)_x86_glibc)
CXX:=$(TC_PATH)/bin/i686-nacl-g++

# Project Build flags
override LDFLAGS+=-lppapi_cpp -lz -lssl -lncurses -lprotobuf -lcrypto -lrt \
	$(LIBDIRS)
# Add this linker flag to build statically:
	#-static -T $(TC_PATH)/x86_64-nacl/lib/ldscripts/elf64_nacl.x.static
#override WARNINGS+=-Wno-long-long -Wall -Wswitch-enum -Werror
#override CXXFLAGS+=-pthread -std=gnu++0x $(WARNINGS) -Iinclude \
        -I$(NACL_SDK_ROOT)/include
override CXXFLAGS+=-pthread $(WARNINGS) -I$(NACL_SDK_ROOT)/include
#override CFLAGS+=-pthread $(WARNINGS) -Iinclude -I$(NACL_SDK_ROOT)/include

COMPAT_INC:=-I$(PNACL_TC_ROOT)/usr/include/glibc-compat
PNACL_CXXFLAGS:=$(CXXFLAGS) -DUSE_NEWLIB $(COMPAT_INC)
THIS_MAKEFILE:=$(abspath $(lastword $(MAKEFILE_LIST)))

# Declare the ALL target first, to make the 'all' target the default build
all: $(PROJECT)_x86_64.nexe

# Define 32 bit compile and link rules for C++ sources
x86_32_OBJS:=$(patsubst %.cc,%_32.o,$(CXX_SOURCES))
$(x86_32_OBJS) : %_32.o : %.cc $(THIS_MAKE) $(CXX_HEADERS)
	$(CXX) -o $@ -c $< -m32 $(CXXFLAGS)

$(PROJECT)_x86_32.nexe : $(x86_32_OBJS)
	$(CXX) -o $@ $^ -m32 $(OBJECTS) $(LIBS) $(CXXFLAGS) $(LDFLAGS)

# Define 64 bit compile and link rules for C++ sources
x86_64_OBJS:=$(patsubst %.cc,%_64.o,$(CXX_SOURCES))
$(x86_64_OBJS) : %_64.o : %.cc $(THIS_MAKE)
	$(CXX) -o $@ -c $< -m64 $(CXXFLAGS)

$(PROJECT)_x86_64.nexe : $(x86_64_OBJS)
	$(CXX) -o $@ $^ -m64 $(OBJECTS) $(LIBS) $(CXXFLAGS) $(LDFLAGS)

clean:
	rm -rf *.o $(PROJECT)*.nexe $(PROJECT).pexe
