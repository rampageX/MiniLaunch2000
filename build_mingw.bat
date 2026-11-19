@echo off
chcp 65001 >nul
setlocal

where x86_64-w64-mingw32-g++ >nul 2>nul
if %errorlevel%==0 (
  set CXX=x86_64-w64-mingw32-g++
  set WINDRES=x86_64-w64-mingw32-windres
) else (
  set CXX=g++
  set WINDRES=windres
)

%WINDRES% MiniLaunch2000.rc -O coff -o MiniLaunch2000_res.o
if errorlevel 1 goto :err
%CXX% -municode -O2 -s -static -mwindows MiniLaunch2000.cpp MiniLaunch2000_res.o -o MiniLaunch2000.exe -lcomctl32 -lshell32 -lshlwapi -lcomdlg32 -luxtheme
if errorlevel 1 goto :err

echo Build OK: MiniLaunch2000.exe
exit /b 0

:err
echo Build failed.
exit /b 1
