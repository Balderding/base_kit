@echo off
rem builds bbb_kit.exe: the window AND the whole kit in C (cport\*.c: the job runner, the layouts, the builders, compose,
rem the height field, the mesher, the pictures), into the kit folder. One self-contained exe: GEOS 3.14.1 is linked in
rem (cport\geos_static, built by its build_geos.cmd; geos_link.c), the model packs are a resource (bbb_kit.rc), -static
rem takes libstdc++ / libgcc / winpthread in: it imports only Windows' own DLLs and runs from any folder.
rem Needs clang with the MinGW headers: cport\tools.cmd sets that up from the tools folder (TOOLS, X:\tools). Close a running bbb_kit.exe
rem first. The C parts must give bit-identical numbers to Python: no fast-math, no fused multiply-add.
rem   build.cmd            -> ..\bbb_kit.exe
rem   build.cmd <out.exe>  -> that file instead (a test build)
setlocal
call "%~dp0..\cport\tools.cmd"
rem BBB_EXTRA: extra compiler flags for a test build (e.g. -g for the profiler's bbb_g.exe)
if defined BBB_EXTRA set CLANG_FLAGS=%CLANG_FLAGS% %BBB_EXTRA%
cd /d "%~dp0"
set OUT=..\bbb_kit.exe
if not "%~1"=="" set OUT=%~1
rem previews.bin: the Models window's pictures, packed into the exe. Missing: a placeholder, made right after the first link
if not exist previews.bin (echo BBPV0) > previews.bin
set PASS=1
set KITVER=
if exist ..\VERSION set /p KITVER=<..\VERSION
:link
"%GCCBIN%\windres.exe" bbb_kit.rc -O coff -o bbb_kit.res || exit /b 1
set CP=..\cport
set KIT=%CP%\runner.c %CP%\draw2d.c %CP%\pipeline.c %CP%\stylefns.c %CP%\make.c %CP%\compose.c %CP%\kinds.c %CP%\builders.c ^
 %CP%\scene.c %CP%\leaf.c %CP%\place.c %CP%\solids.c %CP%\solids3d.c %CP%\samplers.c %CP%\features.c %CP%\scatter.c %CP%\shp.c %CP%\pyexact.c %CP%\pyrandom.c %CP%\pyx.c ^
 %CP%\layouts.c %CP%\layouts_gg2.c %CP%\layouts_s2.c %CP%\layouts_new.c %CP%\groundbig.c %CP%\buildcmds.c %CP%\meshglue.c %CP%\models.c ^
 %CP%\bld_features.c %CP%\bld_details.c %CP%\bld_styles2.c %CP%\bld_gg3.c %CP%\bld_scenes.c %CP%\bld_sd_a.c %CP%\bld_sd_b.c %CP%\bld_sd_c.c ^
 %CP%\bld_sd_d.c %CP%\bld_sd_e.c %CP%\bld_sd_r.c %CP%\bld_sd_models.c %CP%\bld_sd_rx.c %CP%\bld_sd_rx_badlands.c %CP%\bld_sd_rx_coast.c ^
 %CP%\bld_sd_rx_corridor.c %CP%\bld_sd_rx_cynder.c %CP%\bld_sd_rx_dungeon.c %CP%\bld_sd_rx_forest.c %CP%\bld_sd_rx_fortress.c %CP%\bld_sd_rx_frontier.c ^
 %CP%\bld_sd_rx_graveyard.c %CP%\bld_sd_rx_highland.c %CP%\bld_sd_rx_infestation.c %CP%\bld_sd_rx_maahl.c %CP%\bld_sd_rx_mine.c %CP%\bld_sd_rx_swamp.c ^
 %CP%\bld_sd_rx_targa.c %CP%\bld_sd_rx_temple.c %CP%\bld_sd_rx_trederra.c %CP%\bld_sd_rx_urban.c %CP%\bld_newstyles.c %CP%\bld_ns_ashveil.c ^
 %CP%\bld_ns_blight.c %CP%\bld_ns_ferrum.c %CP%\bld_ns_obelis.c %CP%\bld_ns_sundered.c %CP%\bld_ns_tables.c %CP%\bld_ns_thornwild.c %CP%\bld_ns_tiers.c ^
 %CP%\core.c %CP%\mesh.c %CP%\mt.c %CP%\field.c %CP%\livetiles.c %CP%\shot.c %CP%\geos_link.c %CP%\mfjoin.c %CP%\audit.c %CP%\smooth3d.c %CP%\rocks3d.c %CP%\grain.c
rem BBB_NK=1: the window's Nuklear face (gui\nk\PLAN.md), still one exe: Nuklear is compiled in
set NKF=
if "%BBB_NK%"=="1" set NKF=-DBBB_NK -Ink nk\nk_ui.c -lshlwapi
clang %CLANG_FLAGS% -O2 -Wno-unused-function -Wno-unused-parameter -Wno-unsequenced -ffp-contract=off -fno-fast-math -DKIT_VERSION=\"%KITVER%\" -mwindows -static -I%CP% -I%CP%\manifold_static\include -o "%OUT%" %NKF% bbb_kit.c live.c %KIT% %CP%\tbbcap.cpp -I%CP%\manifold_static\include_tbb bbb_kit.res -L%CP%\geos_static\lib -L%CP%\manifold_static\lib -lgeos_c -lgeos -lmanifoldc -lmanifold -lClipper2 -ltbb12 -lstdc++ -lwinpthread -ld3d11 -ldxgi -ldxguid -luuid -lgdiplus -lole32 -lgdi32 -luser32 -lshell32 -lwinhttp -lbcrypt -lkernel32 -lm || exit /b 1
rem the model pictures from the exe just built (its own models and sizes; kept ones are not drawn again), into previews.bin;
rem when they changed, linked once more so the exe carries them
if "%PASS%"=="2" goto built
set BBB_OUT_ROOT=%~dp0..\work\previews
"%OUT%" --job models --pack previews.new > "%~dp0..\work\previews_job.log" 2>&1 || (echo the model pictures failed: see work\previews_job.log & exit /b 1)
fc /b previews.new previews.bin > nul 2>&1 && (del previews.new & goto built)
move /y previews.new previews.bin > nul
set PASS=2
goto link
:built
echo built %OUT%
