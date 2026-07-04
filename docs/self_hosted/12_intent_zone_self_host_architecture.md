# Intent/Zone Self-Host Architecture

Status: `self-host-architecture-shape`

This document records how the self-hosted compiler should use Pergyra's own
language model. The target is not "C compiler folders rewritten in Pergyra".
The target is a compiler world whose visible flow is intent-owned and whose
state is held by resource zones.

## Core Rule

`intent` is the compiler flow owner. `zone` is a resource ownership boundary.

Do not introduce a zone because a file, folder, pass, or helper family exists.
Introduce a zone only when there is a distinct resource that other participants
must access through a fact, view, or intent boundary.

That gives the self-host compiler three layers:

1. `PgyCompilerWorld` names the whole compiler world.
2. Resource zones own compiler state: source intake, token stream, AST tree,
   semantic verdict, MIR fact graph, type facts, ABI layout facts, emitted
   artifact, target capability envelope, and parity evidence.
3. Intent clusters compose those zones into compiler actions.

## Compiler World Shape

`src/self_hosted/compiler/world.pgy` owns the root world topology:

- subjects and objects that define compiler participants and facts;
- `StagePathManifest`;
- resource zones;
- `PgyCompilerWorld`;
- the root `CompilePergyraProgram` intent.

`src/self_hosted/compiler/stage_intents.pgy` owns derived compiler intent
clusters:

- `FrontendPipeline`: source intake -> lexer -> parser.
- `MiddleEndPipeline`: semantic verdict -> MIR fact graph.
- `PlanTargetProjection`: target acceptance/fallback facts before backend
  emission.
- `BackendPipeline`: type facts + ABI layout facts + target capability envelope
  -> emitted C artifact.
- `SelfProofPipeline`: C/LLVM/Pergyra oracle comparison evidence.

These are intent clusters, not zones. They reuse `SourceIntakeZone`,
`TokenStreamZone`, `AstTreeZone`, `SemanticVerdictZone`, `MirFactGraphZone`,
`TypeEnvZone`, `AbiLayoutZone`, `EmissionZone`, and `ParityZone`.
`TargetCapabilityZone` sits between ABI layout facts and emission so backend
projections cannot silently accept, reject, or fall back without an owned
reason.
The participants are also stage-specific: `LexerStage` scans token facts,
`ParserStage` builds AST facts, `SemanticStage` proves semantic verdict facts,
and `MirLowerStage` lowers MIR facts. Do not collapse those actors back into a
generic `StageOwner`; that hides the artifact each stage owns.

## Codegen Shape

Self-hosted codegen is a backend resource cluster:

- `TypeEnvZone` owns type binding facts.
- `AbiLayoutZone` owns ABI/layout facts such as field order, representation
  kind, tag/niche policy, and payload offsets.
- `TargetCapabilityZone` owns the projection envelope: accepted target facts,
  loss/quantization budget, materialization reason, and fallback reason.
- `EmissionZone` currently owns the emitted C text buffer.
- `ProgramEmitter` is the participant that writes through `EmissionZone`.
- `program_emit`, `function_emit`, `stmt_emit`, `expr_rewrite`, and
  `struct_value_emit` are action participants over those resources.

The mental model is a projection nerve bundle. `PgyCompilerWorld` is the
visible compiler body; codegen is where MIR/type/ABI facts leave that body as
backend projections. The current bundle has one C-emission resource owner. The
target bundle splits that owner into C, LLVM, and SelfHosted emission zones
only when each projection consumes the same fact envelope and produces a
comparable artifact. The bundle is grouped by resource zones, not by syntactic
helper categories. `EmissionZone`, `TypeEnvZone`, and `AbiLayoutZone` are
zones; `stmt_emit` and `expr_rewrite` are nerves inside the emission action.

The filesystem split under `src/self_hosted/codegen/` follows owner visibility:

- `input/`: AST path and read boundary.
- `run/`: CLI-to-output orchestration.
- `text/`: AST/expression text scanning.
- `type_facts/`: type evidence.
- `abi_layout/`: self-host C ABI type spelling facts now; broader ABI/layout
  fact projection from MIR ABI rows remains the cross-backend target.
- `emission/`: current C emission participants.

That split is not a semantic claim that every folder is a zone.

## Path And Source Intake

Path facts must not be rediscovered by every stage. The current shared owner is
`src/self_hosted/lib/path.pgy`:

- `SelfHostPath.Dirname`;
- `SelfHostPath.IsAbsolute`;
- `SelfHostPath.Join`;
- `SelfHostPath.NormalizeImportRelative`.

Runtime file-access authorization still belongs to the native runtime path
resolver. `SelfHostPath` owns only self-hosted source/import path string facts.

`StagePathManifest` is the compiler-world fact that should eventually feed
stage source paths and parity paths without recursive discovery scans.

## Growth Rule

When self-hosting grows, add detail in this order:

1. Add or extend a resource zone only if a new owned resource appears.
2. Add an intent cluster when a meaningful compiler action composes existing
   resource zones.
3. Keep stage implementation facts in the stage owner directory.
4. Keep each active stage's `intent.md` bound to its compiler-world row:
   stage name, resource zone, stage actor, and stage intent.
5. Keep oracle comparison and shell bridge mechanics under `tests/self_hosted/`.

Do not add:

- fake zones for folders;
- one intent per tiny helper;
- hidden fallback paths that bypass IR facts;
- generic helper buckets that only rename local logic.

## Verification

The architecture is executable policy:

- `make self-host-compiler-world-contract-test-smoke` checks the compiler world
  shape, imported stage intent clusters, and docs/index wiring.
- `make self-host-preparation-contract-test-smoke` includes that shape gate.
- `make self-host-preparation-parity-test-smoke` remains the heavier
  C/LLVM/Pergyra evidence path.

This document is a shape contract, not a release claim that the compiler is
already fully self-hosted.
