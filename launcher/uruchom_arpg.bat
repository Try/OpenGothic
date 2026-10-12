@echo off
rem gothic-diablo: aktualizuje gre (najnowszy build z GitHuba) i uruchamia w trybie ARPG
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0launcher.ps1" %*
if errorlevel 1 pause
