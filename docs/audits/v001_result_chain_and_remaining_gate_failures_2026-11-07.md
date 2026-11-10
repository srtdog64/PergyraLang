# V.001 Result chain and remaining gate failures

Checked 2026-11-07. Independent executable observations and proposed patches;
this report does not close V.001 or replace the integration lane's state.
Main chat owns the shared compiler, gate edits, installation and publication.
The inspection plan is [here](../agent_work_directives/v001_enum_result_independent_validation_2026-11-07.md).

## Identity and reproduction

- Shared HEAD: `60fd1e26d8ea3987efbcbfc65f59b892b234701a`, with preserved staged
  and unstaged work. Baseline installation came from staged tree
  `c397e5309af6e66e312b8ad5a2ff39b788cf1e0d`.
- Frozen root: `/root/pgy-v001-candidate11.c397e53.20261107a` in
  `Ubuntu-C-WSL`; no baseline source or executable was changed by this audit.
- Launcher SHA-256:
  `138bf5fcfa71b0c518458283fa10df7c4f180edd8fe2ea194d58c2dc43273c7e`.
- Installed driver SHA-256:
  `e236e21c59dc1c12a98dc955614c55c303a2b0e43e08d3098290b09fa5ebc892`.
- Handoff: `D:\PergyraLang\.tmp\v001-enum-result-audit-20261107\handoff-v2`.
  It contains exact fixtures, commands, manifests, raw diagnostics, proposed
  patches and `sha256.json`. The older `handoff` directory has the superseded
  two-argument projection test; use `handoff-v2`.
- The census runs each selected source on native/installed C/LLVM, checks
  independent expected output, continues after a refusal, bounds each process
  and drains its process group on timeout. No timeout occurred in these runs.

`run_census.py NEW_INSTALL_ROOT UNIQUE_LABEL` replays the eleven valid inputs.
`run_negative_census.py NEW_INSTALL_ROOT UNIQUE_LABEL` checks the existing
negative cases and artifact sentinels. Both scripts and their input manifest
are in the handoff. Use a new label; existing evidence is not overwritten.

## Complete bounded positive census

The original full gate stops at its first installed LLVM refusal. Expanding
the affected Result/assignment family gives the following observed matrix.

| Input | Native C | Native LLVM | Installed C | Installed LLVM |
|---|---|---|---|---|
| existing `ref_result_plain` | PASS | PASS | PASS | return-type/route refusal |
| existing `inout_nested` | PASS | PASS | PASS | Result enum route refusal |
| existing `inout_write` | PASS | PASS | PASS | PASS |
| existing `inout_option_result` | PASS | PASS | PASS | record Result return refusal |
| zero-argument Ok factory | PASS | PASS | PASS | route refusal |
| Err factory, ordinal 1 and enum match | PASS | PASS | PASS | route refusal |
| Result argument and forwarding | PASS | PASS | PASS | return-type refusal |
| local Result assignment, branch and loop | phi type conflict | PASS | wrong Result constructor ABI | route refusal |
| IsOk/IsErr/Unwrap/UnwrapErr/UnwrapOr | PASS | PASS | PASS | route refusal |
| imported enum error identity | PASS | PASS | PASS | route refusal |
| existing String-error Result control | PASS | PASS | PASS | three-routine route refusal |

There are 32 successful executions and 12 build refusals across these eleven
valid semantic inputs, not twelve independent compiler causes. Existing
inputs/control are in `independent-enum-result-baseline1`; the six new valid
fixtures are in `baseline2`. The first prototype of those six used reserved
function name `Read`; its baseline1 diagnostics are fixture errors and are
excluded. Baseline2 uses `ObserveOutcome` and supplies the native controls.

## Confirmed seams and remaining obligations

1. **Referenced enum identity and its wrapper.** The original referenced-enum
   producer did not select an enum through a Result argument. The resulting
   empty fact let the general scalar route decline; an unrelated nominal
   two-routine route then claimed the program. Main is repairing wrapper
   selection and nominal rejection behind the existing declaration owners.

2. **Two real MIR declaration encodings.** Issuing the unchanged original
   `public_array_drop_ref_result_plain.pgy` again establishes:
   native `--mir-json` emits five enum fields, SHA-256
   `d84862110455b8bfc9e769201e0ae5ab3c1bd2e339f2dd5b9a8a630f524468a0`;
   installed `--emit-mir-json-verified` emits eight fields, SHA-256
   `70be2c057c703043a19b1d0a6c22d5e59b384659b4284cff44d38511e74908b4`.
   The installed form includes `nominal_kind`, `fields`, and `methods`.
   A stored `*.v001.mir.json` was initially mislabeled native during this
   audit; fresh issuance corrected that attribution before handoff.
   A downstream raw field-count test is not a shared declaration contract.

3. **Result payload generality.** The existing `inout_option_result` gate
   returns `Result<OptionBuilt, OptionBuildError>`. `OptionBuilt` contains an
   `Option<OptionCell>` and count. Closing only `Result<Int,E>` cannot close
   the unchanged public gate. Required consumers include callable admission,
   contextual constructors, parameters, locals, match binding types and both
   C/LLVM emitters. Main has expanded the joined projection accordingly;
   full installed emission/execution remains to be rerun.

4. **Installed source-C assignment drops its expected type.** The existing
   semantic assignment owner supplies `expected_type` through
   `stmt_emit.pgy:282`. `assign_emit_owner.pgy:111` uses the expected-value
   emitter for structs but lets Result fall through to uncontextual emission.
   In `local_flow`, an enum Err becomes `pgy_result_err(const char*)`, and Ok
   becomes the String-error Result helper. Host C rejects both assignments.
   The expected-value emitter already selects exact Result runtime symbols
   (`expr_semantic_composite_literal_emit_owner.pgy:51`); let, return and
   collection-element consumers already use it. The handoff includes a
   one-consumer proposal, explicitly **not execution-validated**.

5. **Native C phi compares inconsistent issued type spellings.** The same
   `local_flow` source emits declaration DEF `result.1` with
   `Result<Int,ReadError>` and assignment DEF `result.2`/`result.5` with
   `Result<Int, ReadError>`. `mir_call_fact.c:303` takes annotation rendering
   for one and the checker's exact lexical binding name for the other.
   `transpiler_mir_phi_type_owner.c:156` correctly fails its exact equality
   check. Repair the producer's canonical type representation; do not relax
   phi identity, replace lexical identity with a name lookup, or overlook a
   genuinely different enum/alias. This production repair is still OPEN.

6. **Backing-summary completion blocks an existing positive and negative.**
   The unmodified native-stage gate fails at
   `public_array_drop_inout_late_forward.pgy`, before its negative section:
   `Arrange` reports `required return backing relation is incomplete or
   stale`, then ArrayDrop loses storage preservation. The later allocation
   message is secondary; no memory exhaustion was observed.
   `ref_result_resource_negative` also stops at that internal summary error
   before its intended borrow diagnostic. Existing debug and verification
   modes reproduce both at work unit 1, fixed-point pass 1, with zero currency
   memo records/reuses. This is not evidence of a stale memo reuse.

   For the positive, twelve focused variations distinguish the boundary:
   moving the callee earlier, naming the pushed value, changing the element,
   payload or error type do not repair it. Removing the Option match or
   expressing its condition with IsSome/if admits MIR. An empty Some arm,
   same-Ok arm and three named-binding variations still fail. Investigate
   `function_param_flow_backing_dependencies.c:1279` (match subject origin,
   pattern binding, arm consumers), its publication facts and completion
   replay. The ArrayPush element-unproved trace also appears in admitted
   variations and is not by itself the root cause.

   The resource-return negative remains an internal summary failure even
   without Err, with String error, or without the caller's match. Do not
   assume the same match-only fix closes both. No scratch source replaces
   either original acceptance fixture.

## Safety checks and proposed patches

- Existing source negatives: **66/66 returned failure and preserved output**;
  **63/66 matched the existing gate's intended diagnostic**. The resource
  Result case above is an unresolved internal-error refusal. Two retaining
  call cases emit the correct `PGY_SEM_BORROW_ESCAPE`,
  `semantic:borrow_escape`, and ArrayDrop storage-preservation reason; the
  gate incorrectly expects a missing-binding message.
- `retaining_diagnostic_assertion.patch` checks the structured code, cause
  and exact storage boundary for those two cases. It preserves both required
  refusal and output sentinels. The exact proposed gate block passed on both
  real cases in 0.214 s. Patch SHA-256:
  `ed800f0d8777cb5e828fc6316f3f362be3e12ece52776ddc09d886e6dea9912e`.
- Existing MIR-stage public ArrayDrop gate: **PASS**, 3.222 s. It executes
  five issued MIR inputs and rejects five modified MIR inputs per C/LLVM
  backend, with artifact preservation. This does not imply source-stage PASS
  or a new allocation-counting/free measurement.
- A separate frozen producer-to-projection probe accepts both actual issued
  enum encodings and their restored inputs (four positive observations).
  Per producer it rejects nine missing/mismatched declaration, enum identity,
  syntax, source-path and variant facts (18 negatives). Six failures occur
  earlier at the existing MIR machine-layer admission, so the initial scratch
  harness's narrow diagnostic matcher was corrected in a separate
  `reviewed-results.json`; original outputs/results are retained. Two extra
  malformed-field explorations also refuse, without claiming a new schema
  contract from those malformed rows. Snapshot and logs are under
  `/root/pgy-v001-enum-result-probe.20261107c` and copied into the handoff.
- Main repaired the new projection's incomplete symbol validation. The
  generalized projection snapshot SHA-256
  `de32501ad3b957f0dae1e6c5e11b774df70cbf27e0595ea0754b2d5f3625d7a9`
  passes independently compiled C and LLVM contract probes: original and
  restoration accepted, all seven individually forged C helper symbols
  refused. The candidate test patch uses the new three-argument API and
  `payload_record_row`; SHA-256
  `68c8191d50f7786fe2171ed1391ad74e1fd2d2b794aafeee00c093cfed26978c`.
- The assignment proposal SHA-256 is
  `4b43f96e38679ac4da4223de3b840db8e12aca00e8357974dfdd2d14b36e88cb`.
  It is a reviewable candidate, not a tested compiler fix. All three patch
  files passed `git apply --check` against the inspected shared tree.

The two test edits briefly made before main's read-only coordination message
were isolated into the handoff and removed from the shared code/gate paths.
The original public gate SHA remains
`8ecf71f50710dfc26754443f4f031cab6a29a026823a4be91ac8b96b0ff396e9`.
No code was staged, committed or pushed by this lane. No safety rule changed.

Main must integrate the appropriate consumer repairs and assertions, rebuild
one fixed candidate, rerun the unchanged full collection/source/MIR gates,
canonical admission, gen2 == gen3 and required installed-driver checks, then
publish and observe CI at that exact SHA. This audit has not run that final
candidate, so neither full repair nor CI green is claimed.
