// One line per control for the Home panel rows (PLAN §B2 "The panel"). Each line
// condenses the Manual's own description (docs/minisite/content/manual.md) and links
// to that control's anchor there, where the full text lives.
export const CONTROLS = [
  { name: 'BLEND', anchor: 'blend', line: 'Dry ↔ wet. Fully right is 100 % wet, for a send/return.' },
  { name: 'DECAY', anchor: 'decay', line: 'How long the tail rings, from a quick slap to a long wash. The top holds (CLEAN, TAPE) or howls (VALVE).' },
  { name: 'TONE', anchor: 'tone', line: 'Warm and dark on the left, neutral at noon, King Tubby’s Big Knob low cut on the right.' },
  { name: 'SPLASH', anchor: 'splash', line: 'How hard your hits hit the springs.' },
  { name: 'TENSION', anchor: 'tension', line: 'Which tank is fitted: tight and quick on the right, loose with a big boing on the left.' },
  { name: 'WOBBLE', anchor: 'wobble', line: 'Pitch movement: tape drift left of noon, a steady warble right of noon.' },
  { name: 'DRIVE', anchor: 'drive', line: 'The tank’s input: up to +24 dB, with colour and grit building as you turn it up.' },
  { name: 'TANK', anchor: 'tank-top-switch', line: '1 · 2 · ECHO: one spring, two springs, or a tape echo into the springs.' },
  { name: 'ATTITUDE', anchor: 'attitude-bottom-switch', line: 'CLEAN · TAPE · VALVE: the saturation inside the tank.' },
  { name: 'THROW / TAP', anchor: 'throw--tap-button', line: 'Throws in TANK 1 and 2; taps the echo’s tempo in TANK ECHO.' },
  { name: 'Gate', anchor: 'gate-input', line: 'Throws in TANK 1 and 2; clocks the echo in TANK ECHO.' },
];

// The feature stack (PLAN §B2 "Features"): one line each, with a mono note.
export const FEATURES = [
  { line: 'Two springs', note: 'TANK 1 · 2' },
  { line: 'Tape echo', note: 'TAP TEMPO · CLOCK' },
  { line: 'Three attitudes', note: 'CLEAN · TAPE · VALVE' },
  { line: 'Throw and hold', note: 'BUTTON · GATE · DECAY' },
  { line: 'The Big Knob', note: 'TONE, RIGHT OF NOON' },
  { line: 'Splash', note: 'YOUR HITS, HARDER' },
  { line: 'Wobble', note: 'DRIFT · WARBLE' },
  { line: 'The Howl', note: 'VALVE · TOP OF DECAY' },
  { line: 'Seven CV inputs', note: '0–5 V' },
];
