// Site-wide settings and links. Everything a non-developer may need to flip is here.

export const REPO = 'jeffebauer/resilio-versio';
export const REPO_URL = `https://github.com/${REPO}`;

/**
 * The go-public switch (BRIEF §9, PLAN §B10). While false, the download buttons show a
 * "coming soon" state, because GitHub's release links 404 while the repo is private.
 * Flip it here, or set DOWNLOADS_LIVE=1 in Vercel's environment, once the repo is public.
 */
export const DOWNLOADS_LIVE = process.env.DOWNLOADS_LIVE === '1' || false;

export const DOWNLOAD = {
  firmware: `${REPO_URL}/releases/latest/download/resilio-versio-firmware.bin`,
  plugin: `${REPO_URL}/releases/latest/download/resilio-versio-plugin-macos.zip`,
  release: `${REPO_URL}/releases/latest`,
};

export const NAV = [
  { href: '/#sound', label: 'Sound' },
  { href: '/manual/', label: 'Manual' },
  { href: '/install/', label: 'Install' },
  { href: '/presets/', label: 'Presets' },
  { href: '/changelog/', label: 'Changelog' },
  { href: '/faq/', label: 'FAQ' },
];

export const DOCS = [
  { href: '/manual/', label: 'Manual' },
  { href: '/install/', label: 'Install' },
  { href: '/presets/', label: 'Starting points' },
  { href: '/changelog/', label: 'Changelog' },
  { href: '/faq/', label: 'FAQ' },
  { href: '/credits/', label: 'Credits' },
];

export const LINKS = {
  source: REPO_URL,
  issues: `${REPO_URL}/issues`,
  mit: `${REPO_URL}/blob/main/LICENSE`,
  agpl: `${REPO_URL}/blob/main/LICENSES/AGPL-3.0.txt`,
  notice: `${REPO_URL}/blob/main/NOTICE`,
  author: 'https://jessebauer.xyz',
  firmwareSwap: 'https://noiseengineering.us/portal/firmware',
};
