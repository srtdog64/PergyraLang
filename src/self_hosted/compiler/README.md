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
`TargetCapabilityZone`, `SandboxCapabilityZone`, `CompatibilityEvolutionZone`, `AirEvidenceZone`, `SymbolFactTableZone`,
`AbiRowProjectionZone`, `EmissionZone`, `ArtifactZone`, `TestHarnessZone`,
`SubprocessRunnerZone`, and `ParityZone`), and each zone is driven by a smaller intent. The stage actors
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
buffer, ABI layout facts, target-capability facts, or parity evidence.
`EmissionZone` currently owns the emitted-C buffer; `ProgramEmitter` is the
participant that drives writes into that buffer. The target compiler world
splits this into peer C, LLVM, and SelfHosted emission zones only when those
projections consume the same MIR/type/ABI/target-capability facts and produce
comparable artifacts. Until then, the peer-emission split is a migration target,
not a status claim.

`TargetCapabilityZone` owns the projection envelope consumed before backend
emission. `target_capability_owner.pgy` names the accepted projection facts and
the fallback reasons (`unsupported_shape`, `forbidden_loss_budget`,
`retained_effect`, `missing_authority_evidence`, and
`host_only_slot_boundary`). `target_capability_manifest.pgy` projects that
envelope as a stable artifact, and
`self-host-target-capability-envelope-parity-test-smoke` requires the C and
LLVM-built self-host tool to agree on both the clean artifact and the
missing-fact fail-closed case. The derived DRV-2 projection row also carries a
bounded target fingerprint; the parity gate mutates that field and requires
the final emitter to reject it. AIR remains upstream verification evidence for
the native `VerifiedProjectionPlan`, not a backend-local fallback or a second
target-capability authority. That keeps CPU fallback or future accelerator
rejects visible as facts instead of backend-local choices.

`SandboxCapabilityZone` owns the capability and frame-budget vocabulary that
must exist before sandbox or interactive runtime claims become active.
`sandbox_capability_owner.pgy` names filesystem, network, clock, random,
subprocess, storage, render, input, frame-budget, ambient-denial, and blocking
host-call boundary facts. `sandbox_capability_manifest.pgy` projects those rows
as a stable artifact, with a missing-budget fail-closed case gated by
`self-host-sandbox-capability-parity-test-smoke`.

`AbiRowProjectionZone` consumes that target envelope when it projects ABI layout
rows. `abi_layout_row_owner.pgy` now ties the current `selfhost-c` ABI policy to
the accepted `cpu-c,self-hosted` projection set plus the required
`layout_shape,materialization_reason` facts and fallback reasons. The ABI row
artifact therefore records not only C value spelling, field order, tag/niche,
ownership, size/align, materialization, and default-return facts, but also the
target capability facts that make those rows valid.
The native MIR ABI layer mirrors that policy as `MIRAbiTargetPolicy`, and
`self-host-backend-abi-layout-contract-parity-test-smoke` requires the native
policy accessor plus the projected fact/fallback words so the target policy
cannot regress back into docs-only status.

`CompatibilityEvolutionZone` owns the versioned compatibility surface envelope:
source, ABI/binary, behavior, diagnostics, AIR evidence, MIR JSON, runtime
trace, capability profile, and stdlib module compatibility. It also owns the
required obsolete-migration fields (`diagnostic_id`, replacement,
`migration_url`, warning/error/remove versions, and codefix status) so upgrade
policy does not split between docs, diagnostics, and backend scripts. It also
owns the seed versioned breaking-change corpus for all nine compatibility
surfaces. The `compatibility_evolution_manifest.pgy` projection emits those rows
as a stable artifact consumed by
`self-host-compatibility-evolution-parity-test-smoke`. The same owner also
names the compatibility-corpus report schema, count fields, finding kinds, and
invalid-codefix, invalid-diagnostic-id, invalid-migration-url,
invalid-change-kind, and invalid-obsolete-migration negative statuses consumed
by `compatibility_evolution_checker`, so report-shape spelling, compatibility
behavior-class spelling, migration policy spelling, and obsolete migration
envelope spelling do not live in the checker.

`backend_emitter_contract_owner.pgy` names the first self-host-consumed
backend dumb-emitter source contract: required MIR/ABI runtime-row consumer
terms and forbidden backend-local runtime-name synthesis terms. The
`backend_emitter_contract_checker` tool consumes those rows and
`self-host-backend-emitter-contract-parity-test-smoke` proves the clean,
missing-required, missing-input, and forbidden-hit paths across C/LLVM-built
self-host tools.
`backend_air_access_contract_owner.pgy` is the adjacent AIR verification-only
boundary owner. It names the backend scan root, source extensions, forbidden
AIR header/type tokens, JSON count fields, finding kind, and negative self-test
path. `backend_air_access_checker` consumes those facts, walks `src/codegen`
with `DirWalk`, rejects AIR header/type tokens in backend sources, and is gated
by `self-host-backend-air-access-parity-test-smoke`.
The native/backend ABI layout contract has its own owner,
`backend_abi_layout_contract_owner.pgy`, and imports `abi_layout_row_owner.pgy`
instead of carrying a second shell list. The checker consumes required and
forbidden `(path, term)` rows through named membership predicates, uses ordered
rows only for artifact emission, requires selected native MIR ABI layout and
runtime function consumer terms, and rejects old `_rel` alias or runtime-name
synthesis terms. Its parity gate proves clean, missing-required, missing-input,
and forbidden-hit paths across C/LLVM-built self-host tools under
`self-host-backend-abi-layout-contract-parity-test-smoke`.
`runtime_call_abi_row_owner.pgy` is the self-host projection of the native
Slot/SecureSlot/DeviceSlot MIR resource runtime-call table as `native-resource`
rows, so backend resource helper spellings are visible in the same runnable
runtime-call ABI artifact as the self-host runtime helper rows.
Runtime-call compatibility policy is owned by
`compatibility_evolution_owner.pgy`; the projection consumes that policy but
does not define a second compatibility authority.
The self-hosted projection consumes the same native MIR operation/call-shape
rows and the native `pgy.machine-layer.declaration.v1` artifact consumed by
`compiler/machine_layer_declaration_consumer.pgy`; it does not repeat the
physical grant literals or claim live-board physical-device agreement. The
declaration also carries board/boot/linker provenance identifiers, which are
bound by the native physical fingerprint; the self-host reader accepts only
the native `pergyra.machine-declaration.*` namespace and requires nonempty
target-specific provenance. C and
LLVM remain peer projections with different representations, while self-hosting
is an additional parity consumer. `mir_lower/machine_layer_fact_owner.pgy`
validates explicit MIR contact rows, and
`tools/machine_layer_air_validator/main.pgy` validates AIR
`machine_layer_sites` rows against the passed declaration artifact.
`tools/machine_layer_rir_validator/main.pgy` consumes the native `pgy.rir.v1`
JSON contact rows before MIR lowering and rejects unknown contact identities
against that same owner.
`machine_layer_runtime_binding_owner.pgy` is the last self-host C consumer: it
emits the verified declaration mapping bind at `Main` startup. The runtime also
offers an embedder-owned board/MMU provider callback; the host-sim default keeps
that callback unbound and therefore makes no live-hardware claim.

The hard-self-host expansion owners live beside the world because they are
compiler-world facts, not codegen implementation details:
`compatibility_evolution_owner.pgy`, `compatibility_evolution_manifest.pgy`,
`air_evidence_owner.pgy`, `sandbox_capability_owner.pgy`,
`sandbox_capability_manifest.pgy`, `symbol_table_owner.pgy`,
`abi_layout_row_owner.pgy`, `backend_abi_layout_contract_owner.pgy`,
`artifact_zone_owner.pgy`, `test_harness_owner.pgy`, and
`subprocess_runner_owner.pgy`. These files own
vocabulary envelopes only. A surface remains active until C, LLVM, and
self-hosted consumers use those rows as data rather than rediscovering the same
facts from shell scripts, emitted text, or backend-local fallbacks.

`stage_intents.pgy` owns derived intent clusters such as `FrontendPipeline`,
`MiddleEndPipeline`, `BackendPipeline`, and `SelfProofPipeline`. These clusters
compose existing resource zones. They are not new zones and must not hide stage
facts or parity policy.

`PgyCompilerWorld` is the sole resource topology. Do not add an aggregate
compiler zone that repeats the actors and artifact types already owned by its
member zones. Stage clusters preserve positional zone carriage; a shorter
value bundle is not a substitute for a visible authority/resource boundary.
The documented target facade groups those owners as `FrontendResources`,
`MiddleEndResources`, `EvidenceResources`, and `BackendResources` for human and
tool inspection. Those names are non-owning projections until a typed
zone-bound handle, complete consumer migration, old-path deletion, and a
missing-child negative gate land together. They must not be implemented as a
second aggregate owner.

The bounded executable topology currently has four ordered members:
`direct_mir: DriverRung2DirectMirZone`, `source_mir: DriverSourceMirZone`,
`source_llvm: DriverSourceLlvmIntentZone`, and `source_c: DriverSourceCZone`.
The sole composition owner materializes all four once. Production `Main`
reaches their actions and consumes typed
outcomes; it does not directly call a migrated backend/source producer or
commit its artifact. Both direct-backend publication and general MIR-to-C
publication share the existing `direct_mir` authority/lifetime boundary. The
fourth zone owns the real-purpose source-to-LLVM intent execution boundary; it
does not exist merely because there is another CLI mode. The exact public `pgy --mir-json <source>` request now makes the
`source_mir` stdout slice `SUBSTITUTING`: `pgy_driver.c` selects the installed
Pergyra-built sibling before `driver_run_pipeline`, and missing/unsupported
requests fail without native retry. This does not make the whole world or every
source/MIR feature `SUBSTITUTING`; the remaining slices keep their independently
measured grade.

Exact public `pgy <source> --emit-llvm -o <output.ll>` is also a bounded
`SUBSTITUTING` slice. The C launcher admits only that file form, materializes
the existing installed source-MIR plus `DirectMirLlvm` actions in a private
workspace, and publishes opaque bytes. It cannot re-enter native semantics or
libLLVM after failure. Stdout `--emit-llvm` remains open and must reuse the same
Pergyra owners rather than adding another backend.

Checkpoint `8bc7f525` closes that stdout form through the same two installed
actions. A fixed 16 KiB binary stream transports the completed private
artifact without a whole-file string or text inspection. Public file/stdout
LLVM IR is byte-equal. General direct-LLVM CFG coverage remains independently
open.

## Pergyra-Style Check

A self-hosted compiler slice is accepted only when it reads as a Pergyra
compiler world, not as a C folder graph rewritten in `.pgy`. The flow must be
owned by `CompilePergyraProgram` or a named derived intent cluster; zones must
own distinct resources; expression/statement/function/program files must remain
participants over those resources; and every semantic decision must come from a
single fact owner. If a slice needs a missing fact, it adds the owner fact or
rejects the input. It must not recover that fact from AST text, JSON text,
backend-local fallbacks, or import order.

The compiler world also owns `StagePathManifest`, the canonical path fact for
self-host source roots, test roots, parity harness roots, and active stage
entrypoints. `path_manifest_owner.pgy` owns the current string values for those
paths and the stage-to-world binding rows that map each stage to its resource
zone, actor, intent, and payload contract.
`tests/self_hosted/compiler_world_manifest.sh` is the shell-side projection used
by the gates. The compiler-world contract compiles the Pergyra TestHarness
manifest and compares its `compiler-world-paths` projection against that shell
file, so the shell list is a checked projection rather than a second unchecked
source of truth. That gives future hard-substitution code a way to consume
paths, compiler-world placement, and stage payload ownership as facts instead
of rediscovering them with recursive scans or folder names.

`driver_rung0_owner.pgy` is the first in-process assembly owner for that rule.
It composes the self-host parser's `ParseRootProgram` output with the
self-host codegen `GenerateC` input only after consuming compiler-world path,
stage-artifact, and target-capability readiness facts. The AST and C artifact
edges are named separately (`CompileSourceToAst`, `CompileAstToC`, and
`CompileSourceToC`). `driver_rung0_main.pgy` is the runnable boundary for those
facts, and `tests/self_hosted/parity/driver_rung0_parity.sh` compares the
assembled AST/C outputs against the C oracle across the same 69 committed
codegen parity fixtures named by `codegen/fixture_manifest_owner.pgy`. It is
not a second driver graph; the owner still only assembles parser and codegen
facts.

`driver_cli_owner.pgy` is the DRV-1 CLI surface owner. It consumes the DRV-0
artifact functions and owns only argv shape: source path, `--emit-ast` /
`--emit-c`, and `-o` artifact writes. `driver_rung1_main.pgy` is the runnable
boundary for that surface, and `tests/self_hosted/parity/driver_rung1_parity.sh`
checks stdout and file-output parity across that shared driver fixture frontier
without moving artifact generation out of the stage owners.

The root `CompilePergyraProgram` intent in `world.pgy` now substitutes the
bounded production source-to-LLVM path, while the `direct_mir`, `source_mir`,
`source_llvm`, and `source_c` members are production-reachable. The remaining
compiler-purpose surface is not implied to be substituting. The file is parse-gated by
`make self-host-compiler-world-contract-test-smoke` and wired into
`make self-host-preparation-test-smoke`. That gate also enforces
**manifest-to-reality conformance**: every stage `StagePathManifest` names must
own a real `src/self_hosted/<stage>/` directory with `.pgy` facts, and every
on-disk stage (a dir with `main.pgy`) must be named by the world so the
architecture manifest cannot silently drift from the stage owners. It does not
claim that the released compiler is self-hosted; it fixes the shape that hard
substitution must grow into and keeps non-reachable target zones out of the
world value until their bypass is deleted.

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
