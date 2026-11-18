@echo off
rem Make bases: Qwen designs each scene, the kit builds it and puts it through the print check. Everything is explained on screen.
rem Options: bbb_kit.cmd --styles mine,swamp,temple --size 100 (or 60,100 or all) --basestobuild 4 --build 2 --mini biped
rem --mini: biped wide walker tripod quad many scorpion crawler tracked hover none mix
title Balderding's Ballin Base Kit - Save time and money.
mode con: cols=170 lines=50
cd /d "%~dp0"
set PYTHONWARNINGS=ignore
C:\Python315\python.exe tools\bbb_kit.py %*
echo.
pause
