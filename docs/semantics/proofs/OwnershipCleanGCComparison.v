(*
  Ownership-based automatic memory management, compared with an explicit
  collector envelope and an ideal-root, nonmoving full-heap sweep.

  This supplement imports the ONE canonical ownership machine. It adds no
  target instruction, ownership rule, source syntax or production collector.
  Correct tracing collection is not declared unsafe. Exact reclamation is a
  stronger boundary property than retaining every readable live block.

  Cost claims are abstract operation counts under the SAME allocation,
  value/copy policy and per-block release cost. The sweep visits every retained
  block; root discovery is granted for free. This is not a model of every GC,
  generational copying, C/LLVM wall time, compiler cost, caches or peak RSS.
*)
Require Import OwnershipCleanCore.
Require Import Stdlib.Lists.List.
Require Import Stdlib.Arith.PeanoNat.
Require Import Stdlib.Sorting.Permutation.
Require Import Stdlib.micromega.Lia.
Import ListNotations.

Definition owner_roots (rho : TEnv) (R : list Block) : list Block :=
  heap_of rho ++ R.

(* Read coverage only: callers must not mistake it for a complete GC proof. *)
Definition gc_read_coverage (roots allocated : list Block) : Prop :=
  NoDup allocated /\ incl roots allocated.

Theorem ownership_has_gc_read_coverage : forall rho beta H n R,
  INV rho beta H n R -> gc_read_coverage (owner_roots rho R) H.
Proof.
  intros rho beta H n R [_ [HN [HP _]]]. split; [exact HN|].
  intros b Hb. eapply Permutation_in; [apply Permutation_sym; exact HP|exact Hb].
Qed.

Theorem ownership_retains_no_more_blocks : forall rho beta H n R G,
  INV rho beta H n R -> gc_read_coverage (owner_roots rho R) G ->
  length H <= length G.
Proof.
  intros rho beta H n R G HI [HG HC].
  destruct (heap_is_live_footprint _ _ _ _ _ HI) as [HP HN].
  rewrite (Permutation_length HP).
  eapply NoDup_incl_length; eassumption.
Qed.

Theorem collector_covers_canonical_reads : forall rho beta H n R G z c bs,
  INV rho beta H n R -> gc_read_coverage (owner_roots rho R) G ->
  tread z rho beta = Some (c, bs) -> incl bs G.
Proof.
  intros rho beta H n R G z c bs HI [_ HC] HR.
  destruct HI as [_ [_ [_ [_ [HB _]]]]].
  unfold tread in HR. destruct (tlookup z rho) as [[v blocks]|] eqn:E.
  - inversion HR; subst v blocks. intros b Hb. apply HC.
    apply in_or_app. left. eapply lookup_heap_incl; eassumption.
  - intros b Hb. apply HC. apply in_or_app. right. eapply HB; eassumption.
Qed.

Theorem deferred_gc_retention_gap : forall (H retired : list Block),
  length (H ++ retired) = length H + length retired.
Proof. intros. apply length_app. Qed.

Theorem deferred_gc_is_read_safe : forall rho beta H n R retired,
  INV rho beta H n R -> NoDup (H ++ retired) ->
  gc_read_coverage (owner_roots rho R) (H ++ retired).
Proof.
  intros rho beta H n R retired HI HN. split; [exact HN|].
  destruct (ownership_has_gc_read_coverage _ _ _ _ _ HI) as [_ HC].
  intros b Hb. apply in_or_app. left. apply HC. exact Hb.
Qed.

Theorem correct_gc_can_match_ownership : forall rho beta H n R,
  INV rho beta H n R ->
  exists G, gc_read_coverage (owner_roots rho R) G /\
    Permutation G (owner_roots rho R) /\ length G = length H.
Proof.
  intros rho beta H n R HI. exists H. split.
  - apply ownership_has_gc_read_coverage with (beta := beta) (n := n). exact HI.
  - split; [apply (proj1 (heap_is_live_footprint _ _ _ _ _ HI))|reflexivity].
Qed.

Example read_safe_retention_is_not_exact_reclamation :
  gc_read_coverage [] [(0, 0)] /\ ~ INV [] [] [(0, 0)] 1 [].
Proof.
  split.
  - split; [repeat constructor; simpl; tauto|intros b []].
  - intros [_ [_ [HP _]]]. apply Permutation_length in HP. discriminate.
Qed.

Example reclaiming_a_live_block_is_not_read_safe :
  ~ gc_read_coverage [(0, 0)] [].
Proof. intros [_ HC]. specialize (HC (0, 0) (or_introl eq_refl)). exact HC. Qed.

(* Ideal marking is an explicit comparison assumption: roots contain the
   complete live footprint. Even this favorable baseline inspects every
   allocated block when sweeping. The visit count follows the recursion. *)
Fixpoint full_heap_sweep (roots H : list Block) : list Block * nat :=
  match H with
  | [] => ([], 0)
  | b :: rest =>
      let '(kept, visits) := full_heap_sweep roots rest in
      (if bmem b roots then b :: kept else kept, S visits)
  end.

Lemma full_heap_sweep_spec : forall roots H,
  fst (full_heap_sweep roots H) = filter (fun b => bmem b roots) H /\
  snd (full_heap_sweep roots H) = length H.
Proof.
  intros roots H. induction H as [|b H IH]; [split; reflexivity|].
  cbn [full_heap_sweep].
  destruct (full_heap_sweep roots H) as [kept visits] eqn:E.
  cbn in IH. destruct IH as [HK HV].
  destruct (bmem b roots) eqn:EB; cbn [filter]; rewrite EB;
    cbn; rewrite HK, HV; split; reflexivity.
Qed.

Theorem full_heap_sweep_preserves_read_coverage : forall roots H,
  gc_read_coverage roots H ->
  gc_read_coverage roots (fst (full_heap_sweep roots H)).
Proof.
  intros roots H [HN HC]. rewrite (proj1 (full_heap_sweep_spec roots H)).
  split; [apply NoDup_filter; exact HN|].
  intros b Hb. apply filter_In. split; [apply HC; exact Hb|].
  apply bmem_true. exact Hb.
Qed.

Theorem full_heap_sweep_matches_exact_ownership : forall rho beta H n R G,
  INV rho beta H n R -> gc_read_coverage (owner_roots rho R) G ->
  Permutation (fst (full_heap_sweep (owner_roots rho R) G)) H.
Proof.
  intros rho beta H n R G HI HC.
  destruct (full_heap_sweep_preserves_read_coverage _ _ HC) as [HN HK].
  destruct (heap_is_live_footprint _ _ _ _ _ HI) as [HP HR].
  eapply Permutation_trans; [|apply Permutation_sym; exact HP].
  apply NoDup_Permutation; [exact HN|exact HR|].
  intros b. split; [|apply HK].
  rewrite (proj1 (full_heap_sweep_spec (owner_roots rho R) G)).
  intros Hb. apply filter_In in Hb. apply bmem_true. exact (proj2 Hb).
Qed.

Lemma full_heap_sweep_no_live_roots : forall H,
  full_heap_sweep [] H = ([], length H).
Proof.
  induction H as [|b H IH]; [reflexivity|].
  cbn [full_heap_sweep]. rewrite IH. reflexivity.
Qed.

(* A shared straight-line workload: allocate one abstract leaf, observe it,
   retire its last use, repeat. The compiler, execution and output below are
   the canonical core's, not a fresh comparison interpreter. *)
Definition retirement_value (_ : list SVal) : SVal := SLeaf 7.
Definition retirement_step : SStmt :=
  SSeq (SDef 0 retirement_value []) (SEmit 0).
Definition retirement_step_target : TStmt :=
  TSeq (TSeq (TDef (Src 0) retirement_value []) TSkip)
    (TSeq (TEmit (Src 0)) (TSeq (TDrop (Src 0)) TSkip)).

Fixpoint retirement_program (count : nat) : SStmt :=
  match count with
  | 0 => SSkip
  | S rest => SSeq retirement_step (retirement_program rest)
  end.
Fixpoint retirement_target (count : nat) : TStmt :=
  match count with
  | 0 => TSkip
  | S rest => TSeq retirement_step_target (retirement_target rest)
  end.

Lemma retirement_step_elaborates :
  elab no_summaries retirement_step [] [] = Some (retirement_step_target, []).
Proof. reflexivity. Qed.

Lemma retirement_step_source : forall funs procs sg,
  sexec funs procs sg retirement_step (supd sg 0 (SLeaf 7)) [SLeaf 7].
Proof.
  intros funs procs sg. unfold retirement_step.
  eapply SE_Seq with (sg2 := supd sg 0 (SLeaf 7)) (tr1 := []) (tr2 := [SLeaf 7]).
  - apply SE_Def with (vs := []). reflexivity.
  - apply SE_Emit. reflexivity.
  - reflexivity.
Qed.

Theorem retirement_program_source_trace : forall funs procs count sg,
  exists sg', sexec funs procs sg (retirement_program count) sg'
    (repeat (SLeaf 7) count).
Proof.
  intros funs procs count. induction count as [|count IH]; intros sg.
  - exists sg. apply SE_Skip.
  - destruct (IH (supd sg 0 (SLeaf 7))) as [sg' HE]. exists sg'.
    cbn [retirement_program repeat].
    eapply SE_Seq with (sg2 := supd sg 0 (SLeaf 7))
      (tr1 := [SLeaf 7]) (tr2 := repeat (SLeaf 7) count).
    + apply retirement_step_source.
    + exact HE.
    + reflexivity.
Qed.

Theorem retirement_program_uses_canonical_elab : forall count,
  elab no_summaries (retirement_program count) [] [] =
    Some (retirement_target count, []).
Proof.
  induction count as [|count IH]; [reflexivity|].
  cbn [retirement_program retirement_target elab]. rewrite IH. reflexivity.
Qed.

Theorem retirement_target_has_no_copies : forall count,
  count_copies (retirement_target count) = 0.
Proof.
  induction count as [|count IH]; [reflexivity|].
  cbn [retirement_target retirement_step_target count_copies]. exact IH.
Qed.

Lemma retirement_step_runs : forall tf tp frontier,
  texec tf tp [] [] [] frontier retirement_step_target
    [] [] [] (S frontier) [SLeaf 7].
Proof.
  intros tf tp frontier.
  set (one := alloc frontier 1).
  set (env := [(Src 0, (SLeaf 7, one))]).
  assert (HF : free one one = []).
  { unfold one, alloc, free, bmem, beqb. cbn. rewrite Nat.eqb_refl. reflexivity. }
  assert (HD : texec tf tp [] [] [] frontier
      (TDef (Src 0) retirement_value []) env [] one (S frontier) []).
  { unfold env, one. change (texec tf tp [] [] [] frontier
      (TDef (Src 0) retirement_value [])
      [(Src 0, (retirement_value [], alloc frontier (vsize (retirement_value []))))]
      [] (alloc frontier (vsize (retirement_value [])) ++ []) (S frontier) []).
    apply TE_Def with (vs := []); [split; reflexivity|reflexivity|constructor|].
    intros b []. }
  assert (HE : texec tf tp env [] one (S frontier)
      (TEmit (Src 0)) env [] one (S frontier) [SLeaf 7]).
  { apply TE_Emit with (bs := one); [reflexivity|apply incl_refl]. }
  assert (HX : texec tf tp env [] one (S frontier)
      (TDrop (Src 0)) (tremove (Src 0) env) [] (free one one) (S frontier) []).
  { apply TE_Drop with (c := SLeaf 7) (bs := one).
    - reflexivity.
    - apply incl_refl.
    - unfold one. apply alloc_nodup. }
  change (tremove (Src 0) env) with ([] : TEnv) in HX. rewrite HF in HX.
  unfold retirement_step_target.
  eapply TE_Seq with (rho2 := env) (beta2 := []) (H2 := one)
    (n2 := S frontier) (tr1 := []) (tr2 := [SLeaf 7]).
  - eapply TE_Seq; [exact HD|apply TE_Skip|reflexivity].
  - eapply TE_Seq with (rho2 := env) (beta2 := []) (H2 := one)
      (n2 := S frontier) (tr1 := [SLeaf 7]) (tr2 := []).
    + exact HE.
    + eapply TE_Seq; [exact HX|apply TE_Skip|reflexivity].
    + reflexivity.
  - reflexivity.
Qed.

Theorem retirement_program_runs_clean : forall tf tp count frontier,
  texec tf tp [] [] [] frontier (retirement_target count)
    [] [] [] (frontier + count) (repeat (SLeaf 7) count).
Proof.
  intros tf tp count. induction count as [|count IH]; intros frontier.
  - cbn [retirement_target repeat]. rewrite Nat.add_0_r. apply TE_Skip.
  - cbn [retirement_target repeat].
    replace (frontier + S count) with (S frontier + count) by lia.
    eapply TE_Seq with (rho2 := []) (beta2 := []) (H2 := [])
      (n2 := S frontier) (tr1 := [SLeaf 7]) (tr2 := repeat (SLeaf 7) count).
    + apply retirement_step_runs.
    + apply IH.
    + reflexivity.
Qed.

(* The deferred collector retains the SAME fresh allocation identities until
   one full sweep. Root removal and application work are common to both. *)
Definition retirement_blocks (count : nat) : list Block :=
  map (fun step => (step, 0)) (seq 0 count).

Lemma retirement_blocks_length : forall count,
  length (retirement_blocks count) = count.
Proof. intros. unfold retirement_blocks. rewrite length_map, length_seq. reflexivity. Qed.

Theorem retirement_blocks_are_fresh : forall count, NoDup (retirement_blocks count).
Proof.
  intros count. unfold retirement_blocks.
  assert (HM : forall (steps : list nat), NoDup steps ->
    NoDup (map (fun step => (step, 0)) steps)).
  { intros steps HN. induction HN as [|s steps HS HN IH]; cbn; constructor; [|exact IH].
    intros HI. apply in_map_iff in HI. destruct HI as [s' [E HI]].
    inversion E; subst s'. contradiction. }
  apply HM. apply seq_NoDup.
Qed.

Theorem deferred_workload_is_read_safe : forall count,
  gc_read_coverage [] (retirement_blocks count).
Proof. intros. split; [apply retirement_blocks_are_fresh|intros b []]. Qed.

Theorem deferred_workload_sweep_visits : forall count,
  full_heap_sweep [] (retirement_blocks count) = ([], count).
Proof. intros. rewrite full_heap_sweep_no_live_roots, retirement_blocks_length. reflexivity. Qed.

(* Weights count allocation, release and heap inspection separately. These
   are input assumptions, NOT axioms, measured nanoseconds, or a runtime ABI. *)
Definition ownership_workload_cost (allocation release count : nat) : nat :=
  (allocation + release) * count.
Definition gc_full_sweep_workload_cost (allocation release inspection count : nat) : nat :=
  allocation * count + release * count +
    inspection * snd (full_heap_sweep [] (retirement_blocks count)).

(* Independent accounting: neither formula invokes the other. Equal allocator,
   copy and release policies are comparison premises, not performance evidence. *)
Theorem equal_policy_cost_accounting : forall allocation release inspection count,
  gc_full_sweep_workload_cost allocation release inspection count =
    ownership_workload_cost allocation release count + inspection * count.
Proof.
  intros. unfold gc_full_sweep_workload_cost, ownership_workload_cost.
  rewrite deferred_workload_sweep_visits. cbn. nia.
Qed.

Theorem same_cost_full_sweep_nonincrease : forall allocation release inspection count,
  ownership_workload_cost allocation release count <=
    gc_full_sweep_workload_cost allocation release inspection count.
Proof. intros. rewrite equal_policy_cost_accounting. lia. Qed.

Theorem positive_inspection_full_sweep_strict_advantage : forall allocation release inspection count,
  0 < inspection -> 0 < count ->
  ownership_workload_cost allocation release count <
    gc_full_sweep_workload_cost allocation release inspection count.
Proof.
  intros allocation release inspection count HI HC.
  rewrite equal_policy_cost_accounting. nia.
Qed.

Theorem zero_inspection_weight_refutes_strict_speed : forall allocation release count,
  ownership_workload_cost allocation release count =
    gc_full_sweep_workload_cost allocation release 0 count.
Proof. intros. rewrite equal_policy_cost_accounting. cbn. lia. Qed.

(* Different policies are essential for sharing/copying collectors. These
   weights are explicit hypothetical inputs; no policy is called measured. *)
Record MemoryCostPolicy := {
  allocation_weight : nat;
  copy_weight : nat;
  reclamation_weight : nat;
  inspection_weight : nat;
  barrier_weight : nat
}.
Definition policy_cost (p : MemoryCostPolicy)
  (allocations copies reclaimed inspected barriers : nat) : nat :=
  allocation_weight p * allocations + copy_weight p * copies +
  reclamation_weight p * reclaimed + inspection_weight p * inspected +
  barrier_weight p * barriers.

Definition unit_memory_policy : MemoryCostPolicy :=
  {| allocation_weight := 1; copy_weight := 1; reclamation_weight := 1;
     inspection_weight := 1; barrier_weight := 1 |}.

Example shared_reference_policy_can_cost_less :
  policy_cost unit_memory_policy 1 0 1 3 2 <
    policy_cost unit_memory_policy 1 10 1 0 0.
Proof. vm_compute. repeat constructor. Qed.

Example ownership_policy_can_cost_less :
  policy_cost unit_memory_policy 3 0 3 0 0 <
    policy_cost unit_memory_policy 3 0 3 3 0.
Proof. vm_compute. repeat constructor. Qed.

Example different_allocation_policy_can_reverse_comparison :
  policy_cost unit_memory_policy 3 0 1 0 0 <
    policy_cost {| allocation_weight := 2; copy_weight := 1;
      reclamation_weight := 1; inspection_weight := 1; barrier_weight := 1 |}
      3 0 3 0 0.
Proof. vm_compute. repeat constructor. Qed.

(* An empty nursery could instead be reset in one operation. The core does
   not model that allocator, so it cannot assert universal speed superiority.
   Common allocation/application costs cancel; only reclamation is compared. *)
Example bulk_reset_cost_refutes_universal_speed :
  1 < ownership_workload_cost 0 1 3.
Proof. vm_compute. repeat constructor. Qed.

Example fixed_full_sweep_cost_witness :
  ownership_workload_cost 1 1 3 = 6 /\
  gc_full_sweep_workload_cost 1 1 1 3 = 9.
Proof. split; reflexivity. Qed.
