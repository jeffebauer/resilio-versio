import type { ImageMetadata } from 'astro';

// The renders in docs/minisite/assets/renders (one source of truth), as Astro image
// imports, so <Picture> can serve AVIF/WebP at several widths (PLAN §B8).
const files = import.meta.glob<{ default: ImageMetadata }>('../../../docs/minisite/assets/renders/*.{jpg,jpeg,png}', { eager: true });

export function render(name: string): ImageMetadata | undefined {
  const key = Object.keys(files).find((k) => k.endsWith(`/${name}`));
  return key ? files[key].default : undefined;
}

export function mustRender(name: string): ImageMetadata {
  const img = render(name);
  if (!img) throw new Error(`Missing render: docs/minisite/assets/renders/${name}`);
  return img;
}

/** Widths for a full-width render, up to the file's own width. */
export const widthsFor = (img: ImageMetadata, list = [640, 960, 1280, 1600, 2048]) =>
  [...list.filter((w) => w < img.width), img.width];
