@echo off
rem Usage:
rem   build.bat           configure + build
rem   build.bat deploy    build, close REAPER, copy DLL to UserPlugins, start REAPER
rem Local settings: build.config.bat (see build.config.example.bat)
setlocal EnableExtensions

set "DLL_NAME=reaper_drop_to_sampler-x64.dll"

rem %~dp0 ends with backslash; trailing \ before " breaks quoting on Windows
set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

set "DEPLOY="
if /I "%~1"=="deploy" set "DEPLOY=1"

rem --- Defaults (overridden by build.config.bat) ---
set "TOOLCHAIN=auto"
set "MINGW_BIN="
set "VS_PATH="
set "EXTRA_PATH="
set "BUILD_TYPE=Release"
set "CMAKE_ARGS="
set "PLUGIN_DIR=%APPDATA%\REAPER\UserPlugins"
set "REAPER_EXE=C:\Program Files\REAPER (x64)\reaper.exe"

if exist "%ROOT%\build.config.bat" call "%ROOT%\build.config.bat"
if not "%EXTRA_PATH%"=="" set "PATH=%EXTRA_PATH%;%PATH%"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if /I "%TOOLCHAIN%"=="mingw" goto setup_mingw
if /I "%TOOLCHAIN%"=="msvc" goto setup_msvc
if /I not "%TOOLCHAIN%"=="auto" goto bad_toolchain

rem --- Auto-detect: MinGW first, then MSVC ---
if not "%MINGW_BIN%"=="" goto setup_mingw
where g++ >NUL 2>&1
if not errorlevel 1 goto setup_mingw
if exist "C:\msys64\ucrt64\bin\g++.exe" set "MINGW_BIN=C:\msys64\ucrt64\bin"
if not "%MINGW_BIN%"=="" goto setup_mingw
goto setup_msvc

:setup_mingw
set "TOOLCHAIN=mingw"
if "%MINGW_BIN%"=="" goto mingw_path_ready
if not exist "%MINGW_BIN%\g++.exe" goto mingw_missing
set "PATH=%MINGW_BIN%;%PATH%"
:mingw_path_ready
where g++ >NUL 2>&1
if errorlevel 1 goto mingw_missing
set "CMAKE_COMPILERS=-DCMAKE_CXX_COMPILER=g++"
goto toolchain_ready

:setup_msvc
set "TOOLCHAIN=msvc"
if not "%VS_PATH%"=="" goto msvc_vs_found
if not exist "%VSWHERE%" goto msvc_missing
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_PATH=%%i"
if "%VS_PATH%"=="" goto msvc_missing
:msvc_vs_found
set "VCVARS=%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" goto msvc_missing
rem vcvars64 itself looks for vswhere on PATH
if exist "%VSWHERE%" set "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%"
call "%VCVARS%" >NUL
if errorlevel 1 goto msvc_missing
set "CMAKE_COMPILERS=-DCMAKE_CXX_COMPILER=cl"
goto toolchain_ready

:toolchain_ready
set "BUILD_DIR=%ROOT%\build\%TOOLCHAIN%"
echo === Toolchain: %TOOLCHAIN% (%BUILD_TYPE%) ===

where cmake >NUL 2>&1
if errorlevel 1 goto cmake_missing
where ninja >NUL 2>&1
if errorlevel 1 goto ninja_missing

if not defined DEPLOY goto build
echo.
echo === Closing REAPER if running ===
tasklist /FI "IMAGENAME eq reaper.exe" 2>NUL | find /I "reaper.exe" >NUL
if errorlevel 1 goto build
taskkill /IM reaper.exe >NUL 2>&1
echo Waiting for REAPER to exit...
:wait_reaper
timeout /T 1 /NOBREAK >NUL
tasklist /FI "IMAGENAME eq reaper.exe" 2>NUL | find /I "reaper.exe" >NUL
if not errorlevel 1 goto wait_reaper
echo REAPER closed.

:build
echo.
echo === Configure ===
cmake -S "%ROOT%" -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=%BUILD_TYPE% %CMAKE_COMPILERS% %CMAKE_ARGS%
if errorlevel 1 goto cmake_fail

echo.
echo === Build ===
cmake --build "%BUILD_DIR%"
if errorlevel 1 goto build_fail
if not exist "%BUILD_DIR%\%DLL_NAME%" goto dll_missing
echo DLL: %BUILD_DIR%\%DLL_NAME%

if not defined DEPLOY goto all_done
echo.
echo === Copy to UserPlugins ===
if not exist "%PLUGIN_DIR%" mkdir "%PLUGIN_DIR%"
copy /Y "%BUILD_DIR%\%DLL_NAME%" "%PLUGIN_DIR%\%DLL_NAME%" >NUL
if errorlevel 1 goto copy_fail
echo Copied to: %PLUGIN_DIR%\%DLL_NAME%

echo.
echo === Start REAPER ===
if not exist "%REAPER_EXE%" goto reaper_missing
start "" "%REAPER_EXE%"
echo Started: %REAPER_EXE%
goto all_done

:bad_toolchain
echo Unknown TOOLCHAIN "%TOOLCHAIN%" - use auto, mingw or msvc.
goto fail

:mingw_missing
echo MinGW g++ not found. Set MINGW_BIN in build.config.bat
echo (e.g. C:\msys64\ucrt64\bin) or put g++ on PATH.
goto fail

:msvc_missing
echo No usable toolchain: MinGW not found, and Visual Studio with
echo "Desktop development with C++" not found either.
echo Set MINGW_BIN or VS_PATH in build.config.bat.
goto fail

:cmake_missing
echo cmake not found. Install it or add its folder to EXTRA_PATH in build.config.bat.
goto fail

:ninja_missing
echo ninja not found. Install it or add its folder to EXTRA_PATH in build.config.bat.
goto fail

:cmake_fail
echo CMake configure failed
goto fail

:build_fail
echo Build failed
goto fail

:dll_missing
echo DLL not found: %BUILD_DIR%\%DLL_NAME%
goto fail

:copy_fail
echo Copy failed - is the DLL locked?
goto fail

:reaper_missing
echo REAPER not found: %REAPER_EXE%
echo Set REAPER_EXE in build.config.bat.
goto fail

:fail
rem Keep the window open on double-click, but not in CI
if not defined CI pause
exit /b 1

:all_done
echo.
echo === Done ===
endlocal
