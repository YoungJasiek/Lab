@echo off
echo ========================================================
echo   Lab Engine ^& Frozen-Life (FL) - Web Server Preview
echo ========================================================
echo.

set PORT=8090

netstat -ano | findstr /R /C:":%PORT% .*LISTENING" >nul 2>&1
if %errorlevel% equ 0 (
    echo Serwer dokumentacji jest juz aktywny na porcie %PORT%.
    echo Otwieranie przegladarki: http://localhost:%PORT% ...
    start "" "http://localhost:%PORT%"
) else (
    echo Uruchamianie serwera dokumentacji na http://localhost:%PORT% ...
    echo Aby zakonczyc, zamknij to okno lub nacisnij Ctrl+C.
    cd /d "%~dp0"
    start "" "http://localhost:%PORT%"
    python -m http.server %PORT%
)
