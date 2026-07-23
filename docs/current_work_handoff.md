# Current Work Handoff

Updated: 2026-07-24 (Asia/Seoul)

This is a resume snapshot, not semantic authority. Verify it against current
source, `git status --short --branch`, the SoT registry, and the focused gate.

## Resume checkpoint

- Latest executable implementation: `ec719baa` (`Close ListPush scalar
  argument SoT`) is the local checkpoint. `origin/main` remains at `67033cad`;
  the branch is two commits ahead before this handoff refresh.
- The committed DRV-2 manifest contains 274 MIR fixtures, with
  `list_push_get_loop` as the latest executable row. Missing multiply-edge and
  mixed scalar-value mutations are its negative gates.
- Two pre-existing/concurrent edits are intentionally preserved and are not
  part of `ec719baa`:
  `src/self_hosted/codegen/emission/stmt_emit.pgy` and
  `src/self_hosted/codegen/emission/try_let_emit_owner.pgy`.
- `67033cad` closed the preceding `for_in_list_int` iteration seam, and
  `7fdef5aa` closed the earlier `list_int_loop` ListGet return-type seam.
- The nested List path is executable: contextual `ListNew` resolves the
  declared `List<HashMap<String, Int>>`, self-host and native MIR agree, and
  emitted C compiles and runs.
- Resume by verifying `git status --short --branch`, the registry rows
  `selfhost.expression_surface`, `selfhost.call_target_identity`, and
  `selfhost.local_binding_statement_routing`, plus the focused gate below.
- Earlier generic checkpoints: `0169b856` (Int specialization), `3d74c9dd`
  (two-argument descriptor), and `e6f321f2` (payload mismatch ratchet).
- VS Code setup remains closed in `720928c5`; its handoff refresh is
  `bf79f07d`.

## Last closed executable family: ListPush scalar graph value typing

Objective card:

- Objective: admit arithmetic ListPush values from the parser-owned expression
  graph without adding a List-local expression type system.
- Priority: one scalar graph operator policy, List call validation, target/ABI
  carriage, negative ratchet, then patch size.
- Fact owner: `ast_expression_graph_scalar_shape_owner.pgy` owns the shared
  scalar operator result policy; the last semantic consumer is
  `SemanticExpressionGraphListCallFactFromResolvedTarget`.
- Forbidden fallback: source-text arithmetic inference, a List-only scalar
  taxonomy, fixture/name special cases, or backend type guessing.
- Verification gate:
  `driver_rung2_list_push_scalar_value_parity_owner.sh`; falsifiers remove the
  multiply right edge and change the source value to `i * "bad"`.

Observed closure:

- Native/self MIR agree on direct `ListPush`, `i * i`, and local `i: Int`.
- Self-host C emits `pgy_list_push_int`, `pgy_list_size_int`, and
  `pgy_list_get_int`; native/self execution prints `5` then `55`.
- Focused C and hard producer-first parity passed with
  `body_fixtures=20 mir_fixtures=1`; the DRV-2 manifest is 274.
- Full C codegen parity passed all 85 fixtures and all named negative/role
  legs. Component, hard contract, authority edge, authority adequacy,
  documentation quality, shell syntax, and `git diff --check` passed. The edge
  registry remains 45 authorities and 40 derived carriers; Coq/Rocq was an
  explicit declared skip because no prover is installed.
- Commit `ec719baa` is local and not pushed. The full unfiltered 274-row DRV-2
  matrix and LLVM lane were not run.

## Next observed executable seam: lexical List shadow identity

- Native MIR accepts
  `tests/cases/backend_compare/list_shadow_scope_metadata/main.pgy`; the current
  self-host driver fails with `MIR shadowed local type changed: items`.
- The fixture uses outer `items: List<Int>` and inner
  `items: List<String>`, then resumes the outer binding after the branch.
- Next objective: preserve parser/semantic binding identity and lexical scope
  through routine SSA construction instead of indexing locals only by source
  name. The first falsifiers are loss of the inner binding identity and failure
  to restore the outer List ABI after block exit.

## Last closed executable family: List foreach iteration and ABI

Objective card:

- Objective: carry `List<Int>` foreach identity from the semantic iteration row
  through MIR reconstruction into one shared Array/List runtime ABI projection.
- Priority: iteration SoT, routine-local MIR validation, fallback removal,
  missing-row negative ratchet, then patch size.
- Fact owner: `SemanticAstIterationTypeFacts` under
  `selfhost.iteration_type_verdict`; last consumers are
  `iteration_type_fact_owner.pgy`, `routine_lower.pgy`,
  `foreach_collection_runtime_owner.pgy`, and `stmt_emit.pgy`.
- Forbidden fallback: source-local type as iteration authority, MIR collection
  guessing, source-text reparse, or a second List-only statement emitter.
- Verification gate: `driver_rung2_for_in_list_parity_owner.sh`; falsifier is
  removal of the carried `iteration_type_facts` array.

Observed closure:

- Native/self MIR agree on `binding_type=Int`, `iterable_type=List<Int>`.
- Self-host C emits `pgy_list_size_int` and `pgy_list_get_int`; native/self run
  output is `12`.
- Focused C and hard producer-first parity passed with
  `body_fixtures=20 mir_fixtures=1`.
- Full C codegen parity passed 85 fixtures and all named negative/role legs.
- Component, hard contract, authority edge, authority adequacy, shell syntax,
  and `git diff --check` passed. The edge registry reports 45 authorities and
  40 derived carriers; Coq/Rocq was an explicit declared skip.
- Commit `67033cad` is pushed.

## Last closed executable family: ListGet compound return type

- `list_call_type_owner.pgy` is the final codegen type consumer for the carried
  ListGet element type.
- `list_int_loop` now carries `Int` into `total + ListGet(xs, i)`, emits C, and
  runs under the focused hard producer-first gate.
- Commit `7fdef5aa` is pushed.

## Last closed executable family: List operation call ABI

Objective card:

- Objective: carry direct `ListPush`, `ListGet`, `ListSet`, `ListRemove`, and
  `ListSize` target/receiver/arity/value/return facts into one self-host C ABI
  lowering path.
- Priority: semantic List-call owner, element-specific runtime symbols,
  addressable receiver enforcement, missing-target negative ratchet, then
  patch size.
- Fact owners: `ast_expression_graph_resolved_call_type_owner.pgy` owns the
  semantic List-call protocol; `list_runtime_owner.pgy` owns C operation
  symbols; `list_call_emit_owner.pgy` is the last codegen consumer.
- Forbidden fallback: source callee spelling in emitted C, receiver/type
  guessing, or accepting a missing `collection_call_target` fact.
- Verification gate:
  `driver_rung2_list_ops_parity_owner.sh`; falsifiers are a source-level List
  call escaping lowering or a missing/malformed call-target row being accepted.

Observed closure:

- Native and self-host MIR agree on all five List operation target rows.
- Self-host C emits `pgy_list_*_int`, compiles, and runs with
  `3, 10, 30, 99, 2, 99`; `List<String>` independently emits and runs with
  `world`.
- The missing `ListPush` target mutation fails closed with
  `MIR instruction expression graph is missing or invalid`.
- Commit `0c19a8c5` is pushed. Component, focused producer-first, hard
  contract, authority adequacy, authority edge, shell syntax, and
  `git diff --check` gates passed; Coq/Rocq remained an explicit skip.

## Last closed executable family: nested generic List ABI

Objective card:

- Objective: carry contextual `ListNew` and nested `List<HashMap<String, Int>>`
  ABI facts from parser-owned call/type surfaces through self-host C emission.
- Priority: one contextual type owner, canonical list runtime ABI,
  fail-closed missing/unsupported facts, negative ratchet, then patch size.
- Fact owners: `SemanticBuiltinSignatureRows`,
  `ast_contextual_builtin_type_owner.pgy`, and
  `list_runtime_owner.pgy`; last consumers are initializer facts, ABI layout,
  runtime header selection, and program emission.
- Forbidden fallback: bare Unknown constructor success, implicit element
  guessing, nested type flattening, source reparse, and backend-only ABI guess.
- Verification gate:
  `driver_rung2_nested_generic_containers_parity_owner.sh`; falsifiers are
  missing contextual type and unsupported nested element ABI.

Observed closure:

- Native and self-host MIR agree on `List<HashMap<String, Int>>`.
- Self-host emits the canonical list specialization and constructor symbol;
  emitted C compiles and the fixture runs with output `nested`.
- Component, hard-contract, shell syntax, `git diff --check`, authority
  adequacy, and authority edge gates passed. Coq/Rocq was an explicit skip
  because no prover is installed.

## Last closed executable family: ability generic multi-bound defaults

Objective card:

- Objective: carry declaration-site ability generic `where` bounds and
  defaults from parser/HIR facts through `SemanticAstRoleFacts`, native/self
  MIR, and emitted C dispatch, with one semantic owner.
- Priority: one SoT owner, owner-directed fact carriage, fail-closed malformed
  or missing bounds, negative ratchet, then patch size.
- Fact owner: `SemanticAstRoleFacts` via `SFAbilityGenericBounds` and the
  `selfhost.ability_generic_bounds` registry row. Last consumers are native MIR
  generic metadata, self-host MIR lowering, the bound verdict, and role
  dispatch C emission.
- Forbidden fallback: source-text reparse by consumers, empty or guessed
  constraint facts, accepting a default that does not implement every bound,
  and dual-read compatibility authority.
- Verification gate: `driver_rung2_generic_multi_bound_defaults_parity_owner.sh`;
  falsifier is the `MissingAbility` mutation plus native/self MIR fact drift.

Observed closure:

- Native and self-host MIR now agree on
  `Packable<T>` with `constraint: Comparable + Cloneable` and
  `default_type: Item`. The self-host semantic owner validates both default
  implementations and emits the stable
  `SemanticAstAbilityGenericBoundVerdict` diagnostic on the negative mutation.
- Focused producer-first parity passed with
  `backends=1 body_fixtures=20 mir_fixtures=1`; emitted C vtable/adapter facts,
  component contracts, shell syntax, `git diff --check`, hard-contract, and
  the authority edge gate passed. The current and previous ability rungs also
  passed together with `mir_fixtures=2`; full C codegen parity passed all 85
  fixtures, including role-operator runtime parity. The edge gate reports 45
  authorities, 39 derived carriers, and
  `CLOSED=25 BRIDGE=20 ACTIVE=0`. Authority adequacy passed its live
  owner/consumer negative mutations.
- The isolated current native compiler built and linked through the repository
  Makefile with `LLVM_ENABLED=0` and Windows `D:/...` build paths. A separate
  `/d/...` response-file invocation needed Windows-path normalization; that is
  an invocation caveat, not a source compilation failure.
- Commit `c5903680` is pushed. The full unfiltered 269-row matrix, LLVM lane,
  and Coq model were not run; Coq was explicitly skipped because no
  `rocq`/`coqc` is installed.

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
  `12`. The full C codegen parity covered 85 fixtures; the role-operator
  regression ran with output `123`. Component contracts, shell syntax, and
  `git diff --check` passed. The authority edge gate now reports 44
  authorities, 39 derived carriers, and `CLOSED=24 BRIDGE=20 ACTIVE=0`; its
  live owner/consumer negative mutations pass. The Coq model was declared
  skipped because no `rocq`/`coqc` is installed.
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

- C producer-first parity for the filtered DRV-2 `list_ops` row 271:
  `backends=1 body_fixtures=20 mir_fixtures=1`.
- `list_ops` emitted C compiled and ran with `3, 10, 30, 99, 2, 99`; the
  missing `ListPush` call-target mutation failed closed with
  `MIR instruction expression graph is missing or invalid`.
- `list_get_string` emitted C compiled and ran with `world` through the String
  List ABI.
- The ordered-bound row and previous dynamic ability-bind row passed together:
  `backends=1 body_fixtures=20 mir_fixtures=2`.
- Full C codegen parity: 85 fixtures, including fail-closed declaration and
  equality negatives; the role-operator runtime output was `123`.
- Ordered multi-bound generated C ran with `bag=4` and
  `multi-bound-defaults`; the missing-bound mutation was rejected by self and
  native, with self owner `SemanticAstAbilityGenericBoundVerdict` and
  `broken_bound: MissingAbility`.
- Fixture 266 mixed capstone parity:
  `backends=1 body_fixtures=20 mir_fixtures=1`; output `42`, `77`, `hi`.
- Generated driver build: 0 errors, 0 warnings.
- String mutation: `let_type_mismatch`, `expected: String`, `actual: Int`;
  native also rejects it. Int payload mutations remain ratcheted.
- `tests/self_hosted_component_contract_smoke.sh` and modified shell syntax.
- `make self-host-hard-contract-test-smoke`.
- `make self-host-substitution-velocity-test-smoke`: 9 blockers, 5 direct and
  4 process/evidence.
- SoT authority edge: 45 authorities, 39 derived carriers,
  `CLOSED=25 BRIDGE=20 ACTIVE=0`.
- `PGY_ALLOW_MISSING_COQ=1 make sot-authority-adequacy-test-smoke`: Coq was an
  explicit declared skip; live owner/consumer binding and negative mutations
  passed.
- `git diff --check` before the executable and handoff commits.

Not run:

- Full unfiltered 271-row DRV-2 matrix.
- LLVM async parity for these rungs.
- Coq/Rocq proof; the toolchain is not installed.

## Next executable work

The next observed executable seam is the compound ListPush value in
`tests/cases/backend_compare/list_push_get_loop/main.pgy`:

- Native C compiles and runs with output `5` then `55`.
- The current self-host driver fails closed as `ast_artifact_invalid`, owner
  `collection_value_type`, at `ListPush(xs, i * i)`.
- `SemanticExpressionGraphScalarTypeName` already owns arithmetic graph typing;
  the List call protocol must consume that owner instead of maintaining a
  literal/leaf-only local classifier.
- First falsifiers: changing an arithmetic operand to a non-numeric type and
  removing a required expression edge must both fail closed.
- Forbidden paths are source-text arithmetic inference, an `i * i` special
  case, fixture/local-name branching, and a List-only parallel type taxonomy.

Adjacent evidence:

- `generic_spawn`, `generic_spawn_multi`, and `generic_call` already self-emit;
  do not count duplicate fixture enrollment as executable substitution.
- `generic_multi_bound_defaults` and the `StorageParty` dynamic ability-bind
  owner seams are closed.
- `list_ops` is closed by `0c19a8c5`, including its call-target negative.
- `list_int_loop` is closed by `7fdef5aa`, including its return-type negative.
- `for_in_list_int` is closed by `67033cad`, including its missing iteration
  row negative.
- `nested_generic_containers` is closed by `474e6e76`, including its
  contextual-type and unsupported-element negatives.
- The next code commit must advance arithmetic ListPush argument typing through
  the existing graph owner, not add a docs-only or duplicate-fixture substitute.

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
2. Verify `git status`, HEAD/origin, and committed fixture count 273 before
   touching the next seam.
3. Reproduce `list_push_get_loop` and verify the `collection_value_type`
   diagnostic plus native output `5`/`55` before editing.
4. Make the next executable replacement; do not spend a third consecutive
   commit on docs or fixture-only enrollment.
5. Refresh exact revision, dirty state, last green gate, next falsifier, and
   blockers.
