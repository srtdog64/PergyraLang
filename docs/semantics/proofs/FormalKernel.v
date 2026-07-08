(*
  Pergyra Formal Semantics -- Formal Kernel Vocabulary Binding

  Status: machine-verified kernel-binding sketch; not whole-language proof.
  This file answers the "heuristic keyword" risk at the proof-pack level:
  source vocabulary is legitimate only when it translates to a named kernel
  primitive and a named owner fact. A citation, game term, or domain metaphor
  does not by itself justify a keyword.

  The model is intentionally small and standalone. WholeProgramCore.v owns the
  step machine; AIRBinding.v owns the live fact-interface shape. This file owns
  the vocabulary-to-kernel binding:

    source keyword -> kernel primitive(s) -> owner fact(s)

  Negative scope:
    - no parser or implementation correctness,
    - no whole-language soundness,
    - no proof that the current compiler emits these facts correctly.
*)

Require Import Coq.Lists.List.
Import ListNotations.

Section FormalKernel.

Inductive SourceKeyword : Type :=
  | KwWorld
  | KwZone
  | KwIntent
  | KwEffect
  | KwAuthority
  | KwSlot
  | KwSubject
  | KwRole
  | KwParty
  | KwProjection
  | KwChannel
  | KwRelation.

Inductive KernelPrimitive : Type :=
  | PrimBoundaryTransfer
  | PrimEffectEmit
  | PrimSlotLifecycle
  | PrimAuthorityFlow
  | PrimCompensation
  | PrimCoordination
  | PrimProjectionLoss
  | PrimParticipantBinding
  | PrimProtocolOrder.

Inductive KernelFact : Type :=
  | FactZoneGate
  | FactEffectGate
  | FactAcquireGate
  | FactHoldings
  | FactStore
  | FactEffectLog
  | FactCompTargets
  | FactDepGraph
  | FactProjectionBudget
  | FactParticipants
  | FactChannelProtocol
  | FactRelationEdge.

Inductive Claim : Type :=
  | ClaimKernelMeaning
  | ClaimWholeLanguage.

Record KernelMeaning : Type := mkMeaning {
  meaning_primitives : list KernelPrimitive;
  meaning_facts : list KernelFact
}.

Definition keyword_meaning (k : SourceKeyword) : KernelMeaning :=
  match k with
  | KwWorld =>
      mkMeaning
        [PrimBoundaryTransfer; PrimCoordination; PrimProjectionLoss]
        [FactZoneGate; FactDepGraph; FactProjectionBudget]
  | KwZone =>
      mkMeaning
        [PrimBoundaryTransfer; PrimAuthorityFlow]
        [FactZoneGate; FactHoldings]
  | KwIntent =>
      mkMeaning
        [PrimCoordination; PrimCompensation; PrimEffectEmit;
         PrimAuthorityFlow; PrimBoundaryTransfer]
        [FactDepGraph; FactCompTargets; FactEffectGate;
         FactHoldings; FactZoneGate]
  | KwEffect =>
      mkMeaning [PrimEffectEmit] [FactEffectGate; FactEffectLog]
  | KwAuthority =>
      mkMeaning [PrimAuthorityFlow] [FactHoldings]
  | KwSlot =>
      mkMeaning [PrimSlotLifecycle] [FactAcquireGate; FactStore]
  | KwSubject =>
      mkMeaning [PrimParticipantBinding; PrimAuthorityFlow]
        [FactParticipants; FactHoldings]
  | KwRole =>
      mkMeaning [PrimParticipantBinding] [FactParticipants]
  | KwParty =>
      mkMeaning [PrimParticipantBinding; PrimCoordination]
        [FactParticipants; FactDepGraph]
  | KwProjection =>
      mkMeaning [PrimProjectionLoss] [FactProjectionBudget]
  | KwChannel =>
      mkMeaning [PrimProtocolOrder; PrimCoordination]
        [FactChannelProtocol; FactDepGraph]
  | KwRelation =>
      mkMeaning [PrimParticipantBinding; PrimProjectionLoss]
        [FactRelationEdge; FactProjectionBudget]
  end.

Definition InterpretsByPrimitive (k : SourceKeyword) (p : KernelPrimitive)
  : Prop := In p (meaning_primitives (keyword_meaning k)).

Definition InterpretsByFact (k : SourceKeyword) (f : KernelFact) : Prop :=
  In f (meaning_facts (keyword_meaning k)).

Definition HasKernelMeaning (k : SourceKeyword) : Prop :=
  meaning_primitives (keyword_meaning k) <> [] /\
  meaning_facts (keyword_meaning k) <> [].

Definition permits_claim (k : SourceKeyword) (c : Claim) : Prop :=
  match c with
  | ClaimKernelMeaning => HasKernelMeaning k
  | ClaimWholeLanguage => False
  end.

Theorem every_keyword_has_kernel_meaning :
  forall k, HasKernelMeaning k.
Proof.
  intros k; destruct k; unfold HasKernelMeaning; simpl; split; discriminate.
Qed.

Theorem intent_decomposes_to_coordination_and_compensation :
  InterpretsByPrimitive KwIntent PrimCoordination /\
  InterpretsByPrimitive KwIntent PrimCompensation /\
  InterpretsByFact KwIntent FactDepGraph /\
  InterpretsByFact KwIntent FactCompTargets.
Proof.
  unfold InterpretsByPrimitive, InterpretsByFact; simpl.
  repeat split; auto.
Qed.

Theorem world_zone_share_boundary_kernel :
  InterpretsByPrimitive KwWorld PrimBoundaryTransfer /\
  InterpretsByPrimitive KwZone PrimBoundaryTransfer /\
  InterpretsByFact KwWorld FactZoneGate /\
  InterpretsByFact KwZone FactZoneGate.
Proof.
  unfold InterpretsByPrimitive, InterpretsByFact; simpl.
  repeat split; auto.
Qed.

Theorem zone_is_not_namespace_only :
  InterpretsByPrimitive KwZone PrimBoundaryTransfer /\
  InterpretsByFact KwZone FactZoneGate.
Proof.
  unfold InterpretsByPrimitive, InterpretsByFact; simpl.
  auto.
Qed.

Theorem authority_effect_not_aliases :
  keyword_meaning KwAuthority <> keyword_meaning KwEffect.
Proof.
  unfold keyword_meaning.
  discriminate.
Qed.

Theorem projection_is_loss_budgeted :
  InterpretsByPrimitive KwProjection PrimProjectionLoss /\
  InterpretsByFact KwProjection FactProjectionBudget.
Proof.
  unfold InterpretsByPrimitive, InterpretsByFact; simpl.
  auto.
Qed.

Theorem participant_keywords_have_participant_fact :
  InterpretsByFact KwSubject FactParticipants /\
  InterpretsByFact KwRole FactParticipants /\
  InterpretsByFact KwParty FactParticipants.
Proof.
  unfold InterpretsByFact; simpl.
  repeat split; auto.
Qed.

Theorem channel_requires_protocol_or_coordination_fact :
  InterpretsByPrimitive KwChannel PrimProtocolOrder /\
  InterpretsByPrimitive KwChannel PrimCoordination /\
  InterpretsByFact KwChannel FactChannelProtocol /\
  InterpretsByFact KwChannel FactDepGraph.
Proof.
  unfold InterpretsByPrimitive, InterpretsByFact; simpl.
  repeat split; auto.
Qed.

Theorem no_keyword_permits_whole_language_claim :
  forall k, ~ permits_claim k ClaimWholeLanguage.
Proof.
  intros k H.
  exact H.
Qed.

Theorem kernel_meaning_permits_only_kernel_claim :
  forall k, permits_claim k ClaimKernelMeaning.
Proof.
  intros k. apply every_keyword_has_kernel_meaning.
Qed.

End FormalKernel.
