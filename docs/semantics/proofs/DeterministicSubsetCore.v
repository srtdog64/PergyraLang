(*
  DeterministicSubsetCore.v  --  the admitted parallel subset is schedule
  independent: every execution order of admitted tasks ends in the state the
  canonical sequential order ends in.

  Companion to docs/204 §2.6 (determinism is the DEFINITION of the admitted
  subset, not a feature), §3.4 and §5 theorem 5 (Deterministic Parallel
  Subset), and to the two files it sits between:

    WitnessDataRace.v       a written slot is accessed by no other context
                            (xor_mut) -- the admission evidence
    ParallelReductionCore.v the join fold is schedule invariant -- the
                            reduction shape

  This file closes the middle: given footprints that satisfy the admission
  evidence, the TASK BODIES commute, so the state after any interleaving of
  the tasks equals the state after running them one by one in index order.
  That equality is what makes "sequential execution is a legal lowering"
  (docs/204 §1 row 2) and "an executor that changes the result is an executor
  bug" (docs/186 §3) sound statements rather than hopes.

  The model. A state maps locations to values. Task i has a read footprint
  R i, a write footprint W i, and a body. Two hypotheses say what a footprint
  means: the body changes no location outside W i, and what it writes depends
  only on locations in R i or W i. Two tasks are [independent] when neither
  writes a location the other touches -- docs/178's Disjointness /
  Exclusivity evidence, and the xor_mut invariant read at footprint
  granularity.

  What is proved.
    [commute]                independent bodies commute, pointwise.
    [run_permutation]        for a pairwise-independent family, any two task
                             orders give the same final state.
    [deterministic_subset]   hence every schedule of tasks 0..n-1 ends in the
                             canonical index-order state.
    [footprints_not_vacuous] the hypotheses are satisfiable by real bodies.

  Refutation.
    [write_conflict_is_schedule_dependent]  two tasks writing one location
                             end in different states in the two orders --
                             exactly the shape WitnessDataRace rejects.
    [conflicting_pair_not_independent]      and that pair fails [independent],
                             so the theorem above never spoke for it.

  Negative scope. Tasks are interleaved as units. Independence by footprint
  makes finer interleavings agree as well, but that refinement is not
  mechanised here. Nothing is said about the fold (ParallelReductionCore.v),
  about who computes the footprints, or about the compiler admitting exactly
  the pairs this file calls independent: that binding is the boundary-witness
  refinement docs/semantics/10 §7 records. No function extensionality is
  assumed: every equality between states is stated pointwise.
*)

Require Import Coq.Lists.List.
Require Import Coq.Arith.PeanoNat.
Require Import Coq.Bool.Bool.
Require Import Coq.Sorting.Permutation.
Import ListNotations.

Definition Loc    := nat.
Definition State  := Loc -> nat.
Definition TaskId := nat.

Section AdmittedSubset.

Variable R W  : TaskId -> Loc -> bool.
Variable body : TaskId -> State -> State.

(* A body changes nothing outside its write footprint ... *)
Hypothesis writes_only : forall i st l, W i l = false -> body i st l = st l.

(* ... and what it writes depends only on what it reads or writes. *)
Hypothesis reads_only : forall i st st',
  (forall l, R i l = true \/ W i l = true -> st l = st' l) ->
  forall l, W i l = true -> body i st l = body i st' l.

(* The admission evidence: neither task writes a location the other touches. *)
Definition independent (i j : TaskId) : Prop :=
  (forall l, W i l = true -> R j l = false /\ W j l = false) /\
  (forall l, W j l = true -> R i l = false /\ W i l = false).

(* ===================================================================== *)
(* 1. Bodies respect pointwise-equal states                               *)
(* ===================================================================== *)

Lemma body_ext : forall i st st',
  (forall l, st l = st' l) -> forall l, body i st l = body i st' l.
Proof.
  intros i st st' Heq l. destruct (W i l) eqn:Hw.
  - apply reads_only. intros l' _. exact (Heq l'). exact Hw.
  - rewrite (writes_only i st l Hw), (writes_only i st' l Hw). exact (Heq l).
Qed.

(* ===================================================================== *)
(* 2. Independent bodies commute                                          *)
(* ===================================================================== *)

Lemma other_leaves_footprint : forall i j st,
  independent i j ->
  forall l, R i l = true \/ W i l = true -> body j st l = st l.
Proof.
  intros i j st [_ Hji] l Hl. apply writes_only.
  destruct (W j l) eqn:Hwj; [| reflexivity].
  destruct (Hji l Hwj) as [Hr Hw].
  destruct Hl as [Hl | Hl]; [rewrite Hr in Hl | rewrite Hw in Hl]; discriminate.
Qed.

Theorem commute : forall i j st,
  independent i j -> forall l, body i (body j st) l = body j (body i st) l.
Proof.
  intros i j st Hind l.
  destruct Hind as [Hij Hji] eqn:Hd.
  destruct (W i l) eqn:Hwi.
  - (* i writes l: j does not touch l, and j leaves i's footprint alone *)
    destruct (Hij l Hwi) as [_ Hwj].
    rewrite (writes_only j (body i st) l Hwj).
    apply reads_only. intros l' Hl'.
    apply (other_leaves_footprint i j st Hind l' Hl'). exact Hwi.
  - destruct (W j l) eqn:Hwj.
    + (* j writes l: symmetric *)
      destruct (Hji l Hwj) as [_ Hwi'].
      rewrite (writes_only i (body j st) l Hwi).
      symmetry. apply reads_only. intros l' Hl'.
      assert (Hind' : independent j i) by (split; assumption).
      apply (other_leaves_footprint j i st Hind' l' Hl'). exact Hwj.
    + (* neither writes l *)
      rewrite (writes_only i (body j st) l Hwi), (writes_only j st l Hwj).
      rewrite (writes_only j (body i st) l Hwj), (writes_only i st l Hwi).
      reflexivity.
Qed.

(* ===================================================================== *)
(* 3. Runs and their schedule independence                                *)
(* ===================================================================== *)

Fixpoint run (sched : list TaskId) (st : State) : State :=
  match sched with
  | []     => st
  | i :: r => run r (body i st)
  end.

Lemma run_ext : forall sched st st',
  (forall l, st l = st' l) -> forall l, run sched st l = run sched st' l.
Proof.
  induction sched as [| i r IH]; intros st st' Heq l; simpl.
  - exact (Heq l).
  - apply IH. intros l'. exact (body_ext i st st' Heq l').
Qed.

Definition pairwise_independent (sched : list TaskId) : Prop :=
  forall i j, In i sched -> In j sched -> i <> j -> independent i j.

Lemma pairwise_tail : forall x r,
  pairwise_independent (x :: r) -> pairwise_independent r.
Proof.
  intros x r H i j Hi Hj Hne. apply H; [right | right |]; assumption.
Qed.

Lemma pairwise_perm : forall s1 s2,
  Permutation s1 s2 -> pairwise_independent s1 -> pairwise_independent s2.
Proof.
  intros s1 s2 Hp H i j Hi Hj Hne. apply H; [| | exact Hne];
    apply (Permutation_in _ (Permutation_sym Hp)); assumption.
Qed.

(* MAIN: for a pairwise-independent family, two task orders that are
   permutations of each other end in the same state. *)
Theorem run_permutation : forall s1 s2,
  Permutation s1 s2 -> pairwise_independent s1 ->
  forall st l, run s1 st l = run s2 st l.
Proof.
  intros s1 s2 Hp. induction Hp as [| x m1 m2 Hperm IH | x y r | m1 m2 m3 Hp1 IH1 Hp2 IH2];
    intros Hind st l.
  - reflexivity.
  - simpl. apply IH. exact (pairwise_tail x m1 Hind).
  - simpl. destruct (Nat.eq_dec x y) as [E | E].
    + subst. reflexivity.
    + apply run_ext. intros l'.
      apply (commute x y st).
      apply Hind; [right; left; reflexivity | left; reflexivity | exact E].
  - rewrite (IH1 Hind st l). apply IH2. exact (pairwise_perm m1 m2 Hp1 Hind).
Qed.

(* MAIN: every schedule of tasks 0..n-1 ends in the canonical sequential
   state. This is docs/204 §2.6's definition of the admitted subset,
   as a theorem about the model. *)
Theorem deterministic_subset : forall n sched,
  Permutation sched (seq 0 n) -> pairwise_independent (seq 0 n) ->
  forall st l, run sched st l = run (seq 0 n) st l.
Proof.
  intros n sched Hp Hind st l.
  apply run_permutation. exact Hp.
  exact (pairwise_perm (seq 0 n) sched (Permutation_sym Hp) Hind).
Qed.

End AdmittedSubset.

(* ===================================================================== *)
(* 4. Instances: what the hypotheses admit and what they refuse           *)
(* ===================================================================== *)

(* Task i writes location i with value i + 1, reading nothing. *)
Definition R_own (_ : TaskId) (_ : Loc) : bool := false.
Definition W_own (i : TaskId) (l : Loc) : bool := Nat.eqb l i.
Definition body_own (i : TaskId) (st : State) : State :=
  fun l => if Nat.eqb l i then S i else st l.

Lemma own_writes_only : forall i st l, W_own i l = false -> body_own i st l = st l.
Proof. intros i st l H. unfold body_own, W_own in *. rewrite H. reflexivity. Qed.

Lemma own_reads_only : forall i st st',
  (forall l, R_own i l = true \/ W_own i l = true -> st l = st' l) ->
  forall l, W_own i l = true -> body_own i st l = body_own i st' l.
Proof.
  intros i st st' _ l H. unfold body_own, W_own in *. rewrite H. reflexivity.
Qed.

(* Two distinct own-location tasks are independent. *)
Lemma own_independent : forall i j, i <> j -> independent R_own W_own i j.
Proof.
  intros i j Hne. unfold independent, R_own, W_own. split; intros l Hl;
    apply Nat.eqb_eq in Hl; subst; split; try reflexivity;
    apply Nat.eqb_neq; [exact Hne | exact (fun H => Hne (eq_sym H))].
Qed.

Definition zero : State := fun _ => 0.

(* The hypotheses are satisfiable, and the theorem yields a concrete
   equality: running tasks 2,0,1 equals running 0,1,2. *)
Theorem footprints_not_vacuous :
  forall l, run body_own [2; 0; 1] zero l = run body_own (seq 0 3) zero l.
Proof.
  apply (deterministic_subset R_own W_own body_own own_writes_only own_reads_only 3).
  - simpl. apply perm_trans with (l' := [0; 2; 1]).
    + apply perm_swap.
    + apply perm_skip. apply perm_swap.
  - intros i j _ _ Hne. exact (own_independent i j Hne).
Qed.

(* Refutation: two tasks writing ONE location. *)
Definition W_clash (_ : TaskId) (l : Loc) : bool := Nat.eqb l 0.
Definition body_clash (i : TaskId) (st : State) : State :=
  fun l => if Nat.eqb l 0 then S i else st l.

Example write_conflict_is_schedule_dependent :
  run body_clash [0; 1] zero 0 <> run body_clash [1; 0] zero 0.
Proof. simpl. unfold body_clash. simpl. discriminate. Qed.

(* ... and that pair is not admitted: task 0 writes a location task 1 writes. *)
Example conflicting_pair_not_independent : ~ independent R_own W_clash 0 1.
Proof.
  intros [H _]. destruct (H 0 eq_refl) as [_ Hw]. discriminate Hw.
Qed.
