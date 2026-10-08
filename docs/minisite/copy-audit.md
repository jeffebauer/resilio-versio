# Site copy audit: signs of AI writing

8 Oct 2026. An audit of the site's copy against the known signs of AI writing, with suggested rewrites. Nothing here is on the site yet: pick what you want and I'll apply it.

**Sources.** [Wikipedia: Signs of AI writing](https://en.wikipedia.org/wiki/Wikipedia:Signs_of_AI_writing), the most thorough public catalogue, kept up to date by editors who remove AI text. [Pangram: 9 signs of AI writing, backed by data](https://www.pangram.com/signs-of-ai-writing), rates per 10,000 words against human text (a detection company, so treat its numbers as indicative). Both agree that **no single sign proves anything**. Readers notice clusters: the same sentence shapes repeating, too tidy, too even.

## The short version

The site doesn't use the obvious AI vocabulary ("delve", "vibrant", "seamless") or em dashes. It reads as AI-written for other reasons:

1. **Colons everywhere.** The house style banned em dashes, so the colon took over their job. Many sentences go "statement: explanation", some with two colons. The panel callouts and tiles have **13.5 colons per 100 words**; a human writer uses about one.
2. **Negation pivots.** "A spring tank, not a room." "Nothing is added: it's your hit, hitting harder." "The reverb isn't a background room: it's…" This "not X, it's Y" shape is one of the most-cited tells.
3. **Bold-label lists.** About 120 bullets start with a bolded phrase and a full stop or colon ("**The throw.** …"). It's the "inline-header list" Wikipedia singles out, and it's 9× more common in AI text.
4. **Threes and verb pile-ups.** "throw snares into, splash, drive, filter like King Tubby, hold forever, push into a howl, or feed from a worn tape echo" (seven in a row). "thrown at single hits, ridden, filtered and muted". "Play it, tell us, hear it change."
5. **Tidy closers and slogans.** Short lines that land a point: "No Versio needed." "Every pass comes out different." "Your ears count too." "The steps themselves are part of the sound." One is style; one per paragraph is a pattern.
6. **Nobody's speaking.** The copy never says "I". It talks about you in the third person ("a designer and dub enthusiast made every sonic decision", "the owner's Wellspring"), and that faceless, process-report voice is a big part of why it reads as generated. The site is one person's project, and the copy should sound like it.
7. **Absolutes.** "Every knob position is usable", "every issue is read", "never a clean tone", "nothing is sampled". Some are true and worth saying. Stacked together they read like marketing.

What's fine and should stay: precise numbers in the manual, the manual's structure (it's reference, and it should read that way), and the clear "Sounds like / Play into it / Move next" pattern on Starting points, which helps scanning.

## One decision first: whose voice?

Everything below is drafted in **first person ("I")**, as you. It's the single biggest change: it fixes tell 6 outright and makes most of the rest easier.

- **I (recommended):** honest for a solo project, and it says plainly that you design by ear and Claude writes the code.
- **We:** fine for "Shape it with us", but it blurs who did what.
- **Resilio / third person:** what the site does now.

## Home page (most visitors only read this)

**Intro (under the tank image)**
> Before: A simulated spring tank you can throw snares into, splash, drive, filter like King Tubby, hold forever, push into a howl, or feed from a worn tape echo. Free firmware for the Versio Eurorack module, and the same sound as an AU/VST3 plugin for macOS.

> After: I wanted a spring reverb I could play the way dub engineers played theirs. Throw a snare into it, ride the filter like King Tubby, let the tail hang under the groove, or push it until it howls. There's a worn tape echo in front of it too. It's free firmware for the Versio, and the same sound runs as a plugin on a Mac.

Why: seven verbs in a row becomes a reason, then four moves. The second sentence was a fragment; now it's a sentence.

**The sound**
> Before: A spring tank, not a room. Every hit lands in the springs as its own splash: a bright clang on the attack, then echoes that sweep upward (the highs arrive after the lows: the "boing" of a real spring), then a tail that darkens and blurs into a wash instead of ticking like a delay.

> After: It sounds like a spring tank. Each hit lands as a bright clang, then the echoes sweep upward (that's the "boing": the highs arrive after the lows), and the tail goes darker and smears into a wash.

Why: drops "not a room" and "instead of ticking like a delay" (two negation pivots) and the colon inside a parenthesis inside a colon sentence.

**“Big Knob” (opening line)**
> Before: An Altec filter created the sweep that became a dub move.

> After: That filter sweep on Tubby's dubs came from an Altec broadcast filter.

And at the end of the story, cut the closer "and the steps themselves are part of the sound", or make it concrete: "and you can hear each step click in."

**Built around dub performance moves (intro)**
> Before: Dub treats the mixing desk as an instrument played live. The reverb isn't a background room: it's thrown at single hits, ridden, filtered and muted, then left to ring on its own, and every pass comes out different.

> After: In dub, the mixing desk is an instrument. Engineers threw single hits into the reverb, rode it, cut it and let it ring out, live, so no two passes were the same. Resilio is built for those moves.

**The plugin**
> Before: … Sketch a patch in your DAW, throw from a MIDI clip, follow the DAW's tempo in echo mode. No Versio needed.

> After: … You can sketch a patch in your DAW and throw from a MIDI clip, and in echo mode it follows your tempo. You don't need a Versio to use it.

**Status (Download section)**
> Before: Resilio is designed by ear: a designer and dub enthusiast made every sonic decision, with Claude (Anthropic's AI) as the engineering partner writing the DSP, the firmware and the tools.

> After: I'm a designer, not a DSP engineer. I made every sound decision by ear, and Claude (Anthropic's AI) wrote the DSP, the firmware and the tools.

**Shape it with us (intro)**
> Before: Resilio is tuned by ear, and your ears count too. Tell us what you hear, what you'd reach for and what gets in the way, and we'll refine it together.

> After: If something sounds wrong, or you want a sound it can't make yet, tell me. Ideas from people playing it are how it gets better.

**Feature and colour tiles, panel callouts** (the colon hotspot)
> Before: "TANK 1: one spring, sparse and the most splashy. TANK 2: two springs, the classic tank."

> After: "One spring in TANK 1, sparse and splashy. Two in TANK 2, the classic tank."

> Before: "Your hits, hitting harder. Nothing added; ghost notes stay quiet."

> After: "Makes your hits hit the springs harder. Ghost notes stay quiet."

> Before: "CLEAN · TAPE · VALVE: the saturation inside the tank, and µ-law grit…"

> After: "Three kinds of saturation inside the tank. TAPE and VALVE add µ-law grit to the reverb. Your dry signal stays clean."

I'd go through all 24 tile and callout lines the same way: no "Label: description" shape, and at most one colon per tile.

## Other pages

- **Credits:** "the owner's Teaching Machines Wellspring" → "my Teaching Machines Wellspring". "and the owner picked" → "and I picked". "turned each technical question into a musical one" is a tidy closer; I'd cut it.
- **FAQ:** mostly reads like a person already. Small fixes: "Two short forms do the asking:" → "There are two short forms."; "Only where you ask for it." can stay.
- **Starting points:** keep the structure. Vary the "Move next" lines a little so they don't all read as a list of commands with commas.
- **Manual and Install:** reference text. Leave it, except the colon chains in the ATTITUDE grit paragraph, which can become two plain sentences.
- **Changelog:** fine as release notes.

## Not AI tells, but wrong on the live site

These need fixing whatever you decide about the voice:

1. **Credits → Licence is an empty heading.** The page shows "Licence" with nothing under it, just a hidden owner note from before launch. Suggested: "Our code, tools and docs are MIT. The plugin is AGPLv3, because it's built on JUCE. The source is on GitHub."
2. **FAQ → "Is it free?" just says "Yes."** It also has a hidden note asking you to confirm. Suggested: "Yes. The firmware and plugin are free, and the source is open (MIT, with the plugin under AGPLv3)."
3. **FAQ → "Which modules does it run on?"** has a hidden note asking which Versio you played it on. Worth one line, e.g. "I play it on a [module]."

## Quotes

The markdown mixes straight and curly quotes, with curly quotes only around “Big Knob”, and the tile data mixes curly and straight apostrophes. The site's renderer turns them all curly, so visitors won't see the mix. It does show in the README on GitHub; low priority.
