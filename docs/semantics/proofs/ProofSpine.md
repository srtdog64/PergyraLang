# Proof Spine

Status: `proof-spine`

`ProofSpine.v` connects the proof pack as named nodes. It is not a new semantic
model and it does not claim whole-language verification. The individual Coq
files keep owning their local models; the spine records which groups must be
present before a proof-pack-level claim can be made.

## Connected Groups

- runtime safety: `SlotCalculus.v`, `WitnessDataRace.v`,
  `SlotLifecycleCore.v`, and `CheckedArith.v`;
- axis ownership: `AxisOwnership.v`, `IRMinimality.v`, and
  `AuthorityIrreducibility.v`;
- intent core: `IntentStepSoundness.v`, `CompensationCore.v`,
  `CoordinationCore.v`, `IntentObligations.v`, `IntentSpine.v`, and
  `IntentConflict.v`;
- unified machine: `ZoneCrossingCore.v`, `EffectAuthorityCore.v`,
  `SlotLifecycleCore.v`, `AuthorityDelegationCore.v`, `UnifiedCore.v`,
  `CompensationCore.v`, `CoordinationCore.v`, and `WholeProgramCore.v`;
- formal kernel: `FormalKernel.v`, `WholeProgramCore.v`, and `AIRBinding.v`;
- basis selection: `BasisCompleteness.v`, `FormalKernel.v`, and
  `AxisOwnership.v`;
- certificate pipeline: `ProofCarryingIR.v` and `IRMinimality.v`;
- verification methodology: `VerificationMethodology.v`.

## Negative Boundary

The capstone theorem is deliberately negative:

```text
complete proof spine != whole-language verification
```

This keeps the proof pack honest. A complete spine means the named proof
artifacts are present and connected. It does not mean the full compiler,
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

`tests/proof_spine_smoke.sh` binds the Coq spine to live files by requiring:

- every proof node file exists;
- the representative theorem names still exist in their owner files;
- `FormalKernel.v` keeps source vocabulary bound to named kernel primitives
  and owner facts without permitting a whole-language proof claim;
- `BasisCompleteness.v` keeps the first M2 basis-completeness fragment tied to
  the proof pack: static bigraph place/link fragments encode into Pergyra axes,
  and channel-free paths preserve world roots;
- `IntentObligations.v` keeps the unit correction for `intent` tied to the
  proof pack: source-level `intent` is a binder that emits verifier fact
  families; purpose and trace stay outside the non-library-expressibility
  claim;
- `IntentSpine.v` keeps the operational intent kernel tied to the proof pack:
  participant coverage, coordination DAG, and compensation coverage imply
  `checked_intent_guard_free`, fact-family reassembly, and checked-intent guard
  erasure;
- `IntentConflict.v` keeps the cross-intent conflict kernel tied to the proof
  pack: static separation evidence makes the runtime admission conflict guard
  unfireable, while priority alone is not separation evidence;
- `AuthorityIrreducibility.v` keeps the authority-axis claim tied to the proof
  pack: delegation history distinguishes configurations that have identical
  capability and zone projections;
- every remaining obligation above is named by the Coq spine and this document;
- `formal_semantics_smoke.sh` type-checks `ProofSpine.v` when `coqc` exists;
- the proof-pack README and language golden spine cite the top-level spine.
