(* Independent proposition consumers and concrete falsifiers. These inputs
   are model fixtures, not production EV/DRV-2 witness exports. *)
From Stdlib Require Import Lists.List Bool.Bool Arith.PeanoNat.
Require Import OwnershipCleanCore OwnershipRefusalExplanation.
Import ListNotations RefusalExplanation.

Definition audit_growth : forall moves op old fresh,
  moves op = true -> old <> fresh ->
  run moves empty_heap
    [Allocate 0 old; CopyDescriptor 1 0; Mutate op 1 fresh; ReadDescriptor 0]
    = StaleRead 0 old := copied_descriptor_growth_stale.
Definition audit_saturation : forall p s x,
  In x (rounds p s (length (universe p))) <-> Shared p s x := saturation_is_sharing.
Definition audit_termination : forall p s,
  incl (rounds p s (S (length (universe p)))) (rounds p s (length (universe p))) := finite_saturation.
Definition audit_height : forall p s n x,
  In x (rounds p s n) <->
  exists d, ValidTree p s d /\ target d = x /\ height d <= n := rounds_derivation_height.
Definition audit_first_height : forall p s n x,
  In x (rounds p s (S n)) -> ~ In x (rounds p s n) ->
  (exists d, ValidTree p s d /\ target d = x /\ height d = S n) /\
  (forall d, ValidTree p s d -> target d = x -> S n <= height d) := first_round_is_minimum_height.
Definition audit_exclusive_fixed_point : forall p s x,
  Exclusive p s x <->
  (forall r, In r (seeds p) -> seed_at r = x -> enabled s (seed_guard r) <> true) /\
  (forall e, In e (edges p) -> edge_to e = x -> enabled s (edge_guard e) = true ->
    Exclusive p s (edge_from e)) := exclusive_fixed_point.
Definition audit_exclusive_greatest : forall p s (candidate : nat -> Prop),
  (forall x, candidate x ->
    (forall r, In r (seeds p) -> seed_at r = x -> enabled s (seed_guard r) <> true) /\
    (forall e, In e (edges p) -> edge_to e = x -> enabled s (edge_guard e) = true -> candidate (edge_from e))) ->
  forall x, candidate x -> Exclusive p s x := exclusive_greatest.
Definition audit_sound : forall p s requested w,
  check_witness p s requested w = true -> ValidWitness p s requested w := check_witness_sound.
Definition audit_complete : forall p s requested w,
  ValidWitness p s requested w -> check_witness p s requested w = true := check_witness_complete.
Definition audit_refusal_cause : forall p s requested,
  refusal p s requested = true -> exists w, check_witness p s requested w = true := refusal_has_witness.
Definition audit_refusal_binding : forall p s requested w,
  check_witness p s requested w = true -> refusal p s requested = true := checked_witness_refuses.
Definition audit_classification : forall p s requested w,
  check_witness p s requested w = true -> exists b,
  nth_error (boundaries p) requested = Some b /\
  ((witness_cause w = Hazard /\ exists buffer, trace_outcome p b = StaleRead (boundary_target b) buffer) \/
   (witness_cause w = ExclusivityAt (boundary_target b) /\
    ~ Exclusive p s (boundary_target b) /\ stale_at (boundary_target b) (trace_outcome p b) = false)) := witness_classified.
Definition audit_exclusive_classification : forall p s b x,
  ValidCause p s b Hazard -> ~ ValidCause p s b (ExclusivityAt x) := hazard_and_missing_disjoint.
Definition audit_locality : forall p s t requested w,
  agree_on (witness_support p requested w) s t ->
  check_witness p s requested w = check_witness p t requested w := witness_sufficient.
Definition audit_responsibility : forall p accepted refused requested,
  refusal p accepted requested = false -> refusal p refused requested = true ->
  exists w k, check_witness p refused requested w = true /\
    In k (witness_support p requested w) /\ accepted k <> refused k := remote_break_attributed.
Definition audit_effect_order : forall p precise coarse requested,
  (forall k, precise k = true -> coarse k = true) ->
  refusal p precise requested = true -> refusal p coarse requested = true := fewer_effects_fewer_refusals.

Definition test_seed := seed UnconsumedAlias 0 (Some 10).
Definition test_e1 := edge Assignment 0 1 None.
Definition test_e2 := edge ArgumentToResult 1 2 (Some 11).
Definition test_e3 := edge FieldAccess 2 3 None.
Definition test_loop := edge Assignment 3 3 None.
Definition test_tree := BecauseTransfer test_e3
  (BecauseTransfer test_e2 (BecauseTransfer test_e1 (BecauseSeed test_seed))).
Definition test_summary (k : nat) := (k =? 10) || (k =? 11) || (k =? 12).
Definition test_trace :=
  [Allocate 3 (0,0); CopyDescriptor 4 3; Mutate 7 4 (1,0); ReadDescriptor 3].
Definition test_boundary := boundary 40 3 (Some 12) test_trace.
Definition test_program := program [test_seed] [test_e1; test_e2; test_e3; test_loop]
  [test_boundary; boundary 41 0 None []] (fun op => op =? 7).
Definition test_hazard := witness 0 40 test_tree Hazard.
Definition with_trace (ops : list Op) := program (seeds test_program) (edges test_program)
  [boundary 40 3 (Some 12) ops] (relocates test_program).
Definition test_missing := witness 0 40 test_tree (ExclusivityAt 3).
Definition no_move_program := program (seeds test_program) (edges test_program)
  [test_boundary] (fun _ => false).
Definition seedless_cycle := program [] [edge Assignment 0 1 None; edge Assignment 1 0 None]
  [boundary 40 0 None []] (fun _ => false).
Definition without (key : nat) : Summary := fun k => negb (k =? key) && test_summary k.
Definition wrong_parent := BecauseTransfer test_e3 (BecauseSeed test_seed).
Definition wrong_summary_tree := BecauseTransfer test_e3
  (BecauseTransfer (edge ArgumentToResult 1 2 (Some 99))
    (BecauseTransfer test_e1 (BecauseSeed test_seed))).

Example actual_stale_accepted : check_witness test_program test_summary 0 test_hazard = true.
Proof. vm_compute. reflexivity. Qed.
Example all_refusals_explained :
  map (refusal test_program test_summary) [0;1] = [true;true] /\
  check_witness test_program test_summary 0 test_hazard = true /\
  check_witness test_program test_summary 1
    (witness 1 41 (BecauseSeed test_seed) (ExclusivityAt 0)) = true.
Proof. vm_compute. auto. Qed.
Example no_move_is_missing_not_hazard :
  check_witness no_move_program test_summary 0 test_hazard = false /\
  check_witness no_move_program test_summary 0 test_missing = true.
Proof. vm_compute. auto. Qed.
Example read_before_growth_is_not_hazard :
  check_witness (with_trace [Allocate 3 (0,0); CopyDescriptor 4 3;
    ReadDescriptor 3; Mutate 7 4 (1,0)]) test_summary 0 test_hazard = false.
Proof. vm_compute. reflexivity. Qed.
Example unrelated_stale_read_is_not_this_hazard :
  check_witness (with_trace [Allocate 0 (0,0); CopyDescriptor 4 0;
    Mutate 7 4 (1,0); ReadDescriptor 0]) test_summary 0 test_hazard = false.
Proof. vm_compute. reflexivity. Qed.
Example invalid_allocation_is_not_hazard :
  check_witness (with_trace [Allocate 3 (0,0); CopyDescriptor 4 3;
    Mutate 7 4 (0,0); ReadDescriptor 3]) test_summary 0 test_hazard = false.
Proof. vm_compute. reflexivity. Qed.
Example unrelated_boundary_rejected : check_witness test_program test_summary 1 test_hazard = false.
Proof. vm_compute. reflexivity. Qed.
Example unknown_boundary_rejected : check_witness test_program test_summary 9 test_hazard = false.
Proof. vm_compute. reflexivity. Qed.
Example forged_location_rejected :
  check_witness test_program test_summary 0 (witness 0 41 test_tree Hazard) = false.
Proof. vm_compute. reflexivity. Qed.
Example forged_parent_rejected :
  check_witness test_program test_summary 0 (witness 0 40 wrong_parent Hazard) = false.
Proof. vm_compute. reflexivity. Qed.
Example forged_summary_rejected :
  check_witness test_program (fun _ => true) 0 (witness 0 40 wrong_summary_tree Hazard) = false.
Proof. vm_compute. reflexivity. Qed.
Example forged_seed_rejected :
  check_tree test_program test_summary (BecauseSeed (seed ConstructorOverlap 0 (Some 10))) = false.
Proof. vm_compute. reflexivity. Qed.
Example forged_target_rejected :
  check_witness test_program test_summary 0 (witness 0 40 (BecauseSeed test_seed) Hazard) = false.
Proof. vm_compute. reflexivity. Qed.
Example forged_missing_fact_rejected :
  check_witness no_move_program test_summary 0 (witness 0 40 test_tree (ExclusivityAt 99)) = false.
Proof. vm_compute. reflexivity. Qed.
Example hazard_cannot_be_misclassified : check_witness test_program test_summary 0 test_missing = false.
Proof. vm_compute. reflexivity. Qed.
Example seedless_cycle_has_no_evidence :
  saturated seedless_cycle test_summary = [] /\ refusal seedless_cycle test_summary 0 = false.
Proof. vm_compute. auto. Qed.
Example shortest_height_not_arbitrary_height :
  member Nat.eq_dec 3 (rounds test_program test_summary 3) = false /\
  member Nat.eq_dec 3 (rounds test_program test_summary 4) = true /\
  height test_tree = 4 /\ height (BecauseTransfer test_loop test_tree) = 5 /\
  check_tree test_program test_summary (BecauseTransfer test_loop test_tree) = true.
Proof. vm_compute. auto. Qed.
Example changed_guard_is_attributed :
  witness_support test_program 0 test_hazard = [12;11;10] /\
  map (fun k => check_witness test_program (without k) 0 test_hazard) [10;11;12] = [false;false;false] /\
  map (fun k => refusal test_program (without k) 0) [10;11;12] = [false;false;false] /\
  check_witness test_program (fun k => (k =? 99) || test_summary k) 0 test_hazard = true.
Proof. vm_compute. auto. Qed.

(* Every rule tag is executable, including the explicit unknown-analysis seed.
   Tags do not prove that the production exporter emits the correct rule. *)
Example all_seed_categories :
  let rs := map (fun kind => seed kind 0 None)
    [ConstructorOverlap; UnconsumedAlias; RefOrigin; IndexOrigin; SharedResultField; UnavailableAnalysisFact] in
  let p := program rs [] [] (fun _ => false) in
  map (fun r => check_tree p (fun _ => false) (BecauseSeed r)) rs = repeat true 6.
Proof. vm_compute. reflexivity. Qed.
