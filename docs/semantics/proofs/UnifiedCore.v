(*
  Pergyra Formal Semantics -- Unified Core Machine (capstone of the four corners)
  Target: docs/semantics/19 "Pergyra Abstract Machine Obligation"
  Status: theorems machine-verified previously (0 admits / 0 axioms, all Qed);
  now REBUILT on the shared PergyraCore root -- pending re-kernel-check in rocq9
  CI after the migration (no local prover on the authoring machine).

  Change: the abstract-machine model (principal/zone/cap/slot, config, the with_*
  constructors, cmap/smap, `step`, `steps`, has_cap, in_circulation, slot_in,
  restore_targets, slot_in_true, cmap_circulation) NO LONGER lives here as a
  private copy. It is `Require Import`ed from PergyraCore.v -- the corpus's shared
  foundation. This file now holds only the SYNTHESIS THEOREMS, proved over the
  imported machine. This is the first migration of the vertical-spine program
  (docs/semantics/proofs/VerticalSpineMigration.md): the capstone composes from
  the one shared machine instead of re-declaring it.

  Scope: unifies the four corner fragments (ZoneCrossingCore, EffectAuthorityCore,
  SlotLifecycleCore, AuthorityDelegationCore) and the Compensation/Rollback facet
  into ONE abstract machine with a single configuration and a single step relation
  carrying all Step forms. This is the synthesis claim docs/19 names: the capability/
  authority disciplines and rollback/compensation coexist on one state without
  interfering.

  Unified theorems:
    - capability_soundness: every backend-visible gated action -- zone crossing,
      effect emission, slot acquisition, authority delegation, AND rollback --
      requires the acting principal to hold the gating capability or capabilities.
      Use/Release are typestate-gated and carry no capability obligation.
    - affine_safety: once a slot is Released, no use/release of it is derivable.
    - authority_conservation: no step introduces a capability that was not already
      in circulation -- delegation redistributes, the others do not touch holdings.
    - rollback_restores: rolling back an effect restores every coupled slot to
      the state recorded in the effect log before the forward effect.
    - delegate_then_rollback_sound: delegation does not interfere with rollback
      soundness over actual step edges, and rollback preserves delegated authority.
    - delegation_furnishes_gated_rollback: delegating every target slot capability
      to the actor furnishes exactly the multi-slot gate that rollback requires.
*)

Require Import Coq.Lists.List.
Require Import Coq.Arith.PeanoNat.
Require Import PergyraCore.
Import ListNotations.

(* The abstract machine (config, step, steps, with_* constructors, cmap/smap,
   has_cap, in_circulation, slot_in_true, cmap_circulation) is imported from
   PergyraCore. Everything below is the synthesis proved over it. *)

(* ---- authority conservation: no step creates a new capability ---- *)

Theorem authority_conservation : forall gz ge ga ct act c c' k,
  step gz ge ga ct act c c' -> in_circulation c' k -> in_circulation c k.
Proof.
  intros gz ge ga ct act c c' k Hstep Hc.
  inversion Hstep; subst; unfold in_circulation, with_zone, with_emit,
    with_store, with_deleg, with_rollback in *; simpl in *.
  - exact Hc.
  - exact Hc.
  - exact Hc.
  - exact Hc.
  - exact Hc.
  - (* SDelegate: holdings became cmap; a new circulation entry is either the
       delegated k0 (held by actor, in circulation) or pre-existing. *)
    apply cmap_circulation in Hc. destruct Hc as [Hin | Hpre].
    + simpl in Hin. destruct Hin as [Hk | Hk].
      * subst k0. unfold has_cap in *. exists (actor c). assumption.
      * exists b. exact Hk.
    + exact Hpre.
  - exact Hc.
Qed.

Theorem authority_conservation_multi : forall gz ge ga ct c c' k,
  steps gz ge ga ct c c' -> in_circulation c' k -> in_circulation c k.
Proof.
  intros gz ge ga ct c c' k Hsteps Hc'.
  induction Hsteps.
  - exact Hc'.
  - apply IHHsteps in Hc'.
    apply (authority_conservation gz ge ga ct act a b k H Hc').
Qed.

(* ---- rollback soundness on the unified machine ---- *)

Theorem rollback_requires_log : forall gz ge ga ct c c',
  step gz ge ga ct ActRollback c c' -> elog c <> [].
Proof.
  intros gz ge ga ct c c' Hstep Hempty.
  inversion Hstep; subst.
  rewrite H in Hempty. discriminate.
Qed.

Theorem rollback_restores : forall gz ge ga ct c c' e before rest s,
  step gz ge ga ct ActRollback c c' ->
  elog c = mkLog e before :: rest ->
  In s (ct e) ->
  store c' s = before s.
Proof.
  intros gz ge ga ct c c' e before rest s Hstep Helog Hin.
  inversion Hstep; subst.
  rewrite Helog in H.
  inversion H; subst.
  simpl. unfold restore_targets.
  rewrite slot_in_true by exact Hin.
  reflexivity.
Qed.

Theorem rollback_pops_log : forall gz ge ga ct c c' e before rest,
  step gz ge ga ct ActRollback c c' ->
  elog c = mkLog e before :: rest ->
  elog c' = rest.
Proof.
  intros gz ge ga ct c c' e before rest Hstep Helog.
  inversion Hstep; subst.
  rewrite Helog in H.
  inversion H; subst.
  simpl. reflexivity.
Qed.

Theorem emit_logs_before_state : forall gz ge ga ct c c' e,
  step gz ge ga ct (ActEmit e) c c' ->
  elog c' = mkLog e (store c) :: elog c.
Proof.
  intros gz ge ga ct c c' e Hstep.
  inversion Hstep; subst. simpl. reflexivity.
Qed.

Theorem emit_then_rollback_restores : forall gz ge ga ct c c1 c2 e s,
  step gz ge ga ct (ActEmit e) c c1 ->
  step gz ge ga ct ActRollback c1 c2 ->
  In s (ct e) ->
  store c2 s = store c s.
Proof.
  intros gz ge ga ct c c1 c2 e s Hemit Hroll Hin.
  apply (rollback_restores gz ge ga ct c1 c2 e (store c) (elog c) s Hroll).
  - apply (emit_logs_before_state gz ge ga ct c c1 e Hemit).
  - exact Hin.
Qed.

(* ---- non-interference: delegation & rollback soundness over step edges ---- *)

Theorem delegate_then_rollback_sound : forall gz ge ga ct c c1 c2 b kd e before rest s,
  step gz ge ga ct (ActDelegate b kd) c c1 ->
  step gz ge ga ct ActRollback c1 c2 ->
  elog c1 = mkLog e before :: rest ->
  In s (ct e) ->
  store c2 s = before s /\ In kd (holdings c2 b).
Proof.
  intros gz ge ga ct c c1 c2 b kd e before rest s Hdel Hroll Hlog Hin.
  split.
  - apply (rollback_restores gz ge ga ct c1 c2 e before rest s Hroll Hlog Hin).
  - inversion Hdel; subst. inversion Hroll; subst.
    simpl. unfold cmap. destruct (Nat.eqb b b) eqn:Eb.
    + simpl. left. reflexivity.
    + apply Nat.eqb_neq in Eb. exfalso. apply Eb. reflexivity.
Qed.

Theorem delegate_rollback_steps_sound : forall gz ge ga ct c c1 c2 b kd,
  step gz ge ga ct (ActDelegate b kd) c c1 ->
  step gz ge ga ct ActRollback c1 c2 ->
  steps gz ge ga ct c c2.
Proof.
  intros gz ge ga ct c c1 c2 b kd Hdel Hroll.
  eapply SStep.
  - exact Hdel.
  - eapply SStep.
    + exact Hroll.
    + apply SRefl.
Qed.

Theorem acquire_delegate_then_rollback_sound :
  forall gz ge ga ct c c1 c2 c3 s b kd e before rest,
  step gz ge ga ct (ActAcquire s) c c1 ->
  step gz ge ga ct (ActDelegate b kd) c1 c2 ->
  step gz ge ga ct ActRollback c2 c3 ->
  elog c2 = mkLog e before :: rest ->
  In s (ct e) ->
  store c3 s = before s /\ In kd (holdings c3 b).
Proof.
  intros gz ge ga ct c c1 c2 c3 s b kd e before rest Hacq Hdel Hroll Hlog Hin.
  split.
  - apply (rollback_restores gz ge ga ct c2 c3 e before rest s Hroll Hlog Hin).
  - inversion Hdel; subst. inversion Hroll; subst.
    simpl. unfold cmap. destruct (Nat.eqb b b) eqn:Eb.
    + simpl. left. reflexivity.
    + apply Nat.eqb_neq in Eb. exfalso. apply Eb. reflexivity.
Qed.

Theorem acquire_delegate_rollback_steps_sound :
  forall gz ge ga ct c c1 c2 c3 s b kd,
  step gz ge ga ct (ActAcquire s) c c1 ->
  step gz ge ga ct (ActDelegate b kd) c1 c2 ->
  step gz ge ga ct ActRollback c2 c3 ->
  steps gz ge ga ct c c3.
Proof.
  intros gz ge ga ct c c1 c2 c3 s b kd Hacq Hdel Hroll.
  eapply SStep.
  - exact Hacq.
  - eapply SStep.
    + exact Hdel.
    + eapply SStep.
      * exact Hroll.
      * apply SRefl.
Qed.

(* ---- capability soundness: gated actions require capabilities ---- *)

Theorem capability_soundness : forall gz ge ga ct act c c',
  step gz ge ga ct act c c' ->
  match act with
  | ActCross z' => has_cap c (gz z')
  | ActEmit e => has_cap c (ge e)
  | ActAcquire s => has_cap c (ga s)
  | ActDelegate b k => has_cap c k
  | ActRollback =>
      exists e before rest,
        elog c = mkLog e before :: rest /\
        Forall (fun s => has_cap c (ga s)) (ct e)
  | ActUse _ | ActRelease _ => True
  end.
Proof.
  intros gz ge ga ct act c c' Hstep.
  inversion Hstep; subst; simpl; eauto.
Qed.

(* ---- affine safety / fail-closed: no operation after release ---- *)

Theorem no_op_after_release : forall gz ge ga ct s c c',
  store c s = Released ->
  ~ (step gz ge ga ct (ActUse s) c c' \/ step gz ge ga ct (ActRelease s) c c').
Proof.
  intros gz ge ga ct s c c' Hrel [Huse | Hrelstep];
    inversion Huse || inversion Hrelstep; subst; congruence.
Qed.

(* ---- coupled non-interference: delegation furnishes the rollback gate ----

   The deeper synthesis claim: rollback's multi-slot gate is satisfied by an
   explicit handoff of every target slot capability to the actor. The gated
   rollback then (a) fires, (b) restores every coupled slot from the logged
   pre-effect store, and (c) leaves the delegated authority intact. Rollback
   consumes the effect log, not capabilities. *)

Lemma target_caps_held_by_actor : forall c ga targets,
  Forall
    (fun s => has_cap (with_target_deleg c (actor c) ga targets) (ga s))
    targets.
Proof.
  intros c ga targets.
  apply Forall_forall. intros s Hin.
  unfold has_cap, with_target_deleg. simpl. unfold cmap.
  rewrite Nat.eqb_refl.
  apply in_or_app. left.
  apply in_map. exact Hin.
Qed.

Theorem delegation_furnishes_gated_rollback :
  forall gz ge ga ct c e before rest,
    elog c = mkLog e before :: rest ->
    step gz ge ga ct ActRollback
         (with_target_deleg c (actor c) ga (ct e))
         (with_rollback
            (with_target_deleg c (actor c) ga (ct e))
            (ct e) before rest)
    /\ (forall s,
          In s (ct e) ->
          store
            (with_rollback
              (with_target_deleg c (actor c) ga (ct e))
              (ct e) before rest) s = before s)
    /\ (forall s,
          In s (ct e) ->
          In (ga s)
             (holdings
               (with_rollback
                 (with_target_deleg c (actor c) ga (ct e))
                 (ct e) before rest)
               (actor c))).
Proof.
  intros gz ge ga ct c e before rest Hlog.
  split; [| split].
  - apply SRollback.
    + simpl. exact Hlog.
    + apply target_caps_held_by_actor.
  - intros s Hin. simpl. unfold restore_targets.
    rewrite slot_in_true by exact Hin.
    reflexivity.
  - intros s Hin. simpl. unfold cmap.
    rewrite Nat.eqb_refl.
    apply in_or_app. left. apply in_map. exact Hin.
Qed.
