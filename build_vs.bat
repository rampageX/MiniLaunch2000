@echo off
chcp 65001 >nul
rem 请在 “x64 Native Tools Command Prompt for VS” 里运行
rc /nologo /fo MiniLaunch2000.res MiniLaunch2000.rc
if errorlevel 1 goto :err
cl /EHsc /DUNICODE /D_UNICODE /O2 MiniLaunch2000.cpp MiniLaunch2000.res /link /SUBSYSTEM:WINDOWS comctl32.lib shell32.lib shlwapi.lib comdlg32.lib uxtheme.lib /OUT:MiniLaunch2000.exe
if errorlevel 1 goto :err

echo Build OK: MiniLaunch2000.exe
exit /b 0

:err
echo Build failed.
exit /b 1
