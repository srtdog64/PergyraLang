# Self-Hosted Compiler World Architecture

Status: `hard-self-host-shape-contract`

The self-hosted compiler must not become a Pergyra rewrite of the C folder
layout. Pergyra's language surface is intent-first and world/zone-oriented, so
the hard self-host shape is rooted in `PgyCompilerWorld`.

## Rule

`PgyCompilerWorld` is the self-host compiler owner. Stage directories own facts;
resource zones own isolated compiler resources; the world owns the user-visible
compiler flow.

- `src/self_hosted/compiler/world.pgy` names the hard-substitution world.
- `PgyCompilerWorld` contains the compiler zone and the derived resource zones.
- `CompilePergyraProgram` is the root compiler intent.
- `SelfHostCompiler` is the closed compiler state for one source unit plus its
  C/LLVM oracle pair.
- `ProgramEmitter` is the emission participant that drives writes into
  `EmissionZone`; it is not a resource zone.
- `SourceIntakeZone`, `TokenStreamZone`, `AstTreeZone`,
  `SemanticVerdictZone`, `MirFactGraphZone`, `TypeEnvZone`, `AbiLayoutZone`,
  `TargetCapabilityZone`, `EmissionZone`, and `ParityZone` are the derived
  resource zones.
- `StagePathManifest` is the canonical path fact for the self-hosted source,
  test, parity, and stage entrypoint locations.
- `path_manifest_owner.pgy` owns the current Pergyra string values for those
  paths; `tests/self_hosted/compiler_world_manifest.sh` is the shell projection
  used by the gates.
- `IntakeSource`, `LexSource`, `ParseTokens`, `CheckProgramSemantics`,
  `LowerProgramFacts`, `PlanTargetProjection`, `EmitProgramArtifact`, and
  `ProveSelfHostedParity` are the derived stage intents.
- Codegen is the projection nerve bundle from the compiler world into backend
  artifacts. It is grouped by resource zones such as `EmissionZone`,
  `TypeEnvZone`, and `AbiLayoutZone`, not by pretending every emitter file owns
  a zone.
- `LexerStage`, `ParserStage`, `SemanticStage`, and `MirLowerStage` are
  distinct actors. The world does not use a generic `StageOwner.Consume()`
  alias because lexing, parsing, semantic checking, and MIR lowering own
  different artifacts.
- `lexer/`, `parser/`, `semantic/`, `mir_lower/`, and `codegen/` remain
  source-of-truth owners for their facts.

This is not a claim that the released compiler is self-hosted. It is a shape
constraint: new hard-substitution work should plug into `PgyCompilerWorld`
instead of growing a second C-style tree.

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
- `EmissionZone` owns the emitted C text buffer and admits writes through the
  `ProgramEmitter` participant.
- `ParityZone` owns C/LLVM/Pergyra comparison evidence.

`program_emit`, `function_emit`, `stmt_emit`, `expr_rewrite`, and
`struct_value_emit` are not zones by themselves. They are participants in the
codegen action graph. They may live in separate files for review size, but they
do not become zones unless they own a distinct resource.

For codegen this is the concrete split:

- `EmissionZone`: `object slot c_output: EmittedC`, driven by
  `subject slot emitter: ProgramEmitter`.
- `TypeEnvZone`: `object slot bindings: TypeEnvironment`, consumed as
  read-mostly type evidence.
- `AbiLayoutZone`: `object slot layouts: AbiLayoutFacts`, consumed as
  read-only ABI/layout evidence.
- `TargetCapabilityZone`: `object slot envelope: TargetCapabilityEnvelope`,
  driven by `subject slot planner: TargetProjectionPlanner`.
- Future symbol/name-mangling state may become a separate zone only if it owns
  mutable symbol facts. A new emitter file is not enough.

That makes codegen a backend resource cluster, not a folder taxonomy. The
cluster has output state, type facts, and ABI layout facts; `program_emit`, `function_emit`,
`stmt_emit`, `expr_rewrite`, and `struct_value_emit` are actions over those
resources. Splitting those actions into files can keep review size under
control, but the split is not a semantic zone split.

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
The same smoke is called by `make self-host-preparation-test-smoke`.

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
  envelope, emission buffer, and codegen handoff facts;
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
`codegen/typed_ast_node_skeleton.pgy`.

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
