(* Independent typed falsifiers for the forest and its authority boundary.
   Raw forest permission counterexamples remain visible, not admitted APIs. *)
Require Import OwnershipTeardown.
Require Import OwnershipTeardownAuthority.
Require Import Stdlib.Lists.List.
Import ListNotations.

Theorem duplicate_release_refused : forall s n g Ul s',
  ~ NoDup Ul -> ~ Step s (OpRelease n g Ul) s'.
Proof. intros s n g Ul s' Hnd HS. apply Hnd. exact (release_unit_unique _ _ _ _ _ HS). Qed.

Theorem duplicate_root_drop_refused : forall s r g Ul s',
  ~ NoDup Ul -> ~ Step s (OpRootDrop r g Ul) s'.
Proof. intros s r g Ul s' Hnd HS. apply Hnd. exact (root_drop_unit_unique _ _ _ _ _ HS). Qed.

Example repeated_node_retirement_refused : forall s', ~ Step st_ab (OpRelease 0 0 [0; 0]) s'.
Proof. intros s'. apply duplicate_release_refused. intro H.
  inversion H as [| x xs Hnot Hnd]; subst. apply Hnot; left; reflexivity. Qed.

Example repeated_root_retirement_refused : forall s', ~ Step run4 (OpRootDrop 0 0 [0; 0; 1]) s'.
Proof. intros s'. apply duplicate_root_drop_refused. intro H.
  inversion H as [| x xs Hnot Hnd]; subst. apply Hnot; left; reflexivity. Qed.

Example incomplete_unit_refused : forall s',
  ~ Step (mkSt h_c ix0 kids_c [0] 2 initial_root_generations) (OpRelease 0 0 [0]) s'.
Proof.
  intros s' HS. destruct (release_step_inv _ _ _ _ _ HS) as [_ [HU _]].
  change (forall y, In y [0] <-> in_sub h_c 0 y) in HU.
  destruct (proj2 (HU 1) c_in_sub_a) as [E | []]; discriminate.
Qed.

Example ownership_cycle_attach_refused : forall s',
  ~ Step (mkSt h_c ix0 kids_c [0] 2 initial_root_generations) (OpAttach 0 0 (OParent 1) 0) s'.
Proof.
  intros s' HS. inversion HS; subst.
  match goal with H : attach_ok _ _ _ _ _ _ |- _ =>
    exact (checked_attach_refuses_cycle [0] initial_root_generations 0 H)
  end.
Qed.

Example self_parent_attach_refused : forall s',
  ~ Step (mkSt h_c ix0 kids_c [0] 2 initial_root_generations) (OpAttach 0 0 (OParent 0) 0) s'.
Proof.
  intros s' HS. inversion HS; subst.
  match goal with H : attach_ok _ _ _ _ _ _ |- _ =>
    destruct H as [_ Hnot]; apply Hnot; constructor
  end.
Qed.

Example undeclared_root_alloc_refused : forall s',
  ~ Step (mkSt h_c ix0 kids_c [0] 2 initial_root_generations) (OpAlloc 2 (ORoot 7) 0) s'.
Proof.
  intros s' HS. inversion HS; subst.
  match goal with H : owner_ok_new _ _ _ _ _ |- _ =>
    destruct H as [[E | []] _]; discriminate E
  end.
Qed.

(* Exact-index admission is a real precondition, not an implicit repair or
   a guard in the pure teardown function. These malformed states cannot
   instantiate the invariant theorem. *)
Example missing_reverse_index_not_admitted : ~ IndexExact h_ab ix0.
Proof. exact (proj1 stale_index_dangles). Qed.

Definition wrong_reverse_index : Index := fun t => if Nat.eqb t 1 then [(1, 0)] else [].
Example misdirected_reverse_index_not_admitted : ~ IndexExact h_ab wrong_reverse_index.
Proof.
  intros H. destruct (proj1 (H 1 1 0) (or_introl eq_refl)) as [g E].
  discriminate E.
Qed.

Example missing_children_index_not_admitted : ~ KidsExact h_c (fun _ => []).
Proof.
  intros H. destruct (proj2 (H (OParent 0) 1)
    (ex_intro _ (mkNode (OParent 0) empty_fields) (conj eq_refl eq_refl))).
Qed.

Example misdirected_children_index_not_admitted :
  ~ KidsExact h_c (fun o => match o with ORoot 0 => [0; 1] | _ => [] end).
Proof.
  intros H. destruct (proj1 (H (ORoot 0) 1) (or_intror (or_introl eq_refl)))
    as [nd [Hnode Howner]].
  change (Some (mkNode (OParent 0) empty_fields) = Some nd) in Hnode.
  injection Hnode as E. subst nd. discriminate Howner.
Qed.

Theorem old_subject_refused : forall op s', Step run7 op s' -> subject op <> Some (0, 0).
Proof.
  intros op s' HS E.
  pose proof (step_handles_resolve run7 op s' (0, 0) HS (or_introl E)) as [Hg _].
  discriminate Hg.
Qed.

Theorem stale_subject_release_refused : forall s', ~ Step run7 (OpRelease 0 0 [0]) s'.
Proof.
  intros s' HS. pose proof (step_handles_resolve run7 _ s' (0, 0) HS (or_introl eq_refl)) as [Hg _].
  discriminate Hg.
Qed.

Theorem stale_parent_alloc_refused : forall s', ~ Step run7 (OpAlloc 2 (OParent 0) 0) s'.
Proof.
  intros s' HS. pose proof (step_handles_resolve run7 _ s' (0, 0) HS (or_intror eq_refl)) as [Hg _].
  discriminate Hg.
Qed.

Theorem stale_link_write_refused : forall s',
  ~ Step run7 (OpSetField 0 1 0 (Some (0, 0))) s'.
Proof.
  intros s' HS. inversion HS; subst.
  match goal with H : link_ok _ _ |- _ => destruct H as [Hg _]; discriminate Hg end.
Qed.

Example unrelated_field_index_row_unchanged : forall h ix x k ol t,
  (forall l, field_at h x k = Some l -> t <> fst l) ->
  (forall l, ol = Some l -> t <> fst l) -> index_set h ix x k ol t = ix t.
Proof. exact index_set_frame. Qed.

Example unrelated_children_row_unchanged : forall kd x old o q,
  q <> old -> q <> o -> kids_move kd x old o q = kd q.
Proof. exact kids_move_frame. Qed.

(* Raw Step is still the internal forest specification. The new admitted
   boundary refuses these same identifying references below. *)
Theorem link_target_release_has_no_owner_permission_premise :
  Inv st_ab /\ owner_at (heap st_ab) 1 = Some (ORoot 1) /\
  field_at (heap st_ab) 1 0 = Some (0, 0) /\
  exists Ul, Step st_ab (OpRelease 0 0 Ul) (teardown st_ab Ul).
Proof.
  split; [exact inv_st_ab |]. split; [reflexivity |]. split; [reflexivity |].
  destruct (release_always_succeeds st_ab 0 0 inv_st_ab) as [Ul [_ [HS _]]].
  - split; [reflexivity | eexists; reflexivity].
  - exists Ul; exact HS.
Qed.

(* The same raw forest gap for root drop; never export it as an authority API. *)
Theorem root_drop_has_no_holder_permission_premise :
  Inv st_ab /\ field_at (heap st_ab) 1 0 = Some (0, 0) /\
  exists Ul, Step st_ab (OpRootDrop 0 0 Ul) (root_drop st_ab 0 Ul).
Proof.
  split; [exact inv_st_ab |]. split; [reflexivity |].
  destruct (root_drop_always_succeeds st_ab 0 inv_st_ab (or_introl eq_refl))
    as [Ul [_ [HS _]]].
  exists Ul. exact HS.
Qed.

(* Same st_ab inputs as the two permission-gap probes. A copied link has no
   effect on the protected ledger or the trusted executing context. *)
Definition ab_rights (r : nat) : option (nat * nat) :=
  if Nat.eqb r 0 then Some (0,10) else if Nat.eqb r 1 then Some (0,20) else None.
Definition ab_reader := mkAuthorityState st_ab 20 ab_rights [] 0.
Example ab_reader_invariant : AuthorityInvariant ab_reader.
Proof.
  split; [exact inv_st_ab |]. split.
  - intros r. unfold ab_reader, ab_rights; cbn.
    destruct r as [|[|r]]; cbn; intuition discriminate.
  - split; [split; [constructor |intros e H; destruct H] |intros e H; destruct H].
Qed.
Example linked_foreign_node_refused : forall Ul,
  retire ab_reader (RetireNode (0,0)) Ul = Refused MissingCleanupRight ab_reader.
Proof. intros Ul; reflexivity. Qed.

Example same_link_input_owner_cleans_and_clears_incoming : exists after,
  retire (in_context ab_reader 10) (RetireNode (0,0)) [0] = Accepted after /\
  field_at (heap (forest after)) 1 0 = None /\
  s_node (heap (forest after) 0) = None /\
  live (heap (forest after)) 1.
Proof. eexists; repeat split; try reflexivity; eexists; reflexivity. Qed.
Example foreign_root_refused : forall Ul,
  retire ab_reader (RetireRoot (0,0)) Ul = Refused MissingCleanupRight ab_reader.
Proof. intros Ul; reflexivity. Qed.

Definition authority_state_after (fallback : AuthorityState) (result : AuthorityResult) :=
  match result with Accepted s => s | Refused _ _ => fallback end.
(* This projection exists only in positive fixture construction, never the
   admitted executor. Every fixture transition has a separate success proof. *)
Definition auth0 := empty_authority 10.
Definition auth1 := authority_state_after auth0 (create_root auth0 0).
Definition auth2 := authority_state_after auth1 (allocate_owned auth1 0 (ORoot 0) 0).
Definition auth3 := authority_state_after auth2 (allocate_owned auth2 1 (OParent 0) 0).
Example actual_issue_allocate_chain :
  create_root auth0 0 = Accepted auth1 /\
  allocate_owned auth1 0 (ORoot 0) 0 = Accepted auth2 /\
  allocate_owned auth2 1 (OParent 0) 0 = Accepted auth3.
Proof. repeat split; reflexivity. Qed.
Example issued_chain_invariant : AuthorityInvariant auth3.
Proof.
  eapply allocation_preserves_authority; [|exact (proj2 (proj2 actual_issue_allocate_chain))].
  eapply allocation_preserves_authority; [|exact (proj1 (proj2 actual_issue_allocate_chain))].
  eapply create_root_preserves_authority; [apply empty_authority_invariant |exact (proj1 actual_issue_allocate_chain)].
Qed.
Example own_subtree_retirement_executes : exists after,
  retire auth3 (RetireNode (0,0)) [0;1] = Accepted after /\
  s_node (heap (forest after) 0) = None /\ s_node (heap (forest after) 1) = None.
Proof. eexists; repeat split; reflexivity. Qed.
Example own_root_retirement_executes : exists after,
  retire auth3 (RetireRoot (0,0)) [0;1] = Accepted after /\ cleanup_rights after 0 = None.
Proof. eexists; split; reflexivity. Qed.
Example descendant_omitted_refused :
  retire auth3 (RetireNode (0,0)) [0] = Refused InvalidUnit auth3 /\
  retire auth3 (RetireRoot (0,0)) [0] = Refused InvalidUnit auth3.
Proof. split; reflexivity. Qed.
Example duplicate_or_foreign_unit_refused :
  retire auth3 (RetireRoot (0,0)) [0;1;1] = Refused InvalidUnit auth3 /\
  retire auth3 (RetireRoot (0,0)) [0;1;2] = Refused InvalidUnit auth3.
Proof. split; reflexivity. Qed.

Definition child_borrow := authority_state_after auth3 (begin_lease auth3 (1,0) BorrowLease 10).
Definition child_pin := authority_state_after auth3 (begin_lease auth3 (1,0) PinLease 10).
Example child_leases_really_issued :
  begin_lease auth3 (1,0) BorrowLease 10 = Accepted child_borrow /\
  begin_lease auth3 (1,0) PinLease 10 = Accepted child_pin.
Proof. split; reflexivity. Qed.
Example child_borrow_blocks_parent_and_root :
  retire child_borrow (RetireNode (0,0)) [0;1] = Refused ActiveLease child_borrow /\
  retire child_borrow (RetireRoot (0,0)) [0;1] = Refused ActiveLease child_borrow.
Proof. split; reflexivity. Qed.
Example child_pin_blocks_parent_and_root :
  retire child_pin (RetireNode (0,0)) [0;1] = Refused ActiveLease child_pin /\
  retire child_pin (RetireRoot (0,0)) [0;1] = Refused ActiveLease child_pin.
Proof. split; reflexivity. Qed.
Example foreign_context_cannot_cancel_child_loan :
  end_lease (in_context child_borrow 20) 0 = Refused WrongLeaseHolder (in_context child_borrow 20).
Proof. reflexivity. Qed.
Definition child_ended := authority_state_after child_borrow (end_lease child_borrow 0).
Example own_lease_end_then_retirement : end_lease child_borrow 0 = Accepted child_ended /\
  exists after, retire child_ended (RetireRoot (0,0)) [0;1] = Accepted after.
Proof. split; [reflexivity |eexists; reflexivity]. Qed.
Definition child_borrow2 := authority_state_after child_ended (begin_lease child_ended (1,0) BorrowLease 10).
Example old_lease_cannot_cancel_new_one :
  begin_lease child_ended (1,0) BorrowLease 10 = Accepted child_borrow2 /\
  end_lease child_borrow2 0 = Refused MissingLease child_borrow2 /\
  retire child_borrow2 (RetireRoot (0,0)) [0;1] = Refused ActiveLease child_borrow2.
Proof. repeat split; reflexivity. Qed.

Definition moved_right := authority_state_after auth3 (transfer_cleanup auth3 (0,0) 20).
Example transferred_right_revokes_old_holder : transfer_cleanup auth3 (0,0) 20 = Accepted moved_right /\
  retire moved_right (RetireRoot (0,0)) [0;1] = Refused MissingCleanupRight moved_right /\
  exists after, retire (in_context moved_right 20) (RetireRoot (0,0)) [0;1] = Accepted after.
Proof. split; [reflexivity |]. split; [reflexivity |]. eexists; reflexivity. Qed.
Example foreign_holder_cannot_transfer :
  transfer_cleanup ab_reader (0,0) 20 = Refused MissingCleanupRight ab_reader.
Proof. reflexivity. Qed.
Example copied_reference_cannot_mint_foreign_pin :
  begin_lease (in_context auth3 20) (1,0) PinLease 20 =
    Refused MissingCleanupRight (in_context auth3 20) /\
  begin_lease moved_right (1,0) PinLease 10 = Refused MissingCleanupRight moved_right.
Proof. split; reflexivity. Qed.
Definition delegated_loan := authority_state_after auth3 (begin_lease auth3 (1,0) BorrowLease 20).
Example owner_issues_loan_reader_cannot_free_but_can_return :
  begin_lease auth3 (1,0) BorrowLease 20 = Accepted delegated_loan /\
  retire (in_context delegated_loan 20) (RetireRoot (0,0)) [0;1] =
    Refused MissingCleanupRight (in_context delegated_loan 20) /\
  end_lease delegated_loan 0 = Refused WrongLeaseHolder delegated_loan /\
  exists after, end_lease (in_context delegated_loan 20) 0 = Accepted after.
Proof. repeat split; try reflexivity; eexists; reflexivity. Qed.
Definition consumed := authority_state_after auth3 (retire auth3 (RetireRoot (0,0)) [0;1]).
Definition reopened := authority_state_after consumed (create_root consumed 0).
Definition reused := authority_state_after reopened (allocate_owned reopened 0 (ORoot 0) 1).
Example reuse_does_not_restore_consumed_identities :
  retire auth3 (RetireRoot (0,0)) [0;1] = Accepted consumed /\
  create_root consumed 0 = Accepted reopened /\
  allocate_owned reopened 0 (ORoot 0) 1 = Accepted reused /\
  retire reused (RetireRoot (0,0)) [0] = Refused StaleIdentity reused /\
  retire reused (RetireNode (0,0)) [0] = Refused StaleIdentity reused /\
  exists after, retire reused (RetireRoot (0,1)) [0] = Accepted after.
Proof. repeat split; try reflexivity; eexists; reflexivity. Qed.

Definition saved_local : Link := (0, 0).
Theorem stored_clear_does_not_refresh_a_saved_local :
  field_at (heap st_ab) 1 0 = Some saved_local /\
  field_at (heap (teardown st_ab [0])) 1 0 = None /\
  ~ resolves (heap (teardown st_ab [0])) saved_local.
Proof. split; [reflexivity |]. split; [reflexivity |]. intros [_ [nd H]]. discriminate H. Qed.

Theorem saved_local_checked_read_stays_refused : forall s',
  Steps (teardown st_ab [0]) s' -> resolve_node (heap s') saved_local = None.
Proof.
  intros s' HS. apply resolve_node_none.
  apply (temp_link_dead_forever (heap st_ab) (rix st_ab) [0] 0 0 (teardown st_ab [0]) s').
  - split; [reflexivity | eexists; reflexivity].
  - left; reflexivity.
  - reflexivity.
  - exact HS.
Qed.

Example reused_node_checked_read : resolve_node (heap run7) saved_local = None /\
  exists nd, resolve_node (heap run7) (0, 1) = Some nd.
Proof. split; [reflexivity | eexists; reflexivity]. Qed.

(* A saved local link to child B (slot 1), not the dropped root's own node,
   reads None after root 0 drops, in every later run. *)
Theorem saved_child_local_refused_after_root_drop : forall s',
  Steps run5 s' -> resolve_node (heap s') (1, 0) = None.
Proof.
  intros s' HS.
  apply (dropped_root_unit_local_read_none_forever run4 0 0 [0; 1] run5 s' 1 0).
  - apply StRootDrop; [split; [left; reflexivity | reflexivity] | exact run4_unit |].
    repeat constructor; simpl; intuition discriminate.
  - split; [reflexivity | eexists; reflexivity].
  - right; left; reflexivity.
  - exact HS.
Qed.

(* Root reuse now distinguishes old/current epochs in the actual Step;
   authority issuance and cross-arena identity remain separate obligations. *)
Definition reopened_root : St :=
  mkSt (heap run5) (rix run5) (kids run5) (0 :: roots run5) (bound run5) (root_gen run5).
Definition new_root_node : St :=
  mkSt (upd (heap reopened_root) 0
        (mkSlot (s_gen (heap reopened_root 0)) (Some (mkNode (ORoot 0) empty_fields))))
       (rix reopened_root) (kids_add (kids reopened_root) (ORoot 0) 0)
       (roots reopened_root) (Nat.max (bound reopened_root) 1) (root_gen reopened_root).

Lemma new_root_node_unit : forall y, In y [0] <-> under_root (heap new_root_node) 0 y.
Proof.
  intros y. split.
  - intros [E | []]. subst y. eapply URHere; reflexivity.
  - intros H. destruct (under_root_live _ _ _ H) as [nd Hnd].
    destruct y as [| [| y]]; [left; reflexivity | discriminate Hnd | discriminate Hnd].
Qed.

Theorem root_epoch_reuse_checked :
  Step run5 (OpRootNew 0) reopened_root /\
  Step reopened_root (OpAlloc 0 (ORoot 0) 1) new_root_node /\
  Inv new_root_node /\ resolves (heap new_root_node) (0, 1) /\
  check_root new_root_node (0, 0) = false /\ check_root new_root_node (0, 1) = true /\
  Step new_root_node (OpRootDrop 0 1 [0]) (root_drop new_root_node 0 [0]).
Proof.
  assert (HN : Step run5 (OpRootNew 0) reopened_root).
  { apply StRootNew. change (~ In 0 []). intros []. }
  assert (HA : Step reopened_root (OpAlloc 0 (ORoot 0) 1) new_root_node).
  { apply StAlloc; [reflexivity | split; [left; reflexivity | reflexivity]]. }
  pose proof full_run_cycle_released as [_ [I5 _]].
  split; [exact HN |]. split; [exact HA |].
  split; [exact (inv_step _ _ _ (inv_step _ _ _ I5 HN) HA) |].
  split; [split; [reflexivity | eexists; reflexivity] |].
  split; [reflexivity |]. split; [reflexivity |].
  apply StRootDrop; [split; [left; reflexivity | reflexivity] | exact new_root_node_unit |].
  constructor; [simpl; tauto | constructor].
Qed.

Theorem old_root_operations_refused : forall op s',
  root_handle op = Some (0, 0) -> ~ Step new_root_node op s'.
Proof.
  intros op s' E HS.
  pose proof (step_root_handles_resolve _ _ _ _ HS E) as [_ Hg]. discriminate Hg.
Qed.

Example old_root_alloc_refused : forall s', ~ Step new_root_node (OpAlloc 2 (ORoot 0) 0) s'.
Proof. intros s'. apply old_root_operations_refused; reflexivity. Qed.
Example old_root_attach_refused : forall s', ~ Step new_root_node (OpAttach 0 1 (ORoot 0) 0) s'.
Proof. intros s'. apply old_root_operations_refused; reflexivity. Qed.
Example old_root_drop_refused : forall s', ~ Step new_root_node (OpRootDrop 0 0 [0]) s'.
Proof. intros s'. apply old_root_operations_refused; reflexivity. Qed.

Theorem old_root_checked_read_refused_after_any_run : forall s',
  Steps run5 s' -> check_root s' (0, 0) = false.
Proof.
  intros s' HS. apply (dropped_root_check_false_forever run4 0 0 [0;1] run5 s').
  - apply StRootDrop; [split; [left; reflexivity | reflexivity] | exact run4_unit |].
    repeat constructor; simpl; intuition discriminate.
  - exact HS.
Qed.

Example future_root_epoch_refused : check_root new_root_node (0, 2) = false.
Proof. reflexivity. Qed.
Example dead_current_root_refused : check_root run5 (0, 1) = false.
Proof. reflexivity. Qed.
Example node_and_root_epochs_are_independent : check_root run7 (9, 0) = true /\
  resolve_node (heap run7) (0, 0) = None /\ exists nd, resolve_node (heap run7) (0, 1) = Some nd.
Proof. split; [reflexivity |]. split; [reflexivity | eexists; reflexivity]. Qed.

Theorem root_drop_other_identity_frame : forall s r Ul q g, q <> r ->
  root_resolves (roots (root_drop s r Ul)) (root_gen (root_drop s r Ul)) (q, g) <->
  root_resolves (roots s) (root_gen s) (q, g).
Proof.
  intros s r Ul q g Hne. unfold root_resolves; cbn [fst snd].
  rewrite root_drop_epoch_frame by exact Hne. rewrite roots_root_drop.
  split.
  - intros [Hin Hg]. apply in_remove in Hin. tauto.
  - intros [Hin Hg]. split; [apply in_in_remove; assumption | exact Hg].
Qed.
