import { getEntry, type CollectionEntry } from 'astro:content';

export type Page = CollectionEntry<'pages'>;
export type Slot = 'demos' | 'downloads' | 'panel' | 'latest' | 'screenshot';
export type Part = { kind: 'html'; html: string } | { kind: 'slot'; slot: Slot };
export type Section = { id: string; title: string; html: string };
export type Heading = { depth: number; slug: string; text: string };

export async function getPage(id: string): Promise<Page> {
  const entry = await getEntry('pages', id);
  if (!entry) throw new Error(`Missing page copy: docs/minisite/content/${id}.md`);
  return entry;
}

export function pageHtml(page: Page): string {
  return page.rendered?.html ?? '';
}

export function pageHeadings(page: Page): Heading[] {
  return (page.rendered?.metadata?.headings as Heading[] | undefined) ?? [];
}

/** The Markdown's own `# Title`, and the HTML without it (layouts set the h1). */
export function takeH1(html: string): { h1: string; rest: string } {
  const m = html.match(/<h1[^>]*>([\s\S]*?)<\/h1>/);
  if (!m) return { h1: '', rest: html };
  return { h1: stripTags(m[1]), rest: html.replace(m[0], '') };
}

/** Split rendered HTML around the slot markers left by remark-slots. */
export function splitSlots(html: string): Part[] {
  const parts: Part[] = [];
  const re = /<div data-slot="(\w+)"><\/div>/g;
  let last = 0;
  for (let m; (m = re.exec(html)); ) {
    if (m.index > last) parts.push({ kind: 'html', html: html.slice(last, m.index) });
    parts.push({ kind: 'slot', slot: m[1] as Slot });
    last = m.index + m[0].length;
  }
  if (last < html.length) parts.push({ kind: 'html', html: html.slice(last) });
  return parts;
}

/** Split rendered HTML at each <h2>: a lead (before the first h2) and the sections. */
export function splitSections(html: string): { lead: string; sections: Section[] } {
  const chunks = html.split(/(?=<h2[\s>])/);
  const lead = chunks[0].startsWith('<h2') ? '' : chunks.shift() ?? '';
  const sections = chunks.map((chunk) => {
    const m = chunk.match(/^<h2(?:\s+id="([^"]*)")?[^>]*>([\s\S]*?)<\/h2>/);
    return {
      id: m?.[1] ?? '',
      title: stripTags(m?.[2] ?? ''),
      html: m ? chunk.slice(m[0].length) : chunk,
    };
  });
  return { lead, sections };
}

export function section(sections: Section[], id: string): Section {
  const s = sections.find((x) => x.id === id);
  if (!s) throw new Error(`Missing section #${id} in the page copy`);
  return s;
}

export function stripTags(s: string): string {
  return s.replace(/<[^>]+>/g, '').replace(/&#x26;|&amp;/g, '&').trim();
}

/** First <p>…</p> of an HTML string, and the rest. */
export function firstParagraph(html: string): { p: string; rest: string } {
  const m = html.match(/<p>[\s\S]*?<\/p>/);
  if (!m) return { p: '', rest: html };
  return { p: m[0], rest: html.replace(m[0], '') };
}
