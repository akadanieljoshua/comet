#pragma once
#include <opxy/types.hpp>
#include "range.h"

// Shared by the audio (fx.cpp) and the page (display.cpp): what Sync and Time
// mean, so the header always names the time the effect plays.
namespace comet {
using opxy::i32;
using opxy::i64;
using opxy::u32;

const u32 kDivisions = 13;
// AS D-Delay's note lengths from 1/32 to 1/2, in 96ths of a quarter note.
const u32 kDivisionTicks[kDivisions] = {12, 16, 18, 24, 32, 36, 48, 64, 72, 96, 128, 144, 192};
const char* const kDivisionNames[kDivisions] = {"1/32", "1/16T", "1/32D", "1/16", "1/8T", "1/16D", "1/8",
                                                "1/4T", "1/8D",  "1/4",   "1/2T", "1/4D", "1/2"};

// A control word as 0..32767.
inline u32 unit15(i32 word) { return word < 0 ? 0u : static_cast<u32>(word) >> 16; }

// An enum control's choice, as the stock Q31-to-Q15 path quantizes it.
inline u32 choice(i32 word, u32 count) {
    const u32 c = (unit15(word) * count) >> 15;
    return c < count ? c : count - 1;
}

inline bool synced(i32 sync_word) { return choice(sync_word, 2) == 1; }

// Time's note length while Sync is on: an equal share of its travel each.
inline u32 division(i32 time_word) { return choice(time_word, kDivisions); }

// 2^(i/16) in Q16: a namespace-scope table, not a static inside a function.
const u32 kExp2[17] = {65536,  68438,  71468,  74632,  77936,  81386,  84990,  88752, 92682,
                       96785,  101070, 105545, 110218, 115098, 120194, 125515, 131072};

// 2^(e / 65536) in Q16, for e in [0, 15 * 65536): the table, interpolated.
inline u32 exp2_q16(u32 e) {
    const u32* const table = kExp2;
    const u32 octave = e >> 16, step = (e >> 12) & 15, part = e & 4095;
    const u32 mantissa = table[step] + (((table[step + 1] - table[step]) * part) >> 12);
    return mantissa << octave;
}

// Time while Sync is off rises evenly in pitch from 0.2 ms, over range.h's
// kFreeOctaves: to 1.4 s in Comet, 2.5 s in Comet Long.
inline u32 free_power(i32 time_word) { return exp2_q16(static_cast<u32>((static_cast<i64>(unit15(time_word)) * kFreeOctaves) >> 15)); }

// The free time in hundredths of a millisecond, from 20, for the header.
inline u32 free_centi_ms(i32 time_word) { return static_cast<u32>((static_cast<i64>(free_power(time_word)) * 20 + 32768) >> 16); }

// The free time in Q12 samples at 44.1 kHz, from 8.82 samples (0.2 ms).
inline u32 free_samples_q12(i32 time_word) { return static_cast<u32>((static_cast<i64>(free_power(time_word)) * 36127) >> 16); }

// Mode's choices: Inc, Rnd, Inc, Rnd, ... kModes in all, so each turn on to
// a further Rnd brings a new draw, as D-Delay's Randomize button does, and
// turning back returns to the draws passed. Choice m is Rnd when odd, with
// draw m / 2. The choice is saved with the project, and the page reads the
// same choice, so it always shows the taps the audio plays.
const u32 kDraws = 128, kModes = 2 * kDraws;  // 256: the most a shift control may have
inline bool is_random(u32 mode) { return (mode & 1) != 0; }

// Rnd's draws: D-Delay's Rnd multiplies each tap's share of the spread by a
// random 0..1 in hundredths (its random 101), drawn apart for left and right.
// These are 128 such draws for taps 2..5, every one drawn at random, then fixed
// so a saved project sounds the same. Only draws that reach past .60 on each
// side and differ between the sides were kept, so no Rnd bunches all its taps
// near Time or puts both sides together.
const unsigned char kDraw[kDraws][2][4] = {
    {{ 38,  82,  22,  72}, { 91,  92,  60,  93}},
    {{ 68,  69,  51,  68}, { 17,  45,  89,  75}},
    {{ 57,  93,  51,  93}, { 28,  22,  72,  87}},
    {{ 63,  69,  26,  19}, { 20,  26,  33,  94}},
    {{ 96,  49,  83,  30}, { 60,  23,  11,  27}},
    {{ 69,  59,  55,  10}, { 79,  99,  16,   3}},
    {{ 79,  40,   3,  57}, { 25,  96,  31,  97}},
    {{ 67,  82,  44,   7}, {  5,   1,  59,  86}},
    {{ 60,  71,  88,  29}, { 23,  29,  42,  91}},
    {{ 87,  96,  85,  28}, { 89,  31,  80,  41}},
    {{ 12,  10,  48,  76}, { 87,  24,  54,  74}},
    {{ 15,  27, 100,  23}, { 87,  94,  88,  77}},
    {{ 23,  48,  64,   2}, { 29,   4,  68,  46}},
    {{  9,  45,  40,  62}, { 88,  75,  54,  53}},
    {{ 83,  16,  99,  51}, { 32,  67,  11,  29}},
    {{ 39,  67,  97,  88}, { 16,  67,  85,  36}},
    {{ 95,  13,  50,  78}, { 64,  39,  70,  58}},
    {{ 81,   9,   2,   0}, { 62,  98,  95,  99}},
    {{ 49,  97,  31,  47}, { 93,  87,   7,  95}},
    {{ 55,  55,  85,  96}, { 20,  15,  74,  30}},
    {{ 66,  77,  24,  49}, { 76,  50,  82,  51}},
    {{ 35,  15,  67,  83}, { 95,  37,  14,   7}},
    {{ 92,  80,  29,   3}, { 54,  94,  68,  57}},
    {{ 52,  41,  60,  75}, { 66,  64,  24,  41}},
    {{ 96,  62,  81,  15}, { 81,  76,  90,  73}},
    {{ 80,  92,  54,  91}, { 23,  52,  68,  87}},
    {{ 98,  17,  72,  22}, { 19,  44,  76,  22}},
    {{100,  43,   7,  12}, { 27,  71,  66,  55}},
    {{ 47,  90,  71,  10}, { 13,  21,  71,  31}},
    {{ 41,  36,  88,  45}, { 78,  63,  65,   1}},
    {{  4,  16,  15,  85}, { 17,  50,  72,  17}},
    {{ 69,  98,  50,  82}, { 38,  62,  21,  17}},
    {{ 89,  49,  54,  39}, { 62,   1,  25,  74}},
    {{  0,  74,  72,  11}, { 62,  22,  90,  19}},
    {{  9,  20,  52,  75}, { 13,  62,  76,   2}},
    {{ 16,  45,  33,  85}, { 43,  65,   5,  34}},
    {{ 76,  42,  55,  20}, { 12,   2,  80,   5}},
    {{ 93,  63,  46,  27}, { 95,  40,  83,  55}},
    {{  4,  63,  13,  74}, { 75,  50,  41,  47}},
    {{ 62,  63,  58,  26}, { 18,  72,  98,  81}},
    {{ 94,   9,  39,  68}, { 45,  12,  67,  21}},
    {{ 68,   8,  53,  62}, { 44,  30,  20,  76}},
    {{ 47,  46,  86,  19}, { 19,  82,  98,  33}},
    {{ 85,  27,  47,  72}, { 88,  80,  10,  30}},
    {{ 50,  55,  38,  61}, {  3,  76,  13,  27}},
    {{ 96,   5,  60,  15}, {  7,  41,  99,  63}},
    {{  8,  55,  42,  81}, { 11,  26, 100,  20}},
    {{ 90,  21,  10,   2}, { 50,  59,  56, 100}},
    {{ 90,  93,   8,  60}, {  5,   9,  69,  62}},
    {{ 42,  57,  19,  67}, { 16,  11,  62,  26}},
    {{ 64,  19,  57,  76}, { 83,  62,  48,  63}},
    {{ 64,  75,  93,  44}, { 76,  24,  36,  46}},
    {{ 18,  95,  21,  99}, { 70,  67,  61,  15}},
    {{ 64,  90,  95,  10}, { 10,  64,  47,  45}},
    {{ 11,   3,  88,  76}, { 47,  99,   7,   9}},
    {{ 42,  84,  84,   8}, { 82,  25,  42,  38}},
    {{ 38,  21,  77,  24}, { 16,  80,   9,  84}},
    {{ 36,  70,  62,  98}, { 51,  90,  56,  18}},
    {{ 37,  61,  28,   7}, { 28,  60,  74,  44}},
    {{  2,  95,   0,  33}, {  7,  78,  77,  68}},
    {{ 36,  85,  92,  62}, {100,  66,  74,   0}},
    {{ 57,  15,  86,   2}, { 42,  30,  94,  76}},
    {{ 59,  72,  70,  56}, { 33,  12,  57,  70}},
    {{ 32,  37,  51,  91}, { 83,  95,  75,  14}},
    {{ 39,  12,  51,  99}, { 92,  23,  79,  21}},
    {{ 85,  25,  61,  81}, { 95,  45,  22,  15}},
    {{ 52,  72,  95,  85}, { 95,  88,  95,  26}},
    {{ 33,  97,  10,  20}, { 17,  79,  50,   7}},
    {{ 18,  79,  14,  96}, {100,  56,  27,   3}},
    {{ 46,  78,  18,  40}, {  6,  19,  82,  92}},
    {{ 43,  85,  14,  76}, { 99,  92,   8,  34}},
    {{ 35,  25,  94,  11}, { 53,  73,  35,  41}},
    {{100,  30,  83,  50}, { 55,  76,  99,  89}},
    {{100,  37,  68,  73}, { 77,  25,   1,  58}},
    {{ 34,  23,  61,  53}, { 31,  65,  13,  88}},
    {{ 25,  29,   6,  80}, { 88,  20,  49,  15}},
    {{ 95,  67,  25,  72}, { 79,  15,  85,  13}},
    {{100,  46,  53,  87}, { 17,  39,  94,  20}},
    {{ 48,  73,  34,  35}, { 85,  34,  38,  16}},
    {{ 76,  50,  40,  78}, { 98,   7,  31,  84}},
    {{ 15,  40,  16,  89}, { 29,  65,  52,  57}},
    {{ 27,  66,  48,   4}, { 65,  54,  40,  26}},
    {{ 77,  24,  38,   7}, {  6,  83,  93,  82}},
    {{ 88,  35,  83,  35}, { 20,  42,  74,  87}},
    {{ 63,  65,  17,  91}, { 65,  86,  89,  80}},
    {{ 46,  61,   5,   5}, { 15,  42,  58,  66}},
    {{  7,  46,  41,  85}, { 51,  82,  44,  50}},
    {{ 63,  86,  94,  60}, {  3,  79,  37,   9}},
    {{  1,  84,  80,  77}, { 18,  43,  45,  76}},
    {{ 53,  62,  40,  80}, { 99,  89,  69,  28}},
    {{ 13,  93,  22,  63}, { 93,  63,  43,  61}},
    {{ 89,  47,  32,  11}, { 80,  68,  59,  86}},
    {{  0,  98,  99,  85}, { 73,  65,  48,  85}},
    {{ 35,  52,  98,  28}, { 79,  43,  37,  85}},
    {{ 23,  49,  51,  64}, { 74,  52,  69,  34}},
    {{ 90,  59,  80,  68}, { 67,  29,   4,  69}},
    {{ 69,  52,  48,  94}, { 79,  74,  74,  15}},
    {{ 60,  24,   3,  76}, { 68,   4,  60,  31}},
    {{ 21,   4,  97,  40}, { 84,  59,  59,  89}},
    {{ 56,  52,  96,  18}, { 51,  40,  13,  85}},
    {{ 75,  71,   5,  81}, { 82,  85,  19,  23}},
    {{ 99,  79,  80,  84}, { 87,  23,  24,   1}},
    {{ 28,   2,  41,  96}, { 53,  89,  31,  55}},
    {{ 91,  28,  15,  30}, {  9,  47,  99,  66}},
    {{ 38,  42,  63,  72}, { 63,  32,  41,  35}},
    {{ 28,  84,  63,  74}, { 85,  92,  51,  14}},
    {{ 93,  62,  75,  56}, { 17,  88,  23,  47}},
    {{ 73,  74,  30,   8}, { 12,  52,  30,  99}},
    {{  0,  73,  28,  43}, { 32,  83,  16,  94}},
    {{ 47,  85,  72,  22}, { 70,  37,  49,  68}},
    {{ 61,  77,  69,   4}, { 41,  52,  68,  70}},
    {{ 68,  87,  54,  84}, { 84,  87,  28,  18}},
    {{ 87,  17,   6,  84}, { 13,  24,  81,  90}},
    {{ 29,  61,  46,   5}, { 44,  57,  94,  28}},
    {{ 93,  20,  40,  92}, { 38,  95,  48,  57}},
    {{ 29,  69,  21,  71}, { 62,  33,  32,  49}},
    {{ 60,  17,  76,  70}, { 55,  68,  32,  25}},
    {{ 23,  80,  70,  65}, { 23,  41,  91,  15}},
    {{ 99,   6,  41,  84}, { 28,  59,  56,  89}},
    {{ 73,  71,   2,  74}, { 36,  92,   0,  48}},
    {{ 61,  51,  77,  84}, { 20,   3,  82,  74}},
    {{ 88,  65,   8,  92}, { 90,  23,  94,  24}},
    {{ 22,   8,  92,  14}, { 95,  68,  21,  84}},
    {{ 91,   9,  25,  32}, { 61,  76,   6,  15}},
    {{100,  86,  12,  69}, { 79,  84,  48,  14}},
    {{ 98,  62,  16,  78}, { 63,  86,  85,  83}},
    {{ 44,  69,  39,  71}, { 98,  11,   4,  39}},
    {{ 90,  20,  28,  11}, { 25,  57,  96,  82}}
};
// Tap k's draw on one side for this Mode, Q15 (1.00 for Inc).
inline u32 scatter(u32 mode, u32 side, u32 k) {
    if (!is_random(mode) || k == 0) return 32768;
    const u32 draw = (mode >> 1) % kDraws;
    return (static_cast<u32>(kDraw[draw][side & 1][(k < 5 ? k : 4) - 1]) * 32768 + 50) / 100;
}

// log2(1 + i/16) in Q16.
const u32 kLog2[17] = {0,     5732,  11136, 16248, 21098, 25711, 30109, 34312, 38336,
                       42196, 45904, 49472, 52911, 56229, 59434, 62534, 65536};

// -log2(x / 32768) in Q16, for x in 1..32768.
inline u32 octaves_below(u32 x) {
    u32 octave = 0;
    while (x < 16384) {
        x <<= 1;
        ++octave;
    }
    // x in [16384, 32768): its log2 within the octave, from the table.
    const u32* const table = kLog2;
    const u32 m = x - 16384, step = m >> 10, part = m & 1023;
    const u32 within = table[step] + (((table[step + 1] - table[step]) * part) >> 10);
    return ((octave + 1) << 16) - within;
}

// D-Delay's Curve, 0..32767: a tap's place in the spread is raised to an
// exponent from 1/8 to 4, D-Delay's scale: 1/8 + 31/8 * Curve^2.12655, so the
// knob's travel gathers near even. 50 keeps the spread even (D-Delay's is even
// a hair below 50, at 49.6: 1.01 at 50); below it the taps crowd late, so the
// gaps shrink; above it they crowd early, so the gaps grow. The exponent in Q16.
const u32 kEven = 65536;
inline u32 curve_exponent(u32 curve) {
    if (curve + 64 >= 16384 && curve <= 16384 + 64) return kEven;
    if (curve == 0) return 8192;
    // Curve^2.12655 in Q15: 2 to the -(2.12655 * octaves below 1).
    const i64 x = (static_cast<i64>(octaves_below(curve)) * 139366) >> 16;  // 2.12655 in Q16
    const u32 power = x >= 15 * 65536 ? 0 : exp2_q16(static_cast<u32>(15 * 65536 - x)) >> 16;
    return 8192 + static_cast<u32>((static_cast<i64>(power) * 253952) >> 15);  // 1/8 + 31/8 * power, Q16
}

inline u32 curved(u32 place, u32 exponent) {
    if (place == 0 || exponent == kEven) return place;
    if (place >= 32768) return 32768;
    const i64 x = static_cast<i64>(octaves_below(place)) * exponent >> 16;  // Q16 octaves
    if (x >= 15 * 65536) return 0;
    return exp2_q16(static_cast<u32>(15 * 65536 - x)) >> 16;  // 2^-x in Q15
}

// Tap k's time as a multiple of Time, Q15: 32768 .. 65535, as D-Delay sets it:
// 1 + Deviate * (k/4)^Curve, times Rnd's draw with Rnd. Inc spreads the taps
// evenly at Curve 50; Rnd moves each one somewhere between Time and that.
// `mode` is Mode's choice (0 Inc, 1 the first Rnd draw, ...); `taps` is unused.
inline u32 spread(u32 deviate, u32 mode, u32 taps, u32 side, u32 k, u32 exponent = kEven) {
    (void)taps;
    if (k == 0) return 32768;
    const u32 share = (curved(k << 13, exponent) * scatter(mode, side, k)) >> 15;  // Q15
    return 32768 + ((deviate * share) >> 15);
}

// A note length in Q12 samples at the tempo (tenths of BPM; none plays as 120).
// 44100 * 60 * 10 / 96 = 275,625 samples per tick at one tenth of a BPM.
inline u32 synced_samples_q12(u32 index, u32 bpm_x10) {
    const u32 bpm = bpm_x10 ? bpm_x10 : 1200;
    const u32 length = kDivisionTicks[index] * 275625u;
    return ((length / bpm) << 12) + (((length % bpm) << 12) / bpm);
}
}
