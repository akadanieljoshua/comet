#pragma once
#include <opxy/types.hpp>

// Free Time's range. The lines keep 32-bit
// samples, as Ping Pong does (proven on the OP-XY): two lines of 63,104 frames
// in the 512 KiB, 1.43 s, so free Time runs 0.2 ms to 1.4 s. (A 16-bit build
// that reached 2.5 s, Comet Long, sounded distorted on the device.)
namespace comet {
const opxy::u32 kFreeOctaves = 837100;  // log2(7000) in Q16: 0.2 ms * 7000 = 1.4 s
}
