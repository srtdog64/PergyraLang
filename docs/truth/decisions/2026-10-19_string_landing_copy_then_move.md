# 2026-10-19: an unprovable String store lands as an owned copy

Decided by: the Claude lane, after the user asked for "whichever is better
long term" on the [open question](2026-10-18_open_string_push_landing.md).

## Rule

Where the proof route cannot admit a String store (a local Array's
element goes into a value that outlives the Array), the compiler selects
the owned-copy operation at that site: an `ArrayPush` becomes
`ArrayPushOwnedString`, with that operation's receiver admission and
ownership effect. The selection is visible in MIR. Sites a proof admits
are unchanged and copy nothing.

## Why copy before move

- A move needs proof that the moved slot is never read again. Many sites
  read the source afterwards, so a move-only rule keeps refusing them;
  the copy is the contract that covers every such site.
- The copy needs no new runtime operation; both backends already emit
  `pgy_array_push_owned_String`.
- A move can replace a copy later, site by site, where a measurement shows
  the allocation matters. That is an optimization of this rule, not a
  different rule.

Replaces: the OPEN question of 2026-10-18.
