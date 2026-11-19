#!/bin/sh
# the end-to-end proof: the same bases through the normal runner, Python mesher vs C mesher; the STL files must be
# byte-identical. PYTHONHASHSEED=0 because a few styles' layouts are not repeatable across processes otherwise.
cd /o/bases || exit 1
PY=/c/Python315/python.exe; OUT=work/cport/stl; rm -rf "$OUT"; mkdir -p "$OUT/py" "$OUT/c"
export PYTHONHASHSEED=0
for a in "mine 30" "urban 40" "badlands 60" "swamp 60" "forest 100" "corridor 100" "temple 160" "graveyard 130" "coast 90x52"; do
  set -- $a
  for mode in py c; do
    if [ $mode = py ]; then export KIT_CMESH=0; else export KIT_CMESH=require; fi
    $PY run_set.py "$1" 2 --size "$2" --detail 4 --seed cproof -q > "$OUT/$mode/$1_$2.log" 2>&1
    d=$(ls -td out/run_*_cproof | head -1)
    for f in "$d/$1/"*.stl; do cp "$f" "$OUT/$mode/$(basename "$f")"; done
    t=$(grep -E "^ *(OK|FLAG|FAIL) " "$OUT/$mode/$1_$2.log" | awk '{s = s $3 " "} END {print s}')
    echo "$mode $1 $2: $t"
  done
done
same=0; diff=0
for f in "$OUT/py/"*.stl; do b=$(basename "$f"); if cmp -s "$f" "$OUT/c/$b"; then same=$((same+1)); else diff=$((diff+1)); echo "DIFF $b"; fi; done
echo "$same STL files the same, $diff different"
