# Ownership clean: G-scope observed receipt

Navigation/evidence only; this report owns no semantics, registry status,
compiler substitution or successor rung. Work authority and edit scopes are
in ../agent_work_directives/ownership_clean_work_split_2026-10-08.md.

## Checkpoint and boundaries

Main remains at 3658548d24bca3d721e4f1974ac7a10da99f7aa8, uncommitted.
Only G items were implemented. Claude's C1/C2/C3/C5 implementation remains
its own lane; no proof repair, MIR contract or alternate heap model was added
here. Per pergyra-authoring, cleanup mechanics remain compiler-owned rather
than becoming new routine user ownership annotations.

G0/G1 and G2's source rollback are locally verified. G6 restores the reached
caps/OWNERS rows and adds the retired-owner direction ratchet. G3 waits for
C2's versioned MIR move/copy/drop/type-glue interface, G4 for C5's self-host
facts, and G5 for actual C3/C5/G3/G4 replacement evidence. On this checkpoint
27_ownership_clean.md §5 is absent; do not invent its interface in a backend.
The baseline manual releases and aggregate-release analyzer are NOT deleted.

No staging, commit, push, installed-binary replacement, remote Actions run or
GUI message occurred. The broader world/component static inventories exceeded
their unchanged 60-s budget; that is incomplete evidence, not a semantic
failure or permission to enlarge the budget. There is no runtime C/LLVM
ownership-clean, ASan/LSan, DRV-2, D1 enforcement or automatic-cleanup claim.

## Stable toolchain and current model admission

Project WSL prefix: /home/c/.local/share/pergyra-rocq/opam.
Named switch: pgy-rocq-9.3.0. Rocq core/runtime and rocqchk are 9.3.0; the
independently versioned Stdlib is 9.2.0. Project-local opam is 2.6.1,
checksum-pinned; system Coq and system opam are preserved. CI uses the pinned
9.0.1 container only to bootstrap a fresh stable switch, never as proof
admission. Local Docker execution and remote CI are not observed.

Current checked OwnershipCleanCore SHA-256:
d4cca76d5d3ff463126452ddfd928a936ca2e5be19bf91f640deee9c865b3c8b.

Fresh corpus checking and formal smoke pass for 58 proof sources plus the
approval export/binding consumer. All 58 copied proof hashes were compared
against the current sources and match. The two approved SlotCalculus
assumption names AND types remain pinned; unsafe kernel features and rewrite
rules are refused. The admitted default kernel theory profile, including
indices-not-mattering dependencies, is documented in
docs/semantics/proofs/AssumptionBudget.md.
The earlier line-1212 failure is preserved but does not reproduce on this
current hash. Its original cause has not been established.

Local activation examples (PowerShell):

```powershell
wsl -d Ubuntu-E-WSL --exec env OPAMROOT=/home/c/.local/share/pergyra-rocq/opam bash /mnt/d/PergyraLang/scripts/run_rocq_toolchain.sh bash /mnt/d/PergyraLang/tests/coq_kernel_check.sh
wsl -d Ubuntu-E-WSL --exec env OPAMROOT=/home/c/.local/share/pergyra-rocq/opam bash /mnt/d/PergyraLang/scripts/run_rocq_toolchain.sh bash /mnt/d/PergyraLang/tests/ownership_cleanup_smoke.sh
```

## G1 extraction and cost boundary

OwnershipCleanup.v was deleted. The harness freshly extracts the canonical
elab(s,L,B), elab_fun and elab_proc; it has no second state machine or heap.
The two-module extraction kernel check has zero assumptions. Fifteen decision
and 24 refusal controls pass, including borrowed inputs, call result/input
overlap, inout/read overlap, malformed function/procedure definitions, and
the actual GUI caller/callees. The current GUI call composition needs two
callee copies; sink inference/elision are still C1 work, not claimed here.

Latest receipt: .tmp/ownership-cleanup/model-cost.json,
schema pergyra.ownership-clean.elab-cost.v2, timestamp
2026-10-08T02:16:39.134912+00:00. It records exact model, extraction, driver,
toolchain-owner, kernel-gate and generator hashes, generated artifacts, fixed
input recipes, host and compiler versions. The measured dirty count is the
WSL Git observation, not a cleanliness claim for Windows Git. Before and
after measurement all six bound source hashes must match the copied snapshot.

Thirty deterministic cases vary statements, live-set/borrowed-set width and
loop depth. Inputs are prebuilt; each case uses one warmup, five fixed repeats
and the median per call. No workload reduction or relaxed gate budget was used.

| Family | Cases | Maximum median CPU time per elaboration |
|---|---:|---:|
| Statements (128–2048) | 5 | 0.111 ms |
| Live width (8–128, 64 statements) | 5 | 3.894 ms |
| Borrowed width (8–128) | 5 | 4.225 ms |
| Loop depth/width | 10 | about 3.77 ms |
| Wide-live program (16–256 definitions, plus observations) | 5 | 41.344 ms |

These are bounded OCaml-extraction analyzer costs, not Pergyra runtime memory,
GC costs, compiler-scale latency, a speedup, or implementation refinement.
No production optimization follows merely from the measured growth.

## G2 recovery and preserved work

The exact 196 tracked restore and 234 new-file deletion inventories are in
../agent_work_directives/ownership_clean_g2_rollback_2026-10-08.md.
All 196 tracked files match 3658548d; all 234 removed paths are absent and
have recovery copies. All 7536 source imports resolve. Twelve explicitly
preserved native assignment/retention files match the pre-work ceafa8f4
snapshot exactly. The old v11–v14 continuation is SUPERSEDED, not revived.

Recovery copies (ignored; preserve them):

- .tmp/ownership-cleanup/g2-recovery/<original relative path>
- refs/backup/main-dirty-2026-10-08 at
  ceafa8f4796f15db0d54e64f5d9775f1cc2f8360
- .tmp/backup/untracked-2026-10-08.tgz

Installed binaries remain unchanged:

- bin/pgy.exe: f6559da94876c93a2e7303866429ef31ef284b1f29ca3d68f67ff5e710cbc1c9
- bin/pgy-self-driver.exe: 707dcd40049a1697a5827b2a7c8d3cf509573aa3c0031f2eee338c9fa0d78ec7

## Observed logs and next falsifiers

Logs are under .tmp/rocq93_ownership/ (exit 0 unless noted):

- kernel-current-c1.log: current 58-file fresh corpus and approval consumer.
- formal-current-c1.log: registered corpus and current fresh kernel check.
- model-calls-final.log: 15/24 controls and 30 cost cases on current C1.
- toolchain-negative-final.log and kernel-negative-admitted-final.log:
  wrong version/checker/provenance, stale/orphan, admits and export/type drift.
- certificate-extraction.log: 262144 finite valuations, four length controls,
  41 envelope controls; not compiler/backend adequacy.
- focused-consumers.log: spine, machine, SoT, methodology proof consumers.
- direction-negative-final.log: retired-owner residue and search-tool/I/O
  failures both fail closed, without mutating the source model.
- ci-language-static.log and beta-toolchain-current.log: proof-job consumers.
- g2-contracts-retention.log and g2-retention-direction.log: hard wiring and
  retained argument-registry projection (the latter's early direction result
  was superseded by direction-negative-final.log).
- syntax-yaml.log and workflow-yaml.log: shell/Python/OCaml syntax and YAML;
  model-calls-final.log separately proves actual OCaml compilation/execution.
- g2-world-trace.log: explicit exit 124 at 60.837 s; no full-world green.
  Component inventory likewise did not finish in its static budget.

Next falsifiers: source/hash or extracted API drift, missing version/approved
fact, a retired manual-chain owner reappearing, or a C2 consumer inferring
ownership instead of reading its admitted fact. G3 may start only after C2
publishes that interface; runtime/shared-fixture leak/UAF and backend parity
then become the acceptance boundary. Proof-model admission is not that receipt.

## Requested read-only Rocq algorithm analysis

Review scope: the fixed d4cca76d... model above, its theorem premises,
elaboration/call boundaries and currently reached runtime/backend source.
No Claude proof or compiler implementation was changed by this review.

The direction matches compiler-owned cleanup: elab derives move/copy and
last-use release, settle unifies release placement, the INV owner excludes
two owning footprints for one block, and CORR binds live variables to source
contents. Readonly loans may share a caller footprint without becoming a
second owner. Per-arm settling gives uniform exit ownership in this bounded
structured language; loop-head facts are checked rather than blindly trusted.
Call frame entry/exit restores the caller's inout owner. H and R are proof
state, not a proposed runtime tracing collector or global shadow-heap list.

Material boundaries, not refutations of the existing conditional theorem:

1. **Admission is conditional.** OwnershipCleanCore.v:2026-2035 assumes all
   callee bodies elaborate, a source execution exists, and initial INV/CORR
   hold. At 2292 the no-leak theorem also assumes source execution. elab
   itself does not resolve a function table or check call arity. The scratch
   OwnershipScopeAudit.v proves six observations: unresolved-call and
   wrong-arity inputs can elaborate with empty live-in; unresolved calls and
   wrong arity have no source execution in the named tables; an undefined
   read is carried as live-in but cannot execute from the empty environment.
   Fresh two-module kernel check passes with zero assumptions:
   .tmp/rocq93_ownership/review-scope-current.log. The earlier scratch tactic
   failures are superseded by this pass, not proof-owner failures. The actual
   pass must consume existing typed/name/arity admission facts and fail closed
   when they are absent; do not create another AST/type validator downstream.
2. **Physical refinement is still missing.** TE_Copy/TE_Field (329/347) give
   fresh abstract storage [n] for an entire pure SVal tree. That proves
   disjoint abstract ownership, not recursive copying/freeing of real String
   pointers, nested Array/Map/List storage or active enum payloads. LLVM's
   nested array literal still explicitly shares inner data
   (src/codegen/llvm_expr_aggregate.c:186). Array<String> clone already
   duplicates strings (runtime copy macro at 197), so that working special
   case must not be called shallow; it still does not establish general
   aggregate refinement. C2 descriptors and G3 parity/leak/UAF evidence must
   connect these concrete operations to the model before manual deletion.
3. **The desired call DX is not complete.** gui_calls_copy_twice (2533)
   confirms two copies across the constructor/inout-append callees, despite
   zero caller copies. D1(B)'s collections need inferred consumption and
   verified readonly alias elision to avoid demanding routine Clone calls.
   These are existing C1 obligations, not a reason to add own/ref annotations
   or a competing parameter-convention owner.
4. **Do not widen the claim to all exits/types.** SStmt has normal structured
   branches/loops, not return/break/continue/error-unwind or partial aggregate
   initialization. The main theorem uses finite big-step derivations; it does
   not establish every prefix of a divergent GUI event loop, OOM behavior,
   peak bytes or the 3 GiB cap. Slot/region/zone/async/FFI and member inout are
   explicitly outside this core. The document's no-drop-flags consequence is
   limited to this uniform-exit model, not a theorem for every Pergyra exit.
   ASAP drop is observationally silent here (TE_Drop emits []); resource
   finalizers with visible effects need their own admitted cleanup boundary.
5. **Cost growth is a measured warning, not an optimization mandate.** On
   the fixed wide-live cases, width 64/128/256 takes 0.777/5.457/41.344 ms.
   Source vdiff/settle uses repeated membership over lists; superlinear growth
   is plausible from that structure, but no asymptotic theorem or compiler-
   scale cost is claimed. Preserve the fact owner and input identity; optimize
   only if this operation blocks the named real closure gate in its budget.

Recommended order within the existing split: keep C1's inference obligations
explicit; publish C2's single typed interface and its source/runtime mapping;
then admit C3+G3 on shared branch/loop/call/copy fixtures and both backends.
Check normal and failure exits plus typed missing-fact refusals at that
boundary. C5+G4 parity precedes G5 deletion. Do not add new research tracks,
weaken the safety premise or call model checking whole-compiler closure.

## Requested concrete-code baseline

The user requested code tests after questioning the extracted model's cost.
Following pergyra-authoring, the fixtures use ordinary func/struct code with
observable results: String -> GuiDraw -> Array, both inline and through a
constructor plus an inout append; three wide-live programs define 64/128/256
heap Strings before observing them all. No new own/ref annotations, manual
cleanup, decorated world, compiler implementation or alternate heap model was
added. Test objective/chain and repeat script are under
.tmp/ownership-cleanup/code-baseline/ (PLAN.md and run_baseline.ps1).

Observed PASS: five positive programs times two explicit backends; one warmup
and three emission samples per route; each of ten built executables ran three
times with exact stdout and no stderr. A separate read-back checks all 30
stdout logs and all ten executable hashes. The existing
tests/concept_semantics/hashmap/inout_string_array_deep_drop.pgy is refused by
both C and LLVM with the specific borrowed-parameter deep-drop diagnostic;
negative cases were compile-only and never executed. Positive compile/build
logs show zero errors and warnings. No failure was hidden by another backend
or the default self-host route. Total focused run: 16.952 s, 84 processes.

Warm emission medians in milliseconds (external native build excluded):

| Source | C | LLVM |
|---|---:|---:|
| GUI inline | 60.638 | 56.804 |
| GUI constructor/inout calls | 47.938 | 49.526 |
| Wide 64 (128 define/observe statements) | 48.291 | 50.613 |
| Wide 128 (256 define/observe statements) | 50.824 | 67.899 |
| Wide 256 (512 define/observe statements) | 64.232 | 74.157 |

These are full compiler-process launch/admission/MIR/emission times, not
isolated liveness/ownership costs. Executable-process medians range from
35.550 to 52.696 ms and include startup and stdout; they are not copy/drop or
allocator microbenchmarks. Three samples are a bounded comparison baseline,
not a statistically established performance claim. The wide cases double
both variable width and statement count. Their shape resembles the model
recipes, but no verified source-to-model projection or timing ratio is claimed.

Provenance: main HEAD 3658548d24bca3d721e4f1974ac7a10da99f7aa8, Windows Git
123 dirty entries before and after; installed compiler hash f6559da9... and
driver hash 707dcd40... unchanged. Input/model/compiler/runtime-source hashes
are stable throughout. The compiler executable's current-source build binding
is UNKNOWN: it was pinned, not rebuilt or replaced here. Available toolchain
versions recorded in the receipt are MSYS2 GCC 16.1.0 and clang 22.1.8; emitted
C/LLVM artifact and built-executable hashes are also recorded. Receipt:
.tmp/ownership-cleanup/code-baseline/receipt.json, schema
pergyra.ownership-clean.concrete-code-baseline.v1, SHA-256
b1d49470fc9dc80a165d36b6e67596fea84a0100da5c6cb95521892b02924e63.

C2's interface and the actual C3 automatic-cleanup pass are still absent.
Thus this cannot answer whether that algorithm is fast enough, demonstrate
runtime automatic frees, zero leaks/UAF, long-running GUI memory, the 3 GiB
compiler cap or DRV-2. No production edits, installation, commit/push or CI run
were performed. Preserve these inputs and repeat at the C2/C3+G3 boundary with
owner-derived move/copy/drop counters and ASan/LSan oracles. Until that actual
implementation is measured, its production performance remains UNMEASURED;
neither the model warning nor this baseline proves adoption or rejection.
