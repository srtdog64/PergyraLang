(*
  Pergyra Proof-Carrying IR Certificate Core

  Status: proof-sketch; not whole-compiler proof. This models the Stage 1
  pgy.proof-carrying-ir.v1 checker contract:

    valid certificate + valid owner payloads => downstream fact consumption
    missing required certificate fact        => fail closed

  The live adequacy smoke binds this small model to
  docs/semantics/17_proof_carrying_pipeline.md and
  tests/proof_carrying_pipeline_smoke.sh.
*)

Inductive CertLayer : Type :=
  | LayerAIR
  | LayerDAG
  | LayerMIR
  | LayerABI
  | LayerBackend.

Inductive AIRFact : Type :=
  | AirStrictEvidence
  | AirDriftZero
  | AirHIRCfg
  | AirRIRBoundary
  | AirRIRAuthority
  | AirDAGMetadata
  | AirMIRCleanup
  | AirMIRTerminator.

Inductive MIRFact : Type :=
  | MirCFGBlocks
  | MirSourceShape
  | MirExpr0
  | MirCleanup.

Inductive BackendPolicy : Type :=
  | FactOrFailClosed
  | CompatMaySucceed.

Record Certificate : Type := {
  has_layer : CertLayer -> Prop;
  has_air_fact : AIRFact -> Prop;
  has_mir_fact : MIRFact -> Prop;
  backend_policy : BackendPolicy;
  negative_deletion_rejects : Prop
}.

Definition RequiredLayers (c : Certificate) : Prop :=
  has_layer c LayerAIR /\
  has_layer c LayerDAG /\
  has_layer c LayerMIR /\
  has_layer c LayerABI /\
  has_layer c LayerBackend.

Definition RequiredAIRFacts (c : Certificate) : Prop :=
  has_air_fact c AirStrictEvidence /\
  has_air_fact c AirDriftZero /\
  has_air_fact c AirHIRCfg /\
  has_air_fact c AirRIRBoundary /\
  has_air_fact c AirRIRAuthority /\
  has_air_fact c AirDAGMetadata /\
  has_air_fact c AirMIRCleanup /\
  has_air_fact c AirMIRTerminator.

Definition RequiredMIRFacts (c : Certificate) : Prop :=
  has_mir_fact c MirCFGBlocks /\
  has_mir_fact c MirSourceShape /\
  has_mir_fact c MirExpr0 /\
  has_mir_fact c MirCleanup.

Definition ValidCertificate (c : Certificate) : Prop :=
  RequiredLayers c /\
  RequiredAIRFacts c /\
  RequiredMIRFacts c /\
  backend_policy c = FactOrFailClosed /\
  negative_deletion_rejects c.

Definition MayConsumeBackendFacts (c : Certificate) : Prop :=
  ValidCertificate c.

Definition MustFailClosed (c : Certificate) : Prop :=
  ~ ValidCertificate c.

Theorem valid_certificate_allows_backend_consumption :
  forall c, ValidCertificate c -> MayConsumeBackendFacts c.
Proof.
  intros c H. exact H.
Qed.

Theorem missing_air_authority_fails_closed :
  forall c, ~ has_air_fact c AirRIRAuthority -> MustFailClosed c.
Proof.
  unfold MustFailClosed, ValidCertificate, RequiredAIRFacts.
  intros c Hmissing Hvalid.
  destruct Hvalid as [_ [Hair _]].
  destruct Hair as [_ [_ [_ [_ [Hauth _]]]]].
  apply Hmissing. exact Hauth.
Qed.

Theorem missing_mir_expr0_fails_closed :
  forall c, ~ has_mir_fact c MirExpr0 -> MustFailClosed c.
Proof.
  unfold MustFailClosed, ValidCertificate, RequiredMIRFacts.
  intros c Hmissing Hvalid.
  destruct Hvalid as [_ [_ [Hmir _]]].
  destruct Hmir as [_ [_ [Hexpr _]]].
  apply Hmissing. exact Hexpr.
Qed.

Theorem compat_success_policy_fails_closed :
  forall c, backend_policy c = CompatMaySucceed -> MustFailClosed c.
Proof.
  unfold MustFailClosed, ValidCertificate.
  intros c Hcompat Hvalid.
  destruct Hvalid as [_ [_ [_ [Hpolicy _]]]].
  rewrite Hcompat in Hpolicy. discriminate Hpolicy.
Qed.

Theorem negative_deletion_gate_required :
  forall c, ~ negative_deletion_rejects c -> MustFailClosed c.
Proof.
  unfold MustFailClosed, ValidCertificate.
  intros c Hmissing Hvalid.
  destruct Hvalid as [_ [_ [_ [_ Hnegative]]]].
  apply Hmissing. exact Hnegative.
Qed.

Theorem valid_certificate_requires_required_layers :
  forall c, ValidCertificate c -> RequiredLayers c.
Proof.
  intros c Hvalid. destruct Hvalid as [Hlayers _]. exact Hlayers.
Qed.

Theorem valid_certificate_requires_air_and_mir_facts :
  forall c, ValidCertificate c -> RequiredAIRFacts c /\ RequiredMIRFacts c.
Proof.
  intros c Hvalid.
  destruct Hvalid as [_ [Hair [Hmir _]]].
  split; assumption.
Qed.
