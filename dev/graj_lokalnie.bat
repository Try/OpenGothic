@echo off
rem gothic-diablo: lokalny build - pobiera najnowszy commit z GitHuba, buduje przyrostowo i uruchamia gre
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0dev.ps1" %*
if errorlevel 1 pause
