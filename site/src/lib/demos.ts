import { readFile } from 'node:fs/promises';
import { join } from 'node:path';
import { MPEGDecoder } from 'mpg123-decoder';
import { MINISITE } from './paths';
import type { DiagramKind } from '../components/diagrams/shapes';

// docs/minisite/assets/audio/demos.json, as written by the demo tools.
// A clip with a `dry` file (demos round 2) is a pair rendered from one source: `file` is
// Resilio fully wet, `dry` is BLEND fully left (the clean passthrough, so they line up),
// both with the same gain. The BLEND slider mixes them like the knob; `blend` (0–1) is
// where it starts. Without a `dry` file, `file` is a finished mix and plays as it is.
export interface Clip {
  file: string;
  dry?: string;
  blend?: number;
  title: string;
  caption: string;
  transcript: string;
  duration_s: number;
  settings: Record<string, string>;
  note?: string;
}
export interface Demos { about: string; version: string; clips: Clip[] }

const AUDIO_DIR = join(MINISITE, 'assets', 'audio');
export const PEAK_COUNT = 240;

let demos: Promise<Demos> | undefined;
export function getDemos(): Promise<Demos> {
  demos ??= readFile(join(AUDIO_DIR, 'demos.json'), 'utf8').then((s) => JSON.parse(s) as Demos);
  return demos;
}

/** Public URL of a clip file (copied into public/audio by scripts/prebuild.mjs). */
export const audioUrl = (file: string) => `/audio/${file}`;

// Peaks are computed once per build: decode the MP3, take the loudest sample
// (both channels) in each of PEAK_COUNT slices, normalise to 0..1.
const peakCache = new Map<string, Promise<number[]>>();

async function computePeaks(file: string): Promise<number[]> {
  const bytes = new Uint8Array(await readFile(join(AUDIO_DIR, file)));
  const d = new MPEGDecoder();
  await d.ready;
  const { channelData, samplesDecoded } = d.decode(bytes);
  d.free();
  const peaks = new Array<number>(PEAK_COUNT).fill(0);
  const per = samplesDecoded / PEAK_COUNT;
  for (const ch of channelData) {
    for (let i = 0; i < samplesDecoded; i++) {
      const b = Math.min(PEAK_COUNT - 1, Math.floor(i / per));
      const v = Math.abs(ch[i]);
      if (v > peaks[b]) peaks[b] = v;
    }
  }
  const max = Math.max(...peaks, 1e-6);
  return peaks.map((p) => Math.round((p / max) * 1000) / 1000);
}

export function getPeaks(file: string): Promise<number[]> {
  let p = peakCache.get(file);
  if (!p) { p = computePeaks(file); peakCache.set(file, p); }
  return p;
}

/** A knob value (0–1) as its clock position: 0 is 7 o'clock, 0.5 noon, 1 is 5 o'clock. */
export function knobClock(v: number): string {
  const q = Math.round((7 + 10 * v) * 4) / 4;
  const h = Math.floor(q);
  const m = Math.round((q - h) * 60);
  return `${((h - 1) % 12) + 1}${m ? `:${String(m).padStart(2, '0')}` : ''}`;
}

/** "0:18" */
export function clock(seconds: number): string {
  const s = Math.round(seconds);
  return `${Math.floor(s / 60)}:${String(s % 60).padStart(2, '0')}`;
}

// The tiny diagram on each demo tile (DESIGN-v2 V11), by clip file number.
const DIAGRAMS: Record<string, DiagramKind> = {
  '01': 'drywet', '02': 'tanks', '03': 'attitude', '04': 'throw', '05': 'splash',
  '06': 'bigknob', '07': 'wobble', '08': 'echo', '09': 'howl', '10': 'hold',
};
export const diagramFor = (file: string): DiagramKind | undefined => DIAGRAMS[file.slice(0, 2)];
