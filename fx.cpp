#include "core.h"
#include <opxy/q31.hpp>

// Comet, inspired by the Ursa Major SST-282 and Sabroi's AS D-Delay, on the
// core Ping Pong's foundation. The
// send enters the lines at half level with the fed-back repeats, through the
// diffuser; up to five taps per side read the lines back.
//
//   in/2 + Feedback x repeats --> diffuser (Diffuse) --> lines --> taps --> out
//          ^                                                                |
//          +---------------- crossed over when Pingpong is on -------------+
//
// Time sets the first tap; Deviate spreads the others up to twice as far,
// evenly or scattered (Mode). Pingpong on is a full ping-pong: the send enters
// on the left and every repeat crosses to the other side, so the repeats
// bounce left, right, left. A send effect: it
// returns only the repeats, and the stock mixer keeps the source's dry.
//
// The tap reading, the unwritten-sample check, the glide and the saturating
// add are Ping Pong's (MIT, Open-XY contributors). The lines keep Q31 samples.
namespace comet {
namespace {
using opxy::q31::multiply;
const i32 kMaximum = opxy::q31::maximum, kMinimum = -kMaximum - 1;
const i32 kShortest = 2 << 8;  // Q8 samples: down to comb and resonator times
const i32 kLongest = static_cast<i32>((kLine - 2) << 8);
const i32 kMostFeedback = 2104533975;  // 0.98, for echoes
const i32 kMostRinging = 2146409906;   // 0.9995: a Karplus-Strong string's longest sustain
// Karplus-Strong comes in below 50 ms and is whole at 20 ms and shorter.
const i64 kStringStart = 2205 << 8, kStringWhole = 882 << 8;  // Q8 samples: 50 ms, 20 ms
// The string's share per Q8 sample below kStringStart: a constant, so the blend
// needs no 64-bit division (the macOS compiler profile has no runtime for one).
const u32 kStringScale = static_cast<u32>(kMaximum / (kStringStart - kStringWhole));
const i32 kInner = 1610612736;         // D-Delay's first two allpasses of a series, 0.75
const i32 kOuter = 1342177280;         // its last two, 0.625
// D-Delay's tap level n^-0.368 for n taps, so more taps sound about as loud.
const i32 kLevel[kTaps] = {kMaximum, 1664132314, 1433533775, 1289572734, 1187941016};
// The loop's limiter holds the repeat fed back under half of full scale
// (-6 dB), so a long Feedback on held or loud sound swells and settles like
// tape instead of clipping.
const i32 kCeiling = 1073741824;

using opxy::q31::add;  // saturating, one instruction on the device

i32 control(i32 word) { return word < 0 ? 0 : word; }

// a and b blended, b's share Q31.
i32 blend(i32 a, i32 b, i32 share) { return add(multiply(a, kMaximum - share), multiply(b, share)); }

// The two lines: reads `delay` (Q8) samples back between the two nearest
// samples, or silence where this effect has not written since initialize (the
// buffer is never cleared), and writes a frame.
struct Reader {
    const i32* sides[2];
    i32* const left_line;
    u32 write, written;
    Reader(i32* left_side, i32* right_side, u32 at, u32 count)
        : sides{left_side, right_side}, left_line(left_side), write(at), written(count) {}
    i32 sample(unsigned side, u32 age) const {  // 1 is the last written
        if (__builtin_expect(age > written, 0)) return 0;
        u32 at = write - age;
        if (static_cast<i32>(at) < 0) at += kLine;
        return sides[side][at];
    }
    i32 tap(unsigned side, i32 delay) const {
        const u32 whole = static_cast<u32>(delay) >> 8, part = static_cast<u32>(delay) & 255;
        const i32 near = sample(side, whole), far = sample(side, whole + 1);
        // near + (far - near) x part / 256, halved first so it can't overflow
        const i32 step = opxy::q31::half(far) - opxy::q31::half(near);
        return near + 2 * multiply(step, static_cast<i32>(part << 23));
    }
    void store(i32 l, i32 r) {
        left_line[write] = l;
        left_line[kLine + write] = r;
        write = write + 1 == kLine ? 0 : write + 1;
        if (written < kLine) ++written;
    }
};

// About a 23 ms glide, so a new time slides like tape instead of clicking.
i32 glide(i32 value, i32 target) {
    const i32 difference = target - value, step = difference / 1024;
    return value + (step ? step : (difference > 0) - (difference < 0));
}

// The same 23 ms, a block at a time, for levels: a sixteenth of the way to
// the target each block, then exactly there.
i32 settle(i32 value, i32 target) {
    const i32 step = (target - value) / 16;
    return step ? value + step : target;
}

i32 clamp_time(i64 value) {
    return value < kShortest ? kShortest : value > kLongest ? kLongest : static_cast<i32>(value);
}

// One allpass stage on one side: the region's sample `length` back, or
// silence until the diffuser has run that long since initialize (it has
// written `diffused` samples, so the one `length` back exists from then on).
// Each stage's place and length are constants, so the series compiles to
// straight code.
template <u32 S, unsigned Side, bool Warm>
inline i32 allpass(i32* diffuser, u32 count, u32 diffused, i32 g, i32 x) {
    constexpr u32 length = Side ? kRightLength[S] : kLeftLength[S];
    constexpr u32 mask = kRegion[S] - 1;
    i32* const region = diffuser + kOffset[S];
    const i32 held = Warm || diffused >= length ? region[(count - length) & mask] : 0;
    const i32 v = add(x, -multiply(g, held));
    region[count & mask] = v;
    return add(multiply(g, v), held);
}

// One side's series: two stages at the inner gain, two at the outer, twice.
template <unsigned Side, bool Warm>
inline i32 diffuse(i32* diffuser, u32 count, u32 diffused, i32 inner, i32 outer, i32 x) {
    static_assert(kStages == 8, "the series below names each stage");
    x = allpass<0, Side, Warm>(diffuser, count, diffused, inner, x);
    x = allpass<1, Side, Warm>(diffuser, count, diffused, inner, x);
    x = allpass<2, Side, Warm>(diffuser, count, diffused, outer, x);
    x = allpass<3, Side, Warm>(diffuser, count, diffused, outer, x);
    x = allpass<4, Side, Warm>(diffuser, count, diffused, inner, x);
    x = allpass<5, Side, Warm>(diffuser, count, diffused, inner, x);
    x = allpass<6, Side, Warm>(diffuser, count, diffused, outer, x);
    return allpass<7, Side, Warm>(diffuser, count, diffused, outer, x);
}
}

void initialize(State& state) {
    // An engine reset runs this again: the lines' old contents are now unwritten.
    state = State();
}

void process(State& state, opxy::Buffer buffer, const opxy::Transport& transport, opxy::Channel left,
             opxy::Channel right, opxy::Parameters parameters) {
    if (!buffer.words || buffer.count < kBufferWords) {
        for (unsigned i = 0; i < opxy::block_frames; ++i) left[i] = right[i] = 0;
        return;
    }
    i32* const line = buffer.words;
    i32* const left_diffuser = buffer.words + kLines;
    i32* const right_diffuser = left_diffuser + kDiffuserWords;

    // Page 1: Sync, Time, Feedback, Dry. Page 2: Taps, Deviate, Curve, Diffuse.
    // The shift row, after the eight: Shift + encoder 1 Mode, + encoder 2
    // Pingpong. Dry is the stock source mixer's (machine.json's dryParameter):
    // this effect never reads it and returns only wet audio.
    const bool sync = synced(parameters[0]);
    const i64 base = (sync ? synced_samples_q12(division(parameters[1]), transport.bpm_x10)
                           : free_samples_q12(parameters[1])) >> 4;  // Q8 samples
    const i32 feedback_word = control(parameters[2]);
    const u32 taps = 1 + choice(parameters[4], kTaps);
    const u32 deviate = unit15(parameters[5]);
    const u32 curve = curve_exponent(unit15(parameters[6]));
    const i32 diffusion = control(parameters[7]);
    const u32 mode = choice(parameters[8], kModes);  // Inc, Rnd, Inc, Rnd, ...
    const bool pingpong = choice(parameters[9], 2) == 1;

    // Deviate spreads tap k up to (1 + Deviate * k/4) times Time, evenly (Inc)
    // or scattered (Rnd, each side its own), bent by Curve.
    // Only the last tap feeds back, as in most multi-tap delays: each repeat
    // brings the whole pattern again, and any number of taps sustains as long
    // as one (taps summed and fed back would need scaling by 1/n to stay
    // stable where they line up, which made them die away fast).
    i32 delay[2][kTaps], gain[kTaps], feed[kTaps];
    for (u32 k = 0; k < kTaps; ++k) {
        const i64 l = wide(static_cast<i32>(base), static_cast<i32>(spread(deviate, mode, taps, 0, k, curve))) >> 15;
        const i64 r = wide(static_cast<i32>(base), static_cast<i32>(spread(deviate, mode, taps, 1, k, curve))) >> 15;
        delay[0][k] = clamp_time(l);
        delay[1][k] = clamp_time(r);
        gain[k] = k < taps ? kLevel[taps - 1] : 0;
        feed[k] = k == taps - 1 ? kMaximum : 0;
    }
    // Short times turn the delay into a Karplus-Strong string: the repeats fed
    // back through a two-sample average, which darkens them a little each pass
    // as a plucked string does, and Feedback reaching closer to 1, curved so
    // its top sets how long the string rings (1 - (1 - Feedback)^3). Above
    // 50 ms echoes have Feedback up to 0.98 and no average. The last tap is
    // the one fed back, so its time is the string's.
    const i64 fed_time = delay[0][taps - 1];
    const i32 string = fed_time >= kStringStart ? 0 : fed_time <= kStringWhole ? kMaximum
        : static_cast<i32>(static_cast<u32>(kStringStart - fed_time) * kStringScale);
    const i32 rest = kMaximum - feedback_word;
    const i32 ringing = multiply(kMaximum - multiply(multiply(rest, rest), rest), kMostRinging);
    const i32 echo = multiply(feedback_word, kMostFeedback);
    const i32 feedback = blend(echo, ringing, string);
    const i32 cross = pingpong ? kMaximum : 0;  // settles, so switching glides
    const i32 mix = add(diffusion, diffusion);  // fully diffused from halfway
    const i32 inner = multiply(diffusion, kInner), outer = multiply(diffusion, kOuter);

    if (!state.started) {  // start at the set controls, not gliding from zero
        for (u32 k = 0; k < kTaps; ++k) {
            state.delay[0][k] = delay[0][k];
            state.delay[1][k] = delay[1][k];
            state.gain[k] = gain[k];
            state.feed[k] = feed[k];
        }
        state.feedback = feedback;
        state.cross = cross;
        state.mix = mix;
        state.inner = inner;
        state.outer = outer;
        state.damping = string;
        state.limit = kMaximum;
        state.started = 1;
    }

    // Levels settle once a block; taps that are off move straight to their
    // times, ready to come in.
    u32 active = 0;
    for (u32 k = 0; k < kTaps; ++k) {
        state.gain[k] = settle(state.gain[k], gain[k]);
        state.feed[k] = settle(state.feed[k], feed[k]);  // a new last tap crossfades in
        if (state.gain[k] || state.feed[k]) active = k + 1;
    }
    for (u32 k = active; k < kTaps; ++k) {
        state.delay[0][k] = delay[0][k];
        state.delay[1][k] = delay[1][k];
    }
    state.feedback = settle(state.feedback, feedback);
    state.cross = settle(state.cross, cross);
    state.mix = settle(state.mix, mix);
    state.inner = settle(state.inner, inner);
    state.outer = settle(state.outer, outer);
    state.damping = settle(state.damping, string);
    const i32 loop_gain = multiply(state.feedback, state.limit);
    u32 peak = 0;

    // The block runs on locals: the compiler can't tell the state from the
    // audio and lines it writes, so it would reload the state every sample.
    // Settled controls take the short path: no glide, crossing or average.
    bool gliding = false;
    for (u32 k = 0; k < active; ++k)
        if (state.delay[0][k] != delay[0][k] || state.delay[1][k] != delay[1][k]) gliding = true;
    i32 at[2][kTaps], level[kTaps], share[kTaps];
    for (u32 k = 0; k < kTaps; ++k) {
        at[0][k] = state.delay[0][k];
        at[1][k] = state.delay[1][k];
        level[k] = state.gain[k];
        share[k] = state.feed[k];
    }
    const i32 crossing = state.cross, damping = state.damping, mixing = state.mix;
    const i32 inner_g = state.inner, outer_g = state.outer;
    const bool plain = crossing == 0 && damping == 0;
    i32 last_left = state.last[0], last_right = state.last[1];
    Reader reader{line, line + kLine, state.write, state.written};
    u32 count = state.count, diffused = state.diffused;
    const bool warm = diffused >= 512;  // every stage's samples exist (each length < 512)

    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        // The taps, each side its own times, gliding like tape.
        i32 out_left = 0, out_right = 0, fed_left = 0, fed_right = 0;
        for (u32 k = 0; k < active; ++k) {
            if (__builtin_expect(gliding, 0)) {
                at[0][k] = glide(at[0][k], delay[0][k]);
                at[1][k] = glide(at[1][k], delay[1][k]);
            }
            const i32 l = reader.tap(0, at[0][k]), r = reader.tap(1, at[1][k]);
            out_left = add(out_left, multiply(l, level[k]));
            out_right = add(out_right, multiply(r, level[k]));
            if (share[k] == kMaximum) {  // settled: the one tap fed back
                fed_left = l;
                fed_right = r;
            } else if (share[k]) {  // crossfading to a new last tap
                fed_left = add(fed_left, multiply(l, share[k]));
                fed_right = add(fed_right, multiply(r, share[k]));
            }
        }

        // What feeds back: the last tap, crossed over when Pingpong is on,
        // averaged with the sample before by the Karplus-Strong share. The
        // input at half level, as Ping Pong's: each side its own, or with
        // Pingpong on both sides together into the left only, so it bounces.
        const i32 half_left = opxy::q31::half(left[i]), half_right = opxy::q31::half(right[i]);
        i32 string_left = fed_left, string_right = fed_right, dry_left = half_left, dry_right = half_right;
        if (!plain) {
            const i32 back_left = blend(fed_left, fed_right, crossing);
            const i32 back_right = blend(fed_right, fed_left, crossing);
            string_left = blend(back_left, add(opxy::q31::half(back_left), opxy::q31::half(last_left)), damping);
            string_right = blend(back_right, add(opxy::q31::half(back_right), opxy::q31::half(last_right)), damping);
            last_left = back_left;
            last_right = back_right;
            const i32 both = add(opxy::q31::half(half_left), opxy::q31::half(half_right));
            dry_left = blend(half_left, both, crossing);
            dry_right = multiply(half_right, kMaximum - crossing);
        } else {
            last_left = fed_left;
            last_right = fed_right;
        }
        const i32 repeat_left = multiply(string_left, loop_gain), repeat_right = multiply(string_right, loop_gain);
        const u32 size_left = repeat_left < 0 ? 0u - static_cast<u32>(repeat_left) : static_cast<u32>(repeat_left);
        const u32 size_right = repeat_right < 0 ? 0u - static_cast<u32>(repeat_right) : static_cast<u32>(repeat_right);
        const u32 size = size_left > size_right ? size_left : size_right;
        if (size > peak) peak = size;
        i32 in_left = add(dry_left, repeat_left);
        i32 in_right = add(dry_right, repeat_right);

        // Diffuse: the allpass series, blended in by Diffuse.
        if (mixing) {
            i32 l, r;
            if (__builtin_expect(warm, 1)) {
                l = diffuse<0, true>(left_diffuser, count, diffused, inner_g, outer_g, in_left);
                r = diffuse<1, true>(right_diffuser, count, diffused, inner_g, outer_g, in_right);
            } else {
                l = diffuse<0, false>(left_diffuser, count, diffused, inner_g, outer_g, in_left);
                r = diffuse<1, false>(right_diffuser, count, diffused, inner_g, outer_g, in_right);
            }
            in_left = blend(in_left, l, mixing);
            in_right = blend(in_right, r, mixing);
            ++count;
            if (diffused < 512) ++diffused;
        }

        reader.store(in_left, in_right);
        left[i] = out_left;
        right[i] = out_right;
    }

    for (u32 k = 0; k < kTaps; ++k) {
        state.delay[0][k] = at[0][k];
        state.delay[1][k] = at[1][k];
    }
    state.last[0] = last_left;
    state.last[1] = last_right;
    state.write = reader.write;
    state.written = reader.written;
    state.count = count;
    state.diffused = diffused;

    // The limiter, once a block: down at once to what holds this block's
    // loudest repeat at the ceiling, back up over about 0.2 s.
    i32 target = kMaximum;
    if (peak > static_cast<u32>(kCeiling)) {
        u32 ratio = (static_cast<u32>(kCeiling) >> 1) / (peak >> 16);  // Q15: 0.5 .. 1
        if (ratio > 32767) ratio = 32767;
        target = multiply(state.limit, static_cast<i32>(ratio << 16));
    }
    if (target < state.limit) state.limit = target;
    else state.limit += (target - state.limit) / 128 ? (target - state.limit) / 128 : target - state.limit;
}
}
