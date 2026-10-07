// @ts-check
import { defineConfig } from 'astro/config';
import sitemap from '@astrojs/sitemap';
import { fileURLToPath } from 'node:url';
import { unified } from '@astrojs/markdown-remark';
import { remarkSlots } from './src/plugins/remark-slots.mjs';
import { rehypeDocs } from './src/plugins/rehype-docs.mjs';

// The public address. On Vercel the production URL is known at build time;
// SITE_URL overrides it (e.g. once a domain is chosen, DESIGN blind spot 9).
const site =
  process.env.SITE_URL ??
  (process.env.VERCEL_PROJECT_PRODUCTION_URL
    ? `https://${process.env.VERCEL_PROJECT_PRODUCTION_URL}`
    : 'https://resilio-versio.vercel.app');

// Mesurer, the measuring/annotation overlay, in `astro dev` only (src/scripts/mesurer-dev.ts).
/** @type {import('astro').AstroIntegration} */
const mesurerDev = {
  name: 'mesurer-dev',
  hooks: {
    'astro:config:setup': ({ command, injectScript }) => {
      if (command === 'dev') {
        injectScript('page', `import ${JSON.stringify(fileURLToPath(new URL('./src/scripts/mesurer-dev.ts', import.meta.url)))};`);
      }
    },
  },
};

export default defineConfig({
  site,
  output: 'static',
  trailingSlash: 'ignore',
  // CSS inlined into each page: one less render-blocking request (Lighthouse, PLAN §B8).
  build: { format: 'directory', inlineStylesheets: 'always' },
  integrations: [
    sitemap({ filter: (page) => !page.includes('/style') }),
    mesurerDev,
  ],
  markdown: {
    // remark/rehype, so the two small plugins can keep the copy untouched (src/plugins).
    processor: unified({ remarkPlugins: [remarkSlots], rehypePlugins: [rehypeDocs] }),
    syntaxHighlight: false,
  },
  vite: {
    // The copy, design tokens and assets live in ../docs/minisite (one source of truth).
    server: { fs: { allow: ['..'] } },
  },
});
