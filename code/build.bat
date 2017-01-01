@echo off

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
set CompilerFlags= /Od /Zi /MT /nologo /Gm- /GR- /EHa- /Oi /WX /W4 /wd4100 /FC

REM Linkder Flags
set LinkerFlags= /opt:ref user32.lib Gdi32.lib DSound.lib Winmm.lib

if not exist ..\build mkdir ..\build
pushd ..\build

cl %CompilerFlags% /LD ..\code\aqcube.cpp /link %LinkerFlags% -export:UpdateGameAndRender -export:GetSoundSamples
cl %CompilerFlags% ..\code\win32_aqcube.cpp /link %LinkerFlags%

popd

endlocal
