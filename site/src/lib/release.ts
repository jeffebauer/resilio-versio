import fallback from '../data/release.json';
import { REPO } from '../data/site';

// The current release, read once per build (PLAN §B5, BRIEF §9 option A).
// GitHub's API first (GITHUB_TOKEN optional, avoids the 60/hour limit);
// src/data/release.json when the API can't be reached (rate limit, network).
export interface Release {
  version: string;
  date: string; // YYYY-MM-DD
  url: string;
  firmwareBytes: number;
  pluginBytes: number;
  source: 'github' | 'fallback';
}

const FIRMWARE = 'resilio-versio-firmware.bin';
const PLUGIN = 'resilio-versio-plugin-macos.zip';

function fromFallback(): Release {
  return {
    version: fallback.version,
    date: fallback.date,
    url: fallback.html_url,
    firmwareBytes: fallback.assets.firmware.bytes,
    pluginBytes: fallback.assets.plugin.bytes,
    source: 'fallback',
  };
}

interface GhAsset { name: string; size: number }
interface GhRelease { tag_name: string; published_at: string; html_url: string; assets: GhAsset[] }

async function fromGitHub(): Promise<Release | null> {
  const headers: Record<string, string> = {
    Accept: 'application/vnd.github+json',
    'X-GitHub-Api-Version': '2022-11-28',
    'User-Agent': 'resilio-versio-site',
  };
  if (process.env.GITHUB_TOKEN) headers.Authorization = `Bearer ${process.env.GITHUB_TOKEN}`;
  try {
    const res = await fetch(`https://api.github.com/repos/${REPO}/releases/latest`, {
      headers,
      signal: AbortSignal.timeout(8000),
    });
    if (!res.ok) return null;
    const r = (await res.json()) as GhRelease;
    const size = (name: string) => r.assets.find((a) => a.name === name)?.size;
    const fw = size(FIRMWARE);
    const pl = size(PLUGIN);
    // Until the release script uploads the stable names, keep the committed values.
    if (!fw || !pl) return null;
    return {
      version: r.tag_name,
      date: r.published_at.slice(0, 10),
      url: r.html_url,
      firmwareBytes: fw,
      pluginBytes: pl,
      source: 'github',
    };
  } catch {
    return null;
  }
}

let cached: Promise<Release> | undefined;
export function getRelease(): Promise<Release> {
  cached ??= fromGitHub().then((r) => r ?? fromFallback());
  return cached;
}

/** 126 KB, 7.8 MB (decimal units, as macOS Finder shows them). */
export function fileSize(bytes: number): string {
  if (bytes >= 1e6) return `${(bytes / 1e6).toFixed(1)} MB`;
  return `${Math.round(bytes / 1e3)} KB`;
}

/** "6 October 2026" */
export function longDate(iso: string): string {
  return new Date(`${iso}T12:00:00Z`).toLocaleDateString('en-GB', {
    day: 'numeric', month: 'long', year: 'numeric', timeZone: 'UTC',
  });
}
