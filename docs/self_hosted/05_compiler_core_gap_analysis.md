# Compiler Core Gap Analysis

This document records why Pergyra must not start hard self-hosting from the
compiler core immediately after beta. It converts the gap into entry criteria
for soft and partial self-hosting.

## Current Reality

Current judgement (2026-06-21): the hard-self-host substrate checklist is broad
enough for staged compiler-pass substitution, but capability 5 remains ACTIVE
until self-hosted MIR lowering deletes and ratchets the transitional `"ast"`
text compatibility fallback. The supported MIR JSON parity subset now consumes
explicit `expr0`/`expr1`/`source_type`/`source_locals` facts first for
let/statement/return/branch/for reconstruction. Raw source-statement
re-dispatch is gone, source-local resource constructors
consume MIR expected type facts, assignment DEFs preserve side effects before
SSA recording, C resource mirroring uses MIR source-statement identity instead
of source-payload pointer identity, and public-surface plus lifecycle MIR JSON
source provenance is capture-time scalar/text fact data; not every body fact is
MIR-owned yet. This is still not a full hard-self-host claim. Passing
`self-host-preparation-test-smoke` proves the side-by-side method and the
C/LLVM/Pergyra parity harness, not permission to replace the semantic checker,
MIR lowering, codegen, compiler driver, or runtime in one jump.

The beta stable subset is intentionally narrow. It is designed to freeze the
core language contract, not to rewrite a large compiler immediately.

Stable today means:

- classifier-backed ownership on the implemented copy, aggregate, movable
  value, slot-handle, channel, and return paths;
- runtime-validated `Slot` / `SecureSlot` / pin contracts instead of Rust-style
  lifetime annotations;
- monomorphized generics with ability/default-argument coverage on the tested
  declaration, call, and module-consumer paths;
- `List`, `Set`, and `HashMap<String|Int|Long|Bool, T>` collection subset;
- Result-first failure handling and explicit diagnostics;
- C backend as the execution oracle, with LLVM parity still gated by explicit
  support-matrix rows.

This is enough for compiler-adjacent tools. It is not yet enough for a
200K-line compiler core rewrite with dense graph traversal, pointer ownership,
arena allocation, lifetime handoff, parser recovery, and backend emission.

## Mismatch

Pergyra's distinctive semantic surface is domain-oriented:

- `subject` names an important domain information center;
- `zone` / `world` model boundary and ownership context;
- `intent` is the orchestration spine for human and AI-authored goals;
- `authority`, `role`, and `party` separate capability from actor identity;
- `slot` makes resource handles portable across backends.

A compiler core stresses a different axis:

- high-volume file I/O and token streams;
- graph algorithms over declarations, CFG, AIR, MIR, and backend inventory;
- arena-heavy transient allocation;
- deterministic data structures and stable output ordering;
- raw interop with C, LLVM, debug info, and platform toolchains;
- tight performance loops where allocation and ownership cost are visible.

The domain model is still useful for the compiler, but it is not a replacement
for systems substrate maturity. Hard self-hosting becomes credible only when
the systems substrate can carry those compiler workloads without forcing every
pass into awkward workarounds.

## Non-Negotiable Pre-Hard-Self-Host Capabilities

Hard self-host cannot start until these are available and smoked. The current
scorecard marks nine substrate forms READY and capability 5 ACTIVE: the
source_ast/source_decl frontier, residual STMT source-payload emission, and
source-local raw statement re-dispatch are closed. LLVM await DEF emission,
C pending SSA-use materialization, LLVM source DEF copy, source-statement emit
predicates, and LLVM DEF emit predicates now consume MIR `expr0` / `expr1`,
source-location, and emit flags directly. C and LLVM residual STMT emission
branches consume MIR source-shape / `expr0` facts, and LLVM missing-return-value
diagnostics use MIR topology errors instead of source payload anchors. Select
dispatch branches carry their readiness channel as a MIR branch `expr0` fact,
so C/LLVM condition emission no longer parses select case payloads. Match-case
condition, body-binding, and remap emission consume MIR-captured branch
pattern/guard facts instead of the match-case source payload. The resource
mirroring path now compares MIR source-statement indexes and source-location /
anchor facts instead of payload pointer identity, and the C resource hook uses
MIR `expr1` type annotations instead of recovering local-decl payloads. C SSA
local type/view registration now consumes MIR destructure binding-name/index
facts instead of reopening destructure statement payloads, and C MIR
destructure emission consumes `inst->expr0` plus those binding facts instead of
the source destructure statement. LLVM MIR destructure emission now consumes the
same initializer and binding facts through `llvm_emit_mir_destructure_inst`.
C and LLVM assignment emission now consume MIR target/value facts:
`MIR_INST_ASSIGN` requires `expr0`/`expr1`, assignment DEFs carry their target
in `expr1`, and backend assignment-parts emitters preserve slot, array, field,
and projection assignment semantics without reopening the source statement
payload. LLVM source-local resource constructor LET emission also consumes MIR
initializer/type facts instead of reopening the source local declaration
payload. C source-local LET DEF emission, generic DEF expression emission, and
receive-payload type inference now consume instruction `arg0` / `expr0` /
`expr1` facts directly, so C codegen no longer calls
`mir_instruction_source_payload`. MIR surface validation now checks payload
presence through source-shape predicates and validates surface-usage facts from
MIR expression facts rather than reopening payloads. Public-surface source
line/column/stable-id/type seeding and lifecycle MIR JSON source-text emission
now consume capture-time facts from
`mir_instruction_capture_source_provenance(...)`; lifecycle dumps consume
`mir_instruction_source_inline_text(inst)` instead of reopening source
payloads. LLVM for-in and with-slot resource-claim diagnostics have already
moved to MIR expression anchors. The remaining capability-5 tail is the
self-hosted `mir_lower` compatibility fallback to transitional `"ast"` text;
the supported parity subset already reconstructs from explicit MIR JSON facts,
including `for` headers from `arg0` plus `expr0`/`expr1` range bounds. The
fallback must be removed and ratcheted before the body source-of-truth row is
fully ready.

- **Module/package resolver stability**: deterministic imports, manifest
  reading, path normalization, and cycle diagnostics.
- **Collections and iteration**: maps/lists/sets over strings, symbols, small
  records, and stable iteration order where output determinism matters.
- **String/path/Unicode policy**: stable comparison, normalization stance,
  slicing or explicit reject, and filesystem-safe path operations.
- **Arena and ownership ergonomics**: compiler-pass scratch/result/persistent
  allocation lanes without manual resource boilerplate in every pass.
- **CFG/MIR body safety as source-of-truth**: move/use/drop/cleanup facts must
  come from CFG/MIR, not AST helper fallbacks.
- **AIR as verifier, not codegen IR**: every abstraction boundary used by
  tools must export stable evidence through `pgy.air.graph.v1`.
- **DAG type resolution as source-of-truth**: no retired recursive resolver
  compatibility path and no unresolved metadata dead-end for stable type facts.
- **Scoped unsafe/raw escape policy**: `unsafe` must be scoped and
  capability-bound; raw pointers must not leak into normal business/domain
  code.
- **Debug info**: generated C/LLVM output needs a DWARF/CodeView Phase 1 path
  before serious self-host debugging.
- **Runtime profile selection**: compiler tools need runtime-none or
  minimal-runtime profiles with clear diagnostics for unsupported features.

## What Can Be Self-Hosted First

Start with tools that use stable inputs and produce stable outputs:

1. Diagnostic catalog checker.
2. AIR graph JSON validator.
3. MIR dump diff tool.
4. Stable subset manifest checker.
5. Backend output comparator.
6. Module/package resolver helper.

These tools are valuable because they stress the language while preserving the
C compiler as oracle. They also expose which missing language features are real
dogfood blockers rather than speculative nice-to-haves.

## What Must Not Be Done First

Do not start self-hosting with:

- parser rewrite;
- type checker rewrite;
- MIR lowering rewrite;
- C backend rewrite;
- LLVM backend rewrite;
- runtime replacement;
- new syntax added only because the compiler rewrite feels hard.

Those paths make the language depend on unproven features before the stable
subset has survived dogfood.

## Architectural Preparation

The self-hosted compiler should not copy the current C layout. The C compiler
was split under C namespace constraints and beta debt pressure. The Pergyra
version should use the language's own module boundaries:

- `compiler.lex`
- `compiler.parse`
- `compiler.ast`
- `compiler.semantic`
- `compiler.dag`
- `compiler.cfg`
- `compiler.air`
- `compiler.mir`
- `compiler.codegen.c`
- `compiler.codegen.llvm`
- `compiler.diagnostics`
- `compiler.runtime_contracts`

Rules:

- split by responsibility and evidence owner, not line count alone;
- avoid `_helpers` modules except for truly cross-feature infrastructure;
- keep cross-layer policy vocabulary in explicit common owners. For example,
  worker-boundary growable-storage names are owned by
  `worker_boundary_storage_policy`, while semantic and C/LLVM only decide
  whether a local fact maps to that policy;
- every pass has an intent-verification pair: named intent, input contract,
  output contract, oracle, and negative fixture;
- every generated artifact has deterministic ordering;
- every unsafe/raw operation is scoped and justified at the boundary.

## Entry Criteria

Hard self-host may be considered only when all are true:

- at least two soft self-host tools run in CI against C-oracle fixtures;
- the stable subset manifest is generated and checked by a Pergyra tool;
- AIR graph JSON validation is written in Pergyra and agrees with the C
  validator;
- module/path/file/string basics are implemented without host-specific hacks;
- `unsafe` is scoped and diagnostics reject unscoped raw/system escapes;
- debug-info Phase 1 is implemented or explicitly replaced by an equivalent
  source-level debugging workflow;
- performance baselines exist for at least one graph-heavy tool and one
  string/file-heavy tool.

Until then, self-host preparation means building tools and closing substrate
gaps, not rewriting the compiler core.
