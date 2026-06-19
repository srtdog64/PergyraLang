# Pergyra Source-Of-Truth Spine

Last updated: 2026-06-18

This document freezes the compiler ownership spine for beta closure. It exists
to stop A -> B -> A refactoring loops. When a future change is unclear, use this
document to decide which layer owns the fact, which layers may only consume it,
and which compatibility seams are allowed to remain.

## 0. Rule

Each semantic fact has exactly one owning layer. Later layers may consume the
fact, attach provenance to it, or emit diagnostics from it, but they must not
rediscover or reinterpret it.

Smoke tests are not source of truth. A smoke test only prevents a frozen owner
contract from drifting.

Current beta closure snapshot:

- `src/semantic/type_checker.c` is a narrow statement/program dispatch owner,
  not a helper warehouse.
- enum declaration checking lives in
  `src/semantic/type_checker_enum_decl.c`.
- assignment target path and borrowed-boundary root rendering live in
  `src/semantic/type_checker_assignment_path.c`. That owner must keep
  scratch-arena and heap ownership behind its path allocator/release helpers;
  recursive path construction must not directly free intermediate path parts or
  accept an external scratch/heap mode flag.
- assignment expression checking lives in
  `src/semantic/type_checker_assignment.c`; the header is declaration-only.
- break/continue semantic validation lives in
  `src/semantic/type_checker_loop_control.c`.
- `bind party.slot = Role;` semantic validation lives in
  `src/semantic/type_checker_bind_stmt.c`. Both the top-level statement
  dispatcher and CFG/body-flow path must call this owner. C/LLVM backends may
  emit bind wiring from the validated fact, but they must not be the first
  layer that discovers unknown party variables, missing role slots, unknown
  roles, or role-slot ability mismatches. If LLVM cannot lower a semantically
  valid bind fact because an inventory/vtable fact is missing, it must report a
  backend diagnostic instead of silently skipping the bind. Multi-ability role
  slots are conjunctive: a bound role must satisfy every required ability, not
  just one matching ability. Backend vtable emission must still resolve the
  concrete dispatch table from the party-slot ability fact. LLVM bind lowering
  consumes `llvm_party_slot_first_ability_name(...)` and
  `llvm_lookup_role_vtable_global(...)`; it must not recover the ability by
  scanning module globals with a role-name prefix. C bind lowering consumes
  `lookup_typed_var(...)` for the party type and
  `transpiler_resolve_active_ssa_name(...)` for the emitted local name; it must
  not scan `ctx->typed_vars` locally or emit the source binding name when MIR
  SSA has already renamed it.
- MIR source-local type facts are keyed by the source local name. LLVM MIR
  alloca/type consumers may see SSA-versioned names such as `local.1`, but the
  consumption owner must normalize those names to the source-local base before
  reading `MIRRoutine::source_local_types`. The MIR fact remains the source of
  truth; LLVM must not recover the local type by rescanning the AST body.
- LLVM verifier diagnostics live at the LLVM C API boundary in
  `src/codegen/llvm_api.c`. `LLVMVerifyModule(...)` may leave the diagnostic
  message pointer null on success; backend code must only call
  `LLVMDisposeMessage(...)` when the pointer is non-null.
- type-resolution DAG worklist execution lives in
  `src/semantic/type_checker_resolution_worklist.c`.
- type-resolution internal declarations live in
  `src/semantic/type_checker_resolution_internal.h`. The general semantic
  internal header may include that declaration surface, but nullable annotation
  readers and metadata dead-end recorders remain private to metadata owners.
  Splitting declarations is only valid when resolver-inventory smoke still
  proves zero fallback seams and no widened annotation-sensitive reads.
- Intent-local type/effect recovery lives in
  `src/semantic/type_checker_intent_types.c`. Intent step `where`, derived
  `using`, participant transfer-source checks, transfer edge consumers, and
  step `causes` checks may consume the intent domain owner seam, but they must
  not rediscover `AST_ZONE_DECL` or `AST_EFFECT_DECL` locally.
- General semantic domain lookup seams live in
  `src/semantic/type_checker_host_helpers.c`. Action contracts may consume
  `semantic_find_zone_decl_by_name(...)` and
  `semantic_find_effect_decl_by_name(...)`; zone relation/effect contract
  validation may consume `semantic_find_relation_decl_by_name(...)` and
  `semantic_find_effect_decl_by_name(...)`; world declaration, world helper,
  and world embedding consumers may consume `semantic_find_zone_decl_by_name(...)`
  and `semantic_find_world_decl_by_name(...)`; zone layer-slot authority
  validation may consume the relation/effect seams. DAG stage signature and
  graph-host lookup consumers may consume the class/party/roster/world/zone/
  relation/effect seams, but must not reopen raw declaration lookup locally.
  These consumers must not call `find_domain_decl_by_name(...)` locally for
  domain declaration recovery.
- The remaining AST slot analyzer pass is explicitly named
  `semantic_run_legacy_slot_resource_analysis(...)` at the semantic entry point.
  It is a compatibility seam for conservative escape/leak provenance only;
  CFG/MIR remains the body-safety source of truth.
- MIR lowering lives in `src/compiler/mir.c`, next to the owner-local lowering
  helpers it consumes. Public MIR query/pass wrappers live in
  `src/compiler/mir_public_surface.c`. MIR lowering is a consumer of HIR/RIR
  shape, not the owner of those inventories: routine iteration must use
  `hir_routine_inventory_from_program(...)`, and RIR scope matching/cleanup/
  population must consume `rir_scope_inventory_*` and `rir_scope_*_at(...)`
  accessors instead of reopening raw HIR/RIR arrays.
- HIR routine consumers must use `hir_routine_inventory_from_program(...)`.
  Mutable HIR pass owners, currently the callgraph pass, must use
  `hir_mutable_routine_inventory_from_program(...)`. Raw `hir->routines` /
  `hir->routine_count` access is confined to HIR construction, destruction, and
  public-surface owners.
- RIR dump and JSON dump output are public-surface consumers of the RIR scope
  inventory. They must use `rir_scope_inventory_get(...)` plus
  `rir_scope_fact_at(...)`, `rir_scope_op_at(...)`, and
  `rir_scope_state_summary_at(...)` instead of walking raw scope arrays.
- RIR read-only lookup helpers are public-surface APIs. Consumers that need a
  named state summary or projection fact must call
  `rir_scope_find_state_summary(...)` or `rir_scope_find_projection_fact(...)`
  instead of casting away `const` scope ownership or reimplementing local lookup.
  Authority/resource lookup by fact kind and capability presence checks follow
  the same rule through `rir_scope_find_fact_by_name_kind(...)` and
  `rir_scope_has_capability_fact(...)`.
- RIR flow enrichment is the mutable flow owner, but program-level scope
  matching must still use `rir_mutable_scope_inventory_from_program(...)`.
  Direct `rir->scopes` / `rir->scope_count` access is confined to RIR
  construction, destruction, and scope-storage/public-surface owners.
  Read-only fact/op/state-summary access inside the flow owner must use the
  RIR scope item accessors; only owner-local state-summary reset/rebuild writes
  may touch the raw summary storage directly.
  Validation and dump consumers must also read flow-block facts through
  `rir_scope_flow_block_*` and `rir_flow_block_fact_*`; reopening
  `scope->flow_blocks` / `block->facts` is limited to storage/build/enrichment
  owners. MIR cleanup invalidation checks are consumers of those facts and must
  use the same accessors. Flow semantic bits have the same rule:
  consumers use `rir_scope_conservative_semantics(...)`,
  `rir_flow_block_entry_semantics(...)`, and
  `rir_flow_block_exit_semantics(...)` instead of reading raw fields. Flow-block
  identity reads use `rir_flow_block_id(...)`,
  `rir_flow_block_is_reachable(...)`, and `rir_flow_block_is_join(...)`.
  Validation and dump consumers read scope metadata through
  `rir_scope_kind/name/owner_name/display_name/has_state_errors`; direct field
  reads are limited to RIR public-surface/storage/build owners.
- MIR declaration method routine links consume the MIR routine inventory
  accessors from `src/compiler/mir_program_inventory.c`. Declaration header
  linking and validation may compare owner/name metadata, but they must not
  reopen `mir->routines` or `mir->routine_count` directly. MIR program
  validation must use the same inventory view for routine-shape checks.
- Public MIR pass wrappers must consume the const/mutable MIR routine inventory
  views. Raw `mir->routines` / `mir->routine_count` access is confined to MIR
  construction, lifecycle, and program-inventory owners.
- C/LLVM backend routine-inventory wrappers are the only backend owners allowed
  to dereference their wrapped routine arrays. Backend declaration, intent, and
  MIR-contract consumers must call the wrapper accessor (`llvm_routine_inventory_get`
  or `transpiler_routine_inventory_get`) instead of indexing `inventory->routines`
  directly.
- Parser, lexer, semantic, compiler, and codegen headers are declaration
  surfaces by default. The only current non-runtime implementation-header
  exception is the macro-only `src/codegen/llvm_limits_internal.h`.
- Core AST public declarations live in `src/parser/ast_api.h`. Domain-oriented
  AST declarations live in `src/parser/ast_domain_api.h`; `ast_api.h` includes
  that header as a compatibility umbrella, but new domain accessors should be
  added to the domain header first. `backend-inc-size-test-smoke` rejects
  moving the frozen domain creation surface back into `ast_api.h`. Domain-only
  owners should include `ast_domain_api.h` directly instead of widening through
  the compatibility umbrella.
- hosted declaration compatibility policy lives in
  `src/codegen/host_decl_compat.c`. C and LLVM declaration lookup, hosted
  method compatibility, pointer-self classification, projection-ready
  classification, shared-field compatibility, and domain-constructor lookup may
  consume that policy, but they must not restate local party/role/roster/
  relation/effect/zone/world switch chains.
- Hosted method compatibility method arrays are internal state of the C/LLVM
  hosted method views. Consumers may ask
  `transpiler_hosted_method_view_compat_method(...)` or
  `llvm_hosted_method_view_compat_method(...)` only in non-MIR compatibility
  paths; MIR-active paths consume `MIRDeclMethod` metadata and linked routine
  indexes. Backend consumers must not index `ast_compat_methods` directly.
- LLVM MIR parameter self-field slot registration consumes
  `LLVMHostedFieldView` and `MIRDeclFieldClaim` rows. It may render a field
  type from AST only in the explicit non-MIR compatibility path; MIR-active
  code must use declaration-field type-name facts and must not index
  `ast_compat_fields` directly.
- Backend declaration name recovery is also centralized: LLVM consumes
  `llvm_decl_node_name(...)`, and C consumes `transpiler_decl_name_local(...)`.
  Lookup predicates must call those owners rather than restating per-declaration
  name accessor switches.
- Backend declaration generic-parameter recovery is centralized in
  `MIRDeclHeader` generic metadata for MIR-active paths. C/LLVM consumers may
  use `ast_declaration_generic_params(...)` only in non-MIR compatibility paths;
  they must not restate a local function/class/ability/role/party/roster
  generic-payload switch. `MIRDeclGenericParam` exposes bound/default
  type-name facts only; it does not carry ASTNode bound/default back-pointers.
  MIR lowering must fail closed if a declared generic bound/default cannot be
  rendered as a type-name fact. LLVM generic formal-default resolution consumes
  those MIR type-name facts directly in MIR-active paths and routes AST generic
  default/constraint nodes only through the explicit non-MIR compatibility
  path.
- LLVM class field index/type recovery is centralized in the LLVM registry.
  Consumers that already resolved a struct field index may ask
  `llvm_class_field_type_at_index(...)` for the field type, but they must not
  iterate `LLVMClassTypeEntry.fields[]` locally or index
  `fields[field_idx]` / `fields[*_idx]` just to map a struct index back to a
  field type. LLVM struct-type-to-class lookup follows the same rule through
  `llvm_lookup_class_by_struct_type(...)`; compatibility wrappers may remain
  only if they delegate to the registry owner. Vtable class lookup by method is
  also registry-owned through `llvm_lookup_vtable_class_with_method(...)`;
  member-call emitters must not scan `ctx->class_types` to rediscover vtable
  method placement. Party-instance and bind emitters use
  `llvm_class_field_index(...)` for class field placement; they must not scan
  `LLVMClassTypeEntry.fields[]` by field name at the emission site.
  When an LLVM consumer must iterate registered class fields for dirty-flag,
  projection, or zone-slot emission, it consumes
  `llvm_class_field_count(...)`, `llvm_class_field_name_at(...)`,
  `llvm_class_field_type_at(...)`,
  `llvm_class_field_struct_index_at(...)`, and
  `llvm_class_field_is_subject_slot_at(...)`; it must not reopen the raw
  `fields[]` array outside the registry owner.
- LLVM enum variant storage is registry-owned. Expression and match emitters
  consume `llvm_lookup_enum_variant(...)` / qualified lookup, and source type
  resolution consumes `llvm_enum_type_exists(...)`; consumers must not scan
  `ctx->enum_variants[]` or `ctx->enum_variant_count` directly.
- LLVM event type storage is owned by `src/codegen/llvm_event.c`. Main-wrapper
  emission may initialize registered events, but it must consume
  `llvm_event_type_count(...)` and `llvm_event_type_at(...)`; it must not read
  `ctx->event_type_count` or `ctx->event_types[]` directly.
- LLVM block creation must be context-explicit. Backend code uses
  `LLVMAppendBasicBlockInContext(ctx->context, ...)`; the deprecated
  global-context `LLVMAppendBasicBlock(...)` form is not allowed in production
  LLVM emitters.
- C backend projection/action codegen may consume active inventory and
  program-view seams for zone/effect/relation declaration recovery, but it must
  not reopen direct domain declaration lookup at each projection, bind, intent,
  or world-frontier use site. The declaration lookup owner remains the active
  inventory view; projection/action owners consume the recovered declaration
  only to emit already-lowered runtime synchronization code. The old
  `find_zone_decl`, `find_world_decl`, `find_relation_decl`, and
  `find_effect_decl` C backend shortcut wrappers are retired; world-embedded
  zone recovery must consume `transpiler_resolve_world_zone_decl(...)`, and new
  direct shortcut wrappers are not allowed.
- LLVM projection nominal lookup lives in `src/codegen/llvm_domain_lookup.c`
  behind `llvm_find_projection_nominal_decl(...)`. Consumers that still need a
  compatibility declaration may ask that owner for the nominal declaration, but
  domain projection value, projection path, member access, and spawn literal
  source-field paths must prefer by-name MIR declaration-header metadata and
  must not reopen a local class-inventory search or keep projection-specific
  compatibility wrappers.
- C and LLVM projection path/literal helpers expose only by-name lowering
  seams once a source type is known. The retired AST-declaration wrappers
  (`resolve_projection_source_path_rec`, `emit_projection_literal`, and
  `llvm_load_projection_path_value`) may not be reintroduced; callers must
  pass the resolved type name from the typed owner that already made the
  declaration decision. The projection class-field view helpers likewise do
  not accept AST compatibility declarations; in MIR-active paths they must use
  declaration headers through the hosted field view and fail closed if the
  field metadata is absent.
- LLVM callable declaration lookup also lives in
  `src/codegen/llvm_domain_lookup.c` behind `llvm_find_function_decl(...)`,
  `llvm_find_intent_decl(...)`, and `llvm_find_callable_decl(...)`. Call
  dispatch consumes the callable owner once and then branches on the returned
  declaration kind; it must not rediscover function and intent declarations
  separately. Existence-only consumers use the header-backed
  `llvm_*_decl_exists(...)` seams from the same owner, or
  `llvm_decl_exists_in_context(...)` for a single declaration kind; they must
  not recover origin AST declarations just to test presence. Boundary
  projection helpers may lower boundary call arguments, but they must not own
  function/intent declaration recovery.
- C callable declaration lookup lives behind `find_callable_decl(...)`.
  User-call emission, expression type inference, and MIR local call-type
  inference consume that owner once and then branch on the returned declaration
  kind. Existence-only C consumers use
  `transpiler_*_decl_exists_local(...)` and stay on MIR declaration headers in
  MIR-active paths. They must not reopen separate function/intent lookup chains
  or recover source declarations just to test presence.
- LLVM current-function declaration context lives in `LLVMGenCtx.current_func_decl`.
  Helpers that need the declaration of the function currently being emitted
  consume that field; they must not recover it by reading
  `LLVMGetValueName(ctx->current_function)` and looking the name up again.
- LLVM spawn generated-name policy lives in
  `src/codegen/llvm_expr_spawn_names.c`. Generic spawn specialization may append
  mangled type suffixes through that owner, but boundary projection helpers must
  not carry spawn naming utilities.
- Worker-boundary growable-storage classification names live in
  `src/common/worker_boundary_storage_policy.c`. Semantic may decide from
  `Type *`, and C/LLVM may decide from rendered type names or backend
  registries, but all user-facing storage kind names, constructor-name
  normalization, and rendered type-name prefix classification must consume the
  common policy owner. Do not restate local
  `Array` / `Slice` / `List` / `Queue` / `Set` / `HashMap` / `Channel` display
  strings in semantic or backend boundary diagnostics.
- C backend subject and projection host declaration checks consume
  `find_subject_host_decl(...)` or
  `transpiler_find_projection_nominal_decl_local(...)`.
  Projection field-path, method-invalidation, projection literal, overlay, and
  provenance owners must not reopen direct `find_class_decl(...)` probes for
  projection source/target host recovery.
- C backend nominal member type lookup lives in `src/codegen/transpiler_nominal.c`
  behind `transpiler_lookup_nominal_host_member_type_name(...)`. Expression
  type inference and MIR local type inference consume that owner for fallback
  member access typing; they must not reopen class-field compatibility directly.
- C backend C-type lowering diagnostics live behind
  `transpiler_require_ast_c_type_copy(...)` and
  `transpiler_require_type_name_c_type_copy(...)` when a `TranspilerCtx` is
  available. Backend owners may still call lower-level copy functions inside
  type-mapping/type-render owners or deliberately local caller-owned-buffer
  paths, but they must not silently convert an unsupported source type into an
  `Unknown` emitted C type. Specialized diagnostics remain closer to the source
  surface: an empty array literal without `Array<T>` annotation, for example,
  must emit the annotation fix before the generic C-type requirement helper is
  allowed to run. Result/tuple specialization fallback preserves user-type names
  through `transpiler_copy_c_type_or_user_type_name(...)`, keeping that policy
  behind the type-require owner instead of reopening raw type mapping in the
  specialization registry.
- party-slot ability tag selection lives in
  `src/codegen/transpiler_role_ability.c`. Bind emission and member-call
  emission may consume `transpiler_party_slot_first_ability_tag(...)` or
  `transpiler_party_slot_method_ability_tag(...)`, but they must not reopen
  party role-slot scans locally. MIR-active generic ability tags fill omitted
  actuals from `MIRDeclGenericParam` type-name metadata, not from AST generic
  default/constraint nodes.
- Party / role / roster compiler facts live in the normal parser, semantic,
  declaration-inventory, and C/LLVM hosted-method path. Standalone compiler-only
  FiberMap extraction/generation APIs are not a beta source of truth; the old
  unused `src/compiler/party_compiler.h` proposal header was removed. Runtime
  FiberMap APIs remain runtime-owned and may only become compiler-owned after
  explicit AIR/MIR evidence is introduced.
- Runtime pointer/container ownership lives in
  `docs/128_pointer_risk_register.md` and the matching runtime owner files.
  C inline runtime and LLVM-linkable runtime exports must share the same
  ownership class for a stable surface. For example, `Channel<String>` is
  `container-owned` on both paths: send copies payloads, receive transfers
  ownership, and destroy frees pending messages.
- ABI layout facts live in `src/runtime/pgy_abi_spec.h`, with compile-time
  assertions in `src/runtime/pgy_abi_spec_asserts.h` and MIR consumer facts in
  `src/compiler/mir_abi_layout.c`. `Option<T>` is currently an explicit tagged
  layout (`MIR_ABI_REPR_EXPLICIT_TAG`) with `niche_none_pattern == NULL`.
  Rust-style niche encoding may only appear after semantic/DAG proof types
  such as `NonZero<T>`, `NonNull<T>`, or `NonEmpty<T>` authorize a reserved
  bit pattern and MIR records it as an ABI fact. C and LLVM backends must not
  infer `None` as zero/null locally. User-directed explicit layout, packed
  structs, offsets, and union overlap are future `unsafe(ffi, layout)` /
  raw-boundary capability work, not ordinary aggregate semantics.
- Stable diagnostic literals live in `src/semantic/diag_codes.h` and are
  mirrored by `docs/72_diagnostic_codes.md`. Driver JSON, LSP diagnostics,
  parser/lexer routing, semantic diagnostics, and backend diagnostics consume
  those literals; they must not invent ad-hoc `code`, `cause_ir`, or
  `fix_source` strings.
- Self-hosting is a post-beta consumer, not the language-completion source of
  truth. Soft self-host tools under `docs/self_hosted/` may consume stable JSON
  and diagnostics contracts as dogfood, but they must not reorder beta closure
  ahead of CFG/AIR/DAG/MIR/ABI language-trust work. The diagnostic catalog
  checker is useful evidence for tooling readiness; it is not a reason to start
  compiler-core self-hosting before the language spine below is closed.
- Python may improve local validation, but it is not the source of truth for
  beta gates. Mandatory smokes must either provide a shell/C/compiler fallback
  or fail when an explicitly supplied required binary/input is unavailable.
- Beta/source-of-truth smoke scripts must remain compatible with macOS Bash
  3.2. `tests/build_source_inventory_smoke.sh` owns the drift alarm for Bash
  4-only `mapfile` / `readarray`, associative arrays, parameter
  case-conversion expansions, and case-pattern continuations that end in `|\`.
  Use explicit while-read loops or `if` checks instead.
- Makefile shell helpers are part of the beta tooling source of truth.
  `pgy_mkdir_p` and `pgy_touch_ref` must not invoke nested login-shell bash in
  local MinGW/Git Bash builds; `tests/build_source_inventory_smoke.sh` owns
  the drift alarm for this because a stalled helper prevents compiler gates
  from running at all.
- `ci-windows` must run executable MinGW tests only across a real MSYS2 bash
  runtime boundary. Git Bash can remain a local direct-target fallback, but it
  is not equivalent to MSYS2 for MinGW compiler invocations and must fail fast
  in the Windows CI preflight instead of surfacing as a later silent `gcc`
  compile failure. `Makefile` owns the shell selection order, and
  `tests/build_source_inventory_smoke.sh` gates the MSYS2-first terms.
- Windows/MSYS executable smokes must call `pgy_prepend_windows_runtime_paths`
  before probing built `.exe` binaries. A missing DLL path is an environment
  setup problem, not a successful skip when the binary was explicitly built.
  Compiler input paths passed from bash to a Windows executable must go through
  `pgy_path_for_compiler(...)`, so the gate validates the compiler behavior
  instead of a path-translation accident.
- CFG body-dataflow, example, tooling, observability, memory/concurrency,
  codegen determinism, runtime panic ABI/codegen, perf baseline, and LLVM
  campaign smokes that receive an explicit required binary/toolchain must fail
  if that binary or toolchain is missing or cannot be launched. Source-only
  fallback is allowed only when the target intentionally has no required
  executable. Timing gates must have a minimal shell fallback; `/usr/bin/time`
  and Python are not beta CI requirements.
- Formatter, module, package-module, and stdlib surface smokes are beta surface
  gates. They must use the shared Windows/MSYS path helper for compiler inputs
  and outputs rather than relying on POSIX paths accidentally accepted by a
  Windows executable.
- IR pipeline and Unicode policy smokes are also beta trust gates. They must
  use the same path helper and fail closed when an explicit compiler binary is
  unavailable.
- `tests/build_source_inventory_smoke.sh` owns the drift alarm for this path
  helper contract. Adding a beta executable smoke without
  `pgy_binary_path_helpers.sh` and `pgy_path_for_compiler(...)` is a source
  inventory failure unless the smoke delegates all compiler execution to a
  helper such as `tests/compare_backends.sh` that owns path conversion.
- `tests/pgy_binary_path_helpers.sh` also owns executable-format
  classification. Backend compare, perf, bench, dogfood, and ABI precheck
  paths must fail closed when an explicit binary is not runnable on the current
  host. The check must not depend only on filename suffixes or `file(1)` text;
  PE/ELF/Mach-O magic bytes are part of the owner contract.
- The same helper owns optional `.exe` selection and runnable-binary rejection
  for P0 executable smokes. Formatter, stdlib, AIR JSON schema, runtime-none,
  raw-escape, and semantic fixture-isolation probes must not execute a binary
  after only checking `-x`; they must call the shared runnable guard first.
- Git for Windows runtime mounts (`C:\Program Files\Git\mingw64\bin` and
  `...\Git\usr\bin`) are not MinGW/LLVM runtime evidence. Bash and PowerShell
  launch helpers may leave existing Git Bash PATH entries later in `PATH`, but
  they must not prepend those mounts ahead of explicit LLVM/MSYS2/MinGW roots.
- Backend-compare PowerShell fallbacks must derive their launch prefix from the
  current Bash `PATH` after `setup_windows_launch_path(...)` has added the
  compiler, generated binary, and toolchain directories. A fallback that only
  consumes the static LLVM/MSYS candidate prefix can re-open Windows exit-127
  drift even when the direct Bash launch path was prepared correctly.
- `tests/compare_backends.sh` owns C/LLVM backend-compare case inventory. A
  fixture under `tests/cases/backend_compare/**/main.pgy` must either be in
  the default case array or be run through an explicit targeted command; the
  inventory-only smoke also rejects stale default entries whose fixture
  directory was removed. This closes the drift where passing backend parity
  cases existed on disk but were not part of the frozen parity suite.
- `tests/semantic_core_shape_smoke.sh` gates these ownership boundaries. The
  test is a drift alarm; the owning `.c` files above are the source of truth.
  `tests/mir_declaration_inventory_smoke.sh` gates the backend declaration and
  party-slot helper ownership boundaries.

### 0.1 Mismatch Containment During Lowering

When a real system has an unavoidable mismatch between business meaning and
machine/runtime constraints, the mismatch must be contained at the IR lowering
boundary. It must not leak upward as business logic boilerplate.

The mismatch appears where two worlds cross: continuous sensor or UI inputs
enter finite program state, DTOs become domain values, or HIR business meaning
is lowered into MIR memory/register/control-flow facts. The architect's job is
not to pretend this projection error is zero. The job is to isolate it so that
domain code does not accumulate `try`/`catch`, null checks, tolerance checks,
manual cleanup, and backend-shaped helper calls.

The rule is:

- business code states the domain fact, intent, resource boundary, or authority
  requirement once;
- the owning IR stage expands that fact into explicit lower-level operations;
- later stages consume the lowered fact and may attach diagnostics/provenance;
- no backend or runtime helper may require the user to restate the lowered
  mechanics in source code.

The placement rule is:

1. **Compiler/language boundary: HIR -> MIR lowering.** If a high-level intent,
   zone, projection, resource, or ownership fact cannot be lowered safely, the
   compiler must either reject it with a diagnostic or require an explicit
   source boundary such as `unsafe`. It must not silently patch the gap with
   backend-local helpers.
2. **Application boundary: DTO -> value-object conversion.** External noise,
   tolerance, missing data, and platform shape mismatch belong at the API,
   sensor, host-import, or module boundary. Once accepted as a domain value, the
   inner domain function should not revalidate the same projection friction.
3. **Control-flow boundary: `Result<Success, Failure>`.** Predictable mismatch
   is recoverable failure, not hidden exception flow. Boundary operations that
   can fail must carry that failure in the type/contract path so callers handle
   it explicitly.

Examples:

- `intent`/`zone` source code should not hand-author scheduler frontier loops;
  RIR/AIR/MIR owns propagation evidence and bounded recompute facts.
- Slot/Pin source code should not manually duplicate every cleanup edge; MIR
  cleanup facts own exit-edge materialization and C/LLVM only emit it.
- Domain ownership, authority, and projection freshness should not be encoded
  as ad-hoc helper calls in business code; DIR/RIR/AIR owns the mismatch and
  presents stable diagnostics when it cannot lower safely.

This is the answer to the architect's question: if the mismatch cannot be
eliminated, lowering must make it explicit in IR, prove or reject it there, and
keep the source-level business vocabulary clean.

## 1. Ownership Table

| Concern | Source of truth | Consumers | Forbidden pattern |
|---|---|---|---|
| Parsed syntax and source spans | AST | Diagnostics, lowering provenance | Backend semantic rediscovery by walking AST |
| Body control flow | HIR CFG | MIR lowering, semantic body facts, AIR evidence | AST helper deciding reachability or all-path return |
| HIR routine inventory | `hir_public.c` const/mutable routine inventory accessors | AIR HIR evidence, HIR validation, HIR callgraph, MIR lowering, RIR flow enrichment, future HIR consumers | Consumers reopening `hir->routines` or `hir->routine_count` directly outside HIR construction/destruction/public owners |
| RIR scope inventory and scope item views | `rir_public_surface.c` const/mutable scope inventory, fact, op, and state-summary accessors | AIR RIR evidence, RIR validation, RIR/DIR validation, RIR dump/JSON output, RIR flow enrichment, MIR cleanup/lowering, future RIR consumers | Consumers reopening `rir->scopes`, `rir->scope_count`, `scope->facts`, `scope->ops`, or `scope->state_summaries` directly outside RIR owners |
| Body safety facts | MIR CFG/dataflow | C backend, LLVM backend, AIR evidence | Backend-local cleanup/drop/pin rules |
| MIR source shape and source-location compatibility | MIR source-shape owner | MIR validators, DCE, C/LLVM emitters, dumps | Consumers reopening raw `source_node_type` / `source_line` fields |
| MIR compatibility AST payload | MIR source-shape owner | C/LLVM residual source emitters, diagnostics, validators | Consumers reading `inst->ast` directly outside MIR construction/population/source-shape owners |
| Declaration/domain inventory | DIR/RIR/MIR declaration headers | C/LLVM declaration emitters | AST-carried backend inventory as final truth |
| Host declaration compatibility lookup | `host_decl_compat.c` type/name table | C/LLVM host method lookup, pointer-self policy, no-MIR compatibility paths | Partial class/enum-only fallback chains that omit party/role/roster/domain hosts |
| MIR public inventory/query/pass wrappers | `mir_public_surface.c` consuming `mir_program_inventory.c` const/mutable routine views | C/LLVM inventory views, MIR tests, public liveness/DCE/fallback-summary pass wrappers | Public query/DCE/liveness wrappers living in the lowering implementation header or reopening raw MIR routine arrays |
| MIR routine/program structure facts | `mir_program_inventory.c` routine inventory and program-shape accessors | AIR MIR evidence, C/LLVM inventory views, MIR public pass wrappers, main-wrapper selection | Consumers reopening `mir->routines`, `mir->routine_count`, `mir->has_main_function`, or `mir->has_top_level_exec` directly |
| Backend routine inventory wrappers | `llvm_inventory_internal.c` and `transpiler_inventory_view.c` | C/LLVM declaration emitters, intent emitters, MIR contract validation, hosted method views | Backend consumers indexing `inventory->routines[...]` instead of the wrapper accessor |
| MIR inventory surface usage | `mir_surface_usage.c` summary and recorded-fact accessors | MIR validators, C/LLVM runtime-dependency selection for thread-pool and intent observability | Consumers reading `mir->has_inventory_surface_usage_facts` or `mir->inventory_uses_*_surface` directly |
| Type/declaration dependency | Type-resolution DAG metadata | Semantic owners, AIR DAG evidence | Recursive resolver fallback on frozen paths |
| Generic/ability contract evidence | Type-resolution DAG + `semantic_role_decl_has_ability(...)` | Semantic contract checks, role/party bind checks, AIR | Program-root based ability match helpers or compatibility counters as semantic truth |
| Semantic host declaration lookup | `semantic_find_*_decl_by_name(...)` in `type_checker_host_helpers.c` | Constructor lookup, callable lookup, class/ability/enum/function lookup, ownership consumers | Public raw `find_type_decl_by_name`, `find_ability_decl_by_name`, `find_callable_decl_by_name`, or `find_type_alias_decl` program-root helpers |
| Hosted method body summary | The checked `Host_Method` function symbol type, reached through `expr_host_method_function_type(...)` | Current-host method calls, instance method calls, intent authority-sensitive call detection, effect/body-summary propagation, parallel secure-call checks | Recomputing hosted method effects from AST-only declarations, dropping `semantic_record_callee_body_summary(...)` for instance calls, or treating AST method arrays as the body-summary source of truth |
| Semantic domain declaration lookup | `semantic_find_*_decl_by_name(...)` in `type_checker_host_helpers.c` | Action contracts, intent where/using/causes, world embedding, zone authority | Public raw `find_domain_decl_by_name(...)` or any new semantic `ASTNode *program` lookup declaration |
| Role declaration lookup | `semantic_find_role_decl_by_name(...)` / `semantic_find_next_role_decl_for_type_name(...)` | Ability matching, bind validation, operator overload lookup, role declaration validation | Public raw `semantic_find_role_decl(ASTNode *program, ...)` |
| Projection source field path | `semantic_resolve_projection_source_field_path(...)` | Projection diagnostics, zone graph metadata, DAG projection materialization | Re-exposing `resolve_projection_source_field_path(ASTNode *program_root, ...)` or local class lookup |
| Stdlib use declaration validation | `SemanticContext.stdlib_use_module_*` inventory in `type_checker_stdlib_use.c` | Duplicate `use` warnings, stdlib surface validation | Re-scanning `ctx->program_root` from the stdlib-use consumer |
| Ref-parameter escape compatibility | `semantic_callable_param_escape_summary(...)` call-contract owner seam; checked function types that already prove no `BODY_SUMMARY_MAY_ESCAPE_REF` bypass the legacy AST analyzer, including checked empty summaries, and escaping refs must aggregate into `BODY_SUMMARY_MAY_ESCAPE_REF` | Ownership call checks, function param summary checks, transitive callee body summaries | Re-exposing `semantic_legacy_ast_callable_param_escape_summary(...)`, making ownership consumers call the slot analyzer directly, forcing legacy AST analysis when typed body-summary facts are already decisive, or dropping the body-summary aggregation |
| Party bind statement validity | `type_checker_bind_stmt.c` | CFG/body flow, C backend bind emit, LLVM bind emit | Backend-only bind validation or silent LLVM bind skips |
| Intent step domain declaration recovery | `type_checker_intent_types.c` intent domain owner seam | Intent step validation, derived using, step causes checks, participant transfer-source checks, transfer contract checks | Consumers reopening `AST_ZONE_DECL` / `AST_EFFECT_DECL` lookup locally |
| Resource/authority/effect propagation | RIR | AIR, runtime/codegen policy emitters | AIR or backend inventing authority/resource facts |
| Cleanup/drop/pin topology | MIR cleanup facts | C/LLVM cleanup emitters, AIR | Topology-only cleanup without expected fact payload |
| Abstraction boundary drift | AIR | Driver diagnostics, CI, LSP/JSON consumers | Backends consuming AIR for codegen |
| AIR evidence provenance | `air_evidence_node.c` append owner | AIR validators, dumps, driver diagnostics, LSP/CI JSON consumers | Empty provider/subject or zero-fact evidence entering inventory and being repaired later |
| Runtime pass/failure policy | Runtime policy headers | C/LLVM codegen wrappers, AIR global evidence | Duplicated pass limits or failure strings in emitters |
| ABI surface | ABI/runtime headers | C/LLVM, tests, docs | Domain layer leaking layout changes into C FFI silently |
| Runtime pointer/container ownership | `docs/128_pointer_risk_register.md` + runtime owner files | C inline runtime, LLVM exports, ABI smoke, generated code | C inline path and LLVM export path using different ownership classes |
| Diagnostic code/cause/fix vocabulary | `src/semantic/diag_codes.h` + `docs/72_diagnostic_codes.md` | Driver JSON, LSP, parser/lexer, semantic, C/LLVM backend diagnostics, soft self-host diagnostic checker | Bare diagnostic routing strings or prose-only diagnostics on stable paths |
| Diagnostic JSON shape | Driver diagnostic JSON emitter + `diagnostics-json-test-smoke` | CLI tooling, LSP bridge, soft self-host diagnostic checker | Python-only validation or regex-only prose matching as the only gate |
| Post-beta self-host tool output | Stable diagnostics/JSON contracts after language spine closure | First Pergyra-written diagnostic/AIR/MIR tools, CI oracle comparison | Treating self-hosting as a beta source-of-truth owner |
| Unsafe/raw capability scope | `docs/132_unsafe_capability_scope.md` plus semantic/AIR gates once implemented | Parser diagnostics, semantic raw escape diagnostics, ABI lowering, self-host roadmap | Plain `unsafe { ... }` granting raw/system-tier escape |
| Runtime-none surface scan | `src/compiler/runtime_none_contract.c` | Driver runtime profile diagnostics, runtime-none smoke, future no-runtime lowering | Early-success scanning of only the first array/tuple literal element |
| Mandatory smoke portability | Owning smoke script plus Makefile target | CI, minimal Windows/Git Bash, soft self-host oracle tools | Python-only validation or explicit required binaries being skipped as success |
| Build source inventory | Makefile source/object inventory | CI, local smoke targets, dependency inclusion | Shell `find` rediscovering build artifacts or source files on Windows paths |
| Local build artifact ownership | One `BUILD_DIR`/`BIN_DIR` pair per active make process | Local gates, CI recipes, troubleshooting docs | Parallel gates sharing the same `build/` and corrupting `.o` files |

### MIR/LLVM Declaration Bootstrap Proof Rows

The declaration bootstrap blocker is measured row-by-row. A row is closed only
when the owner, consumers, and smoke gate are named together. It is not enough
to reduce a fallback counter or move code behind a helper.

| Row | Old fallback / drift risk | Source-of-truth owner | Gate | Status |
|---|---|---|---|---|
| Host declaration type set | Partial class/enum/domain chains that miss party/role/roster | `host_decl_compat.c` host type/name tables | `mir-declaration-inventory-test-smoke` rejects partial hard-coded host chains, local C owner/lookup tables, backend-local host-name switches, direct host-name reads in lookup paths, direct host-name reads in shared field/type lookup consumers, generic-class naming, declaration emit owners, authority checks, and projection/effect/bind/domain-query sync host-name reads. `backend-fail-closed-test-smoke` also fast-gates that LLVM host lookup consumes the compat type table instead of reopening a partial host-kind chain. | Closed for C/LLVM lookup compatibility; direct AST host-name reads are confined to `host_decl_compat.c` |
| Hosted method identity | AST method-array name lookup when MIR metadata exists | `mir_decl_header_method(...)` plus `mir_decl_method_name(...)` through C/LLVM hosted method views | `mir-declaration-inventory-test-smoke` rejects AST method-array lookup helpers, old `*_method_ast` names, backend raw method-name field reads, and hosted-view MIR method-array exposure | Closed for lookup-visible identity |
| Hosted method signature/action contract | AST method param/return/action reads in LLVM/C prototype emitters, member-call argument policy, zone action runtime lowering, or world embedded action/effect sync | `mir_decl_header_method(...)`, `mir_decl_method_param_count(...)`, `mir_decl_method_param(...)`, `mir_decl_method_param_type_name(...)`, `mir_decl_method_return_type(...)`, `mir_decl_method_return_type_name(...)`, `mir_decl_method_is_async(...)`, `mir_decl_method_is_action_like(...)`, `mir_decl_method_within_zone(...)`, and `mir_decl_method_causes_effect(...)` | MIR validator rejects signature metadata drift; smoke rejects direct method param/return reads in guarded consumers, hosted-method view wrappers, hosted-view MIR method-array exposure, LLVM nominal registration signature rediscovery, LLVM domain/role forward signature rediscovery, C/LLVM role operator signature rediscovery, LLVM member-call pointer-self argument adjustment, LLVM hosted-self logical parameter metadata consumption, C member-call param/return wrapping metadata consumption, LLVM let/type inference return-type rediscovery, LLVM zone action method-contract rediscovery, LLVM world embedded sync contract rediscovery, and C zone/world sync contract rediscovery | Closed for frozen hosted-method prototypes, C/LLVM member-call signature consumers, C/LLVM method return/type inference consumers, C/LLVM role operator signatures, LLVM nominal/domain/role method forward and registration signatures, LLVM hosted-self call signature consumers, LLVM member-call argument policy, LLVM zone action method-contract lowering without source-AST method presence, and C/LLVM zone/world action/effect sync contract input |
| Hosted method routine link | AST/name-based routine search or unchecked routine index reuse | `mir_decl_method_routine_index(...)` plus routine owner/name validation | MIR linker requires method owner/name equality; MIR validator rejects routine index overflow and routine link metadata drift; smoke rejects local routine search helpers and backend raw routine-index field reads | Closed for hosted method body selection |
| MIR routine signature metadata | Function/method routine emission reconstructs params and return type from `source_ast` | `MIRRoutine` signature fields plus `param_type_names` / `return_type_name` populated during MIR lowering and consumed through `mir_routine_*`, `transpiler_mir_routine_*`, and `llvm_mir_routine_*` accessors; intent participant/value forward, entry, LLVM MIR parameter, call-site, and early eligibility signatures are carried by MIR intent carrier rows; ordered `IntentBinding(kind, alias, type)` rows carry intent binding order for LLVM MIR type/parameter lowering, C/LLVM MIR-backed intent call argument lowering, C/LLVM intent forward declarations, C prologue signature/entry emission, C early forward eligibility, C MIR emission-mapping alias seeding, C MIR zone-slot/bind/rebind/restore/rollback emission, LLVM entry binding/count setup, and LLVM intent trace materialize/transfer handle lookup; intent declaration-level `priority` / `success` contracts are MIR `IntentEval(priority)` / `IntentCheck(success)` rows anchored by the intent name | `mir-declaration-inventory-test-smoke` requires signature fields, compiler/C/LLVM accessors, C/LLVM forward-declaration consumption, C MIR signature eligibility consumption, LLVM MIR-backed function/parameter emission consumption, C MIR-backed function/SSA-local parameter consumption, C SSA mapping precheck parameter seeding, C MIR mapping precheck intent alias seeding, `IntentValue` and `IntentBinding` materialization, declaration-level intent priority/success carrier materialization and C/LLVM consumption, ordered binding consumption in C/LLVM forward/entry/prologue setup, C/LLVM call-site lowering, and C/LLVM early forward eligibility, plus AST binding-array rejection in LLVM MIR type/param lowering, LLVM entry setup, C MIR emission mapping, C MIR zone-slot binding, C/LLVM forward declaration, C/LLVM call-target lowering, C prologue, and C early-eligibility emission, old separate participant/value collector rejection in LLVM MIR type construction and C MIR mapping precheck, LLVM implicit-self placeholder rejection in declaration lowering, LLVM enum method payload-self metadata checks, LLVM entry-binding `i8ptr`/alias synthesis rejection, LLVM call-target forward `i8ptr` placeholder rejection, and LLVM trace handle zero-synthesis rejection | Partial closure: routine-backed C/LLVM function body emission, C MIR signature eligibility, C forward parameter signatures, C/LLVM forward eligibility, LLVM forward return/parameter signatures, LLVM MIR function/parameter type construction and parameter alloca lowering without separate participant/value collectors, C SSA mapping precheck parameter seeding, C MIR mapping precheck intent alias seeding from ordered carrier rows without separate participant/value collectors, C/LLVM intent forward declaration ordered binding consumption and fail-closed checks without separate MIR participant/value collectors in C/LLVM forward, LLVM entry setup, or C early eligibility, C intent body/prologue fail-closed checks including alias/type row completeness and ordered binding metadata completeness through `IntentBindingMetadataView`, C/LLVM routine-backed no-step intent declaration entry/prologue lowering, C/LLVM declaration-level priority/success consumption through MIR intent carriers, C MIR zone-slot/bind/rebind/restore/rollback ordered binding consumption and fail-closed checks, C/LLVM routine-backed intent call-site ordered binding validation and arity checking from ordered row count without AST participant/value count comparison or separate participant/value collector use, C early forward eligibility ordered binding consumption and fail-closed checks, LLVM entry/count setup ordered binding consumption without MIR-only AST count pre-read, separate participant/value collectors, `i8ptr` type seeding, synthesized `"param"` aliases, or trace handle `0` synthesis, LLVM call-target forward declaration without `i8ptr` parameter placeholders, LLVM MIR ordered binding order consumption, and LLVM call-site/implicit forward-declaration fail-closed checks are metadata-first for routine payloads; LLVM forward declaration no longer substitutes `i32` for missing binding type metadata, LLVM function declaration lowering no longer substitutes `i32` for implicit `self` when current host metadata is unavailable, and LLVM enum method registration now distinguishes no-payload enum `i32` ABI from payload enum struct metadata requirements; remaining intent value consumers outside signature/setup/call-site lowering and AST-only event-handler declarator compatibility remain |
| Role implementation methods | Role impl methods omitted from declaration headers or re-found through owner/name body/operator lookup | `mir_decl_header_set_role_impl_methods(...)`, consumed through `TranspilerHostedMethodView` / `LLVMHostedMethodView` and role-operator `MIRDeclMethod` metadata lookup | Smoke requires role impl metadata recording, rejects role method-count exceptions, rejects the retired C owner/name role routine lookup helper, and rejects LLVM role operator bridge AST method lookup | Closed for role method inventory count, C/LLVM body link, and LLVM operator bridge method selection |
| Host field compatibility view | Class fields and domain shared fields reopened through separate backend switches, or C identifier lowering guessed between a lexical local and an implicit host field from stale SSA snapshots | `host_decl_compat.c` class/shared-field compatibility views, C/LLVM declaration-inventory hosted field views, and `transpiler_host_field_identifier.c` for C current-host identifier emission | Smoke requires C/LLVM constructor channel, class/domain constructor lowering, generic class specialization emission, LLVM domain-parts splitting, MIR SSA zone-field lookup, nominal/current-field, member/local type inference, overlay projection, projection-path helpers, declaration/register emitters to consume hosted field views or field lookup owner seams, C host-field identifier owner coverage, self-receiver gating, and stale host-field snapshot rewriting | Closed for backend compatibility and C current-host identifier lowering; direct codegen field-array access is confined to `host_decl_compat.c`, direct class/shared compatibility-view calls are confined to `host_decl_compat.c`, `transpiler_decl_lookup.c`, and `llvm_inventory_decl_lookup.c`, direct class-field compatibility lookup is confined to `host_decl_compat.c`, and C field/local shadowing no longer treats MIR source-local field write facts as lexical locals. Backend consumers still need to migrate to MIR declaration-field metadata |
| Type-alias target metadata | C/LLVM alias handling reopened `ast_type_alias_target_type(...)` after declaration lookup, or let/local collection lowering treated alias names as container type names | `MIRDeclHeader.type_alias_target_type_name`, resolved through declaration-header inventory accessors and consumed by MIR source-local type facts | `mir-declaration-inventory-test-smoke` requires the header field, accessor, inventory resolver, lifecycle cleanup, validator drift diagnostic, C alias emitter consumption, LLVM alias type-map consumption, LLVM render-context consumption, MIR source-local alias canonicalization, C MIR preserved-let source-local consumption, LLVM MIR local element metadata consumption, LLVM collection-let source-local consumption, and MIR-active fallback guards. `type_alias_array_context` backend fixture proves C/LLVM empty `Array<T>` alias context parity | Closed for C type-alias declaration emission, LLVM alias type mapping/rendering, C/LLVM source-local collection contexts, and C/LLVM empty array literal contexts in MIR-active paths; non-MIR AST compatibility fallback remains explicit |
| Source/provenance payload | Treating `source_ast` as semantic inventory truth | `MIRDeclHeader.source_ast`, `mir_decl_header_source_decl(...)`, `mir_decl_method_source_ast(...)`, `mir_decl_field_source_ast(...)`, and backend `*_source_ast` compatibility accessors are retired; `mir_routine_source_decl_of_type(...)` is compiler-owned only and has no codegen callers | Smoke rejects generic fallback naming, old AST method-array state, backend raw `decl_header->source_ast` / `method->source_ast` reads, backend raw routine `ast` reads, backend `*_source_ast` routine wrappers, the retired method/field/header source accessors, the retired LLVM method body AST compatibility accessor, synthetic-executable AST-returning function lookup, C hosted-method body emission that recovers method source AST instead of passing the linked MIRRoutine directly, C class/zone specialization scans that recover method source AST instead of linked MIR routine facts, C codegen calls to `mir_routine_source_decl_of_type(...)`, LLVM MIR body source-decl recovery, LLVM intent body emission that recovers routine source declarations, and backend/compiler calls to `mir_decl_header_source_decl(...)`; ast_read_surface ratchets source_ast and source_decl at codegen 0 / compiler 0 and routine_source_decl_codegen 0 | Backend and compiler `source_ast`/`source_decl` frontiers closed; C/LLVM hosted method body emission is MIR-routine-only; C class/zone collection-specialization scans are MIR-routine/source-local-fact based; C host-method AST lookup is non-MIR compatibility only; LLVM generic function templates register and specialize through MIRRoutine entries; LLVM MIR body emission consumes routine kind/signature/current-routine metadata without recovering source declarations; LLVM intent body emission starts from active declaration inventory and fail-closes on MIR intent routines without declaration rows; backend declaration payload compatibility validates MIR header rows before active-inventory payload lookup |
| Declaration field metadata | Field/slot/member shape was available only through AST compatibility views | `MIRDeclField` rows populated by `mir_decl_headers.c` and validated by `mir_decl_header_validate.c`; `MIRDeclFieldClaim` claim-shape rows populated by `mir_decl_headers.c` and smoke-gated for consumers | `mir-declaration-inventory-test-smoke` requires `MIRDeclField`/`MIRDeclFieldKind`, field metadata storage, class/shared/role/roster/world/domain/zone-layer field kinds, compiler accessors, field drift diagnostics, C nominal lookup consumption, C class constructor field emission through `TranspilerHostedFieldView`, C annotated-let class constructor delegation to the same constructor owner, C class/generic-class field emission, C party/roster shared-field declaration emission, C projection literal field iteration, C projection invalidation target-field matching, C projection field-path relevance/vessel checks, C relation/effect/world/zone struct shared-field declaration emission, C nominal zone member lookup, overlay zone-field presence checks, overlay zone effect/relation bind lookup, and zone struct layer-slot emission through `TranspilerHostedZoneLayerSlotView`, C party/roster/relation/effect/zone/world constructor shared-field argument/default emission, C constructor Channel shared-field scanning, C projection literal/source-path and overlay-projection invalidation class-field iteration through `TranspilerHostedFieldView`, C projection literal/source-path by-name lowering through MIR declaration headers, C MIR SSA implicit zone layer/shared-field recovery through `TranspilerHostedZoneLayerSlotView` and `TranspilerHostedSharedFieldView`, C projection zone-layer lookup, projection sync layer iteration, intent block caused-effect layer marking, and zone sync/frontier layer iteration through `TranspilerHostedZoneLayerSlotView`, C counted zone frontier pass-limit emission, C zone specialization emission through `TranspilerHostedSharedFieldView`, C overlay/world shared-field presence checks through `TranspilerHostedSharedFieldView`, C party role-slot struct/vtable emission and bind dispatch through `TranspilerHostedRoleSlotView`, C world member type lookup/constructor/declaration emission through `TranspilerHostedWorldZoneSlotView`, C/LLVM class field-claim helper emission through `MIRDeclFieldClaim`, LLVM constructor class-field expected-type metadata/channel checks through `LLVMHostedFieldView`, LLVM current-host Channel target resolution through `LLVMHostedFieldView` type-name metadata, LLVM generic class specialization field type-name lowering through `LLVMHostedFieldView`, LLVM projection/domain-projection source-path field iteration through `LLVMHostedFieldView`, LLVM domain projection value source-path by-name lowering through MIR declaration headers, LLVM projection-borrow/member source-path by-name lowering through MIR declaration headers, LLVM nominal struct field registration through `LLVMHostedFieldView`, LLVM constructor Channel/default shared-field scanning through `LLVMHostedSharedFieldView`, LLVM domain struct shared-field type/layout registration, LLVM zone struct layer-slot type registration, layer-slot field registration, LLVM world zone-slot struct type/field registration through `LLVMHostedWorldZoneSlotView`, zone bind layer-slot lookup, zone-layer query lookup, zone frontier previous-state/reset/continue tracking, zone sync action-cause layer iteration, zone action effect-layer emission, world embedded effect sync layer iteration, and intent effect caused-layer emission through `LLVMHostedZoneLayerSlotView`, LLVM domain declaration-parts cleanup, C/LLVM constructor Channel guard consumption, LLVM field-class lookup consumption, and LLVM party role-slot struct/type/ability lookup through `LLVMHostedRoleSlotView` | Closed for metadata creation, validation, C projection literal/source-path by-name lowering, C/LLVM class field-claim metadata lowering, LLVM constructor field expected-type name lowering, LLVM current-host Channel target inner-type resolution from field type-name metadata, LLVM generic class specialization field type-name lowering, LLVM domain projection value by-name lowering, LLVM projection-borrow/member source-path by-name lowering, and current C/LLVM field consumers; remaining declaration/projection emitter migration remains |
| Zone authority metadata | C/LLVM authority check prelude reopened zone authority children to find the authority subject slot and written ability refs | `MIRDeclZoneAuthority` rows populated by `mir_decl_header_authority.c` and validated by `mir_decl_header_validate.c`; required ability refs share the MIR ability-ref capture owner with role slots | `mir-declaration-inventory-test-smoke` requires the MIR authority accessors in C/LLVM authority consumers and rejects `ast_zone_authorities(...)` / `ast_zone_authority_subject_slot_name(...)` in those codegen paths; `test-mir` preserves a generic authority ability ref and rejects authority metadata drift | Closed for C MIR function-entry authority checks and the LLVM authority owner; semantic zone-authority validation still owns source-AST declaration checking before MIR lowering |
| Zone refresh metadata | Projection lowering reopened `AST_ZONE_REFRESH` to read object/source slot names and explicit target-to-source field maps | `MIRDeclZoneRefresh` rows populated for relation/effect/zone declarations by `mir_decl_header_refresh.c` and validated by `mir_decl_header_validate.c`; C relation/effect declaration sync, C relation/effect constructors, C zone declaration emission, C zone constructor projection-dirty initialization, C relation/effect/zone overlay/assignment projection invalidation, C relation/effect zone bind invalidation, and C world embedded effect sync consume them through `TranspilerHostedZoneRefreshView`; LLVM relation/effect/zone constructor projection-dirty initialization, LLVM relation/effect zone bind invalidation, zone projection sync, projection slot counting, projection-state struct fields, projection value lowering, zone assignment projection invalidation, and current-zone subject projection sync consume them through `LLVMHostedZoneRefreshView` and MIR refresh metadata accessors | `mir-declaration-inventory-test-smoke` requires refresh metadata storage, accessors, relation/effect/zone capture, validation drift checks, MIR tests that preserve field-map and relation/effect rows, C relation/effect sync/constructor/overlay/bind/world-effect-sync consumers, C/LLVM zone refresh views, C/LLVM projection value consumers, C/LLVM projection sync/layout/constructor/assignment-invalidation consumers, LLVM relation/effect/zone constructor/bind consumption, LLVM current-zone subject projection-sync consumption, and rejects reopened refresh inventory in C relation/effect declaration/constructor/overlay/bind/world-effect-sync paths, C zone declaration/constructor/overlay-invalidation emission, and LLVM declaration-parts/struct-field/constructor/bind/assignment-invalidation/projection-sync-call paths | Metadata capture plus C relation/effect sync/constructor/overlay/bind/world-effect-sync cutover, LLVM relation/effect/zone constructor/bind cutover, C and LLVM zone projection sync/struct-layout consumer cutover, C/LLVM assignment invalidation cutover, and LLVM current-zone subject projection-sync cutover closed; remaining LLVM zone action/world-effect-sync invalidation consumers and other zone bind/sync compatibility consumers still need MIR refresh view consumption before all AST refresh accessors can be retired |
| Dedicated declaration IR | `MIRProgram` still carries AST-shaped declaration payloads and some backend consumers still use compatibility field views | `MIRDeclHeader` / `MIRDeclMethod` / `MIRDeclField` / `MIRDeclZoneAuthority` metadata model, consumed through compiler accessors | Partial gate exists for methods, fields, and zone authority rows; full closure requires projection/declaration emitters to stop reopening compatibility views when MIR facts exist | Open beta blocker row |

C overlay projection invalidation now consumes MIRDeclMethod projection write/call
metadata captured by `mir_decl_method_projection.c`; the smoke rejects
recovering method source declarations from that path.

Callable/event-handler and tuple types are not losslessly representable in the
current `param_type_names` / `return_type_name` string cache.
`mir_render_type_name(...)` must therefore leave `AST_EVENT_HANDLER_TYPE` names
and tuple type names absent so C/LLVM consumers fall back to the retained AST
type node and produce function-pointer signatures or anonymous tuple structs
instead of treating the callable/tuple as `Int` or `Tuple`.

Collection ABI type names are different: `Array<T>`, `Slice<T>`,
`List<T>`, `Set<T>`, `Queue<T>`, and `HashMap<K, V>` are losslessly
representable as MIR type-name strings for frozen backend lowering. LLVM
parameter and local alloca emission must therefore register collection
metadata from `llvm_register_typed_var_abi_binding(...)` instead of requiring a
retained AST type node. `HashMap<K, V>` must populate both key and value
metadata from the ABI string before `MapHas` / `MapGet` / `MapSet` lowering.
The same owner applies to LLVM annotated typed-variable registration: when it
still receives an AST annotation, it first renders the concrete type name and
then parses constructed arguments from that rendered string. It must not reopen
`ast_type_generic_args(...)` or `ast_generic_param_constraint(...)` while
registering collection, channel, slot, future, or reference metadata.

LLVM MIR source-local `await` lowering follows the same rule. A local binding
such as `let value: T = await task`, an inline spawn binding such as
`let value: T = await spawn Work(...)`, residual statements such as
`Log(await task)` and `Log(await spawn Work(...))`, and nested expression cases
such as `Write(total, Read(total) + await task)` must have matching RIR/MIR
`AwaitLocal` or `AwaitRemote` resource operations before backend lowering
treats the await as a proved resource boundary. RIR expression walking must
therefore traverse expression containers such as binary/unary/member/array/
literal/cast nodes instead of only top-level statements and calls. LLVM must
recover the awaited `Future<T>` result type from the registered binding or from
the MIR routine source-local type-name fact. The resource-op name is boundary
evidence, not the only owner of the value ABI: local-vs-remote result shape
still comes from the Future binding/type fact. It must not require AST-payload
pointer identity to prove the resource fact, and a no-value initializer must
fail closed instead of letting LLVM verification discover a missing terminator
later.

LLVM MIR local type inference for `recv(channel)` and `await future` is also a
source-local type-name consumer. `llvm_mir_async_fact.c` may inspect the
expression shape to identify the channel/future operand, but `Channel<T>`,
`Future<T>`, and `RemoteFuture<T>` inner type recovery must come from
`mir_routine_source_local_type_name(...)` plus constructed-argument parsing,
not from re-reading the defining let's AST type annotation or spawn
initializer.

Note: the declaration-field metadata row's counted zone frontier pass-limit
consumer is now C/LLVM, not C-only; LLVM zone sync must consume
`LLVMHostedZoneLayerSlotView` counts and call
`pgy_domain_zone_frontier_pass_limit_from_counts(...)`.
World frontier pass-limit selection follows the same counted-fact rule. C and
LLVM world emitters count world zones/states and embedded zone frontier members
once through their owner views, then consume
`pgy_domain_world_embedded_frontier_count_from_zone_types(...)`,
`pgy_domain_world_transitive_frontier_pass_limit_from_counts(...)` and
`pgy_domain_world_derived_frontier_pass_limit_from_count(...)`; they must not
ask an AST wrapper to rediscover those counts at the emission site. The
frontier policy owner itself is AST-free; C/LLVM provide zone member counts
through backend callbacks that consume hosted zone-layer metadata. LLVM world
zone-slot query lookup follows the same row owner through
`LLVMHostedWorldZoneSlotView`; it must not reopen `ast_world_zones(...)`.
C world-field and zone-slot projection/query lookup follows the same row owner
through `TranspilerHostedWorldZoneSlotView`.
LLVM world constructor dirty-flag initialization also consumes
`LLVMHostedWorldZoneSlotView` instead of scanning world zone AST children.
LLVM world sync reset/change detection and directive zone-slot resolution also
consume `LLVMHostedWorldZoneSlotView`. LLVM world frontier zone-sync and
pending-dirty body emission take the same view directly, so this backend family
does not pass world zone AST child arrays across owner boundaries.

The field compatibility smoke has global codegen whitelists. Direct
`ast_class_fields(...)` or domain shared-field array access is allowed only in
`host_decl_compat.c` while C/LLVM consumers migrate from compatibility views to
`MIRDeclField` facts. Direct
`pgy_host_class_fields_compat_view_from_decl(...)`,
`pgy_host_shared_fields_compat_view_from_decl(...)`, and
`pgy_host_shared_field_compat_find(...)` calls are allowed only in the
host-compat owner and the C/LLVM declaration-inventory lookup owners. Direct
`pgy_host_class_field_compat_find(...)` calls are allowed only in
`host_decl_compat.c`. New codegen consumers must use hosted field views or
compiler MIR field accessors; they must not add new local AST field switches or
new direct compatibility-view callers.

LLVM declaration-field metadata failures use the same owner boundary. Missing
hosted field/slot/member facts must route through
`llvm_set_mir_inventory_missing(...)` so they carry the MIR inventory diagnostic
code, cause, and fix-source hints. The declaration-inventory smoke rejects the
old plain `"missing MIR declaration metadata"` / `"MIR declaration inventory
missing"` messages in LLVM codegen files.

Runtime frontier AIR evidence must count the complete frozen runtime policy
surface: pass-limit arithmetic facts plus bounded-overflow reason facts. A
backend may emit those strings, but it may not own or rename them.

Runtime frontier codegen may only consume the codegen policy wrapper
(`src/codegen/domain_frontier_policy.h`) for pass-limit selection. The runtime
policy header owns the arithmetic vocabulary, but C/LLVM emitters must not call
the runtime `pgy_frontier_*_pass_limit(...)` helpers directly. The wrapper is
the backend-facing seam that keeps domain declaration lookup and runtime
frontier arithmetic from mixing in emitter-local code. It does not expose
AST convenience wrappers; world-embedded frontier selection passes zone-slot
type names plus a backend-owned member-count callback, and emitters must derive
that member count from their hosted metadata views before calling the policy.

Semantic projection-field lookup has the same source-of-truth rule. Frozen
projection, intent-role, and constructor validators consume
`projection_source_field_count(...)` / `projection_source_field_at(...)` from
`type_checker_projection_path.c`; they must not reopen `ast_class_fields(...)`
locally to rediscover class-field arrays. The remaining direct semantic
`ast_class_fields(...)` use is limited to declaration validation, graph
collection, and the projection field owner itself until dedicated declaration
field metadata replaces AST-carried class fields.

## 2. Layer Contracts

### AST

AST owns raw parse structure, source spans, and user-facing syntax provenance.
AST does not own semantic truth after lowering begins.

Function declaration payload access is still an AST owner responsibility.
`AST_FUNC_DECL` covers both sync and `async func` declarations; consumers must
use `ast_func_*` accessors for shared declaration facts such as name, params,
return type, generic params, where clause, effects, body, access, and doc
comments. They must not rely on sync/async union field layout or add local
spawn/callable parameter dispatch.

Allowed AST use after semantic/lowering:

- source spans for diagnostics;
- source labels/names for provenance;
- compatibility payloads while a frozen MIR/DIR inventory path is being built.

Forbidden AST use:

- backend walking AST to decide safety;
- backend walking AST to rediscover declaration inventory;
- AST helpers deciding ownership, authority, effect, cleanup, or type success.

### HIR CFG

HIR CFG owns explicit body shape: basic blocks, control-flow edges, reachable
body regions, and source terminator provenance before MIR lowering.

HIR CFG answers:

- which paths exist;
- which body region owns a boundary;
- which source terminator produced a branch/return;
- whether a body can be lowered into CFG-owned MIR;
- whether CFG predecessor inventory shape is internally consistent before
  later MIR/dataflow consumers read it.

HIR CFG does not answer final resource cleanup, pin safety, or backend emission
shape. Those are MIR facts.

### MIR CFG/Dataflow

MIR owns beta body safety and backend execution facts.

MIR answers:

- all-path return and terminator provenance after lowering;
- source-statement emit facts for compatibility lowering;
- cleanup, rollback, invalidation, and pin cleanup edges;
- non-CFG fallback accounting;
- value summaries and liveness facts used by backends;
- declaration headers required for backend parity.

C and LLVM must consume MIR facts rather than duplicate their own body-safety
rules. If C and LLVM disagree, fix the MIR fact or the consumer, not a backend
heuristic.

Backend emission contracts must enter through
`mir_validate_emission_contract(...)`. That seam composes CFG topology
validation with MIR emission-fact validation before either C or LLVM inspects
cleanup/pin/source-shape details. Backends may still add target-specific
diagnostic text or unsupported-instruction checks, but they must not rebuild the
topology-plus-fact contract by calling the lower-level validators separately.

Reachable pin-region emission is part of that shared contract: both C and LLVM
must reject a pin block without a cleanup successor or without the matching
pin-unpin cleanup fact. Backend contracts may format the diagnostic locally,
but the decision must come from MIR cleanup fact helpers.

Pin-region source locals remain SSA definitions. A local such as
`let value = Read(view)` inside a pin block may carry a source-local declaration
emit fact for backend compatibility, but it must not be demoted to a residual
`MIR_INST_STMT` fallback when later CFG returns use that value. The versioned
definition and the return use are MIR SSA/dataflow facts; C and LLVM may only
consume them.

Source-local resource reads that have no matching SSA DEF remain compatibility
statements owned by MIR statement population. For example, secure-slot
destructuring may produce a `Read(slot, token)` let whose value must still be
materialized in C. The backend may emit the assignment, but only because MIR
kept the source-local emit fact; it must not rediscover the missing value by
walking AST call syntax.

MIR loop-local type recovery must consume one local-type owner seam. Range-loop
variables are `Int` because the current MIR/C lowering emits an `int32_t`
counter. For-in variables derive their surface type from the lowered iterable
type (`List<T>`, `Array<T>`, or `Slice<T>`) through
`transpiler_mir_for_loop_variable_type_name(...)`; backend consumers must not
register all loop variables as `Int` and then rely on later expression
rediscovery to repair subject or collection element types.
LLVM for-in body binding uses the MIR CFG region, not AST nesting. In nested
loops, the false-exit reachability check must stop when it re-enters the same
loop condition block; a path that exits an inner loop, reaches an outer
backedge, and later re-enters the inner condition is not evidence that the
current body block is outside the inner loop.

The source-local preservation decision is shared across CFG and non-CFG
population. `Read`/`ViewRead`/`ViewWrite`/`Move` lets are classified by the MIR
statement-source owner, and non-CFG compatibility insertion must use the same
source-statement append path so source indices, call facts, and fallback
accounting cannot drift.

CFG statement interleaving must append through a capacity-checked helper, not
raw `new_insts[new_count++]` writes. The calculated capacity is a MIR
population invariant; if a future statement shape violates it, lowering must
fail closed instead of corrupting the instruction inventory.

Backend source-order scheduling must consume MIR source-shape ordering helpers
instead of reading `source_statement_index` or
`has_source_statement_index` directly. The scheduling decision is an emission
order compatibility fact, not a backend-owned interpretation of MIR source
inventory.

Instruction-local checks such as "first source statement in a select case" must
also use MIR source-shape helpers. Validators may still compare a helper-returned
index against the owning block's statement inventory bounds, but they should not
spread raw source-order field interpretation into call-fact or backend owners.

Branch condition availability is also a MIR source-shape fact. Match/select
branches require a source branch payload whose shape matches the branch
instruction; expression, range, and for branches require a MIR expression
payload. C/LLVM consumers must call
`mir_instruction_has_required_branch_condition_fact(...)` instead of reopening
`source_node_type`, source payload, or `expr0` policy locally.
Inside the source-shape owner, branch shape and with-slot source checks should
also pass through `mir_instruction_source_matches_ast_type(...)`; raw
`source_node_type == ...` comparisons are a construction detail, not a pattern
for new consumers.

Source-statement fallback is also owned here. Residual `MIR_INST_STMT`
compatibility should call `mir_instruction_source_stmt_has_side_effect_hint(...)`
and source-statement emit should call `mir_instruction_source_payload(...)`
rather than recombining raw payload and source-location fields. This keeps
"may emit source" and "may retain fallback statement" on the same source-shape
seam.

Whole-program surface-usage facts follow the same rule. Public MIR surface
recording must seed source locations and usage booleans through
`mir_instruction_source_payload(...)`; it must not reopen `inst->ast` directly.
The source-shape owner is the only place allowed to decide whether an
instruction has a compatibility AST payload.

Terminator provenance is read through the same source-shape seam. Consumers
must use `mir_instruction_source_terminator_matches(...)` and
`mir_instruction_source_terminator_has_value(...)` when they need to validate or
count branch/return provenance. The durable fact remains on MIR; AIR and
backend-adjacent validators may consume it but must not invent a second
terminator-kind policy.

C backend MIR local type consumers may keep bounded stack render buffers for
immediate formatting, but they must not return mutable `static char *` or
`static char rendered[...]` scratch as a local type fact. If a rendered type name
must survive a nested lookup or recursive expression emission, copy it into the
active `TranspilerCtx.arena`. Rendered names are temporary; the typed-var
inventory or MIR type metadata is the durable fact.

Compiler helper string ownership has the same three-lane rule across C backend,
LLVM backend, LSP/tooling, and semantic diagnostics:

- pass-local names and transient rendered facts live in the owner scratch arena;
- immediate formatting output may use caller-owned stack buffers;
- values that survive the current dispatch call or message must be explicitly
  result-owned and released by the caller.

Mutable `static` buffers are not a source of truth for compiler helper results.
Static tables of immutable names are allowed, but static returned strings are a
compatibility smell unless the caller consumes them immediately and cannot
reenter the same helper. New helper APIs should choose one of the three lanes
in their name or contract.

Top-level mutable static state in compiler/codegen/semantic/LSP code is also
closed by default. There are currently no explicit mutable-static exceptions in
those owners; if a new exception is proposed, it must name the owner and the
source-of-truth contract in this document and in `build_source_inventory_smoke`.

Adding another exception requires naming the owner and the source-of-truth
contract in this document and in `build_source_inventory_smoke`.

The MIR ABI layout catalog is intentionally `static const`: it is immutable
data derived from `pgy_abi_spec.h`, not a lowering-time registry. Backends may
look up entries by canonical surface type name, but they must not mutate or
repopulate the catalog.

RIR program-root facts are also explicit now. `RIRProgram` owns the root AST for
the lowering run, and each `RIRScope` snapshots that pointer for fact helpers.
`rir_facts.c` must not reintroduce a process-global program-root bridge.

C backend type rendering is explicit as well. Generic bindings flow through
`render_type_name_in_ctx(...)` and `pergyra_ast_type_to_c_copy_in_ctx(...)`;
`transpiler_type_render.c` must not reintroduce a process-global render context
or push/restore stack.

C compiler detection is caller-owned. `pgy_select_c_compiler(...)` fills a
`PgyCCompilerSelection` supplied by the compile/link pipeline, so `PGY_CC` /
`CC` parsing and MinGW target fallback do not require process-global mutable
cache storage. If no usable compiler probe succeeds, detection returns failure
instead of silently manufacturing a `gcc` fallback; callers must surface that as
a compile/link diagnostic.

Compiler-side source-file reads are owned by `path_read_file(...)`. Debugger,
formatter, import resolver, and driver paths must not each reimplement
`fseek`/`ftell`/`fread` sizing, because that reopens inconsistent size caps and
partial-read handling. If a caller needs a different policy, it must add a
named path-utils owner function rather than embedding another local file reader.

Checked numeric parsing is owned by `src/common/numeric_parse.{h,c}`. Parser,
LSP, debugger, and MIR SSA consumers must not call `atoi`, `strtol`, or
`strtoull` directly; they consume the common prefix/strict parse functions so
overflow, malformed input, negative size values, and positive-only policy are
handled consistently.

Generated select round-robin state is process-local backend state, but it must
not be a plain mutable counter. The C backend emits an `_Atomic unsigned int`
and advances it with `atomic_fetch_add_explicit(..., memory_order_relaxed)`;
the LLVM backend emits the same contract with `LLVMBuildAtomicRMW(...Add...)`.
The contract is fairness rotation without introducing a data race when the
same generated function is reached from parallel code.

Runtime-owned registries may still be global when they represent process
runtime state, but they must name their synchronization owner. The party
scheduler registry is guarded by `g_schedulerRegistryMutex`; tag lookup remains
an indexed `g_schedulerByTag` read under that lock, not a registry scan.

Secure-slot token generation is also process runtime state, but it must not be
a plain mutable counter. Exported secure-slot wrappers use per-type
`atomic_uint_least64_t` counters with relaxed fetch-add: the contract is unique
token allocation, not cross-thread memory ordering.

Parallel pool lifecycle state is guarded separately from queue state. The
runtime uses atomic active flags, explicit shutting-down flags, and lifecycle
mutexes to keep `spawn` / enqueue from racing with `init` or `shutdown`
destroying the pool mutexes and condition variables. Shutdown publishes the
closing state before releasing the lifecycle mutex for worker joins; worker-local
spawn attempts therefore fail closed instead of blocking behind a join-held
lifecycle lock. Queue mutation remains protected by the pool's queue mutex.

Qubit runtime state is also a single guarded owner. The inline C runtime and the
LLVM export runtime both serialize claim, measure, entangle, state query, and
release operations with a qubit-state mutex. This keeps allocation cursors,
entanglement-pool membership, and measurement collapse from becoming hidden data
races when qubit APIs are reached from parallel code. The underlying C
`rand/srand` state is guarded by a separate runtime RNG mutex shared by ordinary
`Random` / `SeedRandom` and qubit collapse, because the RNG state is process
global rather than qubit-local.

Slot security contexts are explicit owners, not hidden singletons.
`SecurityContextCreate` returns the context that the slot manager stores, and
`SecurityContextDestroy` tears down that object directly. A new process-global
security context would reintroduce an ambiguous source of truth and must stay
out of `slot_security.c`.

Intent observability registry pointers are guarded by
`pgy_intent_registry_mutex`. Lookup helpers that find active entries or return
active/recent/history entry pointers are named `*_locked` / `*_locked_export`
because callers must already hold the registry mutex. Public query exports lock,
snapshot string payloads into thread-local borrowed buffers, and unlock before
returning. Reintroducing unqualified entry lookup helpers would hide the lock
precondition again.

High-level secure slot wrappers carry their slot manager owner directly.
`PergyraSecureSlot` records the `SlotManager *` that claimed the handle, and
read/write/release consume that stored owner. The runtime therefore does not
need a process-global `g_pergyraSlotManager` singleton to resolve secure slot
operations.

Intent observability borrowed-string queries return runtime-owned thread-local
snapshots. The registry mutex protects the read and snapshot copy, and callers
receive a pointer that is independent of the mutable active/history/recent
registry storage. This preserves the "caller must not free" ABI while avoiding
raw registry pointers that can be freed by a concurrent intent exit. The pointer
is a short-lived borrowed snapshot: callers must consume or copy it before a
later borrowed string query on the same thread can reuse that snapshot slot.

Runtime file handles are allocated from the handle table itself. The table mutex
protects open/read/write/close, and open scans for the first free descriptor
slot. There is no separate `ftable_next` cursor source of truth because closed
slots must be reusable and a cursor that is only written creates dead mutable
state.

The same C backend rule applies to expression type inference. Constructed names
such as `Array<T>`, `Slice<T>`, `ReadView<T>`, `WriteView<T>`,
`RemoteFuture<T>`, and `Option<T>` may be inferred recursively, so they must be
arena-backed facts rather than shared `static char` scratch. Stack buffers are
allowed only as immediate formatting inputs before the arena copy.

MIR resource-operation emission must snapshot slot inner names before asking the
C type mapper to lower them. A nested resource payload such as `Slot<Array<Int>>`
uses `slot_inner_type_name(...)` to find `Array<Int>`, and `pergyra_type_to_c`
may call the same helper again while lowering that nested payload. The runtime
helper suffix and ABI lookup must therefore use a copied inner-name buffer.

Array destructuring and array stdlib helpers use the same rule. When a backend
derives an element name from `Array<T>` or `Slice<T>` and then lowers that name
through `pergyra_type_to_c(...)`, it must copy `T` first if the original element
name is later used for local type registration or helper naming.

`SliceCopy(Slice<T>) -> Array<T>` is the runtime/ABI source-of-truth boundary
between a borrowed view and an owned snapshot. Semantic ownership keeps
`Slice<T>` as `BORROW_TRACKED`; invalid async/spawn/channel transport is
rejected before AIR graph synthesis. Backends may lower valid `SliceCopy` calls
only through `pgy_slice_copy_<T>` so String payload duplication and array result
ownership stay centralized in the runtime helper.

LLVM backend constructed-type argument parsing has the same rule. A helper that
returns a static scratch pointer is a parser convenience only; recursive type
lowering must copy `List<T>`, `Queue<T>`, `HashMap<K,V>`, and `Option<T>`
arguments into caller-owned storage before asking the type mapper to lower the
nested type.

LLVM boundary slot parameter lowering follows the same ownership contract.
`llvm_keep_rendered_persistent(...)` only accepts heap-rendered type text that
it may free after copying. When a boundary helper derives the inner type from a
caller-owned stack buffer, it must copy directly into `ctx->persistent` instead
of passing that stack buffer to the heap-rendered-type owner.

The same rule applies to expected-type helpers. If an expression emitter reads
an expected `Rc<T>`/container inner type from a static scratch helper, it must
copy that inner type before recursively lowering child expressions or lowering
the nested type itself.

C backend lambda emission follows the same rendered-type lifetime rule. A
lambda helper signature uses the rendered return C type before and after
parameter type rendering, so it must snapshot the return type into
caller-owned storage before emitting helper prototypes or helper bodies.

C backend declarator emission must also stay context-aware. Function signatures,
function-typed locals, event-handler parameters, async capture structs, and MIR
SSA callable locals must call the `_in_ctx` declarator APIs so alias-aware type
rendering flows through the same `TranspilerCtx` source of truth. The legacy
non-context wrappers exist only for compatibility inside the declarator owner;
external codegen consumers are smoke-gated away from them.

The same rule applies to AST type-to-C rendering. C backend consumers must call
`pergyra_ast_type_to_c_copy_in_ctx(...)` when a `TranspilerCtx` exists; the
non-context wrapper is an owner-local compatibility API only. This prevents
generic bindings such as active `T -> Long` specializations from silently
falling back to unbound/default C types inside lambda signatures, event
handlers, match bindings, hosted method forwards, spawn wrappers, and MIR
signature policy.

Concrete type-name-to-C lowering follows the same owner rule. When a backend
consumer already has a string type fact from MIR metadata, specialization
metadata, or expression inference, `transpiler_require_type_name_c_type_copy(...)`
must resolve active generic bindings before it accepts a one-letter nominal
type name. This prevents specialized C functions such as `Identity_Int(T x)`
from reintroducing unbound generic C parameters after MIR/source metadata has
already selected a concrete specialization.

C backend type-name inference follows the same context rule. Expression type
inference, MIR local type lookup, Future/RemoteFuture return inference,
slot-let/Box/Rc inner type extraction, annotated let registration, MIR
effective/local state rendering, MIR local binding discovery, generic
parameter/class specialization naming, class/subject pointer-self argument
policy, user/member call parameter-return policy, intent
prologue participant/value typing, ability vtable specialization, party/role
ability-tag rendering, type-specialization registry naming, and constructed
stdlib/channel/collection alias resolution must use
`render_type_name_in_ctx(...)` whenever a `TranspilerCtx` is available; the
non-context `render_type_name(...)` wrapper is compatibility surface for
immediate-use owner-local code only. `build-source-inventory-test-smoke`
therefore rejects non-context `render_type_name(...)` across `src/codegen`
outside `transpiler_type_render` / declaration headers. Generic binding
rendering is context-required and no longer falls back through the legacy
non-context renderer.

Intent participant classification has the same owner rule. C backend consumers
that need subject-participant or pointer-self classification for an intent
participant type name must call `intent_type_name_is_subject_participant(...)`
or `intent_type_name_uses_pointer_self(...)` from
`transpiler_intent_participant`, even when the type name came from a MIR
carrier row. Direct `is_subject_type_name(...)` /
`is_pointer_self_host_type_name(...)` calls are lower-level policy inputs, not
intent consumer seams. Intent call argument address policy follows this rule
for `intent_param_type_name` facts.

Intent action dispatch follows the declaration-method metadata rule. C intent
emitters must check action presence and only-self dispatch eligibility through
`find_subject_action_metadata(...)` and
`intent_action_metadata_has_only_self(...)` when a `MIRDeclMethod` is available.
The AST action declaration path is compatibility fallback only.

Nominal method return inference follows the same declaration-method metadata
rule. C expression inference, C MIR local type lookup, C nominal receiver
probing, LLVM let-call type inference, and LLVM expression type inference must
consume method return metadata through the C/LLVM hosted-method metadata
accessors before using AST method return fallback. If a host method metadata row
exists, missing return metadata must not reopen nominal method AST lookup just
to rediscover the same signature.
Hosted method forward declarations also consume `MIRDeclMethod` type-name facts
for return and parameter C type lowering before falling back to AST type nodes.

LLVM hosted-self calls and nominal member calls follow the same call-site rule.
When `MIRDeclMethod` metadata is available, logical parameter lookup and
pointer-self argument adjustment may consume the metadata row directly; the
current-host or nominal method AST is a compatibility fallback only and must not
be required just to lower a call.
C member-call lowering follows this split too: signature and argument decisions
consume `MIRDeclMethod` first, while source-body lookup is allowed only for the
projection-invalidation source/provenance fallback.

Rendered type-name classification has a separate source-of-truth rule. C
backend consumers that need to know whether a rendered type is `Channel<T>` must
call `transpiler_type_name_is_channel(...)` from `transpiler_type_mapping`
instead of reopening local prefix checks. LLVM consumers must use
`pgy_classify_type(...)` and compare against `PGY_TK_CHANNEL`. Constructor
guards, channel-let lowering, MIR SSA/preserved-let skip policy, and LLVM
constructor/receive inference all follow this classifier path.

The same classifier rule covers C `Future<T>` / `RemoteFuture<T>` spelling.
Await/spawn type queries and type-to-C lowering must use
`transpiler_type_name_is_future(...)`,
`transpiler_type_name_is_remote_future(...)`, or
`transpiler_type_name_is_any_future(...)` rather than reopening local prefix
checks.

Result/Option and common C collection spelling follows the same rule.
`transpiler_type_mapping` owns `transpiler_type_name_is_result(...)`,
`transpiler_type_name_is_option(...)`,
`transpiler_type_name_is_array_or_slice(...)`, `..._list`, `..._queue`,
`..._set`, `..._hashmap`, `..._box`, `..._box_array`, `..._rc`, and
`..._weak`. Match destructuring, try-let lowering, Option contextual lowering,
array access, for-in lowering, MIR for-in/destructuring type lookup, BoxArray
let lowering, channel type queries, and collection builtin inference consume
those classifiers instead of local `strncmp(...)` checks. The perf contract
smoke rejects new C `transpiler_*.c` direct type-family prefix checks outside
`transpiler_type_mapping.c`.

LLVM for-in lowering also consumes active LLVM scope aggregate facts for
`PgyArray_*` and `PgySlice_*` parameters. A function parameter or preserved
MIR local with a concrete aggregate LLVM type is sufficient metadata for loop
condition length checks and loop element binding; the emitter must not require
the separate array-var registry when the concrete scope type already owns the
same element fact.

LLVM indexed array assignment follows the same element-owner rule. The
assignment emitter must ask `llvm_stmt_resolve_array_elem_type(...)` for the
element fact so parameter arrays and preserved MIR locals lower through the same
metadata path as indexed reads and for-in loops.

Result/Option constructor vocabulary is owned by
`src/common/match_variant_policy.c`. Parser shorthand, semantic match
validation, semantic exhaustiveness, C lowering, LLVM lowering, and SSA
contracts that need to classify `Some`, `None`, `Ok`, or `Err` as constructors
or match destructors must call the shared policy and consume
`PgyMatchVariantKind`/`PgyCodegenMatchVariantKind`. Direct
`strcmp(..., "None")` style rediscovery in parser/semantic/backend consumers is
source-of-truth drift.
Generated C tag spelling for Option/Result variants is owned by
`src/codegen/codegen_match_variant_policy.c`; C emission paths consume
`pgy_codegen_match_variant_c_option_tag(...)` and
`pgy_codegen_match_variant_c_result_tag(...)` rather than retyping the tag
symbols locally.

Result specialization and `let` lowering are recursive-emission boundaries too.
If a C type name is needed after storing another rendered type or after calling
`emit_expression(...)`, copy it into caller-owned storage first. This covers
`Result<T,E>` ok/error C metadata, array literal let bindings, `SetNew`
collection-specialization lets, and try-let lowering.

Array literal expression emission also crosses a recursive-emission boundary.
The `Array<T>` element name returned by `slot_inner_type_name(...)` must be
copied before emitting element expressions, because element emission may render
other constructed types and overwrite the inner-type scratch buffer.

Tuple literal expression emission follows the same rule. The rendered tuple C
type must be copied before emitting tuple element expressions, because element
emission can recursively render other constructed types.

Hosted self ABI is also a MIR/declaration-header fact. Party and roster methods
use the same pointer-self ABI as relation/effect/zone/world methods. C and LLVM
may choose different local names while emitting, but `self.member` inside a
party/roster method must lower through the pointer-self path (`self->member` in
C), not through value-object member access.

Constructor arity/type validation for domain hosts is semantic-owned. The
constructor declaration lookup table must include every constructible domain
host kind, including party and roster, before codegen sees the call. Backends
may emit constructors, but they must not be the first layer to discover too many
or mistyped constructor field arguments.

Domain method `self` typing is semantic-owned. Party and roster method contexts
must set the same current-host state used by world/zone/relation/effect methods,
so `self.member` is accepted or rejected before backend lowering. A backend
failure on an unknown party/roster member is a source-of-truth bug, not an
acceptable diagnostic path.

Implicit host-field access is part of the same contract. If semantic accepts a
bare party/roster method field such as `round` or `tick`, C and LLVM must both
consume the current-host field fact and emit pointer-self access. Leaving the
identifier as a global C symbol is backend drift.

C current-host field identifier lowering has a named owner:
`transpiler_host_field_identifier.c`. It may emit a bare identifier as a host
field only when the current function has a `self` receiver, and it must preserve
lexical local shadowing. MIR `source_local_defs` are not lexical declarations:
field assignments can appear there as preserved emit facts, so treating them as
locals reopens the stale-SSA bug where an opaque self-method call reads the old
SSA snapshot instead of the current host field.

Role include method reuse is a backend wrapper fact, not an AST body-copy fact.
The included role owns the MIR routine for the method body. A derived role that
inherits the method may emit a thin wrapper named for the derived role and
forward to the included role routine, but it must not clone the method body or
invent a missing derived-role MIR routine.

C ability vtable signatures must be type-complete enough before ability
emission. Pointer-self host parameters such as `Player` in an inherited role
method are rendered as `Player *`, so the C backend emits nominal forward
typedefs before ability vtables. The ABI source of truth remains the host
self-cell classification policy; ability emission must not fall back to raw
type strings that bypass that policy.

Role vtable binding is also a named owner seam. LLVM bind lowering must ask
`llvm_party_slot_first_ability_name(...)` for the party-slot ability and
`llvm_lookup_role_vtable_global(...)` for the vtable global. It must not scan
party role-slot AST arrays in `llvm_stmt.c`, walk module globals, or reconstruct
`*_vtable_instance` names at the bind site.

Party role-slot dynamic/vtable field registration is declaration-field
metadata. LLVM struct type and field registration consume
`LLVMHostedRoleSlotView` over `MIR_DECL_FIELD_ROLE_SLOT` rows, not party
role-slot AST arrays.
C party role-slot struct/vtable emission and bind dispatch consume
`TranspilerHostedRoleSlotView` for the same metadata seam.
LLVM party-slot ability lookup consumes `LLVMHostedRoleSlotView` before bind
lowering asks for the vtable global.

World zone-slot struct registration is also declaration-field metadata. LLVM
struct type and field registration consume `LLVMHostedWorldZoneSlotView` over
`MIR_DECL_FIELD_WORLD_ZONE_SLOT` rows, not `ast_world_zones(...)` scans.
C world member type lookup, world constructor lowering, and world declaration
emission consume `TranspilerHostedWorldZoneSlotView` for the same row kind.

### Type-Resolution DAG

The DAG owns declaration/type dependency truth. Recursive resolver fallback is
retired for the frozen beta surface.

Allowed:

- metadata lookup;
- owner-local materialization through the central metadata API;
- explicit dead-end diagnostics;
- retired compatibility mirrors only as quarantine sentinels in tests; active
  semantic/DAG evidence must use metadata and evidence fields directly;
- metadata-first type-ref reads for stable placeholder construction.
- metadata index acceleration, as long as the index is a private cache over the
  same metadata owner and validates its open-addressing capacity invariant.

`SemanticResult` is the public seam for exporting DAG evidence out of semantic
analysis. Later layers must consume `semantic_result_*` accessors for metadata
entries, dead ends, generic-contract evidence, and ability-consumer evidence;
they must not couple directly to raw result counter fields. AIR may translate
those counts into evidence nodes, but the counter vocabulary remains semantic
DAG-owned.

AIR summary counters are compatibility telemetry. Reads and writes should pass
through the summary-counter owner (`air_evidence_summary_count(...)` and
`air_increment_evidence_summary_count(...)`); RIR propagation required counters
use the same owner through `air_evidence_required_count(...)` and
`air_increment_evidence_required_count(...)`. Direct counter access is reserved
for the owner itself or for tests deliberately constructing invalid AIR values.
EvidenceNode inventory remains the proof source of truth. Human and JSON AIR
dumps should iterate through `air_evidence_node_count(...)` and
`air_evidence_node_at(...)` so display code consumes the same inventory seam as
validators rather than reopening the raw array. Read-only duplicate/singleton
probes in HIR, MIR, and runtime evidence collectors should use the same
accessors. Boundary evidence shape validation should also use this accessor
seam for node lookup, and inventory validation should do the same when reading
existing nodes. Raw array ownership stays with the EvidenceNode inventory owner;
top-level storage-shape validation should call the EvidenceNode owner helper
rather than reopening count/storage fields. Boundary summary flag writes should
go through `air_boundary_mark_summary_flag(...)`; HIR/RIR evidence collectors
may request a mark but should not set telemetry booleans directly.
Boundary summary validation should read summary state through
`air_boundary_has_summary_flag(...)`, keeping the boolean fields behind one
flag vocabulary.
AIR boundary authority-name storage has the same rule: `air_boundary.c` owns
the list storage, count/at access, and name membership checks. JSON dumps,
provenance formatting, evidence validation, and RIR evidence collection should
consume `air_boundary_authority_name_count(...)`,
`air_boundary_authority_name_at(...)`, and
`air_boundary_declares_authority_name(...)` rather than iterating the raw
authority array.
AIR graph arrays follow the same source-of-truth rule. `air.c` owns
intent-node and boundary-node storage, and `air_drift.c` owns drift mutation.
Read-only consumers must use `air_intent_node_count(...)`,
`air_intent_node_at(...)`, `air_boundary_node_count(...)`,
`air_boundary_node_at(...)`, `air_drift_count(...)`, and
`air_drift_at(...)`; evidence collectors and verifiers that legitimately
annotate boundary summaries must use `air_boundary_node_mut_at(...)`. Raw
`AIRProgram` graph arrays are not a validation, dump, or evidence-consumer
API.
The const graph accessors are public AIR API because driver diagnostics, JSON
dumping, and future LSP/CI consumers need read-only graph visibility. Mutating
accessors, storage-validity checks, and input-marking helpers remain internal
AIR APIs.
Driver code is a consumer, not an owner, of AIR graph storage. It may report
drift and evidence provenance, but it must do so through the AIR graph accessors
and EvidenceNode accessors. Direct reads of `air->drift_count`, `air->drifts`,
`air->intent_count`, `air->intents`, `air->boundary_count`, or
`air->boundaries` outside AIR graph owners are source-of-truth drift.
`semantic-core-shape-test-smoke` enforces this for graph storage, input flags,
the strict-evidence flag, and EvidenceNode storage; new raw consumers must
first name an AIR owner seam and update this source-of-truth contract.
AIR input-presence flags are also graph metadata. Consumers should read them
through `air_has_hir_input(...)`, `air_has_rir_input(...)`, and
`air_has_mir_input(...)`; evidence collectors should mark late-attached inputs
with `air_mark_*_input(...)`. This keeps verification policy from depending on
open-coded telemetry fields. The strict-evidence policy bit follows the same
rule through `air_requires_strict_evidence(...)`; verifiers should not reopen
the storage flag directly.
AIR JSON summary counters are a checked projection of the EvidenceNode
inventory, not a second proof source. `pgy.air.graph.v1` consumers must be able
to verify that summary counts for DAG, MIR, observability, and runtime frontier
evidence match the emitted EvidenceNode inventory.

Forbidden:

- direct `resolve_type_node(...)` outside the central metadata owner;
- hidden recursive fallback;
- annotation-or-unknown compatibility helpers;
- annotation-only reads outside private metadata owners;
- using compatibility counters as semantic evidence;
- naming explicit DAG dead-end family counters as fallback paths;
- declaration-order success when a DAG dependency fact is missing.

### DIR/RIR

DIR owns declaration/domain graph inventory. RIR owns resource, authority,
effect, relation, projection, channel, IO, and runtime-relevant propagation
facts.

DIR/RIR answer:

- which domain declarations exist;
- which authority/resource/effect boundary exists;
- which operation produces runtime-relevant propagation evidence;
- which declaration inventory is available to backends.

DIR/RIR facts may be summarized into MIR/AIR, but later layers must not invent
them.

### AIR

AIR is a verification layer, not a codegen IR.

AIR answers:

- whether declared intent/zone/world/effect boundaries drift from actual HIR,
  RIR, MIR, DAG, and runtime-policy evidence;
- whether required boundary evidence exists;
- whether evidence provenance is complete enough for diagnostics.

AIR does not own:

- CFG reachability;
- type resolution;
- cleanup generation;
- runtime frontier scheduling;
- backend lowering.

AIR may reject missing or inconsistent evidence. It must not synthesize lower
layer facts to make evidence pass.

Global `AIREvidenceNode` inventory is the verification source of truth. Summary
counters remain telemetry and compatibility surface: counter-only evidence may
produce strict-evidence drift, but evidence-only inventory must remain valid when
the node payload is complete.

DAG generic and ability-consumer evidence names must stay specific. Generic
contract evidence uses `type_resolution_dag_generic_contract_evidence_count`;
ability-consumer evidence uses
`type_resolution_dag_ability_consumer_evidence_count`. Broader or ambiguous
`dag_ability_evidence` mirrors are not source-of-truth fields.

Singleton global evidence, such as runtime observability schema and runtime
frontier policy evidence, is idempotent. Re-collecting the same singleton
schema/policy must not mutate fact counts or summary counters. A duplicate
singleton with conflicting fact or fallback counts is evidence drift and must
fail instead of being merged silently; otherwise compatibility counters would
drift from the EvidenceNode inventory and AIR would stop being the single
verification source of truth.

### Runtime Policy Headers

Runtime policy headers own stable ABI/runtime rules that must be shared by C and
LLVM emitters.

Examples:

- bounded frontier pass-limit arithmetic;
- bounded frontier overflow reason strings;
- panic/failure classes;
- observability schema;
- Slot/Pin ABI constants;
- authority failure query surface.

Emitters may wrap these policies, but must not duplicate them as independent
local rules.

## 3. Consumer Classification

Every consumer of a cross-layer fact must be classified as one of:

| Class | Meaning | Rule |
|---|---|---|
| Truth owner | Computes and stores the fact | Exactly one layer |
| Consumer | Reads fact and emits code/diagnostic | Must not recompute |
| Provenance consumer | Uses AST/name/source info only for messages | May read AST, not decide truth |
| Compatibility seam | Temporary bridge with explicit name | Must be gated and shrinking |
| Smoke gate | Regression guard | Cannot define semantics |

When adding or moving code, classify the file/function before editing it. If the
classification is unclear, do not refactor yet.

## 4. Beta Blocker Order

The beta closure order is:

1. CFG/MIR body safety source-of-truth.
   Current measurable seam: `MIRProgram.has_non_cfg_body_fallback_inventory`,
   `MIRProgram.non_cfg_body_fallback_total`, and
   `MIRProgram.non_cfg_body_fallback_routine_count` aggregate residual
   non-CFG body fallback usage. Consumers may inspect this aggregate; they must
   not rescan AST bodies to rediscover the fallback path.
2. AIR abstraction-boundary verifier coverage.
3. Type-resolution DAG source-of-truth closure.
4. Runtime frontier/failure policy generated-path verification.
5. MIR declaration inventory parity for C/LLVM.
6. ABI/Slot/Pin/Zone-bound handle freeze.
7. Dogfood path through C backend and external modules.

Do not spend beta time on broad folder reshuffles, helper naming cleanup, or
line-count splits unless they directly unblock one of these items.

## 5. Refactoring Stop Rules

Stop a refactor if any of these appear:

- the same fact would be owned by two layers after the change;
- a smoke test becomes the only place where a rule is defined;
- a backend starts walking AST to compensate for missing MIR/DIR/RIR facts;
- a compatibility seam grows without a planned deletion path;
- a split is justified only by line count, not by responsibility;
- the change improves local structure but does not move a beta blocker.

## 5a. Current Architecture Judgement

The 2026-05 beta closure architecture decision is:

**Stabilize the C compiler's ownership spine; do not redesign the whole folder
layout before beta.**

Rationale:

- The active problem is not `.inc` inventory anymore. It is fact ownership:
  CFG/MIR, AIR, DAG, DIR/RIR, runtime policy, backend inventory, and ABI must
  each have one source of truth.
- Large-file reduction is useful only when it narrows a source-of-truth owner
  or removes a compatibility seam. A 600 LOC file is a review signal, not a
  command to split mechanically.
- C has no real namespaces, so horizontal folders such as `parser/`,
  `semantic/`, `compiler/`, `codegen/`, and `runtime/` are acceptable until
  self-host. Forcing feature folders now would create high-risk path churn
  without closing CFG/AIR/DAG/MIR/ABI blockers.
- Implementation headers are debt only when they own behavior across multiple
  translation units or hide source-of-truth logic. A single-include
  implementation header may remain temporarily if moving it would expose a
  wider dependency seam than it removes.
- New `_helpers` owners are discouraged. If a split is needed, the new owner
  name must describe the responsibility (`*_type_render`, `*_sync_clauses`,
  `*_resource_types`, etc.), not just that it is a helper bucket.
- Backend splits must preserve semantic ownership. C/LLVM emitters may consume
  MIR/DIR/RIR/runtime/ABI facts, but a split must not make a backend file the
  new semantic owner of type, effect, authority, cleanup, or body-safety truth.
- Self-host is the natural point to recover a feature-folder/module layout
  because Pergyra will have namespaces/modules as first-class structure. Until
  then, beta work should prefer narrow owner seams over broad directory moves.

Practical rule:

If a refactor cannot name the removed compatibility seam, the source-of-truth
owner it strengthens, and the gate that proves drift did not occur, defer it.

## 5b. C Type Rendering Lifetime Rule

The C type-name mapping layer is copy-first. New code must not keep any
rendered type pointer across another type-rendering or generic-inner-name call.
If the rendered C type is stored, passed through a later emission path, or reused
after a `slot_inner_type_name(...)`/generic lookup call, the owner must lower it
through `transpiler_require_type_name_c_type_copy(...)` when a `TranspilerCtx` is
available, or through a tightly scoped caller-owned compatibility copy inside a
wrapper/source-of-truth owner.

Current closed slices:

- C AST for-in lowering snapshots the iterable inner type and rendered C
  element type before emitting the loop body.
- C MIR CFG for-in lowering returns caller-owned element and inner type buffers
  instead of returning a static `pergyra_type_to_c(slot_inner_type_name(...))`
  pointer.
- C AST/MIR destructuring snapshots both the initializer C type and the
  element C type; the initializer render is not allowed to survive through a
  later element render as a static pointer.
- Slot resource op and ArrayReverse lowering copy their rendered inner C type
  before formatting runtime calls or expression templates.
- Channel receive builtins (`TryRecv`, `RecvTimeout`) copy the rendered payload
  C type before formatting their expression templates.
- Let lowering, inferred let bindings, Result/collection specialization, and
  tuple literal emission use caller-owned C-type buffers through the
  `transpiler_require_type_name_c_type_copy(...)` requirement path for rendered
  types that outlive the immediate mapping expression.
- Match subject/payload lowering, MIR match payload lowering, select receive
  bindings, spawn wrappers, inferred lambda returns, view-like slot
  declarations, tuple literal layouts, Some/Ok/Err match payload bindings,
  await Future/RemoteFuture result lowering, MIR SSA local declarations, MIR
  role-owner subject receivers, MIR for-in element binding, MIR preserved try
  Result operands, role ability vtable returns, post-sync call wrappers, await
  lowering, MIR role-host receiver lowering, intent zone participant rebinding
  and metadata rebinding, and tuple destructuring element
  declarations also consume caller-owned C type buffers.
- Await lowering consumes `lookup_future_inner_type_copy(...)` instead of
  holding a static Future/RemoteFuture payload pointer.
- MIR SSA parameter type lookup returns arena-owned rendered type names instead
  of a backend-local static `rendered_param` buffer.
- Function/event-handler declarator rendering copies each
  `pergyra_ast_type_to_c(...)` result into caller-local storage before rendering
  the next return or parameter type.
- AST type-requirement helpers now expose
  `transpiler_require_ast_c_type_copy(...)` for emitters that need a stable
  rendered type across later declaration/signature emission. The legacy
  pointer-return helper is compatibility surface for immediate-use callers only.
- Intent prologue, intent zone-binding forward declarations, and intent
  step-rebind compatibility paths use the copy helper for AST-carried participant
  and value types. MIR metadata paths already consume type-name copy helpers, and
  C zone-slot metadata variants do not reopen AST participant lookup when
  metadata arrays are provided; C missing carrier rows and LLVM MIR-active
  participant type-registry or binding misses become MIR inventory diagnostics.
  C and LLVM intent emitters must check those diagnostics immediately after
  zone bind/rebind/sync/restore helpers; continuing C text emission or LLVM IR
  construction after a metadata diagnostic is source-of-truth drift.
  MIR-only intent dispatch must also fail closed when dispatch participant
  metadata is missing. C dispatch may use AST action lookup only on non-MIR
  compatibility paths; MIR-backed dispatch consumes `MIRDeclMethod` action
  metadata or skips absent optional action calls without reopening AST methods.
  The AST fallback must obey the same lifetime contract while it remains.
- Annotated `let` lowering snapshots the annotated C type before emitting the
  initializer expression. Initializer emission may recursively render other
  types, so the declaration C type must be caller-owned before the initializer
  path runs.
- Extern declaration emission uses the AST copy helper for return and parameter
  C types, keeping FFI signatures on the same bounded-copy rule as forward
  declarations and intent signatures.
- The perf contract now gates the copy API and the migrated C backend
  consumers above.

## 6. Allowed Temporary Debt

Some seams are allowed until the owning path fully replaces them:

- AST compatibility payloads inside declaration headers, only when explicitly
  named `ast_compat` or equivalent;
- local compiler-run skips in smoke scripts when no explicit `PGY_BIN` is
  provided, while CI/Make-provided `PGY_BIN` remains strict;
- C-era filename namespaces before post-beta self-host;
- explicit quarantine owners that prevent retired implementation bodies from
  reappearing. Zero-only telemetry for retired paths is no longer an allowed
  source-of-truth substitute.
- hard self-host preparation only after the substrate gaps in
  `docs/self_hosted/05_compiler_core_gap_analysis.md` are closed or explicitly
  assigned to soft self-host stages.

Allowed debt must be named. Unnamed fallback is not allowed.

## 7. Inputter / Outputter Boundary Rule

Source-of-truth ownership is not only a data-structure rule. Every compiler
surface also has an inputter/outputter boundary:

- inputter: what source bytes/tokens/facts were adopted, in which context, and
  with which recovery value;
- outputter: what artifact node was built, what was marker/payload/residue, and
  when the result was committed or discarded.

The canonical checklist is `docs/129_tex_semantics_lessons.md`. A new language
surface is not beta-ready until its owner can answer the contract questions from
that document:

- scanner owner, stop condition, lookahead policy, and adopted recovery value;
- capture point, planner point, commit point, rollback/cancel point, and trace
  point for delayed effects;
- marker/payload/residue classification for emitted artifacts;
- semantic equality or canonicalization rule when raw bytes are not a stable
  oracle;
- deterministic side-effect trace for the same seed/profile/environment.

This rule is why pretty output, AST dumps, and byte-for-byte artifacts are not
accepted as the only oracle. The owner must state the operational transition
that the test is proving.

## 8. Working Rule For Agents

Before changing compiler architecture, answer these four questions:

1. Which layer owns the fact?
2. Which consumers should read it?
3. Which old compatibility seam is being removed or narrowed?
4. Which smoke/regression proves the owner contract did not drift?

For scanner, recovery, emitter, formatter, diagnostic, runtime-trace, and
artifact-generation changes, also answer:

5. What is adopted at the input boundary?
6. What is built but not yet committed at the output boundary?
7. Which artifact layer is the oracle: raw bytes, parsed artifact, normalized
   artifact, semantic equality, trace, or rendered output?

If there is no answer, the change is probably another A -> B -> A loop.

## 9. Loss Contract Rule

Every abstraction boundary must also state its loss contract. The owner must
answer:

- what fact is intentionally lost;
- what fact is preserved exactly;
- what fact is preserved as bounded approximation;
- what fact is runtime-checked rather than statically proven;
- which later layer is forbidden from rereading the older source to recover the
  lost fact;
- which smoke, regression, diagnostic, trace, or invariant proves the loss
  budget.

The proof-pack owner is
`docs/semantics/09_abstraction_loss_contracts.md`. This rule generalizes the AIR
epsilon-loss isolation contract: AIR is one verifier of cross-layer loss, while
CFG, DAG, RIR, MIR, ABI/runtime, and backend owners still keep their own facts.
