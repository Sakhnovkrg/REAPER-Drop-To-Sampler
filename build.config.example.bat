@echo off
rem Local build settings for build.bat.
rem Copy this file to build.config.bat (git-ignored) and adjust paths.
rem Every value is optional: anything left empty is auto-detected.

rem Toolchain: auto | mingw | msvc
rem   auto  - MinGW if found (MINGW_BIN, PATH, C:\msys64\ucrt64\bin), otherwise MSVC
set "TOOLCHAIN=auto"

rem MinGW: folder with g++.exe, cmake.exe, ninja.exe (MSYS2 UCRT64 by default)
set "MINGW_BIN="
rem set "MINGW_BIN=C:\msys64\ucrt64\bin"

rem MSVC: Visual Studio install folder (empty = latest found by vswhere)
set "VS_PATH="
rem set "VS_PATH=C:\Program Files\Microsoft Visual Studio\18\Community"

rem Extra folders prepended to PATH (e.g. standalone cmake/ninja), ';'-separated
set "EXTRA_PATH="
rem set "EXTRA_PATH=C:\Tools\cmake\bin;C:\Tools\ninja"

rem Release | Debug | RelWithDebInfo
set "BUILD_TYPE=Release"

rem Extra arguments passed to CMake configure
set "CMAKE_ARGS="

rem "build.bat deploy": where the DLL is copied and which REAPER is restarted
set "PLUGIN_DIR=C:\REAPER\UserPlugins"
set "REAPER_EXE=C:\REAPER\reaper.exe"
