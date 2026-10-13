@echo off
rem gothic-diablo: jednorazowa instalacja lokalnego builda (MSYS2 + kompilator, klon repo, pierwszy build)
title Gothic Diablo - instalacja lokalnego builda
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0dev.ps1" -Setup %*
pause
