// Tiny diagrams (DESIGN-v2 V10): small line drawings of what a control or a demo does,
// computed here so they stay simple and true to the manual. Each is drawn in a 160 × 64
// box; `ink` paths take the text colour, `hot` paths the signal red (one highlight).
export type DiagramKind =
  | 'decay' | 'throw' | 'echo' | 'bigknob' | 'wobble'
  | 'tanks' | 'attitude' | 'splash' | 'hold' | 'howl' | 'drywet';

export interface Shape { ink: string[]; hot: string[]; faint?: string[]; label: string }

const W = 160;
const MID = 34;
const f = (n: number) => +n.toFixed(1);
const line = (pts: [number, number][]) => pts.map(([x, y], i) => `${i ? 'L' : 'M'}${f(x)} ${f(y)}`).join('');

/** A ringing tail: an oscillation under an exponential envelope, from x0 to x1. */
function ring(x0: number, x1: number, amp: number, cycles: number, fall = 3.2, y = MID): string {
  const pts: [number, number][] = [];
  const n = Math.max(24, Math.round((x1 - x0) * 1.5));
  for (let i = 0; i <= n; i++) {
    const u = i / n;
    pts.push([x0 + (x1 - x0) * u, y - amp * Math.exp(-fall * u) * Math.sin(2 * Math.PI * cycles * u)]);
  }
  return line(pts);
}

const tick = (x: number, h: number, base = 58) => `M${f(x)} ${base}V${f(base - h)}`;

export function shape(kind: DiagramKind): Shape {
  switch (kind) {
    case 'decay':
      return {
        label: 'A hit, then the spring’s tail ringing on and fading away.',
        hot: [`M10 ${MID - 26}V${MID + 26}`],
        ink: [ring(10, 154, 24, 16, 3.4)],
      };
    case 'throw': {
      // Lane 1: the send gate opens for one hit. Lane 2: hits; only that one rings on.
      const hits = [16, 46, 76, 106, 136];
      return {
        label: 'The send opens for one hit, closes, and that hit’s tail rings on.',
        faint: ['M4 12H68M80 12H156'],
        hot: ['M68 12V4H80V12'],
        ink: [...hits.map((x) => tick(x, 8)), ring(74, 156, 14, 11, 2.6, 34)],
      };
    }
    case 'echo': {
      const xs = [10, 34, 58, 82, 106, 130, 154];
      return {
        label: 'One hit, then echo repeats, each quieter than the last.',
        hot: [tick(xs[0], 50)],
        ink: xs.slice(1).map((x, i) => tick(x, 50 * Math.pow(0.68, i + 1))),
      };
    }
    case 'bigknob': {
      // High-pass responses on a log axis: the low cut sweeping right, with a small ring at the corner.
      const curve = (fc: number) => {
        const pts: [number, number][] = [];
        for (let x = 4; x <= 156; x += 2) {
          const r = (x - fc) / 9;
          const mag = 1 / (1 + Math.exp(-r * 1.6));
          const bump = 6 * Math.exp(-(r * r) / 1.4);
          pts.push([x, 58 - 42 * mag - bump]);
        }
        return line(pts);
      };
      return {
        label: 'The Big Knob: a steep low cut sweeping upward, with a small ring at the cut.',
        faint: [curve(30), curve(62)],
        hot: [curve(96)],
        ink: ['M34 6H96M90 2.5L96 6L90 9.5'],
      };
    }
    case 'wobble': {
      const pts: [number, number][] = [];
      for (let x = 4; x <= 80; x += 1) {
        const u = x / 80;
        pts.push([x, MID - 9 * Math.sin(u * 5.1) * Math.cos(u * 2.3 + 0.4) - 4 * Math.sin(u * 11)]);
      }
      const warble: [number, number][] = [];
      for (let x = 80; x <= 156; x += 1) warble.push([x, MID - 10 * Math.sin(((x - 80) / 76) * 2 * Math.PI * 5)]);
      return {
        label: 'WOBBLE: a slow, uneven tape drift in pitch, then a steady warble.',
        faint: [`M4 ${MID}H156`],
        ink: [line(pts)],
        hot: [line(warble)],
      };
    }
    case 'tanks': {
      const sparse = [10, 22, 34, 48, 62].map((x, i) => tick(x, 44 * Math.pow(0.7, i)));
      const dense = Array.from({ length: 16 }, (_, i) => tick(88 + i * 4.4, 40 * Math.pow(0.86, i)));
      return {
        label: 'TANK 1, sparse: you hear each echo. TANK 2, dense: a smooth tail.',
        ink: sparse,
        hot: dense,
        faint: ['M80 4V60'],
      };
    }
    case 'attitude': {
      // Transfer curves, input across, output up: clean (straight), tape (soft), valve (hard, lopsided).
      const box = (x0: number, fn: (v: number) => number) => {
        const pts: [number, number][] = [];
        for (let i = 0; i <= 40; i++) {
          const v = -1 + i / 20;
          pts.push([x0 + 22 + v * 22, MID - fn(v) * 24]);
        }
        return line(pts);
      };
      return {
        label: 'CLEAN stays straight, TAPE rounds off, VALVE clips hard and lopsided.',
        faint: [`M4 ${MID}H48M58 ${MID}H102M112 ${MID}H156`],
        ink: [box(4, (v) => v * 0.95), box(58, (v) => Math.tanh(v * 2.2) / Math.tanh(2.2))],
        hot: [box(112, (v) => (v > 0 ? Math.min(1, v * 2.4) * 0.9 : Math.max(-1, v * 1.6)))],
      };
    }
    case 'splash': {
      const xs = [8, 26, 44, 62, 84, 102, 120, 138];
      return {
        label: 'The same hits, first gentle, then hitting the springs much harder.',
        ink: xs.slice(0, 4).map((x) => ring(x, x + 16, 8, 3, 3)),
        hot: xs.slice(4).map((x) => ring(x, x + 18, 24, 3.5, 2.6)),
        faint: ['M74 4V60'],
      };
    }
    case 'hold': {
      const pts: [number, number][] = [];
      for (let x = 6; x <= 156; x += 2) {
        const rise = 1 - Math.exp(-(x - 6) / 14);
        const fade = x > 64 ? Math.exp(-(x - 64) / 600) : 1;
        pts.push([x, 58 - 40 * rise * fade]);
      }
      return {
        label: 'The Hold: the tail blooms into a bed that keeps ringing after the playing stops.',
        faint: ['M64 4V60'],
        ink: [line(pts.filter(([x]) => x <= 64))],
        hot: [line(pts.filter(([x]) => x >= 64))],
      };
    }
    case 'howl': {
      const rough: [number, number][] = [];
      for (let x = 12; x <= 104; x += 2) {
        const env = Math.min(1, (x - 12) / 30);
        rough.push([x, MID - env * 20 * Math.sin(x * 0.9) * (0.7 + 0.3 * Math.sin(x * 0.23))]);
      }
      return {
        label: 'The Howl: one hit tips the tank into its own rough feedback; pull DECAY back and it falls away.',
        ink: [tick(10, 50, 58), ring(104, 156, 16, 6, 3.8)],
        hot: [line(rough)],
      };
    }
    case 'drywet': {
      const xs = [8, 28, 48, 68, 86, 106, 126, 146];
      return {
        label: 'Dry hits on their own, then the same hits with spring tails.',
        ink: xs.map((x) => `M${x} ${MID - 18}V${MID + 18}`),
        hot: xs.slice(4).map((x) => ring(x, Math.min(x + 22, W - 2), 12, 4, 2.4)),
        faint: ['M77 4V60'],
      };
    }
  }
}
