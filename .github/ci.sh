#!/bin/bash

if [ ! $CI ]; then
    echo "This script is intended to be run in CI only"
    exit 1
fi

set -e
echo "Running CI script"

if [ -f CMakeLists.txt ]; then
    echo "Building with CMake"
    cmake -B $GITHUB_WORKSPACE/build -DENABLE_ASAN=ON
    cd $GITHUB_WORKSPACE/build
    cmake --build .
else
    echo "Building with Make"
    cd $GITHUB_WORKSPACE
    make -j
fi

echo "Running tests"
set -x
fallocate -l 512MiB disk.img
mkfs.fat disk.img
./fat disk.img ck
