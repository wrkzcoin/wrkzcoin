FROM debian:stretch

RUN apt-get update && apt-get install -y unzip automake build-essential curl file pkg-config git python libtool libtinfo5 wget openjdk-8-jdk

WORKDIR /opt/android
## INSTALL ANDROID SDK
ENV ANDROID_SDK_REVISION 4333796
ENV ANDROID_SDK_HASH 92ffee5a1d98d856634e8b71132e8a95d96c83a63fde1099be3d86df3106def9
RUN mkdir -p /usr/local/android-sdk-linux \
    && curl -s -O https://dl.google.com/android/repository/sdk-tools-linux-${ANDROID_SDK_REVISION}.zip \
    && echo "${ANDROID_SDK_HASH}  sdk-tools-linux-${ANDROID_SDK_REVISION}.zip" | sha256sum -c \
    && unzip sdk-tools-linux-${ANDROID_SDK_REVISION}.zip -d /usr/local/android-sdk-linux \
    && rm -f sdk-tools-linux-${ANDROID_SDK_REVISION}.zip

# Set environment variable
ENV ANDROID_HOME /usr/local/android-sdk-linux
ENV ANDROID_SDK_ROOT ${ANDROID_HOME}
ENV PATH ${ANDROID_HOME}/tools:$ANDROID_HOME/platform-tools:$PATH

# Make license agreement
RUN mkdir $ANDROID_HOME/licenses && \
    echo 8933bad161af4178b1185d1a37fbf41ea5269c55 > $ANDROID_HOME/licenses/android-sdk-license && \
    echo d56f5187479451eabf01fb78af6dfcb131a6481e >> $ANDROID_HOME/licenses/android-sdk-license && \
    echo 24333f8a63b6825ea9c5514f83c2829b004d1fee >> $ANDROID_HOME/licenses/android-sdk-license && \
    echo 84831b9409646a918e30573bab4c9c91346d8abd > $ANDROID_HOME/licenses/android-sdk-preview-license

# Update and install using sdkmanager
RUN $ANDROID_HOME/tools/bin/sdkmanager "tools" "platform-tools" && \
    $ANDROID_HOME/tools/bin/sdkmanager "build-tools;29.0.2" "build-tools;28.0.3" "build-tools;27.0.3" && \
    $ANDROID_HOME/tools/bin/sdkmanager "platforms;android-29" "platforms;android-28" "platforms;android-27" && \
    $ANDROID_HOME/tools/bin/sdkmanager "extras;android;m2repository" "extras;google;m2repository"

## INSTALL ANDROID NDK
ENV ANDROID_NDK_REVISION 17b
ENV ANDROID_NDK_HASH 5dfbbdc2d3ba859fed90d0e978af87c71a91a5be1f6e1c40ba697503d48ccecd
RUN curl -s -O https://dl.google.com/android/repository/android-ndk-r${ANDROID_NDK_REVISION}-linux-x86_64.zip \
    && echo "${ANDROID_NDK_HASH}  android-ndk-r${ANDROID_NDK_REVISION}-linux-x86_64.zip" | sha256sum  -c \
    && unzip android-ndk-r${ANDROID_NDK_REVISION}-linux-x86_64.zip \
    && rm -f android-ndk-r${ANDROID_NDK_REVISION}-linux-x86_64.zip

ENV WORKDIR /opt/android
ENV ANDROID_NDK_ROOT ${WORKDIR}/android-ndk-r${ANDROID_NDK_REVISION}
ENV ANDROID_NDK_HOME ${WORKDIR}/android-ndk-r${ANDROID_NDK_REVISION}
ENV PREFIX /opt/android/prefix
ENV ANDROID_API 21
ENV TOOLCHAIN_DIR ${WORKDIR}/toolchain-arm64
RUN ${ANDROID_NDK_ROOT}/build/tools/make_standalone_toolchain.py \
         --arch arm64 \
         --api ${ANDROID_API} \
         --install-dir ${TOOLCHAIN_DIR} \
         --stl=libc++

#INSTALL cmake
ENV CMAKE_VERSION 3.12.1
RUN cd /usr \
    && curl -s -O https://cmake.org/files/v3.12/cmake-${CMAKE_VERSION}-Linux-x86_64.tar.gz \
    && tar -xzf /usr/cmake-${CMAKE_VERSION}-Linux-x86_64.tar.gz \
    && rm -f /usr/cmake-${CMAKE_VERSION}-Linux-x86_64.tar.gz
ENV PATH /usr/cmake-${CMAKE_VERSION}-Linux-x86_64/bin:$PATH

## Boost
ARG BOOST_VERSION=1_68_0
ARG BOOST_VERSION_DOT=1.68.0
ARG BOOST_HASH=da3411ea45622579d419bfda66f45cd0f8c32a181d84adfa936f5688388995cf
RUN set -ex \
    && wget http://sourceforge.net/projects/boost/files/boost/${BOOST_VERSION_DOT}/boost_${BOOST_VERSION}.tar.gz \
    && echo "${BOOST_HASH}  boost_${BOOST_VERSION}.tar.gz" | sha256sum -c \
    && tar zxvf boost_${BOOST_VERSION}.tar.gz >/dev/null \
    && rm -f boost_${BOOST_VERSION}.tar.gz \
    && cd boost_${BOOST_VERSION} \
    && ./bootstrap.sh --prefix=${PREFIX}

ENV HOST_PATH $PATH
ENV PATH $TOOLCHAIN_DIR/aarch64-linux-android/bin:$TOOLCHAIN_DIR/bin:$PATH

ARG NPROC=1

# Build iconv for lib boost locale
ENV ICONV_VERSION 1.15
ENV ICONV_HASH  ccf536620a45458d26ba83887a983b96827001e92a13847b45e4925cc8913178
RUN curl -s -O http://ftp.gnu.org/pub/gnu/libiconv/libiconv-${ICONV_VERSION}.tar.gz \
    && echo "${ICONV_HASH}  libiconv-${ICONV_VERSION}.tar.gz" | sha256sum -c \
    && tar -xzf libiconv-${ICONV_VERSION}.tar.gz \
    && rm -f libiconv-${ICONV_VERSION}.tar.gz \
    && cd libiconv-${ICONV_VERSION} \
    && CC=aarch64-linux-android-clang CXX=aarch64-linux-android-clang++ ./configure --build=x86_64-linux-gnu --host=arm-eabi --prefix=${PREFIX} --disable-rpath \
    && make -j${NPROC} && make install

## Build BOOST
RUN cd boost_${BOOST_VERSION} \
    && ./b2 --build-type=minimal link=static runtime-link=static --with-chrono --with-date_time --with-filesystem --with-program_options --with-regex --with-serialization --with-system --with-thread --with-locale --build-dir=android --stagedir=android toolset=clang threading=multi threadapi=pthread target-os=android -sICONV_PATH=${PREFIX} install -j${NPROC}

# openssl
ARG OPENSSL_VERSION=1.1.1j
ARG OPENSSL_HASH=aaf2fcb575cdf6491b98ab4829abf78a3dec8402b8b81efc8f23c00d443981bf
ENV PATH ${ANDROID_NDK_ROOT}/toolchains/llvm/prebuilt/linux-x86_64/bin:$PATH
RUN curl -s -O https://www.openssl.org/source/openssl-${OPENSSL_VERSION}.tar.gz \
    && echo "${OPENSSL_HASH}  openssl-${OPENSSL_VERSION}.tar.gz" | sha256sum -c \
    && tar -xzf openssl-${OPENSSL_VERSION}.tar.gz \
    && rm openssl-${OPENSSL_VERSION}.tar.gz \
    && cd openssl-${OPENSSL_VERSION} \
    && export OPENSSL_CONFIGURE_OPTIONS="no-asm no-shared \
        no-tests no-unit-test no-fuzz-libfuzzer no-fuzz-afl" \
    && ./Configure android-arm64 \
            --static \
           -D__ANDROID_API__=${ANDROID_API}  ${OPENSSL_CONFIGURE_OPTIONS} \
           --prefix=${PREFIX} --openssldir=${PREFIX} \
    && make -j${NPROC} \
    && make install

ADD . /src
ENV ANDROID_NDK_ROOT ${WORKDIR}/android-ndk-r${ANDROID_NDK_REVISION}
#ENV PATH ${ANDROID_NDK_ROOT}/toolchains/llvm/prebuilt/linux-x86_64/bin:$PATH
ENV PATH $TOOLCHAIN_DIR/aarch64-linux-android/bin:$TOOLCHAIN_DIR/bin:$PATH
ENV ANDROID_SYSROOT="$ANDROID_NDK_ROOT/platforms/android-$ANDROID_API/arch-arm64"
ENV CROSS_SYSROOT="$ANDROID_SYSROOT"
ENV NDK_SYSROOT="$ANDROID_SYSROOT"
ENV LABEL="aarch64"
RUN echo "\e[32mbuilding: WrkzCoin\e[39m" \
    && cd /src \
    && mkdir build && cd build \
    && CMAKE_INCLUDE_PATH="${PREFIX}/include" \
       CMAKE_LIBRARY_PATH="${PREFIX}/lib" \
       ANDROID_STL=c++_static \
       AR=aarch64-linux-android-ar LD=aarch64-linux-android-ld AS=aarch64-linux-android-as \
       RANLIB=aarch64-linux-android-ranlib STRIP=aarch64-linux-android-strip \
       CC=aarch64-linux-android-clang CXX=aarch64-linux-android-clang++ cmake .. -DARCH=default -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSTATIC=true -DANDROID=true \
    && make -j${NPROC} \
    && echo "\e[32mdone building WrkzCoin\e[39m" \
    && mkdir binary-android-arm64 \
    && cp ./src/{cryptotest,miner,wallet-upgrader,Wrkzd,wrkz-service,wrkz-wallet,wrkz-wallet-api} binary-android-arm64/
