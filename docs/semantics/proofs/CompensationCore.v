(*
  Pergyra Formal Semantics -- Mechanized Fragment (compensation corner)
  Target: docs/semantics/19 "Pergyra Abstract Machine Obligation" (intent facet)
  Status: proof-sketch; not beta-closure evidence unless checked by CI (coqc).

  Scope: the compensation / rollback Step form -- the intent-specific facet that
  docs/19 flags as the hard COUPLING: compensation is sound only when it names
  both the effect to undo AND the typestate to restore. Saga lineage
  (Garcia-Molina & Salem; Bruni-Melgratti-Montanari).

  Model: a forward step `Forward e` acquires the slot the effect e touches
  (`comp_target e`) and logs e; a `Rollback` step undoes the most recently logged
  effect by restoring that slot to Empty and popping the log. `comp_target` is the
  explicit effect->slot coupling.

  Mechanized obligations:
    - Fail-closed: a rollback requires a non-empty effect log -- you cannot undo
      what was never done (`rollback_requires_log`).
    - Compensation soundness (the coupling): rolling back effect e restores its
      target slot to Empty (`rollback_restores`) and removes exactly that effect
      from the log (`rollback_pops_log`). This is where the effect facet and the
      slot/lifecycle facet must agree -- the synthesis point.

  Negative scope: single-level LIFO compensation (nested saga / partial
  compensation order is a follow-on), and no binding to live AIR/MIR intent facts
  yet (task #45 / docs/18).
*)

Require Import Coq.Lists.List.
Require Import Coq.Arith.PeanoNat.
Import ListNotations.

Section CompensationCore.

Definition slot := nat.
Definition eff  := nat.

Inductive lcstate := Empty | Filled | Released.

Definition store := slot -> lcstate.
Definition smap (s0 : store) (s : slot) (v : lcstate) : store :=
  fun x => if Nat.eqb x s then v else s0 x.

(* The effect->slot coupling: which slot each effect touched. *)
Definition comp_target := eff -> slot.

Record config := mkConfig {
  st   : store;
  elog : list eff
}.

Inductive caction := Forward (e : eff) | Rollback.

Inductive cstep (ct : comp_target) : caction -> config -> config -> Prop :=
| CDo : forall c e,
    st c (ct e) = Empty ->
    cstep ct (Forward e) c (mkConfig (smap (st c) (ct e) Filled) (e :: elog c))
| CComp : forall c e rest,
    elog c = e :: rest ->
    cstep ct Rollback c (mkConfig (smap (st c) (ct e) Empty) rest).

(* ---- fail-closed: cannot roll back an empty log ---- *)

Theorem rollback_requires_log : forall ct c c',
  cstep ct Rollback c c' -> elog c <> [].
Proof.
  intros ct c c' H. inversion H; subst. rewrite H0. discriminate.
Qed.

(* ---- compensation soundness: rollback restores the coupled slot ---- *)

Theorem rollback_restores : forall ct c c' e rest,
  cstep ct Rollback c c' ->
  elog c = e :: rest ->
  st c' (ct e) = Empty.
Proof.
  intros ct c c' e rest H Helog. inversion H; subst.
  rewrite Helog in H0. injection H0 as He Hr. subst.
  simpl. unfold smap. rewrite Nat.eqb_refl. reflexivity.
Qed.

(* ---- compensation removes exactly the compensated effect ---- *)

Theorem rollback_pops_log : forall ct c c' e rest,
  cstep ct Rollback c c' ->
  elog c = e :: rest ->
  elog c' = rest.
Proof.
  intros ct c c' e rest H Helog. inversion H; subst.
  rewrite Helog in H0. injection H0 as He Hr. subst.
  simpl. reflexivity.
Qed.

(* ---- a Forward step that is then rolled back returns the slot to Empty ---- *)
(* The saga round-trip: do then compensate is the identity on the touched slot. *)

Theorem do_then_rollback_restores : forall ct c c1 c2 e,
  cstep ct (Forward e) c c1 ->
  cstep ct Rollback c1 c2 ->
  elog c1 = e :: elog c ->
  st c2 (ct e) = Empty.
Proof.
  intros ct c c1 c2 e Hfwd Hroll Hlog.
  apply (rollback_restores ct c1 c2 e (elog c) Hroll Hlog).
Qed.

End CompensationCore.
