// Small structural fixes on the rendered Markdown, so the copy stays untouched:
// - heading levels never skip (the FAQ goes # → ###, so its questions become h2), for screen readers;
// - tables become hairline .spec tables, wrapped so wide ones scroll on phones
//   (the wrapper is focusable so keyboard users can scroll it too).
function headingLevel(el) {
  const m = /^h([1-6])$/.exec(el.tagName ?? '');
  return m ? Number(m[1]) : 0;
}

function text(node) {
  if (node.type === 'text') return node.value;
  return (node.children ?? []).map(text).join('');
}

export function rehypeDocs() {
  return (tree) => {
    // Levels in use, compacted: a page using # and ### only gets h1 and h2.
    const used = new Set();
    const collect = (n) => { const l = n.type === 'element' && headingLevel(n); if (l) used.add(l); (n.children ?? []).forEach(collect); };
    collect(tree);
    const rank = new Map([...used].sort().map((l, i) => [l, i + 1]));
    let last = 0;
    let lastHeading = '';
    const visit = (node) => {
      if (!node.children) return;
      node.children = node.children.map((child) => {
        if (child.type !== 'element') return child;
        const level = headingLevel(child);
        if (level) {
          const fixed = Math.min(rank.get(level) ?? level, last + 1);
          child.tagName = `h${fixed}`;
          last = fixed;
          lastHeading = text(child);
          return child;
        }
        if (child.tagName === 'table') {
          child.properties = { ...child.properties, className: ['spec'] };
          return {
            type: 'element',
            tagName: 'div',
            properties: {
              className: ['table-scroll'],
              tabIndex: 0,
              role: 'region',
              ariaLabel: lastHeading ? `Table: ${lastHeading}` : 'Table',
            },
            children: [child],
          };
        }
        visit(child);
        return child;
      });
    };
    visit(tree);
  };
}
