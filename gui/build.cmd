@echo off
rem builds bbb_kit.exe (the window in front of the kit) into the kit folder. Needs clang with the MinGW headers:
rem O:\appsnotonc\llvm\env.cmd sets that up on this PC. Close a running bbb_kit.exe first.
setlocal
call O:\appsnotonc\llvm\env.cmd
cd /d "%~dp0"
O:\appsnotonc\gcc\mingw64\bin\windres.exe bbb_kit.rc -O coff -o bbb_kit.res || exit /b 1
clang %CLANG_FLAGS% -O2 -Wall -Wextra -mwindows -static -o ..\bbb_kit.exe bbb_kit.c bbb_kit.res -lgdiplus -lgdi32 -luser32 -lshell32 -lkernel32 -lm || exit /b 1
echo built ..\bbb_kit.exe
