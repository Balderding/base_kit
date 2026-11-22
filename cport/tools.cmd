@echo off
rem tools.cmd: where the kit's compiling tools are, in ONE place (2026-11-22: moved to X:\tools). The build scripts call it.
rem Set TOOLS before calling to use another folder. It sets up clang (LLVM 23.1.4, MinGW target, lld) like llvm\env.cmd did:
rem   TOOLS   the tools folder (llvm, gcc, geos-src, manifold-src, python315)
rem   TOOLSF  the same with forward slashes (for CMake and clang's --sysroot)
rem   GCCBIN  gcc\mingw64\bin (windres, cmake, ninja)
rem   PYTHON  the Python the build scripts run
rem   CLANG_FLAGS, PATH, TMP / TEMP / TMPDIR
if not defined TOOLS set TOOLS=X:\tools
set TOOLSF=%TOOLS:\=/%
set PATH=%TOOLS%\llvm\23.1.4\bin;%PATH%
set TMP=%TOOLS%\llvm\tmp
set TEMP=%TOOLS%\llvm\tmp
set TMPDIR=%TOOLS%\llvm\tmp
set CLANG_FLAGS=--target=x86_64-w64-windows-gnu --sysroot=%TOOLSF%/gcc/mingw64 -fuse-ld=lld
set GCCBIN=%TOOLS%\gcc\mingw64\bin
set PYTHON=%TOOLS%\python315\python.exe
