import { defineCollection } from 'astro:content';
import { glob } from 'astro/loaders';
import { z } from 'astro/zod';

// One source of truth: the page copy lives in docs/minisite/content/ and is read
// from there at build time (no copy in site/). Entry ids are the file names.
export const CONTENT_BASE = '../docs/minisite/content';

const pages = defineCollection({
  loader: glob({
    pattern: '*.md',
    base: CONTENT_BASE,
    generateId: ({ entry }) => entry.replace(/\.md$/, ''),
  }),
  schema: z.object({
    title: z.string(),
    description: z.string(),
    slug: z.string(),
    order: z.number(),
  }),
});

export const collections = { pages };
