@echo off
rem gothic-diablo: szybki start bez okna - aktualizacja + gra w trybie ARPG
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0launcher.ps1" -Console %*
if errorlevel 1 pause
