// Sound checks for Comet, compiled for the host and run by
// python sdk.py test --machine <folder>.
#include "../fx.cpp"
#include <cmath>
#include <cstdio>
#include <vector>

static int failures = 0;
static void check(const char* what, bool ok) {
    if (!ok) { std::printf("FAIL %s\n", what); ++failures; }
}

static opxy::i32 words[comet::kBufferWords];
static const opxy::Buffer kBuffer = {words, comet::kBufferWords};

static opxy::i32 amount(double x) { return static_cast<opxy::i32>(x * 2147483647.0); }
static opxy::i32 pick(unsigned index, unsigned count) {
    return static_cast<opxy::i32>((index * 2 + 1) * (2147483648.0 / (2 * count)));
}
// Free Time spans range.h's kFreeOctaves from 0.2 ms: to 1.4 s (Comet) or 2.5 s (Comet Long).
static const double kOctaves = comet::kFreeOctaves / 65536.0;
static const double kLongestMs = 0.2 * std::pow(2.0, kOctaves);
static opxy::i32 free_time(double ms) { return amount(std::log2(ms / 0.2) / kOctaves); }

// Page 1: Sync, Time, Feedback, Dry. Page 2: Taps, Deviate, Curve, Diffuse.
// Shift row: Mode, Pingpong.
struct Setting {
    bool sync = false, random = false;
    opxy::i32 time = free_time(100);
    double feedback = 0, deviate = 0, diffuse = 0, curve = 0.5;
    double dry = 1;
    bool pingpong = false;  // the shift row's, with random (Mode)
    unsigned taps = 1;
    void words(opxy::i32* p) const {
        p[0] = pick(sync, 2); p[1] = time; p[2] = amount(feedback); p[3] = amount(dry);
        p[4] = pick(taps - 1, 5); p[5] = amount(deviate); p[6] = amount(curve); p[7] = amount(diffuse);
        p[8] = pick(random ? 1 : 0, comet::kModes); p[9] = pick(pingpong, 2); p[10] = p[11] = 0;
    }
};

static void stale() {  // a previous effect left loud data in the send buffer
    for (opxy::u32 i = 0; i < comet::kBufferWords; ++i) words[i] = (i & 1) ? 0x60006000 : -0x60006000;
}

template <class Input>
static void run(comet::State& state, const Setting& s, unsigned blocks, Input input, std::vector<double>& left,
                std::vector<double>& right, opxy::Transport transport = opxy::Transport{1200, 0, true},
                opxy::Buffer buffer = kBuffer) {
    opxy::i32 p[12];
    s.words(p);
    opxy::i32 l[opxy::block_frames], r[opxy::block_frames];
    for (unsigned b = 0; b < blocks; ++b) {
        for (unsigned i = 0; i < opxy::block_frames; ++i) l[i] = r[i] = input(left.size() + i);
        comet::process(state, buffer, transport, l, r, p);
        for (unsigned i = 0; i < opxy::block_frames; ++i) {
            left.push_back(l[i] / 2147483648.0);
            right.push_back(r[i] / 2147483648.0);
        }
    }
}

static opxy::i32 click(size_t n) { return n == 0 ? 0x20000000 : 0; }  // a quarter-scale impulse

static double peak(const std::vector<double>& x, size_t from, size_t to) {
    double m = 0;
    for (size_t i = from; i < to && i < x.size(); ++i) m = std::fmax(m, std::fabs(x[i]));
    return m;
}
static size_t first(const std::vector<double>& x, double above) {
    for (size_t i = 0; i < x.size(); ++i)
        if (std::fabs(x[i]) > above) return i;
    return x.size();
}
// A repeat's level: it may fall between samples, so the sum around it.
static double repeat(const std::vector<double>& x, size_t at) {
    double sum = 0;
    for (size_t i = at - 3; i <= at + 3 && i < x.size(); ++i) sum += std::fabs(x[i]);
    return sum;
}
static double energy(const std::vector<double>& x, size_t from, size_t to) {
    double e = 0;
    for (size_t i = from; i < to && i < x.size(); ++i) e += x[i] * x[i];
    return e;
}

static void response(const Setting& s, unsigned blocks, std::vector<double>& l, std::vector<double>& r,
                     opxy::Transport transport = opxy::Transport{1200, 0, true}) {
    stale();
    static comet::State state;
    comet::initialize(state);
    run(state, s, blocks, click, l, r, transport);
}

int main() {
    static_assert(sizeof(comet::State) <= opxy::effect_buffered_state_bytes - 4, "State fits the send slot with a shift row");
    static_assert(comet::kBufferWords * 4 == 524288, "the buffer matches definition.json");
    opxy::u32 regions = 0;
    for (opxy::u32 s = 0; s < comet::kStages; ++s) {
        const opxy::u32 n = comet::kRegion[s];
        check("each stage's region is a power of two that holds both sides",
              (n & (n - 1)) == 0 && comet::kLeftLength[s] < n && comet::kRightLength[s] < n);
        check("stages sit end to end", comet::kOffset[s] == regions);
        regions += n;
    }
    check("the diffuser's words add up", regions == comet::kDiffuserWords);
    check("the lines and diffusers fill the buffer", comet::kLines + 2 * comet::kDiffuserWords <= comet::kBufferWords);
    check("the line holds the longest free Time", comet::kLine > kLongestMs * 44.1);

    // One tap at 100 ms: silence until it, whatever the buffer held, then the
    // click at half its level, as Ping Pong returns it.
    {
        Setting s;
        std::vector<double> l, r;
        response(s, 200, l, r);
        check("silence before the first repeat, despite a stale buffer", peak(l, 0, 4405) == 0 && peak(r, 0, 4405) == 0);
        check("the first repeat lands at the free Time", first(l, 0.01) >= 4408 && first(l, 0.01) <= 4412);
        check("at half the click's level", std::fabs(repeat(l, 4410) - 0.125) < 0.003);
        check("no feedback, no second repeat", peak(l, 4416, l.size()) == 0);
    }
    // Free Time: 0.2 ms to its longest (1.4 s, or 2.5 s in Comet Long).
    {
        const double longest = kLongestMs * 44.1;
        check("free Time starts at 0.2 ms", std::fabs(comet::free_samples_q12(0) / 4096.0 - 8.82) < 0.01);
        check("free Time ends at its longest", std::fabs(comet::free_samples_q12(amount(1)) / 4096.0 - longest) < longest * 0.003);
        check("its longest is 1.4 s or 2.5 s", std::fabs(kLongestMs - 1400) < 5 || std::fabs(kLongestMs - 2500) < 5);
        Setting s;
        s.time = amount(1);
        std::vector<double> l, r;
        response(s, static_cast<unsigned>(longest / 64) + 100, l, r);
        check("the longest repeat plays", first(l, 0.01) >= longest - 60 && first(l, 0.01) <= longest + 60);
    }
    // Sync: D-Delay's 13 note lengths from 1/32 to 1/2 at the tempo.
    {
        const double beats[comet::kDivisions] = {1 / 8., 1 / 6., 3 / 16., 1 / 4., 1 / 3., 3 / 8., 1 / 2.,
                                                2 / 3., 3 / 4., 1.,      4 / 3., 3 / 2., 2.};
        bool all = true;
        for (unsigned i = 0; i < comet::kDivisions; ++i) {
            all = all && std::fabs(comet::synced_samples_q12(i, 1500) / 4096.0 - beats[i] * 44100 * 60 / 150) < 0.01;
            all = all && comet::division(pick(i, comet::kDivisions)) == i;
        }
        check("Sync plays 1/32 to 1/2 at the tempo", all);
        Setting s;
        s.sync = true;
        s.time = pick(6, comet::kDivisions);  // 1/8 at 120 BPM: 11025 samples
        std::vector<double> l, r;
        response(s, 300, l, r);
        check("a synced eighth lands on the beat", first(l, 0.01) >= 11024 && first(l, 0.01) <= 11026);
        std::vector<double> l0, r0;
        response(s, 300, l0, r0, opxy::Transport{0, 0, false});
        check("no tempo plays as 120 BPM", first(l0, 0.01) == first(l, 0.01));
        s.time = pick(12, comet::kDivisions);  // 1/2 at 40 BPM: 3 s, longer than the line
        std::vector<double> l1, r1;
        response(s, 2100, l1, r1, opxy::Transport{400, 0, true});
        check("a note longer than the line stops at its end", first(l1, 0.01) >= comet::kLine - 4 && first(l1, 0.01) < comet::kLine);
    }
    // Taps and Deviate (Inc): taps 2..5 a quarter of Deviate apart.
    {
        Setting s;
        s.taps = 3;
        s.deviate = 1;
        std::vector<double> l, r;
        response(s, 200, l, r);
        const double level = 0.125 * std::pow(3.0, -0.367877);
        bool all = true;
        for (unsigned k = 0; k < 3; ++k) all = all && std::fabs(repeat(l, static_cast<size_t>(4410 * (1 + 0.25 * k))) - level) < 0.003;
        check("three taps at 1x, 1.25x and 1.5x Time, at D-Delay's level", all);
        check("nothing between the taps", peak(l, 4416, 5508) == 0);
        // One tap is a plain delay, as in D-Delay: Deviate and Mode change
        // nothing, whatever they are set to.
        Setting plain;
        plain.feedback = 0.5;
        std::vector<double> lp, rp;
        response(plain, 400, lp, rp);
        bool same = true;
        for (int mode = 0; mode < 2; ++mode)
            for (double deviate : {0.0, 0.5, 1.0}) {
                Setting one = plain;
                one.random = mode;
                one.deviate = deviate;
                std::vector<double> l1, r1;
                response(one, 400, l1, r1);
                same = same && l1 == lp && r1 == rp;
            }
        check("one tap is a plain delay: Deviate and Mode change nothing", same);
    }
    // Mode Rnd: taps scatter within Deviate's reach, each side its own way.
    {
        Setting s;
        s.taps = 5;
        s.deviate = 1;
        s.random = true;
        std::vector<double> l, r;
        response(s, 300, l, r);
        check("Rnd keeps every repeat between Time and twice Time",
              peak(l, 0, 4405) == 0 && peak(l, 8826, l.size()) == 0 && peak(r, 0, 4405) == 0 && peak(r, 8826, r.size()) == 0);
        check("Rnd scatters each side its own way", l != r);
        std::vector<double> l2, r2;
        response(s, 300, l2, r2);
        check("the same setting scatters the same way", l2 == l);
    }
    // Rnd's places, as D-Delay sets them: 1 + Deviate * (k/4)^Curve * a random
    // 0..1 for each tap and side, here a fixed draw.
    {
        bool formula = true;
        for (unsigned m = 1; m < comet::kModes; m += 2)
            for (unsigned side = 0; side < 2; ++side)
                for (unsigned k = 1; k < 5; ++k) {
                    const double draw = comet::kDraw[m / 2][side][k - 1] / 100.0;
                    const double at = comet::spread(32768, m, 5, side, k) / 32768.0;
                    formula = formula && std::fabs(at - (1 + draw * k / 4.0)) < 0.002;
                }
        check("Rnd: each tap at 1 + Deviate * k/4 * its draw, as D-Delay", formula);
        // Mode runs Inc, Rnd, Inc, Rnd, ...: every Inc the same, every Rnd its own draw.
        bool incs = true, fresh = true;
        for (unsigned m = 0; m < comet::kModes; m += 2)
            for (unsigned k = 0; k < 5; ++k) incs = incs && comet::spread(32768, m, 5, 0, k) == 32768 + k * 8192;
        for (unsigned a = 1; a < comet::kModes; a += 2)
            for (unsigned b = a + 2; b < comet::kModes; b += 2) {
                bool same = true;
                for (unsigned side = 0; side < 2; ++side)
                    for (unsigned k = 1; k < 5; ++k) same = same && comet::spread(32768, a, 5, side, k) == comet::spread(32768, b, 5, side, k);
                fresh = fresh && !same;
            }
        check("every Inc on Mode spreads the taps evenly", incs);
        check("each Rnd on Mode is a different draw", fresh);
        bool decoded = true;
        for (unsigned m = 0; m < comet::kModes; ++m) decoded = decoded && comet::choice(pick(m, comet::kModes), comet::kModes) == m;
        check("Mode's 254 choices decode as the stock word encodes them", decoded);
        const double bent = comet::spread(32768, true, 5, 0, 2, comet::curve_exponent(0)) / 32768.0;
        check("Rnd: Curve bends the share before the draw, as D-Delay",
              std::fabs(bent - (1 + comet::kDraw[0][0][1] / 100.0 * std::pow(0.5, 0.125))) < 0.003);
    }
    // Curve: even in the middle; below it the taps crowd late, above it early;
    // the first and last taps stay where they are.
    {
        using comet::curve_exponent;
        bool middle = true, ends = true, order = true, smooth = true;
        const opxy::u32 half = static_cast<opxy::u32>(amount(0.5)) >> 16;  // the control at 50
        for (unsigned k = 0; k < 5; ++k)
            middle = middle && comet::spread(32768, false, 5, 0, k, curve_exponent(half)) == 32768 + k * 8192;
        double before = 2;
        for (opxy::u32 c = 0; c < 32768; c += 256) {
            const opxy::u32 e = curve_exponent(c);
            ends = ends && comet::spread(32768, false, 5, 0, 0, e) == 32768 && comet::spread(32768, false, 5, 0, 4, e) == 65536;
            for (unsigned k = 1; k < 5; ++k) order = order && comet::spread(32768, false, 5, 0, k, e) >= comet::spread(32768, false, 5, 0, k - 1, e);
            // Turning Curve up moves the middle tap earlier, a little at a time.
            const double at = comet::spread(32768, false, 5, 0, 2, e) / 32768.0;
            smooth = smooth && at <= before + 1e-9 && (before > 1.99 || before - at < 0.02);
            before = at;
        }
        check("Curve sweeps the middle tap smoothly, earlier as it rises", smooth);
        const double late = comet::spread(32768, false, 5, 0, 2, curve_exponent(0)) / 32768.0,
                     early = comet::spread(32768, false, 5, 0, 2, curve_exponent(32767)) / 32768.0;
        check("Curve's middle is the even spread", middle);
        check("Curve keeps the first and last taps", ends);
        check("Curve keeps the taps in order", order);
        char what[96];
        std::snprintf(what, sizeof(what), "Curve bends the middle tap late (%.3f) and early (%.3f)", late, early);
        check(what, std::fabs(late - (1 + std::pow(0.5, 0.125))) < 0.01 && std::fabs(early - (1 + std::pow(0.5, 4.0))) < 0.01);
        // D-Delay's Curve scale: exponent 1/8 + 31/8 * Curve^2.12655.
        bool scale = true;
        for (double c : {0.1, 0.25, 0.4, 0.6, 0.75, 0.9}) {
            const double e = comet::curve_exponent(static_cast<opxy::u32>(c * 32767)) / 65536.0;
            scale = scale && std::fabs(e - (0.125 + 3.875 * std::pow(c, 2.12655))) < 0.01;
        }
        check("Curve follows D-Delay's scale, 1/8 to 4", scale);
        Setting s;
        s.taps = 5;
        s.deviate = 1;
        s.curve = 1;
        std::vector<double> l, r;
        response(s, 300, l, r);
        check("Curve 100 moves tap 2 to just after Time", first(l, 0.01) == 4410 && repeat(l, 4410 + 4410 / 256) > 0.02);
        Setting one = s;
        one.taps = 1;
        Setting plain = one;
        plain.curve = 0.5;
        std::vector<double> l1, r1, lp, rp;
        response(one, 300, l1, r1);
        response(plain, 300, lp, rp);
        check("with one tap Curve changes nothing", l1 == lp && r1 == rp);
    }
    // Pingpong on: the send enters on the left and the repeats bounce left,
    // right, left at the same time on each side.
    {
        Setting s;
        s.pingpong = true;
        s.feedback = 0.5;
        std::vector<double> l, r;
        response(s, 400, l, r);
        check("Pingpong on: the first repeat on the left only", repeat(l, 4410) > 0.05 && peak(r, 0, 8810) < 1e-3);
        check("then on the right", repeat(r, 8820) > 0.02 && peak(l, 4420, 13220) < 1e-3);
        check("then on the left again", repeat(l, 13230) > 0.01 && peak(r, 8830, 17630) < 1e-3);
        Setting off = s;
        off.pingpong = false;
        std::vector<double> l0, r0;
        response(off, 100, l0, r0);
        check("the bounce comes at the level of a plain repeat", std::fabs(repeat(l, 4410) - repeat(l0, 4410)) < 1e-3);
    }
    // Pingpong off: each side repeats on its own side only.
    {
        Setting s;
        s.feedback = 0.5;
        std::vector<double> l, r;
        response(s, 400, l, r);
        check("Pingpong off: both sides repeat together", repeat(l, 8820) > 0.02 && repeat(r, 8820) > 0.02);
    }
    // Feedback: repeats die away, never clip, and hold at the top.
    {
        Setting s;
        s.taps = 5;
        s.deviate = 0.5;
        s.feedback = 1;
        s.diffuse = 1;
        s.pingpong = true;
        s.time = free_time(30);
        stale();
        static comet::State state;
        comet::initialize(state);
        std::vector<double> l, r;
        run(state, s, 690 * 20, [](size_t n) {
            // half-scale noise, where the OP-XY's engines peak
            return n < 44100 ? static_cast<opxy::i32>((n * 2654435761u) & 0xffffffffu) / 2 : 0;
        }, l, r);
        check("half-scale noise through five taps at full Feedback never clips", peak(l, 0, l.size()) < 0.999 && peak(r, 0, r.size()) < 0.999);
        check("even full Feedback dies away (it stops at 95 %)", peak(l, l.size() - 4410, l.size()) < 1e-3);
    }
    // Diffuse smears each repeat: lower peaks, the energy spread longer.
    {
        Setting dry, wet;
        wet.diffuse = 1;
        std::vector<double> l0, r0, l1, r1;
        response(dry, 300, l0, r0);
        response(wet, 300, l1, r1);
        check("Diffuse lowers the repeat's peak", peak(l1, 0, l1.size()) < 0.5 * peak(l0, 0, l0.size()));
        check("and spreads it past 30 ms", energy(l1, 4410 + 1323, l1.size()) > 0.4 * energy(l1, 0, l1.size()));
        check("an allpass diffuser keeps the energy", std::fabs(energy(l1, 0, l1.size()) / energy(l0, 0, l0.size()) - 1) < 0.1);
    }
    // Short times: a Karplus-Strong string. One tap at 2.27 ms (100 samples)
    // with Feedback 90 % rings at its pitch, sustains for over a second, and
    // loses its upper harmonics faster than its fundamental, as a plucked
    // string does.
    {
        Setting s;
        s.time = free_time(1000.0 / 441);
        s.feedback = 0.9;
        std::vector<double> l, r;
        {
            // Plucked as Karplus-Strong plucks: 100 samples of noise at half
            // scale, where the OP-XY's engines peak.
            stale();
            static comet::State state;
            comet::initialize(state);
            run(state, s, 800, [](size_t n) {  // 1.16 s
                return n < 100 ? static_cast<opxy::i32>((n * 2654435761u) & 0xffffffffu) / 2 : 0;
            }, l, r);
        }
        auto rms = [&](size_t from, size_t to) { return std::sqrt(energy(l, from, to) / (to - from)); };
        // The period: the autocorrelation's peak near 100 samples (100.5 with the average).
        size_t best = 0;
        double most = -1;
        for (size_t lag = 90; lag <= 110; ++lag) {
            double c = 0;
            for (size_t n = 8820; n < 17640; ++n) c += l[n] * l[n + lag];  // after the pluck
            if (c > most) { most = c; best = lag; }
        }
        check("a 2.27 ms string rings at its period", best == 100 || best == 101);
        check("and sustains past a second", rms(44100, 48510) > 0.2 * rms(4410, 8820));
        // Its brightness: the share of energy in sample-to-sample change falls as it rings.
        auto bright = [&](size_t from, size_t to) {
            double d = 0;
            for (size_t n = from + 1; n < to; ++n) d += (l[n] - l[n - 1]) * (l[n] - l[n - 1]);
            return d / energy(l, from, to);
        };
        check("and darkens as it rings", bright(44100, 48510) < 0.5 * bright(441, 4410));
        // The same Feedback on an echo (100 ms) repeats as before, about 0.9 x 0.95 each time.
        Setting e;
        e.feedback = 0.9;
        std::vector<double> le, re;
        response(e, 300, le, re);
        double second = 0;
        for (size_t n = 8820 - 6; n <= 8820 + 6; ++n) second += le[n];
        check("an echo above 50 ms is unchanged: no average, Feedback up to 0.95",
              std::fabs(second / 0.125 - 0.9 * 0.95) < 0.02);
    }
    // Loud material comes back clean: a sine at full and at half scale,
    // through Feedback, Taps and Diffuse, is the same sine at half level.
    {
        auto distortion = [](double level, double feedback, unsigned taps, double diffuse) {
            Setting s;
            s.time = free_time(50);
            s.feedback = feedback;
            s.taps = taps;
            s.deviate = 0.5;
            s.diffuse = diffuse;
            static comet::State state;
            stale();
            comet::initialize(state);
            std::vector<double> l, r;
            const double w = 2 * 3.14159265358979 * 441.0 / 44100;
            run(state, s, 1200, [w, level](size_t n) { return static_cast<opxy::i32>(level * 2147000000.0 * std::sin(w * n)); }, l, r);
            double sc = 0, cc = 0, total = 0;
            for (size_t i = l.size() - 4400; i < l.size(); ++i) {
                sc += l[i] * std::sin(w * i);
                cc += l[i] * std::cos(w * i);
                total += l[i] * l[i];
            }
            const double fitted = (sc * sc + cc * cc) * 2 / 4400;
            return std::sqrt(std::fmax(0.0, total - fitted) / std::fmax(fitted, 1e-12));
        };
        check("a full-scale sine repeats clean", distortion(1, 0, 1, 0) < 1e-3);
        check("a half-scale sine stays clean through 60 % Feedback", distortion(0.5, 0.6, 1, 0) < 1e-3);
        check("and through five spread taps, Feedback and Diffuse", distortion(0.5, 0.6, 5, 1) < 5e-3);
    }
    // A send returns only its repeats: nothing of the input passes straight through.
    {
        bool none = true;
        for (unsigned taps = 1; taps <= 5; ++taps) {
            Setting s;
            s.taps = taps;
            s.feedback = 1;
            s.diffuse = 1;
            s.pingpong = true;
            s.deviate = 1;
            std::vector<double> l, r;
            response(s, 80, l, r);
            none = none && peak(l, 0, 4400) == 0 && peak(r, 0, 4400) == 0;
        }
        check("no input reaches the return before the first repeat", none);
    }
    // Moving Time glides like tape, without a click.
    {
        Setting a, b;
        a.time = free_time(50);
        b.time = free_time(80);
        a.feedback = b.feedback = 0.6;
        stale();
        static comet::State state;
        comet::initialize(state);
        std::vector<double> l, r;
        auto tone = [](size_t n) { return static_cast<opxy::i32>(0x10000000 * std::sin(n * 0.0285)); };
        run(state, a, 300, tone, l, r);
        run(state, b, 100, tone, l, r);
        double jump = 0;
        for (size_t i = 300 * 64 - 64; i < l.size(); ++i) jump = std::fmax(jump, std::fabs(l[i] - l[i - 1]));
        check("a Time change glides without a click", jump < 0.02);
    }
    // Outside a send there is no buffer: silence.
    {
        static comet::State state;
        comet::initialize(state);
        std::vector<double> l, r;
        run(state, Setting(), 10, [](size_t) { return 0x40000000; }, l, r, opxy::Transport{1200, 0, true},
            opxy::Buffer{nullptr, 0});
        check("silent without a buffer", peak(l, 0, l.size()) == 0 && peak(r, 0, r.size()) == 0);
    }
    if (failures) return 1;
    std::puts("comet ok");
    return 0;
}
