# Self-compile memory A2 execution

- Status: LOCAL VALIDATION COMPLETE, publication/CI pending. User resumed
  the memory plan on 2026-11-17. Base: `5b0d354fb`, clean tracked tree.
  Results: [A2 evidence](../truth/evidence/2026-11-17_selfhost_memory_a2_phi_reuse.md).
- Integration owner: GPT root. One edit scope, no parallel implementation.
- Objective: reduce the existing phi consumer's repeated allocations on the
  fixed semantic and DRV-2 MIR inputs without changing its answers or output.
- Priority: memory safety, fail-closed identity/dominance, equal C/LLVM output,
  peak RSS, time. No larger gate or memory allowance and no smaller input.
- Fact owners: `ssa_identity_owner.pgy` owns canonical SSA spelling;
  `routine_definition_dominance_fact_owner.pgy` owns dominance and strict
  definition order; `phi_predecessor_binding_fact_owner.pgy` selects incoming
  rows. The last consumers are MIR reconstruction and direct C/LLVM phi emission.
- Full chain: installed driver -> MIR admission -> routine fact index -> phi
  fact -> incoming predecessor binding -> reconstruction/direct emitter -> C/LLVM.
- Change set: compare SSA names without allocating prefix/version Strings;
  retain one avoiding traversal per dominator only inside one phi invocation
  against the same immutable routine index. The dominance owner's shared
  predicate consumes that proof; verification replays every reuse. Pair-only
  reuse missed the target because another predecessor repeated the same
  avoiding traversal; retain the traversal rather than individual answers.
- The graph traversal owner also accepts one named Int queue for the selecting
  invocation and resets its length between traversals. Returned distances do
  not alias that queue. No mutable field carrier/restoration is introduced;
  the queue is a separate invocation-local workspace passed through the owner.
- One stable local borrows a retained proof's Int buffer before the `ref`
  boundary. No Clone, transfer or restoration is added. Self-host currently
  refuses a field expression at that boundary; remove this local when that
  existing boundary accepts an admitted field view. The proof array does not
  grow on the return path where this view is read.
- Forbidden fallback: a spelling-based predecessor, first/last-name selection,
  stale cache across invocations, broader admission, backend-specific decisions,
  manual source Clone/drop, or a new program-shape route.
- Gates: focused canonical SSA boundary examples and query replay; existing
  predecessor/phi negative gates on C/LLVM; one fixed MIR input produces equal
  C before/after; `semantic/main.pgy --mir-json` peak RSS <=800 MiB; codegen and
  driver fixed point plus required CI at the published candidate.
- Validation roots: `D:\PergyraLang` edits/main; WSL `/root/mg-main` builds
  identical source. Heavy builds and measurements run one at a time. Static
  gates <=60 s, focused parity <=5 min, integration shard <=30 min.
- Next dependency: A1 writer convergence after A2 measurement. The larger value
  lifetime plan stays in the user's memory directive and its decision; this
  execution card creates no semantic authority or completion claim.
