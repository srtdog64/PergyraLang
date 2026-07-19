(*
  ParallelReductionCore.v  --  the parallel join reduction is schedule- and
  worker-count invariant.

  Companion to docs/186 (parallel implementation plan), the chunk policy owner
  src/self_hosted/parallel/chunk_policy_owner.pgy, and the partition arithmetic
  in src/runtime/pgy_parallel_chunk.h (pgy_parallel_spawn_chunk_at).

  What `parallel (i in 0..n) join with op` actually does:
    - the index range is cut into k contiguous chunks, where
      k = pgy_parallel_chunk_count n = min(n, workers * PGY_PARALLEL_CHUNK_FACTOR);
    - the chunk tasks run concurrently and each writes its per-index result
      into a dedicated cell;
    - the joining thread then folds THE CELLS IN INDEX ORDER.
  So neither the schedule (which chunk finishes when) nor k -- which is a
  function of the worker count, i.e. of the machine -- is supposed to be
  visible in the result. The `PGY_WORKERS=1 2 3 16` witness observes exactly
  that, byte-identically, including a Float fold whose bit equality is only
  possible if the summation order never changes. This file proves it.

  Mechanized obligations:
    - [join_schedule_invariant]  any two completion orders that cover the range
      produce the same fold. Proved for an ARBITRARY op: no associativity, no
      commutativity, no identity element is assumed anywhere.
    - [chunk_tiles]  the runtime's lo/hi arithmetic tiles [0, n) exactly and in
      order for every chunk count k >= 1 -- the concatenation of the chunk index
      lists is literally `seq 0 n`.
    - [join_chunk_count_invariant]  hence two different chunk counts, i.e. two
      different worker counts, write the same cells and fold to the same value.
      This is the "semantics byte-preserving" claim of the auto-chunk landing
      (WO-RT-4 B3), as a theorem rather than as a measurement.

  Where the advantage is. Each competing reduction shape is defined here too
  and refuted against its own definition, so the comparison is machine-checked
  rather than asserted:
    - [completion_order_matters]  a runtime that reduces in COMPLETION order
      needs op commutative or its answer depends on thread timing.
    - [chunk_count_matters]  a runtime that folds each chunk independently and
      then combines the chunk results (the OpenMP `reduction` / tree-reduce
      shape) needs op associative, or the worker count leaks into the answer.
    - Non-associativity is not a corner case: IEEE floating-point addition is
      non-associative, which is why reproducibility across worker counts is
      normally lost. `Nat.sub` stands in for it below -- it fails associativity
      the same way, and the counterexample is exact rather than epsilon-sized.

  Negative scope: this is the REDUCTION shape only. It says nothing about
  whether the chunk bodies are race-free (WitnessDataRace.v), nothing about
  scheduler progress (ParallelSchedulingCore.v), and nothing about the two
  emitters actually implementing this shape -- that binding is what the C ==
  LLVM backend-compare and the worker-invariance witness check empirically.
  The cell model also assumes each index is written at most once with the same
  value; enforcing that is the outer-write rejection in the front end
  (tests/cases/parallel_join/reject_outer_write.pgy), not something proved here.
*)

Require Import Coq.Lists.List.
Require Import Coq.Arith.PeanoNat.
Require Import Coq.micromega.Lia.
Import ListNotations.

(* ===================================================================== *)
(* 1. The runtime's chunk partition                                       *)
(*                                                                        *)
(* Transcribed from pgy_parallel_spawn_chunk_at:                          *)
(*     base      = item_count / chunk_count;                              *)
(*     remainder = item_count % chunk_count;                              *)
(*     lo = base * index + (index < remainder ? index : remainder);       *)
(*     hi = lo + base + (index < remainder ? 1 : 0);                      *)
(* the conditional in `lo` is min(index, remainder).                       *)
(* ===================================================================== *)

Section ChunkTiling.

Variable n k : nat.
Hypothesis Hk : k <> 0.

Definition cbase : nat := n / k.
Definition crem  : nat := n mod k.

Definition chunk_lo (i : nat) : nat := cbase * i + Nat.min i crem.

Definition chunk_hi (i : nat) : nat :=
  chunk_lo i + cbase + (if Nat.ltb i crem then 1 else 0).

(* The index list a chunk task walks. *)
Definition chunk_indices (i : nat) : list nat :=
  seq (chunk_lo i) (chunk_hi i - chunk_lo i).

Lemma min_succ_split : forall i r,
  Nat.min (S i) r = Nat.min i r + (if Nat.ltb i r then 1 else 0).
Proof.
  intros i r. destruct (Nat.ltb i r) eqn:E.
  - apply Nat.ltb_lt in E.
    rewrite (Nat.min_l i r) by lia. rewrite (Nat.min_l (S i) r) by lia. lia.
  - apply Nat.ltb_ge in E.
    rewrite (Nat.min_r i r) by lia. rewrite (Nat.min_r (S i) r) by lia. lia.
Qed.

Lemma chunk_lo_zero : chunk_lo 0 = 0.
Proof. unfold chunk_lo. rewrite Nat.mul_0_r, Nat.min_0_l. reflexivity. Qed.

Lemma chunk_lo_le_hi : forall i, chunk_lo i <= chunk_hi i.
Proof. intro i. unfold chunk_hi. lia. Qed.

(* No gap and no overlap: chunk i ends exactly where chunk i+1 begins. *)
Lemma chunk_contiguous : forall i, chunk_hi i = chunk_lo (S i).
Proof.
  intro i. unfold chunk_hi, chunk_lo.
  rewrite min_succ_split, Nat.mul_succ_r. lia.
Qed.

(* The last chunk ends exactly at n -- no element is dropped, none invented. *)
Lemma chunk_lo_full : chunk_lo k = n.
Proof.
  unfold chunk_lo, cbase, crem.
  rewrite Nat.min_r
    by (apply Nat.lt_le_incl, Nat.mod_upper_bound, Hk).
  rewrite Nat.mul_comm. symmetry. apply (Nat.div_mod n k Hk).
Qed.

Lemma seq_split_at : forall a d, seq 0 a ++ seq a d = seq 0 (a + d).
Proof. intros a d. rewrite seq_app. simpl. reflexivity. Qed.

Lemma tiles_prefix : forall m,
  concat (map chunk_indices (seq 0 m)) = seq 0 (chunk_lo m).
Proof.
  induction m as [| m IH].
  - simpl. rewrite chunk_lo_zero. reflexivity.
  - rewrite seq_S, map_app, concat_app, IH. simpl. rewrite app_nil_r.
    unfold chunk_indices. rewrite seq_split_at.
    rewrite <- chunk_contiguous. f_equal.
    pose proof (chunk_lo_le_hi m). lia.
Qed.

(* MAIN (tiling): for every chunk count k >= 1 the chunks reproduce the index
   range exactly, in order. Chunking is a re-association of the SPAWN, not of
   the index sequence. *)
Theorem chunk_tiles : concat (map chunk_indices (seq 0 k)) = seq 0 n.
Proof. rewrite tiles_prefix, chunk_lo_full. reflexivity. Qed.

Corollary chunk_covers : forall i,
  i < n -> In i (concat (map chunk_indices (seq 0 k))).
Proof. intros i Hi. rewrite chunk_tiles. apply in_seq. lia. Qed.

End ChunkTiling.

(* ===================================================================== *)
(* 2. Result cells and the index-order fold                               *)
(* ===================================================================== *)

Section ParallelReduction.

Variable A : Type.
Variable f : nat -> A.        (* the loop body's result at index i *)
Variable op : A -> A -> A.    (* the join operator -- NO algebraic law assumed *)
Variable init : A.            (* the join seed *)

Definition Cells := nat -> option A.
Definition no_cells : Cells := fun _ => None.

Definition cell_write (c : Cells) (i : nat) : Cells :=
  fun j => if Nat.eqb j i then Some (f i) else c j.

(* [order] is the order in which indices actually complete -- the schedule. *)
Fixpoint cells_after (order : list nat) (c : Cells) : Cells :=
  match order with
  | []     => c
  | i :: r => cells_after r (cell_write c i)
  end.

Lemma cells_after_untouched : forall order c i,
  ~ In i order -> cells_after order c i = c i.
Proof.
  induction order as [| j r IH]; intros c i Hni; simpl.
  - reflexivity.
  - rewrite IH by (intro H; apply Hni; right; exact H).
    unfold cell_write. destruct (Nat.eqb i j) eqn:E.
    + apply Nat.eqb_eq in E. exfalso. apply Hni. left. exact (eq_sym E).
    + reflexivity.
Qed.

(* A cell written anywhere in the schedule holds that index's value, no matter
   where in the schedule the write happened. *)
Lemma cells_after_written : forall order c i,
  In i order -> cells_after order c i = Some (f i).
Proof.
  induction order as [| j r IH]; intros c i Hin; simpl in *.
  - contradiction.
  - destruct (in_dec Nat.eq_dec i r) as [Hr | Hnr].
    + apply IH. exact Hr.
    + rewrite cells_after_untouched by exact Hnr.
      destruct Hin as [Heq | Hr']; [| contradiction].
      subst j. unfold cell_write. rewrite Nat.eqb_refl. reflexivity.
Qed.

Definition cell_read (c : Cells) (i : nat) : A :=
  match c i with Some a => a | None => init end.

(* What the joining thread runs: a strict left fold over the cells, in index
   order. The schedule is not an argument. *)
Definition index_fold (c : Cells) (n : nat) : A :=
  fold_left op (map (cell_read c) (seq 0 n)) init.

(* MAIN (schedule invariance): any two completion orders covering [0, n) fold
   to the same value. op is arbitrary -- this is why a Float join is
   bit-reproducible across worker counts. *)
Theorem join_schedule_invariant : forall (o1 o2 : list nat) (n : nat),
  (forall i, i < n -> In i o1) ->
  (forall i, i < n -> In i o2) ->
  index_fold (cells_after o1 no_cells) n = index_fold (cells_after o2 no_cells) n.
Proof.
  intros o1 o2 n H1 H2. unfold index_fold. f_equal.
  apply map_ext_in. intros i Hi.
  apply in_seq in Hi. destruct Hi as [_ Hlt]. simpl in Hlt.
  unfold cell_read.
  rewrite (cells_after_written o1 no_cells i (H1 i Hlt)).
  rewrite (cells_after_written o2 no_cells i (H2 i Hlt)).
  reflexivity.
Qed.

(* MAIN (worker-count invariance): the auto-chunk policy picks k from the
   worker count. Two different worker counts -- hence two different chunk
   counts, hence two different spawn shapes -- fold to the same value. This is
   the "semantics byte-preserving" property of WO-RT-4 B3. *)
Theorem join_chunk_count_invariant : forall n k1 k2,
  k1 <> 0 -> k2 <> 0 ->
  index_fold (cells_after (concat (map (chunk_indices n k1) (seq 0 k1))) no_cells) n
  = index_fold (cells_after (concat (map (chunk_indices n k2) (seq 0 k2))) no_cells) n.
Proof.
  intros n k1 k2 H1 H2.
  apply join_schedule_invariant; intros i Hi;
    [ rewrite (chunk_tiles n k1 H1) | rewrite (chunk_tiles n k2 H2) ];
    apply in_seq; lia.
Qed.

End ParallelReduction.

(* ===================================================================== *)
(* 3. What the competing reduction shapes cost                            *)
(*                                                                        *)
(* Both alternatives are defined here and refuted against their own        *)
(* definitions. Neither refutation says the other runtimes are wrong -- it *)
(* says what algebraic law they must assume about op that this file's      *)
(* theorems above do not.                                                  *)
(* ===================================================================== *)

(* (a) Reduce in COMPLETION order: fold results as they arrive. This is the
   shape a naive `join` takes when the accumulator is updated from the worker
   that just finished. It needs op COMMUTATIVE.

   The operator here is list append -- the collection join, which the
   backend-compare fixture parallel_join_collection exercises. Append is
   associative, so a tree reduce would survive; it is not commutative, so a
   completion-order reduce does not. *)
Definition demo_body (i : nat) : list nat := [i].

Definition completion_fold (order : list nat) : list nat :=
  fold_left (@app nat) (map demo_body order) [].

Example completion_order_matters :
  completion_fold [0; 1] <> completion_fold [1; 0].
Proof. unfold completion_fold. simpl. discriminate. Qed.

(* Our shape, on the same data and the same operator, does not move: both
   schedules cover [0,2), so join_schedule_invariant applies directly. *)
Example index_fold_ignores_completion_order :
  index_fold (list nat) (@app nat) []
    (cells_after (list nat) demo_body [0; 1] (no_cells (list nat))) 2
  = index_fold (list nat) (@app nat) []
      (cells_after (list nat) demo_body [1; 0] (no_cells (list nat))) 2.
Proof.
  apply join_schedule_invariant; intros i Hi; simpl;
    destruct i as [| [| i]]; try lia; auto.
Qed.

(* (b) Fold each chunk independently, then combine the chunk results -- the
   OpenMP `reduction(op:x)` / Rayon `reduce` shape. It needs op ASSOCIATIVE,
   because the chunk boundaries re-associate the operator. The chunk count is
   the worker count, so without associativity the machine leaks into the
   answer.

   Nat.sub stands in for IEEE float addition: non-associative in the same way,
   but with an exact counterexample instead of an epsilon-sized one. *)
Definition chunk_reduce (chunks : list (list nat)) : nat :=
  match map (fun c => fold_left Nat.sub (tl c) (hd 0 c)) chunks with
  | []      => 0
  | r :: rs => fold_left Nat.sub rs r
  end.

(* Same three values, two chunk counts, two different answers: (7-1)-2 = 4
   against 7-(1-2) = 7. *)
Example chunk_count_matters :
  chunk_reduce [[7; 1; 2]] <> chunk_reduce [[7]; [1; 2]].
Proof. unfold chunk_reduce. simpl. discriminate. Qed.

(* The contrast is not that our fold happens to agree on this input -- it is
   that [join_chunk_count_invariant] is proved for an ARBITRARY op, so it holds
   at exactly the operator that just broke chunk_reduce. *)
Corollary index_fold_survives_nonassociative : forall n k1 k2,
  k1 <> 0 -> k2 <> 0 ->
  index_fold nat Nat.sub 0
    (cells_after nat (fun i => i) (concat (map (chunk_indices n k1) (seq 0 k1)))
                 (no_cells nat)) n
  = index_fold nat Nat.sub 0
      (cells_after nat (fun i => i) (concat (map (chunk_indices n k2) (seq 0 k2)))
                   (no_cells nat)) n.
Proof. intros n k1 k2 H1 H2. apply join_chunk_count_invariant; assumption. Qed.

(* ===================================================================== *)
(* 4. Scorecard                                                           *)
(*                                                                        *)
(*   shape                     schedule-invariant  worker-count-invariant  *)
(*   ------------------------  ------------------  ----------------------  *)
(*   completion-order reduce   only if commutative  only if commutative     *)
(*   chunk/tree reduce         yes                  only if associative     *)
(*   index-order cell fold     YES (unconditional)  YES (unconditional)     *)
(*                                                                          *)
(* The price of the bottom row is that the reduction itself is serial: the   *)
(* parallelism buys the per-index bodies, not the fold. That is a real cost  *)
(* and it is the reason the fold is O(n) on the joining thread. It is also   *)
(* exactly why the result carries no algebraic side condition -- the two     *)
(* facts are the same fact.                                                 *)
(* ===================================================================== *)
