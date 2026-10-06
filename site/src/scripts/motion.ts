// Home choreography (DESIGN "Motion rules", PLAN §B7). Loaded on Home only.
// Rule 1: visible by default. Nothing is hidden until this script has run and added
// html.js-motion; with "reduce motion" on it does nothing at all, so the page is static.
// Rule 2: transform and opacity only.
import { gsap } from 'gsap';
import { ScrollTrigger } from 'gsap/ScrollTrigger';
import { SplitText } from 'gsap/SplitText';

const reduce = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

if (!reduce) {
  gsap.registerPlugin(ScrollTrigger, SplitText);
  document.documentElement.classList.add('js-motion');
  heroPushIn();
  revealCards();
  introLines();
  panelDraws();
  featureStack();
}

/** Hero: slow push-in on the tank as you scroll away, and one light sweep along the springs. */
function heroPushIn() {
  const zoom = document.querySelector('[data-hero-zoom]');
  const sweep = document.querySelector('[data-sweep]');
  if (zoom) {
    gsap.fromTo(zoom, { scale: 1 }, {
      scale: 1.08, ease: 'none',
      scrollTrigger: { trigger: '.hero', start: 'top top', end: 'bottom top', scrub: true },
    });
  }
  if (sweep) {
    gsap.timeline({ delay: 0.4 })
      .set(sweep, { opacity: 1 })
      .fromTo(sweep, { xPercent: -120 }, { xPercent: 560, duration: 2.4, ease: 'power2.inOut' })
      .to(sweep, { opacity: 0, duration: 0.4 }, '-=0.4');
  }
}

/** [data-reveal] blocks (demo cards, media bands) fade up as they enter, in small batches. */
function revealCards() {
  const show = (els: Element[]) =>
    els.forEach((el, i) => {
      (el as HTMLElement).style.transitionDelay = `${i * 80}ms`;
      el.classList.add('is-in');
    });
  ScrollTrigger.batch('[data-reveal]', {
    start: 'top 92%',
    once: true,
    onEnter: show,
    onEnterBack: show,
    onLeave: show,
  });
}

/** The intro paragraph reveals line by line. */
function introLines() {
  document.querySelectorAll<HTMLElement>('[data-split]').forEach((el) => {
    SplitText.create(el, {
      type: 'lines',
      mask: 'lines',
      linesClass: 'line',
      aria: 'none', // the text stays in the DOM, only wrapped in line spans: screen readers read it as is
      autoSplit: true,
      onSplit(self) {
        return gsap.from(self.lines, {
          yPercent: 100, opacity: 0, duration: 0.9, stagger: 0.09, ease: 'power3.out',
          scrollTrigger: { trigger: el, start: 'top 85%', once: true },
        });
      },
    });
  });
}

/** The panel map draws its linework in once it scrolls into view. */
function panelDraws() {
  document.querySelectorAll<HTMLElement>('.panel-diagram[data-draw]').forEach((wrap) => {
    const shapes = wrap.querySelectorAll('[pathLength]');
    const texts = wrap.querySelectorAll('text');
    gsap.set(shapes, { strokeDasharray: 1, strokeDashoffset: 1 });
    gsap.set(texts, { opacity: 0 });
    gsap.timeline({ scrollTrigger: { trigger: wrap, start: 'top 75%', once: true } })
      .to(shapes, { strokeDashoffset: 0, duration: 1.4, ease: 'power2.inOut', stagger: { amount: 1.2 } })
      .to(texts, { opacity: 1, duration: 0.6, stagger: { amount: 0.6 } }, 0.8);
  });
}

/** Features: each line transitions in as it enters the view (no pin, no scrub), its note just after. */
function featureStack() {
  const box = document.querySelector<HTMLElement>('[data-features]');
  if (!box) return;
  box.querySelectorAll<HTMLElement>('[data-line]').forEach((line) => {
    const note = line.querySelector('[data-note]');
    const tl = gsap.timeline({ scrollTrigger: { trigger: line, start: 'top 88%', once: true } });
    tl.from(line.firstElementChild, { opacity: 0, y: 24, duration: 0.8, ease: 'power3.out' });
    if (note) tl.from(note, { opacity: 0, duration: 0.5, ease: 'power2.out' }, '-=0.35');
  });
}
