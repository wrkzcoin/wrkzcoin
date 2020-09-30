#!/bin/bash

# Save our current working location so that we can return here
# when we are ready
export BASEDIR=`pwd`

# Set build folder
export TOOLCHAIN_DIR=$HOME/build_support
mkdir -p $TOOLCHAIN_DIR && cd $TOOLCHAIN_DIR

# Install necessary 32 bit supports
sudo dpkg --add-architecture i386
sudo apt-get update
sudo apt-get install -y libc6-dbg:i386
sudo apt-get install -y gcc-8-multilib gcc-multilib g++-8-multilib

# Build libboost
echo "Building libboost 32 bits..."
wget http://sourceforge.net/projects/boost/files/boost/1.65.0/boost_1_65_0.tar.gz
tar zxvf boost_1_65_0.tar.gz >/dev/null
cd boost_1_65_0
echo -n "Bootstrapping Boost 1.65 build... "
./bootstrap.sh --prefix=$TOOLCHAIN_DIR/boost_1_65_0 >/dev/null
echo -e "using gcc : : g++-8 : <compileflags>-m32 <linkflags>-m32 ;">tools/build/example/user-config.jam
cp tools/build/example/user-config.jam $TOOLCHAIN_DIR/
echo -n "Building Boost 1.65..."
./b2 --user-config=$TOOLCHAIN_DIR/user-config.jam install --prefix=$TOOLCHAIN_DIR/boost_1_65_0 toolset=gcc address-model=32 \
--with-system --with-filesystem --with-thread --with-date_time --with-chrono --with-regex --with-serialization --with-program_options  >/dev/null
echo "Building Boost 1.65 Complete"
export BOOST_ROOT=$TOOLCHAIN_DIR/boost_1_65_0
cd $TOOLCHAIN_DIR

# build openssl
echo "Building OpenSSL 32 bits..."
wget https://www.openssl.org/source/openssl-1.1.1f.tar.gz
tar zxvf openssl-1.1.1f.tar.gz >/dev/null
cd openssl-1.1.1f
./Configure -m32 linux-generic32 --prefix=$TOOLCHAIN_DIR/openssl --openssldir=$TOOLCHAIN_DIR/openssl >/dev/null
echo -n "Building OpenSSL 1.1.1f... "
make install >/dev/null
echo "OpenSSL 32 bits Complete"
export OPENSSL_ROOT_DIR=$TOOLCHAIN_DIR/openssl
cd $BASEDIR
