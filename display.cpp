#include "display.h"
#include "timing.h"

// Comet's page: the two-row header, so Time can read what it plays (the note
// length while Sync is on, the time itself while it is off), and under it the
// taps as AS D-Delay draws them: a row for each tap, its left side above its
// right, on one timeline. Each tap is a marker in its own colour at its time,
// smeared back to where it was about 170 ms ago, so a moving tap blurs; taps that
// Taps leaves out are grey. The timeline is one fixed size, 2.5 s, marked
// every half second, whatever Sync and Pingpong are; a tap or bounce beyond
// it stops at its end. While Sync is on, the page can't know the tempo, so
// note lengths are placed at 120 BPM (a beat is 500 ms, a mark). With
// Pingpong on, the right side shows the bounce, as D-Delay's right side moves
// out to twice Time: the send enters on the left, so the right
// first hears each of its taps a whole Time later, once the first left repeat
// has crossed over. Its markers glide out to Time plus their tap.
namespace comet {
namespace detail {
using namespace opxy::draw;
using opxy::i32;
using opxy::i64;
using opxy::u32;

const u32 kFirstStrip = 8, kFirstGrid = 18, kFirstMarker = 23, kTrail = 5, kSmear = 10;
const u32 kFirstLabel = 16, kFirstScale = 21, kFirstExtraGrid = 123, kFirstExtraScale = 26;
const i32 kLeft = 60, kWidth = 400, kTop = 44, kPair = 31, kSide = 13;
const i32 kScaleY = 206;
// Palette: 0..4 the taps' shades, tap 1 95 % grey down to tap 5 28 %; 5..9
// the same at 60 % and 10..14 at a third, for the smear as it fades (never
// darker than just above the strips); 15 the grey marker of taps that Taps
// leaves out. panel.colour.svg.fragment, kept for later, puts D-Delay's tap
// colours in the same palette slots. The
// strips and grid keep their authored greys; a left-out tap's strip is hidden,
// showing the darker strip authored beneath it.
const u32 kColour = 0, kDim = 5, kDimmer = 10, kGreyMarker = 15;
// The smear's shade from its head (0) back to its tail (kSmear - 1): bright,
// then dimmer and dimmer the further back it was.
const u32 kFade[kSmear] = {kColour, kColour, kDim, kDim, kDim, kDimmer, kDimmer, kDimmer, kDimmer, kDimmer};
const i32 kSettle = 51;  // the markers' glide, Q10 seconds (50 ms)
const i32 kLongestCenti = 143000;  // the line's 1.43 s, in hundredths of a ms

// The timeline: 2.5 s, as five beats in 96ths of a quarter (Q8) while Sync is
// on (2.5 s at 120 BPM), or in hundredths of a millisecond while it is off,
// marked every half second. A tap is placed in the same units. Seven grid
// lines and labels are bound; this scale uses six.
const u32 kMarks = 7, kShownMarks = 6;
const i32 kSyncedScale = 480 << 8, kFreeScale = 250000;
// Synced marks are in milliseconds at 120 BPM, a beat to 500 ms. The page
// can't know the tempo (the SDK gives it to the audio only), so at another
// tempo the marks still fall on beats, but their times are 120 BPM's.
const char* const kMarkNames[kShownMarks] = {"0", "500ms", "1 s", "1.5 s", "2 s", "2.5 s"};

// A tap in the timeline's units: `multiple` times Time (Q15). A tap stops at
// the line's end, 1.43 s; a bounce is Time and then a tap, each stopped there.
i32 units_of(const i32* slots, u32 multiple, bool bounce) {
    if (synced(slots[0])) return static_cast<i32>((static_cast<i64>(kDivisionTicks[division(slots[1])]) * multiple) >> 7);
    const i64 time = free_centi_ms(slots[1]);
    i64 tap = (time * (bounce ? multiple - 32768 : multiple)) >> 15;
    if (tap > kLongestCenti) tap = kLongestCenti;
    return static_cast<i32>(bounce ? (time > kLongestCenti ? kLongestCenti : time) + tap : tap);
}

void draw_taps(DisplayState& state, const Input& input, Frame& frame) {
    const i32* const slots = input.slots;
    const bool sync = synced(slots[0]);
    const u32 taps = 1 + choice(slots[4], 5);
    const u32 deviate = unit15(slots[5]);
    const u32 exponent = curve_exponent(unit15(slots[6]));
    const u32 mode = input.choices[0];  // Inc, Rnd, Inc, Rnd, ...
    const bool pingpong = input.choices[1] == 1;
    i32 target[2][5];
    for (u32 side = 0; side < 2; ++side)
        for (u32 k = 0; k < 5; ++k) {
            const u32 multiple = spread(deviate, mode, taps, side, k, exponent);
            const bool bounce = pingpong && side == 1;
            target[side][k] = units_of(slots, bounce ? 32768 + multiple : multiple, bounce);
        }
    // Positions glide in the timeline's units; a change between note lengths
    // and time snaps them.
    const bool snap = !state.started || state.track != input.track || input.reduced_motion ||
                      state.synced != static_cast<u32>(sync);
    const i32 scale = sync ? kSyncedScale : kFreeScale;
    const i32 dt = input.elapsed < 0 ? 0 : input.elapsed > 205 ? 205 : input.elapsed;
    const i32 share = (dt << 10) / (dt + kSettle);  // Q10
    for (u32 side = 0; side < 2; ++side)
        for (u32 k = 0; k < 5; ++k) {
            i32& at = state.at[side][k];
            if (snap) {
                at = target[side][k];
                for (u32 j = 0; j < kTrail; ++j) state.trail[j][side][k] = at;
                continue;
            }
            for (u32 j = kTrail - 1; j > 0; --j) state.trail[j][side][k] = state.trail[j - 1][side][k];
            state.trail[0][side][k] = at;
            at += static_cast<i32>((static_cast<i64>(target[side][k] - at) * share) >> 10);
        }
    state.started = 1;
    state.track = input.track;
    state.synced = sync;
    // Q10 pixels for a time on this scale, stopped at the timeline's end.
    auto pixels = [&](i32 units) {
        const i64 x = (static_cast<i64>(units) * kWidth << 10) / scale;
        return (kLeft << 10) + static_cast<i32>(x > (kWidth << 10) ? kWidth << 10 : x < 0 ? 0 : x);
    };

    for (u32 k = 0; k < 5; ++k) {
        const bool on = k < taps;
        for (u32 side = 0; side < 2; ++side) {
            const u32 strip = k * 2 + side;
            const i32 y = (kTop + kPair * static_cast<i32>(k) + kSide * static_cast<i32>(side)) * unit;
            if (on)
                place(frame, kFirstStrip + strip, kLeft * unit, y, unit, 0, keep);
            else
                hide(frame, kFirstStrip + strip);
            // The smear: kSmear markers from the tap back to where it was
            // five frames ago, as D-Delay draws up to 20 over its last 40 ms,
            // fading as they go back; the faster the sweep, the longer the
            // fade. At rest they all sit on the tap, the brightest on top.
            // The artwork draws a strip's markers tail first, so node
            // first + kSmear - 1 is the head.
            const u32 first = kFirstMarker + strip * kSmear;
            const i32 now = pixels(state.at[side][k]), then = pixels(state.trail[kTrail - 1][side][k]);
            for (u32 j = 0; j < kSmear; ++j) {
                const i32 x = now + static_cast<i32>((static_cast<i64>(then - now) * j) / (kSmear - 1));
                const u32 shade = on ? kFade[j] + k : kGreyMarker;
                place(frame, first + (kSmear - 1 - j), x - 2 * unit, y, unit, shade, fill_and_stroke);
            }
        }
        const char number[2] = {static_cast<char>('1' + k), 0};
        text(frame, kFirstLabel + k, 30 * unit, (kTop + kPair * static_cast<i32>(k) + 16) * unit, unit, number);
    }

    // The timeline's marks: the first label starts at its mark, the last ends
    // at it, the rest are centred (about 5 px a letter).
    const u32 marks = kShownMarks;
    for (u32 i = 0; i < kMarks; ++i) {
        const u32 node = i < 5 ? kFirstGrid + i : kFirstExtraGrid + i - 5;
        const u32 slot = i < 5 ? kFirstScale + i : kFirstExtraScale + i - 5;
        if (i >= marks) {
            hide(frame, node);
            text(frame, slot, hidden_x * unit, hidden_y * unit, unit, "-");
            continue;
        }
        const i32 x = kLeft + (kWidth * static_cast<i32>(i)) / static_cast<i32>(marks - 1);
        const char* const mark = kMarkNames[i];
        i32 width = 0;
        while (mark[width]) ++width;
        width *= 5;
        const i32 left = i == 0 ? x : i == marks - 1 ? x - width : x - width / 2;
        place(frame, node, x * unit, kTop * unit, unit, 0, keep);
        text(frame, slot, left * unit, kScaleY * unit, unit, mark);
    }
}

// Time's caption and title: the note length under "time" while Sync is on,
// else the time with its unit in the title, so the value stays narrow:
// 0.2 .. 999 under "time ms", 1.00 .. 2.50 under "time s".
const char* time_caption(char* out, opxy::i32 sync_word, opxy::i32 time_word) {
    if (synced(sync_word)) {
        copy(out, header_capacity, kDivisionNames[division(time_word)]);
        return "Time";
    }
    const u32 c = free_centi_ms(time_word);
    char text[header_capacity];
    u32 at = 0;
    const char* title = "Time ms";
    if (c < 995) {  // 0.2 .. 9.9 ms
        const u32 tenths = (c + 5) / 10;
        text[at++] = static_cast<char>('0' + tenths / 10);
        text[at++] = '.';
        text[at++] = static_cast<char>('0' + tenths % 10);
    } else if (c < 99950) {  // 10 .. 999 ms
        const u32 ms = (c + 50) / 100;
        if (ms >= 100) text[at++] = static_cast<char>('0' + ms / 100);
        text[at++] = static_cast<char>('0' + ms / 10 % 10);
        text[at++] = static_cast<char>('0' + ms % 10);
    } else {  // 1.00 .. 2.50 s
        const u32 hundredths = (c + 500) / 1000;
        text[at++] = static_cast<char>('0' + hundredths / 100);
        text[at++] = '.';
        text[at++] = static_cast<char>('0' + hundredths / 10 % 10);
        text[at++] = static_cast<char>('0' + hundredths % 10);
        title = "Time s";
    }
    text[at] = 0;
    copy(out, header_capacity, text);
    return title;
}
}

inline void display_reset(DisplayState& state) {
    state.header.reset();
    state.started = 0;
    state.track = 0;
}

inline void display(DisplayState& state, const opxy::draw::Input& input, Frame& frame) {
    using namespace opxy::draw;
    using namespace detail;
    // Page 1: Sync, Time, Feedback, Dry. Page 2: Taps, Deviate, Curve, Diffuse.
    // Shift + encoder 1: Mode; Shift + encoder 2: Pingpong.
    const u32 taps = 1 + choice(input.slots[4], 5);
    char values[8][header_capacity];
    copy(values[0], header_capacity, synced(input.slots[0]) ? "on" : "off");
    const char* const time = time_caption(values[1], input.slots[0], input.slots[1]);
    header_amount(values[2], header_capacity, input.slots[2]);
    header_amount(values[3], header_capacity, input.slots[3]);
    const char digit[2] = {static_cast<char>('0' + taps), 0};
    copy(values[4], header_capacity, digit);
    // Deviate, Curve and Mode spread taps 2..5, as in D-Delay, which greys them
    // out with one tap: then Comet is a plain delay and they read off.
    if (taps == 1) {
        copy(values[5], header_capacity, "off");
        copy(values[6], header_capacity, "off");
    } else {
        header_amount(values[5], header_capacity, input.slots[5]);
        header_amount(values[6], header_capacity, input.slots[6]);
    }
    header_amount(values[7], header_capacity, input.slots[7]);
    const char* titles[8] = {"Sync", time, "Feedback", "Dry", "Taps", "Deviate", "Curve", "Diffuse"};
    // While Shift is held, the edited row's first two columns show the shift
    // row: Mode (off with one tap) and Pingpong.
    if (input.shift) {
        const u32 column = input.line ? 4 : 0;
        titles[column] = "Mode";
        copy(values[column], header_capacity, taps == 1 ? "off" : is_random(input.choices[0]) ? "rnd" : "inc");
        titles[column + 1] = "Pingpong";
        copy(values[column + 1], header_capacity, input.choices[1] ? "on" : "off");
    }
    page_header(state.header, input, frame, titles, values, 0);
    draw_taps(state, input, frame);
}
}
