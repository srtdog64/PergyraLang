# Ownership-based automatic memory management: bounded GC comparison

Status: `IMPLEMENTATION COMPLETE` (documentation / formal comparison only;
compiler/runtime implementation remains OPEN, not a new self-host rung).
Base: `3658548d24bca3d721e4f1974ac7a10da99f7aa8`, preserved dirty main.
The user explicitly requested the name, a core-mechanism declaration, and Rocq
evidence for the safety/performance comparison on 2026-10-08.

## Objective card

- **Objective:** adopt "소유권 기반 자동 메모리 관리" as the name of the
  existing compiler-owned cleanup mechanism, and prove only falsifiable,
  explicitly scoped comparisons with tracing collection.
- **Priority:** source semantics and honest quantifiers; one ownership owner;
  memory safety and reclamation; executable negative controls; then cost.
- **Fact owner:** `27_ownership_clean.md` and `OwnershipCleanCore.v` keep
  ownership semantics. A new importing comparison supplement owns only
  collector-envelope and abstract operation-cost propositions.
- **Last consumer:** a typed Rocq audit and the fresh kernel gate; vision and
  algorithm documentation consume these propositions, not theorem names.
- **Forbidden:** changing the core machine, a runtime GC/RC fallback, a second
  compiler ownership pass, equating operation counts with wall time, or
  declaring all correct GCs less safe/slower.
- **Gate / falsifiers:** Rocq 9.3.0 + `rocqchk`, zero assumptions for the focused
  slice; correct GC may match exact retention; retaining an unreachable block
  is read-safe but not exact reclamation; zero inspection weight prevents a
  strict speed claim; a bulk-reset cost alternative refutes universal speed.

## Whole-chain map and closure plan

1. Existing admission / source semantics -> canonical `elab` -> `texec` and
   `INV` remain unchanged. Read the full affected definition and theorem chain.
2. The comparison imports `Block`, `INV`, `heap_of`, `bmem`, `texec`, and `elab`.
   It neither introduces target instructions nor replaces a proof core.
3. At a canonical boundary, compare its exact heap with a duplicate-free
   collector heap containing all live owner/frame blocks. Prove coverage and
   retention bounds; do not call GC coverage itself the entire safety theorem.
4. Give an ideal-root, nonmoving full-heap sweep its actual list traversal
   count. A shared allocate/observe/retire workload must compile and execute
   through canonical `elab`/`texec`. Compare only common allocation/release
   costs plus that sweep's inspection work.
5. Consume exact theorem types and positive/negative witnesses in an audit.
   Register the supplement in the existing proof inventory and CI gate.
6. Update vision, agent direction, doc 27 naming/claim boundary, doc 207
   explanation, proof indexes, and this chat's handoff card. Re-read shared
   files immediately before each append and preserve concurrent changes.

Independent scope: the new comparison proof/audit/gate, naming and comparison
documentation, and append-only registrations. Do not edit Claude's Core,
ReadOnly, Composition or Exits proofs, doc 207 section 11, compiler/runtime
implementation, installed binaries, or SoT statuses. This direct user request
opens this bounded supplement; it does not transfer the remaining proof cores.
Integration owner: this chat. Shared integration gate:
`tests/ownership_gc_comparison_smoke.sh`, then fresh corpus / formal inventory.
Budgets: 60 s static, 300 s focused, 1800 s fresh corpus. No new worktree,
staging, commit, push, installation or GUI message in this scope.

## Initial evidence and remaining obligations

- Read the current implementation directive and source, not an archived rung.
- Current Core SHA-256 at the initial read:
  `c629a6a36c0bf19520687665fd49d1ee1fffc8e49498f1a122837d41ab40ffda`.
  Concurrent proof work has advanced beyond the directive's older hash.
- Existing `ownership_cleanup_smoke.sh` fresh kernel passes 4 modules / zero
  axioms, but its OCaml observer then fails at `ownership_clean_driver.ml:30`:
  `Unbound value borrow_all`. This pre-existing source/harness API drift is
  outside the comparison's ownership decision. This initial failure is
  superseded by the final current-source extraction run recorded below; this
  chat did not change that concurrent observer repair.
- Actual C/LLVM timing, peak RSS, allocator refinement, observable resource
  finalization, self-host implementation, installation and exact-SHA CI remain
  obligations of the existing implementation chain.

## Final observed receipt (2026-10-08)

All runs used the admitted Rocq 9.3.0 / Stdlib 9.2.0 switch. No ownership core,
compiler/runtime file, installed executable or SoT status was changed here.

| Gate | Observed result |
|---|---|
| `ownership_gc_comparison_smoke.sh` | PASS: 3 freshly compiled and kernel-verified modules including the typed audit; 0 axioms, no admits / unsafe kernel features |
| `formal_semantics_smoke.sh` | PASS: 62 fresh proofs plus approval export/binding consumer; exactly the two approved SlotCalculus abstractions |
| Current `ownership_cleanup_smoke.sh` rerun | PASS: concurrent observer update resolves the earlier API failure; existing canonical extraction / places / exits controls and 30 fixed cost cases pass |
| `documentation_quality_smoke.sh` | PASS: UTF, index, wording and executable-example surface |
| Bash syntax, `git diff --check`, `make -n ownership-gc-comparison-test-smoke` | PASS; target resolves to the dedicated comparison gate |

Checked hashes (SHA-256):

- Core: `c629a6a36c0bf19520687665fd49d1ee1fffc8e49498f1a122837d41ab40ffda`
- Comparison: `9ae1efcff4df52ce757ff69c5ee1d04828af9d872e6ea40c92ff4e1eab44899f`
- Typed audit: `1b943593ecc4c5481f35933791297e1952f1af5fe0abb87e276098d058f02787`
- Focused gate: `3f2acea0de2269fabedbad8398a7a817ac5b70502e9a7f543667beb045a4ea28`

Logs: `.tmp/ownership-cleanup/gc-comparison/{kernel,formal,existing-extraction}.log`.
Existing G1 model-cost output is `.tmp/ownership-cleanup/model-cost.json`; its
elaboration timings are not an ownership-vs-GC runtime benchmark.
The original core's nested-list scheme warning, existing corpus name-masking
warnings and approval loadpath warning remain visible. This is not a
warning-free receipt. Kernel theory reports the existing indices-not-mattering
profile explicitly; it is not silently counted as an added axiom.

The comparison is registered in the formal inventory, Make and stable Rocq CI.
The typed audit runs in its dedicated Make target and the stable-Rocq CI step;
it is not unconditionally added to the general formal Make recipe. This
preserves that recipe's existing explicit missing-prover/DECLARED-SKIP policy
without duplicating it in a second owner. The dedicated comparison gate itself
always requires the admitted prover and never reports a skip as success.
No remote workflow ran here. HEAD remains the base revision with the preserved
dirty main; host Git observed 165 dirty entries and an empty index before the
final handoff append. No stage/commit/push/install/GUI message occurred.
Next falsifier: validate the same admitted source/value and cleanup decisions
against actual C/LLVM allocator/glue, measure copied bytes, peak memory,
throughput and release tail latency, and keep the comparison collector and its
cost premises explicit. The existing I1-I8 implementation order is unchanged.
