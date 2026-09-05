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

# ---------------------------------------------------------------------------
# Optional: the wallet applications under extras/. Each stage is off by default
# and adds gigabytes, so they live at the end of the file - switching one on or
# off only rebuilds these layers, and Docker keeps both variants cached.
# build.sh turns on whatever the requested target needs.
#
# Caches that must be writable by an arbitrary uid at run time live under
# /build (a bind mount), not in the image.
# ---------------------------------------------------------------------------
ENV FLUTTER_ROOT=/opt/flutter \
    ANDROID_SDK_ROOT=/opt/android-sdk \
    ANDROID_HOME=/opt/android-sdk \
    EMSDK=/opt/emsdk \
    PUB_CACHE=/build/.pub-cache \
    GRADLE_USER_HOME=/build/.gradle \
    EM_CACHE=/build/.emscripten-cache \
    FLUTTER_SUPPRESS_ANALYTICS=true \
    PATH=/opt/flutter/bin:/opt/flutter/bin/cache/dart-sdk/bin:/opt/android-sdk/cmdline-tools/latest/bin:/opt/android-sdk/platform-tools:$PATH

# Flutter SDK + the Linux desktop toolchain (clang/GTK). Needed by every
# application target. The apps ask for Flutter 3.38+ / Dart 3.10+ in their
# pubspec.yaml, so keep FLUTTER_VERSION at or above that.
ARG WITH_FLUTTER=0
ARG FLUTTER_VERSION=3.38.7
RUN set -eux; \
    if [ "${WITH_FLUTTER}" = "1" ]; then \
        apt-get update; \
        apt-get install -y --no-install-recommends \
            clang \
            libgtk-3-dev \
            liblzma-dev \
            libglu1-mesa; \
        rm -rf /var/lib/apt/lists/*; \
        curl -fsSL -o /tmp/flutter.tar.xz \
          "https://storage.googleapis.com/flutter_infra_release/releases/stable/linux/flutter_linux_${FLUTTER_VERSION}-stable.tar.xz"; \
        tar -C /opt -xf /tmp/flutter.tar.xz; \
        rm -f /tmp/flutter.tar.xz; \
        test -x "${FLUTTER_ROOT}/bin/flutter"; \
        git config --system --add safe.directory "${FLUTTER_ROOT}"; \
        (flutter precache --universal --linux --web && flutter --version) \
          || echo "note: flutter refused to run as root; the caches are fetched on the first build instead"; \
        chmod -R a+rwX "${FLUTTER_ROOT}"; \
    fi

# Android SDK + JDK for the mobile wallet's Gradle build. The NDK version is
# the one the installed Flutter pins (android/app/build.gradle asks for
# flutter.ndkVersion); override it with ANDROID_SDK_NDK_VERSION if the
# detection ever misses. This is separate from the CLI toolchain's
# /opt/android-ndk, which Gradle does not look at.
ARG WITH_ANDROID_SDK=0
ARG ANDROID_CMDLINE_TOOLS_VERSION=13114758
ARG ANDROID_SDK_PACKAGES="platform-tools platforms;android-36 build-tools;36.0.0"
ARG ANDROID_SDK_NDK_VERSION=
RUN set -eux; \
    if [ "${WITH_ANDROID_SDK}" = "1" ]; then \
        apt-get update; \
        apt-get install -y --no-install-recommends openjdk-17-jdk-headless; \
        rm -rf /var/lib/apt/lists/*; \
        mkdir -p "${ANDROID_SDK_ROOT}/cmdline-tools"; \
        curl -fsSL -o /tmp/cmdline-tools.zip \
          "https://dl.google.com/android/repository/commandlinetools-linux-${ANDROID_CMDLINE_TOOLS_VERSION}_latest.zip"; \
        unzip -q /tmp/cmdline-tools.zip -d /tmp/cmdline-tools; \
        mv /tmp/cmdline-tools/cmdline-tools "${ANDROID_SDK_ROOT}/cmdline-tools/latest"; \
        rm -rf /tmp/cmdline-tools /tmp/cmdline-tools.zip; \
        ndk_ver="${ANDROID_SDK_NDK_VERSION}"; \
        if [ -z "${ndk_ver}" ] && [ -d "${FLUTTER_ROOT}/packages/flutter_tools/gradle" ]; then \
            ndk_ver="$(grep -rhoE 'ndkVersion[^0-9]*[0-9]+\.[0-9]+\.[0-9]+' \
                        "${FLUTTER_ROOT}/packages/flutter_tools/gradle" 2>/dev/null \
                       | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)"; \
        fi; \
        if [ -n "${ndk_ver}" ]; then \
            echo "Android SDK NDK for Gradle: ${ndk_ver}"; \
        else \
            echo "No Flutter NDK pin detected; set --build-arg ANDROID_SDK_NDK_VERSION=<x.y.z> if Gradle asks for one"; \
        fi; \
        yes | sdkmanager --licenses > /dev/null; \
        sdkmanager --install ${ANDROID_SDK_PACKAGES} ${ndk_ver:+ndk;${ndk_ver}} > /dev/null; \
        chmod -R a+rwX "${ANDROID_SDK_ROOT}"; \
    fi

# Emscripten for the web wallet's WASM module. 3.1.50 is the version the first
# green web build used.
ARG WITH_EMSDK=0
ARG EMSDK_VERSION=3.1.50
RUN set -eux; \
    if [ "${WITH_EMSDK}" = "1" ]; then \
        git clone --depth 1 https://github.com/emscripten-core/emsdk.git "${EMSDK}"; \
        cd "${EMSDK}"; \
        ./emsdk install "${EMSDK_VERSION}"; \
        ./emsdk activate "${EMSDK_VERSION}"; \
        chmod -R a+rwX "${EMSDK}"; \
    fi

WORKDIR /work
CMD ["bash"]
