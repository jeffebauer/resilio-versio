import { resolve } from 'node:path';

// Astro runs from site/; the minisite package sits next door in docs/minisite.
export const MINISITE = resolve(process.cwd(), '..', 'docs', 'minisite');
export const PUBLIC_DIR = resolve(process.cwd(), 'public');
