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
- **AS D-Delay** by Sabroi (a Max for Live device). Comet's controls and
  behaviour are modelled on it: Sync's note lengths, Deviate and Curve
  spreading the taps by `1 + Deviate x (k/4)^Curve`, Rnd's random scatter per
  tap and side, the tap level law, the input diffusion's allpass lengths and
  the row-per-tap display. Comet contains no Max patch, gen~ code or other
  file from the device; its code and artwork are written from scratch for
  the OP-XY.
- The D-Delay tap display notes that it is modified from a patch by
  **pitchloop89**; Comet's tap display follows that design in turn.

## Code

Comet's audio engine is built on the core **Ping Pong** machine
(Copyright (c) 2026 Open-XY contributors, MIT): its tap reading,
unwritten-sample check, glide and Q31 arithmetic. See `LICENSE`.
