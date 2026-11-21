# Literal Log and declaration erasure route deleted; the general route takes a lowercase main (2026-11-21)

Source: main `71e3c60a9` plus this change. Trees: WSL `Ubuntu-C-WSL`
`/root/ts2` (drivers) and `/root/ts3` (gates); release-profile test builds.

## Before

A one-routine program with a literal `Log` body or only compile-time
declarations went to the compile-time declaration erasure / literal Log
route. In the route census
([record](2026-11-21_role_operator_two_routine_routes_deleted.md)) it served
10 sweep programs, 2 runtime-equal; with it off, the general route ran those
2 and 3 more equal. The route was also the only direct-MIR path for a
lowercase `func main()`: the general route found its entry routine by the
name `Main` (routine instance inventory, route admission, the scalar CFG
routine admission, identity query and partition facts), so
`lowercase_entrypoint_installed_self_host_owner.sh` failed on direct LLVM
("generic routine inventory is missing Main").

## Change

- The routine instance inventory selects the entry with the entrypoint
  selection policy (`Main`, or a unique lowercase `main` when no `Main`
  exists) and seals it at row 0; route admission reads row 0 as the entry,
  and the other checks ask the policy instead of comparing with `"Main"`.
  The entry is emitted as C `main` / LLVM `@main` as before.
- The one-routine dispatch is the general route alone: the erasure /
  literal Log branch and its runtime-declaration bypass are removed. Three
  owners (erasure, literal Log plan and emission) and the functions only
  they reached are deleted, with their stale `one_mir` gate, make target,
  workflow entry and contract family block.
- Registry: the CLOSED rows `projection.direct_mir_compile_time_declaration_erasure`
  and `projection.direct_mir_literal_log_plan` and their facts in
  `SoTAuthority.v` are deleted (CLOSED=60 BRIDGE=19 ACTIVE=2).

## Results

- Default-LLVM sweep, 1,398 programs, against the previous candidate
  (nominal literal route deleted): equal 526 → 529, refused 426 → 423.
  The three `src/self_hosted/codegen/fixture/*_field_declaration.pgy`
  programs the literal Log route refused now run equal on the general
  route; no equal program was lost.
- `lowercase_entrypoint_installed_self_host_owner.sh` PASS: source-to-C,
  direct and public LLVM run `func main()`, and MIR with the entry renamed
  `worker` is refused with "generic routine inventory has no unique
  entrypoint" and no artifact (the gate's expected message moves from the
  deleted route's).
- In `/root/ts3` with the change's driver installed: the 44 self-host gates
  of `scripts/ci_push_linux_steps.sh`, the intent LLVM, scalar multi-routine
  and lowercase entrypoint gates, the component contract,
  `sot_authority_edge_smoke.sh` and `gate_script_reachability_smoke.py`:
  51 PASS.

`git diff --shortstat`: 21 files, 34 insertions, 953 deletions. The
`direct_mir` files under `src/` go from 819 / 75,479 lines to 816 / 74,803.
