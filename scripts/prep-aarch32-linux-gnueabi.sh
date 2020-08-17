#!/bin/bash
# Copyright (c) 2019 The TurtleCoin Developers
#
# Please see the included LICENSE file for more information.

#
# This script is used to set up the cross-compilation environment
# for building aarch32 on x64_64 systems which provides a very
# fast build experience for the aarch32 platform at this time
#

# Save our current working location so that we can return here
# when we are ready
export BASEDIR=`pwd`

# Set our toolchain folder
export TOOLCHAIN_DIR=$HOME/toolchain

# Make and change to a toolchain directory for storage of all
# tools that we will use as part of the build process
mkdir -p $TOOLCHAIN_DIR && cd $TOOLCHAIN_DIR

# Check to see if we have the aarch32 compiler, if not download
# the prebuilt binaries from a trusted source
echo -n "Checking for arm-linux-gnueabi-gcc... "
if [ ! -f $TOOLCHAIN_DIR/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-gcc ]; then
  echo "Not found... Installing..."
  wget https://releases.linaro.org/components/toolchain/binaries/7.5-2019.12/arm-linux-gnueabi/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabi.tar.xz
  tar xfv gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabi.tar.xz >/dev/null
  echo "arm-linux-gnueabi: Installed"
else
  echo "Found"
fi

# Set our environment variables to use the aarch32 compiler
export CC=$TOOLCHAIN_DIR/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-gcc
export CPP=$TOOLCHAIN_DIR/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-cpp
export CXX=$TOOLCHAIN_DIR/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-g++
export RANLIB=$TOOLCHAIN_DIR/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-ranlib
export AR=$TOOLCHAIN_DIR/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-ar
export LD=$TOOLCHAIN_DIR/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-ld
export MAKEDEPPROG=$TOOLCHAIN_DIR/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-gcc
export CUSTOM_TOOLCHAIN_FILE=../scripts/cross-aarch32-linux-gnueabi.cmake

# Check to see if we have Boost 1.55 ready in our toolchain, if
# not we'll download and build it
echo -n "Checking for Boost 1.55... "
if [ ! -f $TOOLCHAIN_DIR/boost_1_55_0_aarch32-linux-gnueabi/stage/lib/libboost_system.a ]; then
  echo "Not found... Installing..."
  wget http://sourceforge.net/projects/boost/files/boost/1.55.0/boost_1_55_0.tar.gz
  tar zxvf boost_1_55_0.tar.gz >/dev/null
  mv boost_1_55_0 boost_1_55_0_aarch32-linux-gnueabi
  cd boost_1_55_0_aarch32-linux-gnueabi
  echo -n "Bootstrapping Boost 1.55 build... "
  ./bootstrap.sh > /dev/null
  echo "Complete"
  echo "using gcc : arm : ${TOOLCHAIN_DIR}/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-g++ ; " >> tools/build/v2/user-config.jam
  echo -n "Building Boost 1.55... "
  ./b2 toolset=gcc-arm --with-system --with-filesystem --with-thread --with-date_time --with-chrono --with-regex --with-serialization --with-program_options >/dev/null
  cd $TOOLCHAIN_DIR
  echo "Complete"
else
  echo "Found"
fi

# Set our environment variable to use the new Boost root
export BOOST_ROOT=$TOOLCHAIN_DIR/boost_1_55_0_aarch32-linux-gnueabi

# Check to see if we have OpenSSL ready in our toolchain, if
# not we'll download and build it
echo -n "Checking for OpenSSL 1.1.1f..."
if [ ! -f $TOOLCHAIN_DIR/openssl_aarch32-linux-gnueabi/lib/libcrypto.a ]; then
  echo "Not found... Installing..."
  wget https://www.openssl.org/source/openssl-1.1.1f.tar.gz
  tar zxvf openssl-1.1.1f.tar.gz >/dev/null
  cd openssl-1.1.1f
  ./Configure linux-armv4 --prefix=$TOOLCHAIN_DIR/openssl_aarch32-linux-gnueabi --openssldir=$TOOLCHAIN_DIR/openssl_aarch32-linux-gnueabi >/dev/null
  echo -n "Building OpenSSL 1.1.1f... "
  make PROCESSOR=ARM install >/dev/null
  echo "Complete"
else
  echo "Found"
fi

# Set our environment variable to use the new OpenSSL root
export OPENSSL_ROOT_DIR=$TOOLCHAIN_DIR/openssl_aarch32-linux-gnueabi

# Set an environment variable that contains our custom CMake toolchain
# commands for our project and other env vars that we'll need
export STRIP=$TOOLCHAIN_DIR/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-strip

# Return to the path we started at
cd $BASEDIR
