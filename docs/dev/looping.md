# Looping, layers and polyrhythm

Status: proposal. Goal: support polyrhythm.

## Current behaviour
- A program evaluates to a finite sequence: a list of timed events. Its
  length is the end of its last event, rests included.
- Every step lasts one beat: `MINUTE / bpm` (500 ms at 120 BPM).
- The player loops the whole sequence (REPL, `--watch`) or plays it once
  (`cane FILE`). See `include/cane/midi.hpp`.
- `a $ b` concatenates the two event lists (`concatenate` in
  `include/cane/ops.hpp`). Neither layer repeats.

So the shorter layer plays once per loop and is then silent:

```text
36@3:8 $ 38@!. ~> "drums"

step   1 2 3 4 5 6 7 8
36     x . . x . . x .
38     x .
```

`@` already handles unequal lengths differently: `map` cycles the rhythm
and the melody to the longer of the two. `$` does not.

### Related bug
Sequence `a, b` uses `join`, which offsets `b` by the latest start time in
`a`, not by its end. `36@!., 38@!!` starts `b` at 500 ms, on `a`'s
rest step, instead of at 1000 ms. Any layer arithmetic below depends on
sequence length, so this must be fixed first. README tutorial section 6
shows the buggy output.

## Two different goals
- **Polymeter**: layers share a step length but loop at different lengths.
  A 3-step and a 4-step layer realign after 12 steps.
- **Polyrhythm**: layers divide the same time span into different numbers of
  equal parts. 3 against 4 puts 3 evenly spaced hits in the time of 4 beats.

Every step in cane is one beat, so cane cannot express polyrhythm today. A
3-step layer always lasts 3 beats. Options 1-3 change only how layers of
different lengths repeat, so none of them gives polyrhythm. Option 4 does.

## Options

### 1. Cycle shorter layers to the longest
`$` repeats each layer until it is as long as the longest, cutting the last
repeat short.

```text
36@!.. $ 38@!...                loop: 4 steps

step   1 2 3 4 | 1 2 3 4
36     x . . x | x . . x        the 3-step layer restarts every 4 steps
38     x . . . | x . . .
```

Pros:
- Loop length is the longest layer, so it is easy to predict.
- Same rule as `@` (`cycle` in `ops.hpp`).
- Short loops, so a new pattern starts soon.

Cons:
- A layer whose length does not divide the longest restarts early. `!..`
  becomes `!..!`, so the 3-step pattern is lost. No polymeter.
- No polyrhythm.

### 2. Repeat layers to the least common multiple
`$` repeats each layer to the LCM of all layer lengths.

```text
36@!.. $ 38@!...                loop: lcm(3, 4) = 12 steps

step   1 2 3 4 5 6 7 8 9 10 11 12
36     x . . x . . x . . x  .  .
38     x . . . x . . . x .  .  .
```

Pros:
- No layer is cut short. Each keeps its own length, so phase is correct
  across loops. This is polymeter.
- When lengths divide each other, the loop equals option 1.

Cons:
- Loops grow quickly. 7 against 16 steps loops every 112 steps (56 s at 120
  BPM).
- The player switches patterns at the end of a loop. A long loop delays
  each change by up to its whole length.
- Still no polyrhythm: all layers share one step length.

### 3. Leave `$` as is; repeat by hand
Users repeat layers themselves, e.g. `36@!.. ** 4`.

Pros:
- No change, and every repeat is explicit.

Cons:
- Users compute the LCM by hand.
- No polymeter by default, and no polyrhythm.

### 4. Add `fit`: rescale a sequence to N steps
`a fit n` stretches or squeezes sequence `a` so it lasts exactly `n` steps.
This is the only option that gives polyrhythm. It works with any of
options 1-3.

```text
36@!!! fit 4 $ 38@!!!!            3 against 4, loop: 4 steps (2000 ms)

ms     0      500    667    1000   1333   1500
36     x             x             x
38     x      x             x             x
```

Pros:
- Real polyrhythm: 3 against 4, 5 against 4, triplets (`fit 2` on 3 steps).
- Explicit: unchanged sequences keep one step per beat, so every current
  example keeps its timing.

Cons:
- New syntax: a keyword and a type rule.
- Steps of `4/3` beats are not whole microseconds, so times must be
  rounded. Rounding each event independently would make the length miss
  `n` steps by a microsecond, and the LCM in option 2 would then be huge.

### Rejected: cycle-based time
TidalCycles fits every layer into one cycle by default. That gives
polyrhythm everywhere, but a step would no longer be one beat. Every
example and every existing tempo assumption would change. `fit` gives the
same result where it is asked for.

## Recommendation
Options 2 and 4 together:
- `$` repeats layers to the LCM of their lengths: polymeter with no layer
  cut short.
- `fit` rescales a sequence to N steps: polyrhythm.

Option 1 is rejected because it breaks polymeter, which is the more common
of the two in step-based music. Option 3 leaves both goals to the user.

The cost of option 2 is loop length. Two limits bound it:
- `$` reports an error if the LCM exceeds 256 steps (128 s at 120 BPM),
  instead of building a loop that long. The error names the layer lengths,
  so the user can adjust them or use `fit`. 256 is a starting value.
- Later, the player can switch patterns at the end of the longest layer
  instead of the whole loop. That cuts the switch delay without changing
  the sound. It needs the player to know the layer lengths, so it is out of
  scope here.

### How it looks
```text
#! Polymeter: 3-step and 4-step layers, realigning every 12 steps.
bd@!.. $ sd@!... ~> "drums"

#! Polyrhythm: 3 kicks against 4 hats in one bar.
bd@!!! fit 4 $ hh@!!!! ~> "drums"

#! Triplets over a straight bar: 6 hits in the time of 4 steps.
hh@!!!!!! fit 4 $ bd@!.!. ~> "drums"

#! Euclidean polymeter: 5-in-16 against 3-in-8 (lcm 16).
bd@5:16 $ sd@3:8 ~> "drums"
```

## Implementation

### 0. Fix `join`
In `ops.hpp`, offset `rhs` by `length(lhs)` (the largest
`timestamp + duration`), not by its largest `timestamp`. `length` exists.
`schedule` in `midi.hpp` computes the same value and should use it.

### 1. `$` repeats to the LCM
In `ops.hpp`, using the existing `length` and `repeat_sequence`:

`layer(lhs, rhs, limit)`:
1. `L = std::lcm(length(lhs).count(), length(rhs).count())`.
2. If `L > limit`, report `ReportKind::Eval` with both lengths in steps.
3. Return both layers, each passed through `repeat_sequence(_, L / len)`,
   merged and passed through `sort_by_time`. Sorting keeps printed output in
   time order; the player sorts anyway.

An empty layer (length 0) contributes nothing. It must not reach `std::lcm`,
because `lcm(x, 0)` is 0, and `L / len` would divide by zero.

The evaluator's four `Layer*` cases call `layer` instead of `concatenate`.
`limit` is `256 * MINUTE / cfg.bpm`. A `max_loop_steps` field in
`Configuration` would let users change it later.

`a $ b $ c` groups left to right, so the LCM covers all three:
`lcm(lcm(a, b), c)`.

### 2. `fit`
- `def.hpp`: add `X(Fit, "fit", Infix, Incr, Left)` after `Layer`. That
  places it above `$` and below `@`, so `bd@!!! fit 4 $ ...` parses as
  `((bd@!!!) fit 4) $ ...` without parentheses. Add operator symbols
  `FitSequenceScalar` and `FitPatternScalar`.
- `lex.hpp`: lex the keyword `fit`, next to `lcm` and `gcd`.
- `passes.hpp`: add the type rules
  `CANE_TYPE_REMAP(Fit, Sequence, Scalar, Sequence, FitSequenceScalar)` and
  the Pattern equivalent. Then add an evaluator case calling
  `fit(seq, n * MINUTE / cfg.bpm)`.
- `ops.hpp`: compute each new time from the original time with one integer
  multiply and divide, so the last event ends exactly at `target`:

```cpp
// Rescales `seq` to last exactly `target`. Start and end times are scaled
// separately, so rounding never accumulates and the last end is exact.
template <typename T>
inline T fit(T seq, TimeUnit target) {
	auto len = length(seq).count();
	if (len == 0) {
		return seq;
	}
	auto scale = [&](TimeUnit t) {
		return TimeUnit { t.count() * target.count() / len };
	};
	for (auto& ev: seq) {
		auto end = scale(ev.timestamp + ev.duration);
		ev.timestamp = scale(ev.timestamp);
		ev.duration = end - ev.timestamp;
	}
	return seq;
}
```

Overflow: `t * target` at 120 BPM and the 256-step limit is about
`1.3e8 * 1.3e8 = 1.7e16` us, below `int64` max (`9.2e18`).

- `fit 0` and negative `n` report an error.

### 3. Tests
`tests/eval.cpp`:
- `36@!.. $ 38@!...`: 24 events over 6000 ms. Note 36 beats at
  steps 1, 4, 7, 10; note 38 at 1, 5, 9.
- `36@3:8 $ 38@!.`: loop 8 steps; note 38 on every odd step.
- A 7-step layer against a 64-step layer (LCM 448) reports the limit error.
- `36@!!! fit 4`: starts at 0, 666666, 1333333 us; ends at exactly
  2000000.
- `36@!., 38@!!`: `b` starts at 1000 ms (the `join` fix).

README:
- Fix tutorial section 6 for `join`.
- Add a "Polymeter and polyrhythm" tutorial step with the examples above.
- Add `fit` to the operator table, and the type rules to `docs/types.md`.
- `tests/readme.cpp` evaluates every new example.

### Files touched
| File | Change |
|-|-|
| `include/cane/ops.hpp` | `layer`, `fit`; `join` fix |
| `include/cane/def.hpp` | `Fit` symbol and operator symbols |
| `include/cane/lex.hpp` | `fit` keyword |
| `include/cane/passes.hpp` | type rules; `Layer*` and `Fit*` evaluator cases |
| `include/cane/midi.hpp` | `schedule` uses `length` |
| `tests/eval.cpp` | cases above |
| `README.md`, `docs/types.md`, `docs/syntax.ebnf` | docs |

## Open questions
- Name: `fit`, or reuse `/` with a rational (`seq / 4:3`)? `:` already
  builds Euclidean rhythms, so the rational form would be ambiguous. A
  keyword reads more clearly.
- Should `fit` take a count of steps (as proposed), or another sequence to
  match, e.g. `a fit b`? The count is simpler. Matching a sequence avoids
  counting by hand.
- Is 256 steps the right limit?
