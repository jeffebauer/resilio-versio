// Markdown in docs/minisite/content carries HTML comments:
//   <!-- PANEL: … -->, <!-- DEMOS … -->, <!-- DOWNLOADS … -->, <!-- LATEST … -->, <!-- SCREENSHOT … -->
//     mark where a component goes: they become <div data-slot="panel"></div>, which the
//     page swaps for the real component (src/lib/content.ts, splitSlots).
//   <!-- OWNER: … --> and any other comment are questions for the owner: never published.
const SLOT = /^<!--\s*(DEMOS|DOWNLOADS|PANEL|LATEST|SCREENSHOT)\b/;
const COMMENT = /^<!--[\s\S]*?-->$/;

function walk(node) {
  if (!node.children) return;
  const out = [];
  for (const child of node.children) {
    if (child.type === 'html') {
      const v = child.value.trim();
      const m = v.match(SLOT);
      if (m) {
        out.push({ type: 'html', value: `<div data-slot="${m[1].toLowerCase()}"></div>` });
        continue;
      }
      if (COMMENT.test(v)) continue;
    }
    walk(child);
    out.push(child);
  }
  node.children = out;
}

export function remarkSlots() {
  return (tree) => walk(tree);
}
