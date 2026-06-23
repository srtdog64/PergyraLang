(*
  Pergyra Verification Methodology Core

  Status: proof-sketch; not whole-language proof. This file models the
  evidence-ladder discipline from docs/139_golden_adt_verification_methodology.md:

    a method permits only the claim it is strong enough to support;
    golden fixtures, differential oracles, verifier gates, and mechanized
    models are not aliases for each other.

  The live smoke gate binds this small model to the methodology document and
  the proof-pack index.
*)

Inductive Method : Type :=
  | ProseContract
  | SmokeGate
  | GoldenFixture
  | DifferentialOracle
  | PropertyMetamorphic
  | VerifierGate
  | MechanizedModel
  | ADTOwner
  | TypestateAnalysis
  | CapabilityEvidence
  | TraceEvidence
  | AbstractInterpretation
  | ModelChecking.

Inductive Claim : Type :=
  | ReviewableContract
  | DriftDetected
  | OutputShapeStable
  | ImplementationParity
  | SampledLaw
  | FactConsumptionSound
  | ModelSoundness
  | HardSelfHostSlice
  | LayoutNicheSoundness
  | RuntimeMaterializationAllowed.

Definition Has (s : Method -> Prop) (m : Method) : Prop := s m.

Definition permits (s : Method -> Prop) (c : Claim) : Prop :=
  match c with
  | ReviewableContract => Has s ProseContract
  | DriftDetected => Has s SmokeGate
  | OutputShapeStable => Has s GoldenFixture
  | ImplementationParity => Has s DifferentialOracle
  | SampledLaw => Has s PropertyMetamorphic
  | FactConsumptionSound => Has s VerifierGate /\ Has s ADTOwner
  | ModelSoundness => Has s MechanizedModel
  | HardSelfHostSlice =>
      Has s ADTOwner /\
      Has s GoldenFixture /\
      Has s DifferentialOracle /\
      Has s VerifierGate /\
      Has s SmokeGate
  | LayoutNicheSoundness =>
      Has s ADTOwner /\
      Has s VerifierGate /\
      Has s GoldenFixture /\
      Has s TypestateAnalysis
  | RuntimeMaterializationAllowed =>
      Has s TraceEvidence /\
      Has s CapabilityEvidence /\
      Has s VerifierGate
  end.

Definition golden_only (s : Method -> Prop) : Prop :=
  Has s GoldenFixture /\ forall m, m <> GoldenFixture -> ~ Has s m.

Definition smoke_only (s : Method -> Prop) : Prop :=
  Has s SmokeGate /\ forall m, m <> SmokeGate -> ~ Has s m.

Theorem golden_only_not_model_soundness :
  forall s, golden_only s -> ~ permits s ModelSoundness.
Proof.
  unfold golden_only, permits, Has.
  intros s [_ Honly] Hmodel.
  apply (Honly MechanizedModel).
  - discriminate.
  - exact Hmodel.
Qed.

Theorem golden_only_not_hard_self_host_slice :
  forall s, golden_only s -> ~ permits s HardSelfHostSlice.
Proof.
  unfold golden_only, permits, Has.
  intros s [_ Honly] Hhard.
  destruct Hhard as [_ [_ [Hdiff _]]].
  apply (Honly DifferentialOracle).
  - discriminate.
  - exact Hdiff.
Qed.

Theorem smoke_only_not_hard_self_host_slice :
  forall s, smoke_only s -> ~ permits s HardSelfHostSlice.
Proof.
  unfold smoke_only, permits, Has.
  intros s [_ Honly] Hhard.
  destruct Hhard as [Howner _].
  apply (Honly ADTOwner).
  - discriminate.
  - exact Howner.
Qed.

Theorem mechanized_model_not_implementation_parity :
  exists s, permits s ModelSoundness /\ ~ permits s ImplementationParity.
Proof.
  exists (fun m => m = MechanizedModel).
  split.
  - unfold permits, Has. reflexivity.
  - unfold permits, Has. intros Hdiff. discriminate Hdiff.
Qed.

Theorem differential_not_model_soundness :
  exists s, permits s ImplementationParity /\ ~ permits s ModelSoundness.
Proof.
  exists (fun m => m = DifferentialOracle).
  split.
  - unfold permits, Has. reflexivity.
  - unfold permits, Has. intros Hmodel. discriminate Hmodel.
Qed.

Theorem hard_self_host_requires_differential :
  forall s, permits s HardSelfHostSlice -> Has s DifferentialOracle.
Proof.
  unfold permits, Has.
  intros s Hhard.
  destruct Hhard as [_ [_ [Hdiff _]]].
  exact Hdiff.
Qed.

Theorem hard_self_host_requires_verifier :
  forall s, permits s HardSelfHostSlice -> Has s VerifierGate.
Proof.
  unfold permits, Has.
  intros s Hhard.
  destruct Hhard as [_ [_ [_ [Hverifier _]]]].
  exact Hverifier.
Qed.

Theorem hard_self_host_requires_owner :
  forall s, permits s HardSelfHostSlice -> Has s ADTOwner.
Proof.
  unfold permits, Has.
  intros s Hhard.
  destruct Hhard as [Howner _].
  exact Howner.
Qed.

Theorem layout_niche_requires_typestate :
  forall s, permits s LayoutNicheSoundness -> Has s TypestateAnalysis.
Proof.
  unfold permits, Has.
  intros s Hlayout.
  destruct Hlayout as [_ [_ [_ Htype]]].
  exact Htype.
Qed.

Theorem layout_niche_requires_verifier :
  forall s, permits s LayoutNicheSoundness -> Has s VerifierGate.
Proof.
  unfold permits, Has.
  intros s Hlayout.
  destruct Hlayout as [_ [Hverifier _]].
  exact Hverifier.
Qed.

Theorem materialization_requires_trace_and_capability :
  forall s,
    permits s RuntimeMaterializationAllowed ->
    Has s TraceEvidence /\ Has s CapabilityEvidence.
Proof.
  unfold permits, Has.
  intros s Hmaterialized.
  destruct Hmaterialized as [Htrace [Hcap _]].
  split; assumption.
Qed.

Theorem materialization_requires_verifier :
  forall s,
    permits s RuntimeMaterializationAllowed ->
    Has s VerifierGate.
Proof.
  unfold permits, Has.
  intros s Hmaterialized.
  destruct Hmaterialized as [_ [_ Hverifier]].
  exact Hverifier.
Qed.

Theorem verifier_with_owner_permits_fact_consumption :
  forall s,
    Has s VerifierGate ->
    Has s ADTOwner ->
    permits s FactConsumptionSound.
Proof.
  unfold permits, Has.
  intros s Hverifier Howner.
  split; assumption.
Qed.

