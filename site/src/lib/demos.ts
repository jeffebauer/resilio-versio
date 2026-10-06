import { readFile } from 'node:fs/promises';
import { join } from 'node:path';
import { MPEGDecoder } from 'mpg123-decoder';
import { MINISITE } from './paths';

// docs/minisite/assets/audio/demos.json, as written by the demo tools.
// A clip may later list a `dry` file (same length, loudness-matched): the A/B
// switch appears only then (PLAN §B6). Until the pairs exist, `file` is the wet clip.
export interface Clip {
  file: string;
  dry?: string;
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

/** "0:18" */
export function clock(seconds: number): string {
  const s = Math.round(seconds);
  return `${Math.floor(s / 60)}:${String(s % 60).padStart(2, '0')}`;
}
