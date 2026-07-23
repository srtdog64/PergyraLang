# Current Work Handoff

Updated: 2026-07-24 (Asia/Seoul)

This is a resume snapshot, not semantic authority. Verify it against current
source, `git status --short --branch`, the SoT registry, and the focused gate.

## Resume checkpoint

- Latest executable implementation: `e09d680e` (`Close self-host ability bind
  dispatch SoT`), pushed with `HEAD=origin/main=e09d680e`.
- The committed DRV-2 manifest now contains 268 MIR fixtures, with
  `generic_default_ability_bind_dispatch` enrolled as the latest row and its
  owner checks wired into the body/producer parity gates.
- At documentation authoring, `git status --short --branch` is clean and
  `HEAD` matches `origin/main`.
- Earlier generic checkpoints: `0169b856` (Int specialization), `3d74c9dd`
  (two-argument descriptor), and `e6f321f2` (payload mismatch ratchet).
- VS Code setup remains closed in `720928c5`; its handoff refresh is
  `bf79f07d`.

## Last closed executable family: dynamic ability bind dispatch

Objective card:

- Objective: carry party role-slot, role implementation, ability method, and
  bind identity from semantic facts through MIR JSON into one C vtable/bind
  ABI.
- Priority: one semantic bind owner, declaration/MIR carriage, fail-closed
  missing ABI facts, direct-call fallback removal, then patch size.
- Fact owners: `ast_bind_statement_type_fact_owner.pgy`, declaration rows,
  role/nominal semantic views, and the native `slot_anchor` MIR projection.
- Last consumers: `ability_bind_emit_owner.pgy`,
  `expr_semantic_dynamic_ability_call_emit_owner.pgy`, and
  `role_dispatch_emit_owner.pgy`.
- Forbidden fallback: party/slot/role text reparse, direct ability-call
  fallback, guessed vtable identity, or successful emission when dispatch ABI
  facts are missing.
- Verification gate: `driver_rung2_ability_bind_dispatch_parity_owner.sh`,
  focused DRV-2 producer-first parity, C compilation, and runtime output.

Observed closure:

- Native and self-host MIR carry party role slots, ability defaults, role
  implementations, `AST_BIND_STMT` with `slot_anchor`, and the resolved
  `Bufferable_Put` member target.
- Self-host C emission now owns the dynamic vtable field, bind helper, role
  adapter, and vtable call. The bad argument mutation fails closed as
  `call_arg_type_mismatch`; no direct-call fallback is admitted.
- The focused gate passed with `backends=1 body_fixtures=20 mir_fixtures=1`.
  Regenerated driver C built with GCC and the emitted fixture ran with output
  `12`. Component contracts, SoT authority checks, shell syntax, and
  `git diff --check` passed. The Coq model was declared skipped because no
  `rocq`/`coqc` is installed.
- Commit `e09d680e` is pushed. The full unfiltered 268-row matrix and LLVM
  lane were not run.

## Last closed executable family: declaration-site generic defaults

Objective card:

- Objective: keep declaration-site generic parameter/default facts owned by the
  parser/semantic declaration owners, carry them through native MIR JSON, and
  consume them in self-host MIR lowering and C codegen.
- Priority: one declaration fact owner, effective nominal field types, native
  MIR carriage, fail-closed malformed/default mismatch diagnostics, then patch
  size.
- Fact owners: `generic_parameter_list_owner.pgy`,
  `ast_generic_parameter_fact_owner.pgy`,
  `ast_nominal_constructor_fact_owner.pgy`, and the native
  `MIRDeclHeader` generic metadata owner.
- Last consumers: `mir_json_dump.c`, `decl_lower.pgy`, nominal constructor
  typing, and emitted C declarations.
- Forbidden fallback: source re-scan by consumers, guessed missing defaults,
  fixture/class-name branches, unresolved `T` in emitted C, and dual reads.
- Verification gate: parser AST parity plus the focused
  `generic_default_contracts` source/MIR/C/runtime parity script and its
  `Box<T = String>` negative mutation.

Observed closure:

- Shared declaration parsing now handles nested default types and fails closed
  through the parser error owner for functions, nominals, and abilities.
- Generic defaults become typed semantic rows; nominal constructor field facts
  substitute the owner-directed defaults once.
- Native MIR JSON carries `generic_params` with `name`, `constraint`, and
  `default_type`; self-host MIR lowering consumes those facts without guessing.
- Focused parser parity, malformed-default rejection, native MIR carriage,
  emitted-C shape, runtime output (`save=9`, `box=7`), and the component
  contract smoke all passed. Mutating `Box<T = Int>` to `Box<T = String>` is
  rejected as `call_arg_type_mismatch` with `expected: String` and
  `actual: Int`.
- Commit `ce712b8e` closed the declaration/default owner; commit `030c82e7`
  enrolled the fixture in the executable DRV-2 rung and is pushed. The
  focused native build used the C backend (`LLVM_ENABLED=0`); the full
  unfiltered 267-row matrix and LLVM lane were not run.

Primary files:

- `src/self_hosted/parser/generic_parameter_list_owner.pgy`
- `src/self_hosted/semantic/ast_generic_parameter_fact_owner.pgy`
- `src/self_hosted/semantic/ast_nominal_constructor_fact_owner.pgy`
- `src/compiler/mir_json_dump.c`
- `src/self_hosted/mir_lower/decl_lower.pgy`
- `tests/self_hosted/parity/generic_default_contracts_parser_parity.sh`
- `tests/self_hosted/parity/driver_rung2_generic_default_contract_parity.sh`
- `tests/self_hosted/parity/driver_rung2_generic_default_contract_parity_owner.sh`
- `tests/self_hosted/parity/driver_rung2_body_parity.sh`
- `tests/self_hosted/parity/driver_rung2_mir_producer_parity_owner.sh`

## Previous closed executable family: generic Future spawn

Counted fixtures:

- 263 `generic_future_spawn_int`
- 264 `generic_future_spawn_multi_arg`
- 265 `generic_future_spawn_string`
- 266 `generic_future_spawn_mixed`

Objective card:

- Objective: carry generic call-node specialization through `spawn` and await
  Int/String payloads through one runtime ABI owner.
- Priority: specialization identity, one Future handle ABI, real concurrency,
  fail-closed payload/arity, then patch size.
- Fact owners: generic specialization graph fact, `FuturePayloadTypeOpt`, ABI
  layout, and `spawn_runtime_owner.pgy`.
- Last consumer: semantic graph spawn/await C emission.
- Forbidden fallback: source-name/fixture branching, synchronous lowering,
  payload- or arity-specific C helpers, dual reads, and detached local capture.
- Falsifiers: Int/String payload mutation, missing specialization carriage,
  and reintroduction of payload-specific runtime entry points.

Observed closure:

- Int specialization previously failed as `ast_artifact_invalid` because the
  generic return fact was consumed only when the call was the graph root.
- Two-argument spawn previously failed behind a one-argument codegen
  assumption.
- String spawn previously failed because `Future<String>` had no owned ABI
  materialization.
- Generic specialization is now resolved by call-node identity under the
  parent spawn.
- One tagged C descriptor owns Int-unary, Int-binary, and String-unary
  signatures. One worker and one `pgy_selfhost_spawn` dispatch all three.
- Named Future handles remain `PgyTaskHandle`; await projects the owned payload
  to `long long` or `const char*` without guessing.

Primary files:

- `src/self_hosted/semantic/ast_expression_graph_generic_call_owner.pgy`
- `src/self_hosted/codegen/runtime_abi/spawn_runtime_owner.pgy`
- `src/self_hosted/codegen/emission/expr_semantic_graph_emit_owner.pgy`
- `src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy`
- `src/self_hosted/compiler/driver_rung2_owner.pgy`
- `tests/self_hosted/parity/driver_rung2_generic_spawn_parity_owner.sh`
- `tests/self_hosted/parity/driver_rung2_generic_string_spawn_parity_owner.sh`
- `tests/self_hosted/parity/driver_rung2_generic_spawn_mixed_parity_owner.sh`

## Last observed verification

Green:

- C producer-first parity for the filtered DRV-2 row 267:
  `backends=1 body_fixtures=20 mir_fixtures=1`.
- The broader filtered 267-row body/producer gate completed with the generic
  default row included; the component contract smoke and shell syntax passed.
- Fixture 266 mixed capstone parity:
  `backends=1 body_fixtures=20 mir_fixtures=1`; output `42`, `77`, `hi`.
- Generated driver build: 0 errors, 0 warnings.
- String mutation: `let_type_mismatch`, `expected: String`, `actual: Int`;
  native also rejects it. Int payload mutations remain ratcheted.
- `tests/self_hosted_component_contract_smoke.sh` and modified shell syntax.
- `make self-host-hard-contract-test-smoke`.
- `make self-host-substitution-velocity-test-smoke`: 9 blockers, 5 direct and
  4 process/evidence.
- `make sot-authority-edge-test-smoke`: 43 authorities, 38 derived carriers,
  `CLOSED=23 BRIDGE=20 ACTIVE=0`; single Gate SoT and 7 protocol rows valid.
- `git diff --check` before both latest commits.

Not run:

- Full unfiltered 267-row DRV-2 matrix.
- LLVM async parity for these rungs.
- Coq/Rocq proof; the toolchain is not installed.

## Next executable work

The next observed executable seam is
`tests/cases/backend_compare/generic_multi_bound_defaults/main.pgy`:

- current native/self-host boundary: parser admission stops at
  `where T: Comparable + Cloneable`;
- first falsifier: the parser/semantic generic-bound owner must carry the
  ordered constraint facts, or reject the declaration with a stable owner
  diagnostic; consumers must not split the source text again;
- `nested_generic_containers` remains a later `ListNew` undefined-function
  failure;
- do not add fixture-name, party/class-name, or compatibility fallback paths.

Adjacent evidence:

- `generic_spawn`, `generic_spawn_multi`, and `generic_call` already self-emit;
  do not count duplicate fixture enrollment as executable substitution.
- `generic_multi_bound_defaults` currently fails parser admission at
  `where T: Comparable + Cloneable`; it is a later generic-bound rung.
- `nested_generic_containers` currently fails explicitly with
  `undefined_function: ListNew`; it is not the active rung.
- The next code commit must be an executable replacement for the
  `StorageParty` owner seam, not another docs-only or fixture-only delta.

## Workstation and recovery facts

- Global rules: `C:/Users/user/.codex/AGENTS.md`; repository `AGENTS.md` is
  more specific.
- Git for Windows, Git LFS, GitHub CLI, MSYS2 UCRT64 GCC/Make/Python/LLVM,
  Node, npm, and ripgrep are installed. GitHub CLI auth remains user-owned.
- Repository `core.autocrlf=false`; use the MSYS runtime path helpers for
  Windows `bin/pgy.exe`. Prefer serial `make` unless jobserver behavior is
  revalidated.
- `.vscode` uses `bin/pgy.exe`, `bin/pgy-lsp.exe`, MSYS2 UCRT64, focused
  build/self-host/doc tasks, and Extension Host launchers.
- Temporary probe/build paths created during this session are cleanup-only and
  must not be committed. Cleanup uses exact workspace paths and the Windows
  Recycle Bin.
- Credential hygiene: earlier process inspection exposed configured Figma,
  GitHub, Render, and Tavily credentials in command output. Never copy values
  into source/docs/logs; rotate or revoke those credentials.

## Resume sequence

1. Read this file, `src/self_hosted/PROGRESS.md`, `src/self_hosted/OWNERS.md`,
   and `selfhost.expression_surface` in the SoT registry.
2. Verify `git status`, HEAD/origin, and committed fixture count 267 before
   touching the next seam.
3. Reproduce `generic_multi_bound_defaults` and name the generic-bound owner
   before editing.
4. Make the next executable replacement; do not spend a third consecutive
   commit on docs or fixture-only enrollment.
5. Refresh exact revision, dirty state, last green gate, next falsifier, and
   blockers.
