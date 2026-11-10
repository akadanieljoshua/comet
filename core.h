#pragma once
#include <opxy/effect.hpp>
#include "timing.h"

// Comet: a five-tap stereo delay with a diffuser in its feedback loop, after
// Sabroi's AS D-Delay, built on the core Ping Pong (Open-XY contributors, MIT):
// its tap reading, its unwritten-sample check and its glide.
namespace comet {
const u32 kTaps = 5;

// The diffuser: two series of four allpasses per side from D-Delay's input
// diffusion (77/111/199/326, 101/122/222/333 and 107/142/277/379 samples; it
// runs three). The left side runs series A then C, the right B then A, so the
// sides decorrelate. Each stage has a power-of-two region per side, so one
// shared counter and a mask address them all.
const u32 kStages = 8;
const u32 kLeftLength[kStages] = {77, 111, 199, 326, 107, 142, 277, 379};
const u32 kRightLength[kStages] = {101, 122, 222, 333, 77, 111, 199, 326};
const u32 kRegion[kStages] = {128, 128, 256, 512, 128, 256, 512, 512};
const u32 kOffset[kStages] = {0, 128, 256, 512, 1024, 1152, 1408, 1920};
const u32 kDiffuserWords = 2432;  // one side's: the sum of kRegion

// The send buffer, the whole 512 KiB (proven on the OP-XY), then the left and
// right diffusers, Q31: two lines of Q31 samples, 63,104 frames each (1.43 s).
const u32 kBufferWords = 131072;  // definition.json: bufferBytes 524288
const u32 kLine = (kBufferWords - 2 * kDiffuserWords) / 2;  // 63,104 frames, 1.43 s
const u32 kLines = 2 * kLine;  // words the lines take

struct State {
    u32 write;     // the next frame of the line
    u32 written;   // positions written since initialize, up to kLine
    u32 count;     // the diffuser's shared position
    u32 diffused;  // samples the diffuser has run since initialize, up to 512
    u32 started;   // 1 once the controls have been read
    i32 delay[2][kTaps];                     // tap times, smoothed, Q8 samples
    i32 gain[kTaps];                         // tap levels, smoothed, Q31
    i32 feedback, cross, mix, inner, outer;  // smoothed, Q31
    i32 damping;                             // the Karplus-Strong share, smoothed, Q31
    i32 last[2];                             // the repeats fed back a sample ago, per side
};
void initialize(State& state);
void process(State& state, opxy::Buffer buffer, const opxy::Transport& transport, opxy::Channel left,
             opxy::Channel right, opxy::Parameters parameters);
}
