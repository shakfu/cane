# Changelog

Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## Unreleased

Changes since the fork from
[tarpit-collective/cane](https://github.com/tarpit-collective/cane) at
`b4ce055`. The original is archived.

### Added
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
- Printing a sequence recursed until the stack overflowed.
- `melody @ rhythm` read the melody as a scalar.
- Melody-scalar `-` and `/` swapped their operands.
- `?` used an unseeded generator and always picked the same side.
- Sequence `*` and `/` scaled durations but not start times, so events
  overlapped or left gaps.
- `/` bound tighter than `*`, and `and` tighter than `or`.
- A trailing `;` failed to parse; a trailing comment read past the end of
  the source.
