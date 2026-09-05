@echo off
echo ========================================================
echo   Lab Engine ^& Frozen-Life (FL) - Web Server Preview
echo ========================================================
echo.

netstat -ano | findstr /R /C:":8080 .*LISTENING" >nul 2>&1
if %errorlevel% equ 0 (
    echo Serwer jest juz aktywny na porcie 8080.
    echo Otwieranie przegladarki: http://localhost:8080 ...
    start "" "http://localhost:8080"
) else (
    echo Uruchamianie lokalnego serwera na http://localhost:8080 ...
    echo Aby zakonczyc, zamknij to okno lub nacisnij Ctrl+C.
    cd /d "%~dp0"
    start "" "http://localhost:8080"
    python -m http.server 8080
)
