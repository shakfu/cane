# cane (v2)
> The beatings will continue until a banger is made

Cane is a programming language for making music. You write rhythms and
melodies as short expressions and send them to MIDI channels.

![C++](https://badgen.net/static/C++/26/orange)
[![license](https://badgen.net/github/license/shakfu/cane)](./LICENSE)

```
#! Kick on a 3-in-8 euclidean rhythm, snare on 3 and 7, closed hats on every step.
kick  = bd@3:8;
snare = sd@2:8:2;
hats  = hh@8:8;
kick | snare | hats ~> "drums"
```

### Contents
- [Status](#status)
- [Requirements](#requirements)
- [Build](#build)
- [Usage](#usage)
- [Tutorial](#tutorial)
- [Examples](#examples)
- [Reference](#reference)
- [Design](#design)

### Status
Cane is in development.

- Works: lexer, parser, binding resolution, type checking, evaluation, MIDI
  output on an internal clock, the REPL, and file watching.
- Not yet: following an external MIDI clock, and recursive functions.
- Each pass also prints debug logs.
- `tests/readme.cpp` evaluates every example and table row in this README.

### Requirements
- CMake 3.25 or newer, and Ninja.
- A C++26 compiler with `<print>`. Tested with Apple clang 21 on macOS.
- Linux: ALSA development headers (e.g. `libasound2-dev`) for MIDI. JACK is
  used when its headers are found.

The dependencies are vendored in `thirdparty/`:
[libremidi](https://github.com/celtera/libremidi) for MIDI and
[linenoise](https://github.com/shakfu/linenoise) for line editing.

### Build
```sh
$ git clone https://github.com/shakfu/cane.git && cd cane/
$ pre-commit install  # Checks/linting
$ make test           # Build and run tests (debug preset)
```

`make PRESET=release` and `make PRESET=sanitize` select the other presets in
`CMakePresets.json`. Each preset builds into `build/<preset>/`.

### Usage
```sh
$ ./build/debug/cane                    # REPL
$ ./build/debug/cane FILE               # Play FILE once, then exit
$ ./build/debug/cane --watch FILE       # REPL, and re-evaluate FILE on save
```

| Option | Effect |
|-|-|
| `-w, --watch` | REPL, and re-evaluate FILE on each save |
| `-p, --port NAME` | Send to an existing output port: its exact name, or a unique part of one |
| `-v, --virtual NAME` | Create a virtual output port NAME |
| `-n, --no-midi` | Print events only |
| `-l, --list` | List output ports and exit |
| `-h, --help` | Show usage |

#### MIDI output
By default `cane` creates a virtual output port named `cane`. Select it as
the MIDI input of a synth or DAW. To send to a port that already exists,
find its name with `--list` and pass it to `--port`:

```sh
$ ./build/debug/cane --list
IAC Driver Bus 1
$ ./build/debug/cane --port "IAC"
```

Some MIDI APIs, e.g. WinMM on Windows, have no virtual ports. There, `cane`
warns and prints events only unless `--port` is given.

The current pattern loops at 120 BPM. Each loop is a cycle: the program is
evaluated again with the next cycle number, so `cycle` and `?` can change
it from loop to loop. A new pattern from the REPL or a saved file starts at
the end of the current loop. If a cycle fails to evaluate, the error is
printed and the previous cycle repeats. A value that is not a pattern,
e.g. `kick = bd@3:8`, does not change what plays. On exit, `cane`
releases every sounding note.

Events with no channel, e.g. a sequence layered into a pattern without
`~>`, and notes above 127 are not sent.

#### Output
Each program's value at cycle 0 is printed: one line per event for a
pattern, and the value itself for anything else.

```text
     0 ms  ch 10  note  36  vel 127  gate 100%  beat
   500 ms  ch 10  note  36  vel 127  gate 100%  rest
```

In the REPL, each line is compiled when you press enter. Errors are reported
and the session continues. Bindings made with `=` stay defined for later
lines. Ctrl+C clears the line. Ctrl+D exits.

`--watch` evaluates FILE at start and after each save, while the REPL stays
open. Both share one set of bindings, so a name bound in the file can be used at
the prompt. Use it to edit in your own editor and try ideas at the prompt.

## Tutorial
Each step below shows a program and the events it produces. In the step
grids, `x` is a note and `.` is a rest. At the default 120 BPM, each step
lasts 500 ms.

### 1. A first pattern
A rhythm is a string of steps: `!` plays, `.` rests.

```
bd@!.!. ~> "drums"
```

```
step  1 2 3 4
      x . x .     note 36
```

- `bd` is the General MIDI bass drum, note 36. See [Drum names](#drum-names).
- `@` maps the note onto the rhythm. The result is a sequence.
- `"drums"` names an output channel. `~>` sends the sequence to it.
- A program must end in a send. A rhythm or a melody alone is not playable.

Channel names map to MIDI channels. They are set in `src/cane.cpp`. The
defaults are `drums` (10) and `303` (12).

### 2. Euclidean rhythms
`beats:steps` spreads `beats` as evenly as possible across `steps`.

```
bd@3:8 ~> "drums"
```

`:` binds tighter than `@`, so `bd@3:8` is `bd@(3:8)`: the rhythm `3:8`, then
`bd` mapped onto it.

A third part shifts the rhythm left by that many steps, as in Tidal's
`bd(3,8,1)`. Hits can't exceed steps, so a note in place of hits, e.g.
`bd:3:8`, is an error.

| Expression | Steps |
|-|-|
| `3:8` | `x..x..x.` |
| `3:8:1` | `..x..x.x` |
| `2:8:2` | `..x...x.` |

```
step  1 2 3 4 5 6 7 8
      x . . x . . x .
```

### 3. Transforming rhythms
Operators change a rhythm before notes are mapped onto it.

| Expression | Steps | Operation |
|-|-|-|
| `!...` | `x...` | Original |
| `~!...` | `.xxx` | Invert |
| `'!..` | `..x` | Reverse |
| `!... >> 1` | `.x..` | Rotate right |
| `!... << 1` | `...x` | Rotate left |
| `!... ** 2` | `x...x...` | Repeat |
| `!... or .!..` | `xx..` | Step-wise or |
| `!... and !.!.` | `x...` | Step-wise and |
| `!.!. xor !!..` | `.xx.` | Step-wise xor |

To play a row, write it as `bd@(...) ~> "drums"`.

### 4. Melodies
`[a, b, c]` is a melody, as in Haskell: `[60]` has one note, `[]` none. Each
step takes the next note, and the melody repeats until every step has one.

```
[C4, D4, E4]@!!!! + 12 ~> "303"
```

```
step  1  2  3  4
      72 74 76 72
```

Notes are MIDI numbers (`60`) or names in scientific pitch (`C4` is 60).
Names are case insensitive and take `#` or `b`: `c#4` is 61, `Bb3` is 58.

A rest also uses up a note:

```
[C2, Eb2, G2]@!.!! !.!. + 12 ~> "303"
```

```
step  1  2  3  4  5  6  7  8
      48 .  55 48 51 .  48 .
```

Melody operators:

| Expression | Result |
|-|-|
| `[60, 62, 64] + 12` | `[72, 74, 76]` |
| `'[60, 62, 64]` | `[64, 62, 60]` |
| `[60, 62, 64] << 1` | `[62, 64, 60]` |
| `[60, 62] ** 2` | `[60, 62, 60, 62]` |
| `head [60, 62, 64]` | `60` |
| `tail [60, 62, 64]` | `[62, 64]` |
| `take 2 [60, 62, 64]` | `[60, 62]` |
| `drop 2 [60, 62, 64]` | `[64]` |
| `[60, 62], [64]` | `[60, 62, 64]` |

Each element is an expression: `[60, 62 + 12]` is `[60, 74]`. `,` between
two melodies concatenates them.

### 5. Transforming sequences
`@` binds tighter than every operator except `:`, so a transform after it
acts on the sequence. Read left to right: map, then transform.

| Expression | Notes per step | Operation |
|-|-|-|
| `sd@..!. ** 2` | `. . 38 . . . 38 .` | Repeat |
| `hh@3:8 >> 2` | `42 . 42 . . 42 . .` | Rotate right by 2 steps |
| `hh@3:8 << 1` | `. . 42 . . 42 . 42` | Rotate left by 1 step |
| `'(bd@!..)` | `. . 36` | Reverse |
| `[C4, E4]@!! + 12` | `72 76` | Transpose up |
| `[C4, E4]@!! - 12` | `48 52` | Transpose down |

`*` and `/` scale a sequence's time: `bd@!! * 2` lasts twice as long.

Repeating after `@` restarts the melody. Repeating the rhythm first
continues it:

```text
[C4, E4, G4]@!!!! ** 2      60 64 67 60  60 64 67 60
[C4, E4, G4]@(!!!! ** 2)    60 64 67 60  64 67 60 64
```

### 6. Bindings
`name = value` names a value. `;` separates expressions. The last one is
the program's result.

```
kick = bd@3:8;
snare = sd@2:8:2;
kick | snare ~> "drums"
```

Spaces are optional: `kick=bd@3:8;snare=sd@2:8:2;kick|snare~>"drums"`.

`=` is also how you build up a session in the REPL:

```
cane> kick = bd@3:8
cane> snare = sd@2:8:2
cane> kick | snare ~> "drums"
```

A later `=` with the same name replaces the earlier one for the lines that
follow.

### 7. Functions
`name params = body` defines a function. A call is the name followed by its
arguments, separated by spaces.

```
hit n = bd@n:8;
hit 3 | hit 5 >> 1 ~> "drums"
```

| Program | Result |
|-|-|
| `f x = x + 1; f 3` | `4` |
| `g x y = (x + y) / 2; g 4 8` | `6` |
| `f x = x + 1; f 3 + 1` | `5` |
| `g x y = (x + y) / 2; h = g 10; h 2` | `6` |
| `twice f x = f (f x); f x = x * 2; twice f 3` | `12` |
| `twice f x = f (f x); twice (* 2) 3` | `12` |
| `(10 -) 3` | `7` |
| `(+) 1 2` | `3` |

- A call binds tighter than any operator: `f 3 + 1` is `(f 3) + 1`, and
  `hit 3:8` is `(hit 3):8`. Write `hit (3:8)` to pass the rhythm.
- A call with fewer arguments returns a function, as `h` above.
- A function can take a function, as `twice` above.
- Builtin functions, as in Haskell:
  - `abs x`: absolute value. `abs (-3)` is 3; `abs -3` is `abs - 3`, an
    error.
  - `head m`: the first note of a melody, as a number.
  - `tail x`, `take n x`, `drop n x`: all but the first, the first `n`, all
    but the first `n`. These work on melodies, rhythms, sequences and
    patterns.
  - `cycle`: the number of loops played so far, from 0. It keeps counting
    when a new pattern replaces the old one. `bd@! + cycle` rises one
    semitone per loop.
  - `every k f x`, `sometimes f x`: see [Changing each
    cycle](#9-changing-each-cycle).
  - `scale name m`, `chord name`, `stack m s`: see [Scales and
    chords](#12-scales-and-chords).
- An operator in parentheses with one side missing is a function of that
  side: `(* 2)` doubles, `(10 -)` subtracts from 10, `(+)` adds two
  arguments. `(- a)` negates, as in Haskell; use `(+ -a)` or `\x -> num x - a`.
- A function cannot call itself.

### 8. Combining sequences
- `a | b` layers: `a` and `b` start at the same time.
- `a, b` concatenates: `b` starts when `a` ends.

```
bd@!., sd@!! ~> "drums"
```

```
time ms  0    500  1000 1500
         36   .    38   38
```

Layers of different lengths repeat until they realign, at the least common
multiple of their lengths. A 3-step and a 4-step layer loop every 12 steps:

```
bd@!.. | sd@!... ~> "drums"
```

```
step  1 2 3 4 5 6 7 8 9 10 11 12
bd    x . . x . . x . . x  .  .
sd    x . . . x . . . x .  .  .
```

A loop longer than 256 steps is an error.

### 9. Changing each cycle
Each loop of a pattern is a cycle, numbered from 0. These change a pattern
from one cycle to the next:

| Expression | Cycles 0, 1, 2, 3 |
|-|-|
| `<36, 38>@!` | 36, 38, 36, 38 |
| `<36, <38, 42> >@!` | 36, 38, 36, 42 |
| `every 3 (+ 12) (bd@!)` | 48, 36, 36, 48 |
| `sometimes (+ 12) (bd@!)` | 36 or 48, picked per cycle |
| `bd@! + cycle` | 36, 37, 38, 39 |

- `<a, b, c>` plays `a`, then `b`, then `c`, then `a` again. Elements can
  be any type, but all the same one: `<bd@3:8, bd@5:8>` alternates
  rhythms.
- A nested `<>` advances only when chosen, as in Tidal. `>>` is rotate, so
  close two with a space: `<a, <b, c> >`.
- `every k f x` applies `f` on cycles 0, `k`, `2k`, and so on.
- `sometimes f x` applies `f` on a random half of cycles. Like `?`, the
  picks are the same every time the program runs.
- Only the chosen branch is evaluated.

```
#! Kick alternates density each bar; the snare shifts every 4th bar.
kick  = <bd@3:8, bd@5:8>;
snare = every 4 (>> 1) (sd@....!...);
kick | snare | hh@!!!!!!!! ~> "drums"
```

### 10. Velocity and gate
`vel` sets velocity, 0 to 127. `gate` sets how much of each step sounds, in
percent: 50 is half a step, 200 holds into the next step. Defaults are 127
and 100.

| Expression | Per step |
|-|-|
| `hh@!!!! vel [127, 60]` | vel 127, 60, 127, 60 |
| `[C2, G2]@!!!! gate [25, 100]` | gate 25%, 100%, 25%, 100% |
| `bd@!! vel 90` | vel 90, 90 |

- A melody gives one value per step, cycling. A rest uses one up, as with
  `@`.
- They bind looser than `+` and tighter than `|`: `[C4]@!! + 12 vel 80 |
  sd@!` sets velocity on the first layer only. Values follow time order, so
  set them before layering.
- `gate` does not change the loop length. A note never sounds past the end
  of its loop.
- `(vel 60)` is a function, so it composes: `every 2 (vel 60) kick`.
- `vel` and `gate` are keywords, so they cannot be used as names.

```
#! Accented hats, short bass notes.
hats = hh@!!!!!!!! vel [127, 50, 80, 50];
bass = [C2, Eb2, G2]@!.!!.!!. gate 40;
(hats ~> "drums") | (bass ~> "303")
```

### 11. Polyrhythm and subdivision
`a fit n` stretches or squeezes `a` to last exactly `n` steps.

```
#! 3 kicks against 4 hats in one bar.
bd@!!! fit 4 | hh@!!!! ~> "drums"
```

```text
ms    0      500    667    1000   1333   1500
bd    x             x             x
hh    x      x             x             x
```

`fit 1` divides one step, so a triplet can sit inside a sequence:

```
#! Kick, a snare triplet, kick.
bd@!, sd@!!! fit 1, bd@! ~> "drums"
```

- `n` is 1 to 256.
- Times round to the microsecond, but the last note always ends exactly at
  `n` steps, so a fitted layer loops in whole steps.
- `fit` binds like `**`: `bd@!!! fit 4 | hh@!!!!` fits only the kicks.

### 12. Scales and chords
`scale name degrees` turns scale degrees into semitones. Add a root to
play them. `chord name` gives a chord's intervals. `stack intervals seq`
plays one copy of `seq` per interval, all at once.

| Expression | Result |
|-|-|
| `scale "minor" [0, 2, 4] + C4` | `[60, 63, 67]` |
| `scale "major" [7, -1]` | `[12, -1]` |
| `chord "maj" + C4` | `[60, 64, 67]` |

```
#! A minor line over block chords.
line   = scale "minor" [0, 2, 4, 6, 4, 2]@!!!!!! + A3;
chords = stack (chord "min") ([A2, D3]@!..!..);
line | chords ~> "303"
```

- Degrees past the end of a scale wrap by an octave: in a 7-note scale, 7
  is 12 and -1 is -1.
- Scales: `major`, `minor`, `dorian`, `phrygian`, `lydian`, `mixolydian`,
  `locrian`, `harmonicMinor`, `melodicMinor`, `majPent`, `minPent`,
  `whole`, `chromatic`.
- Chords: `maj`, `min`, `dim`, `aug`, `sus2`, `sus4`, `maj7`, `dom7`,
  `min7`.
- Names follow Tidal. Unknown names are an error that lists the valid ones.
- A melody of a chord plays as an arpeggio: `(chord "maj" + C4)@!!!`.
- `scale <"major", "minor"> [0, 2, 4]` changes the scale each cycle.

### 13. Comments
`#!` starts a comment that runs to the end of the line.

```
#! Four on the floor
bd@!!!! ~> "drums"
```

## Examples
Save one of these to a file and run `cane --watch FILE`. Edit and save to
re-evaluate.

**Breakbeat**
```
kick  = bd@!.....!. !.!.....;
snare = sd@....!... ....!..!;
hats  = hh@!!!!!!!! ** 2;
kick | snare | hats ~> "drums"
```

**Euclidean layers**
```
#! Three rhythms of different density over 16 steps.
low  = bd@5:16;
mid  = cp@3:16:2;
high = hh@7:16;
low | mid | high ~> "drums"
```

**Bassline**
```
#! Root, minor third, fifth, octave; the rests consume notes too.
notes = [C2, Eb2, G2, C3];
notes@!.!! .!!. !.!. !!.! ~> "303"
```

## Reference

### Literals
| Syntax | Meaning |
|-|-|
| `!`, `.` | Beat, rest (one step each) |
| `60` | Number (scalar) |
| `[60, 62]`, `[60]`, `[]` | Melody. A trailing `,` is allowed |
| `<a, b>` | One element per cycle. A trailing `,` is allowed |
| `C4`, `c#4`, `Bb3` | Note name, scientific pitch: `C4` is 60. Case insensitive. `C0` to `G9` |
| `"drums"` | String |
| `#! ...` | Comment to end of line |

### Operators
Lower rows bind tighter. Operators in the same row have equal precedence and
group left to right, so `3 * 4 / 8` is `(3 * 4) / 8`. `;` and `=` group right to
left.

| Operators | Operation |
|-|-|
| `a ; b` | Sequence expressions; result is `b` |
| `a ~> "name"` | Send to a named channel |
| `name = a` | Bind `a` to `name`; result is `a` |
| `f x y = a` | Define a function of `x` and `y` |
| `(op a)`, `(a op)`, `(op)` | Section: `op` as a function of the missing side |
| `a \| b` | Layer |
| `a , b` | Concatenate melodies, rhythms or sequences |
| `a or b`, `a xor b`, `a and b` | Step-wise logic on rhythms |
| `a ** n` | Repeat |
| `a fit n` | Rescale a sequence to exactly `n` steps |
| `a vel v`, `a gate g` | Velocity 0-127; percent of each step that sounds, 1-10000 |
| `a << n`, `a >> n` | Rotate left, right (sequences: by steps) |
| `~a`, `'a` | Invert rhythm, reverse |
| `a + b`, `a - b` | Add, subtract (sequences: transpose) |
| `a * b`, `a / b` | Multiply, divide (sequences: scale time) |
| `a @ b` | Map a note or melody onto a rhythm: `bd@3:8` |
| `a : b`, `a : b : n` | Euclidean rhythm: `3:8`; shifted left by `n`: `3:8:1` |
| `a lcm b`, `a gcd b` | Least common multiple, greatest common divisor |
| `a ? b` | Choose one of `a` and `b`, again each cycle |
| `-a` | Negate |
| `a++`, `a--` | Increment, decrement |
| `f a b` | Call: `(f a) b` |

The right side of `=`, and the body of `\x -> type a`, run to the next `;` or
`~>`.

Type rules for each operator are in [docs/types.md](docs/types.md). The grammar
is in [docs/syntax.ebnf](docs/syntax.ebnf).

### Drum names
Every session starts with these bindings: General MIDI drum notes, under the
short names [Strudel](https://strudel.cc/learn/samples/) uses. `=` can
rebind any of them.

| Name | Note | Sound |
|-|-|-|
| `bd` | 36 | Bass drum |
| `rim` | 37 | Rimshot (side stick) |
| `sd` | 38 | Snare |
| `cp` | 39 | Clap |
| `hh` | 42 | Closed hi-hat |
| `oh` | 46 | Open hi-hat |
| `lt` | 45 | Low tom |
| `mt` | 47 | Mid tom |
| `ht` | 50 | High tom |
| `cr` | 49 | Crash |
| `rd` | 51 | Ride |
| `tb` | 54 | Tambourine |
| `cb` | 56 | Cowbell |
| `sh` | 70 | Shaker (maracas) |

### Types
| Type | Annotation | Example |
|-|-|-|
| Scalar | `num` | `60` |
| String | `str` | `"drums"` |
| Rhythm | `rhy` | `!.!.` |
| Melody | `mel` | `[60, 62]` |
| Sequence | `seq` | `60@!.!.` |
| Pattern | `pat` | `60@!.!. ~> "drums"` |

Annotations are required in anonymous functions, e.g. `\n -> num n + 12`.
They are not yet checked.

## Design
`cane` compiles a program in passes:

1. Lex and parse into a syntax tree (`include/cane/lex.hpp`, `parse.hpp`).
2. Validate the tree's shape.
3. Resolve bindings: each identifier is replaced by the tree it names.
4. Resolve types: each operator is mapped to a typed operation.
5. Evaluate into a list of timed note events.

The passes are in `include/cane/passes.hpp`. REPL and file-watch state is in
`include/cane/session.hpp`.

### Rationale
> TODO

### Acknowledgements
- [Gwion](https://github.com/Gwion/Gwion)
- [Prop](https://pbat.ch/proj/prop.html)
- [fennecdjay](https://github.com/fennecdjay) (for inspiring the project)
- [qookei](https://github.com/qookei)

### Resources
See the list of resources [here](docs/resources.md)

### License
This project uses the AGPL-3.0 license. (check [LICENSE](LICENSE))

### Progress & Discussion
You can join the discord server in order to follow progress and/or contribute to discussion of the project: https://discord.gg/UVYAtpYYD2
