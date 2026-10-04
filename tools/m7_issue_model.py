#!/usr/bin/env python3
"""Crude in-order Cortex-M7 issue model for firmware hot loops (estimates, not a simulator).

Used for run 16 (firmware/README.md "Run 16 (prepared)") to compare two builds of the same
code without the chip. It reads `arm-none-eabi-objdump -d -l -C --no-show-raw-insn` output and
replays the instructions with addresses in [lo, hi) in address order: branches are skipped
(the fall-through path), so pick a range that is the path you care about (a loop body, or a
straight stretch), and compare builds over the same stretch of source.

    arm-none-eabi-objdump -d -l -C --no-show-raw-insn firmware/build/resilio_versio_profile.elf \\
        --disassemble='rv::Spring::process(float const*, float const*, float const*, float const*, float const*, float*, int)' > spring.s
    python3 tools/m7_issue_model.py spring.s 8007b8e 8007bce 6       # a loop body, 6 passes
    python3 tools/m7_issue_model.py spring.s 80077d6 8007b36 3 20    # + the 20 source lines that stall most

Model: in order, up to 2 instructions per cycle, at most 1 FP data-processing and 1 memory
access per cycle; an instruction issues once its sources are ready. Latencies: FP add / sub /
mul / cvt 3, fused 6, vdiv / vsqrt 14 (one at a time), loads 2, integer multiply 2, everything
else 1; vmrs waits for the vcmp. Not modelled: branch costs (mispredicts!), memory wait states,
store-to-load forwarding, cache. Calibration: it reads main's Chirp section loop at 14 cycles,
what run 8 measured on the chip (firmware/m3_bench.cpp "pipe"), and run 15's `tilt` and
Spring shares within ~0.5 points.
"""
import collections
import re
import sys


def load(path, lo, hi):
    ins, srcof, src = [], {}, '?'
    for line in open(path):
        ms = re.match(r'^(\S+\.(?:h|cpp|tcc)):(\d+)', line)
        if ms:
            src = ms.group(1).split('/')[-1] + ':' + ms.group(2)
            continue
        m = re.match(r'^\s+([0-9a-f]+):\s+(\S+)\s*([^@;]*)', line)
        if m and not m.group(2).startswith('.'):
            a = int(m.group(1), 16)
            if lo <= a < hi:
                ins.append((a, m.group(2), m.group(3).strip()))
                srcof[a] = src
    return ins, srcof


def regs(s):
    return re.findall(r'\b([rsd]\d+|sp|lr|ip|fp|sl|pc)\b', s)


STORES = {'vstr', 'str', 'strb', 'strh', 'strd', 'vstmia', 'vstmdb', 'stmia', 'stmdb', 'push', 'vpush'}
LOADS = {'vldr', 'ldr', 'ldrb', 'ldrh', 'ldrd', 'vldmia', 'vldmdb', 'ldmia', 'ldmdb', 'pop', 'vpop', 'ldrsh', 'ldrsb'}
CMPS = {'cmp', 'cmn', 'tst', 'teq', 'vcmp', 'vcmpe'}
FP3 = {'vadd', 'vsub', 'vmul', 'vnmul', 'vneg', 'vabs', 'vcvt', 'vrintp', 'vrintm', 'vrintz', 'vrintr', 'vrinta', 'vrintn'}


def run(ins, srcof, passes):
    ready, stalls, per = {}, collections.Counter(), []
    state = {'cycle': 0, 'n': 0, 'fp': 0, 'mem': 0}
    divfree = 0

    def advance(to):
        if to > state['cycle']:
            state.update(cycle=to, n=0, fp=0, mem=0)

    for p in range(passes):
        start = state['cycle']
        for a, op, args in ins:
            if (op.startswith('b') and not op.startswith(('bic', 'bfi', 'bfc'))) or op.startswith(('cb', 'it')) or op == 'nop':
                continue
            base = op.split('.')[0]
            rl = regs(args)
            is_store, is_load, is_cmp = base in STORES, base in LOADS, base in CMPS
            fp = base.startswith('v') and not is_load and not is_store and base not in ('vmrs', 'vmsr')
            if base == 'vmov' and len(rl) == 2 and (rl[0][0] == 'r' or rl[1][0] == 'r'):
                fp = False
            if is_store:
                dests, srcs = ([rl[-1]] if '!' in args and base in ('vstmia', 'vstmdb', 'stmia', 'stmdb') else []), rl
            elif is_cmp:
                dests, srcs = ['flags_fp' if base.startswith('v') else 'flags'], rl
            elif base == 'vmrs':
                dests, srcs = ['flags'], ['flags_fp']
            elif base in ('pop', 'vpop'):
                dests, srcs = rl, ['sp']
            elif base in ('vldmia', 'vldmdb', 'ldmia', 'ldmdb'):
                dests, srcs = rl[1:] + ([rl[0]] if '!' in args else []), [rl[0]]
            else:
                dests, srcs = rl[:1], rl[1:]
                if base.startswith('vsel'):
                    srcs = srcs + ['flags']
                if base.startswith(('vmla', 'vfma', 'mla')):
                    srcs = srcs + rl[:1]
            earliest = max([ready.get(r, 0) for r in srcs] + [0])
            if base in ('vdiv', 'vsqrt'):
                earliest = max(earliest, divfree)
            before = state['cycle']
            advance(earliest)
            while state['n'] >= 2 or (fp and state['fp'] >= 1) or ((is_load or is_store) and state['mem'] >= 1):
                advance(state['cycle'] + 1)
            if p == passes - 1:
                stalls[srcof.get(a, '?')] += state['cycle'] - before
            state['n'] += 1
            state['fp'] += fp
            state['mem'] += is_load or is_store
            if base in ('vdiv', 'vsqrt'):
                lat = 14
                divfree = state['cycle'] + 14
            elif is_load:
                lat = 2
            elif fp and base in FP3:
                lat = 3
            elif fp and base.startswith(('vfm', 'vml', 'vnml')):
                lat = 6
            elif base in ('mul', 'mla', 'mls', 'muls', 'umull', 'smull'):
                lat = 2
            else:
                lat = 1
            for d in dests:
                ready[d] = state['cycle'] + lat
        per.append(state['cycle'] - start)
    return per, stalls


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        sys.exit(1)
    ins, srcof = load(sys.argv[1], int(sys.argv[2], 16), int(sys.argv[3], 16))
    passes = int(sys.argv[4]) if len(sys.argv) > 4 else 1
    per, stalls = run(ins, srcof, passes)
    if len(sys.argv) > 5:
        for k, v in stalls.most_common(int(sys.argv[5])):
            print(f'{v:4d} stall cycles  {k}')
    print('instructions', len(ins), 'cycles per pass', per[-1], '(passes:', ' '.join(map(str, per)) + ')')


if __name__ == '__main__':
    main()
