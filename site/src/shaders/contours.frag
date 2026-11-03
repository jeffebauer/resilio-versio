// Contour lines (waves B, "dissolving"), the owner's pick for Resilio, 30 Oct 2026: thin lines
// lifted by a slow swell, crisp at the left, widening and fraying toward the right like a
// sound's tail. Ported from the Sublustris site-visuals study (common.glsl + waves.glsl, V 1;
// written for that project by the same owner, no third-party code), with Resilio's own palette.
// Live only; the still is rendered from this same file (u_live 0, phase STILL_PH).
#define V 1
#define STILL_PH 1.884956
// The study's shared prelude. `vec2 field(vec2 p)` (below) returns x = light energy (0..1),
// y = how much of it is signal colour (0 = warm grey, 1 = signal). main() maps that onto the
// dark band palette.
precision highp float;
uniform vec2 u_res;     // render size in pixels
uniform float u_ph;     // loop phase: 0..TAU over one loop
uniform float u_t;      // seconds since start (live only; never wraps back)
uniform float u_loopT;  // loop length in seconds
uniform float u_live;   // 1 = live: time runs forward forever; 0 = seamless loop (files, stills)
uniform float u_amt;    // strength of the layer
#define TAU 6.28318530718

float h1(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
vec2 h2(vec2 p) {
  return fract(sin(vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)))) * 43758.5453);
}
float h4(vec3 p, float w) { return fract(sin(dot(p, vec3(127.1, 311.7, 74.7)) + w * 113.31) * 43758.5453); }

// Static 2D value noise (for fixed textures).
float vn2(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
  float a = h1(i), b = h1(i + vec2(1.0, 0.0)), c = h1(i + vec2(0.0, 1.0)), d = h1(i + vec2(1.0, 1.0));
  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

// 4D value noise: xy is space, zw is time. Time travels through zw along a path
// (see tpath), so the texture morphs without ever running backwards.
float vn4(vec4 p) {
  vec4 i = floor(p);
  vec4 f = fract(p);
  vec4 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
  float r[2];
  for (int k = 0; k < 2; k++) {
    float w = i.w + float(k);
    float a = h4(vec3(i.xy, i.z), w);
    float b = h4(vec3(i.xy + vec2(1.0, 0.0), i.z), w);
    float c = h4(vec3(i.xy + vec2(0.0, 1.0), i.z), w);
    float d = h4(vec3(i.xy + vec2(1.0, 1.0), i.z), w);
    float e = h4(vec3(i.xy, i.z + 1.0), w);
    float f2 = h4(vec3(i.xy + vec2(1.0, 0.0), i.z + 1.0), w);
    float g = h4(vec3(i.xy + vec2(0.0, 1.0), i.z + 1.0), w);
    float h = h4(vec3(i.xy + vec2(1.0, 1.0), i.z + 1.0), w);
    float lo = mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
    float hi = mix(mix(e, f2, u.x), mix(g, h, u.x), u.y);
    r[k] = mix(lo, hi, u.z);
  }
  return mix(r[0], r[1], u.w);
}

// Where "now" is in the zw time plane, for a path of radius R (in noise cells).
// Loop: once round a circle of radius R per loop, so the file closes seamlessly and never
// reverses (a big enough circle is not a back-and-forth). Live: a straight line at the same
// speed, so the live version never repeats at all.
// Live starts exactly at the still frame (phase STILL_PH, injected by build.py) and leaves
// along the circle's tangent, so the first live frame matches the fallback image.
float liveT() { return u_live * (u_t - STILL_PH / TAU * u_loopT); }
vec2 tpath(float R) {
  if (u_live > 0.5) {
    vec2 c = vec2(cos(STILL_PH), sin(STILL_PH));
    return R * c + vec2(-c.y, c.x) * (liveT() * TAU * R / u_loopT);
  }
  return R * vec2(cos(u_ph), sin(u_ph));
}

// Fractal sum of vn4; each octave doubles space and multiplies the time path by 1.6
// (finer detail drifts a little faster, as in real smoke). Octaves: 1..5.
float fbm4(vec2 p, vec2 tp, int oct) {
  float s = 0.0;
  float a = 0.5;
  float norm = 0.0;
  mat2 rm = mat2(0.8, -0.6, 0.6, 0.8);
  for (int k = 0; k < 5; k++) {
    if (k >= oct) break;
    s += a * vn4(vec4(p, tp + float(k) * 5.17));
    norm += a;
    p = rm * p * 2.03 + 17.0;
    tp *= 1.6;
    a *= 0.5;
  }
  // stretch around 0.5: each extra interpolation in 4D flattens value noise toward grey
  return clamp(0.5 + (s / norm - 0.5) * 1.3, 0.0, 1.0);
}

vec2 field(vec2 p);

void main() {
  vec2 p = (gl_FragCoord.xy - 0.5 * u_res) / u_res.y;
  vec2 fe = field(p);
  float e = clamp(fe.x, 0.0, 1.2) * u_amt;
  float b = clamp(fe.y, 0.0, 1.0);
  vec3 dark = vec3(0.047, 0.043, 0.039);   // --dark #0c0b0a
  vec3 mist = vec3(0.36, 0.34, 0.31);      // warm grey (the --on-dark-2 family, dimmed)
  vec3 sig = vec3(0.74, 0.25, 0.15);       // the signal orange-red (#ee5641 family), held down
  vec3 col = dark + e * mix(mist, sig, b) * 0.5;
  // static triangular dither (+-1 step) so dark gradients survive 8-bit and compression without banding
  col += (h1(gl_FragCoord.xy) + h1(gl_FragCoord.yx + 17.0) - 1.0) / 255.0;
  gl_FragColor = vec4(col, 1.0);
}
// B · Contour lines: thin level lines lifted by a slow swell; crisp at the left, and toward
// the right they widen, fray, jitter and fall apart, like the tail of a sound.
vec2 field(vec2 p) {
  float ar = u_res.x / u_res.y;
  vec2 tp = tpath(0.5);
  float px = 1.0 / u_res.y;
  float sp = 1.0 / 26.0;
  float dx = smoothstep(-0.42 * ar, 0.5 * ar, p.x);       // 0 at the left edge, 1 at the right
  float i0 = floor(p.y / sp);
  float acc = 0.0;
  float env0 = 0.0;
  for (int k = -2; k <= 2; k++) {
    float i = i0 + float(k);
    float base = (i + 0.5) * sp;
    float n1 = vn4(vec4(p.x * 1.4, i * 0.21, tp));
    float n2 = vn4(vec4(p.x * 3.1 + 4.0, i * 0.37, tp * 1.6 + 2.0));
    float n3 = vn4(vec4(p.x * 9.0 + 1.0, i * 0.83, tp * 2.2 + 6.0));
    float env = exp(-pow((base + 0.04) / 0.30, 2.0)) * (0.55 + 0.45 * (1.0 - dx));
    float off = (0.7 * n1 + 0.3 * n2 - 0.5) * 0.11 * env + dx * dx * (n3 - 0.5) * 0.045;
    float d = abs(p.y - base - off);
    float wdt = px * (0.5 + 1.2 * env) + dx * dx * sp * 0.15;
    float line = 1.0 - smoothstep(wdt - px, wdt + px + dx * sp * 0.25, d);
    float frag = mix(1.0, smoothstep(0.25, 0.75, n3) * 1.3, dx);
    acc += line * frag * (0.22 + 0.78 * env) * mix(1.0, 0.4, dx);
    env0 = max(env0, env);
  }
  return vec2(acc * 0.75, 0.25 + 0.6 * env0);
}
