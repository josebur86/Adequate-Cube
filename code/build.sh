#!/bin/bash

clear

echo "Building..."

if [ ! -d ../build ]; then
    mkdir ../build
fi
pushd ../build

clang++ ../code/osx_aqcube.cpp -o osx_aqcube

popd
