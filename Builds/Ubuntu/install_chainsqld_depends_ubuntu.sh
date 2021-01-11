#!/usr/bin/env bash

#
# This scripts installs the dependencies needed by rippled. It should be run
# with sudo. For ubuntu < 15.10, it installs gcc 5 as the default compiler. gcc
# 5 is ABI incompatable with gcc 4. If needed, the following will switch back to
# gcc-4: `sudo update-alternatives --config gcc` and choosing the gcc-4
# option.
#

if hash lsb_release 2>/dev/null; then
    if [ $(lsb_release -si) == "Ubuntu" ]; then
        ubuntu_release=$(lsb_release -sr)
    fi
fi

if [ -z "${ubuntu_release}" ]; then
    echo "System not supported"
    exit 1
fi

if [ ${ubuntu_release} == "14.04" ] || [ ${ubuntu_release} == "15.04" ]; then
    apt-get install python-software-properties
    echo "deb [arch=amd64] https://mirrors.ripple.com/ubuntu/ trusty stable contrib" | sudo tee /etc/apt/sources.list.d/ripple.list
    wget -O- -q https://mirrors.ripple.com/mirrors.ripple.com.gpg.key | sudo apt-key add -
    add-apt-repository ppa:ubuntu-toolchain-r/test
    apt-get update
    apt-get -y upgrade
    apt-get -y install curl git scons ctags pkg-config protobuf-compiler libprotobuf-dev libssl-dev python-software-properties boost-all-dev nodejs g++-5 g++-4.9 libmysqlclient-dev
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 99 --slave /usr/bin/g++ g++ /usr/bin/g++-5
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.9 99 --slave /usr/bin/g++ g++ /usr/bin/g++-4.9
    exit 0
fi

# Test if 0th parameter has a version number greater than or equal to the 1st param
function version_check() { test "$(printf '%s\n' "$@" | sort -V | tail -n 1)" == "$1"; }

# this should work for versions greater than 15.10
if version_check ${ubuntu_release} 15.10; then
    apt-get update
    apt-get -y upgrade
    # https://askubuntu.com/questions/422975/e-package-python-software-properties-has-no-installation-candidate
    apt-get -y install software-properties-common curl git scons ctags pkg-config protobuf-compiler libprotobuf-dev libssl-dev libboost-all-dev nodejs libmysqlclient-dev
    exit 0
fi

echo "System not supported"
exit 1