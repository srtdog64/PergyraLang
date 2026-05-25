# Pergyra Source-Of-Truth Spine

Last updated: 2026-05-25

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
  just one matching ability.
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
  `src/compiler/mir_public_surface.c`.
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
- C backend projection/action codegen may consume active inventory and
  program-view seams for zone/effect/relation declaration recovery, but it must
  not reopen direct domain declaration lookup at each projection, bind, intent,
  or world-frontier use site. The declaration lookup owner remains the active
  inventory view; projection/action owners consume the recovered declaration
  only to emit already-lowered runtime synchronization code. Direct
  `find_zone_decl`, `find_world_decl`, `find_relation_decl`, and
  `find_effect_decl` calls are confined to the declaration lookup owner.
- party-slot ability tag selection lives in
  `src/codegen/transpiler_role_ability.c`. Bind emission and member-call
  emission may consume `transpiler_party_slot_first_ability_tag(...)` or
  `transpiler_party_slot_method_ability_tag(...)`, but they must not reopen
  party role-slot scans locally.
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
- Stable diagnostic literals live in `src/semantic/diag_codes.h` and are
  mirrored by `docs/72_diagnostic_codes.md`. Driver JSON, LSP diagnostics,
  parser/lexer routing, semantic diagnostics, and backend diagnostics consume
  those literals; they must not invent ad-hoc `code`, `cause_ir`, or
  `fix_source` strings.
- Soft self-host tool output contracts live in `docs/self_hosted/`. First-stage
  tools must emit deterministic JSON or structured diagnostics for oracle
  comparison. Human text is a secondary view, not the comparison source of
  truth.
- Python may improve local validation, but it is not the source of truth for
  beta gates. Mandatory smokes must either provide a shell/C/compiler fallback
  or fail when an explicitly supplied required binary/input is unavailable.
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
| Body safety facts | MIR CFG/dataflow | C backend, LLVM backend, AIR evidence | Backend-local cleanup/drop/pin rules |
| MIR source shape and source-location compatibility | MIR source-shape owner | MIR validators, DCE, C/LLVM emitters, dumps | Consumers reopening raw `source_ast_type` / `source_line` fields |
| MIR compatibility AST payload | MIR source-shape owner | C/LLVM residual source emitters, diagnostics, validators | Consumers reading `inst->ast` directly outside MIR construction/population/source-shape owners |
| Declaration/domain inventory | DIR/RIR/MIR declaration headers | C/LLVM declaration emitters | AST-carried backend inventory as final truth |
| Host declaration compatibility lookup | `host_decl_compat.c` type/name table | C/LLVM host method lookup, pointer-self policy, no-MIR compatibility paths | Partial class/enum-only fallback chains that omit party/role/roster/domain hosts |
| MIR public inventory/query/pass wrappers | `mir_public_surface.c` | C/LLVM inventory views, MIR tests | Public query/DCE/liveness wrappers living in the lowering implementation header |
| Type/declaration dependency | Type-resolution DAG metadata | Semantic owners, AIR DAG evidence | Recursive resolver fallback on frozen paths |
| Generic/ability contract evidence | Type-resolution DAG | Semantic contract checks, AIR | Compatibility counters as semantic truth |
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
| Soft self-host tool output | `docs/self_hosted/03_tool_candidates.md` + `04_beta_exit_handoff.md` | First Pergyra-written diagnostic/AIR/MIR tools, CI oracle comparison | Human-readable output as the only oracle or starting from compiler core rewrite |
| Unsafe/raw capability scope | `docs/132_unsafe_capability_scope.md` plus semantic/AIR gates once implemented | Parser diagnostics, semantic raw escape diagnostics, ABI lowering, self-host roadmap | Plain `unsafe { ... }` granting raw/system-tier escape |
| Runtime-none surface scan | `src/compiler/runtime_none_contract.c` | Driver runtime profile diagnostics, runtime-none smoke, future no-runtime lowering | Early-success scanning of only the first array/tuple literal element |
| Mandatory smoke portability | Owning smoke script plus Makefile target | CI, minimal Windows/Git Bash, soft self-host oracle tools | Python-only validation or explicit required binaries being skipped as success |
| Build source inventory | Makefile source/object inventory | CI, local smoke targets, dependency inclusion | Shell `find` rediscovering build artifacts or source files on Windows paths |
| Local build artifact ownership | One `BUILD_DIR`/`BIN_DIR` pair per active make process | Local gates, CI recipes, troubleshooting docs | Parallel gates sharing the same `build/` and corrupting `.o` files |

Runtime frontier AIR evidence must count the complete frozen runtime policy
surface: pass-limit arithmetic facts plus bounded-overflow reason facts. A
backend may emit those strings, but it may not own or rename them.

Runtime frontier codegen may only consume the codegen policy wrapper
(`src/codegen/domain_frontier_policy.h`) for pass-limit selection. The runtime
policy header owns the arithmetic vocabulary, but C/LLVM emitters must not call
the runtime `pgy_frontier_*_pass_limit(...)` helpers directly. The wrapper is
the backend-facing seam that keeps AST/domain declaration lookup and runtime
frontier arithmetic from mixing in emitter-local code.

## 2. Layer Contracts

### AST

AST owns raw parse structure, source spans, and user-facing syntax provenance.
AST does not own semantic truth after lowering begins.

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
`source_ast_type`, source payload, or `expr0` policy locally.
Inside the source-shape owner, branch shape and with-slot source checks should
also pass through `mir_instruction_source_matches_ast_type(...)`; raw
`source_ast_type == ...` comparisons are a construction detail, not a pattern
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

LLVM backend constructed-type argument parsing has the same rule. A helper that
returns a static scratch pointer is a parser convenience only; recursive type
lowering must copy `List<T>`, `Queue<T>`, `HashMap<K,V>`, and `Option<T>`
arguments into caller-owned storage before asking the type mapper to lower the
nested type.

The same rule applies to expected-type helpers. If an expression emitter reads
an expected `Rc<T>`/container inner type from a static scratch helper, it must
copy that inner type before recursively lowering child expressions or lowering
the nested type itself.

C backend lambda emission follows the same rendered-type lifetime rule. A
lambda helper signature uses the rendered return C type before and after
parameter type rendering, so it must snapshot the return type into
caller-owned storage before emitting helper prototypes or helper bodies.

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

`pergyra_type_to_c(...)` remains a compatibility API that may return static
storage. New code must not keep that pointer across another type-rendering or
generic-inner-name call. If the rendered C type is stored, passed through a
later emission path, or reused after a `slot_inner_type_name(...)`/generic
lookup call, the owner must copy it through `pergyra_type_to_c_copy(...)` or an
arena-owned equivalent.

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
  tuple literal emission use `pergyra_type_to_c_copy(...)` for rendered types
  that outlive the immediate mapping expression.
- Match subject/payload lowering, MIR match payload lowering, select receive
  bindings, spawn wrappers, inferred lambda returns, view-like slot
  declarations, MIR SSA local declarations, role ability vtable returns,
  post-sync call wrappers, await lowering, MIR role-host receiver lowering,
  intent zone participant rebinding, and tuple destructuring element
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
  and value types. MIR metadata paths already consume type-name copy helpers; the
  AST fallback must obey the same lifetime contract while it remains.
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
