(*
  Pergyra Formal Semantics -- Mechanized Fragment (third corner)
  Target: docs/semantics/19 "Pergyra Abstract Machine Obligation"
  Status: proof-sketch; not beta-closure evidence unless checked by CI (coqc).

  Scope: the resource-operation Step form of the abstract machine -- the slot /
  lifecycle facet (affine resources + typestate lineage, docs/19 slot row). The
  state carries AuthorityEvidence and a typestate store (each slot is Empty,
  Filled, or Released). Resource operations are labeled steps (acquire / use /
  release), gated by both a capability (acquire) and the slot's typestate.

  Mechanized obligations (docs/19):
    - Typestate soundness: each operation requires its precondition state
      (`acquire_requires_empty`, `use_requires_filled`, `release_requires_filled`).
    - Affine safety / fail-closed: once a slot is Released, no use or release of it
      is derivable -- use-after-release and double-release are impossible
      (`no_op_after_release`).
    - Capability soundness: acquiring a slot requires its acquire capability
      (`acquire_requires_capability`).
    - No ambient authority / composition: every resource step leaves authority
      evidence unchanged (`rstep_preserves_authority`), so this discipline composes
      with the zone-crossing and effect-emit corners over the same authority.

  This complements the existing `SlotCalculus.v` (which proves handle/token/pin
  runtime invariants): here the slot is a *Step form of the core calculus*, shown
  to compose with the other axes, rather than a standalone runtime model.

  Negative scope: a single abstract slot store, three lifecycle states, no
  generation/token layer (that is SlotCalculus.v), no binding to live MIR slot
  facts yet (task #45 / docs/18). Compensation's coupling (restore a prior
  typestate on rollback) is the remaining synthesis.
*)

Require Import Coq.Lists.List.
Require Import Coq.Arith.PeanoNat.
Import ListNotations.

Section SlotLifecycleCore.

Definition slot := nat.
Definition cap  := nat.

Inductive lcstate := Empty | Filled | Released.

(* The typestate store and a point update. *)
Definition store := slot -> lcstate.
Definition upd (s0 : store) (s : slot) (v : lcstate) : store :=
  fun x => if Nat.eqb x s then v else s0 x.

Definition authority := list cap.

Record config := mkConfig {
  held : authority;
  st   : store
}.

Definition has_cap (c : config) (k : cap) : Prop := In k (held c).

(* The capability required to ACQUIRE each slot. *)
Definition acquire_graph := slot -> cap.

(* Labeled resource operations, so theorems can name the slot acted on. *)
Inductive action := Acq (s : slot) | Use (s : slot) | Rel (s : slot).

Inductive rstep (ga : acquire_graph) : action -> config -> config -> Prop :=
| RAcquire : forall c s,
    has_cap c (ga s) ->
    st c s = Empty ->
    rstep ga (Acq s) c (mkConfig (held c) (upd (st c) s Filled))
| RUse : forall c s,
    st c s = Filled ->
    rstep ga (Use s) c c
| RRelease : forall c s,
    st c s = Filled ->
    rstep ga (Rel s) c (mkConfig (held c) (upd (st c) s Released)).

(* ---- no ambient authority / composition ---- *)

Lemma rstep_preserves_authority : forall ga act a b,
  rstep ga act a b -> held b = held a.
Proof. intros ga act a b H. inversion H; subst; simpl; reflexivity. Qed.

(* ---- typestate soundness: each op requires its precondition state ---- *)

Theorem acquire_requires_empty : forall ga s c c',
  rstep ga (Acq s) c c' -> st c s = Empty.
Proof. intros ga s c c' H. inversion H; subst. assumption. Qed.

Theorem use_requires_filled : forall ga s c c',
  rstep ga (Use s) c c' -> st c s = Filled.
Proof. intros ga s c c' H. inversion H; subst. assumption. Qed.

Theorem release_requires_filled : forall ga s c c',
  rstep ga (Rel s) c c' -> st c s = Filled.
Proof. intros ga s c c' H. inversion H; subst. assumption. Qed.

(* ---- capability soundness of acquire ---- *)

Theorem acquire_requires_capability : forall ga s c c',
  rstep ga (Acq s) c c' -> has_cap c (ga s).
Proof. intros ga s c c' H. inversion H; subst. assumption. Qed.

(* ---- affine safety / fail-closed: no operation after release ---- *)
(* Once a slot is Released, neither using nor releasing it is derivable:
   use-after-release and double-release are impossible. *)

Theorem no_op_after_release : forall ga s c c',
  st c s = Released ->
  ~ (rstep ga (Use s) c c' \/ rstep ga (Rel s) c c').
Proof.
  intros ga s c c' Hrel [H | H]; inversion H; subst; congruence.
Qed.

End SlotLifecycleCore.
