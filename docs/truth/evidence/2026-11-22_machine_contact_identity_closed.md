# Machine contact facts by identity; `semantic.machine_layer_transition` closed (2026-11-22)

Source: main `b85f17298` plus this change. Tree: WSL `Ubuntu-C-WSL`
`/root/ts2` (native build, drivers, gates) and `/root/ts3` (CI make-step
scripts). Plan: [directive](../../agent_work_directives/machine_contact_identity_closure_2026-11-22.md).

## Before

- Native MIR attached each machine fact twice. The resource-op lowered from a
  RIR contact op got it from that op. A second pass,
  `mir_enrich_machine_layer_facts`, looked the RIR op up again by AST
  pointer, retried through a `let` initializer and returned success on a
  miss; it copied the fact onto the `def`/`stmt` row only when the
  statement's own node was the call. On five device programs the native MIR
  had 8, 5, 10, 8 and 8 facts; `Log(DeviceRead(d))` and
  `DeviceRead(a) + DeviceRead(b)` got no statement copy.
- The LLVM `let d: DeviceSlot<T> = ClaimDeviceSlot()` path read the claim
  fact from that statement copy; the C/LLVM device builtins and runtime rows
  fell back to the active statement instruction.
- The self-host MIR producer emitted no contact rows: it projected only a
  statement's root call, so a statement with two contacts had none, and
  self-host MIR-to-C still emitted both `pgy_device_read_Int` calls. No
  self-host consumer required the rows: a self-host or native MIR with a
  contact row deleted, or naming another slot, was accepted (rc 0).

## Change

- Native: the AST-pointer pass, its `let` retry and
  `rir_scope_find_op_by_ast` are deleted; cleanup edges attach from their op;
  MIR validation refuses a machine fact on any row that is not a contact op.
  C and LLVM read the fact only from the call's own resource-op (source
  identity) and refuse without it, the claim `let` paths included.
- Self-host: `SelfMirRoutineAddMachineContactRows` emits one resource-op
  per contact call of an expression, before the instruction that evaluates
  it, with its machine row, slot and runtime-call ABI row; the statement
  keeps its runtime-call ABI row and no machine row. MIR validation accepts
  a machine row only on such a row. `MirMachineContactCoverageError`
  (MIR consumer) requires each routine's contact rows and contact calls to
  be equal multisets of contact and slot, on native and self-host MIR. It
  does not compare positions: the native producer gathers a scope's rows at
  its start (an `if` condition, a `while` condition and the final release
  all stand in block 0).
- Registry: `semantic.machine_layer_transition` CLOSED (CLOSED=61
  BRIDGE=18 ACTIVE=2); the board/linker/boot provider and Region/Grant syntax
  stay embedder/product work behind the provider bind export.

## Results

- `test_mir`: 758 passed. Of the four machine contact cases, the base fails
  the first two (facts outside contact resource-ops; a fact copied onto the
  statement is not refused).
- Native MIR facts on the five device programs: 8, 5, 10, 8, 8 before; 4, 3,
  6, 4, 4 after, all on contact resource-ops. Self-host MIR now has the same
  contact rows (4, 3, 6, 4, 4), where it had 4, 3, 4, 4, 4 statement rows;
  the remote read is named `SubmitRead` there and `AwaitRemote` natively.
- Installed driver (test profile), base driver against this one on the five
  programs and on `if`/`while`, assignment, range-bound and `defer` programs:
  source C and MIR-to-C output byte-equal.
- Self-host or native MIR with a read contact row deleted, or naming the
  other slot: base rc 0, now refused ("machine contact call has no contact
  row: read:dev").
- Gates: `tests/ir/machine_layer_pipeline_smoke.sh`,
  `tests/self_hosted/mir_machine_layer_smoke.sh` (with the new contacts
  case), the public machine manifest gate, `slot_contract_smoke.sh`, the
  three device backend-compare cases; native C and LLVM run the probe
  programs equal.
- CI make-step scripts (75) against the previous baseline (release-profile
  driver): the native slice alone, no rc difference; the whole change with a
  test-profile driver, one difference, `domain_runtime_zone_sync_execution_owner.sh`
  timed out at 900 s while building its codegen tool, and with the
  release-profile driver of the same source it passes in 239 s. 14 scripts
  fail locally on both for environment reasons.
- Not changed, seen on the way: native has no contact op for a device call in
  a `defer` body and refuses the program at C and LLVM (base too); the MIR
  consumer refuses native MIR for a range loop ("no loop-init owner") and
  for a remote await, before and after; self-host admits a helper that
  returns a read of a `ref` slot, which native refuses.
