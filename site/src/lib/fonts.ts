import { existsSync } from 'node:fs';
import { join } from 'node:path';
import { PUBLIC_DIR } from './paths';

// Which Phonic files are present this build (scripts/prebuild.mjs fetches them).
export const FONT_FILES = {
  light: 'Phonic-Light.woff2',
  regular: 'Phonic-Regular.woff2',
  medium: 'Phonic-Medium.woff2',
  mono: 'PhonicMonospaced-Regular.woff2',
} as const;

export function fontsPresent(): Record<keyof typeof FONT_FILES, boolean> {
  const has = (f: string) => existsSync(join(PUBLIC_DIR, 'fonts', f));
  return {
    light: has(FONT_FILES.light),
    regular: has(FONT_FILES.regular),
    medium: has(FONT_FILES.medium),
    mono: has(FONT_FILES.mono),
  };
}
