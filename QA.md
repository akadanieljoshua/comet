# Testing Comet on an OP-XY

Checks to run on a device with a firmware that includes Comet, from top to bottom, in one project:
each section leaves the project ready for the next. To run only some sections, set up what their first
line says. Host tests (`python ../open-xy/sdk.py test --machine .`) don't replace these: they can't hear
the sound or see the screen.

Report a failure with the section, the line and what you saw. A photo of the screen helps.

Note the SDK commit and firmware each full pass used.

AI agents working on the machine's code keep this file up to date as they go: each change that adds,
removes or alters something a person would check on the device updates its lines here.

## Setup

- Build a firmware with the machine ([building firmware](https://github.com/etcetc000/open-xy/blob/staging/docs/build.md#3-add-your-machine))
  and install it.
- Create a new project, **plain**, without Comet: give track 1 a stock sound of its own and save it.
  Later steps load it as a project that never had the machine.
- Create another new project, **qa**, for the pass: don't use a project you care about.
- Track 1: a stock synth, with one note on step 1 lasting a whole bar, so it sounds all the time while
  the sequencer plays. Track 15 (a send): Comet (Shift + track 15's key, then choose it). Turn track 1's
  send to track 15 up.
- Press play, and leave it playing unless a step says otherwise.

## Firmware basics

- [ ] The OP-XY boots, and stock engines, effects, the sequencer and project save/load work
- [ ] Comet is in the effect list of tracks 15 and 16 (Shift + their track keys), under its name, and in
      no other track's list

## Its page and controls

Page 1: **Sync, Time, Feedback, Dry**. Page 2: **Taps, Deviate, Curve, Diffuse**. Shift + encoder 1:
**Mode**; Shift + encoder 2: **Pingpong**. Under the header, the taps
in the spirit of AS D-Delay's display.

- [ ] The header shows both rows, page 1 on top and emphasised; press any encoder: all four switch to
      page 2 together and the emphasis moves to the second row; press again for page 1. Each control keeps
      its value across presses
- [ ] Each control through its whole range, both directions: the sound follows, with no clicks, jumps or
      silence at the ends. Sync and Pingpong read `off`/`on`; Time a note length under `time`
      when synced, or a number under `time ms` / `time s` when not; Taps `1`..`5`; Mode `inc`/`rnd`;
      Deviate, Curve and Mode `off` with one tap; the rest 0..100; Dry starts at 100
- [ ] Every caption fits its column at 1:1: `1/16T`, `1/32D`, `1/16D`, `999`, `1.40`, `time ms`, `feedback`,
      `deviate`, `diffuse`, `pingpong`
- [ ] Sync, Taps, Mode and Pingpong move one choice per detent, both directions, and stop at their ends
- [ ] Shift held, on either page: the first two columns read `mode` and `pingpong`; Shift + encoder 1 switches
      Mode, Shift + encoder 2 Pingpong; letting go shows the page again. Shift + encoders 3 and 4 change
      nothing. A new project starts at `inc` and `off`
- [ ] Dry at 100: track 1 sounds at its own level as the send rises, Comet's repeats on top. Dry at 0 and
      track 1's send full: only Comet's repeats are heard. In between, the track's own sound fades as the
      send rises. Turning Dry never changes the repeats' level, and never feeds back
- [ ] Fast turns: the value keeps up, no flicker or slowdown
- [ ] With track 1's send at 0 the effect is silent; raising it brings the effect in smoothly
- [ ] **First:** a track sending to Comet at full send, Feedback 0: one clean repeat per hit and silence
      after, at every send level, with nothing building up. Track 1's dry stays at its own level
- [ ] Mute track 15: the repeats stop; track 1's dry carries on
- [ ] Stop the sequencer while a note rings: the effect's tail ends naturally, with no click
- [ ] The same effect on track 16, at the same time as on track 15: each keeps its own values

## Time and Sync

- [ ] Sync on: Time steps through 13 note lengths, `1/32` to `1/2` (about eight detents each); the repeats
      land on the grid at several tempos, and follow a tempo change
- [ ] Sync on, change the note length while playing: the repeats glide to it like tape, without a click
- [ ] Sync off: Time reads `0.2` under `time ms` at the bottom to `1.40` under `time s` at the top, and the
      repeats land there; turning it bends the pitch of the repeats like tape
- [ ] Switch Sync off and on while playing: no click
- [ ] A send LFO on Time (Sync off, short Time, a little Feedback): a flanger; at about 10 to 30 ms, a chorus
- [ ] `1/2` at a slow tempo (below about 42 BPM), and long Deviate spreads: the longest repeats
      stop at about 1.43 s, without noise or garbage

## Taps, Mode and Deviate

- [ ] Taps 1: Comet is a plain delay, as in D-Delay: Deviate, Curve and Mode read `off` and turning them changes
      nothing. Taps 2 to 5: they read their values again, and Deviate spreads taps 2..5 out to
      (1 + Deviate x k/4) times Time, up to twice
- [ ] Mode `inc`: the taps sit evenly. Mode `rnd`: they scatter unevenly, differently on each side,
      each between Time and its `inc` place, not always in order, as in D-Delay
- [ ] Shift + encoder 1 keeps alternating `inc`, `rnd`, `inc`, `rnd`...: each further `rnd` is a new
      scatter, heard and shown; turning back brings back the scatters passed; it stops after 127 `rnd`s.
      Save and reload: the same scatter
- [ ] Switch Mode while playing, synced and free: the taps move without a click

## The taps on the page

- [ ] Under the header: five numbered rows, each two strips (left above right) on one timeline, with grid
      lines and a fixed scale beneath: `0 500ms 1 s 1.5 s 2 s 2.5 s` always (with Sync on, a beat each at 120 BPM)
- [ ] Each tap's marker in its grey, tap 1 brightest down to tap 5 dimmest, all readable, on dark grey
      strips; taps beyond Taps a dimmer grey on near-black rows. Taps 1: only the first row lit
- [ ] Sync on, 1/4, five taps, Deviate 100, Curve 50, Mode `inc`: the markers at 1/4, then evenly to 1/2
- [ ] Sync on, 1/2, Deviate 100, Pingpong on: the furthest right markers stop at the end, 2.5 s
- [ ] Turning Time, Deviate or Curve: the markers glide there and smear behind as they move, the smear
      fading from the bright marker back, then settle
      to a single bar; no flicker, no stray lines or streaks anywhere on the screen
- [ ] Mode `rnd` (Shift + encoder 1): the left and right markers of each tap scatter differently
- [ ] Pingpong on (Shift + encoder 2): each row's right (lower) marker glides out a whole Time, tap 1 to
      twice Time; off, it glides back under the left. Matches what you hear: the left first, the right a
      Time later
- [ ] Sync off, Time at the top: the first marker near the right end; the farthest taps stop at 1.43 s
- [ ] The page keeps up with fast turns, and the header still switches pages and shows Shift as before

## Curve

- [ ] A new project starts Curve at 50; with one tap it reads `off` and turning it changes nothing
- [ ] Five taps, Deviate 100, Mode `inc`: at 50 the taps are even; turning up crowds them just after the
      first (gaps growing); turning down crowds them before the last (gaps shrinking, like a bouncing
      ball). It sweeps smoothly, gliding without clicks; the first and last taps don't move
- [ ] The same with Mode `rnd`: the scatter leans early or late with Curve

## Pingpong

- [ ] A new project starts Pingpong off
- [ ] Shift + encoder 2 to on: the repeats bounce left, right, left, Time apart, even from a mono source; off: they stay
      on their own sides. Switching while repeats ring: no click
- [ ] Pingpong survives save and load, and each pattern keeps its own

## The smear

- [ ] Feedback: more repeats as it rises; below 100 they always die away, even with five taps; at 100 the
      sound holds without growing louder than the source
- [ ] Short Time (1 to 10 ms) with Feedback: a comb filter, then a pitched resonator as Feedback rises
- [ ] Karplus-Strong: Taps 1, Sync off, Time below 50 ms, Feedback high, short notes into the send: each
      hit rings as a plucked string, pitched by Time (higher as Time falls), darkening as it rings and
      sustaining for seconds at Feedback 100 without growing. Above 50 ms the repeats are a plain echo
- [ ] Feedback 100 at short Time, then stop sending: no hum, hiss or static is left once the string fades
- [ ] Diffuse: each repeat smears into a wash, more with each pass; at 0 the repeats are clean again
- [ ] Each preset (Echo, Ping Pong, Comb, Resonator, Chorus, Wash, Smear, Scatter) sounds as named

## Parameter locks

- [ ] Lock each control on a step (hold the step, turn the knob): it plays on that step, and the page
      shows the lock while the step is held
- [ ] Several locks on one step, and locks on neighbouring steps: each plays as written
- [ ] Clear the locks: the control plays its own value again

## Patterns, scenes and songs

- [ ] Give the track a second pattern with different values: each pattern keeps its own
- [ ] Copy a pattern: the copy holds the same values, and changing one leaves the other alone
- [ ] Switch between the patterns while playing and while stopped: the sound and the page follow at once
- [ ] Make scenes 1 and 2 with the two patterns, and a song of scene 1 then scene 2, looping: each scene
      plays its own values from its first bar, on every pass

Then play scene 1.

## Mute

- [ ] Mute the track (hold aux and press the track's key): it's silent, with no click or tail left behind;
      unmute: it plays as before
- [ ] The same from the mixer
- [ ] Muted in scene 2 only: switching between scenes 1 and 2 mutes and unmutes it

Unmute it everywhere.

## Saving and loading

- [ ] Save **qa**, power off and on, load it: the machine is there with every value as you left it, on
      each pattern
- [ ] Change a value, wait a few seconds and power off without saving: the OP-XY starts normally, and
      **qa** loads as last saved
- [ ] Load **plain**: track 1 has its own sound, nothing of **qa** shows. Load **qa** again: all back
- [ ] While playing, load **plain** and then **qa** again: nothing freezes, no stray values
- [ ] Create a new project: put Comet on a track; it starts at its defaults
- [ ] On a firmware without Comet, open **qa**: it loads, and the rest of the project plays

## CPU

- [ ] Follow [hardware profiling](https://github.com/etcetc000/open-xy/blob/staging/docs/profiling.md):
      a profiling build from committed source, every required workload collected, and the readout
      kept with the commit it measured
- [ ] Run `sdk.py profile report`: inspect both average CPU (`block_percent`) and worst observed block
      CPU (`peak_percent`), and the workload that produced the spike; a heaviest-setting average is not a peak
- [ ] Before submitting, copy the complete generated `cpu` member into the catalog entry; resolve clipped
      or invalid peaks rather than omitting them. Hardware evidence still pending means submission is not ready

## In the builder

Once the machine's catalog entry is merged and released (the beta site from `staging`, open-xy.com from
`main`; see [submitting a machine](https://github.com/etcetc000/open-xy/blob/staging/docs/catalog.md#submit-a-machine)):

- [ ] The machine list shows Comet as Community, with its name, maintainer and description
- [ ] Filtering by its kind (effects) lists it, and filtering by the other kinds doesn't
- [ ] Searching its name, and each of its keywords, finds it; its details panel lists its keywords, and
      clicking one lists it among that keyword's machines
- [ ] Choosing it builds a firmware that includes it, and that firmware passes
      [Firmware basics](#firmware-basics)

## Known limits

Not failures, but worth confirming they haven't changed.

- The line holds about 1.43 s: longer spreads stop there
- Comet has no LFO of its own: use the send's LFO on Time (page 1's controls are the send LFO's targets)
- There is no Randomize button: each further `rnd` on Mode is a new, fixed random set (127 in all)
- The page draws where the taps are, not the repeats themselves; a synced tap past 1.43 s (slow tempos)
  shows where the note falls, though the audio stops it at 1.43 s
- D-Delay's filter (and its LFO), time LFO, Excite drive, jitter and per-tap polarity are not included
