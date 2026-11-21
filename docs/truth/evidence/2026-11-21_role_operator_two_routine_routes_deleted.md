# Role operator and two-routine nominal routes deleted (2026-11-21)

Source: main `c371e2b49` plus this change. Trees: WSL `Ubuntu-C-WSL`
`/root/ts2` (drivers) and `/root/ts3` (gates). Drivers are release-profile
test builds (`self_host_compiler_build.sh`, `PGY_SELFHOST_CC_PROFILE=release`).

## Census

A scratch driver logged the direct-MIR route of every program of the
default-LLVM sweep (1,398 native-admitted programs, not committed). A second
driver switched the remaining shape routes off. Per route, programs served
and their result:

| Route | Programs | Equal | With the route off |
|---|---|---|---|
| role operator | 1 | 0 | refused either way |
| two-routine nominal | 8 | 0 | refused either way |
| legacy / nested intent | 0 | — | — |
| composite intent | 1 | 1 | refused by the general route |
| three-routine | 62 | 1 | 1 equal lost |
| nominal literal | 3 | 2 | 2 equal lost |
| literal log | 10 | 2 | 5 equal (+3) |

The role operator and two-routine nominal routes served no program
runtime-equal, so they go first. The general route refuses the programs the
others still serve: a named record literal, a lowercase `main` entry, a
generic routine returning a non-scalar, and the intent programs.

## Change

- `direct_mir_multi_routine_projection_owner.pgy` loses the role operator
  and two-routine branches (closed nominal, inferred generic nominal and
  member). The owners only they reached are deleted: 47 files, among them
  the struct and Option-struct value-flow families and the inferred generic
  member declaration owner, whose one remaining function moves to its only
  caller.
- Gates of the deleted routes: `one_mir_{role_operator,inferred_generic_member,inferred_generic_nominal,struct_value_flow,option_struct_value_flow}_{projection.sh,mutations.py}`,
  their make targets and parity-workflow entries. They ran in no push or CI
  job; on the main driver they already failed (missing `.exe` driver name,
  native/self semantic drift, owner caps over their limits).
- `role_receiver_canonicalization_owner.sh`, which only the role operator
  gate called, is listed in the manual inventory: on the main driver it
  fails at oracle MIR canonicalization (`call_handoff_missing`).
- Registry: 11 carrier rows and the deleted owners in last-consumer lists
  are removed; the role operator gate leaves the gate column of
  `semantic.callable_receiver_carriage`. No row changes status.

`git diff --shortstat`: 70 files, 29 insertions, 8,563 deletions. The
`direct_mir` files under `src/` go from 878 / 83,046 lines to 831 / 76,570.

## Results

- Default-LLVM sweep, 1,398 programs: equal 526, compile-fail 439,
  refused 423, native-refused 10, the same counts as the `3ca35ae4a` sweep.
- In `/root/ts3` with the change's driver installed: the 42 self-host gates
  of `scripts/ci_push_linux_steps.sh`, the legacy and composite intent LLVM
  gates, `direct_mir_scalar_multi_routine_owner.sh`,
  `lowercase_entrypoint_installed_self_host_owner.sh`, the component
  contract, `sot_authority_edge_smoke.sh` and
  `gate_script_reachability_smoke.py`: 49 PASS.
