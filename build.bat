@echo off
setlocal enabledelayedexpansion

set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=debug

echo === Windows MSVC Build (%BUILD_TYPE%) ===

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 (
    echo Failed to set up MSVC environment
    exit /b 1
)

REM Check if we need to configure:
REM - No CMakeCache.txt means fresh build
REM - CMakeLists.txt changed since last configure (compare against marker)
set NEED_CONFIGURE=0
if not exist "build\CMakeCache.txt" set NEED_CONFIGURE=1
if !NEED_CONFIGURE!==0 (
    if exist "build\.cmake_marker" (
        fc /b CMakeLists.txt build\.cmake_marker >nul 2>&1
        if errorlevel 1 set NEED_CONFIGURE=1
    ) else (
        set NEED_CONFIGURE=1
    )
)

if !NEED_CONFIGURE!==1 (
    echo.
    echo === Configuring ===
    cmake --preset windows-%BUILD_TYPE%
    if errorlevel 1 exit /b 1
    copy /y CMakeLists.txt build\.cmake_marker >nul
    REM Reset deploy marker when reconfiguring
    if exist "build\.deployed" del "build\.deployed"
) else (
    echo.
    echo === Skipping configure (CMakeLists.txt unchanged) ===
)

echo.
echo === Building ===
cmake --build build --parallel
if errorlevel 1 exit /b 1

REM Only deploy if marker file doesn't exist
if not exist "build\.deployed" (
    echo.
    echo === Deploying Qt DLLs ===
    C:\Qt\6.11.0\msvc2022_64\bin\windeployqt.exe --qmldir src\qml build\bin\EmergencyPlan.exe
    if errorlevel 1 exit /b 1
    echo. > build\.deployed
) else (
    echo.
    echo === Skipping Qt deployment (already deployed) ===
)

echo.
echo === Build complete ===
