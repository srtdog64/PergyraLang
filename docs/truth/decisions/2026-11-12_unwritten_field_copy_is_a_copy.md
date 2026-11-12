# 2026-11-12: a grown field copy never assigned back is a copy

Decided by: the Claude lane. The user, asked
[the open question](2026-11-12_open_unwritten_field_copy_copy_or_write_back.md),
answered "the approach that is better in the long run" and left the choice
to the lane. The user can overturn it.

## Rule

`var x = root.field` makes `x` a copy. A change to `x` reaches
`root.field` only through an assignment the program writes, such as
`root.field = x`. Neither front end writes `x` back on its own. Where the
program reads `root.field` again after `x` grew, the copy is made where `x`
was taken (`Clone`), as
[the 2026-10-22 rule](2026-10-22_inout_place_changed_through_a_copy.md)
already does natively; a front end that cannot make that copy refuses the
program.

The self-host focus recovery that writes a part back at an exit is
therefore withdrawn. A focus group whose every exit already assigns the
part back keeps its admission, since then no write-back is added.

## Why

- A `var` that is a copy is what the `let`/`var` decision of 2026-11-08
  chose, and the copy-where-the-proof-fails rules for arguments, loans and
  local copies give the same answer at every site.
- An added write-back is a write the source does not contain. On a failure
  exit it publishes a half-built field to the caller.
- The cost of the copy is in MIR and can be removed: assigning the part
  back on every exit makes it a move, and a later pass may turn a copy whose
  original is dead into a move without changing any result. Programs that
  depend on an added write-back could not be changed back.

Replaces: nothing. Answers the 2026-11-12 OPEN question.
