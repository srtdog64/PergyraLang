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

The remaining body-level SoT tail is narrower and now explicitly named:
residual `MIR_INST_STMT` source-payload emission is retired in C and LLVM, and
side-effect statements are carried through `MIR_STMT.expr0` executable facts.
Source-local declaration and assignment paths no longer re-dispatch through
raw source-statement emitters. The next cut target is the remaining
`requires_source_statement_emit` / `mir_instruction_source_payload` expression
and shape tail: selected body facts are still read from AST payloads rather than
dedicated MIR records.
LLVM source-local resource constructor DEFs now consume MIR expected type-name
facts for `Channel<T>` and slot-like resources (`Slot<T>`, `SecureSlot<T>`,
`DeviceSlot<T>`) instead of falling through standalone constructor expression
paths. Assignment DEF emission preserves the original assignment side effect
and then records the SSA value, so field writes remain field writes while raw
source-statement redispatch stays retired. LLVM await DEF emission, C pending
SSA-use materialization, and LLVM source DEF copy now consume MIR `expr0` /
`expr1` plus MIR local-decl/source-statement flags instead of reopening the
source statement payload.

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
| 5 | CFG/MIR body as SoT | ACTIVE | cfg_body_dataflow_smoke, ast_read_surface_smoke, mir_or_abort_invariant_smoke, ast_read_surface_checker_parity | non_cfg fallback locked at 0; source_ast and source_decl are ratcheted at codegen 0 / compiler 0; residual STMT source-payload emission and raw source-statement re-dispatch are retired; selected source-payload expression/shape reads remain |
| 6 | AIR as verifier | READY | air_json_schema_smoke, air_drift_smoke, air_backend_nonimpact_smoke | pgy.air.graph.v1 evidence export gated; drift count enforced at 0 |
| 7 | DAG type resolution SoT | READY | type_resolution_dag_smoke, type_resolution_resolver_inventory_smoke | recursive resolver compat path retired; metadata_dead_ends enforced at 0 |
| 8 | Scoped unsafe/raw escape | READY | raw_escape_contract_smoke | unsafe is scoped and capability-bound; raw pointers gated out of domain code |
| 9 | Debug info Phase 1 | READY | debug_hygiene_smoke | C #line directives and LLVM DILocation implemented |
| 10 | Runtime profile selection | READY | runtime_none_contract_smoke | runtime-none profile gated with diagnostics for unsupported features |

## Critical path

Capability 5 is closed for the measured source_ast/source_decl frontier, but it
is still active for body-level source-payload compatibility emission. non_cfg
body facts come from MIR and are locked at zero fallback, backend and compiler
source_ast/source_decl readers are locked at zero, residual STMT source-payload
emission and raw source-statement re-dispatch are retired, and the self-hosted
checker proves the same manifest. The remaining source-payload expression/shape
tail must be cut before this row can honestly return to READY. Capability 4 is
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

The order that keeps each step verifiable is: finish capability 5's remaining
source-payload expression/shape tail, keep capabilities 2 and 4 green, expand
the self-hosted tool set from validators toward the MIR dump diff and resolver
helpers named in 05, then rewrite compiler passes against the C compiler as
oracle. Starting broad parser/type-checker/backend rewrites while capability 5
remains ACTIVE is explicitly out of order.

## Measured gaps (blocker burn-down)

The non-READY capability was measured against the tree to make it actionable.
Every remaining step needs the build loop, which is the reason none can be
closed from a static pass alone.

Capability 5 (CFG/MIR SoT, task 74). ACTIVE, with the measured frontier closed: non_cfg
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
directly. The remaining ACTIVE tail is the narrower match/select/resource
shape and selected diagnostics surface, where selected body facts still need
dedicated MIR records or explicit provenance-only handling.

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

The honest summary is that deterministic collection and allocator substrate are
closed for hard-self-host planning, while CFG/MIR body SoT still has one named
source-payload expression/shape tail. The remaining critical path is to cut
that tail, then continue actual staged compiler-pass substitution: semantic
breadth first, then MIR/HIR and codegen parity slices against the C compiler
oracle.
