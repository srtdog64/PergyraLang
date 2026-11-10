# Join-shadow and installed cleanup/fixedpoint integration

State and run receipts: [owned bridge state](../truth/state/raw_view_bridge.md).
This is a continuation of the raw/View bridge, not a separate compiler track.

## Objective card

- Objective: repair the Join-shadow refusal falsifier, carry the admitted
  ownership/provenance/retirement chain through installed C/LLVM execution,
  and establish current-source gen2 == gen3 with the repository gates.
- Priority: callee identity and actual retention; safe inout restoration;
  unchanged refusal contracts; fixed source/executables; physical release;
  whole compiler-input and fixedpoint/installed integration evidence.
- Fact owner: native callee resolution and parameter-flow summary; existing
  MIR cleanup and runtime ABI owners; their Pergyra fact consumers.
- Last consumers: installed general MIR emitters and runtime free, followed
  by codegen/driver fixedpoint comparison and installation receipts.
- Forbidden fallback: treat a declared Join as stdlib by spelling; reject a
  proved safe inout update just to satisfy an old expectation; delete unsafe
  refusal coverage; use an unbound seed or old driver as current evidence;
  compile the replacement driver directly with native C as a substitution.
- Falsifiers: a same-named declared callee that lets Array storage escape
  must still refuse at the ownership transition; the safe original mutation
  must execute and retire once. Bypassing declaration identity in a scratch
  cleanup owner must make the negative falsely admitted. Missing provenance,
  String/scalar-kind confusion and duplicate retirements remain rejected.

## Whole chain and closure plan

1. Verify the shared HEAD, latest main chat and actual binary output. The
   initial planning check found five reached files different from main's live
   validation root; inspect the current wire instead of inferring integration.
2. Callee source -> semantic identity/summary -> graph binding -> private-use
   cleanup -> C/LLVM ABI emission. Original Join only appends an independently
   owned String through an inout descriptor and returns a literal; existing
   restored value-result proof legitimately admits it. Preserve that original
   program as positive coverage. Make the Join-shadow negative exercise a
   real outward storage effect, with the same caller/lifetime hazard; keep all
   other existing positive/negative inputs and expectations unchanged.
3. Verify the negative reaches MIR ownership validation, not an unrelated
   type/link error. Use one scratch forbidden-name-fallback mutation as a
   behavioral control. No production safety change is planned for a stale
   fixture expectation.
4. Freeze the complete current shared compiler input and its relevant test,
   runtime, script and grammar dependencies in a separate validation copy.
   Identify the rebuilt native executable and immutable source manifest.
5. Run the unchanged compiler bootstrap and driver fixedpoint/installation
   owners, preserving budgets and exact-source receipts. Reuse only a seed
   whose repository receipt validates against this source generation.
6. Execute installed C/LLVM source and MIR routes on the fixed original call
   arguments and raw/View cleanup program; observe allocations, reallocations,
   free and leak/UAF controls. Migrate any reached existing owner/consumer seam
   before extending the patch; add no per-program admission route.
7. Verify gen2 == gen3 on the gate's complete compiler input, preserve refusal
   matrices, publish scoped evidence/state and hand the patch/results to the
   main Pergyra chat. Installed-copy evidence is distinct from main publication
   and exact-SHA CI, which this chat does not silently assert.

## Boundaries and budgets

This chat owns the Join fixtures/gate changes and reached raw/View integration
consumers. Main's flow/arena performance owners remain excluded. Current
main native/bin installation prefixes and old frozen evidence are preserved;
new builds/installations use an identified validation copy. No new worktree,
parallel implementation agent, stage, commit or push is planned.

Static owner gates: 60 s; focused parity: 5 min; integration shard: 30 min.
An exceeded gate is a measured blocker; do not increase an allowance, reduce
the semantic compiler input or weaken a rule to turn it green.

## Reached integration obligations (2026-11-05)

- The Join source reaches the common GraphPlan issuer. Its entrypoint Int
  readiness check incorrectly requires every intermediate CFG block to be a
  terminal return. Validate returns at terminal blocks and forbid return rows
  on intermediate blocks; keep missing/wrong terminal return refusals.
- Native Array.Slice graphs carry source spelling `Slice`, while the Pergyra
  semantic registry carries canonical callable `Array_Slice`. Migrate the
  producer's selected builtin identity, not a spelling-based admission fallback.
  Preserve declared-member identity and malformed-call refusal coverage.
- Native retirement rows are validated by MirCollectionCleanupFacts, but the
  general GraphPlan does not consume them. Source-MIR issuance is a distinct
  obligation from native-MIR consumption. Existing String ownership transitions
  remain the sole authority for their retirement; do not add duplicate drops.
- Full driver fixedpoint is currently blocked before the native oracle can
  emit: borrowed aggregate -> default-parameter calls are immediately refused
  by type_checker_ownership_call, even for a pure getter. The first three
  independent errors are protocol identity getters; the other diagnostics
  follow failed publication. Minimal struct/enum positive and retaining
  negative probes must determine the final-summary/loan repair. Main has been
  notified; its flow/arena profiling owner is not edited by this lane.
- Pergyra source parsing omits unsafe labels accepted by native, and indexed
  assignment type resolution omits Slice. These source-route obligations stay
  explicit; accepting only a rewritten source is diagnostic evidence, not
  closure of the original input.

## Reached String replacement contract

The installed source route now admits the unchanged safe Join program, but
LSan observes 36 bytes from its first iteration on both C and LLVM. A fresh
Array<String> definition overwrites the previous runtime descriptor; the
existing terminal ownership owner retires only the last descriptor. Repair
this general replacement boundary using its admitted ownership transition row:
evaluate the fresh literal first, retire the previous owned descriptor, then
publish the new descriptor. The first definition retires an initialized empty
descriptor; the final routine cleanup still retires the last value. This does
not assert lexical loop-exit retirement or close the raw/native-MIR edge seam.

The existing collection cleanup policy owns eligibility and drop selection
for both emitters. Require a fresh literal and an admitted tracked local;
borrowed/default/value-result parameter storage and moved/retired locals gain
no new release authority. Preserve the existing terminal cleanup decision.
An RHS that reads the old array must finish before release. Verify repeated
definitions, RHS reads, false-loop/early-exit cases, ownership mutations and
real-retention refusal on C and LLVM, with leak and premature-release controls.

The reached RHS falsifier also exposes an old untracked `borrowed-literal`
path: cleanup inferred ownership from expression shape and definition count.
The existing transition issuer must carry borrowed-literal and pure-empty
literal rows as well as owned literals. The replacement consumer requires
that row and selects shallow/deep release from it, never from String spelling
or definition count. Other legacy cleanup families are not declared closed.

Replacement permission is a sealed `replacement_modes` facet of the existing
transition fact. Its issuer checks every typed definition of the local once;
all admitted literal lifetimes must agree with the carried element-release
mode. A terminal state alone grants no replacement authority. Both emitters
consume this facet, covered by the fact digest/readiness and GraphPlan v82.

The RHS control reaches one more existing signature consumer: the single-
routine control-flow/builtin route claims still require `Main -> Void`.
Migrate both to the shared `DirectMirScalarCfgEntrypointReturnTypeSupported`
predicate already used by partition and routine admission. Keep non-Int/Void
entrypoint and wrong terminal-return refusals; add no alternate route.

The source checker publishes UNKNOWN for the Array-element RHS rebind control;
it does not prove borrowed elements there. Keep that input as an explicit
missing-owner refusal, rather than treating it as a borrowed literal. This
does not close general source-level element rebind precision. Verify RHS
evaluation order independently on repeated proved-static literals by observing
the old and fresh allocation alive together (peak 2); an emitted-artifact
mutation that frees before evaluating the fresh literal must fail that check.

The minimal MIR wire may legitimately use its unique SSA value identity instead
of an explicit declaration LocalRef. The consumer must use the admitted
transition row's local identity, not reconstruct a `declaration:id:0` spelling.
The row-index query retains its function identity and moves behind one small
owner to keep the fact file within its existing 180-line gate. Duplicate and
unbalanced identity vectors are executable falsifiers; no dual read is added.
