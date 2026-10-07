// Home choreography (DESIGN "Motion rules", PLAN §B7; revision 2 keeps it quieter:
// reveals and one push-in, no pinned stacks). Loaded on Home only.
// Rule 1: visible by default. Nothing is hidden until this script has run and added
// html.js-motion; with "reduce motion" on it does nothing at all, so the page is static.
// Rule 2: transform and opacity only.
import { gsap } from 'gsap';
import { ScrollTrigger } from 'gsap/ScrollTrigger';

const reduce = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

if (!reduce) {
  gsap.registerPlugin(ScrollTrigger);
  document.documentElement.classList.add('js-motion');
  heroPushIn();
  revealCards();
  introLines();
}

/** Hero: a slow push-in on the frame's still or film as you scroll away. */
function heroPushIn() {
  const media = document.querySelectorAll('[data-hero] .hero-still, [data-hero] .hero-video');
  if (!media.length) return;
  gsap.fromTo(media, { scale: 1 }, {
    scale: 1.06, ease: 'none',
    scrollTrigger: { trigger: '.hero', start: 'top top', end: 'bottom top', scrub: true },
  });
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

/** The intro statement rises in as one block (owner, 7 Oct: per-line masks clipped its glow). */
function introLines() {
  document.querySelectorAll<HTMLElement>('[data-rise]').forEach((el) => {
    gsap.from(el, {
      y: 24, opacity: 0, duration: 0.9, ease: 'power3.out',
      scrollTrigger: { trigger: el, start: 'top 85%', once: true },
    });
  });
}
