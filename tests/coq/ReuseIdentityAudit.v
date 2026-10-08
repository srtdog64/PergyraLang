(* Permanent API consumer for C1/C2/M2/M3/M4. Model results are not runtime
   adequacy; runtime_intent_identity_reuse_smoke.sh executes both actual routes. *)
Require Import Stdlib.Lists.List.
Require Import Stdlib.Arith.PeanoNat.
Require Import IntentConflict IntentSpine SlotLifecycleCore
  CollectionOwnershipTransfer ForeignStringOwnership.
Import ListNotations.

Example lifecycle_reuse_positive_and_old_holder_negative :
  SlotLifecycleCore.rstep (fun _ => 0) (SlotLifecycleCore.Use (4,2))
    SlotLifecycleCore.reuse_new SlotLifecycleCore.reuse_new /\
  ~ SlotLifecycleCore.rstep (fun _ => 0) (SlotLifecycleCore.Use (4,1))
    SlotLifecycleCore.reuse_new SlotLifecycleCore.reuse_new.
Proof.
  destruct SlotLifecycleCore.same_slot_new_identity_is_usable as [_ [_ [_ [Hlive Hstale]]]].
  auto.
Qed.

Example lifecycle_reclaim_then_arbitrary_steps_excludes_old_use_and_release :
  forall later last,
    SlotLifecycleCore.rsteps (fun _ => 0) SlotLifecycleCore.reuse_new later ->
    ~ (SlotLifecycleCore.rstep (fun _ => 0) (SlotLifecycleCore.Use (4,1)) later last \/
       SlotLifecycleCore.rstep (fun _ => 0) (SlotLifecycleCore.Rel (4,1)) later last).
Proof.
  intros later last Hrun.
  eapply SlotLifecycleCore.released_identity_forever_stale
    with (c := SlotLifecycleCore.reuse_retired); [reflexivity | reflexivity |].
  eapply SlotLifecycleCore.RSCons; [| exact Hrun].
  apply SlotLifecycleCore.RReclaim;
    [unfold SlotLifecycleCore.has_cap; simpl; auto | reflexivity | reflexivity].
Qed.

Definition lifecycle_arbitrary_continuation := SlotLifecycleCore.released_identity_forever_stale.
Definition identity_arbitrary_continuation := IntentConflict.old_public_identity_never_reissued.
Definition identity_issuance_bridge := IntentConflict.issue_handle_implements_identity_enter.
Definition retired_ancestry_refusal := IntentConflict.retired_parent_is_not_a_waiver.
Definition coordination_cycle_scope := IntentSpine.no_dep_cycle.

Example identity_boundary_exhaustion : forall limit,
  0 < limit ->
  IntentConflict.issue_handle limit limit = (limit,0) /\
  IntentConflict.issue_handle limit 0 = (0,0).
Proof.
  intros limit Hpositive. unfold IntentConflict.issue_handle.
  assert (E : (0 <? limit) = true) by (apply Nat.ltb_lt; exact Hpositive).
  rewrite E, Nat.leb_refl, Nat.eqb_refl. split; reflexivity.
Qed.

Example self_transfer_preserves_source :
  CollectionOwnershipTransfer.owner
    (fst (CollectionOwnershipTransfer.move CollectionOwnershipTransfer.h0 0 0)) 0 = Some 10.
Proof. rewrite CollectionOwnershipTransfer.self_move_is_identity. reflexivity. Qed.

Example transfer_refusals_are_distinguishable :
  snd (CollectionOwnershipTransfer.move CollectionOwnershipTransfer.h0 0 0) =
    CollectionOwnershipTransfer.MoveSelf /\
  snd (CollectionOwnershipTransfer.move CollectionOwnershipTransfer.h0 1 2) =
    CollectionOwnershipTransfer.MoveMissingSource /\
  snd (CollectionOwnershipTransfer.move
    (CollectionOwnershipTransfer.clone CollectionOwnershipTransfer.h0 0 1 20) 0 1) =
    CollectionOwnershipTransfer.MoveDestinationOwned /\
  snd (CollectionOwnershipTransfer.transfer_exec
    {| CollectionOwnershipTransfer.ledger_heap := CollectionOwnershipTransfer.h0;
       CollectionOwnershipTransfer.retired := [] |}
    (CollectionOwnershipTransfer.Retire 1)) = CollectionOwnershipTransfer.RetireMissingOwner.
Proof. repeat split; reflexivity. Qed.

Example consuming_drop_trace_counts_once :
  CollectionOwnershipTransfer.retired
    (CollectionOwnershipTransfer.transfer_run
      {| CollectionOwnershipTransfer.ledger_heap := CollectionOwnershipTransfer.h0;
         CollectionOwnershipTransfer.retired := [] |}
      [CollectionOwnershipTransfer.Transfer 0 0;
       CollectionOwnershipTransfer.Transfer 0 1;
       CollectionOwnershipTransfer.Retire 0;
       CollectionOwnershipTransfer.Retire 1;
       CollectionOwnershipTransfer.Retire 1]) = [10].
Proof. reflexivity. Qed.

Definition arbitrary_transfer_drop_safety := CollectionOwnershipTransfer.transfer_trace_retires_once.
Definition arbitrary_trace_from_unique_owner := CollectionOwnershipTransfer.unique_arbitrary_trace_retirement.
Definition copied_boundary_reuse_safety := ForeignStringOwnership.boundary_value_survives_reuse.

Example raw_pointer_design_falsifier :
  let m : ForeignStringOwnership.Memory :=
    fun p => if Nat.eqb p 40 then Some 7 else None in
  let after := ForeignStringOwnership.foreign_run m
    [ForeignStringOwnership.ForeignFree 40; ForeignStringOwnership.ForeignReuse 40 99] in
  ForeignStringOwnership.string_read after (ForeignStringOwnership.RawPointerString 40) = Some 99 /\
  ForeignStringOwnership.string_read after (ForeignStringOwnership.CopiedString 7) = Some 7.
Proof. split; reflexivity. Qed.

Print Assumptions lifecycle_arbitrary_continuation.
Print Assumptions identity_arbitrary_continuation.
Print Assumptions arbitrary_transfer_drop_safety.
Print Assumptions copied_boundary_reuse_safety.
