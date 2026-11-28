# `semantic.callable_receiver_carriage` and `air.evidence_graph` closed

Observed 2026-11-29 KST on main `92da9b339` plus this change, in WSL
`Ubuntu-C-WSL` tree `/root/ts4` with the integrated test-profile driver
(`ts4-drv-int2`, built from `95c002c21`). The rows close by the user's
scope decisions of the same day
([receiver](../decisions/2026-11-29_receiver_carriage_closes_by_scope.md),
[AIR](../decisions/2026-11-29_air_evidence_graph_closes_by_scope.md)).

## What the decisions rest on

- Native MIR derives each routine's receiver carriage from the declaration
  header's `uses_pointer_self` (`mir_signature_metadata.c`). The MIR
  validator refuses a routine whose carriage disagrees with its owner
  (`mir_program_fact_validate.c`). The native unit test "MIR validator
  rejects pointer-self ABI metadata drift" mutates the header and expects
  that refusal; it runs in `test-mir` under `test-all`.
- `driver_app.c` synthesizes and verifies AIR and hands its verification to
  the C and LLVM projections. It then destroys DIR, AIR, MIR, RIR and HIR in
  that order. `air_destroy` frees only AIR-owned arrays and copied names.

## Gates

| Gate | Before | Now |
|---|---|---|
| `mir_receiver_carriage_admission_owner.sh` | compiled its own `mir_lower` (645 s locally); run only by the weekly harness shard | uses the job's shared MIR lowering tool through the new `self-host-mir-receiver-carriage-admission-test-smoke`; push self-host shard, after the step that builds the tool; the harness shard calls the target |
| `role_override_mir_replacement.sh` | weekly aggregate only | push self-host gates shard (12 s locally) |
| `air_mir_binding_smoke.sh` | manual | push core shard, with the DIR and RIR identity targets |
| `test-mir` pointer-self drift | already in `test-all` | listed on the receiver row |

The receiver row no longer lists two gates:

- `driver_rung2_callable_receiver_carriage_owner.sh` only defines a check
  that the DRV-2 body parity battery calls. Run alone it exits 0 at once.
- `codegen_role_receiver_admission_owner.sh` first builds the self-host
  codegen from source through the installed driver. That build failed after
  17 minutes. Given the bootstrap's `gen2` codegen, the receiver check
  passes in 1 s.

## Results

- SoT edge (79 authorities, 145 derived carriers, `CLOSED=65 BRIDGE=12
  ACTIVE=2`) and adequacy, gate reachability (0 stale, 0 undeclared) and the
  component contract PASS.
- Run as CI runs them: `self-host-mir-function-param-flow-summary`, then
  the resource-flow, loop-flow and receiver carriage targets in one make
  call ("receipts match; kept", 2 s, carriage admission PASS); the role
  override gate PASS (11 s); the DIR, RIR and AIR targets PASS (1 s);
  `test-mir` 758 passed, 0 failed, including the pointer-self drift test.
