// The Resilio panel's controls, in the owner's panel-art units (191 x 486, see
// components/PanelWire.astro). Positions are the control centres; lx/ly the label baseline.

export type KnobKey = 'blend' | 'decay' | 'tone' | 'splash' | 'tension' | 'wobble' | 'drive';
export type Tank = '1' | '2' | 'ECHO';
export type Attitude = 'CLEAN' | 'TAPE' | 'VALVE';
export type PanelSettings = Record<KnobKey, number> & { tank: Tank; attitude: Attitude };

export interface Knob { key: KnobKey; name: string; x: number; y: number; lx: number; ly: number; marks: [string, string] }

export const KNOBS: Knob[] = [
  { key: 'blend', name: 'BLEND', x: 29, y: 70, lx: 29.1, ly: 104, marks: ['○', '●'] },
  { key: 'decay', name: 'DECAY', x: 163.8, y: 70, lx: 164.3, ly: 104, marks: ['−', '+'] },
  { key: 'tone', name: 'TONE', x: 95.1, y: 108.4, lx: 94.8, ly: 143.1, marks: ['▷', '◁'] },
  { key: 'splash', name: 'SPLASH', x: 29.4, y: 149.2, lx: 28.8, ly: 183.1, marks: ['−', '+'] },
  { key: 'tension', name: 'TENSION', x: 163.8, y: 149.2, lx: 163.7, ly: 183.1, marks: ['−', '+'] },
  { key: 'wobble', name: 'WOBBLE', x: 95.1, y: 186.4, lx: 95.4, ly: 221.1, marks: ['WRB', 'VIB'] },
  { key: 'drive', name: 'DRIVE', x: 163.8, y: 228.4, lx: 164.6, ly: 263, marks: ['−', '+'] },
];

export interface Switch<T extends string = string> { key: 'tank' | 'attitude'; name: string; x: number; y: number; options: T[]; marks: string[] }

// Vertical three-way toggles (current Versio hardware): the first option is up.
export const SWITCHES: Switch[] = [
  { key: 'tank', name: 'TANK', x: 32, y: 219, options: ['1', '2', 'ECHO'], marks: ['∿ ', '∿ ', ''] },
  { key: 'attitude', name: 'ATTITUDE', x: 32, y: 255, options: ['CLEAN', 'TAPE', 'VALVE'], marks: ['', '', ''] },
];
