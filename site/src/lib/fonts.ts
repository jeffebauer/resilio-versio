import { existsSync } from 'node:fs';
import { join } from 'node:path';
import { PUBLIC_DIR } from './paths';

// Two variable fonts (Mass-Driver, via Future Fonts): MD UI for text, MD IO for mono.
// scripts/prebuild.mjs fetches them into public/fonts/ (git-ignored: they're licensed
// and the repo goes public).
// MD UI: opsz 6–48 (the browser follows the text size: font-optical-sizing is auto),
// wght 200–900, slnt −12–0 (its italics: declared as an oblique range, so the browser
// slants with the axis instead of faking it). MD IO: wght 200–900, ital 0–1.
export interface Face { file: string; family: 'MD UI' | 'MD IO'; style: string }

export const FACES: Face[] = [
  { file: 'MDUI-VF.woff2', family: 'MD UI', style: 'oblique 0deg 12deg' },
  { file: 'MDIO-VF.woff2', family: 'MD IO', style: 'normal' },
];

export const FONT_FILES = FACES.map((f) => f.file);

/** Preload the text face when present (one file serves every weight). */
export const PRELOAD = ['MDUI-VF.woff2'];

const has = (file: string) => existsSync(join(PUBLIC_DIR, 'fonts', file));

export interface FontPlan {
  /** Files present this build. */
  present: string[];
  /** MD UI leads the sans stack: its file is present. */
  sans: boolean;
  /** MD IO leads the mono stack: its file is present. */
  mono: boolean;
  /** @font-face rules, one per present file. */
  css: string;
}

/** Which faces to declare. Nothing is declared for a missing file, so the browser never asks for one. */
export function fontPlan(): FontPlan {
  const present = FONT_FILES.filter(has);
  const sans = present.includes('MDUI-VF.woff2');
  const mono = present.includes('MDIO-VF.woff2');
  const css = FACES.filter((f) => present.includes(f.file))
    .map((f) => `@font-face{font-family:"${f.family}";src:url("/fonts/${f.file}") format("woff2-variations"),url("/fonts/${f.file}") format("woff2");font-weight:200 900;font-style:${f.style};font-display:swap}`)
    .join('\n');
  return { present, sans, mono, css };
}
