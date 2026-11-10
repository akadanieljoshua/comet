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
const i32 kMostFeedback = 2040109466;  // 0.95, Ping Pong's, for echoes
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
// The rest of 1/n for the repeats fed back, so n taps in phase return no more
// than Feedback.
const i32 kLoop[kTaps] = {kMaximum, 1385612784, 1072335162, 894033717, 776416667};

i32 add(i32 a, i32 b) {  // saturating
    if (b > 0 && a > kMaximum - b) return kMaximum;
    if (b < 0 && a < kMinimum - b) return kMinimum;
    return a + b;
}

i32 control(i32 word) { return word < 0 ? 0 : word; }

// a and b blended, b's share Q31.
i32 blend(i32 a, i32 b, i32 share) { return add(multiply(a, kMaximum - share), multiply(b, share)); }

// One side's sample `age` frames back (1 is the last written), as Q31, or
// silence where this effect has not written since initialize: the buffer is
// never cleared.
i32 sample(const i32* lines, const State& state, u32 age, unsigned side) {
    if (age > state.written) return 0;
    const u32 at = state.write >= age ? state.write - age : state.write + kLine - age;
    return lines[side * kLine + at];
}

// A frame written to the lines.
void store(i32* lines, const State& state, i32 left, i32 right) {
    lines[state.write] = left;
    lines[kLine + state.write] = right;
}

// One side of the line `delay` (Q8) samples back, between its two nearest samples.
i32 tap(const i32* line, const State& state, i32 delay, unsigned side) {
    const u32 whole = static_cast<u32>(delay) >> 8, part = static_cast<u32>(delay) & 255;
    const i32 near = sample(line, state, whole, side), far = sample(line, state, whole + 1, side);
    return near / 256 * static_cast<i32>(256 - part) + far / 256 * static_cast<i32>(part);
}

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
i32 allpass(i32* region, u32 mask, u32 length, const State& state, i32 g, i32 x) {
    const i32 held = state.diffused >= length ? region[(state.count - length) & mask] : 0;
    const i32 v = add(x, -multiply(g, held));
    region[state.count & mask] = v;
    return add(multiply(g, v), held);
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
    i32 delay[2][kTaps], gain[kTaps];
    for (u32 k = 0; k < kTaps; ++k) {
        const i64 l = wide(static_cast<i32>(base), static_cast<i32>(spread(deviate, mode, taps, 0, k, curve))) >> 15;
        const i64 r = wide(static_cast<i32>(base), static_cast<i32>(spread(deviate, mode, taps, 1, k, curve))) >> 15;
        delay[0][k] = clamp_time(l);
        delay[1][k] = clamp_time(r);
        gain[k] = k < taps ? kLevel[taps - 1] : 0;
    }
    // Short times turn the delay into a Karplus-Strong string: the repeats fed
    // back through a two-sample average, which darkens them a little each pass
    // as a plucked string does, and Feedback reaching closer to 1, curved so
    // its top sets how long the string rings (1 - (1 - Feedback)^3). Above
    // 50 ms echoes are as Ping Pong's: Feedback up to 0.95 and no average.
    const i64 first_time = delay[0][0];
    const i32 string = first_time >= kStringStart ? 0 : first_time <= kStringWhole ? kMaximum
        : static_cast<i32>(static_cast<u32>(kStringStart - first_time) * kStringScale);
    const i32 rest = kMaximum - feedback_word;
    const i32 ringing = multiply(kMaximum - multiply(multiply(rest, rest), rest), kMostRinging);
    const i32 echo = multiply(feedback_word, kMostFeedback);
    const i32 feedback = multiply(blend(echo, ringing, string), kLoop[taps - 1]);
    const i32 cross = pingpong ? kMaximum : 0;  // settles, so switching glides
    const i32 mix = add(diffusion, diffusion);  // fully diffused from halfway
    const i32 inner = multiply(diffusion, kInner), outer = multiply(diffusion, kOuter);

    if (!state.started) {  // start at the set controls, not gliding from zero
        for (u32 k = 0; k < kTaps; ++k) {
            state.delay[0][k] = delay[0][k];
            state.delay[1][k] = delay[1][k];
            state.gain[k] = gain[k];
        }
        state.feedback = feedback;
        state.cross = cross;
        state.mix = mix;
        state.inner = inner;
        state.outer = outer;
        state.damping = string;
        state.started = 1;
    }

    // Levels settle once a block; taps that are off move straight to their
    // times, ready to come in.
    u32 active = 0;
    for (u32 k = 0; k < kTaps; ++k) {
        state.gain[k] = settle(state.gain[k], gain[k]);
        if (state.gain[k]) active = k + 1;
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

    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        // The taps, each side its own times, gliding like tape.
        i32 out_left = 0, out_right = 0;
        for (u32 k = 0; k < active; ++k) {
            state.delay[0][k] = glide(state.delay[0][k], delay[0][k]);
            state.delay[1][k] = glide(state.delay[1][k], delay[1][k]);
            out_left = add(out_left, multiply(tap(line, state, state.delay[0][k], 0), state.gain[k]));
            out_right = add(out_right, multiply(tap(line, state, state.delay[1][k], 1), state.gain[k]));
        }

        // What feeds back: the repeats, crossed over when Pingpong is on,
        // averaged with the sample before by the Karplus-Strong share.
        const i32 back_left = blend(out_left, out_right, state.cross);
        const i32 back_right = blend(out_right, out_left, state.cross);
        const i32 string_left = blend(back_left, add(opxy::q31::half(back_left), opxy::q31::half(state.last[0])), state.damping);
        const i32 string_right = blend(back_right, add(opxy::q31::half(back_right), opxy::q31::half(state.last[1])), state.damping);
        state.last[0] = back_left;
        state.last[1] = back_right;
        // The input at half level, as Ping Pong's: each side its own, or with
        // Pingpong on both sides together into the left only, so it bounces.
        const i32 half_left = opxy::q31::half(left[i]), half_right = opxy::q31::half(right[i]);
        const i32 both = add(opxy::q31::half(half_left), opxy::q31::half(half_right));
        i32 in_left = add(blend(half_left, both, state.cross), multiply(string_left, state.feedback));
        i32 in_right = add(multiply(half_right, kMaximum - state.cross), multiply(string_right, state.feedback));

        // Diffuse: the allpass series, blended in by Diffuse.
        if (state.mix) {
            i32 l = in_left, r = in_right;
            for (u32 s = 0; s < kStages; ++s) {
                const i32 g = s & 2 ? state.outer : state.inner;
                const u32 mask = kRegion[s] - 1;
                l = allpass(left_diffuser + kOffset[s], mask, kLeftLength[s], state, g, l);
                r = allpass(right_diffuser + kOffset[s], mask, kRightLength[s], state, g, r);
            }
            in_left = blend(in_left, l, state.mix);
            in_right = blend(in_right, r, state.mix);
            ++state.count;
            if (state.diffused < 512) ++state.diffused;
        }

        store(line, state, in_left, in_right);
        state.write = state.write + 1 == kLine ? 0 : state.write + 1;
        if (state.written < kLine) ++state.written;
        left[i] = out_left;
        right[i] = out_right;
    }
}
}
