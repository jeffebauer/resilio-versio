#!/bin/bash
# Round 5: rebuild and run the suites in parallel (repo root): the default (build-fit, with the plugin, so
# plugin_host_test runs) and the whole suite as if 8 / 9 / 10 shipped (build-v8..10). Logs in renders/fit_round5/;
# read the "tests passed" lines (never ctest's exit code through a pipe).
OUT=renders/fit_round5
for b in build-fit build-v8 build-v9 build-v10; do cmake --build $b > $OUT/build_$b.log 2>&1 || echo "build $b failed"; done
for b in build-fit build-v8 build-v9 build-v10; do
  ( ctest --test-dir $b > $OUT/ctest_$b.log 2>&1
    grep -hE "^FAIL" $b/Testing/Temporary/LastTest.log | sort -u > $OUT/ctest_${b}_fails.txt ) &
done
wait
for b in build-fit build-v8 build-v9 build-v10; do echo "== $b: $(grep -E 'tests passed' $OUT/ctest_$b.log)"; done
echo suites done
