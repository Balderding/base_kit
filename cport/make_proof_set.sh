#!/bin/sh
# the C mesher's proof set: input + Python's answer for a spread of bases (fixed seeds, PYTHONHASHSEED=0)
cd /o/bases || exit 1
OUT=work/cport/mesh; mkdir -p "$OUT"
PY="${TOOLS:-/x/tools}/python315/python.exe"
export PYTHONHASHSEED=0 KIT_QUIET=1 KIT_WORKER=1 SALT=proof1 SETDIR="$(cd "$(dirname "$0")/.." && pwd -W)/work/cport/out"
BASE_MM=30  $PY cport/dump_mesh.py $OUT mine 3 urban 8
BASE_MM=60  $PY cport/dump_mesh.py $OUT badlands 12 swamp 17
BASE_MM=100 $PY cport/dump_mesh.py $OUT forest 6 corridor 19
BASE_MM=160 $PY cport/dump_mesh.py $OUT temple 14
BASE_MM=33.3333 KIT_SCALE=3 KIT_FINAL_MM=100 $PY cport/dump_mesh.py $OUT highland 9
BASE_MM=90 KIT_OVAL=90x52 $PY cport/dump_mesh.py $OUT coast 4
exit 0
