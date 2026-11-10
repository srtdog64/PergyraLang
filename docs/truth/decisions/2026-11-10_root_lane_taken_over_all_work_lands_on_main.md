# The root lane is taken over; all preserved work lands on main (2026-11-10)

Decided by the user on 2026-11-10.

- The GPT root lane stops. Claude takes over main, the bootstrap, the
  installed driver and required CI.
- Everything the GPT lane had preserved lands on main: the staged V.001
  index and the unstaged later-milestone work (the 2026-11-08 binding rule,
  compound assignment, Raw/View and owner views). The earlier plan to keep
  the later milestone out of the V.001 commit is withdrawn.
- The goal is required CI green on a published commit. Failures are fixed
  in dependency order and each verified batch is pushed.
- The fixed-point job does not get a larger `timeout-minutes`. Its time is
  brought down by making the native landing analysis faster. If the
  achievable speed-up is not enough, the measured numbers are reported
  before anything else is tried.

This replaces the lane assignment in
[2026-10-16_main_only_integration](2026-10-16_main_only_integration.md) and
the "next milestone must not enter the V.001 commit" line of the former
`state/main.md`. The ownership direction and the explicit-`Clone` allowance
of [2026-10-25](2026-10-25_real_sharing_site_may_take_explicit_clone.md) are
unchanged.

The re-evaluation that
[2026-10-25_ownership_direction_reevaluated_2026-11-08](2026-10-25_ownership_direction_reevaluated_2026-11-08.md)
set for 2026-11-08 was not written by the GPT lane and is still owed.
