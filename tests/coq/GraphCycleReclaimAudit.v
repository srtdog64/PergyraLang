(* Independent consumers pin propositions, not just theorem names. These are
   model checks, not a real MIR certificate producer or native runtime test. *)
Require Import Stdlib.Lists.List Stdlib.Arith.PeanoNat.
Require Import OwnershipCleanCore OwnershipGraphLinks.
Require Import OwnershipGraphCycleReclaim OwnershipGraphRootCompleteness.
Require OwnershipTeardown OwnershipTeardownAuthority.
Require OwnershipTeardownAtomicBatch.
Import ListNotations.
Module TD := OwnershipTeardown.
Module TA := OwnershipTeardownAuthority.
Module AB := OwnershipTeardownAtomicBatch.

Theorem audit_selection_contract : forall cnt fuel s roots C G,
  (forall k, indeg (sslots s) k <= cnt k) ->
  trial_garbage_with cnt fuel s roots C = Some G ->
  forall j, reach s roots j -> ~ In j G.
Proof. exact trial_garbage_with_unreachable. Qed.

Theorem audit_counted_run_contract : forall gmax ops c,
  CountsExact (counted_counts c) (counted_graph c) ->
  AllEdgesIssued (counted_graph c) -> counted_run_admitted gmax c ops ->
  CountsExact (counted_counts (counted_grun gmax c ops))
    (counted_graph (counted_grun gmax c ops)) /\
  AllEdgesIssued (counted_graph (counted_grun gmax c ops)).
Proof. exact counted_run_preserves_counts. Qed.

Theorem audit_reference_run_contract : forall gmax funs rights ctx,
  FunsOK funs -> forall K rho g s rho' g' tr o,
  rexec gmax funs rights ctx links_of true false K rho g s rho' g' tr o ->
  forall L E Lin r,
  rcheck s L E = Some Lin -> SimInv gmax K rho Lin g r ->
  exists r', rexec gmax funs rights ctx links_of true true K rho r s rho' r' tr o /\
    SimInv gmax K rho' (out_live o L E) g' r'.
Proof. exact reclaim_simulates. Qed.

Theorem audit_checked_boundary_contract : forall gmax a bindings c c' K rho L R sid C fuel g,
  CountsExact (counted_counts c) (counted_graph c) -> AllEdgesIssued (counted_graph c) ->
  SimInv gmax K rho L g (counted_graph c) ->
  checked_ledger_reclaim gmax a bindings c K rho L R sid C fuel = CountedBatchAccepted c' ->
  SimInv gmax K rho L g (counted_graph c') /\
  CountsExact (counted_counts c') (counted_graph c') /\ AllEdgesIssued (counted_graph c').
Proof. exact checked_ledger_reclaim_simulates. Qed.

Theorem audit_ledger_transfer_contract : forall a after bindings sid r epoch new_holder,
  TA.transfer_cleanup a (r, epoch) new_holder = TA.Accepted after ->
  new_holder <> TA.acting_context a -> bindings sid = Some (r, epoch) ->
  holds (ledger_rights after bindings) (TA.acting_context after) sid = false.
Proof. exact ledger_transfer_revokes. Qed.

Theorem audit_ledger_consumption_contract : forall a after bindings sid r epoch unit,
  TA.retire a (TA.RetireRoot (r, epoch)) unit = TA.Accepted after ->
  bindings sid = Some (r, epoch) ->
  holds (ledger_rights after bindings) (TA.acting_context after) sid = false.
Proof. exact ledger_consumption_revokes. Qed.

(* A recount is only test initialization, not the counted operation or a
   locality/performance claim. Every subsequent counted step uses its deltas. *)
Definition audit_counted (g : GS) : CountedGraph := mkCountedGraph g
  (fun sid k => match find_store sid (gstores g) with
                | Some s => indeg (sslots s) k | None => 0 end).

Theorem audit_initial_count_exact : forall g,
  CountsExact (counted_counts (audit_counted g)) g.
Proof. intros g sid s H k. cbn. rewrite H. reflexivity. Qed.

Definition audit_counted_cycle_retired :=
  counted_grun 3 (audit_counted gcyc) (reclaim_ops cyc_store [2; 3]).

Example audit_counted_reclaim_executes :
  counted_reclaim 3 (audit_counted gcyc) 0 [2; 3] 2 cyc_roots =
    CountedBatchAccepted audit_counted_cycle_retired /\
  counted_graph audit_counted_cycle_retired = gcyc_reclaimed /\
  counted_counts audit_counted_cycle_retired 0 2 = 0 /\
  counted_counts audit_counted_cycle_retired 0 3 = 0.
Proof. repeat split; reflexivity. Qed.

(* The witnesses must actually be accepted by the canonical machine. A
   refused-result placeholder cannot satisfy the equations below. *)
Definition audit_live : TA.AuthorityState :=
  match TA.create_root (TA.empty_authority 0) 0 with
  | TA.Accepted a => a | TA.Refused _ _ => TA.empty_authority 42 end.
Definition audit_transferred : TA.AuthorityState :=
  match TA.transfer_cleanup audit_live (0, 0) 1 with
  | TA.Accepted a => a | TA.Refused _ _ => TA.empty_authority 42 end.
Definition audit_consumed : TA.AuthorityState :=
  match TA.retire audit_live (TA.RetireRoot (0, 0)) [] with
  | TA.Accepted a => a | TA.Refused _ _ => TA.empty_authority 42 end.
Definition audit_reissued : TA.AuthorityState :=
  match TA.create_root audit_consumed 0 with
  | TA.Accepted a => a | TA.Refused _ _ => TA.empty_authority 42 end.
Definition audit_binding (epoch : nat) : StoreRootBindings :=
  fun sid => if Nat.eqb sid 0 then Some (0, epoch) else None.

Example audit_ledger_operations_admitted :
  TA.create_root (TA.empty_authority 0) 0 = TA.Accepted audit_live /\
  TA.transfer_cleanup audit_live (0, 0) 1 = TA.Accepted audit_transferred /\
  TA.retire audit_live (TA.RetireRoot (0, 0)) [] = TA.Accepted audit_consumed /\
  TA.create_root audit_consumed 0 = TA.Accepted audit_reissued.
Proof. repeat split; reflexivity. Qed.

Example audit_ledger_identity_and_holder :
  holds (ledger_rights audit_live (audit_binding 0)) 0 0 = true /\
  holds (ledger_rights audit_live (fun _ => None)) 0 0 = false /\
  holds (ledger_rights (TA.in_context audit_live 1) (audit_binding 0)) 1 0 = false /\
  holds (ledger_rights audit_transferred (audit_binding 0)) 0 0 = false /\
  holds (ledger_rights (TA.in_context audit_transferred 1) (audit_binding 0)) 1 0 = true /\
  holds (ledger_rights audit_consumed (audit_binding 0)) 0 0 = false /\
  holds (ledger_rights audit_reissued (audit_binding 0)) 0 0 = false /\
  holds (ledger_rights audit_reissued (audit_binding 1)) 0 0 = true.
Proof. repeat split; reflexivity. Qed.

Example audit_denial_keeps_counts_and_storage :
  ledger_counted_reclaim 3 audit_consumed (audit_binding 0)
    (audit_counted gcyc) 0 [2; 3] 2 cyc_roots =
      CountedBatchRefused (audit_counted gcyc) BatchAuthorityDenied /\
  ledger_counted_reclaim 3 audit_transferred (audit_binding 0)
    (audit_counted gcyc) 0 [2; 3] 2 cyc_roots =
      CountedBatchRefused (audit_counted gcyc) BatchAuthorityDenied /\
  ledger_counted_reclaim 3 audit_reissued (audit_binding 0)
    (audit_counted gcyc) 0 [2; 3] 2 cyc_roots =
      CountedBatchRefused (audit_counted gcyc) BatchAuthorityDenied.
Proof. repeat split; reflexivity. Qed.

Example audit_incomplete_roots_refused :
  checked_ledger_reclaim 3 audit_live (audit_binding 0) (audit_counted ex_g)
    [] ex_rho [1] [] 0 [0] 2 =
      CountedBatchRefused (audit_counted ex_g) BatchIncompleteRoots.
Proof. reflexivity. Qed.

Theorem audit_deep_aggregate_falsifier :
  rcheck agg_prog [] [] = Some [1] /\
  (exists rho' g', rexec 3 ex_nofuns ex_rights 0 links_of true true [] ex_rho ex_g agg_prog rho' g' [7] RNorm) /\
  (forall rho' g' tr o, rexec 3 ex_nofuns ex_rights 0 top_links true true [] ex_rho ex_g agg_prog rho' g' tr o ->
    o = RErr /\ tr = []).
Proof. exact aggregate_only_link_needs_deep_roots. Qed.

Theorem audit_caller_frame_falsifier :
  rcheck (frame_prog [1]) [] [] = Some [1] /\
  (exists rho' g', rexec 3 frame_funs ex_rights 0 links_of true true [] ex_rho ex_g (frame_prog [1]) rho' g' [7] RNorm) /\
  (forall rho' g' tr o, rexec 3 frame_funs ex_rights 0 links_of false true [] ex_rho ex_g (frame_prog [1]) rho' g' tr o ->
    o = RErr /\ tr = []) /\
  rcheck (frame_prog []) [] [] = None /\
  (forall rho' g' tr o, rexec 3 frame_funs ex_rights 0 links_of true true [] ex_rho ex_g (frame_prog []) rho' g' tr o ->
    o = RErr /\ tr = []).
Proof. exact caller_frame_link_needs_frame_roots. Qed.

Theorem audit_view_falsifier :
  rcheck view_prog [] [] = Some [1] /\
  (exists rho' g', rexec 3 ex_nofuns ex_rights 0 links_of true true [] ex_rho ex_g view_prog rho' g' [7] RNorm) /\
  (forall rho' g' tr o, rexec 3 ex_nofuns ex_rights 0 links_no_views true true [] ex_rho ex_g view_prog rho' g' tr o ->
    o = RErr /\ tr = []).
Proof. exact view_only_backing_needs_view_roots. Qed.

Theorem audit_return_falsifier :
  rcheck ret_prog [] [] = Some [1] /\
  (exists rho' g', rexec 3 (ret_funs [6]) ex_rights 0 links_of true true [] ex_rho ex_g ret_prog rho' g' [7] RNorm) /\
  rcheck (ret_body []) [6] [] = None /\
  (forall rho' g' tr o, rexec 3 (ret_funs []) ex_rights 0 links_of true true [] ex_rho ex_g ret_prog rho' g' tr o ->
    o = RErr /\ tr = []).
Proof. exact returned_link_must_stay_rooted. Qed.

Theorem audit_generation_counter_falsifier :
  trial_garbage_with index_counter 1 reuse_store cyc_roots [2] = Some [2] /\
  reach reuse_store cyc_roots 2 /\
  trial_garbage_with identity_counter 1 reuse_store cyc_roots [2] = Some [].
Proof. exact index_decrement_after_reuse_deletes_reachable. Qed.

Theorem audit_closure_falsifier :
  In 2 (naive_trial chain_store cyc_roots [1; 2; 3]) /\ reach chain_store cyc_roots 2.
Proof. exact naive_trial_deletes_reachable. Qed.

Example audit_small_budget_defers : trial_garbage 0 chain_store cyc_roots [1; 2; 3] = None.
Proof. exact small_budget_defers. Qed.

Example audit_allocation_observes_reuse :
  snd (gexec 3 gcyc (OInsert 0 2 9 [] [(10, 0)] (11, 0))) = GRefused RConflict /\
  snd (gexec 3 gcyc_reclaimed (OInsert 0 2 9 [] [(10, 0)] (11, 0))) = GLink (mkLink 0 2 1).
Proof. exact reclaimed_slot_reuse_is_visible. Qed.

(* Baseline retention is not UB or a claim about optimal runtime cost. *)
Theorem audit_manual_lifecycle_retains_unreachable :
  let g := grun 3 gempty [ONew (0, 0); OInsert 0 0 1 [(1, 0)] [(2, 0)] (1, 0);
                          OInsert 0 1 2 [(0, 0)] [(4, 0)] (3, 0);
                          OInsert 0 2 3 [(3, 0)] [(6, 0)] (5, 0);
                          OInsert 0 3 4 [(2, 0)] [(8, 0)] (7, 0)] in
  length (gheap g) = 5.
Proof. exact unreachable_nodes_retained. Qed.

(* The legacy sequential runner remains a falsifier, not reclaim's execution
   path. This is outside the checked language's empty-borrow premise. *)
Definition audit_borrowed_cycle : GS :=
  fst (gexec 3 gcyc (OBegin (mkLink 0 3 0) false)).

Lemma audit_borrowed_cycle_valid : GInv 3 audit_borrowed_cycle.
Proof. unfold audit_borrowed_cycle. apply ginv_step. apply gcyc_inv. Qed.

Definition audit_partial_reclaim : GS := counted_graph
  (counted_grun 3 (audit_counted audit_borrowed_cycle)
    (reclaim_ops cyc_store [2; 3])).

Example audit_borrowed_batch_is_not_atomic :
  trial_garbage_with (counted_counts (audit_counted audit_borrowed_cycle) 0)
    2 cyc_store cyc_roots [2; 3] = Some [2; 3] /\
  gresults 3 audit_borrowed_cycle (reclaim_ops cyc_store [2; 3]) =
    [GUnit; GRefused RBorrowed] /\
  length (gheap audit_borrowed_cycle) = 5 /\
  length (gheap audit_partial_reclaim) = 4 /\
  resolve audit_partial_reclaim link2 = Missing RStale /\
  resolve audit_partial_reclaim (mkLink 0 3 0) =
    Found (mkNode 4 [(2, 0)] [(8, 0)]).
Proof. repeat split; reflexivity. Qed.

Theorem audit_graph_batch_refusal_contract : forall gmax c ls why after,
  counted_delete_batch gmax c ls = CountedBatchRefused after why -> after = c.
Proof. intros. eapply counted_delete_batch_refused_unchanged; eauto. Qed.

Example audit_borrowed_batch_atomic_refusal :
  counted_reclaim 3 (audit_counted audit_borrowed_cycle) 0 [2; 3] 2 cyc_roots =
    CountedBatchRefused (audit_counted audit_borrowed_cycle)
      (BatchDeleteRefused (mkLink 0 3 0) RBorrowed) /\
  counted_delete_batch 3 (audit_counted audit_borrowed_cycle)
    [mkLink 0 3 0; mkLink 0 2 0] =
    CountedBatchRefused (audit_counted audit_borrowed_cycle)
      (BatchDeleteRefused (mkLink 0 3 0) RBorrowed).
Proof. split; reflexivity. Qed.

Example audit_checked_boundary_borrow_refused :
  ledger_counted_reclaim 3 audit_live (audit_binding 0)
    (audit_counted audit_borrowed_cycle) 0 [2; 3] 2 cyc_roots =
    CountedBatchRefused (audit_counted audit_borrowed_cycle)
      (BatchDeleteRefused (mkLink 0 3 0) RBorrowed) /\
  checked_ledger_reclaim 3 audit_live (audit_binding 0)
    (audit_counted audit_borrowed_cycle) cyc_roots rempty [] [] 0 [2; 3] 2 =
    CountedBatchRefused (audit_counted audit_borrowed_cycle)
      (BatchDeleteRefused (mkLink 0 3 0) RBorrowed).
Proof. split; reflexivity. Qed.

Example audit_graph_batch_duplicate_and_stale_refused :
  counted_delete_batch 3 (audit_counted gcyc) [link2; link2] =
    CountedBatchRefused (audit_counted gcyc) (BatchDuplicate link2) /\
  counted_delete_batch 3 (audit_counted gcyc) [mkLink 0 2 1; mkLink 0 3 0] =
    CountedBatchRefused (audit_counted gcyc)
      (BatchDeleteRefused (mkLink 0 2 1) RStale) /\
  counted_delete_batch 3 (audit_counted gcyc) [link2; mkLink 0 99 0] =
    CountedBatchRefused (audit_counted gcyc)
      (BatchDeleteRefused (mkLink 0 99 0) RStale) /\
  counted_delete_batch 3 audit_counted_cycle_retired [mkLink 0 2 1] =
    CountedBatchRefused audit_counted_cycle_retired
      (BatchDeleteRefused (mkLink 0 2 1) RStale).
Proof. repeat split; reflexivity. Qed.

Example audit_graph_batch_empty_and_budget_explicit :
  counted_delete_batch 3 (audit_counted gcyc) [] =
    CountedBatchAccepted (audit_counted gcyc) /\
  counted_reclaim 3 (audit_counted gcyc) 0 [2; 3] 0 cyc_roots =
    CountedBatchAccepted audit_counted_cycle_retired /\
  counted_reclaim 3 (audit_counted (mkGS [chain_store] [] 1 [] []))
    0 [1; 2; 3] 0 cyc_roots =
      CountedBatchRefused (audit_counted (mkGS [chain_store] [] 1 [] []))
        BatchSelectionDeferred /\
  counted_reclaim 3 (audit_counted gcyc) 99 [2; 3] 2 cyc_roots =
    CountedBatchRefused (audit_counted gcyc) (BatchMissingStore 99).
Proof. repeat split; reflexivity. Qed.

Definition audit_reclaimed_cycle_store : Store :=
  match find_store 0 (gstores gcyc_reclaimed) with
  | Some s => s
  | None => mkStore 0 (0, 0) []
  end.

(* Full-inventory copying is not an RStmt observation. It needs its own
   complete read-footprint contract before claiming reclaim refinement. *)
Example audit_full_inventory_snapshot_observes_reclaim :
  option_map ndata (follow (snapshot cyc_store 1 20) (2, 0)) = Some 3 /\
  option_map ndata (follow (snapshot audit_reclaimed_cycle_store 1 20) (2, 0)) = None.
Proof. split; reflexivity. Qed.

(* Proposition-typed consumers prevent a vacuous replacement of the importing
   batch theorems. These compose canonical retirement, not physical frees. *)
Theorem audit_canonical_batch_refusal_contract : forall requests a why after,
  AB.retire_batch a requests = TA.Refused why after -> after = a.
Proof. exact AB.retire_batch_refusal_unchanged. Qed.

Theorem audit_canonical_batch_execution_contract : forall requests a after,
  AB.retire_batch a requests = TA.Accepted after <->
  AB.InitialBatchAdmitted a requests /\ AB.AdmittedRetirementBatch a requests after.
Proof. exact AB.retire_batch_accepted_iff. Qed.

Theorem audit_canonical_batch_invariant_contract : forall requests a after,
  TA.AuthorityInvariant a -> AB.retire_batch a requests = TA.Accepted after ->
  TA.AuthorityInvariant after.
Proof. exact AB.retire_batch_preserves_authority. Qed.

Theorem audit_canonical_node_batch_rights_contract : forall requests a after,
  AB.NodeOnlyBatch requests -> AB.retire_batch a requests = TA.Accepted after ->
  TA.cleanup_rights after = TA.cleanup_rights a.
Proof. exact AB.retire_batch_node_rights_unchanged. Qed.

Theorem audit_canonical_batch_lease_contract : forall requests a after,
  AB.retire_batch a requests = TA.Accepted after ->
  TA.acting_context after = TA.acting_context a /\
  TA.active_leases after = TA.active_leases a /\
  TA.next_lease after = TA.next_lease a.
Proof. exact AB.retire_batch_leases_unchanged. Qed.

Theorem audit_canonical_all_initial_requests_contract : forall requests a after,
  AB.retire_batch a requests = TA.Accepted after ->
  Forall (fun req =>
    TA.target_current (TA.forest a) (AB.batch_target req) = true /\
    TA.target_authorized a (AB.batch_target req) = true /\
    TA.valid_unit (TA.forest a) (AB.batch_target req) (AB.batch_unit req) = true /\
    TA.unit_quiet a (AB.batch_unit req) = true) requests.
Proof. exact AB.retire_batch_initial_requests_checked. Qed.

Theorem audit_canonical_initial_exact_units_contract : forall requests a after,
  TD.Inv (TA.forest a) -> AB.retire_batch a requests = TA.Accepted after ->
  Forall (fun req => TA.UnitExact (TA.forest a)
    (AB.batch_target req) (AB.batch_unit req)) requests.
Proof. exact AB.retire_batch_initial_requests_exact. Qed.

Theorem audit_canonical_disjoint_units_contract : forall requests a after,
  AB.retire_batch a requests = TA.Accepted after -> AB.DisjointRetirementUnits requests.
Proof. exact AB.retire_batch_initial_units_disjoint. Qed.

Theorem audit_canonical_targets_unique_contract : forall requests a after,
  AB.retire_batch a requests = TA.Accepted after -> NoDup (map AB.batch_target requests).
Proof. exact AB.retire_batch_targets_unique. Qed.

(* Test-only result projection: the subsequent admission equations require
   every fixture issuer to return Accepted; a refusal cannot stand in for a
   constructed state. This is not a production fallback or binding issuer. *)
Definition audit_authority_result_state (result : TA.AuthorityResult) :=
  match result with TA.Accepted a | TA.Refused _ a => a end.
Definition audit_flat0 := audit_authority_result_state
  (TA.allocate_owned audit_live 0 (TD.ORoot 0) 0).
Definition audit_flat1 := audit_authority_result_state
  (TA.allocate_owned audit_flat0 1 (TD.ORoot 0) 0).
Definition audit_nested := audit_authority_result_state
  (TA.allocate_owned audit_flat1 2 (TD.OParent 1) 0).
Definition audit_late_lease (kind : TA.LeaseKind) := audit_authority_result_state
  (TA.begin_lease audit_nested (2, 0) kind 0).
Definition audit_nested_requests :=
  [AB.mkRetirementRequest (TA.RetireNode (0, 0)) [0];
   AB.mkRetirementRequest (TA.RetireNode (1, 0)) [1; 2]].
Definition audit_nested_retired := audit_authority_result_state
  (AB.retire_batch audit_nested audit_nested_requests).

Example audit_canonical_fixture_issuers_admitted :
  TA.allocate_owned audit_live 0 (TD.ORoot 0) 0 = TA.Accepted audit_flat0 /\
  TA.allocate_owned audit_flat0 1 (TD.ORoot 0) 0 = TA.Accepted audit_flat1 /\
  TA.allocate_owned audit_flat1 2 (TD.OParent 1) 0 = TA.Accepted audit_nested /\
  (forall kind, TA.begin_lease audit_nested (2, 0) kind 0 =
    TA.Accepted (audit_late_lease kind)).
Proof. repeat split; reflexivity. Qed.

Lemma audit_canonical_fixture_reachable :
  TA.AuthorityRun (TA.empty_authority 0) audit_nested.
Proof.
  eapply TA.ARExecute with (op := TA.CreateRoot 0) (b := audit_live);
    [reflexivity|].
  eapply TA.ARExecute with (op := TA.AllocateOwned 0 (TD.ORoot 0) 0)
    (b := audit_flat0); [reflexivity|].
  eapply TA.ARExecute with (op := TA.AllocateOwned 1 (TD.ORoot 0) 0)
    (b := audit_flat1); [reflexivity|].
  eapply TA.ARExecute with (op := TA.AllocateOwned 2 (TD.OParent 1) 0)
    (b := audit_nested); [reflexivity|constructor].
Qed.

Lemma audit_canonical_fixture_invariant : TA.AuthorityInvariant audit_nested.
Proof.
  eapply TA.admitted_runs_preserve_authority;
    [apply TA.empty_authority_invariant|apply audit_canonical_fixture_reachable].
Qed.

Example audit_canonical_batch_late_lease_atomic : forall kind,
  AB.retire_batch (audit_late_lease kind) audit_nested_requests =
    TA.Refused TA.ActiveLease (audit_late_lease kind).
Proof. intros []; reflexivity. Qed.

Example audit_canonical_node_batch_keeps_root_right :
  AB.retire_batch audit_nested audit_nested_requests = TA.Accepted audit_nested_retired /\
  TD.s_node (TD.heap (TA.forest audit_nested_retired) 0) = None /\
  TD.s_node (TD.heap (TA.forest audit_nested_retired) 1) = None /\
  TD.s_node (TD.heap (TA.forest audit_nested_retired) 2) = None /\
  TA.cleanup_rights audit_nested_retired 0 = Some (0, 0) /\
  TA.holds_cleanup audit_nested_retired (0, 0) = true.
Proof. repeat split; reflexivity. Qed.

Definition audit_root_after_nodes := audit_authority_result_state
  (AB.retire_batch audit_nested_retired
    [AB.mkRetirementRequest (TA.RetireRoot (0, 0)) []]).

Example audit_canonical_root_end_consumes_once :
  AB.retire_batch audit_nested_retired
    [AB.mkRetirementRequest (TA.RetireRoot (0, 0)) []] =
      TA.Accepted audit_root_after_nodes /\
  TA.cleanup_rights audit_root_after_nodes 0 = None /\
  TA.holds_cleanup audit_root_after_nodes (0, 0) = false /\
  AB.retire_batch audit_root_after_nodes
    [AB.mkRetirementRequest (TA.RetireRoot (0, 0)) []] =
      TA.Refused TA.StaleIdentity audit_root_after_nodes.
Proof. repeat split; reflexivity. Qed.

Example audit_canonical_invalid_batch_unchanged :
  AB.retire_batch audit_nested
    [AB.mkRetirementRequest (TA.RetireNode (0, 0)) [0];
     AB.mkRetirementRequest (TA.RetireNode (0, 0)) [0]] =
      TA.Refused TA.InvalidUnit audit_nested /\
  AB.retire_batch audit_nested
    [AB.mkRetirementRequest (TA.RetireNode (0, 0)) [0];
     AB.mkRetirementRequest (TA.RetireNode (1, 0)) [1]] =
      TA.Refused TA.InvalidUnit audit_nested /\
  AB.retire_batch audit_nested
    [AB.mkRetirementRequest (TA.RetireNode (0, 0)) [0];
     AB.mkRetirementRequest (TA.RetireNode (1, 0)) [1; 2; 2]] =
      TA.Refused TA.InvalidUnit audit_nested /\
  AB.retire_batch audit_nested
    [AB.mkRetirementRequest (TA.RetireNode (0, 0)) [0];
     AB.mkRetirementRequest (TA.RetireNode (99, 0)) [99]] =
      TA.Refused TA.StaleIdentity audit_nested.
Proof. repeat split; reflexivity. Qed.

Definition audit_root7 := audit_authority_result_state (TA.create_root audit_live 7).
Definition audit_foreign_node0 := audit_authority_result_state
  (TA.allocate_owned audit_root7 0 (TD.ORoot 0) 0).
Definition audit_foreign_node1 := audit_authority_result_state
  (TA.allocate_owned audit_foreign_node0 1 (TD.ORoot 7) 0).
Definition audit_foreign := audit_authority_result_state
  (TA.transfer_cleanup audit_foreign_node1 (7, 0) 1).

Example audit_canonical_foreign_fixture_admitted :
  TA.create_root audit_live 7 = TA.Accepted audit_root7 /\
  TA.allocate_owned audit_root7 0 (TD.ORoot 0) 0 = TA.Accepted audit_foreign_node0 /\
  TA.allocate_owned audit_foreign_node0 1 (TD.ORoot 7) 0 = TA.Accepted audit_foreign_node1 /\
  TA.transfer_cleanup audit_foreign_node1 (7, 0) 1 = TA.Accepted audit_foreign.
Proof. repeat split; reflexivity. Qed.

Example audit_canonical_batch_late_foreign_holder_atomic :
  AB.retire_batch audit_foreign
    [AB.mkRetirementRequest (TA.RetireNode (0, 0)) [0];
     AB.mkRetirementRequest (TA.RetireNode (1, 0)) [1]] =
      TA.Refused TA.MissingCleanupRight audit_foreign /\
  AB.retire_batch audit_foreign
    [AB.mkRetirementRequest (TA.RetireRoot (0, 0)) [0];
     AB.mkRetirementRequest (TA.RetireRoot (7, 0)) [1]] =
      TA.Refused TA.MissingCleanupRight audit_foreign.
Proof. split; reflexivity. Qed.

Example audit_canonical_root_node_overlap_unchanged :
  AB.retire_batch audit_nested
    [AB.mkRetirementRequest (TA.RetireNode (0, 0)) [0];
     AB.mkRetirementRequest (TA.RetireRoot (0, 0)) [0; 1; 2]] =
      TA.Refused TA.InvalidUnit audit_nested /\
  AB.retire_batch audit_nested
    [AB.mkRetirementRequest (TA.RetireRoot (0, 0)) [0; 1; 2];
     AB.mkRetirementRequest (TA.RetireNode (0, 0)) [0]] =
      TA.Refused TA.InvalidUnit audit_nested /\
  AB.retire_batch audit_consumed [AB.mkRetirementRequest (TA.RetireRoot (0, 0)) []] =
      TA.Refused TA.StaleIdentity audit_consumed /\
  AB.retire_batch audit_reissued [AB.mkRetirementRequest (TA.RetireRoot (0, 0)) []] =
      TA.Refused TA.StaleIdentity audit_reissued /\
  AB.retire_batch audit_nested [] = TA.Accepted audit_nested.
Proof. repeat split; reflexivity. Qed.

(* Independent red-team discovered this missing INITIAL-state check. A child
   delete must not make an initially incomplete parent/root unit admissible. *)
Example audit_canonical_shrinking_units_refused :
  TA.valid_unit (TA.forest audit_nested) (TA.RetireNode (1, 0)) [1] = false /\
  AB.retire_batch audit_nested
    [AB.mkRetirementRequest (TA.RetireNode (2, 0)) [2];
     AB.mkRetirementRequest (TA.RetireNode (1, 0)) [1]] =
      TA.Refused TA.InvalidUnit audit_nested /\
  TA.valid_unit (TA.forest audit_nested) (TA.RetireRoot (0, 0)) [] = false /\
  AB.retire_batch audit_nested
    [AB.mkRetirementRequest (TA.RetireNode (0, 0)) [0];
     AB.mkRetirementRequest (TA.RetireNode (2, 0)) [2];
     AB.mkRetirementRequest (TA.RetireNode (1, 0)) [1];
     AB.mkRetirementRequest (TA.RetireRoot (0, 0)) []] =
      TA.Refused TA.InvalidUnit audit_nested /\
  AB.retire_batch audit_live
    [AB.mkRetirementRequest (TA.RetireRoot (0, 0)) [];
     AB.mkRetirementRequest (TA.RetireRoot (0, 0)) []] =
      TA.Refused TA.InvalidUnit audit_live.
Proof. repeat split; reflexivity. Qed.
