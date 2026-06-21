(*
  WitnessDataRace.v  --  stage-0 data-race-freedom invariant (Witness model).

  Companion to docs/semantics/10_ability_witness_evidence.md.

  Thesis (the construction we trade for Rust's borrow checker): a slot's WRITE
  capability has a single owner across concurrent contexts. A *write-Witness* is
  the compiler-internal proof that a context is that sole owner. "Every
  concurrency boundary carries a single-writer Witness" denotes the invariant
  [single_writer] below; this file proves that invariant *rules out data races
  by construction*, and that the permitted boundary steps (move / drop /
  acquire-fresh -- never duplicate) PRESERVE it. Preservation is what the type
  system's boundary rules must discharge; here it is proved for the abstract
  step relation, so the construction is shown sound at the model level.

  Stage-0 status: statements + proofs complete at the model level. The remaining
  obligation is the refinement: that Pergyra's actual boundary typing emits only
  these step shapes (the implementation's job; see docs/semantics/10 §6).
*)

Require Import Coq.Lists.List.
Require Import Coq.Arith.PeanoNat.
Import ListNotations.

Definition Slot    := nat.
Definition Context := nat.

(* A configuration = the multiset of (context, slot) write-capabilities held. *)
Definition Config := list (Context * Slot).

Definition holds_write (g : Config) (c : Context) (s : Slot) : Prop :=
  In (c, s) g.

(* Single-writer invariant: every slot has at most one write owner. This is the
   Witness invariant -- a write-Witness for [s] is the proof that the holder is
   the unique owner. *)
Definition single_writer (g : Config) : Prop :=
  forall c1 c2 s, holds_write g c1 s -> holds_write g c2 s -> c1 = c2.

(* A data race: two DISTINCT contexts both hold a write-cap to the SAME slot. *)
Definition data_race (g : Config) : Prop :=
  exists c1 c2 s, c1 <> c2 /\ holds_write g c1 s /\ holds_write g c2 s.

(* ----------------------------------------------------------------------- *)
(* Theorem 1: the invariant rules out data races by construction.          *)
(* This is the denotation of the single-writer Witness: hold the Witness    *)
(* invariant and a data race is impossible.                                 *)
(* ----------------------------------------------------------------------- *)
Theorem single_writer_no_data_race :
  forall g, single_writer g -> ~ data_race g.
Proof.
  intros g Hsw [c1 [c2 [s [Hne [H1 H2]]]]].
  apply Hne. exact (Hsw c1 c2 s H1 H2).
Qed.

(* ----------------------------------------------------------------------- *)
(* Boundary steps the type system permits. None DUPLICATES a write-cap into *)
(* a second context for an already-owned slot.                              *)
(* ----------------------------------------------------------------------- *)

(* Remove every cap for slot [s] (clears the slot's ownership). *)
Definition clear_slot (g : Config) (s : Slot) : Config :=
  filter (fun p => negb (Nat.eqb (snd p) s)) g.

Lemma holds_clear_slot : forall g s c s',
  holds_write (clear_slot g s) c s' <-> (holds_write g c s' /\ s' <> s).
Proof.
  intros g s c s'. unfold holds_write, clear_slot. split.
  - intro Hin. apply filter_In in Hin. destruct Hin as [Hin Hneq].
    simpl in Hneq. split. exact Hin.
    intro Heq. subst s'. rewrite Nat.eqb_refl in Hneq. discriminate.
  - intros [Hin Hneq]. apply filter_In. split. exact Hin.
    simpl. apply Bool.negb_true_iff. apply Nat.eqb_neq. exact Hneq.
Qed.

Inductive step : Config -> Config -> Prop :=
  (* MOVE: transfer slot [s] (cleared first) to context [c']. Send/transfer. *)
  | step_move : forall g s c',
      step g ((c', s) :: clear_slot g s)
  (* DROP: release ownership of slot [s]. *)
  | step_drop : forall g s,
      step g (clear_slot g s)
  (* ACQUIRE-FRESH: take a slot that currently has no owner. *)
  | step_fresh : forall g c s,
      (forall c', ~ holds_write g c' s) ->
      step g ((c, s) :: g).

(* ----------------------------------------------------------------------- *)
(* Theorem 2 (Preservation): every permitted step preserves single_writer.  *)
(* This is the "construction" half -- the invariant is closed under the      *)
(* boundary steps, so it holds along any execution.                          *)
(* ----------------------------------------------------------------------- *)
Theorem single_writer_preserved :
  forall g g', single_writer g -> step g g' -> single_writer g'.
Proof.
  intros g g' Hsw Hstep.
  destruct Hstep as [g s c' | g s | g c s Hfresh].
  - (* move *)
    intros c1 c2 s0 H1 H2. unfold holds_write in H1, H2.
    simpl in H1, H2.
    destruct H1 as [E1 | H1]; destruct H2 as [E2 | H2].
    + inversion E1; inversion E2; subst; reflexivity.
    + inversion E1; subst c' s.
      apply holds_clear_slot in H2. destruct H2 as [_ Hneq].
      exfalso. apply Hneq. reflexivity.
    + inversion E2; subst c' s.
      apply holds_clear_slot in H1. destruct H1 as [_ Hneq].
      exfalso. apply Hneq. reflexivity.
    + apply holds_clear_slot in H1. apply holds_clear_slot in H2.
      destruct H1 as [H1 _]. destruct H2 as [H2 _].
      exact (Hsw c1 c2 s0 H1 H2).
  - (* drop *)
    intros c1 c2 s0 H1 H2.
    apply holds_clear_slot in H1. apply holds_clear_slot in H2.
    destruct H1 as [H1 _]. destruct H2 as [H2 _].
    exact (Hsw c1 c2 s0 H1 H2).
  - (* acquire-fresh *)
    intros c1 c2 s0 H1 H2. unfold holds_write in H1, H2. simpl in H1, H2.
    destruct H1 as [E1 | H1]; destruct H2 as [E2 | H2].
    + inversion E1; inversion E2; subst; reflexivity.
    + inversion E1; subst c s0. exfalso. apply (Hfresh c2). exact H2.
    + inversion E2; subst c s0. exfalso. apply (Hfresh c1). exact H1.
    + exact (Hsw c1 c2 s0 H1 H2).
Qed.

(* ----------------------------------------------------------------------- *)
(* Corollary: a well-typed run starting single-writer is data-race-free at  *)
(* every reachable state.                                                    *)
(* ----------------------------------------------------------------------- *)
Inductive steps : Config -> Config -> Prop :=
  | steps_refl : forall g, steps g g
  | steps_trans : forall g g' g'', step g g' -> steps g' g'' -> steps g g''.

Theorem run_data_race_free :
  forall g g', single_writer g -> steps g g' -> ~ data_race g'.
Proof.
  intros g g' Hsw Hrun.
  apply single_writer_no_data_race.
  induction Hrun as [g | g g' g'' Hstep Hrun IH].
  - exact Hsw.
  - apply IH. exact (single_writer_preserved g g' Hsw Hstep).
Qed.
