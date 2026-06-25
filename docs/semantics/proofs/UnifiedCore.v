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
    elog     : effect log
    store    : slot typestate map
  has_cap c k := the acting principal holds k.

  Step forms (all capability/typestate gated, fail-closed by construction):
    Cross z' | Emit e | Acquire s | Use s | Release s | Delegate b k | Rollback
  Rollback is capability-gated too: restoring effect e's coupled slot (ct e)
  requires the acting principal to hold that slot's acquire-capability
  (ga (ct e)). Rollback is backend-visible (compensation emits code), so leaving
  it ungated would contradict capability_soundness; SRollback now carries the gate.

  Unified theorems:
    - capability_soundness: every backend-visible gated action -- zone crossing,
      effect emission, slot acquisition, authority delegation, AND rollback --
      requires the acting principal to hold the gating capability. Use/Release are
      typestate-gated (not capability-gated) and carry no capability obligation.
      (docs/19 Capability soundness, whole machine, all five gated forms.)
    - affine_safety: once a slot is Released, no use/release of it is derivable.
    - authority_conservation: no step introduces a capability that was not already
      in circulation -- delegation redistributes, the others do not touch holdings.
      (docs/19 no-ambient-authority, whole machine.)
    - rollback_restores: rolling back an effect restores its coupled slot to Empty.
    - delegate_then_rollback_sound: delegation does not interfere with rollback soundness,
      and rollback preserves delegated authority (non-interference).
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

Definition zone_graph    := zone -> cap.
Definition effect_graph  := eff  -> cap.
Definition acquire_graph := slot -> cap.
Definition comp_target   := eff -> slot.

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
Definition smap (s0 : slot -> lcstate) (s : slot) (v : lcstate)
  : slot -> lcstate :=
  fun x => if Nat.eqb x s then v else s0 x.

Record config := mkConfig {
  actor    : principal;
  holdings : principal -> list cap;
  here     : zone;
  elog     : list eff;
  store    : slot -> lcstate
}.

Definition has_cap (c : config) (k : cap) : Prop := In k (holdings c (actor c)).
Definition in_circulation (c : config) (k : cap) : Prop :=
  exists p, In k (holdings c p).

Definition with_zone  (c : config) (z : zone)        : config :=
  mkConfig (actor c) (holdings c) z (elog c) (store c).
Definition with_emit  (c : config) (e : eff)         : config :=
  mkConfig (actor c) (holdings c) (here c) (e :: elog c) (store c).
Definition with_store (c : config) (s : slot) (v : lcstate) : config :=
  mkConfig (actor c) (holdings c) (here c) (elog c) (smap (store c) s v).
Definition with_deleg (c : config) (b : principal) (k : cap) : config :=
  mkConfig (actor c) (cmap (holdings c) b (k :: holdings c b))
           (here c) (elog c) (store c).
Definition with_rollback (c : config) (s : slot) (rest : list eff) : config :=
  mkConfig (actor c) (holdings c) (here c) rest (smap (store c) s Empty).

Inductive step (gz : zone_graph) (ge : effect_graph) (ga : acquire_graph) (ct : comp_target)
  : action -> config -> config -> Prop :=
| SCross   : forall c z', has_cap c (gz z') -> step gz ge ga ct (ActCross z') c (with_zone c z')
| SEmit    : forall c e,  has_cap c (ge e)  -> step gz ge ga ct (ActEmit e) c (with_emit c e)
| SAcquire : forall c s,  has_cap c (ga s) -> store c s = Empty ->
               step gz ge ga ct (ActAcquire s) c (with_store c s Filled)
| SUse     : forall c s,  store c s = Filled -> step gz ge ga ct (ActUse s) c c
| SRelease : forall c s,  store c s = Filled ->
               step gz ge ga ct (ActRelease s) c (with_store c s Released)
| SDelegate: forall c b k, has_cap c k ->
               step gz ge ga ct (ActDelegate b k) c (with_deleg c b k)
| SRollback: forall c e rest,
               elog c = e :: rest ->
               has_cap c (ga (ct e)) ->
               step gz ge ga ct ActRollback c (with_rollback c (ct e) rest).

(* Inductive multi-step relation *)
Inductive steps (gz : zone_graph) (ge : effect_graph) (ga : acquire_graph) (ct : comp_target)
  : config -> config -> Prop :=
| SRefl : forall c, steps gz ge ga ct c c
| SStep : forall act a b c, step gz ge ga ct act a b -> steps gz ge ga ct b c -> steps gz ge ga ct a c.

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
  intros gz ge ga ct act c c' k H Hc.
  inversion H; subst; unfold in_circulation, with_zone, with_emit, with_store,
    with_deleg, with_rollback in *; simpl in *.
  - exact Hc.
  - exact Hc.
  - exact Hc.
  - exact Hc.
  - exact Hc.
  - (* SDelegate: holdings became cmap; a new circulation entry is either the
       delegated k0 (held by actor, in circulation) or pre-existing. *)
    apply cmap_circulation in Hc. destruct Hc as [Hin | Hpre].
    + (* k is in the delegated list (k0 :: holdings c b) *)
      simpl in Hin. destruct Hin as [Hk | Hk].
      * (* k = k0: in circulation via the delegator (actor), which held k0 *)
        subst k0. exists (actor c). exact H0.
      * (* k already held by the delegatee b *)
        exists b. exact Hk.
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
  intros gz ge ga ct c c' H. inversion H; subst.
  rewrite H0. discriminate.
Qed.

Theorem rollback_restores : forall gz ge ga ct c c' e rest,
  step gz ge ga ct ActRollback c c' ->
  elog c = e :: rest ->
  store c' (ct e) = Empty.
Proof.
  intros gz ge ga ct c c' e rest H Helog.
  inversion H; subst.
  rewrite Helog in H0. injection H0 as He Hr. subst.
  simpl. unfold smap. rewrite Nat.eqb_refl. reflexivity.
Qed.

Theorem rollback_pops_log : forall gz ge ga ct c c' e rest,
  step gz ge ga ct ActRollback c c' ->
  elog c = e :: rest ->
  elog c' = rest.
Proof.
  intros gz ge ga ct c c' e rest H Helog.
  inversion H; subst.
  rewrite Helog in H0. injection H0 as He Hr. subst.
  simpl. reflexivity.
Qed.

(* ---- non-interference: delegation & rollback soundness ---- *)

Theorem delegate_then_rollback_sound : forall ct c c1 c2 b kd e rest,
  has_cap c kd ->
  c1 = with_deleg c b kd ->
  elog c1 = e :: rest ->
  c2 = with_rollback c1 (ct e) rest ->
  store c2 (ct e) = Empty /\ In kd (holdings c2 b).
Proof.
  intros ct c c1 c2 b kd e rest Hcap Hc1 Hlog Hc2.
  subst c1 c2. simpl.
  split.
  - unfold smap. rewrite Nat.eqb_refl. reflexivity.
  - unfold cmap. destruct (Nat.eqb b b) eqn:Eb.
    + simpl. left. reflexivity.
    + apply Nat.eqb_neq in Eb. exfalso. apply Eb. reflexivity.
Qed.

Theorem acquire_delegate_then_rollback_sound : forall ga ct c c1 c2 c3 s b kd e rest,
  has_cap c (ga s) ->
  store c s = Empty ->
  c1 = with_store c s Filled ->
  has_cap c1 kd ->
  c2 = with_deleg c1 b kd ->
  elog c2 = e :: rest ->
  ct e = s ->
  c3 = with_rollback c2 (ct e) rest ->
  store c3 s = Empty /\ In kd (holdings c3 b).
Proof.
  intros ga ct c c1 c2 c3 s b kd e rest Hcap_acq Hempty Hc1 Hcap_del Hc2 Hlog Hcoupling Hc3.
  subst c1 c2 c3. simpl.
  split.
  - rewrite Hcoupling. unfold smap. rewrite Nat.eqb_refl. reflexivity.
  - unfold cmap. destruct (Nat.eqb b b) eqn:Eb.
    + simpl. left. reflexivity.
    + apply Nat.eqb_neq in Eb. exfalso. apply Eb. reflexivity.
Qed.

(* ---- capability soundness: gated actions require capabilities ---- *)

Theorem capability_soundness : forall gz ge ga ct act c c',
  step gz ge ga ct act c c' ->
  match act with
  | ActCross z' => has_cap c (gz z')
  | ActEmit e => has_cap c (ge e)
  | ActAcquire s => has_cap c (ga s)
  | ActDelegate b k => has_cap c k
  | ActRollback => exists e rest, elog c = e :: rest /\ has_cap c (ga (ct e))
  | ActUse _ | ActRelease _ => True
  end.
Proof.
  intros gz ge ga ct act c c' H.
  inversion H; subst; simpl; eauto.
Qed.

(* ---- affine safety / fail-closed: no operation after release ---- *)

Theorem no_op_after_release : forall gz ge ga ct s c c',
  store c s = Released ->
  ~ (step gz ge ga ct (ActUse s) c c' \/ step gz ge ga ct (ActRelease s) c c').
Proof.
  intros gz ge ga ct s c c' Hrel [H | H]; inversion H; subst; congruence.
Qed.

End UnifiedCore.
