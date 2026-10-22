# Design notes

The original design plan, and where the implementation now differs.
User-facing docs: `README.md`, `docs/types.md`, `docs/syntax.ebnf`.

## Architecture
The plan: one thread sequences the current loop; another compiles new
expressions. Buffers swap at the end of a loop.

Implemented:
- Main thread: REPL, or file mode. Compiles each line via `Session::eval`
  (`include/cane/session.hpp`).
- Player thread: loops the current `Schedule` (`include/cane/midi.hpp`).
- Watcher thread (`--watch` only): polls the file every 100 ms and
  evaluates it on change.

Hand-off uses one queued slot, not two buffers. `Player::play` stores the
next schedule. The player swaps it in at the end of the current loop. A
second `play` before then replaces the first, so only the latest pattern
plays. On stop, the player releases every sounding note.

The planned "instructions" encoding is `Schedule`: MIDI messages with time
offsets, plus the loop length. Note-offs sort before note-ons at the same
time.

## User interface
| Plan | Now |
|-|-|
| One MIDI channel per instance, `--channel` | Named channels in one program (`~> "drums"`). The name-to-channel map is fixed in `src/cane.cpp`: `drums` 10, `303` 12 |
| `--device NAME` | `--port NAME` (exact or unique substring), `--virtual NAME`. Default: a virtual port `cane` |
| `--clock NAME`: follow an external clock; no internal clock | Internal clock only, fixed at 120 BPM |

Open:
- `--bpm`, then external clock: MIDI start/stop plus 24 clock pulses per
  quarter note.
- Channel names from the command line or a config file.
- Ctrl+C in file mode exits without releasing sounding notes.
- Each compiler pass prints debug logs to stdout.

## Types
The plan's five types exist, plus `String` for channel names. Typing is
static. Operators are overloaded per type; there are no implicit
coercions. `&` converts a scalar to a melody explicitly. The accepted
combinations are in `docs/types.md`.

## Syntax: plan vs now
| Plan | Now |
|-|-|
| `x => name`: bind and evaluate to `x` | `name = x; ...` |
| `{ a b c }`: random choice | `a ? b`, scalars only |
| `<`, `>`: rotate | `<<`, `>>` |
| `a b`: implicit concatenation | `a , b`. Whitespace inside a rhythm is ignored, so `!.!! !.!.` is 8 steps |
| `[ a b ]`: layer sequences into a pattern | `a $ b` layers; `~> "name"` makes a pattern |
| `melody @ rhythm`, `rhythm @ melody` | `note@rhythm` only, e.g. `bd@3:8` |
| `seq * n`, `seq / n`: scale time relative to the parent | Scales timestamps and durations by `n`, in absolute time |

## Not yet implemented
- Function calls: they parse and type-check, but evaluation fails.
- `**`, `<<`, `>>`, `'`, `+`, `-` on patterns: no type rule. Sequences have them.
- Random choice over melodies, rhythms, sequences and patterns.
- `,` on patterns concatenates whole patterns. The plan concatenated them
  pair-wise, per layer.
- `a , b` on sequences starts `b` at the last start time of `a`, not its
  end. See `looping.md`.
- Polymeter and polyrhythm. See `looping.md`.

## Decisions
- `=` replaced `let`. `@` takes the note first: `kick=bd@3:8`. It binds
  tighter than everything except `:`, so transforms after it act on the
  sequence (`sd@..!.**2`). Sequence `**`, `<<`, `>>`, `'`, `+`, `-` were
  added for that. A brief `bd:3:8` form was dropped: rhythms are values, so
  `bd:groove` or `bd:3` could not tell note from hits.
- Note names have no octave -1 (MIDI 0-11). `C-1` would make `e-1` (one-letter
  binding minus 1) silently lex as a note. If needed, spell it `C_1`.
