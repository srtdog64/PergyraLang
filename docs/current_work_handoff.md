# Current Work Handoff

Updated: 2026-07-24 (Asia/Seoul)

This is a resume snapshot, not semantic authority. Verify it against current
source, `git status --short --branch`, the SoT registry, and the focused gate.

## Resume checkpoint

- Latest executable implementation: `ce712b8e` (`Close self-host generic
  default contract SoT`), pushed with `HEAD=origin/main=ce712b8e`.
- The committed DRV-2 manifest remains at 266 MIR fixtures. A concurrent,
  uncommitted follow-up currently enrolls `generic_default_contracts` as row
  267 and wires its owner checks into the broader gate.
- At documentation authoring, the worktree is intentionally dirty in that
  follow-up: `src/compiler/mir_json_dump.c`,
  `src/self_hosted/compiler/driver_rung2_owner.pgy`,
  `tests/self_hosted/parity/driver_rung2_body_parity.sh`,
  `tests/self_hosted/parity/driver_rung2_mir_producer_parity_owner.sh`, and
  `tests/self_hosted_component_contract_smoke.sh`. Preserve these changes.
- Earlier generic checkpoints: `0169b856` (Int specialization), `3d74c9dd`
  (two-argument descriptor), and `e6f321f2` (payload mismatch ratchet).
- VS Code setup remains closed in `720928c5`; its handoff refresh is
  `bf79f07d`.

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
- Commit `ce712b8e` is pushed. The focused native build used the C backend
  (`LLVM_ENABLED=0`); the full unfiltered 266-row matrix and LLVM lane were
  not run.

Primary files:

- `src/self_hosted/parser/generic_parameter_list_owner.pgy`
- `src/self_hosted/semantic/ast_generic_parameter_fact_owner.pgy`
- `src/self_hosted/semantic/ast_nominal_constructor_fact_owner.pgy`
- `src/compiler/mir_json_dump.c`
- `src/self_hosted/mir_lower/decl_lower.pgy`
- `tests/self_hosted/parity/generic_default_contracts_parser_parity.sh`
- `tests/self_hosted/parity/driver_rung2_generic_default_contract_parity.sh`

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

- C producer-first parity for fixtures 261-265:
  `backends=1 body_fixtures=20 mir_fixtures=5`.
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

- Full unfiltered 266-row DRV-2 matrix.
- LLVM async parity for these rungs.
- Coq/Rocq proof; the toolchain is not installed.

## Next executable work

The first observed post-closure failure is
`tests/cases/backend_compare/generic_default_ability_bind_dispatch/main.pgy`:

- current self-host result: semantic `undefined_function` for `StorageParty`;
- therefore the next seam is party/role ability-bind declaration ownership and
  its consumer admission, not generic-default substitution itself;
- first falsifier: the same fixture must reach `StorageParty`'s declared
  `dyn role` binding and emit the `IntBuffer` dispatch, or fail with a stable
  owner diagnostic rather than a generic undefined-function result;
- do not add party/class-name or fixture-name exceptions.

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
2. Verify `git status`, HEAD/origin, committed fixture count 266, and the
   concurrent dirty integration's fixture count 267 before touching it.
3. Reproduce `generic_default_ability_bind_dispatch` and name the
   `StorageParty`/ability-bind owner before editing.
4. Make the next executable replacement; do not spend a third consecutive
   commit on docs or fixture-only enrollment.
5. Refresh exact revision, dirty state, last green gate, next falsifier, and
   blockers.
