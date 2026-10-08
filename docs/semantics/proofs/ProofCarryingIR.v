(*
  Pergyra Proof-Carrying IR Certificate Core

  Scope: not a whole-compiler proof. This models the Stage 1
  pgy.proof-carrying-ir.v1 checker contract:

    valid certificate + valid owner payloads => downstream fact consumption
    missing required certificate fact        => fail closed

  The adequacy smoke compiles and extracts this finite checker, then compares
  every 18-bit input with the executable envelope admission core. Hash/JSON
  admission and producer correctness are separate obligations.
*)

Require Import Stdlib.Bool.Bool Stdlib.Lists.List Stdlib.Arith.PeanoNat.

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
  backend_policy : BackendPolicy
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
  backend_policy c = FactOrFailClosed.

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
  destruct Hvalid as [_ [_ [_ Hpolicy]]].
  rewrite Hcompat in Hpolicy. discriminate Hpolicy.
Qed.

(* Deletion refusal is a property of the checker, not an untrusted certificate
   field that can assert its own negative tests passed. *)
Theorem negative_deletion_gate_required :
  forall c,
  (~ RequiredLayers c \/ ~ RequiredAIRFacts c \/ ~ RequiredMIRFacts c) ->
  MustFailClosed c.
Proof.
  unfold MustFailClosed, ValidCertificate.
  intros c Hmissing Hvalid. tauto.
Qed.

(* The executable core has exactly 18 finite decisions: five layers, eight AIR
   facts, four MIR facts and one backend policy. JSON shape, byte binding and
   manifest-only layer status are checked before/around this core. *)
Record CertificateInput : Type := {
  layer_bit : CertLayer -> bool;
  air_bit : AIRFact -> bool;
  mir_bit : MIRFact -> bool;
  input_policy : BackendPolicy
}.

Definition logical_certificate (c : CertificateInput) : Certificate := {|
  has_layer := fun l => layer_bit c l = true;
  has_air_fact := fun f => air_bit c f = true;
  has_mir_fact := fun f => mir_bit c f = true;
  backend_policy := input_policy c
|}.

Definition certificate_check (c : CertificateInput) : bool :=
  (layer_bit c LayerAIR && layer_bit c LayerDAG && layer_bit c LayerMIR &&
   layer_bit c LayerABI && layer_bit c LayerBackend) &&
  (air_bit c AirStrictEvidence && air_bit c AirDriftZero &&
   air_bit c AirHIRCfg && air_bit c AirRIRBoundary &&
   air_bit c AirRIRAuthority && air_bit c AirDAGMetadata &&
   air_bit c AirMIRCleanup && air_bit c AirMIRTerminator) &&
  (mir_bit c MirCFGBlocks && mir_bit c MirSourceShape &&
   mir_bit c MirExpr0 && mir_bit c MirCleanup) &&
  (match input_policy c with FactOrFailClosed => true | CompatMaySucceed => false end).

Theorem checker_reflects_certificate_validity : forall c,
  certificate_check c = true <-> ValidCertificate (logical_certificate c).
Proof.
  intros [hl ha hm p]. destruct p;
    unfold certificate_check, ValidCertificate, RequiredLayers,
      RequiredAIRFacts, RequiredMIRFacts, logical_certificate; simpl;
    repeat rewrite andb_true_iff; intuition discriminate.
Qed.

Definition layer_index (l : CertLayer) : nat :=
  match l with LayerAIR => 0 | LayerDAG => 1 | LayerMIR => 2 |
               LayerABI => 3 | LayerBackend => 4 end.
Definition air_index (f : AIRFact) : nat :=
  match f with AirStrictEvidence => 5 | AirDriftZero => 6 | AirHIRCfg => 7 |
    AirRIRBoundary => 8 | AirRIRAuthority => 9 | AirDAGMetadata => 10 |
    AirMIRCleanup => 11 | AirMIRTerminator => 12 end.
Definition mir_index (f : MIRFact) : nat :=
  match f with MirCFGBlocks => 13 | MirSourceShape => 14 |
               MirExpr0 => 15 | MirCleanup => 16 end.

Definition input_from_bits (bits : list bool) : CertificateInput := {|
  layer_bit := fun l => nth (layer_index l) bits false;
  air_bit := fun f => nth (air_index f) bits false;
  mir_bit := fun f => nth (mir_index f) bits false;
  input_policy := if nth 17 bits false then FactOrFailClosed else CompatMaySucceed
|}.

Definition check_bits (bits : list bool) : bool :=
  Nat.eqb (length bits) 18 && certificate_check (input_from_bits bits).

Theorem bit_checker_reflects_certificate_validity : forall bits,
  check_bits bits = true <->
  length bits = 18 /\ ValidCertificate (logical_certificate (input_from_bits bits)).
Proof.
  intro bits. unfold check_bits.
  rewrite andb_true_iff, Nat.eqb_eq, checker_reflects_certificate_validity.
  reflexivity.
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
