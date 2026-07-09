# Self-Hosted Compiler Substrate Architecture

Status: `self-host-substrate-architecture-contract`

This document connects the compiler world shape to the concrete substrate a
Pergyra-written compiler needs. It sits below
`11_compiler_world_architecture.md` and
`12_intent_zone_self_host_architecture.md`.

It is not a release claim that the compiler is already self-hosted. It is the
architecture contract for how hard self-hosted slices should be added without
recreating the C compiler's folder graph.

The executable expansion checklist is
[`15_pre_self_host_expansion_ledger.md`](15_pre_self_host_expansion_ledger.md).
That ledger classifies each pre-self-host surface as `READY`, `ACTIVE`, or
`HOLD`; this document explains the architecture behind those classifications.

## Core Rule

The self-hosted compiler has one visible compiler flow and many fact owners.

- `PgyCompilerWorld` owns the compiler action.
- Stage directories own stage facts.
- Resource zones own isolated compiler resources.
- Intent clusters compose resources into compiler actions.
- Parity gates prove that a Pergyra implementation can replace one real slice.

The unit of architecture is not "a folder with code." The unit is an owned
fact, resource, or artifact boundary.

The compiler world also avoids generic stage actors. `LexerStage`,
`ParserStage`, `SemanticStage`, and `MirLowerStage` are separate participants
because they own different artifacts. A shared `StageOwner` alias would hide
which stage is allowed to scan tokens, build AST facts, prove semantic
verdicts, or lower MIR facts.

## Compiler Tree And Projection Nerves

The intended shape is tree-like, not bucket-like:

- `PgyCompilerWorld` is the trunk: it names the visible compiler action and the
  resource topology.
- Stage fact zones are owned nodes: source intake, token stream, AST tree,
  semantic verdict, MIR fact graph, type environment, ABI layout, emission, and
  parity evidence.
- `codegen/` is not a separate backend kingdom. It is the projection nerve
  bundle that leaves the compiler world after MIR/type/ABI facts are known.
- A codegen zone is a bundle around a resource: emitted text, type facts, ABI
  layout facts, symbol/mangle facts, runtime-link facts, or artifact facts.
- C, LLVM, and self-hosted emission are projections that consume the same
  owner facts. They must not invent their own field order, symbol spelling,
  authority evidence, slot layout, or unsupported-surface verdict.

The same rule is why the self-hosted compiler must not become a C folder graph
rewritten in Pergyra syntax. C is only the current CPU projection. A future
tensor/NPU or other accelerator projection should attach at the same codegen
nerve bundle and consume the same intent/effect/authority/coordination/slot/
layout/loss facts, with explicit reject or fallback evidence when the target
cannot accept them.

That means the codegen folder may contain many action files, but those files are
not the architecture. The architecture is the path facts take from
`PgyCompilerWorld` into backend projections. A new codegen file is only a new
participant unless it owns a distinct resource or fact table.

Anti-rule: do not split codegen by "expr/stmt/function" and then call each
split a zone. Those are nerves in one projection bundle while they mutate the
same emitted artifact. Split zones only when the resource owner changes.

## Architecture Stack

The self-hosted compiler is described at three levels.

| Level | Owner | What it decides | What it must not decide |
|---|---|---|---|
| compiler world | `src/self_hosted/compiler/world.pgy` | the visible compiler flow, resource zones, root intent, path manifest shape | stage-local facts, backend layout, parser recovery rules |
| stage fact owners | `lexer/`, `parser/`, `semantic/`, `mir_lower/`, `codegen/` | the artifact owned by one stage | orchestration aliases, oracle policy, unrelated stage facts |
| shared substrates | `lib/`, future collection/import/diagnostic/layout owners | reusable facts consumed by multiple stages | hidden semantic recovery from AST/text payloads |

This is the architecture difference from the C implementation. The C compiler
may stay fragmented because it is the oracle. The Pergyra compiler must be read
as one compiler world that delegates to named fact owners.

The target layout is therefore not:

```text
compiler/
  frontend/
  middle/
  backend/
  helpers/
```

The target layout is:

```text
compiler/
  world.pgy                 -- topology and root intent
  stage_intents.pgy         -- compiler action clusters
  path_manifest_owner.pgy   -- path facts
  stage_artifact_owner.pgy  -- stage artifact envelope facts
  driver_rung0_owner.pgy    -- source -> AST text -> emitted C assembly owner
  driver_rung0_main.pgy     -- DRV-0 runnable artifact boundary

lexer/                      -- token facts
parser/                     -- AST/tree facts
semantic/                   -- diagnostic/type verdict facts
mir_lower/                  -- MIR JSON/fact lowering
codegen/                    -- backend resource cluster
lib/                        -- shared fact owners, not helper buckets
```

Folders are allowed only when they expose ownership. They are not a license to
copy the C file graph.

## Compiler Flow

The root flow is:

1. `SourceIntakeZone`: source path, root source, import bundle, path manifest.
2. `TokenStreamZone`: token stream facts.
3. `AstTreeZone`: AST tree facts and source provenance.
4. `SemanticVerdictZone`: diagnostic verdicts, type facts, and fail-closed
   semantic checks.
5. `MirFactGraphZone`: MIR JSON/fact graph, CFG/body facts, and ABI-relevant
   lowering facts.
6. `TypeEnvZone`: read-mostly type environment consumed by backend emission.
7. `AbiLayoutZone`: read-only ABI/layout facts consumed by backend emission.
8. `CompatibilityEvolutionZone`: source/ABI/behavior/diagnostic/AIR/MIR/trace/
   capability/stdlib compatibility plus obsolete migration metadata.
9. `AirEvidenceZone`: AIR evidence vocabulary for hard-rung proof facts.
10. `SymbolFactTableZone`: cross-backend symbol rows.
11. `AbiRowProjectionZone`: cross-backend ABI/layout rows.
12. `EmissionZone`: emitted artifact buffer.
13. `ArtifactZone`: comparable diagnostic, AST text, AIR JSON, MIR JSON,
    ABI/layout, runtime-materialization, emitted, and run artifacts.
14. `TestHarnessZone`: fixture/result row vocabulary.
15. `SubprocessRunnerZone`: capability envelope for oracle processes.
16. `ParityZone`: C/LLVM/Pergyra comparison evidence.

`CompilePergyraProgram` is the root intent over those resources. The derived
pipelines in `stage_intents.pgy` are compiler actions, not hidden helper
folders.

## Stage Owners

Each stage directory owns a different kind of source-of-truth:

| Stage | Owned artifact | Current oracle |
|---|---|---|
| `lexer/` | token text | `pgy --tokens` |
| `parser/` | AST text | `pgy --ast` |
| `semantic/` | diagnostic verdict blocks | C compiler accept/reject oracle |
| `mir_lower/` | MIR JSON fact lowering | C backend run-output oracle |
| `codegen/` | emitted C and run stdout | C/LLVM backend run-output oracle |
| `compiler/` | world, path manifest, root compiler intent | compiler-world contract smoke |

The entrypoint `main.pgy` in a stage is only a run boundary. It must not own
semantic facts, AST recovery, JSON lookup, diagnostic rendering, or fallback
translation. Those decisions belong in named owner files.

## Required Substrates

Hard self-hosting needs these substrates before a slice can count as a real
compiler replacement.

| Substrate | Owner shape | Reason |
|---|---|---|
| path manifest | `StagePathManifest` plus path owner | prevents repeated recursive discovery and platform path drift |
| import graph | source-bundle/import owner | prevents duplicate declaration materialization and hidden source order |
| deterministic collections | collection owner or stable iteration policy | keeps diagnostics, MIR JSON, emitted C, caches, and parity output stable |
| diagnostic rendering | shared diagnostic owner | prevents raw text or JSON construction in entrypoints |
| JSON read primitives | shared JSON owner | prevents every fact tool from hand-rolling string scans |
| type environment | `TypeEnvZone` and stage type-fact owners | prevents backend emitters from re-inferring source types |
| parameter mode facts | AST printer plus stage function-signature owner | keeps `inout` / `own` / `ref` ABI decisions from being guessed from body text |
| MIR fact graph | `MirFactGraphZone` | gives backend and self-host lowering one fact source |
| ABI/layout facts | MIR ABI/layout owner | prevents C/LLVM/self-hosted emitters from inventing layout independently |
| symbol/mangle facts | symbol owner | prevents backend emitters from spelling names independently |
| compatibility evolution facts | `compatibility_evolution_owner.pgy` plus `CompatibilityEvolutionZone` | prevents source/API/ABI/diagnostic/AIR/MIR/runtime/capability/stdlib compatibility policy from splitting across docs and scripts |
| AIR evidence rows | `air_evidence_owner.pgy` plus `AirEvidenceZone` | keeps intent/effect/authority/coordination/materialization proof facts consumable |
| cross-backend ABI rows | `abi_layout_row_owner.pgy` plus `AbiRowProjectionZone` | makes field order, tag, niche, ownership, size/align, and materialization policy one table |
| cross-backend symbol rows | `symbol_table_owner.pgy` plus `SymbolFactTableZone` | makes C/LLVM/self-hosted spelling rows one table |
| emission buffer | `EmissionZone` | gives output writes one owner |
| artifact evidence | `artifact_zone_owner.pgy` plus `ArtifactZone` | keeps diagnostics, AIR JSON, MIR JSON, ABI/layout, runtime materialization, emitted artifacts, and run output comparable |
| parity evidence | `ParityZone`, `test_harness_owner.pgy`, and `subprocess_runner_owner.pgy` | proves substitution against the C/LLVM oracle pair without giving the shell a permanent SoT role |
| runtime materialization policy | runtime/frontier owner | distinguishes erased hot paths from explicit managed boundaries |
| target capability envelope | projection fact owner | keeps CPU, self-hosted, and future accelerator acceptance/reject/fallback decisions visible |
| nominal-record array substrate | LLVM array registry element-name facts plus raw record-array exports | lets typed compiler records live in `Array<T>` without AST element-type guessing |

If one of these facts is missing, the fix is to add the fact to the owner or
fail closed. The fix is not a local compatibility fallback.

### Incremental Compilation Position

Incremental compilation is required for self-host scale, but it must be
fact-owner based. A file timestamp or emitted-text cache would reintroduce the
same source-of-truth drift that the hard self-host gates are removing.

The cache key for a reusable stage artifact must include the source bytes hash,
import/module graph fingerprint, language/runtime/stdlib/capability/target
profile, stage owner schema version, ABI/runtime row fingerprints, and tool
executable version. A cache hit is usable only after the stage verifier proves
that every consumed owner fact and dependency fingerprint still matches.

Reusable units are stage artifacts, not ad hoc text snippets: lexer token
streams, parser typed-tree bridge artifacts, semantic verdict/fact rows, MIR
and AIR fact graphs, ABI/layout rows, backend emitted artifacts, diagnostics,
and parity evidence. Missing owner facts fail closed and recompute; no
incremental path may reconstruct semantic facts from cached text or backend
output.

The first implementation target should be the self-host completeness and
codegen parity harnesses, because they already expose the source set, stage
owner boundaries, artifact rows, and oracle comparison points. The compiler
driver can consume the same cache discipline after those harness caches prove
stable.

Rung 0 of this cache is deliberately coarse: the self-host completeness harness
keys stage-tool builds by the full production source-set fingerprint, tool
source hash, and compiler executable fingerprint, then keys pass artifacts by
the full production source-set fingerprint, stage name, source path, check
target, tool executable fingerprint, and producer executable fingerprint. This
makes repeated proof runs cheap without pretending that import graph
invalidation is solved. A change to any production self-host source invalidates
the current cache; later rungs may replace `source-set` with precise
import/module graph fingerprints after that graph is Pergyra-owned.

The matching rung0 impact plan is also fact-owned:
`completeness_ledger_owner.pgy` emits
`pgy.selfhost.completeness-impact.v1`, mapping source patterns to the
`PGY_SELFHOST_COMPLETENESS_SOURCES` /
`PGY_SELFHOST_COMPLETENESS_STAGES` knobs and the required proof gate. This is
not yet automatic dependency invalidation; it is the owner-owned contract that
prevents changed-source impact from living as an unreviewed shell list.

The pre-self-host expansion ledger is the ratchet for that rule: a hard rung may
consume `READY` surfaces, must treat `ACTIVE` surfaces as blockers or explicit
unsupported input, and must not depend on `HOLD` surfaces.

The basic nominal-record array substrate is now a `READY` input to that ledger:
`Array<NominalRecord>` supports creation, parameter passing, push, set, pop,
indexing, and indexed member access under C/LLVM parity. It is not yet a full
generic collection-algorithm surface; map/filter/sort/slice over nominal
records need separate ABI/runtime owners before hard rungs can rely on them.
The current self-host codegen consumes that substrate for the bootstrap-owned
`Array<CodegenAstTextNode>` bridge and now has a typed AST arena payload
contract in `codegen/typed_ast_node_skeleton.pgy`. The contract proves the
flat node vocabulary and traversal idiom; it is not yet a claim that parser or
codegen has replaced the transitional AST-text payload. The current codegen does
consume typed arena indent/parent facts for program, function, and statement
emission-depth traversal, so new depth decisions must be added to the arena
projection owner rather than reading raw text-node indentation. `GenerateCUnit`
builds that projection once and passes the `AstArena` fact into function and
statement emission participants; downstream emitters must not rebuild the bridge
projection locally. The arena also carries name, type-name, and mode rows for
function/declaration emission, so signature and field metadata must be consumed
from the typed arena projection rather than directly from transitional text-node
fields. Single-payload statements, `Let` name/type/initializer, `Assign`
target/RHS, `ArrayPush` target/value, and `ArraySet` target/index/value now
consume arena atom/value rows; `For` loop-var/start/end/collection payloads do
the same. Program runtime/header usage facts are also consumed from the same
arena projection instead of re-reading raw `CodegenAstTextNode` payload/kind
rows. Those facts are now lane-specific: type/header requirements read
`type_name` rows, builtin-call requirements scan only expression-bearing rows
with string-literal-aware call matching, and statement-only requirements use
kind facts. This still leaves expression payload strings inside the transitional
bridge; it removes the whole-arena usage scan, not the final expression-row
blocker.

`src/self_hosted/compiler/path_manifest_owner.pgy` is the current path owner.
It owns the Pergyra source/test/parity path values for `StagePathManifest` and
the active stage-to-world binding rows. Each row names the stage, resource zone,
actor, intent, and payload contract so a bootstrap driver can route a stage
through `PgyCompilerWorld` instead of inferring ownership from a folder name.
`tests/self_hosted/compiler_world_manifest.sh` is the shell projection and is
contract-checked against the Pergyra owner.

`src/self_hosted/compiler/stage_artifact_owner.pgy` is the current stage
artifact and payload-readiness owner. It proves that each active stage actor
consumes a manifest path and the expected world/zone/actor/intent binding row
before it can claim readiness. The binding row also names the payload contract,
and the readiness function delegates to that owner for the stage: lexer token
facts, parser AST-tree text facts, semantic verdict facts, MIR fact graph rows,
or the codegen typed-AST arena migration contract. That is the load-bearing
replacement for the old stage `return true` scaffolding.

`src/self_hosted/compiler/driver_rung0_owner.pgy` is the current DRV-0 assembly
owner. It consumes the path manifest, stage artifact readiness, and target
capability envelope before composing `ParseRootProgram(source)` with
`GenerateC(ast_text)`. The owner exposes that boundary as three artifact
functions: `CompileSourceToAst`, `CompileAstToC`, and `CompileSourceToC`.
`src/self_hosted/compiler/driver_rung0_main.pgy` is only the runnable artifact
boundary for those facts. `tests/self_hosted/parity/driver_rung0_parity.sh`
compares assembled AST text against a separately built self-parser AST producer
and assembled emitted C against the current codegen oracle, so DRV-0 is a
landed artifact rung. It deliberately does not own parser facts or codegen
emission facts; those remain in their stage owners.

## Codegen Architecture

Self-hosted codegen is a backend resource cluster:

- `TypeEnvZone` owns type binding facts.
- `AbiLayoutZone` owns ABI/layout facts.
- `EmissionZone` owns emitted C.
- `ProgramEmitter` is the participant that writes through `EmissionZone`.
- `input/` owns AST path/read boundaries and the AST-text line inventory while
  the rung still consumes transitional parser AST text.
- `run/` owns CLI-to-output orchestration.
- `text/` owns text and expression scanning facts for the compatibility bridge.
- `type_facts/` owns the type environment consumed by emitters.
- `compiler/symbol_table_owner.pgy` owns emitted-symbol spelling rows.
- `abi_layout/` owns self-host C ABI type spelling facts for the supported
  signature, local declaration, and field subset.
- `runtime_abi/` owns self-host C collection, math/random, host I/O/argv,
  Option/Result, and string/text runtime helper symbol facts for the supported
  subset.
- `emission/` contains participants that write or route emitted C.

`program_emit`, `function_emit`, `stmt_emit`, `expr_rewrite`, and
`struct_value_emit` are not zones. They are action participants over the same
output and type resources. A new zone appears only when there is a new distinct
resource, such as a mutable cross-backend symbol/name-mangling table.

The current `input/` bridge has explicit fact owners:

- `ast_text_inventory_owner.pgy` owns raw AST-text line splitting, typed
  `CodegenAstTextNode` inventory, indentation/kind rows, blank-line
  filtering, `[export]` normalization, marker-node predicates, declaration and
  signature payload accessors, and cursor expectation diagnostics.
- `ast_text_typed_arena_owner.pgy` owns projection from the text inventory into
  typed `AstArena` rows, including parent, indent, value, and aux-value facts.
- `ast_text_row_fact_owner.pgy` owns the typed `CodegenAstTextRowFactInput`
  contract and the name/type/value/aux-value/mode rows derived from payloads:
  function, return, role, nominal, enum, field, parameter, and `Let`
  name/type row facts are populated once during inventory construction and
  then consumed from `CodegenAstTextNode` fields.
- `ast_text_array_literal_owner.pgy` owns transitional array literal shape and
  top-level element facts for `Let` initializers while expression payloads are
  still string-backed.
- `text/enum_literal_owner.pgy` owns payload-free enum literal projection facts
  for call arguments and match cases so emission participants consume the env
  row rather than rebuilding enum keys or emitted symbols locally.
- `text/expr_sequence_owner.pgy` owns top-level comma-separated expression
  sequence facts for array literals, call arguments, and struct literal field
  lists while expression payloads are still string-backed.
- `text/struct_literal_call_owner.pgy` owns struct literal call-envelope facts:
  `Name(...)` recognition plus the typed type-name/inner-payload fact row.
- `text/struct_literal_field_owner.pgy` owns the typed struct literal
  field-entry fact row, including positional field fallback from collected
  field rows.
- Statement-row facts for `Let`, `Assign`, `Log`, `Return`, `Defer`,
  `ArrayPop`, `ArraySet`, `ArrayPush`, `Exit`, `Break`, `Continue`, `For`,
  `While`, `If`, `Else`/`else if` routing, and bare call statements now live in
  the row-fact owner plus typed arena projection; the old statement-owner alias
  file is retired.

This does not close the mixed AST-like tree owner; it only prevents emission
participants from each recovering inventory or statement facts locally. The
current `program_emit.pgy`, declaration collectors, function signature
emission, and statement body emission consume typed nodes for program-level
declaration routing, `Main` counting, event rejection, owner skipping,
method/function dispatch, function header, parameter, return, body-marker, and
statement reads, global function environment construction, role-operator
discovery, struct/enum collection, and prototype emission. The legacy parallel
`indents`/`texts` projection has been removed; the remaining bridge debt is that
`CodegenAstTextNode.text` is still a line-text payload inside the input owner
rather than a tagged AST semantic record. Single-payload statements, `Let`,
`Assign`, `ArrayPush`, `ArraySet`, and `For` now read arena rows in emission.
Parameter mode is part of this bridge contract:
native and
self-host AST printers preserve `inout`,
`own`, and `ref`; the current codegen consumes `inout` via function-env `pm`
facts and lowers calls/signatures from that fact instead of guessing mutation
from `ArrayPush` or statement text. The
current `compiler/symbol_table_owner.pgy` and `abi_layout/abi_layout_owner.pgy`
owners are read-only: they centralize the self-host C subset's emitted symbol
and ABI type spelling without claiming full C/LLVM symbol or ABI row closure.
`runtime_abi/collection_runtime_owner.pgy`
is the read-only owner for self-host C collection runtime helper names;
it also normalizes the current AST-text bridge spellings
`Array<Int: Int>` / `Array<String: String>` /
`Array<CodegenAstTextNode: CodegenAstTextNode>` to canonical collection kind
facts, including the bootstrap-only typed AST-line record-array lane.
`runtime_abi/math_runtime_owner.pgy` is the read-only owner for supported
self-host C math/random helper names and C target-library spellings.
`runtime_abi/host_io_runtime_owner.pgy` is the read-only owner for supported
self-host C host file/argv/process helper names and target-library spellings.
`runtime_abi/option_result_runtime_owner.pgy` is the read-only owner for
supported self-host C Option/Result runtime helper names.
`runtime_abi/string_runtime_owner.pgy` is the read-only owner for supported
self-host C string/text helper names and conversion target-library spellings.
`program_emit.pgy` still owns the generated helper definitions.
Expression/statement emitters should not locally spell Pergyra `pgy_*` runtime
helper names or supported target-library call names such as `sqrt`, `atof`, or `exit`. Direct C
standard-library calls are target-library spelling facts, not expression-local
literals.

This is the projection-nerve rule in code form: the backend does not own a new
truth. It receives MIR/type/ABI facts from the compiler world and sends one
projection through `EmissionZone`. C, LLVM, and self-hosted codegen may have
different syntax emitters, but their input facts must be the same.

The current codegen rung may consume AST text because that is the declared
bridge input. It must not treat AST text as the final semantic source of truth.
New semantic decisions should enter through type facts, MIR facts, ABI facts, or
a declared unsupported diagnostic.

For future non-CPU targets, codegen must also consume target-capability facts:
accepted operations, required loss/quantization budget, buffer transfer shape,
host/device ownership, and fallback/materialization reason. These are facts, not
backend-local guesses.

### Codegen Resource Contract

The long-term codegen shape is resource-first:

| Resource | Zone/owner | Consumers | Gate expectation |
|---|---|---|---|
| emitted artifact text | `EmissionZone` / emission participants | C compiler, parity harness | one write owner; no scattered stdout construction |
| type bindings | `TypeEnvZone` / `type_facts/` | expression, statement, return, log routing | emitters consume type facts and parameter-mode rows instead of re-inferring from source text |
| symbol and mangle facts | `compiler/symbol_table_owner.pgy`; cross-backend owner still active beyond the self-host C consumer | C, LLVM, and self-hosted emission | emitters consume canonical spelling facts; no owner/member string concatenation in local emission |
| self-host C ABI type spelling | `compiler/abi_layout_row_owner.pgy` for supported concrete rows, consumed by `abi_layout/abi_layout_owner.pgy` for self-host C subset; cross-backend native row projection still active | self-hosted C emission | signature, local, and field declarations consume canonical C ABI rows before user-struct lookup |
| self-host typed AST-text bridge | `input/ast_text_inventory_owner.pgy` for parser AST-text lines, `CodegenAstTextNode`, indentation, coarse kind, marker predicates, blank filtering, `[export]` normalization, declaration payload accessors, and cursor expectations; `input/ast_text_typed_arena_owner.pgy` for parent/indent/child projection into `AstArena` plus `CodegenTypedAstBridgeReady`; `input/ast_text_row_fact_owner.pgy` for `CodegenAstTextRowFactInput` plus function/return/role/nominal/enum-name/field/parameter/statement name, type, value, aux-value, and mode rows; `input/ast_text_array_literal_owner.pgy` for transitional `Let` array literal shape, initializer fact, and top-level element facts; `input/ast_text_enum_variant_owner.pgy` for payload-free enum variant payload facts; `input/ast_text_try_let_owner.pgy` for `Let` try-initializer shape and inner-expression facts; `input/ast_text_function_signature_owner.pgy` for function name/parameter/return signature facts; `input/ast_text_declaration_owner.pgy` for nominal/role/enum/field declaration facts; `input/ast_text_local_binding_owner.pgy` for local binding name/type/initializer facts; `input/ast_text_assignment_owner.pgy` for assignment target/RHS facts; `input/ast_text_for_stmt_owner.pgy` for `For` loop-var/range/foreach facts; `input/ast_text_statement_payload_owner.pgy` for single-payload statement argument/condition facts; `input/ast_text_collection_stmt_owner.pgy` for `ArrayPush`/`ArraySet` statement payload facts; `input/ast_expression_usage_owner.pgy` for expression-part projection facts, expression usage facts, and builtin-callee group rows; `input/ast_kind_usage_owner.pgy` for statement-shape usage facts; `input/ast_type_usage_owner.pgy` for type-surface usage facts; `text/enum_literal_owner.pgy` for payload-free enum literal projection facts; `text/expr_sequence_owner.pgy` for top-level comma-separated expression sequence facts; `text/struct_literal_call_owner.pgy` for struct literal call-envelope facts; `text/struct_literal_field_owner.pgy` for typed struct literal field-entry fact rows; `typed_ast_node_skeleton.pgy` for the typed AST arena payload contract that will replace the bridge | self-hosted C emission today; parser/codegen cutover later | `program_emit` consumes typed arena kind/atom predicates for declaration routing, Main counting, event rejection, and top-level function selection, plus typed arena indent/descendant facts for owner-body traversal; declaration collectors also consume typed arena kind/atom predicates for env/prototype/struct/enum/role-operator prepasses; function signature emission consumes typed arena Parameters/Returns/Fields marker predicates, runtime/header usage facts consume `CodegenExpressionUsageFacts`, `CodegenKindUsageFacts`, and `CodegenTypeUsageFacts` rows, and statement body emission consumes typed arena statement-kind predicates plus Body/Block/Then marker expectations while remaining expression payload reads stay on the transitional bridge; `GenerateC` consumes `CodegenTypedAstBridgeReady(nodes, count)` before emission and validates row-aligned `AstArena` kind/atom/value/aux-value/parent/indent/child facts; function/declaration names, payload-free enum variant payload lists/literals, `Let` name/type/initializer, try-initializer, array-literal initializer, and array-literal element facts, call argument facts, struct literal field-list/envelope and field-entry facts, `Assign` target/RHS, `ArrayPush` target/value, `ArraySet` target/index/value, `For` loop-var/start/end/collection, and single-payload statement payloads consume input/text owner rows/facts; `inout` signatures/calls consume recorded `pm` facts; no emission participant may re-split AST text, consume parallel `indents`/`texts` arrays, route usage facts through raw node payload scans, or import the retired statement-owner alias |
| self-host C collection runtime symbols | `runtime_abi/collection_runtime_owner.pgy` for `Array<Int>` / `Array<String>` helper calls plus the `Array<CodegenAstTextNode>` bootstrap bridge | self-hosted C emission | expression/statement emitters consume canonical helper-name facts from collection kind-code facts; generated helper definitions stay in one definition host |
| self-host C math/random symbols | `runtime_abi/math_runtime_owner.pgy` for `Abs` / `Min` / `Max` / `Sqrt` / `Pow` / `Floor` / `Ceil` / `SeedRandom` / `Random` helper or target-library calls | self-hosted C emission | expression emitters consume canonical symbol facts; generated helper definitions stay in one definition host |
| self-host C host I/O/process symbols | `runtime_abi/host_io_runtime_owner.pgy` for file, directory-walk, `Args()`, and `Exit(Int)` helper or target-library calls | self-hosted C emission | expression/statement emitters consume canonical symbol facts; generated helper definitions stay in one definition host |
| self-host C Option/Result runtime symbols | `runtime_abi/option_result_runtime_owner.pgy` for `Option<Int>` / `Result<Int>` helper calls | self-hosted C emission | expression/statement emitters consume canonical helper-name facts; generated helper definitions stay in one definition host |
| self-host C string/text symbols | `runtime_abi/string_runtime_owner.pgy` for supported string/text builtin helper and conversion target-library calls | self-hosted C emission | expression/statement emitters consume canonical symbol facts; generated helper definitions stay in one definition host |
| ABI/layout facts | `AbiLayoutZone` over the MIR ABI/layout owner | C, LLVM, self-hosted codegen | no backend invents field order, niche, pointer, or ownership shape |
| unsupported surface | codegen diagnostic owner | parity harness | fail visibly, never emit broken C |
| target acceptance/fallback | `target_capability_owner.pgy` plus future target-specific extensions | C, LLVM, self-hosted, accelerator projections | no hidden CPU fallback or unsupported accelerator lowering |

The current `input/`, `run/`, `text/`, `type_facts/`, `abi_layout/`,
`runtime_abi/`, and `emission/` directories are an intermediate
resource split. `text/` exists because the current rung still consumes
parser AST text as a compatibility bridge. As MIR facts replace that bridge,
text scanning should shrink; it must not become a second parser or a place to
recover semantic truth.

`program_emit`, `function_emit`, `stmt_emit`, `expr_rewrite`, and
`struct_value_emit` remain action participants. They may split further only by
owned responsibility:

- a new type-fact owner is valid;
- a new ABI/layout owner such as `AbiLayoutZone` is valid;
- a new symbol/mangle row consumer is valid;
- a generic `emit_helpers.pgy` bucket is not valid;
- a fake `ExprZone` or `StmtZone` is not valid while both mutate the same
  emitted-output resource.

## Compiler Architecture

The self-hosted compiler should not be organized as `frontend/`, `middle/`,
`backend/` buckets that copy the C implementation. The compiler architecture is:

- root world: `src/self_hosted/compiler/world.pgy`;
- derived compiler actions: `src/self_hosted/compiler/stage_intents.pgy`;
- fact owners: `lexer/`, `parser/`, `semantic/`, `mir_lower/`, `codegen/`;
- stage artifact envelope owner:
  `src/self_hosted/compiler/stage_artifact_owner.pgy`;
- shared substrate owners: `lib/` and future collection/import/path owners;
- oracle and parity machinery: `tests/self_hosted/`.

That keeps the Pergyra implementation readable as a Pergyra compiler world:
intent owns the flow, zone owns resource isolation, and owner files own facts.

### Pergyra-Style Self-Host Test

A self-hosted slice is Pergyra-style only if it keeps the language's semantic
shape visible. Writing a compiler slice in `.pgy` is not enough.

The slice must pass these design checks:

1. The root flow is an intent or a named derived intent cluster, not a hidden
   import order in `main.pgy`.
2. A zone appears only for a distinct owned resource: source facts, token
   facts, AST/tree facts, semantic verdicts, MIR facts, type bindings, ABI
   layout, target capability, emitted artifacts, or parity evidence.
3. Implementation files such as expression, statement, function, or program
   emitters are action participants over resource zones. They are not fake
   zones merely because the code was split into files.
4. A semantic decision is consumed from one fact owner. If the fact is missing,
   the slice must add that fact to the owner or reject the input; it must not
   rediscover the answer from text, JSON, AST payloads, or a backend fallback.
5. C, LLVM, and self-hosted outputs are peer projections over the same facts.
   No backend is allowed to become a second semantic oracle for the same flow.
6. Parity evidence belongs in a proof/artifact owner. A passing `.pgy` tool is
   not a hard substitution until its diagnostics, facts, emitted artifacts where
   stable, and run behavior are compared against the C/LLVM oracle.

This is the practical answer to "is the self-host compiler Pergyra enough?":
the code should read as `PgyCompilerWorld` plus intent-driven resource
ownership, not as a C folder graph translated into Pergyra syntax.

`tests/self_host_pergyra_likeness_smoke.sh` now reports both sides of that
claim. The blocking negative smell metrics (`core_string_munge_sig`,
`ast_string_surface`, `sentinel`) must trend down as typed facts replace text
bridges. The broad `total_string_munge_sig` remains informational so
tools/LSP/fuzz/path/harness text domains stay visible without diluting the
compiler-core ratchet. `compiler_world_stub_actions` must stay at zero now that
stage actors consume owned facts instead of scaffold `true`.
`stage_envelope_only` is the next payload-depth ratchet: it counts stage
readiness functions that only prove path/world-binding envelopes. It started at
four; the lexer stage moved to a token payload contract owned by
`lexer/token_owner.pgy`, the parser stage
consumes the compact-AST text contract owned by `parser/tree_text_owner.pgy`,
and the semantic stage consumes the verdict contract owned by
`semantic/diagnostic_owner.pgy`. The MIR stage now consumes the MIR fact graph
contract owned by `mir_lower/mir_fact_graph_contract_owner.pgy`, so the current
baseline is zero.
`result_use` must not fall.
The positive topology metrics are different:
`compiler_world`, `resource_zones`, `intent_surface`, and `zone_bound_steps`
are floors, not scores. Adding fake zones or one-intent-per-helper files does
not make the compiler more Pergyra-like; the floor only prevents the root
world, resource ownership boundaries, and intent-bound zone steps from
disappearing while the bootstrap grows.

Treat this as a bootstrap-quality guard, not an aesthetic scoreboard. A higher
keyword count does not prove the compiler is eating itself in a Pergyra-shaped
way. The proof improves when a named owner produces the fact, the consumer
checks that fact, and C/LLVM/Pergyra parity proves the result. For example,
moving delimiter absence from integer sentinels to `Option<Int>` in the codegen
text owner is a Pergyra-likeness improvement even though it adds no new zone:
the ownership boundary becomes sharper, and the bootstrap consumes a typed fact
instead of hidden C-style control flow.

A low keyword density inside `codegen/` is not itself a failure. The failure
would be a codegen slice that hides resource ownership, re-parses facts locally,
or lets `main.pgy` become the flow owner. Pergyra-likeness is measured by the
visible `PgyCompilerWorld` topology and by fact owners such as
`ast_text_row_fact_owner.pgy`, `TypeEnvZone`, `AbiLayoutZone`, and
`EmissionZone` owning the decisions they consume.

The weakest current signal is not missing `world` or `zone` syntax; it is
load-bearing depth. Source intake now consumes the path-manifest owner through
`CompilerStagePathManifestReady()`, and parity comparison consumes artifact and
test-harness facts. Emission consumes ABI-layout, symbol, and
target-capability facts. Lexing, parsing, semantic checking, and MIR lowering
now consume `stage_artifact_owner.pgy` facts, so `world.pgy` has no scaffold
`return true` actor actions left. Lexer readiness also consumes
`LexerTokenPayloadContractReady()` from `lexer/token_owner.pgy`, which ties the
token stage to its token-stream schema, fixture count, keyword classification
quirks, and token-line payload formatting. Parser readiness consumes
`ParserAstTreePayloadContractReady()` from `parser/tree_text_owner.pgy`, tying
the parser stage to the current compact-AST text schema, committed fixture
count, and root `Program:` / implicit-`Main` output shape. Semantic readiness
consumes `SemanticVerdictPayloadContractReady()` from
`semantic/diagnostic_owner.pgy`, tying the stage to the verdict schema,
108-fixture parity surface, ok/error status rendering, and 17-code vocabulary.
MIR readiness consumes `MirFactGraphPayloadContractReady()` from
`mir_lower/mir_fact_graph_contract_owner.pgy`, tying the stage to the MIR JSON
schema, 85-fixture parity surface, declaration/routine arrays, source-local
arrays, and instruction source facts. The `stage_envelope_only` ratchet is now
closed at zero; future work should deepen each payload contract rather than
reintroducing envelope-only readiness.

### Current-To-Target Mapping

| Current surface | Target owner shape | Migration rule |
|---|---|---|
| stage `main.pgy` imports every sibling in order | each owner imports the fact owners it consumes | entrypoints stop being dependency aggregators |
| stage actor returns scaffold `true` | `stage_artifact_owner.pgy` readiness fact | actors consume path/world-binding envelopes before claiming readiness |
| AST text read by codegen | MIR/type/ABI facts consumed by codegen | AST text remains a declared bridge until its facts exist |
| shell scripts rediscovering files | `StagePathManifest` plus path/world-binding owner projection | stage paths normalize once and are passed with resource zone, actor, and intent facts |
| raw diagnostic strings in tools | shared diagnostic owner and stage diagnostic vocabulary | diagnostics are structured before parity compares them |
| recursive filesystem discovery for closed stage sets | manifest-owned direct paths | discovery is allowed only when the test is measuring discovery drift |
| C/LLVM/backend-specific layout guesses | `AbiLayoutZone` over ABI/layout facts | backend emitters consume one layout fact source |

This mapping is the self-hosted architecture work queue. A slice does not count
as hard substitution if it merely moves logic into Pergyra while preserving a
hidden C-style alias or fallback path.

### Required Compiler Substrates

The compiler needs these architectural substrates before full hard
self-hosting can close:

1. Source intake: path manifest, import graph, duplicate import materialization
   policy, and source hash identity.
2. Frontend facts: token stream and AST/tree facts with stable ordering.
3. Semantic facts: type environment, diagnostic vocabulary, and fail-closed
   unsupported-surface verdicts.
4. Middle-end facts: MIR JSON/fact graph, CFG/body facts, cleanup/defer facts,
   and authority/effect evidence.
5. Backend facts: ABI/layout rows, symbol/mangle rows, emitted-artifact owner,
   runtime materialization policy, target acceptance/fallback facts.
6. Proof facts: C/LLVM/Pergyra parity verdicts, run-output equality, diagnostic
   equality, AIR JSON equality, MIR JSON equality, and layout equality.

The rule is the same for every substrate: if the fact is required and missing,
add it to the owner or reject the program. Do not locally reconstruct it from an
older artifact.

## Anti-Patterns

These shapes are rejected for the self-hosted architecture:

- a stage `main.pgy` that becomes a hidden import-order owner;
- one folder per implementation detail when no resource is owned;
- fake zones around recursive functions that mutate the same resource;
- generic `_helpers` modules that do not name a fact owner;
- raw JSON/text parsing to reconstruct a semantic fact already owned by MIR,
  DAG, ABI, or a stage-specific owner;
- backend fallbacks that silently choose C-like behavior when C and LLVM facts
  disagree;
- parity gates that compare only process success when the owned artifact is a
  diagnostic, IR, ABI shape, or emitted output.

## Caching Shape

Caching is allowed only behind stable fact owners.

Good cache keys:

- normalized `StagePathManifest` entries;
- import graph node identity plus source content hash;
- token stream schema plus source hash;
- AST/MIR JSON schema plus stable ordering;
- type environment version plus function/declaration identity;
- ABI layout fact version plus target ABI policy;
- emitted artifact schema plus backend target and ABI/layout fact version.

Bad cache keys:

- filesystem scan order;
- raw pointer identity;
- current process path spelling;
- backend-specific emitted text when the owned fact is a MIR or ABI row;
- source text snippets used to recover semantic facts that should already be in
  MIR/DAG/ABI metadata.

The first optimization target is path/import caching: resolve stage paths once,
normalize once, and pass path facts to source intake. Repeated recursive scans
belong in tests only when the test is explicitly measuring discovery drift.

## Runtime And Materialization

The compiler must distinguish erased facts from explicit materialized runtime
boundaries.

- Hot static paths should consume evidence and lower directly.
- External IO, FFI/raw escape, dynamic capability checks, and open-world
  boundaries may materialize runtime state.
- Materialization must be visible through an owner fact, effect, capability, or
  runtime-frontier policy. It must not be a backend surprise.

This is not a zero-runtime requirement. It is a no-hidden-runtime requirement.

## Promotion Rule

A self-hosted slice is promoted only when:

1. the owner file names the fact it owns;
2. `main.pgy` stays an entrypoint;
3. unsupported input fails visibly;
4. C and LLVM oracle comparison is defined for the owned artifact;
5. the focused parity gate is green;
6. the preparation contract gate knows the owner shape;
7. no semantic fact is reconstructed from an older representation when the
   owning IR/fact should carry it.

SoT closure is therefore not a separate cleanup after self-hosting. It is the
condition that lets a self-hosted slice count.
