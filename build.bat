@echo off

REM Rebuild the Tags File
ctags -R .

setlocal

REM Compiler Flags
REM --------------
REM /Zi: Generate Debug PDB
REM /MT: Multithreaded lib
REM /Gm-: Disable Minimal Rebuild
REM /GR-: Disable RTTI
REM /EHa-: Disable exception generation
REM /Oi: Generate intrinsic functions
REM /WX: Treat warnings as errors
REM /W4: Level 4 warnings
REM /wd####: Disable warning number ####
REM /FC: Display full path to source
REM
set CompilerFlags= /Od /MTd /nologo /Gm- /GR- /EHa- /Zo /Oi /WX /W4 /wd4100 /wd4189 /wd4201 /wd4505 /FC /Z7

REM Linker Flags
set LinkerFlags= /incremental:no /opt:ref user32.lib Gdi32.lib DSound.lib Winmm.lib Opengl32.lib

if not exist .\build mkdir .\build
pushd .\build

del *.pdb > NUL 2> NUL

echo "Compiling AQCube.dll" > lock.tmp
cl %CompilerFlags% /LD ..\code\aqcube.cpp /link %LinkerFlags% /PDB:aqcube_%random%.pdb -export:UpdateGameAndRender -export:GetSoundSamples
del lock.tmp
cl %CompilerFlags% /I ..\code\third_party\GLAD\include ..\code\win32_aqcube.cpp /link %LinkerFlags%

popd

endlocal
