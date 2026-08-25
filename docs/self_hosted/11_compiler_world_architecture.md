# Self-Hosted Compiler World Architecture

Status: `hard-self-host-shape-contract / source-MIR/source-C REACHABLE BRIDGE`

The self-hosted compiler must not become a Pergyra rewrite of the C folder
layout. Pergyra's language surface is intent-first and world/zone-oriented, so
the target hard self-host shape is rooted in `PgyCompilerWorld`.

The bounded direct-MIR, source-to-MIR, and source-to-C slices are now part of the production
call graph. The installed self-driver and the full bootstrap artifact root
share the same source-to-MIR world/action owner; source folders remain fact
boundaries, not competing program graphs. The reachable paths are:

```text
driver_bootstrap_main.Main -> DriverRung2ExecuteInstalledRequest -> DriverRung2InstalledPublishDirectMir -> EmitDirectMirThroughPgyCompilerWorld
    -> PgyCompilerWorld.EmitDirectMir
    -> PgyCompilerWorld.direct_mir
    -> DriverRung2DirectMirZone.execution
    -> DriverRung2Execution.EmitDirectMir

driver_bootstrap_main.Main -> DriverRung2ExecuteInstalledRequest -> DriverRung2InstalledPublishSourceMir -> PublishSourceMirArtifactThroughPgyCompilerWorld
    -> PgyCompilerWorld.PublishSourceMirArtifact
    -> PgyCompilerWorld.source_mir
    -> DriverSourceMirZone.execution
    -> DriverSourceMirExecution.PublishSourceMirArtifact

driver_bootstrap_main.Main -> DriverRung2ExecuteInstalledRequest -> DriverRung2InstalledPublishSourceC -> PublishSourceCArtifactThroughPgyCompilerWorld
    -> PgyCompilerWorld.PublishSourceCArtifact
    -> PgyCompilerWorld.source_c
    -> DriverSourceCZone.execution
    -> DriverSourceCExecution.PublishSourceCArtifact

bin/pgy --self-driver -> native sibling launcher -> bin/pgy-self-driver
    -> driver_rung2_main.Main -> RunDriverRung2FromArgs
    -> ProduceSourceMirThroughPgyCompilerWorld
    -> PgyCompilerWorld.ProduceSourceMir
    -> PgyCompilerWorld.source_mir
    -> DriverSourceMirZone.execution
    -> DriverSourceMirExecution.ProduceSourceMir
```

`bin/pgy.exe src/self_hosted/compiler/world.pgy --emit-c` is also green with
zero errors and zero warnings. This proves that the declared world is a valid
compilation unit; it does not mean that `CompilePergyraProgram` is the
production root or that a C-owned compiler path has been replaced. The exact
SURFACE/REACHABLE/SUBSTITUTING distinction and takeover order remain owned by
`17_pergyra_native_dogfood_contract.md`.

## Rule

`PgyCompilerWorld` is the self-host compiler orchestration owner. Stage
directories own facts; resource zones own isolated compiler resources. The
production direct-MIR, source-to-MIR, and source-to-C entrypoints now reach three bounded
world slices after their old direct subject/file-helper calls were deleted.
MIR-to-C modes still bypass the declared root intent through direct installed
compile/commit calls. Root-intent takeover follows when a real compiler
purpose binder and its cross-axis fact bundle reach the production call graph,
delete the direct bypass, and earn the same replacement evidence. The number
of actions is not the takeover criterion.

- `src/self_hosted/compiler/world.pgy` names the hard-substitution world.
- The import closure declares exactly 21 concrete resource-zone types, while
  `PgyCompilerWorld` contains only the production-reachable `direct_mir` and
  `source_mir`, and `source_c` members. A declared target zone becomes a world member only in
  the rung that deletes its old production bypass.
- `DriverRung2DirectMirZone` owns only the execution subject's authority and
  lifetime boundary. Target selection, MIR, backend projection, artifact
  validation, and output writing remain with their existing owners and the
  action reached through that zone.
- `DriverSourceMirZone` owns the source-to-MIR execution subject's authority
  and lifetime boundary. Its action admits the pressure mode, consumes exactly
  one existing typed source-to-MIR producer. The same subject exposes an
  `io_read` payload action and an `io_read, io_write` artifact action, so the
  installed stdout CLI does not inherit write authority. The artifact action
  rejects an empty output path before compilation and commits once through the
  shared artifact transaction. Empty-path stdout sentinels and temp-file round
  trips are forbidden.
- `DriverSourceCZone` owns only source-to-C execution authority and lifetime.
  Its action validates subject/topology and artifact identity, consumes the
  existing `CompileSourceToCVerified` and shared transaction exactly once, and
  preserves executed, request-rejected, and artifact-rejected outcomes. It does
  not own source, semantic, MIR, target, or C-emission facts.
- `compiler_world_direct_mir_owner.pgy` is the sole composition owner. It
  materializes all three active zones in positional order and delegates through
  `PgyCompilerWorld`; it may not declare another world.
- `CompilePergyraProgram` is the root compiler intent.
- No aggregate compiler zone mirrors those resources. The former
  `SelfHostCompiler` aggregate duplicated every stage actor and artifact type
  already owned by the resource zones, while its root-intent parameter was
  unused; it is forbidden by the compiler-world gate.
- `ProgramEmitter` is the current C-emission participant that drives writes
  into `EmissionZone`; it is not a resource zone.
- `DriverRung2DirectMirZone`, `DriverSourceMirZone`, `DriverSourceCZone`, `SourceIntakeZone`, `TokenStreamZone`,
  `AstTreeZone`, `SemanticVerdictZone`, `MirFactGraphZone`, `TypeEnvZone`,
  `AbiLayoutZone`, `TargetCapabilityZone`, `CompatibilityEvolutionZone`,
  `AirEvidenceZone`, `SandboxCapabilityZone`, `SymbolFactTableZone`,
  `AbiRowProjectionZone`, `EmissionZone`, `ArtifactZone`, `TestHarnessZone`,
  `SubprocessRunnerZone`, and `ParityZone` are the 21 concrete resource zones.
- `StagePathManifest` is the canonical path fact for the self-hosted source,
  test, parity, and stage entrypoint locations.
- `path_manifest_owner.pgy` owns the current Pergyra string values for those
  paths; `tests/self_hosted/compiler_world_manifest.sh` is the shell projection
  used by the gates. The compiler-world contract compiles the Pergyra
  TestHarness manifest, asks the path owner for `compiler-world-paths`, and
  compares that set against the shell projection.
- `IntakeSource`, `LexSource`, `ParseTokens`, `CheckProgramSemantics`,
  `LowerProgramFacts`, `PlanTargetProjection`, `EmitProgramArtifact`, and
  `ProveSelfHostedParity` are the derived stage intents.
- Codegen is the projection nerve bundle from the compiler world into backend
  artifacts. The production direct-MIR action admits both C and LLVM requests
  through the same `DriverRung2DirectMirZone` and the same target/MIR/artifact
  facts; it does not create backend-specific worlds or execution zones. The
  older root-intent surface still represents emission with `EmissionZone` and
  `ProgramEmitter`. Any future resource split must prove a distinct owned
  resource rather than mirror emitter files or backend folders.
- `LexerStage`, `ParserStage`, `SemanticStage`, and `MirLowerStage` are
  distinct actors. The world does not use a generic `StageOwner.Consume()`
  alias because lexing, parsing, semantic checking, and MIR lowering own
  different artifacts.
- `lexer/`, `parser/`, `semantic/`, `mir_lower/`, and `codegen/` remain
  source-of-truth owners for their facts.

This is not a claim that the released compiler is self-hosted or that the
current bootstrap executes the whole root intent. The bootstrap executes the
`direct_mir`, `source_mir`, and `source_c` world members and their real actions; the remaining zone types and
intents are target topology, not dormant members filled with zero values. New
hard-substitution work must extend that one graph and remove the corresponding
direct bypass rather than grow a second C-style tree.

## Recursive Topology Rule

The compiler is self-similar by contract, not by a numeric line-count ratio.
Every scale repeats the same directed shape:

```text
resource zone -> intent transition -> typed owner fact -> final consumer
              -> verifier -> missing/corrupt-fact negative gate
```

The root world composes resource zones through the four named stage-intent
clusters. A stage cluster composes owner-directed actions. A leaf owner
projects one stable fact to its last legitimate consumer, where a negative
gate prevents source text, AST text, JSON text, or backend-local recovery.
Adding nested `world`, `zone`, or `intent` syntax without a new owned resource
does not increase Pergyra-likeness.

The language-wide carriage decision remains positional by default. Compiler
zones therefore stay explicit parameters at the orchestration boundary;
packing them into a value-typed stage bundle would hide the boundary and reopen
the value-carriage decision. Positional construction is exact-arity: it may
not treat omitted world members or aggregate zero-fill as authority facts.
Only a separately approved zone-bound handle may make that transition. The
balance target is one owner and one visible path, not the literal golden ratio
and not a shorter signature at any semantic cost.

## Target Resource Facade

The long-term readable projection of the compiler world is fixed as follows:

```text
PgyCompilerWorld
+-- FrontendResources
|   +-- Intake
|   +-- Token
|   `-- AST
+-- MiddleEndResources
|   +-- Semantic
|   `-- MIR
+-- EvidenceResources
|   +-- AIR
|   +-- Compatibility
|   `-- ABI
`-- BackendResources
    +-- Target
    +-- Emission
    +-- Artifact
    +-- DirectMIR
    +-- SourceMIR
    `-- SourceC
```

This is a target facade projection, not a claim that four new aggregate zones
have landed. It gives humans and tools a stable coarse topology while the
concrete resource zones remain the only ownership authorities. A facade leaf
may project more than one concrete zone, but it may not copy facts, infer a
missing fact, or become a second authority.

| Facade id | Current concrete owners |
| --- | --- |
| `FrontendResources.Intake` | `SourceIntakeZone` |
| `FrontendResources.Token` | `TokenStreamZone` |
| `FrontendResources.AST` | `AstTreeZone` |
| `MiddleEndResources.Semantic` | `SemanticVerdictZone`, `TypeEnvZone` |
| `MiddleEndResources.MIR` | `MirFactGraphZone` |
| `EvidenceResources.AIR` | `AirEvidenceZone` |
| `EvidenceResources.Compatibility` | `CompatibilityEvolutionZone` |
| `EvidenceResources.ABI` | `AbiLayoutZone`, `AbiRowProjectionZone` |
| `BackendResources.Target` | `TargetCapabilityZone`, `SandboxCapabilityZone` |
| `BackendResources.Emission` | `SymbolFactTableZone`, `EmissionZone` |
| `BackendResources.Artifact` | `ArtifactZone`, `TestHarnessZone`, `SubprocessRunnerZone`, `ParityZone` |
| `BackendResources.DirectMIR` | `DriverRung2DirectMirZone` |
| `BackendResources.SourceMIR` | `DriverSourceMirZone` |
| `BackendResources.SourceC` | `DriverSourceCZone` |

The facade is allowed to become visible in `world.pgy` only after all of the
following are true:

1. A typed zone-bound handle or equivalent non-owning resource view has an
   approved language contract.
2. Every current positional consumer has migrated to that stable handle
   identity without a dual-read path.
3. The handle preserves access to each concrete zone and cannot manufacture
   authority for its siblings.
4. Missing child facts and stale facade membership fail closed in a negative
   gate.
5. The old positional spelling is deleted in the same closure rung after
   C/LLVM/self-hosted parity passes.

Until then, the target facade belongs in documentation and generated
inspection output only. It must not be emitted as a nested value bundle, used
to shorten intent signatures, or counted as hard self-host substitution. This
keeps the future shape visible without reintroducing the deleted aggregate
owner.

## Zone Rule

A zone is a resource ownership boundary, not a module, folder, phase, or helper
bucket. The one-line test is:

> Does this boundary own a distinct resource that other participants must access
> through a fact/view/intent boundary?

If yes, it can be a zone. If it is only a function family that recursively calls
other functions over the same resource, it is a participant inside the existing
zone.

For compiler self-hosting that means:

- `TokenStreamZone` owns token-stream facts.
- `AstTreeZone` owns AST-tree facts.
- `SemanticVerdictZone` owns semantic verdict facts.
- `MirFactGraphZone` owns MIR fact graph state.
- `TypeEnvZone` owns type binding facts.
- `AbiLayoutZone` owns ABI/layout facts consumed by backend emitters.
- `TargetCapabilityZone` owns target acceptance, loss/fallback, and
  materialization-reason facts consumed before backend emission.
- `DriverRung2DirectMirZone` owns the production direct-MIR execution
  subject's authority and lifetime boundary. Its action consumes the existing
  target projection and emitted-artifact facts and owns the final output
  transition; the zone does not copy those facts or select a backend itself.
- `DriverSourceMirZone` owns the production source-to-MIR execution subject's
  authority and lifetime. Its shared admission owns pressure/identity checks;
  a read-only payload action and write-authorized artifact action own their
  distinct publication transitions while existing typed owners retain
  source/MIR semantics.
- `DriverSourceCZone` owns the production source-to-C execution subject's
  authority and lifetime. It admits one existing C compiler artifact and one
  typed publication transition without copying semantic or emission facts.
- `CompatibilityEvolutionZone` owns source, ABI/binary, behavior, diagnostic,
  AIR evidence, MIR JSON, runtime trace, capability profile, stdlib module, and
  obsolete-migration metadata facts.
- `EmissionZone` currently owns the emitted C text buffer and admits writes
  through the `ProgramEmitter` participant. It is a current-state owner, not
  the final claim that C, LLVM, and SelfHosted emissions have already been
  separated as peer projection zones.
- `ParityZone` owns C/LLVM/Pergyra comparison evidence.

`program_emit`, `function_emit`, `stmt_emit`, `expr_rewrite`, and
`struct_value_emit` are not zones by themselves. They are participants in the
codegen action graph. They may live in separate files for review size, but they
do not become zones unless they own a distinct resource.

For current codegen this is the concrete split:

- `DriverRung2DirectMirZone`: `subject slot execution:
  DriverRung2Execution` with `authority execution`; C and LLVM requests enter
  the same action through this member of `PgyCompilerWorld`.
- `DriverSourceMirZone`: `subject slot execution:
  DriverSourceMirExecution` with `authority execution`; verified and
  pressure-observed requests enter one admission owner through two
  capability-separated actions, and only the full driver source may use the
  pressure-observed request.
- `DriverSourceCZone`: `subject slot execution:
  DriverSourceCExecution` with `authority execution`; source/output identity
  enters one write-authorized action and its typed outcome reaches installed
  CLI without a direct compile/commit retry.
- `EmissionZone`: `object slot c_output: EmittedC`, driven by
  `subject slot emitter: ProgramEmitter`.
- `TypeEnvZone`: `object slot bindings: TypeEnvironment`, consumed as
  read-mostly type evidence.
- `AbiLayoutZone`: `object slot layouts: AbiLayoutFacts`, consumed as
  read-only ABI/layout evidence.
- `TargetCapabilityZone`: `object slot envelope: TargetCapabilityEnvelope`,
  driven by `subject slot planner: TargetProjectionPlanner`.
- `CompatibilityEvolutionZone`: `object slot facts:
  CompatibilityEvolutionFacts`, driven by `subject slot owner:
  CompatibilityEvolutionOwner`.
- Future symbol/name-mangling state may become a separate zone only if it owns
  mutable symbol facts. A new emitter file is not enough.
- Future `CEmissionZone`, `LLVMEmissionZone`, and `SelfHostedEmissionZone`
  become real zones only when they each own a comparable artifact resource and
  consume the same `TypeEnvZone`, `AbiLayoutZone`, `TargetCapabilityZone`,
  `CompatibilityEvolutionZone`, symbol rows, and MIR/AIR evidence facts. Until then, the target split remains
  an architecture direction, not a status claim.

That makes codegen a backend resource cluster, not a folder taxonomy. The
cluster has output state, type facts, and ABI layout facts; `program_emit`,
`function_emit`, `stmt_emit`, `expr_rewrite`, and `struct_value_emit` are
actions over those resources. Splitting those actions into files can keep
review size under control, but the split is not a semantic zone split.

## Why This Exists

The current C compiler is allowed to remain fragmented because it is the oracle.
The self-hosted compiler is not allowed to inherit that fragmentation as its
architecture. If the Pergyra implementation is written only as `main.pgy` plus
stage helper files, it can pass parity while failing to demonstrate Pergyra's
own programming model.

`PgyCompilerWorld` keeps these concerns separate:

- resource owners decide facts;
- parity harnesses compare artifacts;
- the root compiler intent declares the whole compiler action;
- derived stage intents make each source transformation explicit without
  turning the tree into C-style helper folders.

That gives hard self-hosting one visible world without hiding the per-stage SoT
owners that make the flow verifiable.

## Gate

`make self-host-compiler-world-contract-test-smoke` parses
`src/self_hosted/compiler/world.pgy` with the current compiler and checks that
the docs, owner manifest, and Makefile wiring still name `PgyCompilerWorld`.
It also executes the Pergyra path projection and fails if the shell manifest
drifts from `path_manifest_owner.pgy`. The same smoke is called by
`make self-host-preparation-test-smoke`.

For the owner-edit loop, `tests/self_host_compiler_topology_smoke.sh` checks the
root world, exactly 21 concrete resource zones and three executable world members,
the ordered `direct_mir`, `source_mir`, and `source_c` bindings, four derived stage clusters,
aggregate-zone prohibition, and the absence of a second compiler world in one
small pass. It also pins the production Main -> composition -> world -> zone
path so Main cannot silently restore the direct subject call.
`PGY_SELFHOST_COMPILER_WORLD_TOPOLOGY_ONLY=1` selects it through the full gate
entrypoint. The default gate remains the complete owner, artifact, path, and
AST check required at integration boundaries.

The topology and AST gates prove declared ownership and compilation shape.
The direct-MIR and source-to-MIR action gates separately prove the production
calls, rejected identity/pressure behavior, exact owner consumption, deleted
bypasses, and one action-owned write. `world.pgy --emit-c` is green, but root-intent takeover
still requires a later rung that makes `CompilePergyraProgram` executable and
removes its corresponding old path.

## Growth Rule

`PgyCompilerWorld` is not allowed to become a privileged monolith. `world.pgy`
stays under the same 600-line split-review cap as the other self-hosted owner
files. If it crosses that threshold, the fix is not an exception; the fix is to
move detail behind the owner that actually owns the fact.

The split unit is a resource-owned intent cluster:

- source-intake cluster: source path, import bundle, filesystem boundary;
- token/AST clusters: token stream and AST tree facts;
- middle-end clusters: semantic verdict and MIR fact flow;
- backend clusters: type environment, ABI layout facts, target capability
  envelope, current C emission buffer, future peer projection buffers, and
  codegen handoff facts;
- parity cluster: C/LLVM/Pergyra oracle comparison facts.

This is deliberately not "one file per small intent." Too many tiny files would
recreate C-style fragmentation under a different name. The root world should
keep the topology, `StagePathManifest`, and `CompilePergyraProgram`; stage
clusters should own detailed intent expansion only when that detail becomes
load-bearing. A new folder is not enough reason to introduce a new zone; a new
owned resource is.

## Stage Binding Visibility

Each active self-host compiler stage must name its compiler-world binding in
its own `intent.md`. The required row is:

```text
<stage>|<resource zone>|<stage actor>|<stage intent>|<payload contract>
```

That row mirrors `CompilerStageWorldBindingAt(...)` in
`path_manifest_owner.pgy` and the shell projection in
`tests/self_hosted/compiler_world_manifest.sh`. The point is not to inflate a
score by repeating words. The point is to make the stage say which compiler
resource it owns, which actor executes it, which intent the root world calls,
and which payload contract proves the stage fact is real. If a self-host stage
can be moved, renamed, or expanded without updating that row, the compiler
world has become decorative instead of load-bearing.

The likeness gate also tracks scaffold actions inside `world.pgy`: a compiler
actor that only returns `true` is still topology, not proof. Those stubs are
not allowed to regrow. The replacement is not more syntax; the replacement is
an actor action that consumes a concrete owner fact such as a path manifest,
stage artifact envelope, emitted artifact, or parity verdict.

Current source intake already consumes the `StagePathManifest` path fact through
`CompilerStagePathManifestReady()`, and parity comparison consumes artifact and
test-harness facts. Emission consumes ABI-layout, symbol, target-capability, and
codegen-stage payload facts. Lexing, parsing, semantic checking, MIR lowering,
and emission now consume `stage_artifact_owner.pgy`, which proves their
path/world-binding envelope before a stage can claim readiness. Lexer readiness
then goes one step deeper: it also consumes `LexerTokenPayloadContractReady()`
from `lexer/token_owner.pgy`.
Parser readiness does the same for the current compact-AST text rung through
`ParserAstTreePayloadContractReady()` in `parser/tree_text_owner.pgy`.
Semantic readiness consumes `SemanticVerdictPayloadContractReady()` from
`semantic/diagnostic_owner.pgy`, tying the stage to its verdict schema, fixture
count, status rendering, and code vocabulary. MIR readiness consumes
`MirFactGraphPayloadContractReady()` from
`mir_lower/mir_fact_graph_contract_owner.pgy`,
tying the stage to the MIR JSON schema, 85-fixture parity surface,
decl/routine arrays, source-local array, and instruction source facts. No
compiler stage is allowed to remain envelope-only. Backend emission consumes
`CompilerEmissionFactReady()`, which binds `ProgramEmitter` to the codegen
stage row and the typed AST arena migration contract in
`hir/typed_ast_arena_owner.pgy`.

## Pergyra-Likeness Reading

The Pergyra-likeness gate is not a beauty score. A high count of `world`,
`zone`, or `intent` tokens can still be decorative if those declarations do not
force stage code to consume owned facts. The useful reading is:

- topology exists: `PgyCompilerWorld` names the compiler resource map;
- topology is load-bearing: world actions call named compiler fact owners;
- stage rows are bound: each active stage publishes a
  `stage|zone|actor|intent|payload_contract` row;
- payloads are bound: stage readiness consumes the current payload contract,
  not only a path or world-placement row;
- bridge debt is visible: text-munging and sentinel metrics remain ratchets
  that should fall as typed AST/MIR facts replace compact text bridges.

This is why the gate tracks both positive topology and negative C-shaped
signals. The target is not to make every compiler helper into a zone. The target
is that every self-hosted compiler decision can be traced to one resource owner
inside the compiler world. If a new slice only adds more directories, helper
names, or repeated `zone` syntax without consuming a fact owner, it lowers
Pergyra-likeness even if the keyword count rises.

## Cost Model

The compiler world is an architecture contract, not a runtime hot path. It
should stay small enough that parsing it is cheaper than parsing a real
self-hosted stage entrypoint.

Local Windows measurement on 2026-06-24, after two warmup runs, using
`bin/pgy.exe --ast <file>`. `trimmed avg` drops the fastest and slowest sample
from the measured set:

| Source | n | trimmed avg ms | p50 ms | p90 ms |
|---|---:|---:|---:|---:|
| `src/self_hosted/compiler/world.pgy` | 15 | 18.2 | 18.3 | 19.1 |
| `src/self_hosted/lexer/main.pgy` | 15 | 24.5 | 23.6 | 25.6 |
| `src/self_hosted/parser/main.pgy` | 15 | 66.6 | 53.7 | 73.8 |

These numbers are a local smoke observation, not a cross-machine performance
claim. The useful invariant is relative: the compiler world must stay a thin
coordination contract. If it grows slower than a real stage entrypoint, it has
started absorbing source-owner work that belongs in a stage zone.

## Gate Cost

The default repository build is `make all`, and it builds the compiler/LSP
binaries only. Test binaries and parity fixtures are opt-in through
`make all-with-tests`, `make test-all`, or the named self-host gates below.

`make self-host-compiler-world-contract-test-smoke` is the fast structural gate:
it parses `world.pgy`, checks the manifest, and enforces the 600-line cap. That
is the right gate for edits to the compiler-world topology itself.

`make self-host-preparation-contract-test-smoke` is the normal quick
self-host-preparation check. It keeps tests to structural contracts and small
substrate checks; it does not run the full parity bundle.

`make self-host-preparation-parity-test-smoke` is the optional heavy path. It
runs the C/LLVM/Pergyra fixture comparisons and bootstrap checks.

`make self-host-preparation-impact-test-smoke` is the changed-path path. It
requires `PGY_SELFHOST_IMPACT_CHANGED_PATHS` or
`PGY_SELFHOST_IMPACT_CHANGED_PATHS_FILE`, then executes every affected
Pergyra-owned run group without re-running the whole heavy bundle.

`make self-host-preparation-impact-changed-paths-test-smoke` proves the outer
caller wrapper. The wrapper can collect changed paths from git or explicit env
input, but it does not classify paths; it forwards them to the Pergyra impact
planner through the impact Make target.

`make self-host-preparation-test-smoke` is intentionally much heavier. It is not
the normal compiler build; it is the development/CI wrapper that runs the quick
contract gate plus the heavy parity gate. A long runtime there usually means the
parity bundle is doing its job, not that `PgyCompilerWorld` itself is slow.

## Path Manifest Optimization

The compiler world gives self-hosting a single path topology. That matters for
speed because many current harnesses still rediscover files by scanning source
and test trees. The world shape lets a compiler run resolve the stage paths
once, cache their normalized variants, and then pass stable facts to source
intake, lexing, parsing, MIR lowering, codegen, and parity.

`StagePathManifest` is the first explicit fact for that direction.
`src/self_hosted/compiler/path_manifest_owner.pgy` owns the current path
values, and the shell manifest gate rejects drift between that owner and the
harness projection. It carries:

- the self-host source root;
- the self-host test root;
- the parity harness root;
- the compiler world source path;
- the active stage entrypoint paths;
- the stage-to-world binding rows that map each active stage to its resource
  zone, actor, and intent.

Existing discovery-style flow:

1. start from a directory;
2. recursively scan for candidate files;
3. filter by name or suffix;
4. normalize shell/Windows/compiler paths repeatedly;
5. infer the stage owner from the discovered path.

Compiler-world flow:

1. consume `StagePathManifest`;
2. check exact paths;
3. normalize once per run;
4. cache the normalized forms;
5. pass the path and world-binding facts to the stage-specific actor and intent.

Local Windows path-cost probe on 2026-06-24:

```text
make self-host-compiler-world-perf-probe
[self-host-compiler-world-perf] canonical_paths=13 direct_iters=1000 direct_avg_us=2982 scan_iters=100 scan_files=479 scan_avg_us=284238 scan_over_direct_x=95.29
```

This compares exact canonical path checks against recursive discovery over
`src/self_hosted` and `tests/self_hosted`. It is not a CI performance contract,
but it shows the optimization shape: once stage paths are first-class facts,
we should remove repeated discovery scans from parity/bootstrap/selfcheck code
and replace them with manifest-driven direct lookups.

Planned optimization order:

1. Cache normalized Bash/Windows/compiler path variants for every manifest path.
2. Replace `find src/self_hosted ...` discovery in self-host scripts with
   manifest checks where the stage set is closed.
3. Compile/copy stage bundles from manifest-owned import roots instead of
   copying every `*.pgy` sibling by glob.
4. Carry the same manifest into the self-hosted compiler driver once it becomes
   executable, so stage intents consume path facts instead of rebuilding them.
