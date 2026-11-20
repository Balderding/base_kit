@echo off
rem builds bbb_kit.exe: the window AND the whole kit in C (cport\*.c: the job runner, the layouts, the builders, compose,
rem the height field, the mesher, the pictures), into the kit folder. One self-contained exe: GEOS 3.14.1 is linked in
rem (cport\geos_static, built by its build_geos.cmd; geos_link.c), the model packs are a resource (bbb_kit.rc), -static
rem takes libstdc++ / libgcc / winpthread in: it imports only Windows' own DLLs and runs from any folder.
rem Needs clang with the MinGW headers: O:\appsnotonc\llvm\env.cmd sets that up on this PC. Close a running bbb_kit.exe
rem first. The C parts must give bit-identical numbers to Python: no fast-math, no fused multiply-add.
rem   build.cmd            -> ..\bbb_kit.exe
rem   build.cmd <out.exe>  -> that file instead (a test build)
setlocal
call O:\appsnotonc\llvm\env.cmd
cd /d "%~dp0"
set OUT=..\bbb_kit.exe
if not "%~1"=="" set OUT=%~1
O:\appsnotonc\gcc\mingw64\bin\windres.exe bbb_kit.rc -O coff -o bbb_kit.res || exit /b 1
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
 %CP%\core.c %CP%\mesh.c %CP%\mt.c %CP%\field.c %CP%\livetiles.c %CP%\shot.c %CP%\geos_link.c %CP%\mfjoin.c %CP%\smooth3d.c %CP%\rocks3d.c
clang %CLANG_FLAGS% -O2 -Wno-unused-function -Wno-unused-parameter -Wno-unsequenced -ffp-contract=off -fno-fast-math -mwindows -static -I%CP% -I%CP%\manifold_static\include -o "%OUT%" bbb_kit.c live.c %KIT% bbb_kit.res -L%CP%\geos_static\lib -L%CP%\manifold_static\lib -lgeos_c -lgeos -lmanifoldc -lmanifold -lClipper2 -lstdc++ -lwinpthread -ld3d11 -ldxgi -ldxguid -luuid -lgdiplus -lgdi32 -luser32 -lshell32 -lwinhttp -lbcrypt -lkernel32 -lm || exit /b 1
echo built %OUT%
