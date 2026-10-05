#!/bin/bash
# Round 5 flash: per-function size difference (bytes) between two firmware ELFs (rv:: and everything else),
# biggest changes first. Usage: docs/prototypes/wellspring-fit-5/fwdiff.sh BEFORE.elf AFTER.elf
export PATH="$HOME/.local/arm-gnu-toolchain/bin:$PATH"
python3 - "$1" "$2" <<'EOF'
import subprocess, sys, collections
def sizes(p):
    d = collections.Counter()
    for l in subprocess.run(['arm-none-eabi-nm', '--size-sort', '-S', '-C', p], capture_output=True, text=True).stdout.splitlines():
        parts = l.split(' ', 3)
        if len(parts) == 4 and parts[2] in 'tTrRdDbB' and parts[2] not in 'bB':
            d[parts[3]] += int(parts[1], 16)
    return d
a, b = sizes(sys.argv[1]), sizes(sys.argv[2])
diff = sorted(((b[k] - a[k], k) for k in set(a) | set(b) if b[k] != a[k]), key=lambda x: -abs(x[0]))
print('total', sum(x for x, _ in diff))
for x, k in diff[:40]:
    print(f'{x:+6d}  {k[:110]}')
EOF
