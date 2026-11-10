#pragma once
#include <opxy/page_header.hpp>

namespace comet {
// Matches resources.display in definition.json: paths, points, nodes, texts, palette.
// Nodes 0..7 are the header's bars, 8..17 the tap strips, 18..22 the time grid,
// 23..122 the tap markers (ten per strip: the marker and its smear),
// 123..124 the timeline's last two grid lines. Texts 0..15 are the
// header's, 16..20 the tap numbers, 21..27 the time scale.
using Frame = opxy::draw::Frame<0, 0, 125, 28, 16>;
struct DisplayState {
    opxy::draw::PageHeader header;
    opxy::i32 at[2][5];        // each marker's time, gliding to its tap, in the timeline's units
    opxy::i32 trail[5][2][5];  // the markers' last five positions, newest first
    opxy::u32 started, track;
    opxy::u32 synced;          // whether the timeline is in note lengths
};
}
