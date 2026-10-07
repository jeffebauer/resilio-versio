import type { DiagramKind } from '../components/diagrams/shapes';

// One line per control for Home's annotated panel (DESIGN-v2 V7). Each line condenses
// the Manual's own description (docs/minisite/content/manual.md) and links to that
// control's anchor there, where the full text lives. `at` is the control's centre on the
// panel in mm from the top-left (docs/panel/versio_panel_coordinates.csv); `side` is
// where its callout sits on the annotated render.
export interface Control { name: string; anchor: string; line: string; at: [number, number]; side: 'l' | 'r' }

export const CONTROLS: Control[] = [
  { name: 'BLEND', anchor: 'blend', line: 'Dry ↔ wet. Fully right is 100 % wet, for a send and return.', at: [7.77, 18.53], side: 'l' },
  { name: 'DECAY', anchor: 'decay', line: 'How long the tail rings, from a quick slap to a long wash. At the top it holds in CLEAN and TAPE, and howls in VALVE.', at: [43.33, 18.53], side: 'r' },
  { name: 'TONE', anchor: 'tone', line: 'Warm and dark on the left, neutral at noon, King Tubby’s “Big Knob” low cut on the right.', at: [25.169, 28.69], side: 'l' },
  { name: 'SPLASH', anchor: 'splash', line: 'How hard your hits hit the springs.', at: [7.77, 39.485], side: 'l' },
  { name: 'TENSION', anchor: 'tension', line: 'Which tank is fitted. Tight and quick to the right, loose with a big boing to the left.', at: [43.33, 39.485], side: 'r' },
  { name: 'WOBBLE', anchor: 'wobble', line: 'Pitch movement. Tape drift left of noon, a steady warble right of it.', at: [25.169, 49.328], side: 'l' },
  { name: 'DRIVE', anchor: 'drive', line: 'How hard you hit the tank, up to +24 dB. Colour and grit build as you turn it up.', at: [43.33, 60.44], side: 'r' },
  { name: 'TANK', anchor: 'tank-top-switch', line: 'One spring, two springs, or a tape echo into the springs.', at: [8.405, 57.9], side: 'l' },
  { name: 'ATTITUDE', anchor: 'attitude-bottom-switch', line: 'The saturation inside the tank. TAPE and VALVE also add 12- or 10-bit µ-law grit to the reverb.', at: [8.405, 67.425], side: 'l' },
  { name: 'THROW / TAP', anchor: 'throw--tap-button', line: 'Throws in TANK 1 and 2, and taps the echo’s tempo in TANK ECHO.', at: [26.185, 69.33], side: 'r' },
  { name: 'Gate', anchor: 'gate-input', line: 'Throws in TANK 1 and 2, and clocks the echo in TANK ECHO.', at: [44.6, 97.27], side: 'r' },
];

// Features as bento tiles (DESIGN-v2 V9, owner's review of 7 Oct): one big statement each,
// a tiny diagram (V10) where the pictogram used to be, and one line. Lines are shortened
// from docs/minisite/content/overview.md and manual.md; a single "Read the manual" button
// sits under the grid. The order matters: with three columns and dense packing it fills
// five even rows (the big tile beside two small ones, then a wide and a small per row).
export interface FeatureTile { big: string; line: string; diagram: DiagramKind; size?: 'big' | 'wide' }

export const FEATURE_TILES: FeatureTile[] = [
  { big: '2 springs', size: 'big', diagram: 'tanks', line: 'One spring in TANK 1, sparse and the splashiest. Two in TANK 2, the classic tank.' },
  { big: 'Tape echo', diagram: 'echo', line: 'TANK ECHO puts a worn tape echo in front of the springs. Clock it from your sequencer or tap it in.' },
  { big: '3 attitudes', diagram: 'attitude', line: 'Three kinds of saturation inside the tank, with µ-law grit on the reverb in TAPE and VALVE. Your dry signal stays clean.' },
  { big: 'The throw', size: 'wide', diagram: 'throw', line: 'Open the springs for one snare, close them again and let the tail ring on. The button and the gate input both do it.' },
  { big: 'WOBBLE', diagram: 'wobble', line: 'Tape drift left of noon, a steady warble right of it.' },
  { big: 'SPLASH', diagram: 'splash', line: 'Makes your hits hit the springs harder. Ghost notes stay quiet.' },
  { big: 'The “Big Knob”', diagram: 'bigknob', line: 'The right half of TONE is King Tubby’s steep low cut, up to 800 Hz. Sweep it by hand or with CV.' },
  { big: 'The howl', diagram: 'howl', line: 'In VALVE, the top of DECAY lets the tank feed back into a howl you can ride. Pull DECAY back and it falls into a normal tail.' },
];

// Colour as bento tiles (owner, 7 Oct: "minimal mention of µ-law and bit reduction"). The
// same form as the features. Facts from SPEC §3 ATTITUDE row and §4.8 (ADR 0042 and its
// amendments): the wet only, 24 kHz µ-law, TAPE 12-bit, VALVE 10-bit, CLEAN untouched,
// before TONE's return filter. Order: the big tile beside two small, then a wide and a small.
export const COLOUR_TILES: FeatureTile[] = [
  { big: 'µ-law grit', size: 'big', diagram: 'mulaw', line: 'TAPE and VALVE send the reverb through a 24 kHz µ-law converter, the grain you hear on early digital delays and samplers. Its steps are coarse on loud hits and fine on quiet tails, so a fading tail turns to grain before it goes silent.' },
  { big: 'TAPE: 12-bit', diagram: 'bits12', line: 'A fine grain on top of the tape saturation. This is the core dub colour.' },
  { big: 'VALVE: 10-bit', diagram: 'bits10', line: 'Coarse and clearly gritty, on top of the valve’s hard, lopsided saturation.' },
  { big: 'Only on the reverb', size: 'wide', diagram: 'wetonly', line: 'Your dry signal stays as it is, and CLEAN leaves the reverb alone too. The grit comes before TONE, so turning TONE right thins the grit along with the tail.' },
  { big: '24 kHz', diagram: 'rate', line: 'Half the usual sample rate. The top octave, above about 11 kHz, is cut cleanly, so nothing aliases.' },
];
