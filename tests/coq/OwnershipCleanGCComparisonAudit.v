(* Typed consumers prevent names-only or weaker-comparison green results. *)
Require Import OwnershipCleanCore.
Require Import OwnershipCleanGCComparison.
Require Import Stdlib.Lists.List.
Require Import Stdlib.Arith.PeanoNat.
Require Import Stdlib.Sorting.Permutation.
Import ListNotations.

Definition audit_retention_contract : forall rho beta H n R G,
  INV rho beta H n R -> gc_read_coverage (owner_roots rho R) G ->
  length H <= length G := ownership_retains_no_more_blocks.

Definition audit_correct_gc_contract : forall rho beta H n R G,
  INV rho beta H n R -> gc_read_coverage (owner_roots rho R) G ->
  Permutation (fst (full_heap_sweep (owner_roots rho R) G)) H :=
  full_heap_sweep_matches_exact_ownership.

Definition audit_canonical_elaboration_contract : forall count,
  elab no_summaries (retirement_program count) [] [] =
    Some (retirement_target count, []) := retirement_program_uses_canonical_elab.

Definition audit_canonical_execution_contract : forall tf tp count frontier,
  texec tf tp [] [] [] frontier (retirement_target count)
    [] [] [] (frontier + count) (repeat (SLeaf 7) count) := retirement_program_runs_clean.

Definition audit_source_execution_contract : forall funs procs count sg,
  exists sg', sexec funs procs sg (retirement_program count) sg'
    (repeat (SLeaf 7) count) := retirement_program_source_trace.

Definition audit_read_safety_contract : forall rho beta H n R G z c bs,
  INV rho beta H n R -> gc_read_coverage (owner_roots rho R) G ->
  tread z rho beta = Some (c, bs) -> incl bs G := collector_covers_canonical_reads.

Definition audit_deferred_workload_contract : forall count,
  gc_read_coverage [] (retirement_blocks count) := deferred_workload_is_read_safe.

Definition audit_cost_contract : forall allocation release inspection count,
  0 < inspection -> 0 < count ->
  ownership_workload_cost allocation release count <
    gc_full_sweep_workload_cost allocation release inspection count :=
  positive_inspection_full_sweep_strict_advantage.

Definition audit_read_safety_not_exact_contract :
  gc_read_coverage [] [(0, 0)] /\ ~ INV [] [] [(0, 0)] 1 [] :=
  read_safe_retention_is_not_exact_reclamation.

Definition audit_no_universal_speed_contract : forall allocation release count,
  ownership_workload_cost allocation release count =
    gc_full_sweep_workload_cost allocation release 0 count :=
  zero_inspection_weight_refutes_strict_speed.

Example audit_sweep_retains_live_and_reclaims_dead :
  full_heap_sweep [(0, 0)] [(0, 0); (1, 0)] = ([(0, 0)], 2).
Proof. reflexivity. Qed.

Example audit_empty_and_nonempty_sweep :
  full_heap_sweep [] (retirement_blocks 0) = ([], 0) /\
  full_heap_sweep [] (retirement_blocks 3) = ([], 3).
Proof. split; reflexivity. Qed.

Example audit_live_reclamation_rejected : ~ gc_read_coverage [(0, 0)] [] :=
  reclaiming_a_live_block_is_not_read_safe.

Example audit_positive_costs :
  ownership_workload_cost 1 1 3 = 6 /\
  gc_full_sweep_workload_cost 1 1 1 3 = 9 := fixed_full_sweep_cost_witness.

Example audit_bulk_reset_counterexample :
  1 < ownership_workload_cost 0 1 3 := bulk_reset_cost_refutes_universal_speed.

Definition audit_independent_accounting : forall allocation release inspection count,
  gc_full_sweep_workload_cost allocation release inspection count =
    ownership_workload_cost allocation release count + inspection * count :=
  equal_policy_cost_accounting.

Example audit_shared_reference_counterexample :
  policy_cost unit_memory_policy 1 0 1 3 2 <
    policy_cost unit_memory_policy 1 10 1 0 0 := shared_reference_policy_can_cost_less.

Example audit_ownership_policy_witness :
  policy_cost unit_memory_policy 3 0 3 0 0 <
    policy_cost unit_memory_policy 3 0 3 3 0 := ownership_policy_can_cost_less.

Definition audit_allocation_policy_counterexample :=
  different_allocation_policy_can_reverse_comparison.

(* The kernel checks the module's axiom profile; these reports also expose the
   theorem assumptions and do not stand in for the kernel verdict. *)
Print Assumptions audit_retention_contract.
Print Assumptions audit_correct_gc_contract.
Print Assumptions audit_canonical_execution_contract.
Print Assumptions audit_cost_contract.
