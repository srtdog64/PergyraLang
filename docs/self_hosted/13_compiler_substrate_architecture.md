# Self-Hosted Compiler Substrate Architecture

Status: `self-host-substrate-architecture-contract`

This document connects the compiler world shape to the concrete substrate a
Pergyra-written compiler needs. It sits below
`11_compiler_world_architecture.md` and
`12_intent_zone_self_host_architecture.md`.

It is not a release claim that the compiler is already self-hosted. It is the
architecture contract for how hard self-hosted slices should be added without
recreating the C compiler's folder graph.

## Core Rule

The self-hosted compiler has one visible compiler flow and many fact owners.

- `PgyCompilerWorld` owns the compiler action.
- Stage directories own stage facts.
- Resource zones own isolated compiler resources.
- Intent clusters compose resources into compiler actions.
- Parity gates prove that a Pergyra implementation can replace one real slice.

The unit of architecture is not "a folder with code." The unit is an owned
fact, resource, or artifact boundary.

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
7. `EmissionZone`: emitted artifact buffer.
8. `ParityZone`: C/LLVM/Pergyra comparison evidence.

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
| type environment | `TypeEnvZone` and stage type-fact owners | prevents backend emitters from re-inferring source types |
| MIR fact graph | `MirFactGraphZone` | gives backend and self-host lowering one fact source |
| ABI/layout facts | MIR ABI/layout owner | prevents C/LLVM/self-hosted emitters from inventing layout independently |
| emission buffer | `EmissionZone` | gives output writes one owner |
| parity evidence | `ParityZone` and test harnesses | proves substitution against the C/LLVM oracle pair |
| runtime materialization policy | runtime/frontier owner | distinguishes erased hot paths from explicit managed boundaries |

If one of these facts is missing, the fix is to add the fact to the owner or
fail closed. The fix is not a local compatibility fallback.

## Codegen Architecture

Self-hosted codegen is a backend resource cluster:

- `TypeEnvZone` owns type binding facts.
- `EmissionZone` owns emitted C.
- `ProgramEmitter` is the participant that writes through `EmissionZone`.
- `input/` owns AST path and read boundaries while the rung still consumes
  `pgy --ast`.
- `run/` owns CLI-to-output orchestration.
- `text/` owns text and expression scanning facts for the compatibility bridge.
- `type_facts/` owns the type environment consumed by emitters.
- `emission/` contains participants that write or route emitted C.

`program_emit`, `function_emit`, `stmt_emit`, `expr_rewrite`, and
`struct_value_emit` are not zones. They are action participants over the same
output and type resources. A new zone appears only when there is a new distinct
resource, such as a mutable symbol/name-mangling table.

The current codegen rung may consume AST text because that is the declared
bridge input. It must not treat AST text as the final semantic source of truth.
New semantic decisions should enter through type facts, MIR facts, ABI facts, or
a declared unsupported diagnostic.

## Compiler Architecture

The self-hosted compiler should not be organized as `frontend/`, `middle/`,
`backend/` buckets that copy the C implementation. The compiler architecture is:

- root world: `src/self_hosted/compiler/world.pgy`;
- derived compiler actions: `src/self_hosted/compiler/stage_intents.pgy`;
- fact owners: `lexer/`, `parser/`, `semantic/`, `mir_lower/`, `codegen/`;
- shared substrate owners: `lib/` and future collection/import/path owners;
- oracle and parity machinery: `tests/self_hosted/`.

That keeps the Pergyra implementation readable as a Pergyra compiler world:
intent owns the flow, zone owns resource isolation, and owner files own facts.

## Caching Shape

Caching is allowed only behind stable fact owners.

Good cache keys:

- normalized `StagePathManifest` entries;
- import graph node identity plus source content hash;
- token stream schema plus source hash;
- AST/MIR JSON schema plus stable ordering;
- type environment version plus function/declaration identity;
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

