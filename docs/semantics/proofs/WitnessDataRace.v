(*
  WitnessDataRace.v  --  data-race-freedom invariant (Witness model).

  Companion to docs/semantics/10_ability_witness_evidence.md.

  Thesis (the construction we trade for Rust's borrow checker): access to a slot
  across concurrent contexts obeys aliasing-xor-mutability -- a slot is EITHER
  written by a single context (with no other-context access) OR read by many. A
  Witness is the compiler-internal proof a context's access conforms. "Every
  concurrency boundary carries a Witness" denotes the invariant [xor_mut] below;
  this file proves that invariant rules out data races -- BOTH write-write and
  read-write -- by construction, and that the permitted boundary steps preserve
  it. Preservation is the obligation Pergyra's boundary typing discharges
  (docs/semantics/10 §7 maps move/consume + pin/view exclusivity + atomic-shared
  + cannot-cross to these steps).

  A data race (the real UB class) = two DISTINCT contexts concurrently access the
  same slot with AT LEAST ONE write. This file models read AND write capabilities
  so both write-write and read-write races are ruled out.
*)

Require Import Coq.Lists.List.
Require Import Coq.Arith.PeanoNat.
Import ListNotations.

Definition Slot    := nat.
Definition Context := nat.

Inductive Mode := Rd | Wr.

(* A configuration = the capabilities held: (context, slot, mode). *)
Definition Cap    := (Context * Slot * Mode)%type.
Definition Config := list Cap.

Definition holds (g : Config) (c : Context) (s : Slot) (m : Mode) : Prop :=
  In (c, s, m) g.

Definition writes   (g : Config) (c : Context) (s : Slot) : Prop := holds g c s Wr.
Definition accesses (g : Config) (c : Context) (s : Slot) : Prop :=
  exists m, holds g c s m.

(* Aliasing-xor-mutability invariant: a write-cap on a slot excludes any
   other-context access to it. (Many readers coexist; a writer is alone.) This
   is the Witness invariant for concurrent access. *)
Definition xor_mut (g : Config) : Prop :=
  forall c1 c2 s, writes g c1 s -> accesses g c2 s -> c1 = c2.

(* A data race: distinct contexts access the same slot, at least one writing.
   (Existential is symmetric in read/write: relabel so the writer is c1.) *)
Definition data_race (g : Config) : Prop :=
  exists c1 c2 s, c1 <> c2 /\ writes g c1 s /\ accesses g c2 s.

(* --------------------------------------------------------------------- *)
(* Theorem 1: the invariant rules out data races by construction          *)
(* (write-write AND read-write).                                          *)
(* --------------------------------------------------------------------- *)
Theorem xor_mut_no_data_race :
  forall g, xor_mut g -> ~ data_race g.
Proof.
  intros g Hxm [c1 [c2 [s [Hne [Hw Ha]]]]].
  apply Hne. exact (Hxm c1 c2 s Hw Ha).
Qed.

(* --------------------------------------------------------------------- *)
(* Permitted boundary steps. None creates a conflicting concurrent access. *)
(* --------------------------------------------------------------------- *)

(* Clear every capability for slot [s] (release the slot). *)
Definition clear_slot (g : Config) (s : Slot) : Config :=
  filter (fun p => negb (Nat.eqb (snd (fst p)) s)) g.

Lemma holds_clear_slot : forall g s c s' m,
  holds (clear_slot g s) c s' m <-> (holds g c s' m /\ s' <> s).
Proof.
  intros g s c s' m. unfold holds, clear_slot. split.
  - intro Hin. apply filter_In in Hin. destruct Hin as [Hin Hneq].
    simpl in Hneq. split. exact Hin.
    intro Heq. subst s'. rewrite Nat.eqb_refl in Hneq. discriminate.
  - intros [Hin Hneq]. apply filter_In. split. exact Hin.
    simpl. apply Bool.negb_true_iff. apply Nat.eqb_neq. exact Hneq.
Qed.

Lemma accesses_clear_slot : forall g s c s',
  accesses (clear_slot g s) c s' -> (accesses g c s' /\ s' <> s).
Proof.
  intros g s c s' [m Hm]. apply holds_clear_slot in Hm.
  destruct Hm as [Hm Hneq]. split. exists m; exact Hm. exact Hneq.
Qed.

Inductive step : Config -> Config -> Prop :=
  (* ACQUIRE-WRITE: only on a slot with no current access (exclusive). *)
  | step_acq_write : forall g c s,
      (forall c' m, ~ holds g c' s m) ->
      step g ((c, s, Wr) :: g)
  (* ACQUIRE-READ: only on a slot with no current writer (readers coexist). *)
  | step_acq_read : forall g c s,
      (forall c', ~ writes g c' s) ->
      step g ((c, s, Rd) :: g)
  (* RELEASE: clear a slot's capabilities. *)
  | step_release : forall g s,
      step g (clear_slot g s).

(* --------------------------------------------------------------------- *)
(* Theorem 2 (Preservation): every permitted step preserves xor_mut.      *)
(* --------------------------------------------------------------------- *)
Theorem xor_mut_preserved :
  forall g g', xor_mut g -> step g g' -> xor_mut g'.
Proof.
  intros g g' Hxm Hstep.
  destruct Hstep as [g c s Hfree | g c s Hnowriter | g s].
  - (* acquire-write: precond -- s had no access at all *)
    intros c1 c2 s0 Hw Ha.
    unfold writes, holds in Hw. simpl in Hw.
    destruct Ha as [m2 Ha]. unfold holds in Ha. simpl in Ha.
    destruct Hw as [E1 | Hw]; destruct Ha as [E2 | Ha].
    + inversion E1; inversion E2; subst; reflexivity.
    + inversion E1; subst c s0. exfalso. apply (Hfree c2 m2). exact Ha.
    + inversion E2; subst c s0. exfalso. apply (Hfree c1 Wr). exact Hw.
    + exact (Hxm c1 c2 s0 Hw (ex_intro _ m2 Ha)).
  - (* acquire-read: precond -- s had no writer; we add only a reader *)
    intros c1 c2 s0 Hw Ha.
    unfold writes, holds in Hw. simpl in Hw.
    destruct Ha as [m2 Ha]. unfold holds in Ha. simpl in Ha.
    destruct Hw as [E1 | Hw].
    + inversion E1. (* (c,s0,Wr) = (c,s,Rd) is impossible: Wr <> Rd *)
    + destruct Ha as [E2 | Ha].
      * inversion E2; subst c s0.
        exfalso. apply (Hnowriter c1). exact Hw.
      * exact (Hxm c1 c2 s0 Hw (ex_intro _ m2 Ha)).
  - (* release: caps only shrink *)
    intros c1 c2 s0 Hw Ha.
    unfold writes in Hw. apply holds_clear_slot in Hw. destruct Hw as [Hw _].
    apply accesses_clear_slot in Ha. destruct Ha as [Ha _].
    exact (Hxm c1 c2 s0 Hw Ha).
Qed.

(* --------------------------------------------------------------------- *)
(* Corollary: a well-typed run starting xor_mut is data-race-free at every *)
(* reachable state.                                                        *)
(* --------------------------------------------------------------------- *)
Inductive steps : Config -> Config -> Prop :=
  | steps_refl  : forall g, steps g g
  | steps_trans : forall g g' g'', step g g' -> steps g' g'' -> steps g g''.

Theorem run_data_race_free :
  forall g g', xor_mut g -> steps g g' -> ~ data_race g'.
Proof.
  intros g g' Hxm Hrun.
  apply xor_mut_no_data_race.
  induction Hrun as [g | g g' g'' Hstep Hrun IH].
  - exact Hxm.
  - apply IH. exact (xor_mut_preserved g g' Hxm Hstep).
Qed.

(* Sanity: many readers coexist (no over-restriction). With two readers of the
   same slot from distinct contexts, there is no data race. *)
Example readers_share_ok :
  forall c1 c2 s, ~ data_race [(c1, s, Rd); (c2, s, Rd)].
Proof.
  intros c1 c2 s [d1 [d2 [s0 [Hne [Hw _]]]]].
  unfold writes, holds in Hw. simpl in Hw.
  destruct Hw as [E1 | [E2 | F]].
  - inversion E1.
  - inversion E2.
  - exact F.
Qed.
