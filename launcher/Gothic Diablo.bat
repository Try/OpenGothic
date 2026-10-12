@echo off
rem gothic-diablo: okno launchera (aktualizacja, wybor wersji, start gry)
start "" powershell -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "%~dp0launcher.ps1" %*
