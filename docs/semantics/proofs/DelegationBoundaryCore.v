(*
  Pergyra Formal Semantics -- Delegation Boundary Core

  Status: proof-sketch; not whole-language verification.
  Budget: 0 admits / 0 axioms.

  This model separates facts that must not be collapsed:
    - a source declaration attributes purpose and requested capability;
    - trusted authority, complete mediation, and delegability are enforcement
      evidence supplied by their owners, not self-certified source fields;
    - capability possession is not permission to delegate judgment;
    - missing static evidence may reject or retain a checked runtime path, but
      it can never become guessed static success.

  Negative scope: this file does not prove moral legitimacy, human consent,
  compiler correctness, runtime implementation adequacy, or whole-language
  soundness. It defines the narrow permit envelope that those systems must
  refine and enforce.
*)

Require Import Coq.Lists.List.
Import ListNotations.

Section DelegationBoundaryCore.

Definition Principal := nat.
Definition Purpose := nat.
Definition Capability := nat.

Inductive Delegability : Type :=
  | Delegable
  | HumanRequired
  | NonDelegable.

Inductive AuthorityEvidence : Type :=
  | TrustedAuthorityEvidence
  | UntrustedAuthorityEvidence.

Inductive Mediation : Type :=
  | CompleteMediation
  | IncompleteMediation.

Inductive EvidenceState : Type :=
  | StaticEvidence
  | RuntimeEvidence
  | MissingEvidence.

Inductive PermitKind : Type :=
  | StaticPermit
  | RuntimePermit.

Record SourceDeclaration := mkSourceDeclaration {
  declared_principal : Principal;
  declared_purpose : Purpose;
  declared_capabilities : list Capability
}.

Record EnforcementEvidence := mkEnforcementEvidence {
  evidence_delegability : Delegability;
  evidence_authority : AuthorityEvidence;
  evidence_mediation : Mediation
}.

Definition DeclaresCapability
  (declaration : SourceDeclaration) (cap : Capability) : Prop :=
  In cap (declared_capabilities declaration).

Inductive AutomatedPermit
  (declaration : SourceDeclaration) (enforcement : EnforcementEvidence)
  (cap : Capability) : EvidenceState -> bool -> PermitKind -> Prop :=
  | permit_static :
      evidence_delegability enforcement = Delegable ->
      evidence_authority enforcement = TrustedAuthorityEvidence ->
      evidence_mediation enforcement = CompleteMediation ->
      DeclaresCapability declaration cap ->
      AutomatedPermit declaration enforcement cap
        StaticEvidence false StaticPermit
  | permit_runtime :
      forall guard_passed,
      evidence_delegability enforcement = Delegable ->
      evidence_authority enforcement = TrustedAuthorityEvidence ->
      evidence_mediation enforcement = CompleteMediation ->
      DeclaresCapability declaration cap ->
      guard_passed = true ->
      AutomatedPermit declaration enforcement cap
        RuntimeEvidence guard_passed RuntimePermit.

Theorem automated_permit_requires_declared_capability :
  forall declaration enforcement cap evidence guard kind,
    AutomatedPermit declaration enforcement cap evidence guard kind ->
    DeclaresCapability declaration cap.
Proof.
  intros declaration enforcement cap evidence guard kind Hpermit.
  inversion Hpermit; subst; assumption.
Qed.

Theorem automated_permit_requires_trusted_authority_evidence :
  forall declaration enforcement cap evidence guard kind,
    AutomatedPermit declaration enforcement cap evidence guard kind ->
    evidence_authority enforcement = TrustedAuthorityEvidence.
Proof.
  intros declaration enforcement cap evidence guard kind Hpermit.
  inversion Hpermit; subst; assumption.
Qed.

Theorem automated_permit_requires_complete_mediation :
  forall declaration enforcement cap evidence guard kind,
    AutomatedPermit declaration enforcement cap evidence guard kind ->
    evidence_mediation enforcement = CompleteMediation.
Proof.
  intros declaration enforcement cap evidence guard kind Hpermit.
  inversion Hpermit; subst; assumption.
Qed.

Theorem automated_permit_requires_delegable_judgment :
  forall declaration enforcement cap evidence guard kind,
    AutomatedPermit declaration enforcement cap evidence guard kind ->
    evidence_delegability enforcement = Delegable.
Proof.
  intros declaration enforcement cap evidence guard kind Hpermit.
  inversion Hpermit; subst; assumption.
Qed.

Theorem runtime_permit_requires_retained_guard :
  forall declaration enforcement cap guard,
    AutomatedPermit declaration enforcement cap
      RuntimeEvidence guard RuntimePermit ->
    guard = true.
Proof.
  intros declaration enforcement cap guard Hpermit.
  inversion Hpermit; subst; reflexivity.
Qed.

Theorem missing_evidence_never_permits_automation :
  forall declaration enforcement cap guard kind,
    ~ AutomatedPermit declaration enforcement cap
        MissingEvidence guard kind.
Proof.
  intros declaration enforcement cap guard kind Hpermit.
  inversion Hpermit.
Qed.

Theorem human_required_blocks_automated_permit :
  forall declaration enforcement cap evidence guard kind,
    evidence_delegability enforcement = HumanRequired ->
    ~ AutomatedPermit declaration enforcement cap evidence guard kind.
Proof.
  intros declaration enforcement cap evidence guard kind Hhuman Hpermit.
  pose proof (automated_permit_requires_delegable_judgment
    declaration enforcement cap evidence guard kind Hpermit) as Hdelegable.
  rewrite Hhuman in Hdelegable.
  discriminate.
Qed.

Theorem non_delegable_blocks_automated_permit :
  forall declaration enforcement cap evidence guard kind,
    evidence_delegability enforcement = NonDelegable ->
    ~ AutomatedPermit declaration enforcement cap evidence guard kind.
Proof.
  intros declaration enforcement cap evidence guard kind Hnondelegable Hpermit.
  pose proof (automated_permit_requires_delegable_judgment
    declaration enforcement cap evidence guard kind Hpermit) as Hdelegable.
  rewrite Hnondelegable in Hdelegable.
  discriminate.
Qed.

Definition attributed_example : SourceDeclaration :=
  mkSourceDeclaration 1 10 [7].

Definition human_required_evidence : EnforcementEvidence :=
  mkEnforcementEvidence HumanRequired TrustedAuthorityEvidence CompleteMediation.

Theorem authorization_does_not_imply_delegability :
  exists declaration enforcement cap,
    DeclaresCapability declaration cap /\
    evidence_authority enforcement = TrustedAuthorityEvidence /\
    evidence_delegability enforcement = HumanRequired.
Proof.
  exists attributed_example, human_required_evidence, 7.
  split.
  - simpl. left. reflexivity.
  - split; reflexivity.
Qed.

Theorem declared_purpose_does_not_establish_actual_purpose :
  exists actual_purpose,
    declared_purpose attributed_example <> actual_purpose.
Proof.
  exists 11.
  simpl.
  discriminate.
Qed.

End DelegationBoundaryCore.
