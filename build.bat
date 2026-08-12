@echo off
setlocal

pushd "%~dp0" >nul
call scripts\build.bat
set BUILD_EXIT=%ERRORLEVEL%
popd >nul

exit /b %BUILD_EXIT%
