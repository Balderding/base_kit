@echo off
rem builds bbb_kit.exe (the window in front of the kit, and the kit's C parts: bbb_kit.exe --core-...) into the kit
rem folder. Needs clang with the MinGW headers: O:\appsnotonc\llvm\env.cmd sets that up on this PC. Close a running
rem bbb_kit.exe first. The C parts must give bit-identical numbers to Python: no fast-math, no fused multiply-add.
setlocal
call O:\appsnotonc\llvm\env.cmd
cd /d "%~dp0"
O:\appsnotonc\gcc\mingw64\bin\windres.exe bbb_kit.rc -O coff -o bbb_kit.res || exit /b 1
set CP=..\cport
clang %CLANG_FLAGS% -O2 -Wall -Wextra -Wno-unused-function -ffp-contract=off -fno-fast-math -mwindows -static -I%CP% -o ..\bbb_kit.exe bbb_kit.c live.c %CP%\core.c %CP%\mesh.c %CP%\mt.c %CP%\field.c %CP%\livetiles.c %CP%\geos_dyn.c %CP%\pyx.c %CP%\shot.c bbb_kit.res -ld3d11 -ldxgi -ldxguid -luuid -lgdiplus -lgdi32 -luser32 -lshell32 -lkernel32 -lm || exit /b 1
echo built ..\bbb_kit.exe
