# Pergyra Source-Of-Truth Spine

Last updated: 2026-05-10

This document freezes the compiler ownership spine for beta closure. It exists
to stop A -> B -> A refactoring loops. When a future change is unclear, use this
document to decide which layer owns the fact, which layers may only consume it,
and which compatibility seams are allowed to remain.

## 0. Rule

Each semantic fact has exactly one owning layer. Later layers may consume the
fact, attach provenance to it, or emit diagnostics from it, but they must not
rediscover or reinterpret it.

Smoke tests are not source of truth. A smoke test only prevents a frozen owner
contract from drifting.

## 1. Ownership Table

| Concern | Source of truth | Consumers | Forbidden pattern |
|---|---|---|---|
| Parsed syntax and source spans | AST | Diagnostics, lowering provenance | Backend semantic rediscovery by walking AST |
| Body control flow | HIR CFG | MIR lowering, semantic body facts, AIR evidence | AST helper deciding reachability or all-path return |
| Body safety facts | MIR CFG/dataflow | C backend, LLVM backend, AIR evidence | Backend-local cleanup/drop/pin rules |
| MIR source shape and source-location compatibility | MIR source-shape owner | MIR validators, DCE, C/LLVM emitters, dumps | Consumers reopening raw `source_ast_type` / `source_line` fields |
| MIR compatibility AST payload | MIR source-shape owner | C/LLVM residual source emitters, diagnostics, validators | Consumers reading `inst->ast` directly outside MIR construction/population/source-shape owners |
| Declaration/domain inventory | DIR/RIR/MIR declaration headers | C/LLVM declaration emitters | AST-carried backend inventory as final truth |
| Type/declaration dependency | Type-resolution DAG metadata | Semantic owners, AIR DAG evidence | Recursive resolver fallback on frozen paths |
| Generic/ability contract evidence | Type-resolution DAG | Semantic contract checks, AIR | Compatibility counters as semantic truth |
| Resource/authority/effect propagation | RIR | AIR, runtime/codegen policy emitters | AIR or backend inventing authority/resource facts |
| Cleanup/drop/pin topology | MIR cleanup facts | C/LLVM cleanup emitters, AIR | Topology-only cleanup without expected fact payload |
| Abstraction boundary drift | AIR | Driver diagnostics, CI, LSP/JSON consumers | Backends consuming AIR for codegen |
| Runtime pass/failure policy | Runtime policy headers | C/LLVM codegen wrappers, AIR global evidence | Duplicated pass limits or failure strings in emitters |
| ABI surface | ABI/runtime headers | C/LLVM, tests, docs | Domain layer leaking layout changes into C FFI silently |

## 2. Layer Contracts

### AST

AST owns raw parse structure, source spans, and user-facing syntax provenance.
AST does not own semantic truth after lowering begins.

Allowed AST use after semantic/lowering:

- source spans for diagnostics;
- source labels/names for provenance;
- compatibility payloads while a frozen MIR/DIR inventory path is being built.

Forbidden AST use:

- backend walking AST to decide safety;
- backend walking AST to rediscover declaration inventory;
- AST helpers deciding ownership, authority, effect, cleanup, or type success.

### HIR CFG

HIR CFG owns explicit body shape: basic blocks, control-flow edges, reachable
body regions, and source terminator provenance before MIR lowering.

HIR CFG answers:

- which paths exist;
- which body region owns a boundary;
- which source terminator produced a branch/return;
- whether a body can be lowered into CFG-owned MIR.

HIR CFG does not answer final resource cleanup, pin safety, or backend emission
shape. Those are MIR facts.

### MIR CFG/Dataflow

MIR owns beta body safety and backend execution facts.

MIR answers:

- all-path return and terminator provenance after lowering;
- source-statement emit facts for compatibility lowering;
- cleanup, rollback, invalidation, and pin cleanup edges;
- non-CFG fallback accounting;
- value summaries and liveness facts used by backends;
- declaration headers required for backend parity.

C and LLVM must consume MIR facts rather than duplicate their own body-safety
rules. If C and LLVM disagree, fix the MIR fact or the consumer, not a backend
heuristic.

### Type-Resolution DAG

The DAG owns declaration/type dependency truth. Recursive resolver fallback is
retired for the frozen beta surface.

Allowed:

- metadata lookup;
- owner-local materialization through the central metadata API;
- explicit dead-end diagnostics;
- compatibility counters that report zero fallback on beta paths.
- metadata-first type-ref reads for stable placeholder construction.

Forbidden:

- direct `resolve_type_node(...)` outside the central metadata owner;
- hidden recursive fallback;
- annotation-or-unknown compatibility helpers;
- annotation-only reads outside private metadata owners;
- using compatibility counters as semantic evidence;
- declaration-order success when a DAG dependency fact is missing.

### DIR/RIR

DIR owns declaration/domain graph inventory. RIR owns resource, authority,
effect, relation, projection, channel, IO, and runtime-relevant propagation
facts.

DIR/RIR answer:

- which domain declarations exist;
- which authority/resource/effect boundary exists;
- which operation produces runtime-relevant propagation evidence;
- which declaration inventory is available to backends.

DIR/RIR facts may be summarized into MIR/AIR, but later layers must not invent
them.

### AIR

AIR is a verification layer, not a codegen IR.

AIR answers:

- whether declared intent/zone/world/effect boundaries drift from actual HIR,
  RIR, MIR, DAG, and runtime-policy evidence;
- whether required boundary evidence exists;
- whether evidence provenance is complete enough for diagnostics.

AIR does not own:

- CFG reachability;
- type resolution;
- cleanup generation;
- runtime frontier scheduling;
- backend lowering.

AIR may reject missing or inconsistent evidence. It must not synthesize lower
layer facts to make evidence pass.

### Runtime Policy Headers

Runtime policy headers own stable ABI/runtime rules that must be shared by C and
LLVM emitters.

Examples:

- bounded frontier pass-limit arithmetic;
- panic/failure classes;
- observability schema;
- Slot/Pin ABI constants;
- authority failure query surface.

Emitters may wrap these policies, but must not duplicate them as independent
local rules.

## 3. Consumer Classification

Every consumer of a cross-layer fact must be classified as one of:

| Class | Meaning | Rule |
|---|---|---|
| Truth owner | Computes and stores the fact | Exactly one layer |
| Consumer | Reads fact and emits code/diagnostic | Must not recompute |
| Provenance consumer | Uses AST/name/source info only for messages | May read AST, not decide truth |
| Compatibility seam | Temporary bridge with explicit name | Must be gated and shrinking |
| Smoke gate | Regression guard | Cannot define semantics |

When adding or moving code, classify the file/function before editing it. If the
classification is unclear, do not refactor yet.

## 4. Beta Blocker Order

The beta closure order is:

1. CFG/MIR body safety source-of-truth.
2. AIR abstraction-boundary verifier coverage.
3. Type-resolution DAG source-of-truth closure.
4. Runtime frontier/failure policy generated-path verification.
5. MIR declaration inventory parity for C/LLVM.
6. ABI/Slot/Pin/Zone-bound handle freeze.
7. Dogfood path through C backend and external modules.

Do not spend beta time on broad folder reshuffles, helper naming cleanup, or
line-count splits unless they directly unblock one of these items.

## 5. Refactoring Stop Rules

Stop a refactor if any of these appear:

- the same fact would be owned by two layers after the change;
- a smoke test becomes the only place where a rule is defined;
- a backend starts walking AST to compensate for missing MIR/DIR/RIR facts;
- a compatibility seam grows without a planned deletion path;
- a split is justified only by line count, not by responsibility;
- the change improves local structure but does not move a beta blocker.

## 6. Allowed Temporary Debt

Some seams are allowed until the owning path fully replaces them:

- AST compatibility payloads inside declaration headers, only when explicitly
  named `ast_compat` or equivalent;
- local compiler-run skips in smoke scripts when no explicit `PGY_BIN` is
  provided, while CI/Make-provided `PGY_BIN` remains strict;
- C-era filename namespaces before post-beta self-host;
- compatibility telemetry counters that report retired paths as zero.

Allowed debt must be named. Unnamed fallback is not allowed.

## 7. Working Rule For Agents

Before changing compiler architecture, answer these four questions:

1. Which layer owns the fact?
2. Which consumers should read it?
3. Which old compatibility seam is being removed or narrowed?
4. Which smoke/regression proves the owner contract did not drift?

If there is no answer, the change is probably another A -> B -> A loop.
