@echo off

if not exist ..\build mkdir ..\build
pushd ..\build

cl /Od /Zi /nologo ..\code\win32_sidescroller.cpp /link user32.lib Gdi32.lib

popd
