# cane (v2)
> The beatings will continue until a banger is made

Cane is a programming language for making music. You write rhythms and
melodies as short expressions and send them to MIDI channels.

![C++](https://badgen.net/static/C++/26/orange)
[![license](https://badgen.net/github/license/tarpit-collective/cane)](./LICENSE)
[![discord](https://badgen.net/discord/members/UVYAtpYYD2)](https://discord.gg/UVYAtpYYD2)

```
#! Kick on a 3-in-8 euclidean rhythm, snare on 3 and 7, closed hats on every step.
kick  = bd@3:8;
snare = sd@..!.**2;
hats  = hh@8:8;
kick $ snare $ hats ~> "drums"
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
- Not yet: following an external MIDI clock, and calling functions (they
  parse and type-check, but evaluation fails).
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
$ git clone https://github.com/tarpit-collective/cane.git && cd cane/
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

The current pattern loops at 120 BPM. A new pattern from the REPL or a saved
file starts at the end of the current loop. A value that is not a pattern,
e.g. `kick = bd@3:8`, does not change what plays. On exit, `cane`
releases every sounding note.

Events with no channel, e.g. a sequence layered into a pattern without
`~>`, and notes above 127 are not sent.

#### Output
Each program's value is printed: one line per event for a pattern, and the
value itself for anything else.

```text
     0 ms  ch 10  note  36  beat
   500 ms  ch 10  note  36  rest
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
`,` joins numbers into a melody. Each step takes the next note, and the
melody repeats until every step has one.

```
(C4, D4, E4)@!!!! + 12 ~> "303"
```

```
step  1  2  3  4
      72 74 76 72
```

Notes are MIDI numbers (`60`) or names in scientific pitch (`C4` is 60).
Names are case insensitive and take `#` or `b`: `c#4` is 61, `Bb3` is 58.

A rest also uses up a note:

```
(C2, Eb2, G2)@!.!! !.!. + 12 ~> "303"
```

```
step  1  2  3  4  5  6  7  8
      48 .  55 48 51 .  48 .
```

Melody operators:

| Expression | Result |
|-|-|
| `(60, 62, 64) + 12` | `72, 74, 76` |
| `'(60, 62, 64)` | `64, 62, 60` |
| `(60, 62, 64) << 1` | `62, 64, 60` |
| `(60, 62) ** 2` | `60, 62, 60, 62` |
| `head (60, 62, 64)` | `60` |
| `tail (60, 62, 64)` | `62, 64` |

Use parentheses around a melody. `,` binds more loosely than the arithmetic
operators, so `60, 62 + 12` means `60, 74`.

### 5. Transforming sequences
`@` binds tighter than every operator except `:`, so a transform after it
acts on the sequence. Read left to right: map, then transform.

| Expression | Notes per step | Operation |
|-|-|-|
| `sd@..!. ** 2` | `. . 38 . . . 38 .` | Repeat |
| `hh@3:8 >> 2` | `42 . 42 . . 42 . .` | Rotate right by 2 steps |
| `hh@3:8 << 1` | `. . 42 . . 42 . 42` | Rotate left by 1 step |
| `'(bd@!..)` | `. . 36` | Reverse |
| `(C4, E4)@!! + 12` | `72 76` | Transpose up |
| `(C4, E4)@!! - 12` | `48 52` | Transpose down |

`*` and `/` scale a sequence's time: `bd@!! * 2` lasts twice as long.

Repeating after `@` restarts the melody. Repeating the rhythm first
continues it:

```text
(C4, E4, G4)@!!!! ** 2      60 64 67 60  60 64 67 60
(C4, E4, G4)@(!!!! ** 2)    60 64 67 60  64 67 60 64
```

### 6. Bindings
`name = value` names a value. `;` separates expressions. The last one is
the program's result.

```
kick = bd@3:8;
snare = sd@..!. ** 2;
kick $ snare ~> "drums"
```

Spaces are optional: `kick=bd@3:8;snare=sd@..!.**2;kick$snare~>"drums"`.

`=` is also how you build up a session in the REPL:

```
cane> kick = bd@3:8
cane> snare = sd@..!. ** 2
cane> kick $ snare ~> "drums"
```

A later `=` with the same name replaces the earlier one for the lines that
follow.

### 7. Combining sequences
- `a $ b` layers: `a` and `b` start at the same time.
- `a, b` concatenates: `b` starts when `a` ends.

```
bd@!., sd@!! ~> "drums"
```

```
time ms  0    500  1000
         36   38   38
```

### 8. Comments
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
kick $ snare $ hats ~> "drums"
```

**Euclidean layers**
```
#! Three rhythms of different density over 16 steps.
low  = bd@5:16;
mid  = cp@3:16 >> 2;
high = hh@7:16;
low $ mid $ high ~> "drums"
```

**Bassline**
```
#! Root, minor third, fifth, octave; the rests consume notes too.
notes = (C2, Eb2, G2, C3);
notes@!.!! .!!. !.!. !!.! ~> "303"
```

## Reference

### Literals
| Syntax | Meaning |
|-|-|
| `!`, `.` | Beat, rest (one step each) |
| `60` | Number (scalar) |
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
| `a $ b` | Layer |
| `a , b` | Concatenate |
| `f a` | Call (not yet evaluated) |
| `a or b`, `a xor b`, `a and b` | Step-wise logic on rhythms |
| `a ** n` | Repeat |
| `a << n`, `a >> n` | Rotate left, right (sequences: by steps) |
| `~a`, `'a` | Invert rhythm, reverse |
| `a + b`, `a - b` | Add, subtract (sequences: transpose) |
| `a * b`, `a / b` | Multiply, divide (sequences: scale time) |
| `a @ b` | Map a note or melody onto a rhythm: `bd@3:8` |
| `a : b` | Euclidean rhythm: `3:8` |
| `a lcm b`, `a gcd b` | Least common multiple, greatest common divisor |
| `a ? b` | Choose one of `a` and `b` |
| `head a`, `tail a` | First element, all but the first |
| `+a`, `-a` | Absolute value, negate |
| `a++`, `a--` | Increment, decrement |
| `&a` | Coerce a number to a melody |

The value bound by `x = a`, and the body of a function `\x -> type a`, run to
the next `;` or `~>`.

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
| Melody | `mel` | `60, 62` |
| Sequence | `seq` | `60@!.!.` |
| Pattern | `pat` | `60@!.!. ~> "drums"` |

Annotations are used in function definitions, e.g. `\n -> num n + 12`.

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
