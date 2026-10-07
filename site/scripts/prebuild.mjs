// Runs before `astro dev` and `astro build` (npm's pre-scripts).
// 1. Copies the demo audio, the panel art and the plugin screenshot from docs/minisite/assets into public/
//    (one source of truth: the site never keeps its own copy in git).
// 2. Fetches the licensed Phonic webfonts into public/fonts/ (git-ignored):
//    - FONTS_DIR=/path/to/woff2s       copy from a local folder, or
//    - FONTS_URL=https://…/ + FONTS_TOKEN=…   download each file with a bearer token,
//    - neither: skip. The site builds and renders on the fallback stack in tokens.css.
import { mkdir, copyFile, readdir, writeFile, access } from 'node:fs/promises';
import { join, resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const site = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const minisite = resolve(site, '..', 'docs', 'minisite');
const pub = join(site, 'public');

// The four files tokens.css asks for (DESIGN D1).
const FONT_FILES = [
  'Phonic-Light.woff2',
  'Phonic-Regular.woff2',
  'Phonic-Medium.woff2',
  'PhonicMonospaced-Regular.woff2',
];

async function copyDir(from, to, filter) {
  await mkdir(to, { recursive: true });
  const names = (await readdir(from)).filter(filter);
  await Promise.all(names.map((n) => copyFile(join(from, n), join(to, n))));
  return names.length;
}

async function exists(p) {
  try { await access(p); return true; } catch { return false; }
}

async function fonts() {
  const dest = join(pub, 'fonts');
  const { FONTS_DIR, FONTS_URL, FONTS_TOKEN } = process.env;

  if (FONTS_DIR) {
    const n = await copyDir(resolve(FONTS_DIR), dest, (f) => f.endsWith('.woff2'));
    return `fonts: copied ${n} WOFF2 file(s) from FONTS_DIR`;
  }

  if (FONTS_URL && FONTS_TOKEN) {
    await mkdir(dest, { recursive: true });
    const base = FONTS_URL.endsWith('/') ? FONTS_URL : `${FONTS_URL}/`;
    let got = 0;
    for (const name of FONT_FILES) {
      const res = await fetch(new URL(name, base), {
        headers: { Authorization: `Bearer ${FONTS_TOKEN}` },
      });
      if (!res.ok) {
        console.warn(`fonts: ${name} -> HTTP ${res.status}, skipped`);
        continue;
      }
      await writeFile(join(dest, name), Buffer.from(await res.arrayBuffer()));
      got++;
    }
    return `fonts: downloaded ${got}/${FONT_FILES.length} from FONTS_URL`;
  }

  const present = (await Promise.all(FONT_FILES.map((f) => exists(join(dest, f))))).filter(Boolean).length;
  return present
    ? `fonts: no FONTS_DIR/FONTS_URL; using the ${present} file(s) already in public/fonts`
    : 'fonts: none supplied; building on the fallback stack (see README)';
}

const audio = await copyDir(join(minisite, 'assets', 'audio'), join(pub, 'audio'), (f) => f.endsWith('.mp3'));
const panel = await copyDir(join(minisite, 'assets', 'panel'), join(pub, 'panel'), (f) => f.endsWith('-art.svg'));
const shots = await copyDir(join(minisite, 'assets', 'screenshots'), join(pub, 'screenshots'), (f) => f.endsWith('.png'));
const renders = await copyDir(join(minisite, 'assets', 'renders'), join(pub, 'renders'), (f) => /\.(jpe?g|webp|avif)$/.test(f));
console.log(`prebuild: ${audio} demo clip(s), ${panel} panel art file(s), ${shots} screenshot(s), ${renders} render(s) copied`);
console.log(`prebuild: ${await fonts()}`);
