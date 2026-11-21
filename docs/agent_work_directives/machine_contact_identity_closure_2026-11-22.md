# Machine contact identity closure (2026-11-22)

Status: IMPLEMENTATION COMPLETE (2026-11-22; results in the
[evidence](../truth/evidence/2026-11-22_machine_contact_identity_closed.md)).
One edit lane (Claude). Base revision `b85f17298`. Integration owner: Claude; integration gate: `test_mir`,
`tests/ir/machine_layer_pipeline_smoke.sh`,
`tests/self_hosted/mir_machine_layer_smoke.sh`,
`tests/self_hosted/parity/driver/driver_rung2_machine_mir_parity_owner.sh`
(through its DRV-2 runner) and the backend-compare device cases. Outputs are
implementation candidates; current source, the SoT registry and executable
gates stay authoritative.

## Objective card

- Objective: close `semantic.machine_layer_transition`. One machine contact
  fact per RIR contact event, carried on the MIR row lowered from that event;
  every consumer reads that row; a missing row fails closed; the AST-pointer
  attach path is deleted.
- Priority: one fact owner and identity; fail closed; behavioral negative
  gates; then patch size.
- Forbidden: recovering a contact from the statement around a call; a backend
  falling back to the active statement instruction; a self-host MIR artifact
  that omits a contact; weakening an existing negative.

## Whole chain (observed on the base revision)

- Native producer: RIR adds one contact op per device call
  (`rir_builder_walk.c`); MIR lowers each to a resource-op and attaches its
  fact from the op (`mir_lower_population.c`). A second pass,
  `mir_enrich_machine_layer_facts`, re-found RIR ops by AST pointer, retried
  through a `let` initializer and silently succeeded on a miss; it copied
  facts onto `def`/`stmt` rows only when the statement's own node was the
  call (`Log(DeviceRead(d))` and `DeviceRead(a) + DeviceRead(b)` got none).
- Native consumers of those statement copies: the LLVM `ClaimDeviceSlot` let
  path (read the active `def`), the C/LLVM device builtins and runtime rows
  (fell back to the active instruction), and a consumer clause in
  `mir_fact_surface_validate.c`. AIR sites, the MIR validator and the C
  resource-op core read the resource-op.
- Self-host producer: no resource-op rows; it projects the statement's root
  call only (`machine_layer_projection_owner.pgy`), so a statement with two
  contacts carries none. Self-host MIR-to-C emits those calls anyway: no
  self-host consumer requires the row.

## Slices

1. Native (M1). Delete the enrich pass and the AST lookup; attach cleanup
   edges from their op; MIR validation refuses a machine fact on any row that
   is not a contact op; C/LLVM read the fact only from the call's own
   resource-op (source identity) and refuse without it. Gate: `test_mir`
   machine contact identity cases (the old path fails the first two).
2. Self-host (M2). The producer emits one resource-op contact row per contact
   call, before the instruction that evaluates it, in evaluation order; the
   statement keeps its runtime-call ABI row and no machine row. `mir_lower`
   requires each routine's contact rows and contact calls to be equal
   multisets of (contact, slot) and refuses a missing or extra row; this
   applies to native and self-host MIR alike. Position is not compared: the
   native producer gathers a scope's rows at its start (observed: an `if`
   condition, a `while` condition and a final release all in block 0). Gate: a contact row deleted from
   self-host and native MIR is refused (the base accepts the self-host MIR of
   a two-contact statement that has no rows).
3. Registry: the board/linker/boot provider and Region/Grant syntax are
   recorded as a product boundary decision, not an open migration; the row
   moves to CLOSED with its gates.

## Unknowns

- Whether any admitted program places a contact call in an `expr1`,
  collection target or match subject position; the M2 consumer check
  refuses rather than passes one the producer misses.
