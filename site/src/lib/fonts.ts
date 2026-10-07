import { existsSync } from 'node:fs';
import { join } from 'node:path';
import { PUBLIC_DIR } from './paths';

// The twelve Phonic cuts (DESIGN-v2 V3). scripts/prebuild.mjs fetches them into
// public/fonts/ (git-ignored: Phonic is licensed and the repo goes public).
export interface Face { file: string; family: 'Phonic' | 'Phonic Mono'; weight: 300 | 400 | 500 | 700; style: 'normal' | 'italic' }

export const FACES: Face[] = [
  { file: 'Phonic-Light.woff2', family: 'Phonic', weight: 300, style: 'normal' },
  { file: 'Phonic-LightItalic.woff2', family: 'Phonic', weight: 300, style: 'italic' },
  { file: 'Phonic-Regular.woff2', family: 'Phonic', weight: 400, style: 'normal' },
  { file: 'Phonic-RegularItalic.woff2', family: 'Phonic', weight: 400, style: 'italic' },
  { file: 'Phonic-Medium.woff2', family: 'Phonic', weight: 500, style: 'normal' },
  { file: 'Phonic-MediumItalic.woff2', family: 'Phonic', weight: 500, style: 'italic' },
  { file: 'Phonic-Bold.woff2', family: 'Phonic', weight: 700, style: 'normal' },
  { file: 'Phonic-BoldItalic.woff2', family: 'Phonic', weight: 700, style: 'italic' },
  { file: 'PhonicMonospaced-Light.woff2', family: 'Phonic Mono', weight: 300, style: 'normal' },
  { file: 'PhonicMonospaced-Regular.woff2', family: 'Phonic Mono', weight: 400, style: 'normal' },
  { file: 'PhonicMonospaced-Medium.woff2', family: 'Phonic Mono', weight: 500, style: 'normal' },
  { file: 'PhonicMonospaced-Bold.woff2', family: 'Phonic Mono', weight: 700, style: 'normal' },
];

export const FONT_FILES = FACES.map((f) => f.file);

/** The cuts worth preloading when present: body text and display. */
export const PRELOAD = ['Phonic-Light.woff2', 'Phonic-Bold.woff2'];

const has = (file: string) => existsSync(join(PUBLIC_DIR, 'fonts', file));

export interface FontPlan {
  /** Files present this build. */
  present: string[];
  /** Phonic leads the sans stack: all four upright cuts (300/400/500/700) are present. */
  sans: boolean;
  /** Phonic Mono leads the mono stack: all four mono cuts are present. */
  mono: boolean;
  /** @font-face rules, one per present file of an enabled family. */
  css: string;
}

/**
 * Which faces to declare. A family is used only when its four upright weights are all
 * there (otherwise the browser would fake the missing weight); italics are added when
 * present and otherwise synthesised. Nothing is declared for a missing file, so the
 * browser never asks for one.
 */
export function fontPlan(): FontPlan {
  const present = FONT_FILES.filter(has);
  const complete = (family: Face['family']) =>
    FACES.filter((f) => f.family === family && f.style === 'normal').every((f) => present.includes(f.file));
  const sans = complete('Phonic');
  const mono = complete('Phonic Mono');
  const css = FACES.filter((f) => present.includes(f.file) && (f.family === 'Phonic' ? sans : mono))
    .map((f) => `@font-face{font-family:"${f.family}";src:url("/fonts/${f.file}") format("woff2");font-weight:${f.weight};font-style:${f.style};font-display:swap}`)
    .join('\n');
  return { present, sans, mono, css };
}
