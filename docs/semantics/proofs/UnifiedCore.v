(*
  Pergyra Formal Semantics -- Unified Core Machine (capstone of the four corners)
  Target: docs/semantics/19 "Pergyra Abstract Machine Obligation"
  Status: machine-verified (coqc, 0 admits / 0 axioms). All theorems close with Qed.

  Scope: unifies the four corner fragments (ZoneCrossingCore, EffectAuthorityCore,
  SlotLifecycleCore, AuthorityDelegationCore) and the Compensation/Rollback facet
  into ONE abstract machine with a single configuration and a single step relation
  carrying all Step forms. This is the synthesis claim docs/19 names: the capability/
  authority disciplines and rollback/compensation coexist on one state without
  interfering.

  Configuration:
    actor    : the principal currently acting
    holdings : authority distribution (principal -> capabilities)
    here     : zone residence
    elog     : effect log; each entry carries the pre-effect store snapshot
    store    : slot typestate map
  has_cap c k := the acting principal holds k.

  Step forms (all capability/typestate gated, fail-closed by construction):
    Cross z' | Emit e | Acquire s | Use s | Release s | Delegate b k | Rollback

  Rollback is capability-gated too: restoring effect e's coupled slots
  (ct e : list slot) requires the acting principal to hold every target slot's
  acquire-capability. Rollback is backend-visible (compensation emits code), so
  leaving it ungated would contradict capability_soundness; SRollback carries
  the multi-slot gate.

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
Import ListNotations.

Section UnifiedCore.

Definition principal := nat.
Definition zone := nat.
Definition cap  := nat.
Definition eff  := nat.
Definition slot := nat.

Inductive lcstate := Empty | Filled | Released.

Definition slot_store := slot -> lcstate.

Definition slot_in (s : slot) (ss : list slot) : bool :=
  existsb (fun x => Nat.eqb s x) ss.

Definition restore_targets
  (current : slot_store) (before : slot_store) (targets : list slot) : slot_store :=
  fun x => if slot_in x targets then before x else current x.

Lemma slot_in_true : forall s targets,
  In s targets -> slot_in s targets = true.
Proof.
  intros s targets Hin. unfold slot_in.
  induction targets as [| x xs IH]; simpl in *.
  - contradiction.
  - destruct Hin as [Heq | Hin].
    + subst x. rewrite Nat.eqb_refl. reflexivity.
    + destruct (Nat.eqb s x) eqn:E.
      * reflexivity.
      * apply IH. exact Hin.
Qed.

Definition zone_graph    := zone -> cap.
Definition effect_graph  := eff  -> cap.
Definition acquire_graph := slot -> cap.
Definition comp_target   := eff -> list slot.

Record effect_log_entry := mkLog {
  logged_eff : eff;
  before_store : slot_store
}.

Inductive action :=
  | ActCross (z' : zone)
  | ActEmit (e : eff)
  | ActAcquire (s : slot)
  | ActUse (s : slot)
  | ActRelease (s : slot)
  | ActDelegate (b : principal) (k : cap)
  | ActRollback.

Definition cmap (h : principal -> list cap) (p : principal) (cs : list cap)
  : principal -> list cap :=
  fun x => if Nat.eqb x p then cs else h x.
Definition smap (s0 : slot_store) (s : slot) (v : lcstate)
  : slot_store :=
  fun x => if Nat.eqb x s then v else s0 x.

Record config := mkConfig {
  actor    : principal;
  holdings : principal -> list cap;
  here     : zone;
  elog     : list effect_log_entry;
  store    : slot_store
}.

Definition has_cap (c : config) (k : cap) : Prop := In k (holdings c (actor c)).
Definition in_circulation (c : config) (k : cap) : Prop :=
  exists p, In k (holdings c p).

Definition with_zone  (c : config) (z : zone) : config :=
  mkConfig (actor c) (holdings c) z (elog c) (store c).
Definition with_emit  (c : config) (e : eff) : config :=
  mkConfig (actor c) (holdings c) (here c)
           (mkLog e (store c) :: elog c) (store c).
Definition with_store (c : config) (s : slot) (v : lcstate) : config :=
  mkConfig (actor c) (holdings c) (here c) (elog c) (smap (store c) s v).
Definition with_deleg (c : config) (b : principal) (k : cap) : config :=
  mkConfig (actor c) (cmap (holdings c) b (k :: holdings c b))
           (here c) (elog c) (store c).
Definition with_target_deleg
  (c : config) (b : principal) (ga : acquire_graph) (targets : list slot)
  : config :=
  mkConfig (actor c)
           (cmap (holdings c) b (map ga targets ++ holdings c b))
           (here c) (elog c) (store c).
Definition with_rollback
  (c : config) (targets : list slot) (before : slot_store)
  (rest : list effect_log_entry) : config :=
  mkConfig (actor c) (holdings c) (here c) rest
           (restore_targets (store c) before targets).

Inductive step (gz : zone_graph) (ge : effect_graph) (ga : acquire_graph) (ct : comp_target)
  : action -> config -> config -> Prop :=
| SCross   : forall c z', has_cap c (gz z') ->
               step gz ge ga ct (ActCross z') c (with_zone c z')
| SEmit    : forall c e,  has_cap c (ge e) ->
               step gz ge ga ct (ActEmit e) c (with_emit c e)
| SAcquire : forall c s,  has_cap c (ga s) -> store c s = Empty ->
               step gz ge ga ct (ActAcquire s) c (with_store c s Filled)
| SUse     : forall c s,  store c s = Filled ->
               step gz ge ga ct (ActUse s) c c
| SRelease : forall c s,  store c s = Filled ->
               step gz ge ga ct (ActRelease s) c (with_store c s Released)
| SDelegate: forall c b k, has_cap c k ->
               step gz ge ga ct (ActDelegate b k) c (with_deleg c b k)
| SRollback: forall c e before rest,
               elog c = mkLog e before :: rest ->
               Forall (fun s => has_cap c (ga s)) (ct e) ->
               step gz ge ga ct ActRollback c
                    (with_rollback c (ct e) before rest).

(* Inductive multi-step relation. *)
Inductive steps (gz : zone_graph) (ge : effect_graph) (ga : acquire_graph) (ct : comp_target)
  : config -> config -> Prop :=
| SRefl : forall c, steps gz ge ga ct c c
| SStep : forall act a b c,
    step gz ge ga ct act a b -> steps gz ge ga ct b c -> steps gz ge ga ct a c.

(* ---- authority conservation: no step creates a new capability ---- *)

Lemma cmap_circulation : forall h b cs k,
  (exists p, In k (cmap h b cs p)) ->
  In k cs \/ (exists p, In k (h p)).
Proof.
  intros h b cs k [p Hp]. unfold cmap in Hp.
  destruct (Nat.eqb p b) eqn:E.
  - left. exact Hp.
  - right. exists p. exact Hp.
Qed.

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

End UnifiedCore.
