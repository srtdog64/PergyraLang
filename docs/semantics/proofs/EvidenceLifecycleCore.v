(*
  Pergyra evidence-lifecycle compression core.

  This is a bounded model of the architecture rule owned by
  docs/semantics/09_abstraction_loss_contracts.md:

    prove once; carry the authority established by the proof;
    erase construction evidence after its last semantic consumer.

  It does not prove that the live compiler classifies every artifact correctly,
  that its payload size decreases in bytes, or that a backend implements the
  modeled projection.  The companion adequacy gate only binds this vocabulary
  to the owner document, proof registration, and the live AIR evidence-lifetime
  manifest.

  Budget: 0 admits / 0 axioms.
*)

Require Import Coq.Arith.PeanoNat.

Section EvidenceLifecycleCore.

Inductive EvidenceClass : Type :=
  | IdentityEvidence
  | ValidityEvidence
  | DiagnosticEvidence
  | ConstructionEvidence.

Inductive EvidenceDisposition : Type :=
  | Retain
  | Reference
  | Summarize
  | Materialize
  | Erase.

Record LifecycleContext : Type := {
  admitted : bool;
  before_last_consumer : bool;
  authority_required : bool;
  runtime_required : bool;
  diagnostic_required : bool;
  would_redecide_without_carrier : bool
}.

(* A projection decides the lifetime of the rich evidence payload separately
   from the compact authority carrier established by that evidence.  This split
   is the central point of the model: Erase does not imply that downstream must
   rediscover the semantic decision. *)
Record EvidenceProjection : Type := {
  disposition : EvidenceDisposition;
  carries_established_authority : bool
}.

Definition project_evidence
  (kind : EvidenceClass)
  (context : LifecycleContext) : EvidenceProjection :=
  match kind with
  | IdentityEvidence =>
      if admitted context then
        if runtime_required context then
          {| disposition := Materialize;
             carries_established_authority := authority_required context |}
        else if authority_required context then
          {| disposition := Reference;
             carries_established_authority := true |}
        else
          {| disposition := Erase;
             carries_established_authority := false |}
      else
        {| disposition := Retain;
           carries_established_authority := false |}
  | ValidityEvidence =>
      if admitted context then
        if runtime_required context then
          {| disposition := Materialize;
             carries_established_authority := authority_required context |}
        else if authority_required context then
          {| disposition := Summarize;
             carries_established_authority := true |}
        else
          {| disposition := Erase;
             carries_established_authority := false |}
      else
        {| disposition := Retain;
           carries_established_authority := false |}
  | DiagnosticEvidence =>
      if diagnostic_required context then
        {| disposition := Reference;
           carries_established_authority := false |}
      else
        {| disposition := Erase;
           carries_established_authority := false |}
  | ConstructionEvidence =>
      if admitted context then
        if before_last_consumer context then
          {| disposition := Retain;
             carries_established_authority := authority_required context |}
        else
          {| disposition := Erase;
             carries_established_authority := authority_required context |}
      else
        {| disposition := Retain;
           carries_established_authority := false |}
  end.

Definition DownstreamMayConsume
  (context : LifecycleContext)
  (projection : EvidenceProjection) : Prop :=
  admitted context = true /\
  (authority_required context = true ->
   carries_established_authority projection = true).

Definition ReceiptJustified (context : LifecycleContext) : Prop :=
  authority_required context = true /\
  would_redecide_without_carrier context = true.

Theorem missing_admission_fails_closed : forall kind context,
  admitted context = false ->
  ~ DownstreamMayConsume context (project_evidence kind context).
Proof.
  intros kind context Hmissing [Hadmitted _].
  rewrite Hmissing in Hadmitted. discriminate.
Qed.

Theorem identity_reference_carries_authority : forall context,
  admitted context = true ->
  runtime_required context = false ->
  authority_required context = true ->
  disposition (project_evidence IdentityEvidence context) = Reference /\
  carries_established_authority
    (project_evidence IdentityEvidence context) = true.
Proof.
  intros context Hadmitted Hruntime Hauthority.
  unfold project_evidence.
  rewrite Hadmitted, Hruntime, Hauthority.
  split; reflexivity.
Qed.

Theorem validity_summarizes_to_receipt : forall context,
  admitted context = true ->
  runtime_required context = false ->
  authority_required context = true ->
  disposition (project_evidence ValidityEvidence context) = Summarize /\
  carries_established_authority
    (project_evidence ValidityEvidence context) = true.
Proof.
  intros context Hadmitted Hruntime Hauthority.
  unfold project_evidence.
  rewrite Hadmitted, Hruntime, Hauthority.
  split; reflexivity.
Qed.

Theorem construction_erasure_preserves_established_authority : forall context,
  admitted context = true ->
  before_last_consumer context = false ->
  authority_required context = true ->
  disposition (project_evidence ConstructionEvidence context) = Erase /\
  carries_established_authority
    (project_evidence ConstructionEvidence context) = true.
Proof.
  intros context Hadmitted Hbefore Hauthority.
  unfold project_evidence.
  rewrite Hadmitted, Hbefore, Hauthority.
  split; reflexivity.
Qed.

Theorem construction_erasure_requires_discharge_and_last_consumer :
  forall context,
    disposition (project_evidence ConstructionEvidence context) = Erase ->
    admitted context = true /\ before_last_consumer context = false.
Proof.
  intros [Hadmitted Hbefore Hauthority Hruntime Hdiagnostic Hredecide] Herase.
  simpl in Herase.
  destruct Hadmitted; destruct Hbefore; simpl in Herase;
    try discriminate; split; reflexivity.
Qed.

Theorem materialization_requires_explicit_runtime_need : forall kind context,
  disposition (project_evidence kind context) = Materialize ->
  admitted context = true /\
  runtime_required context = true /\
  (kind = IdentityEvidence \/ kind = ValidityEvidence).
Proof.
  intros kind context Hmaterialize.
  destruct kind.
  - simpl in Hmaterialize.
    destruct (admitted context) eqn:Hadmitted; try discriminate.
    destruct (runtime_required context) eqn:Hruntime.
    + split; [assumption | split; [assumption | left; reflexivity]].
    + destruct (authority_required context); discriminate.
  - simpl in Hmaterialize.
    destruct (admitted context) eqn:Hadmitted; try discriminate.
    destruct (runtime_required context) eqn:Hruntime.
    + split; [assumption | split; [assumption | right; reflexivity]].
    + destruct (authority_required context); discriminate.
  - simpl in Hmaterialize.
    destruct (diagnostic_required context); discriminate.
  - simpl in Hmaterialize.
    destruct (admitted context);
      [destruct (before_last_consumer context) |]; discriminate.
Qed.

Theorem receipt_justified_iff_redecision_when_authority_required :
  forall context,
    authority_required context = true ->
    (ReceiptJustified context <->
     would_redecide_without_carrier context = true).
Proof.
  intros context Hauthority.
  unfold ReceiptJustified.
  split.
  - intros [_ Hredecide]. exact Hredecide.
  - intro Hredecide. split; assumption.
Qed.

Theorem no_redecision_means_no_receipt_justification : forall context,
  would_redecide_without_carrier context = false ->
  ~ ReceiptJustified context.
Proof.
  intros context Hno [_ Hredecide].
  rewrite Hno in Hredecide. discriminate.
Qed.

Theorem justified_validity_receipt_carries_authority : forall context,
  admitted context = true ->
  runtime_required context = false ->
  ReceiptJustified context ->
  disposition (project_evidence ValidityEvidence context) = Summarize /\
  carries_established_authority
    (project_evidence ValidityEvidence context) = true.
Proof.
  intros context Hadmitted Hruntime [Hauthority _].
  apply validity_summarizes_to_receipt; assumption.
Qed.

(* "Semantic entropy" is modeled conservatively as the finite count of still
   possible interpretations, not as Shannon entropy.  Representation size is a
   second abstract natural-number measure.  A valid compression step may keep
   either count equal, but may increase neither. *)
Record SemanticSnapshot : Type := {
  possible_interpretations : nat;
  representation_units : nat
}.

Definition CompressionStep
  (before after : SemanticSnapshot) : Prop :=
  possible_interpretations after <= possible_interpretations before /\
  representation_units after <= representation_units before.

Inductive CompressionTrace : SemanticSnapshot -> SemanticSnapshot -> Prop :=
  | CompressionTraceRefl : forall snapshot,
      CompressionTrace snapshot snapshot
  | CompressionTraceCons : forall before middle after,
      CompressionStep before middle ->
      CompressionTrace middle after ->
      CompressionTrace before after.

Theorem compression_step_transitive : forall first middle last,
  CompressionStep first middle ->
  CompressionStep middle last ->
  CompressionStep first last.
Proof.
  intros first middle last [HmiddleInterpretations HmiddleUnits]
    [HlastInterpretations HlastUnits].
  split.
  - eapply Nat.le_trans; eauto.
  - eapply Nat.le_trans; eauto.
Qed.

Theorem compression_trace_nonincreasing : forall before after,
  CompressionTrace before after ->
  possible_interpretations after <= possible_interpretations before /\
  representation_units after <= representation_units before.
Proof.
  intros before after Htrace.
  induction Htrace as
    [snapshot | first middle last Hstep Htail IH].
  - split; apply Nat.le_refl.
  - destruct Hstep as [HstepInterpretations HstepUnits].
    destruct IH as [IHInterpretations IHUnits].
    split.
    + eapply Nat.le_trans; eauto.
    + eapply Nat.le_trans; eauto.
Qed.

Theorem admitted_interpretation_does_not_reopen : forall before after,
  CompressionTrace before after ->
  possible_interpretations before = 1 ->
  possible_interpretations after <= 1.
Proof.
  intros before after Htrace Hone.
  destruct (compression_trace_nonincreasing before after Htrace)
    as [Hinterpretations _].
  rewrite <- Hone. exact Hinterpretations.
Qed.

End EvidenceLifecycleCore.
