@echo off
echo ========================================================
echo   Lab Wiki - Web Server Preview
echo ========================================================
echo.

set PORT=8990
if not "%~1"=="" set PORT=%~1

cd /d "%~dp0"
npm run dev -- --port %PORT%