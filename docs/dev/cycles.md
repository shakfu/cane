# Cycles: patterns that change between loops

Status: implemented, except `--seed` and `--cycles`. Open questions 1 and 2 below were settled: the cycle count is global, and `<>` nests as in Tidal. Goal: a pattern can differ from one loop to the next. Prerequisite: `looping.md` steps 0 and 1 (`join` fix, `|` to the LCM).

## Current behaviour

- `Session::eval` evaluates a program once. The player loops the resulting `Schedule` unchanged (`include/cane/midi.hpp`).

- `?` picks with `std::random_device` at evaluation (`ops.hpp`). The pick is fixed for every loop of that program, and differs between runs.

So every loop is identical. TidalCycles and Strudel get most of their variation from functions of the cycle number: `every`, `<a b>`, `sometimes`, seeded random.

## Definitions

- A **cycle** is one loop of the program's output. Its length is the length of that loop's sequence, so cycles of one program can differ in length, e.g. under `every 2 (** 2)`.

- The **cycle number** `n` counts loops since the player started. It keeps counting when a new program replaces the old one, so `every 4` stays aligned with earlier bars after an edit. Tidal does the same.

## Proposal

A program is evaluated once per cycle, with `n` as an input.

1. `Session` keeps the typed tree of the last playable program. The tree is immutable and its source text is never freed (`sources` is a deque), so the player thread can evaluate it without the session mutex.

2. The evaluator takes the cycle number: `pass_evaluator_walk(cfg, n, node)`.

3. `Player::play` takes a generator, `std::function<Sequence(Scalar n)>`, in place of one `Sequence`. While cycle `n` plays, the player evaluates cycle `n + 1`. One cycle of lookahead hides evaluation time.

4. An evaluation error in cycle `n + 1` is printed. The player repeats cycle `n`'s schedule and tries again on the next boundary.

5. Each cycle seeds a `std::mt19937_64` from `(seed, n)`. Each `?` takes the next draw in evaluation order, so two `?` in one cycle differ. Nodes hold only a `string_view`, so a source offset was not available, and a binding used twice would have shared one. Editing a program shifts the draws after the edit. The seed is 0 by default; `--seed` will change it. The same program and seed always play the same.

`cane FILE` (play once) plays cycle 0. `--cycles N` would play cycles 0 to N-1.

As built, `Session::Result::at` evaluates any cycle, and holds the session's source text so it can outlive the session. `main` passes it to `Player::play`.

## Syntax

| Syntax | Meaning | Type |
|-|-|-|
| `cycle` | The cycle number | scalar |
| `<a, b, c>` | Element `n mod 3` | each element's type; all must match |
| `every k f x` | `f x` when `n mod k` is 0, else `x` | `x`'s type |
| `sometimes f x` | `f x` on a seeded half of cycles, else `x` | `x`'s type |
| `a ? b` | As now, but seeded per cycle | scalar |

- `<` and `>` are not lexed alone today, so `<a, b>` is free. Inside it, `,` separates elements, as in a melody literal. `>>` lexes as rotate, so a nested close needs a space: `<a, <b> >`.

- `<a, b>` takes any type, so it alternates sequences as well as notes: `<bd@3:8, bd@5:8> | hh@!!!!`.

- `every` and `sometimes` are builtins. The evaluator evaluates only the chosen branch. A general `if` needs `==` and a boolean type; neither exists, and these two cover the common cases.

```text
#! Snare shifted one step every 4th bar.
snare = sd@....!...;
every 4 (>> 1) snare | bd@!...!... ~> "drums"

#! Bassline root alternates each bar.
[<C2, F2>, Eb2, G2]@!.!! ~> "303"
```

## Implementation

### Files touched

| File | Change |
|-|-|
| `include/cane/def.hpp` | `Cycle`, `Alternate`, `Every`, `Sometimes` symbols |
| `include/cane/lex.hpp` | `<`, `>` |
| `include/cane/parse.hpp` | `<a, b>` literal |
| `include/cane/passes.hpp` | Type rules (n-ary, all elements equal); evaluator takes `n`; lazy branches |
| `include/cane/ops.hpp` | Seeded `choice`; hash helper |
| `include/cane/session.hpp` | Keep the typed tree; builtins `cycle`, `every`, `sometimes`; generator |
| `include/cane/midi.hpp` | Generator-driven player with lookahead |
| `src/cane.cpp` | `--seed`, `--cycles` |

### Tests

`tests/eval.cpp`, evaluating cycles 0 to 3 directly:

- `<36, 38>@!`: notes 36, 38, 36, 38.

- `every 2 (+ 12) (bd@!)`: 48, 36, 48, 36.

- `36 ? 38` twice for the same cycle and seed: equal. Over 64 cycles: both occur.

- `sometimes` with seed 0: a fixed, recorded sequence of choices.

`tests/player.cpp`:

- The generator is called once per loop, with increasing `n`.

- A generator whose loop length changes per cycle keeps timing without drift.

- A generator that throws on one cycle: the previous schedule repeats.

## Open questions

- Global cycle count (proposed), or reset to 0 for each new program?

- Should `<a, b>` nest Tidal-style, so `<a, <b, c>>` yields `a b a c`? Tidal divides `n` for inner alternations. Flat indexing is simpler.

- Should the REPL print only cycle 0, or the first few cycles?

- Lookahead of one cycle means a very short cycle (one step, 500 ms) gives evaluation 500 ms. Is that enough for the largest expected programs?
