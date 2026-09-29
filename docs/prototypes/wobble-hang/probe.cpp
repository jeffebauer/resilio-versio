// WOBBLE hang probe (prototype, not part of the repo).
//
// Hang metric: after each chord's note-off, every chord partial (D chord
// saws, < 1.3 kHz) is followed in time against its two neighbouring
// partials (mean dB of the partial just below and just above), smoothed
// over ~0.35 s so beating averages out. hang = how far that relative level
// climbs in the late tail (0.5 s on, until the neighbours have faded 50 dB)
// above where it sat just after note-off (0.1-0.4 s). Worst of mono, L, R.
// A partial that fades with the rest scores ~0-4 dB; one that hangs on
// scores its excess.
//
// probe grid <variant> <stim.wav> [decay] [attitude]  -> table (mean/max over note-offs)
// probe render <variant> <in.wav> <out.wav> key=val ... -> wav + metric line
#include "dsp/Tank.h"
#include "params/ParamSpec.h"
#include "Fft.h"
#include "Metrics.h"
#include "Wav.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace rv;

struct Settings {
    float decay = 0.5f, tone = 0.5f, tension = 0.5f, splash = 0.3f, drive = 0.25f, wobble = 0.2f, mix = 1.0f,
          springs = 0.5f, attitude = 0.5f;
};

static void setAll(Tank& t, const Settings& s)
{
    t.setParam(ParamId::Decay, s.decay);
    t.setParam(ParamId::Tone, s.tone);
    t.setParam(ParamId::Tension, s.tension);
    t.setParam(ParamId::Splash, s.splash);
    t.setParam(ParamId::Drive, s.drive);
    t.setParam(ParamId::Wobble, s.wobble);
    t.setParam(ParamId::Mix, s.mix);
    t.setParam(ParamId::Springs, s.springs);
    t.setParam(ParamId::Attitude, s.attitude);
}

static double g_renderSeconds = 0;
static std::vector<std::vector<float>> render(const wav::Audio& in, const Settings& s)
{
    Tank t;
    t.prepare(float(in.sampleRate), 48);
    setAll(t, s);
    const size_t n = in.frames();
    const float* L = in.channels[0].data();
    const float* R = in.channels.size() > 1 ? in.channels[1].data() : L;
    std::vector<std::vector<float>> out(2, std::vector<float>(n));
    auto t0 = std::chrono::steady_clock::now();
    for (size_t p = 0; p < n; p += 48) {
        const int m = int(std::min<size_t>(48, n - p));
        t.process(L + p, R + p, out[0].data() + p, out[1].data() + p, m);
    }
    g_renderSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    return out;
}

struct Hang {
    double db = -99, hz = 0, tAt = 0;
};

static std::vector<double> partials()
{
    std::vector<double> p;
    for (double f0 : {293.66, 369.99, 440.0})
        for (int k = 1; k * f0 < 1300.0; ++k) p.push_back(k * f0);
    std::sort(p.begin(), p.end());
    std::vector<double> q;
    for (double f : p)
        if (q.empty() || f > q.back() * 1.02) q.push_back(f);
    return q;
}

// Note-offs: last active sample before >= 2 s of silence.
static std::vector<double> noteOffs(const wav::Audio& in)
{
    const auto& c = in.channels[0];
    std::vector<double> offs;
    long last = -1;
    const long gap = long(2.0 * in.sampleRate);
    for (long i = 0; i < long(c.size()); ++i) {
        if (std::fabs(c[size_t(i)]) > 1.0e-3f) {
            if (last >= 0 && i - last > gap) offs.push_back(double(last) / in.sampleRate);
            last = i;
        }
    }
    if (last >= 0) offs.push_back(double(last) / in.sampleRate);
    return offs;
}

static double g_focusHz = 0; // if > 0, only this partial counts
static Hang measure(const std::vector<float>& x, int sr, double tOff, bool dump = false)
{
    const size_t N = 8192, hop = 512;
    const auto parts = partials();
    const size_t P = parts.size();
    const double binHz = double(sr) / N;
    std::vector<std::vector<double>> lv;
    std::vector<double> times;
    size_t st0 = size_t(std::max(0.0, tOff * sr - N / 2.0));
    for (size_t st = st0; st + N < x.size() && (st + N / 2.0) / sr < tOff + 6.5; st += hop) {
        auto mag = fft::magnitudeSpectrum(x.data() + st, N, N);
        std::vector<double> row;
        for (double f : parts) {
            const double w = std::max(8.0, 0.02 * f);
            size_t b0 = size_t(std::floor((f - w) / binHz)), b1 = size_t(std::ceil((f + w) / binHz));
            double e = 0;
            for (size_t b = b0; b <= b1 && b < mag.size(); ++b) e += double(mag[b]) * mag[b];
            row.push_back(10.0 * std::log10(e + 1e-20));
        }
        lv.push_back(row);
        times.push_back((st + N / 2.0) / sr - tOff);
    }
    const size_t F = lv.size();
    // relative level vs neighbours, and the neighbours' level
    std::vector<std::vector<double>> rel(F, std::vector<double>(P)), nb(F, std::vector<double>(P));
    for (size_t k = 0; k < F; ++k)
        for (size_t p = 0; p < P; ++p) {
            const double lo = p > 0 ? lv[k][p - 1] : lv[k][p + 1];
            const double hi = p + 1 < P ? lv[k][p + 1] : lv[k][p - 1];
            nb[k][p]  = 0.5 * (lo + hi);
            rel[k][p] = lv[k][p] - nb[k][p];
        }
    // smooth over ~0.35 s
    const int half = std::getenv("NOSMOOTH") ? 0 : int(0.175 * sr / hop);
    std::vector<std::vector<double>> rs(F, std::vector<double>(P));
    for (size_t k = 0; k < F; ++k)
        for (size_t p = 0; p < P; ++p) {
            double s = 0; int c = 0;
            for (int j = -half; j <= half; ++j) {
                const long kk = long(k) + j;
                if (kk < 0 || kk >= long(F)) continue;
                s += rel[size_t(kk)][p]; ++c;
            }
            rs[k][p] = s / c;
        }
    Hang h;
    for (size_t p = 0; p < P; ++p) {
        if (g_focusHz > 0 && std::fabs(parts[p] - g_focusHz) > 5.0) continue;
        double ref = 0; int nr = 0;
        double nb0 = -999;
        for (size_t k = 0; k < F; ++k)
            if (times[k] >= 0.1 && times[k] <= 0.4) { ref += rs[k][p]; ++nr; nb0 = std::max(nb0, nb[k][p]); }
        ref /= std::max(1, nr);
        for (size_t k = 0; k < F; ++k) {
            if (times[k] < 0.5) continue;
            if (nb[k][p] < nb0 - 50.0 || lv[k][p] < -100.0) break;
            const double d = rs[k][p] - ref;
            if (d > h.db) h = {d, parts[p], times[k]};
        }
    }
    if (dump) {
        std::printf("   t  ");
        for (size_t p = 0; p < P; ++p) std::printf(" %6.0f", parts[p]);
        std::printf("   (dB re neighbours, smoothed)\n");
        for (size_t k = 0; k < F; k += 16) {
            std::printf("%5.2f ", times[k]);
            for (size_t p = 0; p < P; ++p) std::printf(" %6.1f", rs[k][p]);
            std::printf("\n");
        }
    }
    return h;
}

struct Result {
    double mean = 0, max = -99, hzAtMax = 0;
    std::vector<Hang> per;
};

static Result evaluate(const std::vector<std::vector<float>>& out, int sr, const std::vector<double>& offs, bool dump = false)
{
    Result r;
    if (std::getenv("RING")) {
        auto m = metrics::compute(out, float(sr));
        r.mean = r.max = std::isnan(m.ringingDb) ? -1.0 : m.ringingDb;
        r.hzAtMax = std::isnan(m.ringingHz) ? 0.0 : m.ringingHz;
        return r;
    }
    std::vector<float> mono(out[0].size());
    for (size_t i = 0; i < mono.size(); ++i) mono[i] = 0.5f * (out[0][i] + out[1][i]);
    for (double t : offs) {
        Hang w = measure(mono, sr, t, dump);
        dump = false;
        for (int c = 0; c < 2; ++c) {
            Hang h = measure(out[size_t(c)], sr, t);
            if (h.db > w.db) w = h;
        }
        r.per.push_back(w);
        r.mean += w.db;
        if (w.db > r.max) { r.max = w.db; r.hzAtMax = w.hz; }
    }
    r.mean /= std::max<size_t>(1, offs.size());
    return r;
}

static bool parseKV(Settings& s, const std::string& a)
{
    auto eq = a.find('=');
    if (eq == std::string::npos) return false;
    const std::string k = a.substr(0, eq);
    const float v = float(std::atof(a.substr(eq + 1).c_str()));
    if (k == "decay") s.decay = v; else if (k == "tone") s.tone = v; else if (k == "tension") s.tension = v;
    else if (k == "splash") s.splash = v; else if (k == "drive") s.drive = v; else if (k == "wobble") s.wobble = v;
    else if (k == "mix") s.mix = v; else if (k == "springs") s.springs = v; else if (k == "attitude") s.attitude = v;
    else return false;
    return true;
}

int main(int argc, char** argv)
{
    if (argc < 4) { std::fprintf(stderr, "usage\n"); return 2; }
    const std::string mode = argv[1];
    Tank::protoWobbleShare = std::atoi(argv[2]);
    if (const char* e = std::getenv("SHARE_FROM")) Tank::protoShareFrom = float(std::atof(e));
    if (const char* e = std::getenv("SHARE_TO")) Tank::protoShareTo = float(std::atof(e));
    if (const char* e = std::getenv("FOCUS")) g_focusHz = std::atof(e);
    if (const char* e = std::getenv("FLOOR")) Tank::protoIndepFloor = float(std::atof(e));
    wav::Audio in;
    std::string err;
    if (!wav::read(argv[3], in, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    const auto offs = noteOffs(in);

    if (mode == "grid") {
        Settings base;
        for (int i = 4; i < argc; ++i) parseKV(base, argv[i]);
        std::vector<float> tens = {0.5f, 0.75f, 0.875f, 1.0f};
        if (const char* e = std::getenv("TENS")) {
            tens.clear();
            std::string t = e; size_t a = 0;
            while (a <= t.size()) { size_t b = t.find(',', a); if (b == std::string::npos) b = t.size(); tens.push_back(float(std::atof(t.substr(a, b - a).c_str()))); a = b + 1; }
        }
        const float wobs[] = {0.0f, 0.1f, 0.2f, 0.3f, 0.5f};
        const float sprs[] = {0.0f, 0.5f, 1.0f};
        struct Cell { Settings s; Result r; };
        std::vector<Cell> cells;
        for (float sp : sprs)
            for (float te : tens)
                for (float wo : wobs) {
                    Settings s = base;
                    s.springs = sp; s.tension = te; s.wobble = wo;
                    cells.push_back({s, {}});
                }
        std::mutex mu;
        size_t next = 0;
        auto worker = [&]() {
            for (;;) {
                size_t i;
                { std::lock_guard<std::mutex> l(mu); if (next >= cells.size()) return; i = next++; }
                auto out = render(in, cells[i].s);
                cells[i].r = evaluate(out, in.sampleRate, offs);
            }
        };
        std::vector<std::thread> th;
        const unsigned nt = std::max(2u, std::thread::hardware_concurrency());
        for (unsigned k = 0; k < nt; ++k) th.emplace_back(worker);
        for (auto& t : th) t.join();
        std::printf("variant %d  decay %.2f att %.1f tone %.2f  %zu note-offs  hang dB mean/max@Hz; WOBBLE 0 / .1 / .2 / .3 / .5\n",
                    Tank::protoWobbleShare, base.decay, base.attitude, base.tone, offs.size());
        size_t k = 0;
        for (float sp : sprs)
            for (float te : tens) {
                std::printf("S%d T%.3f |", int(sp * 2 + 1.5f), te);
                for (int w = 0; w < 5; ++w, ++k)
                    std::printf("  %4.1f/%4.1f@%4.0f", cells[k].r.mean, cells[k].r.max, cells[k].r.hzAtMax);
                std::printf("\n");
            }
        // summary over 2- and 3-Spring rows: mean of cell means, max of cell maxima
        std::printf("SUM S2+S3 |");
        const size_t rows1 = tens.size();
        for (int w = 0; w < 5; ++w) {
            double m = 0, mx = -99; int c = 0;
            for (size_t r = rows1; r < 3 * rows1; ++r) {
                const auto& cr = cells[r * 5 + size_t(w)].r;
                m += cr.mean; ++c; mx = std::max(mx, cr.max);
            }
            std::printf("  %4.1f/%4.1f     ", m / c, mx);
        }
        std::printf("\nSUM S1    |");
        for (int w = 0; w < 5; ++w) {
            double m = 0, mx = -99; int c = 0;
            for (size_t r = 0; r < rows1; ++r) {
                const auto& cr = cells[r * 5 + size_t(w)].r;
                m += cr.mean; ++c; mx = std::max(mx, cr.max);
            }
            std::printf("  %4.1f/%4.1f     ", m / c, mx);
        }
        std::printf("\n");
        return 0;
    }
    if (mode == "render") {
        Settings s;
        const std::string outPath = argv[4];
        for (int i = 5; i < argc; ++i)
            if (!parseKV(s, argv[i])) { std::fprintf(stderr, "bad arg %s\n", argv[i]); return 2; }
        auto out = render(in, s);
        Result r = evaluate(out, in.sampleRate, offs, std::getenv("DUMP") != nullptr);
        double pk = 0, ss = 0;
        for (auto& c : out) for (float v : c) { pk = std::max(pk, double(std::fabs(v))); ss += double(v) * v; }
        std::printf("hang mean %.1f max %.1f dB @ %.0f Hz  [", r.mean, r.max, r.hzAtMax);
        for (auto& h : r.per) std::printf(" %.1f@%.0f", h.db, h.hz);
        std::printf(" ]  peak %.2f dBFS  rms %.2f dBFS  render %.3f s (%.1f us/sample)\n", 20 * std::log10(pk + 1e-12),
                    10 * std::log10(ss / (2.0 * out[0].size()) + 1e-30), g_renderSeconds,
                    1e6 * g_renderSeconds / double(out[0].size()));
        if (outPath != "-") {
            if (const char* e = std::getenv("GAIN_DB")) {
                const float g = float(std::pow(10.0, std::atof(e) / 20.0));
                for (auto& c : out) for (auto& v : c) v *= g;
            }
            wav::Audio o;
            o.sampleRate = in.sampleRate; o.bitsPerSample = 24; o.channels = out;
            if (!wav::write(outPath, o, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
        }
        return 0;
    }
    return 2;
}
