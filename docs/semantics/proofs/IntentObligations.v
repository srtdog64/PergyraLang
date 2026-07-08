(*
  Pergyra Intent Obligation Unit Correction

  Status: proof sketch for the intent-axis unit correction. This file does not
  prove every intent implementation rule. It fixes the formal claim boundary:
  source-level intent is a binder that elaborates into fact families on the
  verification plane. The non-library-expressibility claim belongs to the
  verifier fact families, not to the word "intent" as one thick atom.
*)

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

Definition binder_emits (_ : VerifierFamily) : Prop := True.

Definition all_verifier_families_emitted : Prop :=
  binder_emits VFParticipant /\
  binder_emits VFCoordination /\
  binder_emits VFBoundary /\
  binder_emits VFAuthority /\
  binder_emits VFEffect /\
  binder_emits VFCompensation.

Theorem intent_binder_emits_all_verifier_families :
  all_verifier_families_emitted.
Proof.
  repeat split; exact I.
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

Theorem intent_binder_inherits_verifier_family_strength :
  all_verifier_families_emitted ->
  bucket_claim BucketBinder = ClaimNonLibraryExpressible.
Proof.
  intros _.
  reflexivity.
Qed.

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
