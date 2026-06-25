(*
  Pergyra Proof Spine

  Status: proof-spine; not whole-language verification. This file connects the
  proof-pack artifacts as named nodes. It does not import or restate every
  underlying model; the live smoke gate binds each node to the corresponding
  file and theorem names.

  Purpose:
    complete proof spine => every named proof-pack node is present
    complete proof spine => the core-machine, runtime, certificate, and
                             methodology groups are connected
    complete proof spine != whole-language verification
*)

Inductive ProofNode : Type :=
  | NodeSlotCalculus
  | NodeAxisOwnership
  | NodeIntentStepSoundness
  | NodeIRMinimality
  | NodeWitnessDataRace
  | NodeCheckedArith
  | NodeZoneCrossingCore
  | NodeEffectAuthorityCore
  | NodeSlotLifecycleCore
  | NodeAuthorityDelegationCore
  | NodeUnifiedCore
  | NodeCompensationCore
  | NodeCoordinationCore
  | NodeProofCarryingIR
  | NodeVerificationMethodology.

Inductive SpineClaim : Type :=
  | RuntimeSafetyConnected
  | AxisOwnershipConnected
  | IntentCoreConnected
  | UnifiedMachineConnected
  | CertificatePipelineConnected
  | VerificationMethodologyConnected
  | WholeLanguageVerified.

Inductive RemainingObligation : Type :=
  | ObligationPinExceptionalCleanup
  | ObligationParserToAstManifest
  | ObligationBehaviorJudgmentDiagnosticMap
  | ObligationTransitiveFrontierScheduler
  | ObligationAirMirLiveOwnerFactBinding
  | ObligationWindowsLlvmRunnerParity.

Definition HasNode (s : ProofNode -> Prop) (n : ProofNode) : Prop := s n.

Definition ProofSpineComplete (s : ProofNode -> Prop) : Prop :=
  HasNode s NodeSlotCalculus /\
  HasNode s NodeAxisOwnership /\
  HasNode s NodeIntentStepSoundness /\
  HasNode s NodeIRMinimality /\
  HasNode s NodeWitnessDataRace /\
  HasNode s NodeCheckedArith /\
  HasNode s NodeZoneCrossingCore /\
  HasNode s NodeEffectAuthorityCore /\
  HasNode s NodeSlotLifecycleCore /\
  HasNode s NodeAuthorityDelegationCore /\
  HasNode s NodeUnifiedCore /\
  HasNode s NodeCompensationCore /\
  HasNode s NodeCoordinationCore /\
  HasNode s NodeProofCarryingIR /\
  HasNode s NodeVerificationMethodology.

Definition PermitsClaim (s : ProofNode -> Prop) (c : SpineClaim) : Prop :=
  match c with
  | RuntimeSafetyConnected =>
      HasNode s NodeSlotCalculus /\
      HasNode s NodeWitnessDataRace /\
      HasNode s NodeSlotLifecycleCore /\
      HasNode s NodeCheckedArith
  | AxisOwnershipConnected =>
      HasNode s NodeAxisOwnership /\
      HasNode s NodeIRMinimality
  | IntentCoreConnected =>
      HasNode s NodeIntentStepSoundness /\
      HasNode s NodeCompensationCore /\
      HasNode s NodeCoordinationCore
  | UnifiedMachineConnected =>
      HasNode s NodeZoneCrossingCore /\
      HasNode s NodeEffectAuthorityCore /\
      HasNode s NodeSlotLifecycleCore /\
      HasNode s NodeAuthorityDelegationCore /\
      HasNode s NodeUnifiedCore /\
      HasNode s NodeCompensationCore /\
      HasNode s NodeCoordinationCore
  | CertificatePipelineConnected =>
      HasNode s NodeProofCarryingIR /\
      HasNode s NodeIRMinimality
  | VerificationMethodologyConnected =>
      HasNode s NodeVerificationMethodology
  | WholeLanguageVerified => False
  end.

Definition RemainingObligationDischarged
  (d : RemainingObligation -> Prop)
  (o : RemainingObligation) : Prop := d o.

Definition AllRemainingObligationsDischarged
  (d : RemainingObligation -> Prop) : Prop :=
  RemainingObligationDischarged d ObligationPinExceptionalCleanup /\
  RemainingObligationDischarged d ObligationParserToAstManifest /\
  RemainingObligationDischarged d ObligationBehaviorJudgmentDiagnosticMap /\
  RemainingObligationDischarged d ObligationTransitiveFrontierScheduler /\
  RemainingObligationDischarged d ObligationAirMirLiveOwnerFactBinding /\
  RemainingObligationDischarged d ObligationWindowsLlvmRunnerParity.

Definition WholeLanguageVerificationReady
  (s : ProofNode -> Prop)
  (d : RemainingObligation -> Prop) : Prop :=
  ProofSpineComplete s /\ AllRemainingObligationsDischarged d.

Definition HasOpenRemainingObligation
  (d : RemainingObligation -> Prop) : Prop :=
  exists o, ~ RemainingObligationDischarged d o.

Theorem complete_spine_has_node :
  forall s n, ProofSpineComplete s -> HasNode s n.
Proof.
  intros s n Hcomplete.
  destruct n; unfold ProofSpineComplete in Hcomplete; tauto.
Qed.

Theorem complete_spine_connects_runtime_safety :
  forall s, ProofSpineComplete s -> PermitsClaim s RuntimeSafetyConnected.
Proof.
  intros s Hcomplete.
  unfold PermitsClaim.
  repeat split; apply complete_spine_has_node; exact Hcomplete.
Qed.

Theorem complete_spine_connects_axis_ownership :
  forall s, ProofSpineComplete s -> PermitsClaim s AxisOwnershipConnected.
Proof.
  intros s Hcomplete.
  unfold PermitsClaim.
  split; apply complete_spine_has_node; exact Hcomplete.
Qed.

Theorem complete_spine_connects_intent_core :
  forall s, ProofSpineComplete s -> PermitsClaim s IntentCoreConnected.
Proof.
  intros s Hcomplete.
  unfold PermitsClaim.
  repeat split; apply complete_spine_has_node; exact Hcomplete.
Qed.

Theorem complete_spine_connects_unified_machine :
  forall s, ProofSpineComplete s -> PermitsClaim s UnifiedMachineConnected.
Proof.
  intros s Hcomplete.
  unfold PermitsClaim.
  repeat split; apply complete_spine_has_node; exact Hcomplete.
Qed.

Theorem complete_spine_connects_certificate_pipeline :
  forall s, ProofSpineComplete s -> PermitsClaim s CertificatePipelineConnected.
Proof.
  intros s Hcomplete.
  unfold PermitsClaim.
  split; apply complete_spine_has_node; exact Hcomplete.
Qed.

Theorem complete_spine_connects_methodology :
  forall s, ProofSpineComplete s -> PermitsClaim s VerificationMethodologyConnected.
Proof.
  intros s Hcomplete.
  unfold PermitsClaim.
  apply complete_spine_has_node. exact Hcomplete.
Qed.

Theorem complete_spine_is_not_whole_language_verification :
  forall s, ProofSpineComplete s -> ~ PermitsClaim s WholeLanguageVerified.
Proof.
  intros s _ Hwhole.
  exact Hwhole.
Qed.

Theorem whole_language_ready_requires_pin_exceptional_cleanup :
  forall s d,
    WholeLanguageVerificationReady s d ->
    RemainingObligationDischarged d ObligationPinExceptionalCleanup.
Proof.
  unfold WholeLanguageVerificationReady, AllRemainingObligationsDischarged.
  intros s d [_ [Hpin _]].
  exact Hpin.
Qed.

Theorem whole_language_ready_requires_parser_to_ast_manifest :
  forall s d,
    WholeLanguageVerificationReady s d ->
    RemainingObligationDischarged d ObligationParserToAstManifest.
Proof.
  unfold WholeLanguageVerificationReady, AllRemainingObligationsDischarged.
  intros s d [_ [_ [Hparser _]]].
  exact Hparser.
Qed.

Theorem whole_language_ready_requires_behavior_judgment_map :
  forall s d,
    WholeLanguageVerificationReady s d ->
    RemainingObligationDischarged d ObligationBehaviorJudgmentDiagnosticMap.
Proof.
  unfold WholeLanguageVerificationReady, AllRemainingObligationsDischarged.
  intros s d [_ [_ [_ [Hbehavior _]]]].
  exact Hbehavior.
Qed.

Theorem whole_language_ready_requires_transitive_frontier_scheduler :
  forall s d,
    WholeLanguageVerificationReady s d ->
    RemainingObligationDischarged d ObligationTransitiveFrontierScheduler.
Proof.
  unfold WholeLanguageVerificationReady, AllRemainingObligationsDischarged.
  intros s d [_ [_ [_ [_ [Hfrontier _]]]]].
  exact Hfrontier.
Qed.

Theorem whole_language_ready_requires_windows_llvm_runner_parity :
  forall s d,
    WholeLanguageVerificationReady s d ->
    RemainingObligationDischarged d ObligationWindowsLlvmRunnerParity.
Proof.
  unfold WholeLanguageVerificationReady, AllRemainingObligationsDischarged.
  intros s d [_ [_ [_ [_ [_ [_ Hwindows]]]]]].
  exact Hwindows.
Qed.

Theorem whole_language_ready_requires_air_mir_live_owner_binding :
  forall s d,
    WholeLanguageVerificationReady s d ->
    RemainingObligationDischarged d ObligationAirMirLiveOwnerFactBinding.
Proof.
  unfold WholeLanguageVerificationReady, AllRemainingObligationsDischarged.
  intros s d [_ [_ [_ [_ [_ [Hbinding _]]]]]].
  exact Hbinding.
Qed.

Theorem open_obligation_blocks_whole_language_ready :
  forall s d,
    HasOpenRemainingObligation d ->
    ~ WholeLanguageVerificationReady s d.
Proof.
  unfold HasOpenRemainingObligation, WholeLanguageVerificationReady.
  unfold AllRemainingObligationsDischarged, RemainingObligationDischarged.
  intros s d [o Hopen] [_ Hall].
  destruct o; tauto.
Qed.
