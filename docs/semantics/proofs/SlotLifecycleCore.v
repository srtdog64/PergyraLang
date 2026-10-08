(*
  Pergyra Formal Semantics -- Mechanized Fragment (third corner)
  Target: docs/semantics/19 "Pergyra Abstract Machine Obligation"
  Status: proof-sketch; not beta-closure evidence unless checked by CI (coqc).

  Scope: the resource-operation Step form of the abstract machine -- the slot /
  lifecycle facet (affine resources + typestate lineage, docs/19 slot row). The
  state carries AuthorityEvidence, typestate and a generation per slot.
  Resource operations name (slot,generation), not reusable slot numbers.

  Mechanized obligations (docs/19):
    - Typestate soundness: each operation requires its precondition state
      (`acquire_requires_vacancy`, `use_requires_filled`, `release_requires_filled`).
    - Affine safety / fail-closed: a released identity stays unusable through
      arbitrary steps, including reclaim of the same slot with a new generation
      (`released_identity_forever_stale`). Reuse itself remains executable.
    - Capability soundness: acquiring a slot requires its acquire capability
      (`acquire_requires_capability`).
    - No ambient authority / composition: every resource step leaves authority
      evidence unchanged (`rstep_preserves_authority`), so this discipline composes
      with the zone-crossing and effect-emit corners over the same authority.

  This complements the existing `SlotCalculus.v` (which proves handle/token/pin
  runtime invariants): here the slot is a *Step form of the core calculus*, shown
  to compose with the other axes, rather than a standalone runtime model.

  Negative scope: an unbounded generation abstract store, no token/pin layer
  (that is SlotCalculus.v), no binding to live MIR slot
  facts yet (task #45 / docs/18). Compensation's coupling (restore a prior
  typestate on rollback) is the remaining synthesis.
*)

Require Import Stdlib.Lists.List.
Require Import Stdlib.Arith.PeanoNat.
Require Import Stdlib.micromega.Lia.
Import ListNotations.

Section SlotLifecycleCore.

Definition slot := nat.
Definition cap  := nat.
Definition SlotHandle := (slot * nat)%type.

Inductive lcstate := Empty | Filled | Released.

(* The typestate store and a point update. *)
Definition store := slot -> lcstate.
Definition upd {A : Type} (s0 : slot -> A) (s : slot) (v : A) : slot -> A :=
  fun x => if Nat.eqb x s then v else s0 x.

Definition authority := list cap.

Record config := mkConfig {
  held : authority;
  st   : store;
  generation : slot -> nat
}.

Definition has_cap (c : config) (k : cap) : Prop := In k (held c).

(* The capability required to ACQUIRE each slot. *)
Definition acquire_graph := slot -> cap.

(* Labeled resource operations, so theorems can name the slot acted on. *)
Inductive action := Acq (h : SlotHandle) | Use (h : SlotHandle) | Rel (h : SlotHandle).

Inductive rstep (ga : acquire_graph) : action -> config -> config -> Prop :=
| RAcquire : forall c s g,
    has_cap c (ga s) ->
    st c s = Empty ->
    g = S (generation c s) ->
    rstep ga (Acq (s,g)) c
      (mkConfig (held c) (upd (st c) s Filled) (upd (generation c) s g))
| RReclaim : forall c s g,
    has_cap c (ga s) ->
    st c s = Released ->
    g = S (generation c s) ->
    rstep ga (Acq (s,g)) c
      (mkConfig (held c) (upd (st c) s Filled) (upd (generation c) s g))
| RUse : forall c s g,
    st c s = Filled ->
    generation c s = g ->
    rstep ga (Use (s,g)) c c
| RRelease : forall c s g,
    st c s = Filled ->
    generation c s = g ->
    rstep ga (Rel (s,g)) c
      (mkConfig (held c) (upd (st c) s Released) (generation c)).

(* ---- no ambient authority / composition ---- *)

Lemma rstep_preserves_authority : forall ga act a b,
  rstep ga act a b -> held b = held a.
Proof. intros ga act a b H. inversion H; subst; simpl; reflexivity. Qed.

(* ---- typestate soundness: each op requires its precondition state ---- *)

Theorem acquire_requires_vacancy : forall ga s g c c',
  rstep ga (Acq (s,g)) c c' -> st c s = Empty \/ st c s = Released.
Proof. intros ga s g c c' H. inversion H; subst; auto. Qed.

Theorem use_requires_filled : forall ga s g c c',
  rstep ga (Use (s,g)) c c' -> st c s = Filled /\ generation c s = g.
Proof. intros ga s g c c' H. inversion H; subst; auto. Qed.

Theorem release_requires_filled : forall ga s g c c',
  rstep ga (Rel (s,g)) c c' -> st c s = Filled /\ generation c s = g.
Proof. intros ga s g c c' H. inversion H; subst; auto. Qed.

(* ---- capability soundness of acquire ---- *)

Theorem acquire_requires_capability : forall ga s g c c',
  rstep ga (Acq (s,g)) c c' -> has_cap c (ga s).
Proof. intros ga s g c c' H. inversion H; subst; assumption. Qed.

(* ---- affine safety / fail-closed: no operation after release ---- *)
(* Immediate released-state exclusion. The arbitrary-run theorem below applies
   to a retired IDENTITY even when the same storage is Filled again. *)

Theorem no_op_after_release : forall ga s g c c',
  st c s = Released ->
  ~ (rstep ga (Use (s,g)) c c' \/ rstep ga (Rel (s,g)) c c').
Proof.
  intros ga s g c c' Hrel [H | H]; inversion H; subst; congruence.
Qed.

Definition retired_identity (h : SlotHandle) (c : config) : Prop :=
  snd h < generation c (fst h) \/
  (snd h = generation c (fst h) /\ st c (fst h) = Released).

Lemma retirement_preserved : forall ga act c c' h,
  rstep ga act c c' -> retired_identity h c -> retired_identity h c'.
Proof.
  intros ga act c c' [s g] Hstep Hdead.
  inversion Hstep; subst; unfold retired_identity in *; simpl in *;
    unfold upd; destruct (Nat.eqb s s0) eqn:E; simpl in *;
    try exact Hdead; apply Nat.eqb_eq in E; subst;
    destruct Hdead as [Hlt | [Heq Hrel]]; try (left; lia);
    try (right; split; [lia | reflexivity]); congruence.
Qed.

Inductive rsteps (ga : acquire_graph) : config -> config -> Prop :=
| RSNil : forall c, rsteps ga c c
| RSCons : forall c mid last act,
    rstep ga act c mid -> rsteps ga mid last -> rsteps ga c last.

Lemma retirement_preserved_run : forall ga c c' h,
  rsteps ga c c' -> retired_identity h c -> retired_identity h c'.
Proof.
  intros ga c c' h H. induction H; intros Hdead; auto.
  apply IHrsteps. eapply retirement_preserved; eauto.
Qed.

Theorem released_identity_forever_stale : forall ga s g c c' c'',
  generation c s = g -> st c s = Released -> rsteps ga c c' ->
  ~ (rstep ga (Use (s,g)) c' c'' \/ rstep ga (Rel (s,g)) c' c'').
Proof.
  intros ga s g c c' c'' Hgen Hrel Hrun Huse.
  assert (Hdead : retired_identity (s,g) c').
  { eapply retirement_preserved_run; [exact Hrun |].
    right. simpl. auto. }
  unfold retired_identity in Hdead. simpl in Hdead.
  destruct Huse as [Huse | Huse]; inversion Huse; subst;
    destruct Hdead as [Hlt | [Heq Hreleased]]; try lia; congruence.
Qed.

Definition reuse_initial := mkConfig [0] (fun _ => Empty) (fun _ => 0).
Definition reuse_live := mkConfig [0] (upd (st reuse_initial) 4 Filled)
  (upd (generation reuse_initial) 4 1).
Definition reuse_retired := mkConfig [0] (upd (st reuse_live) 4 Released)
  (generation reuse_live).
Definition reuse_new := mkConfig [0] (upd (st reuse_retired) 4 Filled)
  (upd (generation reuse_retired) 4 2).

Example same_slot_new_identity_is_usable :
  rstep (fun _ => 0) (Acq (4,1)) reuse_initial reuse_live /\
  rstep (fun _ => 0) (Rel (4,1)) reuse_live reuse_retired /\
  rstep (fun _ => 0) (Acq (4,2)) reuse_retired reuse_new /\
  rstep (fun _ => 0) (Use (4,2)) reuse_new reuse_new /\
  ~ rstep (fun _ => 0) (Use (4,1)) reuse_new reuse_new.
Proof.
  repeat split.
  - apply RAcquire; [unfold has_cap; simpl; auto | reflexivity | reflexivity].
  - apply RRelease; reflexivity.
  - apply RReclaim; [unfold has_cap; simpl; auto | reflexivity | reflexivity].
  - apply RUse; reflexivity.
  - intros H. apply use_requires_filled in H. destruct H as [_ Hgen].
    discriminate Hgen.
Qed.

Print Assumptions released_identity_forever_stale.

End SlotLifecycleCore.
