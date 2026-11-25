# Three-routine direct-MIR route deleted (2026-11-25)

Source: main `0f97b334d` plus this change. Trees: WSL `Ubuntu-C-WSL`
`/root/ts2` (drivers, sweep) and `/root/ts3` (gates). Drivers (test
profile, `-O0`): base `driver_gi4` (the `0f97b334d` source), change
`driver_dl1`. Slice 3 of the
[point 6 plan](../../agent_work_directives/review_improvements_and_sot_closure_2026-11-24.md).

## Before

The multi-routine dispatch sent a three-routine program the general route
declined to `CompileAdmittedDirectMirThreeRoutine`, which classified it into
one of six shape families (inferred generic scalar, constructed generic
member, constructed record-array member, Array argument, generic nominal
value flow, struct argument). After `0f97b334d` the route census showed it
running no corpus program: it received 35 and refused all of them.

## Change

- `direct_mir_multi_routine_projection_owner.pgy`: a program the general
  route declines is refused with the general route's receipt; the terminal
  dispatch owner is deleted.
- 101 files that nothing reaches after that edge is cut are deleted (11,484
  lines in all), with three functions of surviving files
  (`DirectMirArrayIntAssignmentTargetReady`, `DirectMirExactStringArrayCount`,
  `DirectMirLogInstructionEnvelopeReady`). `src/self_hosted/compiler/direct_mir`
  goes from 187 entries to 85.
- Gates: the six `one_mir_*` gates of the deleted families
  (`array_argument`, `constructed_array_member`, `constructed_generic_member`,
  `constructed_record_array_member`, `generic_struct_value_flow`,
  `inferred_generic_scalar`) with their mutation scripts, and
  `direct_mir_mutation_ownership_contract.sh` with its positive probe
  (`direct_mir_array_argument_mutation_ownership.pgy`, which imported the
  deleted Array-argument plan owner). None of them ran in a CI job, and with
  `wslpath` hidden all six `one_mir` gates failed on the base driver too.
  `one_mir_struct_argument_projection.sh`, which CI runs through
  `make self-host-replacement-frontier-installed-test-smoke`, stays: its
  fixture runs on the general route, and its owner cap list now names only
  the two owners that still exist. The inferred-scalar gate's control
  program, `tests/self_hosted/parity/fixture/generic_template_concrete_return.pgy`,
  moves into `direct_mir_generic_struct_instance_owner.sh`.
- Makefile targets, the component-contract caps, `OWNERS.md` entries and the
  word inventory follow.
- Registry: `projection.direct_mir_array_argument_program_identity` and
  `projection.direct_mir_array_argument_plan` (CLOSED) leave with their Rocq
  constructors, 24 derived carrier rows of deleted files go, deleted paths
  leave the consumer and gate lists of `mir.execution_graph`,
  `mir.generic_specialization`, `abi.layout_rows` and
  `abi.mir_array_int_layout_projection`, and the surviving Array<Int> graph
  fact carrier moves to `mir.execution_graph` (the legacy intent route still
  reads it). Counts: `CLOSED=61 BRIDGE=16 ACTIVE=2`, 79 authorities.

## Results

- The self-host compiler builds the driver without the deleted files.
- Sweep of 2,746 programs: equal to the base driver on both routes.
  Default LLVM: 887 equal to native, 0 wrong. Default C: 1,418 equal. The 35
  programs the route used to refuse are refused by the general route.
- Run with `wslpath` hidden: the 19 push-step gates that reach direct MIR,
  the record-field, record-array literal and generic instance gates, and the
  scripts of `make self-host-replacement-frontier-installed-test-smoke`
  (`one_mir_struct_argument_projection.sh`, `role_override_mir_replacement.sh`,
  callable-parameter identity, region user callee, lowercase entrypoint,
  callable-parameter installed) PASS. The last two need the driver installed
  beside `bin/pgy`, as in CI.
- Component contract, language word registry, SoT authority edge and
  adequacy (79 authorities, 144 carriers, `CLOSED=61 BRIDGE=16 ACTIVE=2`)
  and gate reachability (886 scripts, 0 undeclared, 0 stale) PASS.
- Codegen bootstrap: gen2 == gen3 (125,935 lines). The front end is
  unchanged.

## Correction

The [record field](2026-11-25_record_field_array_abi.md) record says the
seven `one_mir_*` gates of the route "run in no CI job and stop at their
native/self MIR comparison on main". `one_mir_struct_argument_projection.sh`
runs in CI (above) and passes there. Locally it failed only because WSL's
`wslpath` turned the repository root into a `\\wsl.localhost` path in
`pgy_selfhost_root_forward_slash`, so the native oracle's `/root/...` source
paths no longer matched. With `wslpath` hidden
(`PATH=/root/nowsl-bin:/usr/local/bin`) it passes. The other six fail with
`wslpath` hidden as well. The same record's count of failing
`tests/self_hosted/parity/direct_mir` scripts was measured with `wslpath`
visible. A separate audit re-measures them with it neutralised.

The negative half of the deleted mutation-ownership gate already failed
before this change: native accepts
`tests/self_hosted/fixtures/borrowed_ref_constructor_store_reject.pgy`,
which that gate expected to be refused ("cannot escape through constructor
field store"). The fixture stays in the tree as the reproduction.
