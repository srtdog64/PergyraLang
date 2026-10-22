# Proof Spine

Status: `proof-spine`

`ProofSpine.v` connects the proof pack as named nodes. It is not a new semantic
model and it does not claim whole-language verification. The individual Coq
files keep owning their local models. The schematic spine records which
propositions a claim requires; an arbitrary `ProofNode -> Prop` is not evidence
that files exist or that their theorems have any particular type. The concrete
cleanup instantiation below imports and checks the actual model contracts.

## Connected Groups

- runtime safety: `SlotCalculus.v`, `WitnessDataRace.v`,
  `SlotLifecycleCore.v`, and `CheckedArith.v`;
- axis ownership: `AxisOwnership.v`, `IRMinimality.v`, and
  `AuthorityIrreducibility.v`;
- intent core: `IntentStepSoundness.v`, `CompensationCore.v`,
  `CoordinationCore.v`, `IntentObligations.v`, `IntentSpine.v`, and
  `IntentConflict.v`;
- unified machine: `ZoneCrossingCore.v`, `EffectAuthorityCore.v`,
  `SlotLifecycleCore.v`, `MachineLayerCore.v`, `AuthorityDelegationCore.v`,
  `UnifiedCore.v`, `CompensationCore.v`, `CoordinationCore.v`, and
  `WholeProgramCore.v`;
- architecture boundary: `DelegationBoundaryCore.v`,
  `LossCompositionCore.v`, `ResourceMachineBridge.v`, `MachineLayerCore.v`,
  and `AuthorityDelegationCore.v`;
- formal kernel: `FormalKernel.v`, `WholeProgramCore.v`, and `AIRBinding.v`;
- basis selection: `BasisCompleteness.v`, `FormalKernel.v`, and
  `AxisOwnership.v`;
- certificate pipeline: `ProofCarryingIR.v` and `IRMinimality.v`;
- verification methodology: `VerificationMethodology.v`;
- bounded-rung SoT authority: `SoTAuthority.v`;
- structured async: `AsyncLifecycleCore.v` and `AsyncContextCore.v`. This
  connects named-Future containment to task-context carriage without making
  the `async` marker own either responsibility;
- async direction: `AsyncScopeCore.v`, `CapabilityFlowCore.v`,
  `SuspensionRevalidationCore.v`, and `DeterministicSubsetCore.v`, each
  connected to the landed node it refines (`AsyncLifecycleCore.v`,
  `AsyncContextCore.v`, `SlotCalculus.v`, `ParallelReductionCore.v`). These
  model the disciplines `docs/204` adopts above the landed contracts; the
  spine proves the claim discharges no remaining obligation;
- ordinary-value cleanup: `OwnershipCleanCore.v`,
  `OwnershipCleanComposition.v`, `OwnershipCleanReadOnly.v`, and
  `OwnershipCleanExits.v`. The independent consumer
  `tests/coq/OwnershipCleanupSpineAudit.v` assigns these nodes concrete
  propositions and supplies their proofs from the imported owners.

## Negative Boundary

The capstone theorem is deliberately negative:

```text
complete proof spine != whole-language verification
```

Completeness of the schematic spine means its input propositions hold, not
that every proof in the repository has been bound to an implementation.
It does not mean the full compiler,
runtime, C backend, LLVM backend, or self-hosted implementation has been
formally verified.

## Remaining Obligations

The proof spine intentionally keeps these obligations open until the named
implementation and gate work exists:

| Obligation | Current status | Required closure |
| --- | --- | --- |
| `ObligationPinExceptionalCleanup` | Lexical pin blocks have normal CFG cleanup evidence, but exceptional exit and cancellation cleanup are still a proof obligation. | DropOnce / ReleaseAfterUnpin must cover all exceptional and cancellation exits in C and LLVM lowering. |
| `ObligationParserToAstManifest` | `parser_to_ast` is tracked in the loss manifest as documentation-only. | Move the parser boundary into the pass manifest and add a gate that checks accepted loss and forbidden recovery. |
| `ObligationBehaviorJudgmentDiagnosticMap` | Behavior-contract gaps are documented, but judgment rules are not yet mapped 1:1 to diagnostics. | Stable judgment rule ids must map to compiler diagnostic codes and verifier causes. |
| `ObligationTransitiveFrontierScheduler` | Frontier slices are implemented and smoke-tested, but the full transitive world/zone/projection scheduler is not closed. | One source-of-truth scheduler must cover world, zone, projection, authority, failure, and handoff propagation. |
| `ObligationAirMirLiveOwnerFactBinding` | Coq models name graphs, holdings, compensation targets, and snapshots as parameters. | Bind those terms to live AIR/MIR owner facts so implementation gates prove the same facts the model assumes. |
| `ObligationWindowsLlvmRunnerParity` | Windows beta policy remains C backend only for strict platform claims. | Windows LLVM runner parity must be proven before Windows LLVM can be cited as a beta-stable leg. |

The Coq spine models these as `RemainingObligation` constructors. A proof pack
can be connected while still failing `WholeLanguageVerificationReady`; any open
remaining obligation blocks that stronger claim.

## Live Adequacy Boundary

`tests/proof_spine_smoke.sh` compiles fresh copies of the spine, its four
cleanup dependencies and the independent typed consumer, then runs `rocqchk`
with no admitted axioms. It checks:

- the core cleanup conclusion has the original trace and an empty target heap;
- normalization preserves the complete target execution relation, not just output;
- readonly elision retains original admission and source execution premises;
- exit cleanup preserves the exit outcome and frees the closed program's heap;
- omitting any of those four concrete contracts prevents the cleanup claim;
- a proof of `True` cannot substitute for either cleanup contract.

The production `tests/coq_kernel_check.sh` includes that consumer in its fresh
whole-corpus kernel check. `tests/ownership_cleanup_smoke.sh` separately extracts
and executes the algorithm's positive/refusal controls. The extraction uses
OCaml as a test harness, not as a new Pergyra implementation language.

These are model and typed-interface checks, not parser, allocator, native
codegen or installed-driver refinement. Graph ownership and resource teardown
have their own importing proofs and executable gates; their presence is not
inferred from this ordinary-value cleanup group. The full corpus is larger
than this representative spine.

No source-text/theorem-name existence scan establishes this boundary. An
explicit missing-Rocq skip is reported as unchecked, never as proof evidence.
