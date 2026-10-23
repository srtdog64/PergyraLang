# String copy to physical release: review follow-up

Status: IMPLEMENTATION COMPLETE for the bounded String-copy slice only.
Publication and whole ownership closure remain OPEN; see the
[run record](../truth/evidence/2026-10-23_string_copy_physical_release.md).
Base: `8fb4b7998` on shared main. Native reference snapshot: `dca7fd0ce`.
The 2026-10-23 user request authorizes repairs, formal checks and counterexamples;
the subsequent answer leaves publication and exact-SHA remote CI to main's
integration worker. This lane commits only its own verified changes.

## Objective card and complete reached chain

- Objective: a checked String copy used only by a synchronous non-retaining
  consumer acquires a compiler-owned lifetime and is physically freed when
  that lifetime ends. Source code contains no drop, region or new annotation.
- Priority: exact call/type identity and unique ownership; no early/double
  release; physical release evidence; C/LLVM agreement; patch size.
- Fact owner: `semantic_region_escape_collect`, using resolved builtin identity,
  the checked Clone result type and the existing argument-retention owner.
- Carrier: allocation syntax ID + function syntax ID + scope ID through
  semantic -> HIR region facts -> MIR region facts -> AIR-admitted immutable
  `PgyRegionPlan`. No second escape analysis in either backend.
- Consumers: C `transpiler_emit_clone_value` and LLVM `llvm_emit_clone_value`.
  The String copy takes the existing region allocation twin only for a
  certified site in its exact active scope. Nested aggregate fields are not
  granted the root call's lifetime.
  The shared C concat consumer and LLVM concat twin also refuse an issued
  grant in the wrong active scope; they must not silently downgrade it.
- Last consumer: the existing function-exit region cleanup in C and LLVM,
  followed by `pgy_region_destroy`, which frees the region's physical blocks.
- Forbidden: free an arbitrary `char *`; infer ownership from `let`; use source
  spelling as Clone identity; return region storage; grant a whole aggregate
  from one String field; GC/RC fallback; suppressed LSan or skipped comparison.
- Gate/falsifier: repeated calls without source drops must retain no String-copy
  allocation; normal/early returns preserve output. Returned/retained copies,
  unresolved identity/type and a non-String Clone receive no region grant.

This closes the selected lifetime end-to-end, not all String lifetimes. The
existing bounded region owner handles direct synchronous consumer arguments;
unproved local aliases, returned values, retaining calls, deferred bodies,
aggregate payload ownership and loop-local last-use synthesis are not silently
promoted. Region reclamation is at the existing function boundary, not a new
claim of per-expression ASAP free or bounded peak memory in an unbounded loop.
Uncertified sites retain their existing heap disposition; that disposition is
explicitly outside this cleanup claim, not a declaration of leak freedom.

## Planned slices and independent scopes

1. Contract navigation and `let` comparison: `dca7fd0ce`; no syntax change.
2. Typed proof-spine consumer and missing-contract negatives: `8fb4b7998`.
3. The reached String copy fact producer, C/LLVM consumers and physical-release
   gate. The gate must fail on the identified reference compiler before the
   repair and pass on an identified compiler built from the candidate.
4. Main integration: the remaining ordinary-value lifetime contract, removal
   of per-shape self-host routes/MIR->AST reconstruction, canonical admission,
   bootstrap and installed-driver parity. These depend on the active main
   ownership rung and are not closed by the focused String gate.

This lane owns this directive, its new audit/evidence records, the reached
region-copy changes and their tests. Preserve the existing edits to
`docs/01_intent_first_design.md`, `src/self_hosted/mir/artifact_lower_owner.pgy`
and the untracked indexed-String directive. Do not edit main's truth-state
files or the other worker's active MIR routine path. No new worktree/branch.

## Validation and handoff

Static gates: 60 s. Focused execution: 5 min. Build/integration shard: 30 min.
Use a fresh temporary source snapshot and an isolated `-j2` native build;
never replace installed binaries or terminate another worker's process.
Check semantic unit negatives, actual emitted artifacts and runtime output,
C ASan/UBSan with leak detection enabled, and LLVM physical leak detection.
Do not describe uninstrumented LLVM execution as full ASan coverage.

The integration owner is the main worker. Report exact commit(s), executable
identity, observed gates and unresolved obligations; no push or CI-green claim
is made here. The shared integration gate remains the unchanged canonical,
bootstrap/gen2==gen3 and installed C/LLVM driver on one published SHA.
