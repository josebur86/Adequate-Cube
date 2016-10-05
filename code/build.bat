@echo off

if not exist ..\build mkdir ..\build
pushd ..\build

cl /nologo ..\code\win32_sidescroller.cpp /link user32.lib

popd
