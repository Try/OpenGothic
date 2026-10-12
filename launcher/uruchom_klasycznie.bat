@echo off
rem gothic-diablo: aktualizuje gre i uruchamia bez trybu ARPG (oryginalna kamera)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0launcher.ps1" -Classic %*
if errorlevel 1 pause
