# Changelog

Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## Unreleased

Changes since the fork from
[tarpit-collective/cane](https://github.com/tarpit-collective/cane) at
`b4ce055`. The original is archived.

### Added
- Each loop is a cycle: the player evaluates the program again for every
  loop, one cycle ahead. The builtin `cycle` counts loops from 0 and keeps
  counting across new patterns, so bar-aligned changes stay aligned after
  an edit. A cycle that fails to evaluate is reported and the previous one
  repeats. Design: `docs/dev/cycles.md`.
- `<a, b, c>` plays one element per cycle. `every k f x` applies `f` on
  cycles 0, `k`, `2k`, ...; `sometimes f x` on a seeded half. Each takes
  any type and evaluates only the chosen branch.

  ```
  <bd@3:8, bd@5:8> | every 4 (>> 1) (sd@....!...) ~> "drums"
  ```

  A nested `<>` advances only when chosen, so `<a, <b, c> >` plays
  `a b a c`, as in Tidal; flat indexing never plays `b`. A nested close
  needs a space, because `>>` lexes as rotate. Reading `>>` as two closes
  would force parentheses around rotate inside an element.
- `a vel v` sets velocity (0-127) and `a gate g` the percent of each step
  that sounds (1-10000), from a scalar or a melody of per-step values.
  Printed events show both.

  ```
  hh@!!!!!!!! vel [127, 50, 80, 50] | [C2, G2]@!!!! gate 40
  ```

  The gate is stored apart from the step duration, because the duration
  sets the loop length. A note-off past the loop end moves to it; wrapped
  into the next loop, it would be lost on a pattern change and leave the
  note sounding.
- `a fit n` rescales a sequence to exactly `n` steps (1-256): polyrhythm
  with `bd@!!! fit 4 | hh@!!!!`, subdivision with `bd@!, sd@!!! fit 1`.
  Each time is scaled from the original, so the last end lands exactly on
  `n` steps, which layering by LCM needs. It binds like `**`, not just
  above `|` as `docs/dev/looping.md` proposed, so it can fit one part of a
  concatenation.
- Builtins `scale name degrees` (degrees to semitones, wrapping by
  octave), `chord name` (intervals as a melody) and `stack intervals seq`
  (one copy of `seq` per interval, sounding together). Names follow Tidal.

  ```
  scale "minor" [0, 2, 4] + C4             #! [60, 63, 67]
  stack (chord "min") ([A2, D3]@!..!..)    #! block chords
  ```
- CMake build with presets (`debug`, `release`, `sanitize`), wrapped by a
  `Makefile`. `make test` runs 11 tests.
- MIDI output via libremidi, looping at 120 BPM. A new pattern starts at the
  end of the current loop. Sounding notes are released on exit. Options:
  `--port`, `--virtual`, `--list`, `--no-midi`.
- REPL (linenoise). Bindings persist across lines; a failed line commits
  none.
- `--watch FILE`: re-evaluate FILE on each save, sharing bindings with the
  REPL.
- Note names in scientific pitch, case insensitive: `C4` is 60, `Bb3` is 58.
  Range `C0`-`G9`; octave -1 is omitted because `e-1` would lex as a note.
- Drum names bound at start: `bd`, `sd`, `hh` and 11 more, from General MIDI.
- Euclidean offset: `a:b:n` shifts `a:b` left by `n` steps, matching Tidal's
  `bd(a,b,n)`.
- Sequence operators `+`, `-` (transpose), `**`, `<<`, `>>`, `'`, so
  transforms can follow `@`: `sd@..!.**2`.
- Function definitions and calls. Calls are curried and bind tightest.

  ```
  g x y = (x + y) / 2;
  h = g 10;
  h 2                    #! 6
  ```

  Calls are reduced by substituting the argument into the body during
  binding resolution, so the evaluator needed no closures. Recursion is not
  supported.
- Operator sections, as in Haskell: `(+ 1)`, `(10 -)`, `(+)`. `(- a)` still
  negates.
- Builtin functions `take n x` and `drop n x` for melodies, rhythms,
  sequences and patterns. A negative `n` counts as 0, as in Haskell.
- Type errors for operand types with no rule, e.g. `` `|` does not take
  rhythm and rhythm ``. These used to reach the evaluator and fail as
  "internal error: unable to evaluate".

### Changed
- `|` repeats each layer to the LCM of the layer lengths, so each keeps
  its own period (polymeter). A shorter layer used to play once per loop
  and then fall silent. Cycling to the longest layer instead would restart
  a 3-step layer every 4 steps. Loops over 256 steps are an error, because
  a new pattern waits for the current loop to end.
- `?` picks again each cycle, seeded by the cycle number, so a program
  always plays the same picks. It first always picked the same side, then
  used `std::random_device`, which fixed one pick for every loop.
- `vel`, `gate` and `fit` are keywords, so they can no longer be binding
  names.
- `name = value` replaces `let name value`. `let` implies a constant.
- `note@rhythm` replaces `rhythm @ note`, and binds tighter than every
  operator but `:`, so `bd@3:8` needs no parentheses. A `bd:3:8` form was
  rejected: rhythms are values, so `bd:groove` could not tell note from
  hits.
- The builtin function `abs` replaces prefix `+a` for absolute value, as
  in Haskell. This frees `(+ a)` to be a section.
- `head` and `tail` are builtin functions, not keywords. Like `abs`, they
  can be passed and rebound, and bind as calls: `head x , y` is now
  `(head x) , y`.
- `head` returns a melody's first note as a number, as in Haskell. It no
  longer takes rhythms, sequences or patterns, which have no element type;
  use `take 1 x`.
- Melodies are written `[60, 62]`, as in Haskell; `[60]` and `[]` work.
  `,` no longer builds a melody from numbers, only concatenates, and `&`
  is removed. Port `(60, 62)` to `[60, 62]` and `&60` to `[60]`.
- `|` replaces `$` for layering. In Haskell and Tidal, `$` is function
  application.
- Docs moved from `doc/` to `docs/`; development notes are in `docs/dev/`.

### Fixed
- Sequence `a, b` started `b` at `a`'s last start time, not its end, so
  `bd@!., sd@!!` played `sd` on `bd`'s rest.
- Printing a sequence recursed until the stack overflowed.
- `melody @ rhythm` read the melody as a scalar.
- Melody-scalar `-` and `/` swapped their operands.
- Sequence `*` and `/` scaled durations but not start times, so events
  overlapped or left gaps.
- `/` bound tighter than `*`, and `and` tighter than `or`.
- A trailing `;` failed to parse; a trailing comment read past the end of
  the source.
