#!/bin/bash
# Round 5 gates for the proposed voicings (repo root, after cmake --build build-r):
#   M6 Ringing + Howl grids for 7-10 (renders/fit_round5/m6, WAVs deleted), then the whole suite as if
#   8 / 9 / 10 shipped (scratch builds build-v8..10: cmake ... -DCMAKE_CXX_FLAGS=-DRV_TANK_DEFAULT_VOICING=N),
#   each logged to renders/fit_round5/ctest_vN.log; read the "tests passed" lines.
set -u
OUT=renders/fit_round5
rm -rf $OUT/m6
docs/prototypes/wellspring-fit-3/m6_grid.sh "7 8 9 10" $OUT/m6 > $OUT/m6.log 2>&1
for v in 7 8 9 10; do echo "== v$v"; python3 docs/prototypes/wellspring-fit-3/m6_summary.py $OUT/m6/v$v; done >> $OUT/m6.log 2>&1
for v in 8 9 10; do
  cmake --build build-v$v > /dev/null 2>&1
  ctest --test-dir build-v$v > $OUT/ctest_v$v.log 2>&1
  grep -hE "^FAIL" build-v$v/Testing/Temporary/LastTest.log | sort -u > $OUT/ctest_v${v}_fails.txt
done
echo gates done
