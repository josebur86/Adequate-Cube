#!/bin/bash

clear

echo "Building..."

if [ ! -d ../build ]; then
    mkdir ../build
fi
pushd ../build

clang++ -g -O0 ../code/osx_aqcube.cpp -L../code/libs -lSDL2 -o osx_aqcube

popd
