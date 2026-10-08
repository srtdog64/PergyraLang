# Proof red-team remediation and reuse closure

Status: **AUDIT REMEDIATION VERIFIED LOCALLY; physical/model-to-runtime closure OPEN**.
Base: `3658548d24bca3d721e4f1974ac7a10da99f7aa8`, preserved dirty `main`
(173 status entries, empty index at entry). User explicitly reopened all
findings in `docs/audits/proof_model_redteam_2026-10-08.md` and requested
parallel repair of the reuse family. The self-host implementation hold is
not lifted by this work.

## Shared objective card and complete chain

- Objective: repair the audit's actual transition/identity defects and
  withdraw claims stronger than their evidence. Reuse must never recover a
  retired identity, right, pending task, or physical ownership obligation.
- Priority: semantic identity and one owner; faithful transitions;
  forbidden-path removal; executable negative evidence; then performance.
- Fact owners: each existing Rocq module's transition relation; the actual
  intent runtime registry and clock builtin/ABI owners for product behavior.
  The audit and this directive are navigation, not additional authorities.
- Last consumers: dependent proof modules, the fresh kernel/API approval
  consumer, real intent/clock runtime callers and their backend declarations.
- Forbidden fallback: resurrection by snapshot, reused numeric identity as
  authority, cancellation treated as termination, guessed empty footprint,
  wrapping identifiers, unchecked zero extent, and definitional evidence
  cited as operational or universal performance proof.
- Gate: focused fresh compilation/kernel checks and permanent falsifiers,
  followed by one fixed-source whole-corpus check and source-bound runtime
  probes. Only the existing two approved Slot abstractions are allowed.

Observed dependency map: PergyraCore -> UnifiedCore / PergyraCoreComposition /
PergyraCoreZoneBridge; WholeProgramCore -> AIRBinding -> BinaryAdequacy;
OwnershipCleanCore -> ownership comparison and GraphLinks -> memory-boundary
consumer. Intent trace enter/leave -> locked active registry -> conflict
waiver and public trace projections. Now builtin registry -> C/LLVM and
self-host signature projections -> runtime export. Async task/scope states
-> close/cancel and parallel merge; scheduler queue/worker facts -> park,
compensation and progress claims. Capability grants -> lend/share/return;
module-owned authority -> requesting module -> resolution. These chains,
not individual first diagnostics, are the integration boundary.

Reached-consumer amendment: CoordinationCore's once-only completion also
reaches WholeProgramCore.ready -> AIRBinding -> BinaryAdequacy.accept.
The integrated gate must refuse re-execution after a real SRun and intervening
steps, preserving completion/NoDup; a fixed local CoordinationCore is not
enough. Lane 2 owns WholeProgramCore/BinaryAdequacy and its typed consumer;
main verifies AIRBinding and the integrated boolean independently.

Clock consumer amendment: full native semantic census exposed datetime.Instant
and TimeSpan still narrowing Now, with reached timer/obligation/device timestamp
consumers. Migrate that timestamp/duration chain to Long (calendar fields,
device addresses/values and Sleep remain Int). Re-check all five native unit
failures on fixed input; do not suppress those failures or install a driver.

LLVM clock-consumer amendment: the C stdlib leg passed, but LLVM rejected
the same timer/obligation/device calls because admitted Int arguments were
left as i32 for Long parameters. The owner is the existing MIR routine or
method parameter signature, not literal spelling. Trace boundary calls,
ordered intent value bindings, hosted self calls and member calls; reuse
the existing numeric store coercion (callable/closure calls already do).
Keep Slot, inout, participant addressing and raw runtime ABI paths unchanged.
Verify signed widening, Float-to-Double, all reached call routes, and rejection
of Long-to-Int or String-to-Long. Then rerun the unchanged C/LLVM stdlib input.

Strict-C consumer amendment: the standalone capability TU includes the new
host clock but its C11 recipe omits the already-owned PLATFORM_CFLAGS, hiding
CLOCK_MONOTONIC. Check all reached standalone runtime C11 profiles; use the
existing platform feature contract, never guessed clock IDs or a realtime
fallback. Slot-scope/capability/budget recipes share this boundary. The MIR
extern-result test must expect Long, not its retired Int result.
Read-only census reached seven direct linked-runtime C11 harnesses missing
their existing linked_runtime_compile_profile_owner and one lane-scheduler
header TU missing its -pthread compile flag. The source-test harness fan-out
also reaches ABI/security TUs without the platform/thread profile. Migrate
those nine consumers; a failed harness compiler must fail, not skip as green;
compile the one common runtime TU once, attribute reuse to the seven profiles,
and execute the lane-scheduler gate. Do not relaunch their installed-driver
matrices or claim self-host substitution from a runtime-profile check.

## Independent edit scopes

All lanes use the same checkout. Preserve unrelated changes, including all
pre-existing dirty proof edits. No worktree, staging, commit/push, artifact
installation, thread messaging, or shared process termination. Never use
`pkill`, `killall`, or name-based process cancellation; isolated compilation
scratch directories have their own finite command lifetime.

1. Identity/reuse lane: C1/C2, M2/M3/M4. IntentConflict, IntentSpine,
   SlotLifecycleCore, CollectionOwnershipTransfer, ForeignStringOwnership;
   the intent runtime and its direct negative tests. Trace the real parent
   identity/leave/conflict consumers before changing representation.
2. Lifecycle/authority lane: B-F1 through B-F10 and C5/C6. PergyraCore,
   UnifiedCore, WholeProgramCore, MachineLayerCore, CapabilityFlowCore,
   ModuleAuthority, PartySlotBinding, EvidenceLifecycleCore,
   BindingIdentityScope, ResourceMachineBridge, DelegationBoundaryCore,
   AxisOwnership, AuthorityIrreducibility, ReadingConfluence. Include reached
   PergyraCore dependent proofs when signatures change. Distinguish interface
   contracts from derived safety; no replacement proof holes.
3. Async lane: A-F1 through A-F11 (except the clock A-F12). AsyncScopeCore,
   AsyncLifecycleCore, ParallelSchedulingCore, WitnessDataRace,
   CoordinationCore, AsyncContextCore, CompensationCore and direct model
   docs/tests. Cancellation remains pending until a completion/drain edge;
   scope reuse requires retired ancestry or a generation.
4. Main integration: A-F12, C3/C4/C7, M5 and the already repaired M1 / graph
   frame finding; clock ABI chain, GuardWitnessBinding, IRMinimality,
   IntentObligations, AIRBinding, OwnershipCleanGCComparison. Main alone
   edits shared indexes, handoff, aggregate regression gate/receipt, and
   the pergyra-authoring skill. The requested executable footprint/placement/
   growth/retirement formal closure follows these admitted repaired owners;
   it must not be declared done from the existing conditional frame proof.

No lane edits another lane's owners or shared documentation. Report any newly
reached dependent owner before expanding a patch. Model-local documentation
may be corrected in its lane; shared citations are reported to main.

## Validation and disposition

Commands: read-only Git/status/hash checks; apply_patch; isolated pinned
Rocq 9.3.0 / Stdlib 9.2.0 compilation and rocqchk; existing focused Bash/C
gates. Static budget 60 seconds, focused budget 5 minutes, integration shard
30 minutes. Do not enlarge time/memory allowances or weaken predicates to
make evidence green. Each lane returns exact files, finding IDs, observed
gates, next falsifier, and unresolved refinement boundaries.

Main is the integration owner. The shared integration gate is
`tests/formal_semantics_smoke.sh` plus a permanent red-team regression
consumer through the same fresh kernel gate. Runtime changes additionally
need direct source-bound positive and negative probes; a model PASS is not
runtime parity. Audit dispositions distinguish REPAIRED, CLAIM CORRECTED,
ALREADY REPAIRED VERIFIED, and OPEN. New source-bound findings are revisited
in this plan before implementation. Deliverables are implementation candidates
until main independently observes the integration result.

## Observed integration receipt

Main independently checked the frozen 63-owner + six-regression snapshot and
approved-assumption consumer with Rocq 9.3.0 / rocqchk. Current hashes of all
69 inputs match the observed run. Source-bound intent/clock probes, fresh
native C/LLVM clock and admitted-argument execution, unchanged stdlib input,
3107 semantic, 1006 transpile and 217 MIR tests pass. The MIR integration
target's reached facts/render gates also pass; existing warnings remain visible.
Standalone slot-scope/capability/budget and lane-scheduler execution pass.
Strict C11 profile checks compile the common runtime TU once for seven profile
consumers; the root source harness compiles 18 TUs, not 18 test executions.
Fresh certificate extraction agrees on 262144 finite valuations, four length
controls and 41 envelope/owner controls. No new assumption was approved.
The existing full self-host structural gate still returns 1 after its checker
subgate; installed-driver and remote CI were not claimed or changed.

Disposition: 24 bounded-model/runtime repairs, nine claim corrections, two
already repaired findings reverified. Exact observations, source hashes,
remaining algorithm/refinement obligations and skill changes are in
[the integrated audit receipt](../audits/proof_model_redteam_remediation_2026-10-08.md).
This directive does not lift the self-host hold or close the still conditional
footprint/placement/growth/retirement execution algorithm.
