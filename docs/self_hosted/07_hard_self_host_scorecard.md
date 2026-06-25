# Hard Self-Host Readiness Scorecard

This scorecard measures the ten non-negotiable capabilities from the gap
analysis (05) against the current tree. Each capability is gated by a smoke
test; tests/self_host_readiness_scorecard.sh verifies the gates are present and
prints the tier without a build. This document records the reasoning behind
each tier and the work that remains.

## Verdict

Hard self-hosting has started as staged compiler-pass substitution, not as a
single full compiler rewrite. The infrastructure is mature: every one of the
ten capabilities has a gate, and the tree carries a broad smoke gate set. The
process-argument tooling gap is closed by `Args() -> Array<String>`, and
allocator pass lanes now have explicit `AllocatorDestroy(namedAllocator)`
cleanup that works through C and LLVM. The first post-substrate slice is the
semantic typed `let` / return verdict parity rung.

As a planning estimate, hard self-host substrate readiness is effectively
complete for the first pass-rewrite stage, but capability 5 is not a blanket
"no AST payload anywhere" claim. The declaration-level codegen SoT burn-down is
closed: the codegen frontier went from 127 original source_ast reads to 0. The
compiler-side source_ast tail is now 0; `MIRDeclHeader.source_ast` and
`mir_decl_header_source_decl` are removed. Source-type/location scalar
provenance has been split to source_node names, method and field declaration
back-pointers are removed, and MIR validation no longer compares generic, enum,
method, or field metadata against original AST nodes. Source_decl is ratcheted
at codegen 0 / compiler 0, and routine_source_decl_codegen is ratcheted at 0.

The remaining body-level SoT tail is narrower and now explicitly measured:
residual `MIR_INST_STMT` source-payload emission is retired in C and LLVM, and
side-effect statements are carried through `MIR_STMT.expr0` executable facts.
Source-local declaration and assignment paths no longer re-dispatch through
raw source-statement emitters. Source-statement emit predicates and LLVM DEF
emit predicates now consume MIR source-location/expression facts instead of
payload presence. C and LLVM residual STMT branches now consume MIR
source-shape / `expr0` facts, and LLVM missing-return-value diagnostics use MIR
topology diagnostics instead of reopening source payload anchors. Select
dispatch branches carry their readiness channel as a MIR branch `expr0` fact,
so C/LLVM no longer parse the select case source payload for dispatch
conditions. Match-case condition emission now consumes MIR-captured branch
pattern/guard facts (`match_case_pattern*` and `match_case_guard`) through
`mir_instruction_match_pattern_*` / `mir_instruction_match_guard`, and C/LLVM
condition, body-binding, and remap emitters are ratcheted against reopening
`mir_instruction_source_payload`. MIR destructure instructions now carry
`destructure_binding_names`, and C SSA local type/view registration consumes
`mir_instruction_destructure_binding_index` instead of reopening source
payloads for binding-name recovery. C MIR destructure emission consumes
`inst->expr0` and the MIR binding facts instead of reading
`ast_let_destructure_*` from the source statement. LLVM MIR destructure emission
now consumes the same initializer and binding facts through
`llvm_emit_mir_destructure_inst`. C and LLVM assignment emission now consume MIR
target/value facts: `MIR_INST_ASSIGN` requires `expr0`/`expr1`, assignment DEFs
carry their target in `expr1`, and backend assignment-parts emitters preserve
slot, array, field, and projection assignment semantics without reopening the
source statement payload. LLVM source-local resource constructor LET emission
also consumes MIR initializer/type facts instead of reopening the source local
declaration payload. C source-local LET DEF emission, generic DEF expression
emission, and receive-payload type inference now consume instruction `arg0` /
`expr0` / `expr1` facts directly, so C codegen no longer calls
`mir_instruction_source_payload`. MIR surface validation no longer reopens
source payloads for payload-presence or surface-usage checks; it consumes
source-shape predicates and MIR expression facts. Public-surface source
line/column/stable-id/type seeding and transitional MIR JSON source text are
now capture-time provenance facts owned by
`mir_instruction_capture_source_provenance(...)`; lifecycle dump emission
consumes `mir_instruction_source_inline_text(inst)` instead of reopening the
source payload. The self-hosted `mir_lower` now consumes MIR JSON
`expr0`/`expr1`/`source_type`/`source_locals` facts for supported
let/statement/return/branch/for reconstruction and is ratcheted against reading
the transitional `"ast"` text field.
LLVM source-local resource constructor DEFs now consume MIR expected type-name
facts for `Channel<T>` and slot-like resources (`Slot<T>`, `SecureSlot<T>`,
`DeviceSlot<T>`) instead of falling through standalone constructor expression
paths. Assignment DEF emission preserves the original assignment side effect
and then records the SSA value, so field writes remain field writes while raw
source-statement redispatch stays retired. LLVM await DEF emission, C pending
SSA-use materialization, and LLVM source DEF copy now consume MIR `expr0` /
`expr1` plus MIR local-decl/source-statement flags instead of reopening the
source statement payload. C resource mirroring now compares MIR
source-statement indexes, and C resource hook type annotation consumes the DEF
`expr1` fact instead of recovering a local declaration from source payload.
C SSA local type/view registration now consumes DEF `expr0` / `expr1` and
routine source-local type facts, including MIR destructure binding-name/index
facts for destructured locals. MIR DCE
and source-statement emit validation now consume source-shape scalar facts
instead of payload presence for those decisions, and source-statement / LLVM DEF
emit predicates are keyed by MIR emit facts rather than source payload presence.
C and LLVM residual STMT emission paths are also ratcheted to MIR
source-shape/`expr0` facts. Select dispatch condition emission is ratcheted to
MIR branch `expr0` channel facts.

## Compiler Maturity Bar

Hard self-hosting is not measured by rewriting more compiler code in Pergyra
while the C and LLVM backends still carry compatibility escape paths. The short
path to maturity is a narrow, verified core:

- Every IR layer needs a verifier that owns its contract: AIR evidence,
  HIR/DAG type resolution, MIR CFG/body/ownership facts, ABI layout facts, and
  backend fact consumption.
- Compatibility fallback is counted as debt, not as support surface. Semantic
  fallback must ratchet to zero; provenance fallback is allowed only under an
  explicit source/provenance name.
- C, LLVM, and self-hosted tools form a three-way oracle. Execution output is
  not enough; ABI shape, diagnostics, AIR/MIR JSON, authority/effect evidence,
  layout facts, and deterministic ordering are golden-test material.
- Canonicalization comes before optimization: the same meaning should produce
  the same MIR, the same ABI facts, stable declaration/generic/ability order,
  and stable emitted-code diffs.
- Pergyra's competitive axis is preserving intent/effect/authority and
  coordination evidence as first-class AOT IR facts through backend emission,
  not merely matching Swift SIL or Rust MIR feature-for-feature.

## Tiers

READY means the capability is gated and its Phase 1 mechanism is complete.
SUBSET means it works over a limited surface and substrate maturity remains.
ACTIVE means it is on the critical path and still in progress.

| # | Capability | Tier | Gate | Remaining gap |
|---|-----------|------|------|---------------|
| 1 | Module/package resolver | READY | module_smoke, package_module_resolver_smoke, type_resolution_resolver_inventory_smoke | deterministic imports and cycle diagnostics gated; a resolver tool is already self-hosted |
| 2 | Collections + iteration | READY | stdlib_surface_smoke, stage4_determinism_smoke | List/Set/HashMap have stable scalar key forms (String, Int, Long, Bool); MapKeys and SetValues order are locked; compiler-facing symbol/record/handle-like keys are normalized to canonical scalar IDs rather than raw aggregate keys |
| 3 | String/path/Unicode policy | READY | unicode_policy_smoke, source_utf8_smoke, memory_string_safety_smoke, filesystem_directory_walk_smoke | stable comparison, normalization, and deterministic directory snapshot stance gated |
| 4 | Arena/ownership ergonomics | READY | verify_arena_closure, runtime_abi_lifetime_smoke, abi_ownership_shape_smoke | `Allocator` is a single C/LLVM-backed value surface, `BoxArray` can consume a named allocator local, scratch/result/persistent lane constructors carry distinct runtime kinds, and `AllocatorDestroy(namedAllocator)` closes explicit pass-lane cleanup on C and LLVM |
| 5 | CFG/MIR body as SoT | READY | cfg_body_dataflow_smoke, ast_read_surface_smoke, mir_or_abort_invariant_smoke, ast_read_surface_checker_parity, self-host-mir-json-parity-test-smoke | non_cfg fallback locked at 0; source_ast and source_decl are ratcheted at codegen 0 / compiler 0; residual STMT source-payload emission and raw source-statement re-dispatch are retired; select and match condition/body-binding/remap emission consume MIR branch facts; resource matching uses source-index/location/anchor facts; C/LLVM destructure binding/initializer emission, C/LLVM assignment emission, LLVM source-local resource LET emission, C source-local LET/DEF/receive paths, MIR surface validation, public-surface scalar provenance seeding, and lifecycle MIR JSON source-text emission consume MIR/source-shape facts; self-hosted `mir_lower` consumes explicit MIR JSON facts for the supported CFG plus selected codegen fixture subset, rejects unsupported declaration facts cleanly, and is ratcheted against transitional `"ast"` compatibility reads |
| 6 | AIR as verifier | READY | air_json_schema_smoke, air_drift_smoke, air_backend_nonimpact_smoke | pgy.air.graph.v1 evidence export gated; drift count enforced at 0 |
| 7 | DAG type resolution SoT | READY | type_resolution_dag_smoke, type_resolution_resolver_inventory_smoke | recursive resolver compat path retired; metadata_dead_ends enforced at 0 |
| 8 | Scoped unsafe/raw escape | READY | raw_escape_contract_smoke | unsafe is scoped and capability-bound; raw pointers gated out of domain code |
| 9 | Debug info Phase 1 | READY | debug_hygiene_smoke | C #line directives and LLVM DILocation implemented |
| 10 | Runtime profile selection | READY | runtime_none_contract_smoke | runtime-none profile gated with diagnostics for unsupported features |

## Critical path

Capability 5 is closed for the measured source_ast/source_decl frontier and for
the supported self-hosted MIR-lowering subset's transitional `"ast"` text
fallback.
non_cfg body facts come from MIR and are locked at zero fallback, backend and
compiler source_ast/source_decl readers are locked at zero, residual STMT
source-payload emission and raw source-statement re-dispatch are retired,
source-statement / LLVM DEF emit predicates consume MIR facts, C/LLVM residual
STMT branches consume MIR source-shape/`expr0` facts, resource matching is MIR
source-statement-index/location/anchor based, select dispatch consumes MIR branch
`expr0` channel facts, match condition/body-binding/remap emission consumes MIR
branch pattern/guard facts, and resource-op source-statement matching uses
source-location, source-index, and anchor facts rather than payload pointer
identity. Destructure binding-name/index recovery, C/LLVM destructure emission,
and C/LLVM assignment emission are MIR-owned for SSA local type/view and
backend emission facts. C source-local LET DEF emission, generic DEF expression
emission, receive-payload type inference, MIR surface validation, and
public-surface scalar provenance seeding and lifecycle MIR JSON source-text
emission are also MIR/source-shape owned. The self-hosted checker proves the
same manifest. `mir_lower` now reconstructs the supported MIR-lower/codegen
inventory from `expr0`/`expr1`/`source_type`/`source_locals` facts only,
including `for` headers from `arg0` plus range bounds and selected
args/array/string/Bool/Float/file/recursion, straight-line call, integer
arithmetic, directory-walk, exit-guard, Bool-literal branch reassignment,
trailing-newline Log, nested string concat, string array concat, string
case/index/trim builtins, string reassignment, array pop, array for-each,
typed struct field declarations/value flow, two-log, loop-control
`continue`/`break` edge-block surfaces, and break edges after non-empty
statement blocks, inferred `Random()` Int source-local facts, and match-case
integer pattern conditions from `match_patterns` facts, plus runtime-aligned
absolute-path I/O rejection. Phi-bearing loop headers are now classified by
CFG backedges rather than phi presence alone, so nested `if` branches inside
loops are not materialized as loops. Array destructure binding names are emitted
as MIR JSON facts and consumed by `mir_lower` to reconstruct typed array-index
`Let:` bindings without source-text parsing. Plain class declarations and
methods are reconstructed from MIR field/method/owner facts and lowered through
the self-hosted value-nominal path, field-only subject/object/tobject/vessel
nominal declarations are reconstructed from MIR `nominal_kind`/field facts with
their exact AST labels, payload-free enum declarations are reconstructed from
MIR variant facts, and `Option<Int>` match branches consume MIR-owned variant
and binding facts. Array sort/map/filter/reverse combinators, `Result<Int>` core
constructors and inspection helpers, and `Join`/`ToFloat` string utility flow
also run through this MIR-JSON path. Ability declarations are reconstructed from MIR method
signature facts and treated as zero-artifact declaration hosts by the
self-hosted codegen pre-passes. The committed MIR-lower/codegen fixture
inventory is now measured at 85 PASS / 0 gap plus 0 clean rejects through this
path. Role declarations now flow as MIR-owned `kind:"role"` facts with
`for_type`, impl ability spans, and method signature facts; the supported
Int/`Arithmetic.Add` operator dispatch path is now consumed by self-hosted
MIR lowering/codegen instead of a clean-reject boundary. Richer
projection/identity semantics beyond field-only nominal declarations still
require later facts and fixtures; payload enum variants reject from their
variant facts; `self-host-mir-json-parity-test-smoke` rejects
reintroducing transitional `"ast"` reads.
Capability 4 is
closed for the current compiler-pass
substrate: named allocator lanes can be constructed, consumed by
allocation-aware owners, and explicitly destroyed through the same C/LLVM value
surface.

## Next substrate work

After capability 5, the last substrate item was pass-lane allocator cleanup.
Capability 2 now has stable `MapKeys` and
`SetValues` order for the stable scalar subset, and
`stage4_determinism_smoke` proves stable output across insertion orders for
C/LLVM-generated Pergyra programs. Compiler-facing symbol/record/handle-like
keys are now specified as canonical scalar IDs, not raw aggregate keys, and the
Stage 4 collection fixture exercises those canonical key shapes. Capability 4
now has a stable `Allocator` value surface on both C and LLVM, including
`BoxArray(capacity, allocator)` lowering through a named allocator local and
language-level `AllocatorScratch`, `AllocatorResult`, and `AllocatorPersistent`
constructors with distinct runtime lane kinds. `AllocatorDestroy(namedAllocator)`
is the explicit cleanup operation that pass authors pair with `defer`, so
scratch/result/persistent compiler pass lanes no longer need an out-of-language
cleanup convention.

The previous filesystem and parser-backend substrate items are now evidence,
not blockers: `filesystem_directory_walk_smoke` gates deterministic
`DirWalk(String) -> Array<String>` on C and LLVM; examples, production size,
and ast-read-surface self-host tools consume that surface directly; and
`parser_parity.sh` compiles the self-host parser through both C and LLVM over
the committed fixture set, including a deep nested generic type case.

AST-like mixed data trees remain a compiler-core design item rather than a
closed substitution claim. Backend/parser fixtures prove user classes, nested
records, and deep generic containers, but the current self-hosted parser and
codegen rungs still consume text AST artifacts. The hard self-host claim should
not count AST replacement until a Pergyra pass owns explicit node
records/classes and has oracle parity for traversal.

These are the axis the gap analysis calls systems substrate, distinct from the
domain-oriented surface the language is already strong on.

## Sequencing

The order that keeps each step verifiable is: keep capability 5's fact-only
self-hosted MIR-lowering gate green, keep capabilities 2 and 4 green, expand
the self-hosted tool set from validators toward the MIR dump diff
and resolver helpers named in 05, then rewrite compiler passes against the C
compiler as oracle. Starting broad parser/type-checker/backend rewrites without
this gate staying green is explicitly out of order.

## Measured closures

The formerly non-READY capability was measured against the tree and closed with
build-backed ratchets. Future self-host expansion still needs the build loop;
the status below is about substrate readiness, not a claim that every compiler
pass has already been rewritten in Pergyra.

Capability 5 (CFG/MIR SoT, task 74). READY for the measured frontier: non_cfg
body facts are MIR-owned and locked at zero fallback, the source_ast ratchet is
now codegen 0 / compiler 0, source_decl is ratcheted at codegen 0 / compiler 0,
routine_source_decl_codegen is ratcheted at 0, and the shared ratchet spec is
verified by both the shell smoke and a
Pergyra-written ast_read_surface_checker parity rung. The
dead slot-source accessors have been deleted. Against the original codegen
frontier of 127, all 127 reads are retired. LLVM domain/role and C
hosted-method forward declarations now consume
MIRDeclMethod metadata without source AST back-pointers, and C/LLVM hosted
method bodies use the linked MIRRoutine as their body provenance owner. C
class/zone collection-specialization scans consume MIR routine signature and
source-local type facts, so they no longer recover method source declarations.
C overlay projection invalidation consumes MIRDeclMethod projection write/call
facts instead of recovering method source declarations for invalidation walks.
LLVM
nominal method registry and LLVM hosted/member call emission also no longer
read method source back-pointers in MIR-active paths. LLVM function routine
lookup now keys off MIR routine names instead of source AST identity. The
zone action effect-sync path also consumes MIRDeclMethod flags without method
source back-pointers, and C member-call projection invalidation walks
MIRDeclMethod projection facts. Shared-field constructor defaults now
consume MIRDeclField initializer metadata instead of recovering source AST
nodes, and the unused shared-field source accessors are retired. C class/zone
specialization scans now reach method bodies through linked MIRRoutine
provenance instead of the hosted-method source view, and the C/LLVM routine
source thin aliases are retired. LLVM generic class method specialization now
uses MIRDeclMethod routine metadata directly, and the LLVM method body AST
compatibility accessor is retired. C hosted method body emission now passes
the linked MIRRoutine directly into the MIR body emitter, so direct backend
method source back-pointer reads are retired. The C MIR body emitter now binds
that linked routine's body as the current function context, and current-host
field identifier lowering is owned by `transpiler_host_field_identifier.c`; it
requires a self receiver, preserves lexical local shadowing, and rewrites stale
host-field SSA snapshots back through the host-field owner. C MIR emission-contract
compatibility now validates routine kind/name/signature facts without opening
routine source_ast, and LLVM intent forward declarations now use MIR routine
binding metadata directly. Non-generic LLVM function routine forward
declarations now use MIR routine signatures directly by passing no source
declaration into the compatibility emitter; generic template registration
records MIRRoutine entries and generic specialization emits through that
routine. LLVM MIR body emission consumes routine kind/signature/current-routine
metadata without recovering source declarations, and LLVM intent body emission
now starts from active declaration inventory instead of recovering intent source
declarations from routine payloads. LLVM function routine
emit/validation now consumes MIR routine kind/name/generic facts without
recovering source declarations; source-declaration compatibility diagnostics
live at the body emit boundary. C host-method
lookup now performs method-name matching from MIRDeclMethod metadata, while
LLVM host-method lookup keeps its non-MIR fallback on the explicit compatibility
array and the unused LLVM hosted method source accessor is retired, along with
the thin LLVM MIR method source alias. C/LLVM routine source thin aliases are
retired, routine source declaration checks no longer appear in codegen,
`mir_routine_source_decl_of_type` is compiler-owned only,
`MIRDeclHeader.source_ast` and `mir_decl_header_source_decl` are removed, and
no compiler or backend `.c` file contains a source_ast declaration-header
payload read. Type-alias target names are now captured on `MIRDeclHeader`, validated by
`mir_decl_header_validate.c`, resolved through declaration-header inventory
accessors, and consumed by LLVM alias mapping/rendering and MIR source-local
type facts before any compatibility AST fallback. C/LLVM now compile and run the
`type_alias_array_context` fixture, proving empty `Array<T>` alias contexts use
the same canonical MIR fact. C projection literal/source-path lowering now has a
by-name entry point that consumes MIR declaration headers and `MIRDeclField`
rows, so ToTObject, projection-borrow materialization, member access, and domain
provenance refresh no longer need projection source declarations in MIR-active
paths. LLVM projection-borrow materialization and member access now use the same
by-name MIR header path instead of recovering projection source declarations.
LLVM domain projection value lowering also uses by-name MIR header source paths
instead of recovering projection source declarations.
LLVM generic class specialization type mapping now reads hosted field type names
from `MIRDeclField` metadata before any template-AST fallback.
LLVM class constructor argument lowering now consumes `MIRDeclField` type-name
metadata for expected-type context before template-AST fallback.
C/LLVM class field-slot claim helper emission now consumes `MIRDeclFieldClaim`
metadata instead of reopening class destructure AST in MIR-active paths.
C/LLVM role-slot ability tag rendering now fills omitted generic actuals from
`MIRDeclHeader` generic metadata instead of reopening ability source
declarations in MIR-active paths. Ability declaration method rows are also
captured on `MIRDeclHeader`, and C party-slot method dispatch consumes those
`MIRDeclMethod` rows to pick the owning ability tag instead of reopening
`ast_ability_method_*` in MIR-active paths.
C/LLVM declaration existence checks that only need a yes/no result now consume
header-backed existence seams in MIR-active paths, so class/enum/function/
intent/callable/constructor presence no longer recovers origin AST declarations.
C/LLVM declaration payload compatibility now validates MIR declaration-header
rows before active-inventory lookup instead of opening declaration-header
source_decl provenance. Method and field back-pointers are already removed, and
the old header-shape AST recomputation arm is gone. Build-gated.
C/LLVM zone refresh compatibility arrays are also confined to the hosted refresh
view owners; projection sync/value and invalidation consumers use view-owned
mapped-source/source-field APIs instead of indexing compatibility arrays.
Residual `MIR_INST_STMT` source-payload emission is now retired in C and LLVM;
side-effect statements are carried as `MIR_STMT.expr0` executable facts.
Source-local declarations and assignments no longer re-dispatch through raw
source-statement emitters, and LLVM source-local resource constructors consume
MIR expected type-name facts at the DEF owner for `Channel<T>` and slot-like
resources. Assignment DEF emission preserves source assignment side effects
while recording the SSA value. Await DEF emission, pending SSA-use
materialization, and source DEF copy also consume MIR expression/type facts
directly. C resource matching now uses source-statement indexes plus
source-location/anchor facts, and the C resource hook DEF type annotation uses
`expr1`. C SSA local type/view facts
consume DEF expression/type facts and MIR destructure binding-name/index facts.
C/LLVM MIR destructure emission consumes MIR initializer and binding facts.
MIR DCE
and source-statement emit validation use source-shape scalar facts for their
decisions, and source-statement / LLVM DEF emit predicates use MIR emit facts
instead of payload presence. C/LLVM residual STMT branches consume MIR
source-shape/`expr0` facts, and LLVM missing-return-value diagnostics use MIR
topology errors rather than payload anchors. Select dispatch uses MIR branch
`expr0` channel facts instead of source case payloads. Match body binding and
remap also consume MIR branch pattern/guard facts rather than match-case source
payloads, and resource-op source-statement matching no longer uses payload
pointer identity. C/LLVM assignment emission, LLVM source-local resource LET
emission, MIR surface validation, and public-surface scalar provenance seeding
now consume MIR/source-shape facts, lifecycle MIR JSON source-text emission
also consumes a captured source-shape fact, and the C preserved-statement
helper surface has been retired. The self-hosted `mir_lower` parity rung now
rejects transitional `"ast"` text reads and reconstructs the supported subset
from explicit MIR JSON facts. LLVM for-in and with-slot resource-claim
diagnostics use MIR expression anchors.

Capability 2 (collections). Closed for the hard-self-host substrate: integer keys are implemented
(pgy_runtime_map_int_key_inline.h covers i32 and i64), and `MapKeys` /
`SetValues` now return stable sorted snapshots for String, Int, Long, and Bool
keys/values. The compiler-facing policy is to normalize symbol/record-like
identities to canonical strings and handle-like identities to stable integer or
long IDs before insertion. `stage4_determinism_smoke` exercises those canonical
key shapes across forward and reverse insertion orders on C and LLVM. Raw
aggregate keys remain out of beta rather than becoming a second collection
truth.

Capability 4 (arena ergonomics). Closed for the hard-self-host substrate:
compiler-internal scratch/result/persistent lanes are mirrored by the
language-level `Allocator` value surface on C and LLVM, lane constructors carry
distinct runtime kinds, `BoxArray(capacity, allocator)` consumes named allocator
locals, and `AllocatorDestroy(namedAllocator)` gives pass authors an explicit
cleanup operation. Build-gated.

The honest summary is that deterministic collection, allocator substrate, and
the measured CFG/MIR body SoT frontier are closed for hard-self-host planning.
The remaining critical path is actual staged compiler-pass substitution:
semantic breadth first, then MIR/HIR and codegen parity slices against the C
compiler oracle.
