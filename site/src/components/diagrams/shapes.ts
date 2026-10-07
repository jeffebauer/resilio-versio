// Tiny diagrams (DESIGN-v2 V10): small line drawings of what a control or a demo does,
// computed here so they stay simple and true to the manual. Each is drawn in a 160 × 64
// box; `ink` paths take the text colour, `hot` paths the signal red (one highlight).
export type DiagramKind =
  | 'decay' | 'throw' | 'echo' | 'bigknob' | 'wobble'
  | 'tanks' | 'attitude' | 'splash' | 'hold' | 'howl' | 'drywet' | 'cv'
  | 'mulaw' | 'bits12' | 'bits10' | 'rate' | 'wetonly';

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

/** µ-law rounding to `levels` steps a side: fine near silence, coarse near full scale. The
 *  real box is µ 255 with 2,048 or 512 steps a side; a drawing has room for a handful, so the
 *  bit-depth tiles use a gentler µ to keep the steps readable. */
function muQuantise(v: number, levels: number, mu = 255): number {
  const c = Math.sign(v) * Math.log1p(mu * Math.abs(v)) / Math.log1p(mu);
  const q = Math.round(c * levels) / levels;
  return Math.sign(q) * (Math.pow(1 + mu, Math.abs(q)) - 1) / mu;
}

/** A signal drawn as the converter's staircase: held flat between samples, jumping to each new step. */
function stairs(fn: (u: number) => number, x0: number, x1: number, samples: number, levels: number, amp: number, mu = 255, y = MID): string {
  let d = '';
  for (let i = 0; i <= samples; i++) {
    const x = x0 + ((x1 - x0) * i) / samples;
    const yy = y - amp * muQuantise(fn(i / samples), levels, mu);
    d += i ? `H${f(x)}V${f(yy)}` : `M${f(x)} ${f(yy)}`;
  }
  return d + `H${f(x1)}`;
}

/** The same signal, smooth (for the faint reference under a staircase). */
function smooth(fn: (u: number) => number, x0: number, x1: number, amp: number, y = MID): string {
  const pts: [number, number][] = [];
  for (let i = 0; i <= 120; i++) pts.push([x0 + ((x1 - x0) * i) / 120, y - amp * fn(i / 120)]);
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
        label: 'The “Big Knob”: a steep low cut sweeping upward, with a small ring at the cut.',
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
      // One hit, then the tank's own feedback building up and holding: the envelope grows
      // and levels off (it never runs away), the pitch creeps up, the edge stays rough.
      const env = (x: number) => 1 - Math.exp(-(x - 16) / 28);
      const rough: [number, number][] = [];
      const top: [number, number][] = [];
      const bottom: [number, number][] = [];
      let phase = 0;
      for (let x = 16; x <= 156; x += 0.5) {
        phase += 0.5 * (0.42 + 0.3 * ((x - 16) / 140));
        const a = 24 * env(x);
        rough.push([x, MID - a * Math.sin(phase) * (0.82 + 0.18 * Math.sin(x * 0.21))]);
        if (x % 4 === 0) { top.push([x, MID - a]); bottom.push([x, MID + a]); }
      }
      return {
        label: 'The Howl: one hit tips the tank into its own feedback, which builds into a rough, rising roar and holds there until you pull DECAY back.',
        faint: [line(top), line(bottom)],
        ink: [tick(10, 50, 58)],
        hot: [line(rough)],
      };
    }
    case 'cv': {
      // A knob's arc (seven o'clock to five o'clock): the hand-set position in ink, the
      // CV's voltage (0–5 V) in red, carrying the arc on past where the hand left it.
      const cx = 30, cy = 35, r = 22;
      const at = (p: number, rr = r): [number, number] => {
        const a = ((-135 + 270 * p) * Math.PI) / 180;
        return [cx + rr * Math.sin(a), cy - rr * Math.cos(a)];
      };
      const arc = (p0: number, p1: number) => {
        const [x0, y0] = at(p0);
        const [x1, y1] = at(p1);
        return `M${f(x0)} ${f(y0)}A${r} ${r} 0 ${(p1 - p0) * 270 > 180 ? 1 : 0} 1 ${f(x1)} ${f(y1)}`;
      };
      const knob = 0.42, cv = 0.78;
      const [hx0, hy0] = at(knob, 6);
      const [hx1, hy1] = at(knob, 16);
      const volts: [number, number][] = [];
      for (let x = 84; x <= 156; x += 1) volts.push([x, 50 - 18 * (1 - Math.cos(((x - 84) / 72) * 2 * Math.PI))]);
      return {
        label: 'CV: a voltage from 0 to 5 V adds to where the knob is set, turning it further than your hand did.',
        faint: [arc(0, 1), 'M84 50H156'],
        ink: [arc(0, knob), `M${f(hx0)} ${f(hy0)}L${f(hx1)} ${f(hy1)}`, 'M64 35H74M69 30V40'],
        hot: [arc(knob, cv), line(volts)],
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
    case 'mulaw': {
      // A fading tail through the µ-law box: big steps on the loud start, fine ones as it
      // fades, then exact silence (under half a step is zero).
      const tail = (u: number) => Math.exp(-3 * u) * Math.sin(2 * Math.PI * 4.5 * u);
      return {
        label: 'µ-law: a fading tail drawn as the converter’s steps, coarse while loud, finer as it fades, then silence.',
        faint: [smooth(tail, 6, 154, 26)],
        ink: [],
        hot: [stairs(tail, 6, 154, 90, 5, 26)],
      };
    }
    case 'bits12':
    case 'bits10': {
      // The same wave at two bit depths: TAPE's finer steps, VALVE's coarser ones.
      const fine = kind === 'bits12';
      const wave = (u: number) => 0.92 * Math.sin(2 * Math.PI * 1.5 * u);
      const st = stairs(wave, 6, 154, fine ? 72 : 36, fine ? 9 : 4, 24, 12);
      return {
        label: fine
          ? 'TAPE, 12-bit: the wave in fine steps, close to the smooth original.'
          : 'VALVE, 10-bit: the same wave in coarse steps, clearly gritty.',
        faint: [smooth(wave, 6, 154, 24)],
        ink: fine ? [st] : [],
        hot: fine ? [] : [st],
      };
    }
    case 'rate': {
      // A spectrum: flat, then a steep cut at about 11 kHz; the lost top octave faint.
      return {
        label: '24 kHz: the sound is flat up to about 11 kHz, then the top octave is cut cleanly, nothing folding back.',
        faint: ['M118 18H156', 'M118 4V60'],
        ink: ['M4 18H112C116 18 118 22 120 30C122 40 124 52 126 58'],
        hot: ['M118 4V10'],
      };
    }
    case 'wetonly': {
      // Two lanes: the dry runs straight through; the wet goes through the µ-law box,
      // then TONE (a knob), then out.
      const tone = 'M110 46m-7 0a7 7 0 1 0 14 0a7 7 0 1 0 -14 0M110 46L114.5 41.5';
      return {
        label: 'The dry runs straight through, untouched. The wet goes through the grit, then TONE, which can thin it.',
        faint: [],
        ink: ['M4 14H156', 'M4 46H48', 'M84 46H103', 'M117 46H156', tone],
        hot: ['M48 36H84V56H48Z', 'M54 52H60V48H66V44H72V41H78'],
      };
    }
  }
}
