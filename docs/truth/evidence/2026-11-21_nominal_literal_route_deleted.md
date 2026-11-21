# Nominal literal route deleted; the general route builds subjects and vessels (2026-11-21)

Source: main `6caa7552a` plus this change. Trees: WSL `Ubuntu-C-WSL`
`/root/ts2` (drivers) and `/root/ts3` (gates); release-profile test builds.

## Before

The one-routine dispatch sent every program with a nominal literal to the
nominal literal route first. In the route census
([record](2026-11-21_role_operator_two_routine_routes_deleted.md)) it served
three sweep programs, two runtime-equal (`nominal_subject.pgy`,
`nominal_vessel.pgy`). With that route off, the general route refused both
at `stage=named-record-literal`: the named literal resolved only value
records, not the identity cells of a `subject` or `vessel`. The route also
claimed the constructor call `Hero(7)` and refused it ("nominal literal
graph topology is invalid"), although the general route builds that call
as an identity cell.

## Change

- `direct_mir_scalar_program_named_record_literal_owner.pgy` resolves a
  named literal's type as a value record or an identity cell (exactly one),
  takes the field ordinal from that row, and projects to the record or
  identity-cell constructor, the row the call form resolves.
- The one-routine dispatch counts identity cells as runtime declarations,
  so a subject or vessel program reaches the general route instead of
  compile-time declaration erasure, which refused its declaration schema.
- The nominal literal branch is removed; the owners only it reached go: 12
  files (the nominal literal family and the mutable nominal identity
  family) and one mir_lower function. Their two `one_mir` gates (already
  failing on main, run by no push or CI job), make targets, workflow
  entries, both contracts' cap blocks and the registry carriers and gate
  entries go too.
- New gate `direct_mir_general_route_nominal_literal_owner.sh` (push core
  step): a named subject, a named vessel, the `Hero(7)` call and a literal
  with fields out of declaration order run on native C/LLVM, default LLVM
  and the general emitter's C with equal output and clean under ASan; a
  forged MIR literal field the declaration lacks is refused at the
  named-record-literal stage with no C.

`git diff --shortstat`: 24 files, 257 insertions, 1,554 deletions. The
`direct_mir` files under `src/` go from 831 / 76,570 lines to 819 / 75,479.

## Results

- Default-LLVM sweep, 1,398 programs, against the previous candidate
  (slice A): equal 526 → 526. `nominal_subject.pgy` and
  `nominal_vessel.pgy` stay equal, now on the general route; the other
  moves are the element-release re-land's 3 `*_landed_valid` programs.
- The new gate PASS on the change's driver; on the driver before it, it
  fails at `called_subject` (`Hero(7)` refused).
- In `/root/ts3` with the change's driver installed: the 44 self-host gates
  of `scripts/ci_push_linux_steps.sh`, the intent LLVM, scalar
  multi-routine and lowercase entrypoint gates, the component contract,
  `sot_authority_edge_smoke.sh` and `gate_script_reachability_smoke.py`:
  51 PASS.
