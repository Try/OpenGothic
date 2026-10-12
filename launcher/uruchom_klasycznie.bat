@echo off
rem gothic-diablo: szybki start bez okna - aktualizacja + gra bez trybu ARPG
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0launcher.ps1" -Console -Classic %*
if errorlevel 1 pause
