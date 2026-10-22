# Types

## Primitives
| Type | Annotation | Values |
|-|-|-|
| Scalar | `num`, `number` | Integers, e.g. `60`, or note names, e.g. `C4` |
| String | `str`, `string` | `"drums"` |
| Rhythm | `rhy`, `rhythm` | Steps: `!` beat, `.` rest |
| Melody | `mel`, `melody` | List of scalars (MIDI notes) |
| Sequence | `seq`, `sequence` | Timed note events, not yet sent |
| Pattern | `pat`, `pattern` | A sequence sent to a channel |

A program must evaluate to a pattern.

## Operator Types
The type checker accepts exactly these combinations. Any other is a type
error. Generated from `CANE_TYPE_REMAP` in `include/cane/passes.hpp`.

| Operator | `a` | `b` | Result |
|-|-|-|-|
| `+a` | scalar |  | scalar |
| `-a` | scalar |  | scalar |
| `~a` | rhythm |  | rhythm |
| `'a` | rhythm |  | rhythm |
| `'a` | melody |  | melody |
| `a++` | scalar |  | scalar |
| `a--` | scalar |  | scalar |
| `&a` | scalar |  | melody |
| `&a` | melody |  | melody |
| `a + b` | scalar | scalar | scalar |
| `a - b` | scalar | scalar | scalar |
| `a * b` | scalar | scalar | scalar |
| `a / b` | scalar | scalar | scalar |
| `a << b` | scalar | scalar | scalar |
| `a >> b` | scalar | scalar | scalar |
| `a lcm b` | scalar | scalar | scalar |
| `a gcd b` | scalar | scalar | scalar |
| `a : b` | scalar | scalar | rhythm |
| `a , b` | scalar | scalar | melody |
| `a ? b` | scalar | scalar | scalar |
| `a @ b` | melody | rhythm | sequence |
| `a @ b` | scalar | rhythm | sequence |
| `a << b` | melody | scalar | melody |
| `a >> b` | melody | scalar | melody |
| `a + b` | melody | scalar | melody |
| `a - b` | melody | scalar | melody |
| `a * b` | melody | scalar | melody |
| `a / b` | melody | scalar | melody |
| `a + b` | melody | melody | melody |
| `a - b` | melody | melody | melody |
| `a * b` | melody | melody | melody |
| `a / b` | melody | melody | melody |
| `a ** b` | melody | scalar | melody |
| `a , b` | melody | melody | melody |
| `a , b` | melody | scalar | melody |
| `a , b` | scalar | melody | melody |
| `head a` | melody |  | melody |
| `tail a` | melody |  | melody |
| `a << b` | rhythm | scalar | rhythm |
| `a >> b` | rhythm | scalar | rhythm |
| `a ** b` | rhythm | scalar | rhythm |
| `a , b` | rhythm | rhythm | rhythm |
| `a or b` | rhythm | rhythm | rhythm |
| `a xor b` | rhythm | rhythm | rhythm |
| `a and b` | rhythm | rhythm | rhythm |
| `head a` | rhythm |  | rhythm |
| `tail a` | rhythm |  | rhythm |
| `a , b` | sequence | sequence | sequence |
| `a $ b` | sequence | sequence | sequence |
| `head a` | sequence |  | sequence |
| `tail a` | sequence |  | sequence |
| `a * b` | sequence | scalar | sequence |
| `a / b` | sequence | scalar | sequence |
| `a + b` | sequence | scalar | sequence |
| `a - b` | sequence | scalar | sequence |
| `a ** b` | sequence | scalar | sequence |
| `a << b` | sequence | scalar | sequence |
| `a >> b` | sequence | scalar | sequence |
| `'a` | sequence |  | sequence |
| `a ~> b` | sequence | string | pattern |
| `head a` | pattern |  | pattern |
| `tail a` | pattern |  | pattern |
| `a $ b` | pattern | pattern | pattern |
| `a $ b` | pattern | sequence | pattern |
| `a $ b` | sequence | pattern | pattern |
| `a , b` | pattern | pattern | pattern |
| `a , b` | pattern | sequence | pattern |
| `a , b` | sequence | pattern | pattern |
| `a ~> b` | pattern | string | pattern |

`x = a` has the type of `a`. A function `\x -> T a` takes a parameter of
type `T`.
