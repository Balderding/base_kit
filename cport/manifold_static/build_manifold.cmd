@echo off
rem build_manifold.cmd: Manifold v3.5.4 (github.com/elalish/manifold, Apache 2.0; O:\appsnotonc\manifold-src) as static
rem libraries for bbb_kit.exe: libmanifold.a, libmanifoldc.a (its C interface) and Clipper2 (fetched by CMake, Boost
rem Software License). No TBB: the parallel backend is off (MANIFOLD_PAR OFF), the kit already runs one base per core.
rem Output: cport\manifold_static\lib\*.a, headers in cport\manifold_static\include. Build folder: work\manifold\build.
setlocal
call O:\appsnotonc\llvm\env.cmd
set GCCBIN=O:\appsnotonc\gcc\mingw64\bin
set SRC=O:/appsnotonc/manifold-src
set OUT=%~dp0
set BLD=O:\bases\work\manifold\build
if not exist %BLD% mkdir %BLD%
set CXXFL=--target=x86_64-w64-windows-gnu --sysroot=O:/appsnotonc/gcc/mingw64
"%GCCBIN%\cmake.exe" -S %SRC% -B %BLD% -G Ninja -DCMAKE_MAKE_PROGRAM=%GCCBIN%/ninja.exe ^
  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DMANIFOLD_TEST=OFF -DMANIFOLD_PAR=OFF -DMANIFOLD_CBIND=ON ^
  -DMANIFOLD_CROSS_SECTION=ON -DMANIFOLD_USE_BUILTIN_CLIPPER2=OFF -DMANIFOLD_PYBIND=OFF -DMANIFOLD_EXPORT=OFF ^
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ ^
  "-DCMAKE_C_FLAGS=%CXXFL%" "-DCMAKE_CXX_FLAGS=%CXXFL%" ^
  "-DCMAKE_C_FLAGS_RELEASE=-O2 -DNDEBUG" "-DCMAKE_CXX_FLAGS_RELEASE=-O2 -DNDEBUG" ^
  "-DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld" "-DCMAKE_SHARED_LINKER_FLAGS=-fuse-ld=lld" ^
  -DCMAKE_AR=O:/appsnotonc/llvm/23.1.4/bin/llvm-ar.exe -DCMAKE_RANLIB=O:/appsnotonc/llvm/23.1.4/bin/llvm-ranlib.exe || exit /b 1
"%GCCBIN%\ninja.exe" -C %BLD% || exit /b 1
if not exist "%OUT%lib" mkdir "%OUT%lib"
if not exist "%OUT%include" mkdir "%OUT%include"
for /r %BLD% %%f in (*.a) do copy /y "%%f" "%OUT%lib\" >nul
xcopy /y /e /i /q O:\appsnotonc\manifold-src\bindings\c\include "%OUT%include" >nul
echo built Manifold into %OUT%lib
dir /b "%OUT%lib"
