# Credits and notices

Comet is an independent work. It is not affiliated with, endorsed by or
sponsored by Ursa Major, Sabroi or their successors. The names below are used
only to credit inspiration; any trademarks belong to their owners.

## Inspiration

- **Ursa Major Space Station SST-282** (Christopher Moore, Ursa Major, 1978):
  a multi-tap digital delay and "reverberation effect" in which several taps
  of one delay line are summed and fed back. Comet's idea of a few taps
  spreading and feeding back until one echo becomes a smear, comb, string or
  wash comes from it. Comet contains no code, firmware, circuit design or
  sound data from the SST-282, and does not implement its patented
  tap-modulation scheme.
- **AS D-Delay** by Sabroi (a Max for Live device). Comet follows its controls
  and behaviour, and takes these values from the device's patch, read as a
  reference: the 13 note lengths for Sync; the tap spacing
  `1 + Deviate x (k/4)^Curve` and Rnd's random 0..1 per tap and side; Curve's
  scale (1/8 + 31/8 x Curve^2.12655); the tap level law n^-0.368; the input
  diffusion's allpass lengths (77/111/199/326, 101/122/222/333,
  107/142/277/379) and gains; and the tap display's layout, row-per-tap strips
  and tap colours. Comet contains no Max patch, gen~ code or other file from
  the device; its code is written from scratch for the OP-XY.
- The D-Delay tap display notes that it is modified from a patch by
  **pitchloop89**; Comet's tap display follows that design in turn.

## Code

Comet's audio engine is built on the core **Ping Pong** machine
(Copyright (c) 2026 Open-XY contributors, MIT): its tap reading,
unwritten-sample check, glide and Q31 arithmetic. See `LICENSE`.

## Before publishing

Credit is not permission. If Comet is shared or sold, check AS D-Delay's
licence or terms of use and ask Sabroi whether the values and display design
above may be used, and avoid presenting Comet as an SST-282 or D-Delay
emulation in its name, artwork or marketing. This note is not legal advice.
