# TODO

## Critical

## High

- [ ] Ctrl+C in file mode exits without releasing sounding notes, so they hang on the synth.

- [ ] Syntax errors at end of input print no location. `1 +` reports "expected a primary expression or a prefix operator" with no line. Report the last line, without a column.

- [ ] `--bpm` and channel names from the command line (e.g. `drums=10`) or a config file. Both are fixed in `src/cane.cpp`: 120 BPM, `drums` 10, `303` 12.

## Medium

- [ ] The type annotation in `\x -> T a` is parsed and dropped, not checked. A wrong annotation passes silently.

- [ ] Export a program's events to a MIDI file. `Schedule` already holds timed messages.

- [ ] Follow an external clock: MIDI start/stop plus 24 clock pulses per quarter note. Depends on `--bpm`.

- [ ] A print operator that shows a value mid-expression, without changing it.

- [ ] Per-expression eval tracing: print each node's source as it evaluates. For operators, print the source between the operands too.

- [ ] Recursion: a function body cannot name the function.

- [ ] `**`, `<<`, `>>`, `'`, `+`, `-` on patterns. Sequences have them; patterns have no type rule.

- [ ] Random choice `?` over melodies, rhythms, sequences and patterns. It takes scalars only.

- [ ] `,` on patterns concatenates whole patterns. The plan concatenated them pair-wise, per layer.

- [ ] `--seed` and `--cycles`. See `docs/dev/cycles.md`.

## Low

- [ ] A parent node's `sv` should span all its children, so a statement's `sv` covers the whole statement.

- [ ] Accept a unique prefix of a keyword as shorthand.
