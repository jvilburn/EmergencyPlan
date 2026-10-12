@echo off
REM Builds the named ninja targets inside build\windows-debug, with the MSVC
REM environment set up as build.bat sets it up. For checking a few objects while
REM the tree as a whole does not link.
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%~dp0build\windows-debug"
ninja -k 0 %*
