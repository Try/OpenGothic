@echo off
rem gothic-diablo: zostaw otwarte - kazdy nowy commit na GitHubie jest od razu budowany lokalnie
title Gothic Diablo - build w tle
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0dev.ps1" -Watch %*
pause
