### How To Compile
#### Linux

##### Prerequisites

- You will need the following packages: boost (1.55 or higher), cmake (3.8 or higher), git, gcc/g++ (7.0 or higher) or Clang (6.0 or higher), make, and python. Most of these should already be installed on your system.
- For Ubuntu: `sudo apt-get install -y build-essential python-dev gcc g++ git cmake libboost-all-dev`
- For CentOS 6/7 (x64): `yum install git-svn make libstdc++-static -y`
  - Install later version of boost devel:
  - `wget https://phoenixnap.dl.sourceforge.net/project/boost/boost/1.58.0/boost_1_58_0.tar.gz`
  - `tar -xzf boost_1_58_0.tar.gz`
  - `cd boost_1_58_0`
  - `./bootstrap.sh --prefix=/opt/boost`
  - `./b2 install --prefix=/opt/boost --with=all`
  - Install devtoolset-7:
  - `yum install centos-release-scl epel-release`
  - `yum install devtoolset-7-gcc* make`
  - Install cmake 3 latest from https://cmake.org/download/
  - `wget https://github.com/Kitware/CMake/releases/download/v3.13.4/cmake-3.13.4-Linux-x86_64.sh`
  - `chmod +x cmake-3.13.4-Linux-x86_64.sh`
  - `./cmake-3.13.4-Linux-x86_64.sh --prefix=/usr/`

##### Building

- `git clone https://github.com/wrkzcoin/wrkzcoin`
- `cd wrkzcoin`
- `mkdir build && cd $_`
- For Ubuntu:
- `cmake ..`
- `make`
- For CentOS 6/7, use devtoolset-7:
- `scl enable devtoolset-7 bash`
- `cmake -DBOOST_ROOT=/opt/boost ..`
- `make`


#### Ubuntu with Clang

##### Prerequisites
- `sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y`
- `wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -`
  - llvm-toolchain For Ubuntu 14.04 (Trusty):
  
    `sudo add-apt-repository "deb https://apt.llvm.org/trusty/ llvm-toolchain-trusty 6.0 main"`
  - llvm-toolchain For Ubuntu 16.04 (Xenial):
  
    `sudo add-apt-repository "deb https://apt.llvm.org/xenial/ llvm-toolchain-xenial 6.0 main"`
  - llvm-toolchain For Ubuntu 18.04 (Bionic):
  
    `sudo add-apt-repository "deb https://apt.llvm.org/bionic/ llvm-toolchain-bionic 6.0 main"`
- `sudo apt-get update && sudo apt-get install aptitude -y`
- `sudo aptitude install -y -o Aptitude::ProblemResolver::SolutionCost='100*canceled-actions,200*removals' build-essential clang-6.0 libstdc++-7-dev git libboost-all-dev python-pip`
- `sudo pip install cmake`


##### Building
- `export CC=clang-6.0`
- `export CXX=clang++-6.0`
- `git clone https://github.com/wrkzcoin/wrkzcoin`
- `cd wrkzcoin`
- `mkdir build && cd $_`
- `cmake ..` or `cmake -DBOOST_ROOT=<path_to_boost_install> ..` when building
- `make`

#### Apple

##### Prerequisites

- Install Homebrew https://brew.sh/ (if you don't have it)
- Install [cmake](https://cmake.org/). See [here](https://stackoverflow.com/questions/23849962/cmake-installer-for-mac-fails-to-create-usr-bin-symlinks) if you are unable call `cmake` from the terminal after installing.
- Install the [boost](http://www.boost.org/) libraries. Either compile boost manually or run `brew install boost`.
- Install XCode and Developer Tools.

##### Building using Clang

- `which brew || /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"`
- `brew install --force cmake boost llvm`
- `export CC=/usr/local/opt/llvm/bin/clang`
- `export CXX=/usr/local/opt/llvm/bin/clang++`
- `git clone https://github.com/wrkzcoin/wrkzcoin`
- `cd wrkzcoin`
- `mkdir build && cd $_`
- `cmake ..` or `cmake -DBOOST_ROOT=<path_to_boost_install> ..` when building
  from a specific boost install. If you used brew to install boost, your path is most likely `/usr/local/include/boost.`
- `make`

##### Building using GCC

- `which brew || /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"`
- `brew install --force cmake boost llvm gcc@8`
- `export CC=gcc-8`
- `export CXX=g++-8`
- `git clone https://github.com/wrkzcoin/wrkzcoin`
- `cd wrkzcoin`
- `chmod +x ./external/rocksdb/build_tools/build_detect_platform`
- `chmod +x ./external/rocksdb/build_tools/version.sh`
- `mkdir build && cd $_`
- `cmake ..` or `cmake -DBOOST_ROOT=<path_to_boost_install> ..` when building
  from a specific boost install. If you used brew to install boost, your path is most likely `/usr/local/include/boost.`
- `make`

The binaries will be in `./src` after compilation is complete.

Run `./src/Wrkzd` to connect to the network and let it sync (it may take a while).

#### Windows 10

##### Prerequisites
- Install [Visual Studio 2017 Community Edition](https://www.visualstudio.com/thank-you-downloading-visual-studio/?sku=Community&rel=15&page=inlineinstall)
- When installing Visual Studio, it is **required** that you install **Desktop development with C++** and the **VC++ v140 toolchain** when selecting features. The option to install the v140 toolchain can be found by expanding the "Desktop development with C++" node on the right. You will need this for the project to build correctly.
- Install [Boost 1.59.0](https://sourceforge.net/projects/boost/files/boost-binaries/1.59.0/), ensuring you download the installer for MSVC 14.

##### Building

- From the start menu, open 'x64 Native Tools Command Prompt for vs2017'.
- `cd <your_wrkzcoin_directory>`
- `mkdir build`
- `cd build`
- Set the PATH variable for cmake: ie. `set PATH="C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin";%PATH%`
- `cmake -G "Visual Studio 14 Win64" .. -DBOOST_ROOT=C:/local/boost_1_59_0` (Or your boost installed dir.)
- `MSBuild WrkzCoin.sln /p:Configuration=Release /m`
- If all went well, it will complete successfully, and you will find all your binaries in the '..\build\src\Release' directory.
- Additionally, a `.sln` file will have been created in the `build` directory. If you wish to open the project in Visual Studio with this, you can.

#### Sync
For faster sync, please use checkpoints: https://github.com/wrkzcoin/checkpoints

#### Thanks
Cryptonote Developers, Bytecoin Developers, Monero Developers, Forknote Project, TurtleCoin Project, DeroGold Association 
