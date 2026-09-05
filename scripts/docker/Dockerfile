# WrkzCoin CLI release builder image.
#
# One image carries every toolchain the portable release set needs:
#   - the native GCC of the base image      -> Linux x86_64 (fully static, glibc)
#   - MinGW-w64 (posix threads) + a static OpenSSL built for the Windows target
#                                           -> Windows x86_64
#   - the Android NDK + a static libucontext per ABI (bionic has no
#     getcontext/swapcontext/makecontext, which the fiber dispatcher needs)
#                                           -> Android arm64-v8a / x86_64
#
# The repository is never copied into the image. build.sh bind-mounts it at
# run time, so the image only changes when one of the pinned versions below
# changes. The build context is the repo's scripts/ directory:
#
#   docker build -t wrkzcoin-cli-builder -f scripts/docker/Dockerfile scripts
#
# Every ARG can be overridden with --build-arg (build.sh passes
# IMAGE_BUILD_ARGS straight through).

ARG UBUNTU_VERSION=24.04
FROM ubuntu:${UBUNTU_VERSION}

ARG DEBIAN_FRONTEND=noninteractive

# Static OpenSSL for the Windows target (the Linux target uses libssl-dev).
ARG OPENSSL_VERSION=3.5.8
# Android NDK. r27 is the LTS line; the SHA1 comes from
# https://developer.android.com/ndk/downloads. Leave it empty to skip the check
# when pointing at a version whose checksum you have not looked up.
ARG ANDROID_NDK_VERSION=r27d
ARG ANDROID_NDK_SHA1=22105e410cf29afcf163760cc95522b9fb981121
# libucontext tag (or branch) built for each Android ABI listed here.
ARG LIBUCONTEXT_REF=libucontext-1.5.2
ARG ANDROID_ABIS="arm64-v8a x86_64"
ARG ANDROID_PLATFORM=24

# ---------------------------------------------------------------------------
# Host packages: native toolchain, MinGW-w64 cross toolchain, build tools.
# Only the x86_64 posix-thread MinGW flavour is installed; the "mingw-w64"
# metapackage would also pull the i686 and win32-thread variants.
# ---------------------------------------------------------------------------
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      build-essential \
      cmake \
      ninja-build \
      pkg-config \
      ccache \
      git \
      curl \
      ca-certificates \
      unzip \
      zip \
      xz-utils \
      file \
      perl \
      make \
      python3 \
      meson \
      libssl-dev \
      zlib1g-dev \
      gcc-mingw-w64-x86-64-posix \
      g++-mingw-w64-x86-64-posix \
      binutils-mingw-w64-x86-64 \
      mingw-w64-x86-64-dev \
 && (update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix || true) \
 && (update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix || true) \
 && rm -rf /var/lib/apt/lists/*

# ---------------------------------------------------------------------------
# OpenSSL for the Windows target. Mirrors scripts/prep-windows-x86_64.sh, with
# --libdir=lib so the layout is the same whatever OpenSSL's default for the
# mingw64 target is; scripts/cross-windows-x86_64.cmake finds it through
# CROSS_PREFIX, which container-build.sh points here.
# ---------------------------------------------------------------------------
ENV WRKZ_MINGW_PREFIX_DIR=/opt/toolchain/windows-x86_64/prefix
RUN set -eux; \
    mkdir -p /tmp/openssl; \
    cd /tmp/openssl; \
    curl -fsSL -o openssl.tar.gz \
      "https://github.com/openssl/openssl/releases/download/openssl-${OPENSSL_VERSION}/openssl-${OPENSSL_VERSION}.tar.gz"; \
    tar xzf openssl.tar.gz; \
    cd "openssl-${OPENSSL_VERSION}"; \
    ./Configure mingw64 \
        no-shared \
        no-tests \
        no-module \
        --cross-compile-prefix=x86_64-w64-mingw32- \
        --prefix="${WRKZ_MINGW_PREFIX_DIR}" \
        --libdir=lib \
        --openssldir="${WRKZ_MINGW_PREFIX_DIR}/ssl"; \
    make -j"$(nproc)"; \
    make install_sw; \
    cd /; \
    rm -rf /tmp/openssl; \
    test -f "${WRKZ_MINGW_PREFIX_DIR}/lib/libcrypto.a"; \
    test -f "${WRKZ_MINGW_PREFIX_DIR}/lib/libssl.a"

# ---------------------------------------------------------------------------
# Android NDK. simpleperf and shader-tools are not needed for a CLI build and
# account for a good share of the unpacked size.
# ---------------------------------------------------------------------------
ENV ANDROID_NDK=/opt/android-ndk \
    ANDROID_NDK_HOME=/opt/android-ndk \
    ANDROID_NDK_ROOT=/opt/android-ndk
RUN set -eux; \
    cd /tmp; \
    curl -fsSL -o ndk.zip \
      "https://dl.google.com/android/repository/android-ndk-${ANDROID_NDK_VERSION}-linux.zip"; \
    if [ -n "${ANDROID_NDK_SHA1}" ]; then \
        echo "${ANDROID_NDK_SHA1}  ndk.zip" | sha1sum -c -; \
    fi; \
    unzip -q ndk.zip -d /opt; \
    rm -f ndk.zip; \
    mv "/opt/android-ndk-${ANDROID_NDK_VERSION}" "${ANDROID_NDK}"; \
    rm -rf "${ANDROID_NDK}/simpleperf" "${ANDROID_NDK}/shader-tools"; \
    test -f "${ANDROID_NDK}/build/cmake/android.toolchain.cmake"

# ---------------------------------------------------------------------------
# libucontext per Android ABI, built with the repo's own script so the bionic
# x86_64 REG_* patch lives in exactly one place. The script is copied from the
# build context (scripts/), then CR-stripped in case the checkout is CRLF.
# ---------------------------------------------------------------------------
ENV WRKZ_ANDROID_LIBUCONTEXT_BASE=/opt/android-libucontext
COPY build-libucontext-android.sh /opt/wrkz/scripts/build-libucontext-android.sh
RUN set -eux; \
    sed -i 's/\r$//' /opt/wrkz/scripts/build-libucontext-android.sh; \
    for abi in ${ANDROID_ABIS}; do \
        ABI="${abi}" \
        ANDROID_PLATFORM="${ANDROID_PLATFORM}" \
        LIBUCONTEXT_REF="${LIBUCONTEXT_REF}" \
        WORK_DIR=/tmp/libucontext \
        PREFIX="${WRKZ_ANDROID_LIBUCONTEXT_BASE}/${abi}" \
        bash /opt/wrkz/scripts/build-libucontext-android.sh; \
        test -f "${WRKZ_ANDROID_LIBUCONTEXT_BASE}/${abi}/lib/libucontext.a"; \
    done; \
    rm -rf /tmp/libucontext

# ---------------------------------------------------------------------------
# Run-time layout. build.sh mounts the repo at /work, the build tree at /build
# and the package directory at /out, and runs the container as the calling
# host user so nothing comes back root-owned. HOME lives under /tmp so an
# arbitrary uid always has a writable home.
# ---------------------------------------------------------------------------
ENV HOME=/tmp/wrkz-home \
    CCACHE_DIR=/build/.ccache
RUN mkdir -p /work /build /out /tmp/wrkz-home \
 && chmod 1777 /work /build /out /tmp/wrkz-home

WORKDIR /work
CMD ["bash"]
