@echo off
title Frozen-Life Launcher
cd /d "%~dp0"
if exist "demo\FrozenLife_Demo.exe" (
    cd demo
    call run_demo.bat
) else (
    echo Launching with engine runner...
    start "" "..\..\build\Release\LabPlayer.exe" --config game.config
)
