#!/bin/bash

clear

echo "Building..."

if [ ! -d build ]; then
    mkdir build
fi
pushd build > /dev/null

CompilerFlags="-std=c++11 -g -O0"
Warnings="-Wno-null-dereference -Wno-writable-strings"

clang++ $CompilerFlags $Warnings -shared -fpic ../code/aqcube.cpp -o libaqcube.so
clang++ $CompilerFlags $Warnings ../code/osx/osx_aqcube.cpp -L../code/osx/libs -lSDL2 -o osx_aqcube

popd > /dev/null
echo "...Done!"
