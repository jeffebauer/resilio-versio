// The A/B demo player (PLAN §B6). Web Audio, one clip at a time.
// - Audio loads on the first press of Play (never autoplays; iOS needs that tap).
// - With a dry file, dry and wet run from one start time through two gains, and the
//   DRY / WET switch crossfades in 30 ms, so you compare the sound, not the timing.
// - Keyboard: Space/Enter on Play; on the waveform slider ←/→ 1 s, PgUp/PgDn 5 s,
//   Home/End; Space toggles play there too.

type Mode = 'dry' | 'wet';

let ctx: AudioContext | null = null;
let current: Player | null = null;

const XFADE = 0.03;

function audioContext(): AudioContext {
  if (!ctx) {
    const AC = window.AudioContext ?? (window as unknown as { webkitAudioContext: typeof AudioContext }).webkitAudioContext;
    ctx = new AC();
  }
  return ctx;
}

const clock = (s: number) => {
  const t = Math.max(0, Math.round(s));
  return `${Math.floor(t / 60)}:${String(t % 60).padStart(2, '0')}`;
};

class Player {
  el: HTMLElement;
  playBtn: HTMLButtonElement;
  wave: HTMLElement;
  rect: SVGRectElement;
  head: SVGLineElement;
  pos: HTMLElement;
  abButtons: HTMLButtonElement[];
  title: string;
  wetUrl: string;
  dryUrl?: string;
  duration: number;
  viewW: number;

  buffers: Partial<Record<Mode, AudioBuffer>> = {};
  loading: Promise<void> | null = null;
  sources: AudioBufferSourceNode[] = [];
  gains: Partial<Record<Mode, GainNode>> = {};
  mode: Mode = 'wet';
  playing = false;
  offset = 0;
  startedAt = 0;
  raf = 0;

  constructor(el: HTMLElement) {
    this.el = el;
    this.playBtn = el.querySelector('.play')!;
    this.wave = el.querySelector('.wave')!;
    this.rect = el.querySelector('rect.played')!;
    this.head = el.querySelector('line.head')!;
    this.pos = el.querySelector('[data-pos]')!;
    this.abButtons = [...el.querySelectorAll<HTMLButtonElement>('[data-ab]')];
    this.title = el.dataset.title ?? '';
    this.wetUrl = el.dataset.src!;
    this.dryUrl = el.dataset.dry || undefined;
    this.duration = Number(el.dataset.duration) || 0;
    this.viewW = (this.wave.querySelector('svg') as SVGSVGElement).viewBox.baseVal.width;

    this.playBtn.addEventListener('click', () => this.toggle());
    this.wave.addEventListener('keydown', (e) => this.onKey(e));
    this.wave.addEventListener('pointerdown', (e) => this.onPointer(e));
    for (const b of this.abButtons) b.addEventListener('click', () => this.setMode(b.dataset.ab as Mode));
  }

  async load() {
    this.loading ??= (async () => {
      const ac = audioContext();
      const get = async (url: string) => ac.decodeAudioData(await (await fetch(url)).arrayBuffer());
      const [wet, dry] = await Promise.all([get(this.wetUrl), this.dryUrl ? get(this.dryUrl) : Promise.resolve(undefined)]);
      this.buffers = { wet, dry };
      this.duration = wet.duration;
    })();
    return this.loading;
  }

  async toggle() {
    if (this.playing) this.pause();
    else await this.play();
  }

  async play() {
    const ac = audioContext();
    if (ac.state === 'suspended') void ac.resume();
    if (current && current !== this) current.pause();
    current = this;
    if (!this.buffers.wet) {
      this.playBtn.setAttribute('aria-busy', 'true');
      try { await this.load(); } finally { this.playBtn.removeAttribute('aria-busy'); }
      if (current !== this) return; // another clip started while this one loaded
    }
    if (this.offset >= this.duration - 0.05) this.offset = 0;
    this.start(this.offset);
    this.playing = true;
    this.el.classList.add('is-playing');
    this.playBtn.setAttribute('aria-label', `Pause: ${this.title}`);
    this.tick();
  }

  start(at: number) {
    const ac = audioContext();
    this.stopSources();
    const t0 = ac.currentTime + 0.01;
    (['wet', 'dry'] as Mode[]).forEach((m) => {
      const buf = this.buffers[m];
      if (!buf) return;
      const src = ac.createBufferSource();
      src.buffer = buf;
      const g = ac.createGain();
      g.gain.value = m === this.mode || !this.buffers.dry ? 1 : 0;
      src.connect(g).connect(ac.destination);
      src.start(t0, at);
      if (m === 'wet') src.onended = () => { if (this.sources.includes(src) && this.playing) this.ended(); };
      this.sources.push(src);
      this.gains[m] = g;
    });
    this.startedAt = t0 - at;
  }

  stopSources() {
    for (const s of this.sources) { s.onended = null; try { s.stop(); } catch { /* not started */ } s.disconnect(); }
    this.sources = [];
  }

  position(): number {
    if (!this.playing || !ctx) return this.offset;
    return Math.min(this.duration, Math.max(0, ctx.currentTime - this.startedAt));
  }

  pause() {
    if (!this.playing) return;
    this.offset = this.position();
    this.stopSources();
    this.playing = false;
    this.el.classList.remove('is-playing');
    this.playBtn.setAttribute('aria-label', `Play: ${this.title}`);
    cancelAnimationFrame(this.raf);
    this.render();
  }

  ended() {
    this.pause();
    this.offset = 0;
    this.render();
  }

  seek(t: number) {
    this.offset = Math.min(Math.max(0, t), this.duration);
    if (this.playing && this.buffers.wet) this.start(this.offset);
    this.render();
  }

  setMode(m: Mode) {
    this.mode = m;
    for (const b of this.abButtons) b.setAttribute('aria-pressed', String(b.dataset.ab === m));
    if (!ctx) return;
    const now = ctx.currentTime;
    (['wet', 'dry'] as Mode[]).forEach((k) => {
      const g = this.gains[k]?.gain;
      if (!g) return;
      g.cancelScheduledValues(now);
      g.setValueAtTime(g.value, now);
      g.linearRampToValueAtTime(k === m ? 1 : 0, now + XFADE);
    });
  }

  onKey(e: KeyboardEvent) {
    const t = this.position();
    const steps: Record<string, number> = { ArrowRight: 1, ArrowUp: 1, ArrowLeft: -1, ArrowDown: -1, PageUp: 5, PageDown: -5 };
    if (e.key in steps) { this.seek(t + steps[e.key]); }
    else if (e.key === 'Home') { this.seek(0); }
    else if (e.key === 'End') { this.seek(this.duration); }
    else if (e.key === ' ' || e.key === 'Enter') { void this.toggle(); }
    else return;
    e.preventDefault();
  }

  onPointer(e: PointerEvent) {
    const box = this.wave.getBoundingClientRect();
    const at = (x: number) => ((x - box.left) / box.width) * this.duration;
    this.seek(at(e.clientX));
    this.wave.setPointerCapture(e.pointerId);
    const move = (ev: PointerEvent) => this.seek(at(ev.clientX));
    const up = () => { this.wave.removeEventListener('pointermove', move); this.wave.removeEventListener('pointerup', up); };
    this.wave.addEventListener('pointermove', move);
    this.wave.addEventListener('pointerup', up);
  }

  tick = () => {
    this.render();
    if (this.playing) this.raf = requestAnimationFrame(this.tick);
  };

  render() {
    const t = this.position();
    const x = this.duration ? (t / this.duration) * this.viewW : 0;
    this.rect.setAttribute('width', String(x));
    this.head.setAttribute('x1', String(x));
    this.head.setAttribute('x2', String(x));
    this.pos.textContent = clock(t);
    this.wave.setAttribute('aria-valuenow', String(Math.round(t)));
    this.wave.setAttribute('aria-valuemax', String(Math.round(this.duration)));
    this.wave.setAttribute('aria-valuetext', `${clock(t)} of ${clock(this.duration)}`);
  }
}

function init() {
  document.querySelectorAll<HTMLElement>('[data-player]:not([data-ready])').forEach((el) => {
    el.dataset.ready = '';
    new Player(el);
  });
}

init();
document.addEventListener('astro:page-load', init);
