@echo off
rem build_geos.cmd: GEOS 3.14.1 (%TOOLS%\geos-src, see cport\tools.cmd) as static libraries for the C port, so bbb_kit.exe needs no
rem geos_c.dll. Output: cport\geos_static\lib\libgeos_c.a + libgeos.a, headers in cport\geos_static\include.
rem Steps: patch_geos.py copies the source to work\cscene\geosstatic\src-msvc and swaps in msvc_stl.h (MSVC's
rem std::sort, std::hash and unordered_map order), then CMake + Ninja build it in work\cscene\geosstatic\build-msvc.
rem
rem Same numbers as shapely's geos_c.dll (built by MSVC, /O2 /fp:precise, doubles in SSE2):
rem   -ffp-contract=off -fno-fast-math   no fused multiply-add, IEEE arithmetic (= /fp:precise on x64)
rem   -O2 -DNDEBUG -DUSE_UNSTABLE_GEOS_CPP_API   the wheel's Release definitions
rem   -fno-builtin-sin/cos/...                    nothing folded or swapped (sincos): the UCRT math the wheel calls
rem   TrianglePredicate.cpp with -mlong-double-64: MSVC's long double IS double; MinGW's is 80-bit x87, which
rem     would change the in-circle test of the Delaunay / Voronoi code.
setlocal
call "%~dp0..\tools.cmd"
set ORIG=%TOOLS%\geos-src
set SRC=%~dp0..\..\work\cscene\geosstatic\src-msvc
set OUT=%~dp0
set BLD=%~dp0..\..\work\cscene\geosstatic\build-msvc
if not exist %BLD% mkdir %BLD%
"%PYTHON%" -I "%~dp0patch_geos.py" %ORIG% %SRC% > %BLD%\..\patch_log.txt || exit /b 1
set FPFL=-ffp-contract=off -fno-fast-math -fno-builtin-sin -fno-builtin-cos -fno-builtin-atan2 -fno-builtin-pow -fno-builtin-log -fno-builtin-log10 -fno-builtin-exp2 -fno-builtin-exp
set CXXFL=--target=x86_64-w64-windows-gnu --sysroot=%TOOLSF%/gcc/mingw64 %FPFL%
"%GCCBIN%\cmake.exe" -S %SRC% -B %BLD% -G Ninja -DCMAKE_MAKE_PROGRAM=%GCCBIN%/ninja.exe ^
  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DBUILD_BENCHMARKS=OFF -DBUILD_GEOSOP=OFF ^
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ ^
  "-DCMAKE_C_FLAGS=%CXXFL%" "-DCMAKE_CXX_FLAGS=%CXXFL%" ^
  "-DCMAKE_C_FLAGS_RELEASE=-O2 -DNDEBUG" "-DCMAKE_CXX_FLAGS_RELEASE=-O2 -DNDEBUG" ^
  "-DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld" "-DCMAKE_SHARED_LINKER_FLAGS=-fuse-ld=lld" ^
  -DCMAKE_AR=%TOOLSF%/llvm/23.1.4/bin/llvm-ar.exe -DCMAKE_RANLIB=%TOOLSF%/llvm/23.1.4/bin/llvm-ranlib.exe || exit /b 1
"%GCCBIN%\ninja.exe" -C %BLD% geos geos_c || exit /b 1
rem the in-circle predicate again, with MSVC's 64-bit long double, replacing its object in libgeos.a
clang++ %CXXFL% -O2 -DNDEBUG -DUSE_UNSTABLE_GEOS_CPP_API -DHAVE_FENV -std=c++17 -mlong-double-64 -I%SRC%/include -I%BLD%/include ^
  -c %SRC%/src/triangulate/quadedge/TrianglePredicate.cpp -o %BLD%\TrianglePredicate.cpp.obj || exit /b 1
if not exist "%OUT%lib" mkdir "%OUT%lib"
if not exist "%OUT%include" mkdir "%OUT%include"
copy /y %BLD%\lib\libgeos.a "%OUT%lib\libgeos.a" >nul || exit /b 1
copy /y %BLD%\lib\libgeos_c.a "%OUT%lib\libgeos_c.a" >nul || exit /b 1
"%TOOLS%\llvm\23.1.4\bin\llvm-ar.exe" r "%OUT%lib\libgeos.a" %BLD%\TrianglePredicate.cpp.obj || exit /b 1
copy /y %BLD%\capi\geos_c.h "%OUT%include\geos_c.h" >nul || exit /b 1
echo built %OUT%lib\libgeos.a and libgeos_c.a
