// Mesurer (https://mesurer.dev): an overlay for measuring, inspecting and annotating the
// live page while we polish the design. Dev only: astro.config.mjs injects this script
// when `astro dev` runs, so neither React nor Mesurer ever reaches a built page.
// Press M to show or hide it.
import { createElement } from 'react';
import { createRoot } from 'react-dom/client';
import { Mesurer } from 'mesurer';
import 'mesurer/styles.css';

const host = document.createElement('div');
host.id = 'mesurer-dev';
document.body.append(host);
// Signal red for guides and arrows, so annotations match the site's accent.
createRoot(host).render(createElement(Mesurer, { guideColor: '#ee5641', arrowColor: '#ee5641', persistOnReload: true }));
