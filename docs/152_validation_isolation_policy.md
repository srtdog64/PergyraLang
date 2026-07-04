# Validation Isolation Policy

Status: `beta-closure-validation-policy`

This document fixes how Pergyra chooses validation scope during source-of-truth
closure and hard self-host work.

The purpose is not to avoid evidence. The purpose is to respect the ownership
boundaries that the compiler already created. If a fact is isolated behind one
owner, unrelated surfaces must not be re-executed just because a CI summary
lists many targets.

## Core Rule

Run a gate only when the changed owner can affect the artifact that gate owns.

Read-only inspection is always allowed. Case execution, backend compare shards,
full CI wrappers, and `test-all` are not default follow-up actions. They require
one of these conditions:

- the changed owner emits or mutates the artifact that the broader gate checks;
- the narrow owner gate reports a cross-boundary failure;
- a release, merge, or push checkpoint explicitly asks for broad confidence;
- the user explicitly asks to run the broader gate.

A CI failure list is triage input, not a dependency graph. First inspect the
failed log tail, identify the owner surface, and then choose the smallest gate
that proves that owner did not drift.

## Impact Decision

Before running any executable case, answer these three questions in order:

1. **Which owner changed?** Use the path and the owning document/module, not the
   CI target name. `src/self_hosted/compiler/target_capability_owner.pgy`
   changes `TargetCapabilityZone`; it does not change every backend compare
   shard just because a platform summary later lists backend failures.
2. **Which artifact can that owner emit or mutate?** If the answer is only a
   vocabulary/envelope document, owner source, or static contract, the default
   evidence is read-only inspection plus a contract smoke at most.
3. **Which consumer actually crosses the owner boundary?** Escalate only to a
   consumer gate that reads that exact fact. If no consumer fact crosses the
   boundary, do not run unrelated cases.

The default when uncertain is not "run more." The default is to inspect the
owner linkage until the affected artifact is named. Broad execution is useful
release evidence, but it is not a substitute for impact analysis.

## CI Runner Modes

Pergyra has two different validation modes, and they must not be confused.

**Owner validation mode** is the normal source-of-truth work loop. It starts
from the changed owner, chooses one artifact, and runs either no case, a static
contract gate, or the narrowest executable gate for that artifact. This is the
mode to use during AST/MIR/AIR/ABI/self-host closure.

**Release collection mode** is what the platform CI step lists do. The runner in
`scripts/ci_step_runner.sh` defaults to collect mode: after one step fails, it
keeps executing independent steps and prints a final summary. That is useful for
expensive CI because one run exposes every red surface. It is not an impact
selector and it is not evidence that the first failed owner affected every later
failed owner.

`PGY_CI_FAIL_FAST=1` can turn the runner back into first-failure mode, but that
only changes reporting. It still does not infer the changed owner. The owner
must be named by reading the diff and the failed log tail.

Platform step lists such as `scripts/ci_linux_steps.sh`,
`scripts/ci_windows_steps.sh`, and `scripts/ci_macos_steps.sh` are release
surface enumerations. They intentionally run many unrelated owners:

- build/source inventory and shell policy;
- frontend, semantic, AIR, MIR, ABI, runtime, and backend gates;
- self-host contract and parity bundles;
- broad C/LLVM backend compare shards.

That shape is acceptable for release confidence. It is not the default local
response to an isolated SoT edit. If a change touches only MIR declaration
metadata, a later macOS slot-contract failure is an independent owner failure
until the log proves that the MIR declaration fact crossed into the slot
artifact.

## Isolation Surfaces

| Surface | Owned Artifact | Narrow Evidence | Escalate Only When |
|---|---|---|---|
| Build/source inventory | Makefile source lists, CI shell selection, generated source inventory | `build-source-inventory-test-smoke` | a source list, build script, shell runner, or generated inventory path changed |
| Lexer/parser | token stream, compact AST text, parser diagnostics | lexer/parser focused smokes, frontend fixture for the changed syntax | downstream behavior changed because the token/AST contract changed |
| Semantic/type/DAG | structured diagnostics, checked type facts, resolver metadata | `semantic-core-shape-test-smoke`, `type-resolution-*` gates, focused semantic fixture | the change alters emitted IR facts or backend-visible type metadata |
| AIR/evidence | intent/effect/authority/coordination evidence, AIR JSON | AIR schema/drift/intent-compression gates | backend erasure/materialization or self-host evidence consumes the changed AIR fact |
| MIR CFG/body | CFG, branch/join, cleanup, local type facts, body dataflow | `cfg-body-dataflow-test-smoke`, focused MIR fixture | a backend consumes the changed MIR body fact |
| MIR declaration inventory | `MIRDeclHeader`, `MIRRoutine` signature facts, binding rows, method/field/generic metadata | `mir-declaration-inventory-test-smoke` | emitted C/LLVM shape changes for a fixture that consumes the changed declaration fact |
| ABI/layout/runtime shape | slot/pin/handle ownership shape, layout facts, runtime ABI boundary | ABI/slot/runtime shape gates for the touched owner | a backend projection or runtime boundary consumes the changed ABI fact |
| C backend projection | emitted C text and C run behavior | C-focused smoke or named fixture for the changed emitter | LLVM parity is needed because the same MIR/AIR/ABI fact is shared |
| LLVM backend projection | emitted LLVM IR and LLVM run behavior | LLVM-focused smoke or named fixture for the changed lowering | C parity is needed because the same MIR/AIR/ABI fact is shared |
| Backend compare | C/LLVM behavioral parity on committed cases | named fixture or shard only for the changed projection surface | user asks for broad parity, or a shared projection owner changed |
| Self-host rung | Pergyra implementation artifact compared to oracle | the rung's contract smoke or focused parity fixture | the rung is being promoted or a shared compiler-world owner changed |
| Docs/proofs | documented contract, proof model, methodology binding | documentation/proof smoke for the touched document family | implementation code also changed the contract being documented |

## Change-Surface Defaults

This table is the practical "do not run unrelated work" rule.

| Changed Surface | Default Local Evidence | Do Not Run By Default |
|---|---|---|
| Documentation only | read-only diff review, optional documentation/proof smoke if the touched family has one | backend compare, slot/runtime gates, self-host parity bundles |
| Proof/model file only | proof/document smoke for that model family, if execution is explicitly allowed | C/LLVM backend compare, semantic fixture sweeps, platform CI |
| Shell/Makefile/source inventory | source inventory gate and the edited shell contract | semantic/runtime/backend cases unless their source list changed |
| Self-host compiler-world envelope owner | `self-host-compiler-world-contract-test-smoke` or static term inspection | frontend fixtures, slot contract, broad backend compare, platform wrappers |
| Self-host stage owner | that stage's component contract or one named parity fixture | full `self-host-preparation-test-smoke` before the component gate is stable |
| Lexer/parser syntax owner | the focused lexer/parser fixture for the changed syntax | backend compare, runtime ABI, self-host parity unrelated to that fixture |
| Semantic/type owner | focused semantic/type resolver gate | backend projection gates until emitted IR facts change |
| AIR evidence owner | AIR schema/drift gate for the changed evidence | backend nonimpact/full compare unless that AIR fact is consumed by output |
| MIR CFG/body owner | CFG/body/dataflow gate and a named MIR fixture | declaration inventory, slot/runtime, broad backend compare |
| MIR declaration owner | declaration-inventory gate | CFG/body, slot/runtime, semantic core shape |
| ABI/layout owner | ABI/layout/slot shape gate for that owner | parser/semantic fixtures that do not consume layout facts |
| C-only emitter | C projection fixture for the touched emitter | LLVM shard sweep unless the input fact is shared |
| LLVM-only lowering | LLVM projection fixture for the touched lowering | C projection sweep unless the input fact is shared |
| Shared projection fact | narrow owner gate, then one named C/LLVM parity fixture if output-visible | all shards, all platforms, `test-all` |
| Runtime/materialization classification | runtime/materialization evidence gate | parser/semantic/backend gates outside retained-runtime consumers |

## Isolation Face Audit

This is the current judgment after rechecking the Makefile targets, platform CI
step lists, self-host gates, and runner behavior.

| Isolation Face | Current Judgment | What It Means |
|---|---|---|
| Owner/fact isolation | Good direction, still discipline-dependent | The repo has the SoT spine and this policy, but the CI summary cannot infer ownership. Every change still needs an explicit owner sentence before execution. |
| Build artifact isolation | Mostly sound | `BUILD_DIR`/`BIN_DIR` are threaded through platform executable gates. Text/static gates deliberately ignore build dirs because they own repository text, not build products. |
| Source inventory isolation | Global hygiene, not impact-local | `build-source-inventory-test-smoke` checks Makefile lists, shell policy, tracked artifacts, and portable scripts. Its cross-platform failure is expected because it owns repo-wide build surface. Do not run it for a pure MIR/AIR/semantic owner edit unless source lists or CI scripts changed. |
| Fixture/case isolation | Needs explicit selection | Backend compare and AIR nonimpact can run named fixtures, limits, or shards, but broad defaults still scan committed case sets. For local SoT work, use named fixtures only when the changed owner reaches that fixture's artifact. |
| Backend projection isolation | Conditionally shared | C and LLVM are isolated projections only after MIR/AIR/type/ABI facts are fixed. A backend-local emitter edit should start with that backend's gate. A shared fact edit may require C/LLVM parity. |
| AIR nonimpact isolation | Heavy by design | The nonimpact smoke proves strict AIR evidence does not perturb selected backend outputs. It is not a cheap AIR owner gate and should not run after unrelated docs, self-host, or MIR declaration edits. |
| Self-host isolation | Split correctly, but the wrapper is heavy | `self-host-preparation-contract-test-smoke` is the structural path. `self-host-preparation-parity-test-smoke` is the oracle comparison path. `self-host-preparation-test-smoke` runs both and should be treated as release/development evidence, not the first local step for a single owner edit. |
| Platform isolation | Release-only | Linux, Windows, and macOS step lists enumerate platform confidence surfaces. A platform CI summary groups independent owner failures; it does not mean the current patch touched all those owners. |
| Runtime/materialization isolation | Evidence-bound | Runtime calls or retained world/zone/slot artifacts are acceptable only when AIR/MIR/ABI retaining facts explain them. If a change touches materialization classification, use the runtime/materialization gate; otherwise runtime gates are independent. |
| Docs/proof isolation | Text-contract only | A doc/proof smoke proves wording and model alignment. It does not prove implementation erasure or self-host substitution unless paired with the owner implementation gate. |

The weak spot is not that unrelated surfaces exist. The weak spot is treating a
release CI collection as an owner dependency graph. The fix is procedural:
classify each red step by owner before deciding whether it belongs to the
current patch.

## Per-Face Trigger Contract

This table is the operational form of the audit. It answers: if this face is
red, what can it prove, and what must it not pull into the current patch by
default?

| Isolation Face | First Proof To Inspect | Belongs To Current Patch When | Must Stay Isolated From |
|---|---|---|---|
| Source/build inventory | changed Makefile/source-list/shell term and the inventory log tail | the diff changed source registration, shell selection, generated-file policy, or tracked fixture inventory | semantic, runtime, backend, and self-host failures that do not name a source-list or script fact |
| Semantic/type shape | diagnostic/type/resolver owner named by the failing fixture | the diff changed that semantic owner or changed a fact consumed by it | backend compare and runtime gates until emitted IR/type facts are shown to change |
| Checked arithmetic | arithmetic semantic/lowering owner and the checked-arith fixture tail | the diff touched checked-arith syntax, semantic facts, or lowering/runtime exports | general semantic-core or backend shards that are not checked-arith consumers |
| AIR evidence/nonimpact | AIR schema/evidence/materialization fact named in the log | the diff changed AIR evidence, AIR strictness, or a backend artifact under strict AIR | self-host, slot, parser, and broad backend compare failures without an AIR fact crossing |
| MIR CFG/body | MIR body/source-shape/local-type owner and named fixture | the diff changed CFG/body facts or a backend-visible MIR body consumer | declaration inventory, ABI/runtime, and semantic-core gates unless the log names the crossed fact |
| MIR declaration inventory | declaration header/routine signature/binding-row owner | the diff changed declaration metadata or a backend-visible declaration consumer | CFG/body, slot/runtime, and unrelated semantic fixtures |
| Slot/runtime/materialization | ABI/slot/runtime-frontier/materialization owner | the diff changed slot layout, retained-runtime policy, runtime ABI exports, or materialization evidence | parser/frontend/self-host gates unless they consume that exact runtime fact |
| Backend projection | the named C or LLVM emitter/lowering fixture | the diff changed that projection owner, or a shared MIR/AIR/ABI fact that projection consumes | the other backend's full shard set until a shared fact or parity drift is named |
| Backend compare | the single failing case and its projection owner | the changed owner reaches that case's emitted artifact or run output | unrelated backend-compare shards, semantic sweeps, and self-host parity bundles |
| Self-host contract | the rung owner, declared oracle, artifact kind, and focused contract/parity tail | the diff changed that rung, a shared compiler-world owner, or its artifact/test-harness owner | platform CI wrappers and unrelated compiler-stage rungs |
| Platform CI | failed child step plus platform/toolchain tail | the diff changed platform scripts, toolchain discovery, path conversion, or a platform-only runtime boundary | treating all later red steps as current-patch regressions |
| Docs/proofs | touched doc/proof family and any matching doc smoke | the patch changes the contract text or model for that owner | implementation gates unless implementation files also changed |

If a face is red but the "Belongs To Current Patch" column is false, record it
as an independent owner failure. Do not keep executing adjacent cases just
because the release runner would have collected them.

## CI Summary Reading Example

The 2026-07-04 red summaries are a good example of why release collection must
not become the local work loop. A platform summary can list many red targets,
but the validation decision is still per isolation face:

| Reported Failure | Face | Default Classification | Local Response |
|---|---|---|---|
| `build-source-inventory-test-smoke` on Linux/macOS/Windows | Source/build inventory | one global hygiene owner, not three language regressions | inspect source-list/shell-policy drift; do not run semantic/backend/self-host cases unless the source inventory log names them |
| `checkedarith-failclosed-test-smoke` | Checked arithmetic | impacted only if checked-arith syntax, semantic fact, lowering, or runtime export changed | use the checked-arith owner log first; do not expand to semantic-core or backend shards by target adjacency |
| `semantic-core-shape-test-smoke` | Semantic/type shape | independent unless the current diff changed semantic/type facts | inspect the failing diagnostic/type owner; backend compare is not implied until emitted IR/type facts cross |
| `self-host-preparation-test-smoke` | Self-host wrapper | aggregate wrapper over contract/parity rungs | follow the child rung, usually `contract` or one parity script; do not treat the wrapper as a reason to run every self-hosted tool |
| `slot-contract-test-smoke` | Slot/runtime/materialization | independent unless slot ABI/materialization facts changed | inspect slot owner terms; do not connect it to parser/self-host/backend failures without a named retained-runtime fact |
| `llvm-test-backend-compare` | Backend compare | named projection failure | identify the single case and projection owner; do not rerun all shards unless a shared MIR/AIR/ABI fact is implicated |
| `air-strict-backend-compare-test-smoke` | AIR evidence plus backend projection | shared only when strict AIR facts can change output | inspect AIR fact crossing; otherwise treat as a separate release-confidence failure |
| `test-all` or platform `ci-*` | Aggregate wrapper | class C wrapper | ignore the wrapper name after the child failure is identified |

This example is intentionally procedural. It does not say those failures are
unimportant. It says they are not automatically part of the same patch. Each
one needs an owner/fact sentence before any case family is executed.

## Execution Budget For Isolated Work

When the user asks to stop case execution, the allowed local actions are:

- read diffs, docs, and gate scripts;
- inspect log snippets already supplied by CI;
- update contracts, docs, or static ratchets;
- run formatting/static text checks only when they do not execute fixtures,
  compiler outputs, or platform wrappers.

Disallowed by default in that mode:

- `test-all`, `ci-*`, backend compare shard sweeps, and AIR nonimpact sweeps;
- any smoke that compiles or runs committed fixture programs;
- repeating a platform summary locally just to see whether unrelated faces are
  still red.

The escape hatch is explicit: name the owner, name the artifact, name the
crossed fact, and get approval for the narrow executable gate.

## Stop Conditions

Stop expanding validation when any of these is true:

- the next gate owns an artifact that the changed owner cannot emit, mutate, or
  verify;
- the current failure is an aggregate wrapper and the child failure has already
  been identified;
- the proposed gate would execute an entire case family before a named fixture
  or static owner contract has failed;
- the only reason to run the gate is that it appeared later in a platform CI
  summary;
- the patch is documentation/proof-only and no implementation artifact changed.

Continue only when the crossed owner fact is named in the log tail, the diff, or
the gate contract. This is the operational version of source-of-truth closure:
validation follows facts, not fear.

## CI Failure Classification

When a CI summary reports multiple failures, classify each failed step before
running more code:

| Class | Meaning | Default Action |
|---|---|---|
| A. Impacted owner failure | The changed owner emits, mutates, or verifies the artifact that failed. | Debug it in the current patch with the narrow owner gate. |
| B. Independent owner failure | The failed gate owns a different artifact and the log tail shows no crossed owner fact. | Record it as separate work; do not expand the current validation scope. |
| C. Aggregate wrapper failure | A target failed only because it calls another failed target, such as `test-all`, platform CI, or a preparation wrapper. | Follow the child failure, not the wrapper name. |
| D. Environment/toolchain failure | The log tail names missing tools, path conversion, compiler executable discovery, or platform runtime mismatch. | Route to build/toolchain ownership; do not infer language or backend drift. |

Examples:

- `build-source-inventory-test-smoke` failing on every platform is one global
  build/source-inventory failure, not three unrelated language regressions.
- `self-host-preparation-test-smoke` failing because a parity child failed
  belongs to that child rung; the wrapper is class C.
- `llvm-test-backend-compare` failing only one named case belongs to the
  projection owner for that case. It does not justify rerunning every unrelated
  semantic, runtime, and self-host gate.
- `air-strict-backend-compare-test-smoke` is relevant only when the changed
  owner can change AIR evidence or a backend artifact under strict AIR.

## Protocol

For every source-of-truth change:

1. Name the owner surface before running anything.
2. Inspect the changed files and choose the narrow evidence row from the table.
3. Run no case if the evidence can be established by read-only inspection or
   documentation review.
4. If execution is needed, run only the narrow owner gate first.
5. Escalate to targeted fixtures only when the narrow gate proves the changed
   fact reaches that fixture's artifact.
6. Escalate to full backend compare, `test-all`, platform CI wrappers, or shard
   sweeps only after explicit user approval or a release checkpoint.
7. If a release CI summary is already red, classify each failure as A/B/C/D
   before adding it to the current patch. Do not let an aggregate summary expand
   an isolated owner edit by default.

The same rule applies when fixing CI. A failing target outside the touched
surface is not automatically part of the current change. It is a separate owner
failure until its log tail proves otherwise.

## Example: MIR Generic Metadata In C Specialization

Changing `src/codegen/transpiler_generic_specialization_emit.c` to consume
`MIRDeclGenericParam` facts touches the MIR declaration inventory surface and
the C generic-specialization projection.

Allowed default evidence:

- inspect the diff for AST semantic rereads;
- run `mir-declaration-inventory-test-smoke` if execution is allowed.

Optional targeted evidence, only when requested:

- backend compare fixtures that exercise generic specialization.

Not default evidence:

- slot contract;
- checked arithmetic;
- semantic core shape;
- full backend compare;
- platform CI wrappers;
- `test-all`.

Those gates own different artifacts. Running them without a crossed owner fact
weakens the isolation discipline the compiler is trying to prove.

## Hard Self-Host Impact

Hard self-hosting does not loosen this policy. A self-hosted rung must still
name its owner, oracle, artifact, and narrow evidence. A parity bundle is
load-bearing only for the rung it owns. It must not become a habit of rerunning
unrelated compiler surfaces after every isolated SoT edit.
