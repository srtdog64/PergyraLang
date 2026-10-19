# 2026-10-19: a result may lend storage of a `ref` input

Decided by: the Claude lane, applying the user's standing preference for
the option that is better long term and uses less memory (given for the
[String landing](2026-10-19_string_landing_copy_then_move.md) and the
[Clone depth](2026-10-19_struct_clone_is_deep.md) questions). The user can
overturn it.

## Rule

A routine may return storage of a `ref` input in any result type, not only
in a String or `Option<String>`. That result is a loan, and every caller
proves it with the existing returned-loan walk:

- a caller that returns the loan does so through a `ref` input of its own
  with the same escape;
- a caller that keeps the loan to itself needs an owner it leaves
  unchanged.

It covers `ref` Strings and borrow-tracked aggregates. Three kinds keep
their own rules:

- Handles, move-only values and subjects are not lent.
- A receiver (`self`) is not lent either: a method call is not a call row,
  so no caller would prove the loan. Its return stays refused where it
  returns.
- `ref` read-view inputs and Array results of non-`ref` inputs.

## Why lend before copying

- The canonical program builds view values over large borrowed text: the
  JSON fact tables hold the MIR document they index. A copy per view would
  duplicate that document for every table.
- Most fact structs built from borrowed facts are read by the caller and
  dropped. A loan costs nothing there.
- Where a loan cannot be proved (stored into a container that outlives the
  owner), the [copy landing](2026-10-19_string_landing_copy_then_move.md)
  still applies at that store.

## Consequence for cleanup

A value that borrows from a `ref` input is owned by that input's owner.
Cleanup must not drop it, or the parts of it that depend on the input.
The flow facts record that dependence per value (not per field), so a
partly fresh, partly borrowed value is not dropped at all. That leaks the
fresh part, which is safe. Cleanup synthesis must read this fact. It does
not exist yet.
