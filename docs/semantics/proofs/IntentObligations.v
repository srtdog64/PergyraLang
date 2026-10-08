(*
  Pergyra Intent Obligation Unit Correction

  Status: checked taxonomy and finite emission-admission interface. This file does not
  prove every intent implementation rule. It fixes the formal claim boundary:
  source-level intent is a binder that elaborates into fact families on the
  verification plane. The non-library-expressibility claim belongs to the
  verifier fact families, not to the word "intent" as one thick atom.
  ClaimClass labels are architectural declarations, NOT proofs against every
  library encoding. No compiler emitter is refined by this interface.
*)

Require Import Stdlib.Lists.List.
Require Import Stdlib.Bool.Bool.
Import ListNotations.

Inductive IntentBucket : Type :=
  | BucketBinder
  | BucketVerifierFamily
  | BucketPurpose
  | BucketTrace.

Inductive VerifierFamily : Type :=
  | VFParticipant
  | VFCoordination
  | VFBoundary
  | VFAuthority
  | VFEffect
  | VFCompensation.

Inductive IntentSubfact : Type :=
  | IntentPurposeFact
  | IntentParticipantFact
  | IntentCoordinationFact
  | IntentBoundaryFact
  | IntentAuthorityFact
  | IntentEffectFact
  | IntentCompensationFact
  | IntentTraceFact.

Inductive ClaimClass : Type :=
  | ClaimNonLibraryExpressible
  | ClaimLibraryExpressible
  | ClaimHumanMeaning.

Definition family_claim (_ : VerifierFamily) : ClaimClass :=
  ClaimNonLibraryExpressible.

Definition bucket_claim (b : IntentBucket) : ClaimClass :=
  match b with
  | BucketBinder => ClaimNonLibraryExpressible
  | BucketVerifierFamily => ClaimNonLibraryExpressible
  | BucketPurpose => ClaimHumanMeaning
  | BucketTrace => ClaimLibraryExpressible
  end.

Definition subfact_bucket (f : IntentSubfact) : IntentBucket :=
  match f with
  | IntentPurposeFact => BucketPurpose
  | IntentParticipantFact => BucketVerifierFamily
  | IntentCoordinationFact => BucketVerifierFamily
  | IntentBoundaryFact => BucketVerifierFamily
  | IntentAuthorityFact => BucketVerifierFamily
  | IntentEffectFact => BucketVerifierFamily
  | IntentCompensationFact => BucketVerifierFamily
  | IntentTraceFact => BucketTrace
  end.

Definition family_eqb (a b : VerifierFamily) : bool :=
  match a, b with
  | VFParticipant, VFParticipant | VFCoordination, VFCoordination
  | VFBoundary, VFBoundary | VFAuthority, VFAuthority
  | VFEffect, VFEffect | VFCompensation, VFCompensation => true
  | _, _ => false
  end.

Lemma family_eqb_spec : forall a b, family_eqb a b = true <-> a = b.
Proof. destruct a, b; simpl; split; intros H; try reflexivity; discriminate. Qed.

Definition required_verifier_families : list VerifierFamily :=
  [VFParticipant; VFCoordination; VFBoundary; VFAuthority; VFEffect; VFCompensation].
Definition binder_emits (emitted : list VerifierFamily) (f : VerifierFamily) : Prop :=
  In f emitted.
Definition all_verifier_families_emitted (emitted : list VerifierFamily) : Prop :=
  forall f, In f required_verifier_families -> binder_emits emitted f.
Definition emitted_allb (emitted : list VerifierFamily) : bool :=
  forallb (fun f => existsb (family_eqb f) emitted) required_verifier_families.

Theorem intent_binder_emits_all_verifier_families :
  forall emitted, emitted_allb emitted = true <-> all_verifier_families_emitted emitted.
Proof.
  intros emitted. unfold emitted_allb, all_verifier_families_emitted, binder_emits.
  rewrite forallb_forall. split; intros H f HF.
  - specialize (H f HF). apply existsb_exists in H.
    destruct H as [f' [HI HE]]. apply family_eqb_spec in HE. subst f'. exact HI.
  - apply existsb_exists. exists f. split; [apply H; exact HF|].
    apply family_eqb_spec. reflexivity.
Qed.

Theorem verifier_families_are_nonexpressibility_units :
  forall f, family_claim f = ClaimNonLibraryExpressible.
Proof.
  destruct f; reflexivity.
Qed.

Theorem purpose_trace_outside_nonexpressibility_claim :
  bucket_claim BucketPurpose <> ClaimNonLibraryExpressible /\
  bucket_claim BucketTrace <> ClaimNonLibraryExpressible.
Proof.
  simpl; split; discriminate.
Qed.

Theorem verifier_subfacts_are_claim_units :
  forall f,
    subfact_bucket f = BucketVerifierFamily ->
    bucket_claim (subfact_bucket f) = ClaimNonLibraryExpressible.
Proof.
  intros f Hbucket.
  rewrite Hbucket.
  reflexivity.
Qed.

Theorem purpose_trace_subfacts_outside_claim :
  bucket_claim (subfact_bucket IntentPurposeFact) <> ClaimNonLibraryExpressible /\
  bucket_claim (subfact_bucket IntentTraceFact) <> ClaimNonLibraryExpressible.
Proof.
  simpl; split; discriminate.
Qed.

Definition binder_strength (emitted : list VerifierFamily) : option ClaimClass :=
  if emitted_allb emitted then Some (bucket_claim BucketBinder) else None.

Theorem intent_binder_inherits_verifier_family_strength : forall emitted,
  all_verifier_families_emitted emitted ->
  binder_strength emitted = Some ClaimNonLibraryExpressible.
Proof.
  intros emitted H. apply intent_binder_emits_all_verifier_families in H.
  unfold binder_strength. rewrite H. reflexivity.
Qed.

Example complete_emission_is_admitted :
  binder_strength required_verifier_families = Some ClaimNonLibraryExpressible.
Proof. reflexivity. Qed.

Example compensation_missing_is_refused :
  binder_strength [VFParticipant; VFCoordination; VFBoundary; VFAuthority; VFEffect] = None.
Proof. reflexivity. Qed.

Example absent_emission_is_refused : binder_strength [] = None.
Proof. reflexivity. Qed.

Definition atomic_intent_fact_permitted : Prop := False.

Theorem no_atomic_intent_fact :
  ~ atomic_intent_fact_permitted.
Proof.
  unfold atomic_intent_fact_permitted.
  tauto.
Qed.

Inductive IntentWorkItem : Type :=
  | WO_INT_0_family_naming
  | WO_INT_1_participant_declared_used
  | WO_INT_2_compensation_coverage
  | WO_INT_3_coordination_dag
  | WO_INT_4_cross_intent_conflict
  | WO_INT_5_guard_free_erasure.

Definition work_precedes (a b : IntentWorkItem) : Prop :=
  match a, b with
  | WO_INT_0_family_naming, WO_INT_1_participant_declared_used => True
  | WO_INT_1_participant_declared_used, WO_INT_2_compensation_coverage => True
  | WO_INT_2_compensation_coverage, WO_INT_3_coordination_dag => True
  | WO_INT_3_coordination_dag, WO_INT_4_cross_intent_conflict => True
  | WO_INT_4_cross_intent_conflict, WO_INT_5_guard_free_erasure => True
  | _, _ => False
  end.

Theorem int0_precedes_participant_declared_used :
  work_precedes WO_INT_0_family_naming WO_INT_1_participant_declared_used.
Proof.
  exact I.
Qed.
