// Checks for Comet's header-only page, compiled for the host and run by
// python sdk.py test --machine <folder>.
#include "../display.cpp"
#include <cmath>
#include <cstdio>
#include <cstring>

static int failures = 0;
static void check(const char* what, bool ok) {
    if (!ok) { std::printf("FAIL %s\n", what); ++failures; }
}
static opxy::i32 amount(double x) { return static_cast<opxy::i32>(x * 2147483647.0); }
static opxy::i32 pick(unsigned index, unsigned count) {
    return static_cast<opxy::i32>((index * 2 + 1) * (2147483648.0 / (2 * count)));
}

static comet::DisplayState state;
static comet::Frame frame;
static opxy::draw::Input input;
static void show(const opxy::i32* slots, unsigned page) {
    for (unsigned i = 0; i < 8; ++i) input.slots[i] = slots[i];
    for (unsigned i = 0; i < 4; ++i) {
        input.shown[i] = static_cast<opxy::u8>(page ? 4 + i : i);
        input.parameters[i] = slots[input.shown[i]];
    }
    comet::display(state, input, frame);
}
// Page 1's titles are text slots 0..3, its values 4..7; page 2's 8..11 and 12..15.
static bool says(unsigned slot, const char* text) { return std::strcmp(frame.texts[slot].chars, text) == 0; }

int main() {
    comet::display_reset(state);
    input.track = 14;
    // Page 1: Sync, Time, Feedback, Dry. Page 2: Taps, Deviate, Curve, Diffuse.
    opxy::i32 s[8] = {pick(1, 2), pick(9, comet::kDivisions), amount(0.4), amount(1), 0, amount(0.5), amount(0.5), 0};
    show(s, 0);
    check("every frame is valid", opxy::draw::valid(frame));
    check("page 1 reads Sync, Time, Feedback, Dry",
          says(0, "sync") && says(1, "time") && says(2, "feedback") && says(3, "dry"));
    check("page 2 reads Taps, Deviate, Curve, Diffuse",
          says(8, "taps") && says(9, "deviate") && says(10, "curve") && says(11, "diffuse"));
    check("Sync on: Time reads its note length", says(4, "on") && says(5, "1/4"));
    check("Dry reads 100 by default", says(7, "100"));
    bool names = true;
    for (unsigned i = 0; i < comet::kDivisions; ++i) {
        s[1] = pick(i, comet::kDivisions);
        show(s, 0);
        names = names && says(5, comet::kDivisionNames[i]) && says(1, "time");
    }
    check("every note length from 1/32 to 1/2 reads by name", names && says(5, "1/2"));
    s[0] = pick(0, 2);
    const struct { double ms; const char* value; const char* title; } times[] = {
        {0.2, "0.2", "time ms"}, {4.4, "4.4", "time ms"}, {25, "25", "time ms"}, {236, "236", "time ms"},
        {999, "999", "time ms"}, {1400, "1.40", "time s"}};
    const double octaves = comet::kFreeOctaves / 65536.0, longest = 0.2 * std::pow(2.0, octaves);
    for (const auto& t : times) {
        if (t.ms > longest + 1) continue;
        s[1] = amount(std::fmin(1.0, std::log2(t.ms / 0.2) / octaves + 1e-6));
        show(s, 0);
        char what[128];
        std::snprintf(what, sizeof(what), "Sync off: %.1f ms reads %s %s (got %s %s)", t.ms, t.title, t.value,
                      frame.texts[1].chars, frame.texts[5].chars);
        check(what, says(5, t.value) && says(1, t.title));
    }
    check("Sync off reads off", says(4, "off"));
    show(s, 1);
    check("one tap: Deviate and Curve read off", says(12, "1") && says(13, "off") && says(14, "off"));
    s[4] = pick(2, 5);
    show(s, 1);
    check("three taps: Deviate and Curve read their amounts", says(12, "3") && says(13, "50") && says(14, "50"));
    check("still a valid frame", opxy::draw::valid(frame));
    // Shift: Mode and Pingpong in the edited row's first two columns.
    input.shift = 1;
    input.line = 1;
    input.choices[0] = 1;
    input.choices[1] = 0;
    show(s, 1);
    check("Shift on page 2 shows Mode rnd and Pingpong off",
          says(8, "mode") && says(12, "rnd") && says(9, "pingpong") && says(13, "off"));
    input.choices[0] = 0;
    input.choices[1] = 1;
    input.line = 0;
    show(s, 0);
    check("Shift on page 1 shows Mode inc and Pingpong on",
          says(0, "mode") && says(4, "inc") && says(1, "pingpong") && says(5, "on"));
    s[4] = pick(0, 5);
    show(s, 0);
    check("Shift with one tap: Mode reads off", says(4, "off") && says(5, "on"));
    check("a valid frame with Shift", opxy::draw::valid(frame));
    input.shift = 0;
    show(s, 0);
    check("without Shift the columns are Sync and Time again", says(0, "sync") && says(1, "time s"));

    // The taps, as AS D-Delay draws them. Sync on, 1/4, five taps, Deviate
    // full, Curve even, Inc: each strip's marker at 1/4, 5/16 .. 1/2 of a whole
    // note, on the fixed timeline: 2.5 s, five beats at 120 BPM, 400 px from x = 60.
    comet::display_reset(state);
    input.choices[0] = 0;
    input.choices[1] = 0;
    input.elapsed = 34;
    opxy::i32 t[8] = {pick(1, 2), pick(9, comet::kDivisions), amount(0.4), amount(1), pick(4, 5), amount(1), amount(0.5), 0};
    show(t, 0);
    const opxy::u32 marker = 23, smear = 10;
    // j counts back along the smear from its head; the artwork draws a strip's
    // markers tail first, so the head is its last node.
    auto node = [&](unsigned k, unsigned side, unsigned j) -> const opxy::draw::Node& {
        return frame.nodes[marker + (k * 2 + side) * smear + (smear - 1 - j)];
    };
    auto left = [&](unsigned k, unsigned side, unsigned j) { return (node(k, side, j).x + 2048) / 1024.0; };
    auto at = [](double wholes) { return 60 + 400 * std::fmin(wholes / 1.25, 1.0); };  // a synced time: 2.5 s is 1 1/4 whole notes at 120 BPM
    bool even = true;
    for (unsigned k = 0; k < 5; ++k)
        for (unsigned side = 0; side < 2; ++side) even = even && std::fabs(left(k, side, 0) - at(0.25 * (1 + k / 4.0))) < 0.6;
    check("Sync 1/4, five taps, Deviate full: the markers sit at 1/4 to 1/2 of a whole note", even);
    check("each tap in its own colour", node(0, 0, 0).shade == 0 && node(4, 1, 0).shade == 4);
    check("the smear fades back from its head: two full, three at 60 %, five at a third",
          node(2, 0, 1).shade == 2 && node(2, 0, 2).shade == 7 && node(2, 0, 4).shade == 7 && node(2, 0, 5).shade == 12 &&
              node(2, 0, smear - 1).shade == 12);
    check("the timeline reads 0 500ms 1 s 1.5 s 2 s 2.5 s, beats at 120 BPM",
          says(21, "0") && says(22, "500ms") && says(23, "1 s") && says(24, "1.5 s") && says(25, "2 s") &&
              says(26, "2.5 s") && frame.nodes[124].x == opxy::draw::hidden_x * 1024);
    check("the tap rows are numbered", says(16, "1") && says(20, "5"));
    bool rest = true;
    for (unsigned j = 1; j < smear; ++j) rest = rest && node(0, 0, j).x == node(0, 0, 0).x;
    check("at rest the smear sits on the tap", rest);
    check("a valid frame with the taps", opxy::draw::valid(frame));
    // Turning Time: the markers glide and smear back to where they were.
    t[1] = pick(12, comet::kDivisions);
    show(t, 0);
    check("Time turned up to 1/2: the timeline stays put", says(25, "2 s") && says(26, "2.5 s"));
    check("the marker glides from 1/4 toward 1/2", left(0, 0, 0) > at(0.25) + 0.5 && left(0, 0, 0) < at(0.5) - 0.5);
    check("and smears back toward 1/4", left(0, 0, smear - 1) < left(0, 0, 0));
    for (int i = 0; i < 60; ++i) show(t, 0);
    check("then settles at 1/2", std::fabs(left(0, 0, 0) - at(0.5)) < 0.6 && left(0, 0, smear - 1) == left(0, 0, 0));
    // Reduced motion snaps.
    input.reduced_motion = 1;
    t[1] = pick(6, comet::kDivisions);
    show(t, 0);
    check("reduced motion: the marker snaps", std::fabs(left(0, 0, 0) - at(0.125)) < 0.6 && left(0, 0, smear - 1) == left(0, 0, 0));
    input.reduced_motion = 0;
    // Two taps: taps 3..5 grey, their strips too.
    t[4] = pick(1, 5);
    show(t, 0);
    check("taps Taps leaves out are grey, their tinted strips hidden",
          node(2, 0, 0).shade == 15 && node(2, 0, 6).shade == 15 && frame.nodes[8 + 4].x == opxy::draw::hidden_x * 1024 &&
              node(1, 0, 0).shade == 1 && frame.nodes[8 + 2].x == 60 * 1024);
    // Free time: the same 2.5 s timeline; a tap past the line's 1.43 s stops
    // there, and a bounce past 2.5 s at the timeline's end.
    t[0] = pick(0, 2);
    t[1] = amount(1);
    t[4] = pick(4, 5);
    input.reduced_motion = 1;
    show(t, 0);
    check("free: the same timeline, 0 500ms ... 2.5 s", says(22, "500ms") && says(26, "2.5 s"));
    check("free: 1.4 s sits at 1.4 / 2.5 of the timeline", std::fabs(left(0, 0, 0) - (60 + 400 * 1.4 / 2.5)) < 1);
    check("free: a tap past the line stops at 1.43 s", std::fabs(left(4, 0, 0) - (60 + 400 * 1.43 / 2.5)) < 0.6);
    input.choices[1] = 1;
    show(t, 0);
    check("free with Pingpong: the timeline keeps its size", says(26, "2.5 s") && std::fabs(left(0, 0, 0) - (60 + 400 * 1.4 / 2.5)) < 1);
    check("free: a bounce past 2.5 s stops at the timeline's end", std::fabs(left(4, 1, 0) - 460) < 0.6);
    input.choices[1] = 0;
    // Rnd moves the taps per side, as the audio does.
    input.choices[0] = 1;
    t[1] = amount(0.85);
    show(t, 0);
    bool sides = false;
    for (unsigned k = 1; k < 5; ++k) sides = sides || std::fabs(left(k, 0, 0) - left(k, 1, 0)) > 1;
    check("Rnd: the sides scatter differently", sides);
    const double first_draw = left(2, 0, 0);
    input.choices[0] = 2;
    input.shift = 1;
    input.line = 0;
    show(t, 0);
    check("the next Inc on Mode reads inc", says(0, "mode") && says(4, "inc"));
    input.choices[0] = 5;
    show(t, 0);
    check("a later Rnd reads rnd", says(4, "rnd"));
    input.shift = 0;
    input.choices[0] = 3;
    show(t, 0);
    check("the next Rnd on Mode moves the taps to a new draw", std::fabs(left(2, 0, 0) - first_draw) > 1);
    // Pingpong: the right side glides out to the bounce, Time plus its tap.
    input.choices[0] = 0;
    input.choices[1] = 0;
    input.reduced_motion = 1;
    t[0] = pick(1, 2);
    t[1] = pick(9, comet::kDivisions);  // 1/4
    t[5] = amount(0.5);                // taps at 1/4, 9/32, 5/16, 11/32, 3/8
    show(t, 0);
    const double off_right = left(0, 1, 0);
    input.choices[1] = 1;
    show(t, 0);
    bool bounce = true;
    for (unsigned k = 0; k < 5; ++k) {
        const double tap = 0.25 * (1 + 0.5 * k / 4.0);
        bounce = bounce && std::fabs(left(k, 0, 0) - at(tap)) < 0.6 && std::fabs(left(k, 1, 0) - at(0.25 + tap)) < 0.6;
    }
    check("Pingpong off: the right side sits with the left", std::fabs(off_right - at(0.25)) < 0.6);
    check("Pingpong on: the left at its taps, the right a whole Time later", bounce);
    t[1] = pick(12, comet::kDivisions);  // 1/2 with Deviate full: the furthest bounce, 1 1/2 whole notes
    t[5] = amount(1);
    show(t, 0);
    check("the furthest bounce, 1/2 then a whole note, stops at the timeline's end", std::fabs(left(4, 1, 0) - 460) < 0.6);
    input.reduced_motion = 0;
    input.choices[1] = 0;
    show(t, 0);
    check("Pingpong off again: the right glides back", left(0, 1, 0) < at(1) && left(0, 1, 0) > at(0.5) + 0.5);
    check("a valid frame at the end", opxy::draw::valid(frame));
    if (failures) return 1;
    std::puts("display ok");
    return 0;
}
