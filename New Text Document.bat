@echo off
setlocal
cd /d "%~dp0"
set "BIN=C:\msys64\ucrt64\bin"
set "PATH=%BIN%;%PATH%"
if not exist "%BIN%\g++.exe" goto nogcc
"%BIN%\windres.exe" resources.rc -O coff -o resources.o
if errorlevel 1 goto fail
"%BIN%\g++.exe" -std=c++17 -O2 -s -mwindows -static -static-libgcc -static-libstdc++ -o "AmisDay.exe" main.cpp resources.o -lgdi32 -luser32 -lwinmm
if errorlevel 1 goto fail
echo.
echo Built: AmisDay.exe
pause
exit /b 0
:nogcc
echo Looked for "%BIN%\g++.exe" and did not find it.
echo In the MSYS2 UCRT64 window run:  pacman -S mingw-w64-ucrt-x86_64-toolchain
pause
exit /b 1
:fail
echo.
echo Something went wrong. Read the messages above.
pause
exit /b 1
