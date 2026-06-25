# Compiler World

This directory owns the self-hosted compiler world. It is not a C-side driver
mirror, and it is not a bucket for stage harnesses.

The companion architecture documents are
[`docs/self_hosted/11_compiler_world_architecture.md`](../../../docs/self_hosted/11_compiler_world_architecture.md),
[`docs/self_hosted/12_intent_zone_self_host_architecture.md`](../../../docs/self_hosted/12_intent_zone_self_host_architecture.md),
and
[`docs/self_hosted/13_compiler_substrate_architecture.md`](../../../docs/self_hosted/13_compiler_substrate_architecture.md).
`13_compiler_substrate_architecture.md` is the concrete architecture-stack and
substrate contract for codegen resources, stage fact owners, import graph
ownership, deterministic facts, runtime materialization, caching, and parity
promotion.

Pergyra self-hosting should be organized around the language's own surface.
`PgyCompilerWorld` is the owner, and `CompilePergyraProgram` is the root
compiler intent. The source unit flows through derived resource zones
(`SourceIntakeZone`, `TokenStreamZone`, `AstTreeZone`,
`SemanticVerdictZone`, `MirFactGraphZone`, `TypeEnvZone`, `AbiLayoutZone`,
`EmissionZone`, and `ParityZone`), and each zone is driven by a smaller intent. The stage actors
are named by what they own: `LexerStage`, `ParserStage`, `SemanticStage`, and
`MirLowerStage` drive token, AST, semantic verdict, and MIR fact resources
respectively. There is no generic `StageOwner` alias in the compiler world.
The stage owners remain under `lexer/`, `parser/`,
`semantic/`, `mir_lower/`, and `codegen/`; this directory records the
orchestration contract that makes them one compiler slice instead of C-style
fragments.

Zone is a resource ownership boundary, not a folder/module label. The test is:
does this boundary own a distinct resource that other participants must access
through a fact, view, or intent boundary? A codegen file like `stmt_emit.pgy` is
a participant in the emission action graph; it is not a zone merely because it
is a file. A zone appears when there is a distinct owned resource such as token
facts, AST facts, semantic verdicts, MIR facts, type bindings, an emitted-C
buffer, ABI layout facts, or parity evidence.
`EmissionZone` owns the emitted-C buffer; `ProgramEmitter` is the participant
that drives writes into that buffer.

`stage_intents.pgy` owns derived intent clusters such as `FrontendPipeline`,
`MiddleEndPipeline`, `BackendPipeline`, and `SelfProofPipeline`. These clusters
compose existing resource zones. They are not new zones and must not hide stage
facts or parity policy.

The compiler world also owns `StagePathManifest`, the canonical path fact for
self-host source roots, test roots, parity harness roots, and active stage
entrypoints. `path_manifest_owner.pgy` owns the current string values for those
paths, while `tests/self_hosted/compiler_world_manifest.sh` is the shell-side
projection used by the gates. That gives future hard-substitution code a way
to consume paths as facts instead of rediscovering them with recursive scans.

`world.pgy` is the current scaffold. It is parse-gated by
`make self-host-compiler-world-contract-test-smoke` and wired into
`make self-host-preparation-test-smoke`. That gate also enforces
**manifest-to-reality conformance**: every stage `StagePathManifest` names must
own a real `src/self_hosted/<stage>/` directory with `.pgy` facts, and every
on-disk stage (a dir with `main.pgy`) must be named by the world so the
architecture manifest cannot silently drift from the stage owners. It does not
claim that the released compiler is self-hosted; it fixes the shape that hard
substitution must grow into.

## Growth Rule

`world.pgy` stays under the same 600-line cap as other self-hosted owner files.
There is no compiler-world exception: if the root world grows, the extra work is
in the wrong owner.

The split unit is a resource-owned intent cluster, not one file per tiny intent.
Keep the root file to:

- shared compiler-world subjects/objects;
- `StagePathManifest`;
- resource zones and `PgyCompilerWorld`;
- the top-level `CompilePergyraProgram` flow.

Keep derived intent clusters in `stage_intents.pgy` until a cluster owns enough
detail to justify a more specific owner. Do not split one file per tiny intent.

When a stage needs more detail, move that detail into a named stage owner such
as a source-intake, frontend, middle-end, backend, or parity cluster. The root
world may name the cluster, but it must not absorb the stage's facts or hidden
fallback policy.
