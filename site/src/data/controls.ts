import type { DiagramKind } from '../components/diagrams/shapes';

// One line per control for Home's annotated panel (DESIGN-v2 V7). Each line condenses
// the Manual's own description (docs/minisite/content/manual.md) and links to that
// control's anchor there, where the full text lives. `at` is the control's centre on the
// panel in mm from the top-left (docs/panel/versio_panel_coordinates.csv); `side` is
// where its callout sits on the annotated render.
export interface Control { name: string; anchor: string; line: string; at: [number, number]; side: 'l' | 'r' }

export const CONTROLS: Control[] = [
  { name: 'BLEND', anchor: 'blend', line: 'Dry ↔ wet. Fully right is 100 % wet, for a send/return.', at: [7.77, 18.53], side: 'l' },
  { name: 'DECAY', anchor: 'decay', line: 'How long the tail rings, from a quick slap to a long wash. The top holds (CLEAN, TAPE) or howls (VALVE).', at: [43.33, 18.53], side: 'r' },
  { name: 'TONE', anchor: 'tone', line: 'Warm and dark on the left, neutral at noon, King Tubby’s Big Knob low cut on the right.', at: [25.169, 28.69], side: 'l' },
  { name: 'SPLASH', anchor: 'splash', line: 'How hard your hits hit the springs.', at: [7.77, 39.485], side: 'l' },
  { name: 'TENSION', anchor: 'tension', line: 'Which tank is fitted: tight and quick on the right, loose with a big boing on the left.', at: [43.33, 39.485], side: 'r' },
  { name: 'WOBBLE', anchor: 'wobble', line: 'Pitch movement: tape drift left of noon, a steady warble right of noon.', at: [25.169, 49.328], side: 'l' },
  { name: 'DRIVE', anchor: 'drive', line: 'The tank’s input: up to +24 dB, with colour and grit building as you turn it up.', at: [43.33, 60.44], side: 'r' },
  { name: 'TANK', anchor: 'tank-top-switch', line: '1 · 2 · ECHO: one spring, two springs, or a tape echo into the springs.', at: [8.405, 57.9], side: 'l' },
  { name: 'ATTITUDE', anchor: 'attitude-bottom-switch', line: 'CLEAN · TAPE · VALVE: the saturation inside the tank.', at: [8.405, 67.425], side: 'l' },
  { name: 'THROW / TAP', anchor: 'throw--tap-button', line: 'Throws in TANK 1 and 2; taps the echo’s tempo in TANK ECHO.', at: [26.185, 69.33], side: 'r' },
  { name: 'Gate', anchor: 'gate-input', line: 'Throws in TANK 1 and 2; clocks the echo in TANK ECHO.', at: [44.6, 97.27], side: 'r' },
];

// Features as bento tiles (DESIGN-v2 V9, owner's review of 7 Oct): one big statement each,
// a tiny diagram (V10) where the pictogram used to be, and one line. Lines are shortened
// from docs/minisite/content/overview.md and manual.md; a single "Read the manual" button
// sits under the grid. The order matters: with three columns and dense packing it fills
// five even rows (the big tile beside two small ones, then a wide and a small per row).
export interface FeatureTile { big: string; line: string; diagram: DiagramKind; size?: 'big' | 'wide' }

export const FEATURE_TILES: FeatureTile[] = [
  { big: '2 springs', size: 'big', diagram: 'tanks', line: 'TANK 1: one spring, sparse and the most splashy. TANK 2: two springs, the classic tank.' },
  { big: '3 attitudes', diagram: 'attitude', line: 'CLEAN · TAPE · VALVE: the saturation inside the tank. Your dry signal stays clean.' },
  { big: '7 CV inputs', diagram: 'cv', line: 'One per knob, 0–5 V, added to the knob’s position.' },
  { big: 'The throw', size: 'wide', diagram: 'throw', line: 'Open the springs for one snare, close them, let the tail ring on. The THROW / TAP button and the gate input both throw.' },
  { big: 'Wobble', diagram: 'wobble', line: 'Tape drift left of noon, a steady warble right of noon.' },
  { big: 'Splash', diagram: 'splash', line: 'Your hits, hitting harder. Nothing added; ghost notes stay quiet.' },
  { big: 'The Big Knob', size: 'wide', diagram: 'bigknob', line: 'TONE right of noon: King Tubby’s steep low cut, up to 800 Hz, swept smoothly by hand or with CV.' },
  { big: 'Tape echo', size: 'wide', diagram: 'echo', line: 'TANK ECHO: a worn tape echo feeding the springs, clocked from your sequencer or tapped in on the button.' },
  { big: 'The Howl', diagram: 'howl', line: 'In VALVE the top of DECAY lets the tank howl: rideable spring feedback. Pull DECAY back and it falls into a normal tail.' },
];
