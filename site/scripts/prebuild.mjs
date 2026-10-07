// Runs before `astro dev` and `astro build` (npm's pre-scripts).
// 1. Copies the demo audio, the panel art, the plugin screenshot and the renders (stills, and the
//    hero reveal video when it exists) from docs/minisite/assets into public/
//    (one source of truth: the site never keeps its own copy in git).
// 2. Fetches the licensed MD UI / MD IO variable webfonts into public/fonts/ (git-ignored):
//    - FONTS_DIR=/path/to/woff2s       copy from a local folder, or
//    - FONTS_URL=https://…/ + FONTS_TOKEN=…   download each file with a bearer token
//      (FONTS_TOKEN defaults to BLOB_READ_WRITE_TOKEN, which Vercel sets once a Blob store
//      is connected to the project: a private store then needs only FONTS_URL),
//    - neither: skip. The site builds and renders on the fallback stack in tokens.css.
import { mkdir, copyFile, readdir, writeFile, access } from 'node:fs/promises';
import { join, resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const site = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const minisite = resolve(site, '..', 'docs', 'minisite');
const pub = join(site, 'public');

// The two variable fonts (the same list as src/lib/fonts.ts). Each one is used when
// present; the site declares no @font-face for a missing file.
const FONT_FILES = ['MDUI-VF.woff2', 'MDIO-VF.woff2'];

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
  const { FONTS_DIR, FONTS_URL } = process.env;
  const FONTS_TOKEN = process.env.FONTS_TOKEN || process.env.BLOB_READ_WRITE_TOKEN;

  if (FONTS_DIR) {
    const n = await copyDir(resolve(FONTS_DIR), dest, (f) => FONT_FILES.includes(f));
    return `fonts: copied ${n}/${FONT_FILES.length} font file(s) from FONTS_DIR`;
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
// Stills, plus the hero reveal film when it exists (hero_reveal / hero_hold_loop .mp4 + .webm, hero_still.jpg).
const renders = await copyDir(join(minisite, 'assets', 'renders'), join(pub, 'renders'), (f) => /\.(jpe?g|webp|avif|mp4|webm)$/.test(f));
console.log(`prebuild: ${audio} demo clip(s), ${panel} panel art file(s), ${shots} screenshot(s), ${renders} render(s) copied`);
console.log(`prebuild: ${await fonts()}`);
