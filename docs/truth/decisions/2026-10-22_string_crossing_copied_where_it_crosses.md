# 2026-10-22: a String that leaves its Array owner where no copy operation stands is copied there

Decided by: the Claude lane. It extends
[an unprovable String store lands as an owned copy](2026-10-19_string_landing_copy_then_move.md)
from builtin stores to every place the String owner refuses, as
[a String argument no proof admits](2026-10-21_string_argument_no_proof_admits_is_copied.md)
did for declared calls. The user can overturn it.

## Rule

The type checker refuses a String borrowed from an `Array<String>`
element where it is kept past the Array: passed to a struct or enum
constructor, returned, placed in an Array or tuple literal, assigned
into another value's field or element, written to a slot, or held by any
other place with no non-retention proof ("cannot cross an opaque
boundary", "cannot be retained beyond its Array owner without a copy").
The refusal stands in this analysis, and the next analysis takes the
String expression there as `Clone(expression)`: an owned copy, which
`Clone` makes of any String. Builtin stores keep their own owned-copy
operation (`ArrayPushOwnedString`) where one exists.

## Why

On the canonical driver input 86 refusals were such crossings, mostly
String fields handed to fact constructors (`MirAbiLayoutRowCapture`,
`SelfMirRoutineBuild`, `SelfDirIntentStep...`) and Strings returned from
routines whose local Array owns them. A copy is the contract that covers
each such site; a move needs a proof that the element is never read
again, which these sites do not give.

## Cost on the record

One String copy per execution of each landed site. As with the other
landings it stays allocated until the process ends while emitted code
frees no String.

Replaces: nothing.
