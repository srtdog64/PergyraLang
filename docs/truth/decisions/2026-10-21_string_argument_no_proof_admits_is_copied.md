# 2026-10-21: a String argument no proof admits is copied at the call

Decided by: the Claude lane. It extends the rule the user chose for the
[String landing](2026-10-19_string_landing_copy_then_move.md): copy where
the proof fails, the long-term choice the user asked for. The user can
overturn it.

## Rule

A String element of an Array goes to a declared callee as an argument. If
no receipt admits it, the next analysis passes `Clone(argument)`, visible
in MIR. A receipt admits it when the callee only reads the String and
every other argument is read-only. It is refused when:

- the callee keeps or returns the String;
- another argument is `inout` or `own`;
- the callee's summary is not final.

In any of those cases the callee then gets a String of its own, and the
caller's Array is left as it was.

The refusal of the analysis that selects the copy stays as it was. A copy
of a String is a new String on both backends, so whatever the callee does
with it is safe.

## Why

- On the canonical driver input, these were the "String element cannot
  cross an opaque boundary" refusals at declared calls. Most of them pass
  an element beside an `inout` output that the callee writes, which makes
  the receipt fail even when the callee only reads the String. They fall
  from 171 to 86, and the total from 1431 to 1297
  ([census record](../evidence/2026-10-21_native_census_string_argument_copied.md)).
- It is the same place and reason as the String store landing: the
  element would be used where its Array's owner is not proved to keep it.

## Cost on the record

One String copy per such call, each time it runs. Emitted code frees no
String yet, so each copy stays allocated until the process ends.

Replaces: nothing.
