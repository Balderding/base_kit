@echo off
rem builds the C port: the proof tools in cport\bin and ..\bbbcore.dll (the C parts Python calls).
rem Exact floating point: no fast-math, no fused multiply-add (-ffp-contract=off), SSE2 doubles; -O2 is safe for that.
setlocal
call "%~dp0tools.cmd"
cd /d "%~dp0"
if not exist bin mkdir bin
set FL=%CLANG_FLAGS% -O2 -Wall -Wextra -Wno-unused-function -ffp-contract=off -fno-fast-math -static
clang %FL% -o bin\meshtest.exe meshtest.c mesh.c mt.c geos_dyn.c pyx.c -lkernel32 || exit /b 1
clang %FL% -o bin\rndtest.exe rndtest.c pyrandom.c pyx.c -lbcrypt -lkernel32 || exit /b 1
clang %FL% -shared -o ..\bbbcore.dll bbbcore.c mesh.c mt.c field.c livetiles.c scatter.c geos_dyn.c pyx.c pyrandom.c -lbcrypt -lkernel32 || exit /b 1
echo built cport\bin\meshtest.exe, cport\bin\rndtest.exe and bbbcore.dll
