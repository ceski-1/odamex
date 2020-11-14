#!/usr/bin/env bash

# http://redsymbol.net/articles/unofficial-bash-strict-mode/

set -euo pipefail
IFS=$'\n\t'

set -x

if [[ $(uname -s) == "Linux" ]]; then
    # Ubuntu
    sudo apt update
    if [[ -z ${USE_SDL12:-} ]]; then
        sudo apt install ninja-build libsdl2-dev libsdl2-mixer-dev \
            libcurl4-openssl-dev libwxgtk3.0-gtk3-dev deutex
    else
        sudo apt install ninja-build libsdl1.2-dev libsdl-mixer1.2-dev \
            libcurl4-openssl-dev libwxgtk3.0-gtk3-dev deutex
    fi
else
    # macOS

    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install.sh)"
    brew update
    brew upgrade

    brew install ninja sdl2 sdl2_mixer wxmac
fi

mkdir -p build && cd build
if [[ -z ${USE_SDL12:-} ]]; then
    cmake .. -GNinja \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo -DUSE_COLOR_DIAGNOSTICS=1 \
        -DBUILD_OR_FAIL=1 -DBUILD_CLIENT=1 -DBUILD_SERVER=1 \
        -DBUILD_MASTER=1 -DBUILD_LAUNCHER=1
else
    cmake .. -GNinja \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo -DUSE_COLOR_DIAGNOSTICS=1 -DUSE_SDL12=1 \
        -DBUILD_OR_FAIL=1 -DBUILD_CLIENT=1 -DBUILD_SERVER=1 \
        -DBUILD_MASTER=1 -DBUILD_LAUNCHER=1
fi
