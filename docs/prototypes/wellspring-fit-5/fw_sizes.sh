#!/bin/bash
# Round 5: firmware sizes with each tank voicing as the default (as if picked), then the real default again.
# Usage (repo root): docs/prototypes/wellspring-fit-5/fw_sizes.sh LIBDAISY_DIR "8 9 10"
# Wipes firmware/build/ (this checkout's firmware objects only) between builds: flags alone don't trigger a
# rebuild. The as-if builds pass the voicing through the environment's C_DEFS (libDaisy's Makefile starts from
# it), release and profile only (m0test has no Tank).
export PATH="$HOME/.local/arm-gnu-toolchain/bin:$PATH"
LD=$1
for v in $2; do
  rm -rf firmware/build
  echo "== voicing $v"
  for m in release profile; do
    C_DEFS="-DRV_TANK_DEFAULT_VOICING=$v" make -C firmware MODE=$m LIBDAISY_DIR="$LD" > /tmp/r5fw_${v}_$m.log 2>&1
    echo "$m: $(stat -f %z firmware/build/resilio_versio$([ $m = profile ] && echo _profile).bin 2>/dev/null) B"
    grep -E " error|overflowed" /tmp/r5fw_${v}_$m.log | head -3
  done
done
rm -rf firmware/build
make -C firmware all-variants LIBDAISY_DIR="$LD" > /tmp/r5fw_default.log 2>&1
echo "== default"
grep -E "error|resilio_versio.*bytes" /tmp/r5fw_default.log
