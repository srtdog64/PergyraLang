(* Functional transaction over the canonical authority machine. Intermediate
   states are private mathematical values, not a native undo log. Native frees
   require stable whole-batch admission and a non-refusing commit. Requests do
   not issue rights, leases, store bindings, or mutable-access authority. *)
Require Import OwnershipTeardown OwnershipTeardownAuthority.
Require Import Stdlib.Lists.List Stdlib.Bool.Bool Stdlib.Arith.PeanoNat.
Import ListNotations.

Record RetirementRequest := mkRetirementRequest {
  batch_target : RetirementTarget;
  batch_unit : list nat
}.

Definition retirement_target_eq_dec : forall x y : RetirementTarget, {x = y} + {x <> y}.
Proof. decide equality; decide equality; apply Nat.eq_dec. Defined.

Definition request_independent (req : RetirementRequest) (rest : list RetirementRequest) : bool :=
  forallb (fun other =>
    (if retirement_target_eq_dec (batch_target req) (batch_target other) then false else true) &&
    forallb (fun x => negb (inU (batch_unit other) x)) (batch_unit req)) rest.

(* Every canonical check is made against the SAME original authority state.
   Conflicts include empty-root target repetition, not just intersecting nodes. *)
Fixpoint initial_batch_check (a : AuthorityState) (requests : list RetirementRequest)
    : option AuthorityFailure :=
  match requests with
  | [] => None
  | req :: rest =>
      match retire a (batch_target req) (batch_unit req) with
      | Refused why _ => Some why
      | Accepted _ => if request_independent req rest then initial_batch_check a rest
                      else Some InvalidUnit
      end
  end.

Fixpoint InitialBatchAdmitted (a : AuthorityState) (requests : list RetirementRequest) : Prop :=
  match requests with
  | [] => True
  | req :: rest =>
      (exists after, retire a (batch_target req) (batch_unit req) = Accepted after) /\
      request_independent req rest = true /\ InitialBatchAdmitted a rest
  end.

Fixpoint DisjointRetirementUnits (requests : list RetirementRequest) : Prop :=
  match requests with
  | [] => True
  | req :: rest =>
      (forall other, In other rest -> forall x, In x (batch_unit req) -> ~ In x (batch_unit other)) /\
      DisjointRetirementUnits rest
  end.

(* Private functional commit staging. It cannot admit a public batch without
   the original-state preflight. Native non-refusal after preflight is OPEN. *)
Local Fixpoint retire_batch_stage (a : AuthorityState)
    (requests : list RetirementRequest) : AuthorityResult :=
  match requests with
  | [] => Accepted a
  | req :: rest =>
      match retire a (batch_target req) (batch_unit req) with
      | Refused why _ => Refused why a
      | Accepted middle =>
          match retire_batch_stage middle rest with
          | Accepted after => Accepted after
          | Refused why _ => Refused why a
          end
      end
  end.

Definition retire_batch (a : AuthorityState) (requests : list RetirementRequest) : AuthorityResult :=
  match initial_batch_check a requests with
  | Some why => Refused why a
  | None => match retire_batch_stage a requests with
            | Accepted after => Accepted after
            | Refused why _ => Refused why a
            end
  end.

(* The relation records EVERY supplied request, including empty units. Exact
   units and current identities are checked by canonical retire at each commit
   step, IN ADDITION to original-state admission. No footprint repair occurs. *)
Inductive AdmittedRetirementBatch :
    AuthorityState -> list RetirementRequest -> AuthorityState -> Prop :=
| ARBEmpty : forall a, AdmittedRetirementBatch a [] a
| ARBCons : forall a req rest middle after,
    retire a (batch_target req) (batch_unit req) = Accepted middle ->
    AdmittedRetirementBatch middle rest after ->
    AdmittedRetirementBatch a (req :: rest) after.

Definition NodeOnlyBatch (requests : list RetirementRequest) : Prop :=
  Forall (fun req => exists l, batch_target req = RetireNode l) requests.

Theorem retire_batch_refusal_unchanged : forall requests a why after,
  retire_batch a requests = Refused why after -> after = a.
Proof.
  intros requests a why after H. unfold retire_batch in H.
  destruct (initial_batch_check a requests); [inversion H; reflexivity|].
  destruct (retire_batch_stage a requests); [discriminate|inversion H; reflexivity].
Qed.

(* Transparent executable witnesses: each state is obtained from a successful
   canonical operation, never by a failed-operation fallback or forged ledger. *)
Definition atomic_batch_root_witness :
  {a | create_root (empty_authority 0) 0 = Accepted a}.
Proof. eexists; reflexivity. Defined.
Definition atomic_batch_root := proj1_sig atomic_batch_root_witness.

Definition atomic_batch_node0_witness :
  {a | allocate_owned atomic_batch_root 0 (ORoot 0) 0 = Accepted a}.
Proof. eexists; reflexivity. Defined.
Definition atomic_batch_node0 := proj1_sig atomic_batch_node0_witness.

Definition atomic_batch_node1_witness :
  {a | allocate_owned atomic_batch_node0 1 (ORoot 0) 0 = Accepted a}.
Proof. eexists; reflexivity. Defined.
Definition atomic_batch_node1 := proj1_sig atomic_batch_node1_witness.

Definition atomic_batch_tree_witness :
  {a | allocate_owned atomic_batch_node1 2 (OParent 1) 0 = Accepted a}.
Proof. eexists; reflexivity. Defined.
Definition atomic_batch_tree := proj1_sig atomic_batch_tree_witness.

Definition atomic_batch_requests :=
  [mkRetirementRequest (RetireNode (0,0)) [0];
   mkRetirementRequest (RetireNode (1,0)) [1;2]].

Definition atomic_batch_success_witness :
  {a | retire_batch atomic_batch_tree atomic_batch_requests = Accepted a}.
Proof. eexists; reflexivity. Defined.
Definition atomic_batch_after_nodes := proj1_sig atomic_batch_success_witness.

Example atomic_batch_success_preserves_root_right :
  cleanup_rights atomic_batch_after_nodes 0 = Some (0,0).
Proof. reflexivity. Qed.

Definition atomic_batch_root_end_witness :
  {a | retire_batch atomic_batch_after_nodes
    [mkRetirementRequest (RetireRoot (0,0)) []] = Accepted a}.
Proof. eexists; reflexivity. Defined.
Definition atomic_batch_after_root := proj1_sig atomic_batch_root_end_witness.

Example atomic_batch_root_end_consumes_right :
  cleanup_rights atomic_batch_after_root 0 = None.
Proof. reflexivity. Qed.

Example atomic_batch_consumed_root_refused :
  retire_batch atomic_batch_after_root
    [mkRetirementRequest (RetireRoot (0,0)) []] =
  Refused StaleIdentity atomic_batch_after_root.
Proof. reflexivity. Qed.

Example atomic_batch_zero_filled_missing_node_refused :
  retire_batch atomic_batch_root [mkRetirementRequest (RetireNode (0,0)) [0]] =
  Refused StaleIdentity atomic_batch_root.
Proof. reflexivity. Qed.

Example atomic_batch_duplicate_unit_refused :
  retire_batch atomic_batch_tree
    [mkRetirementRequest (RetireNode (0,0)) [0];
     mkRetirementRequest (RetireNode (1,0)) [1;2;2]] =
  Refused InvalidUnit atomic_batch_tree.
Proof. reflexivity. Qed.

Example atomic_batch_repeated_node_refused :
  retire_batch atomic_batch_tree
    [mkRetirementRequest (RetireNode (0,0)) [0];
     mkRetirementRequest (RetireNode (0,0)) [0]] =
  Refused InvalidUnit atomic_batch_tree.
Proof. reflexivity. Qed.

Example atomic_batch_later_wrong_generation_refused :
  retire_batch atomic_batch_tree
    [mkRetirementRequest (RetireNode (0,0)) [0];
     mkRetirementRequest (RetireNode (1,1)) [1;2]] =
  Refused StaleIdentity atomic_batch_tree.
Proof. reflexivity. Qed.

Example atomic_batch_repeated_empty_root_refused :
  retire_batch atomic_batch_root
    [mkRetirementRequest (RetireRoot (0,0)) [];
     mkRetirementRequest (RetireRoot (0,0)) []] =
  Refused InvalidUnit atomic_batch_root.
Proof. reflexivity. Qed.

Example atomic_batch_overlapping_units_refused :
  retire_batch atomic_batch_tree
    [mkRetirementRequest (RetireNode (2,0)) [2];
     mkRetirementRequest (RetireNode (1,0)) [1;2]] =
  Refused InvalidUnit atomic_batch_tree.
Proof. reflexivity. Qed.

(* Regression: a later unit must not become valid by shrinking its original
   subtree, or by removing all nodes before an initially nonempty root drop. *)
Example atomic_batch_shrinking_parent_refused :
  retire_batch atomic_batch_tree
    [mkRetirementRequest (RetireNode (2,0)) [2];
     mkRetirementRequest (RetireNode (1,0)) [1]] =
  Refused InvalidUnit atomic_batch_tree.
Proof. reflexivity. Qed.

Example atomic_batch_shrinking_root_refused :
  retire_batch atomic_batch_tree
    [mkRetirementRequest (RetireNode (0,0)) [0];
     mkRetirementRequest (RetireNode (2,0)) [2];
     mkRetirementRequest (RetireNode (1,0)) [1];
     mkRetirementRequest (RetireRoot (0,0)) []] =
  Refused InvalidUnit atomic_batch_tree.
Proof. reflexivity. Qed.

Definition atomic_batch_lease_witness (kind : LeaseKind) :
  {a | begin_lease atomic_batch_tree (2,0) kind 7 = Accepted a}.
Proof. eexists; destruct kind; reflexivity. Defined.
Definition atomic_batch_leased kind := proj1_sig (atomic_batch_lease_witness kind).

Example atomic_batch_later_nested_borrow_refused :
  retire_batch (atomic_batch_leased BorrowLease) atomic_batch_requests =
  Refused ActiveLease (atomic_batch_leased BorrowLease).
Proof. reflexivity. Qed.

Example atomic_batch_later_nested_pin_refused :
  retire_batch (atomic_batch_leased PinLease) atomic_batch_requests =
  Refused ActiveLease (atomic_batch_leased PinLease).
Proof. reflexivity. Qed.

Definition atomic_batch_second_root_witness :
  {a | create_root atomic_batch_tree 7 = Accepted a}.
Proof. eexists; reflexivity. Defined.
Definition atomic_batch_second_root := proj1_sig atomic_batch_second_root_witness.
Definition atomic_batch_foreign_root_witness :
  {a | transfer_cleanup atomic_batch_second_root (7,0) 1 = Accepted a}.
Proof. eexists; reflexivity. Defined.
Definition atomic_batch_foreign_root := proj1_sig atomic_batch_foreign_root_witness.

Example atomic_batch_later_foreign_holder_refused :
  retire_batch atomic_batch_foreign_root
    [mkRetirementRequest (RetireNode (0,0)) [0];
     mkRetirementRequest (RetireRoot (7,0)) []] =
  Refused MissingCleanupRight atomic_batch_foreign_root.
Proof. reflexivity. Qed.

Local Theorem retire_batch_stage_accepted_iff : forall requests a after,
  retire_batch_stage a requests = Accepted after <->
  AdmittedRetirementBatch a requests after.
Proof.
  induction requests as [|req rest IH]; intros a after; cbn [retire_batch_stage].
  - split; intro H; [inversion H; constructor|inversion H; reflexivity].
  - split; intro H.
    + destruct (retire a (batch_target req) (batch_unit req)) as [middle|why bad]
        eqn:E; [|discriminate].
      destruct (retire_batch_stage middle rest) as [final|why bad] eqn:R;
        [|discriminate].
      inversion H; subst. econstructor; [exact E|apply IH; exact R].
    + inversion H as [|a' req' rest' middle final HE HR]; subst.
      rewrite HE. apply IH in HR. rewrite HR. reflexivity.
Qed.

Lemma initial_batch_check_admitted : forall requests a,
  initial_batch_check a requests = None <-> InitialBatchAdmitted a requests.
Proof.
  induction requests as [|req rest IH]; intro a;
    cbn [initial_batch_check InitialBatchAdmitted].
  - split; [intro; exact I|intro; reflexivity].
  - destruct (retire a (batch_target req) (batch_unit req)) as [middle|why bad] eqn:HR.
    + destruct (request_independent req rest) eqn:HD.
      * rewrite IH. split.
        -- intro HA. split; [exists middle; reflexivity|split; [reflexivity|exact HA]].
        -- intros [_ [_ HA]]. exact HA.
      * split; [discriminate|intros [_ [HF _]]; discriminate].
    + split; [discriminate|intros [[middle HF] _]; discriminate].
Qed.

Theorem retire_batch_accepted_iff : forall requests a after,
  retire_batch a requests = Accepted after <->
  InitialBatchAdmitted a requests /\ AdmittedRetirementBatch a requests after.
Proof.
  intros requests a after. unfold retire_batch. split.
  - destruct (initial_batch_check a requests) eqn:HC; [discriminate|].
    destruct (retire_batch_stage a requests) eqn:HS; [|discriminate].
    intro H. injection H as H. subst. split.
    + apply initial_batch_check_admitted; exact HC.
    + apply retire_batch_stage_accepted_iff; exact HS.
  - intros [HI HS]. apply initial_batch_check_admitted in HI.
    apply retire_batch_stage_accepted_iff in HS. rewrite HI, HS. reflexivity.
Qed.

Lemma request_independent_properties : forall req rest,
  request_independent req rest = true ->
  ~ In (batch_target req) (map batch_target rest) /\
  (forall other, In other rest -> forall x, In x (batch_unit req) -> ~ In x (batch_unit other)).
Proof.
  intros req rest H. unfold request_independent in H. rewrite forallb_forall in H. split.
  - intro HI. apply in_map_iff in HI. destruct HI as [other [HE HI]].
    pose proof (H other HI) as HP. apply andb_true_iff in HP. destruct HP as [HT _].
    destruct (retirement_target_eq_dec (batch_target req) (batch_target other));
      [discriminate|congruence].
  - intros other HI x HX. pose proof (H other HI) as HP.
    apply andb_true_iff in HP. destruct HP as [_ HU].
    apply forallb_forall with (x := x) in HU; [|exact HX].
    apply negb_true_iff in HU. apply inU_false; exact HU.
Qed.

Lemma initial_batch_admitted_properties : forall requests a,
  InitialBatchAdmitted a requests ->
  NoDup (map batch_target requests) /\ DisjointRetirementUnits requests /\
  Forall (fun req => target_current (forest a) (batch_target req) = true /\
    target_authorized a (batch_target req) = true /\
    valid_unit (forest a) (batch_target req) (batch_unit req) = true /\
    unit_quiet a (batch_unit req) = true) requests.
Proof.
  induction requests as [|req rest IH]; intros a HA.
  - split; [constructor|split; [exact I|constructor]].
  - destruct HA as [[middle HR] [HD HA]].
    destruct (request_independent_properties _ _ HD) as [HT HU].
    destruct (IH a HA) as [HN [HD' HC]]. cbn [map DisjointRetirementUnits].
    split; [constructor; assumption|split; [split; assumption|]].
    constructor; [eapply accepted_retirement_checks_whole_unit; exact HR|exact HC].
Qed.

Theorem retire_batch_initial_requests_checked : forall requests a after,
  retire_batch a requests = Accepted after ->
  Forall (fun req => target_current (forest a) (batch_target req) = true /\
    target_authorized a (batch_target req) = true /\
    valid_unit (forest a) (batch_target req) (batch_unit req) = true /\
    unit_quiet a (batch_unit req) = true) requests.
Proof.
  intros requests a after H. apply retire_batch_accepted_iff in H.
  exact (proj2 (proj2 (initial_batch_admitted_properties _ _ (proj1 H)))).
Qed.

Theorem retire_batch_initial_units_disjoint : forall requests a after,
  retire_batch a requests = Accepted after -> DisjointRetirementUnits requests.
Proof.
  intros requests a after H. apply retire_batch_accepted_iff in H.
  exact (proj1 (proj2 (initial_batch_admitted_properties _ _ (proj1 H)))).
Qed.

Theorem retire_batch_targets_unique : forall requests a after,
  retire_batch a requests = Accepted after -> NoDup (map batch_target requests).
Proof.
  intros requests a after H. apply retire_batch_accepted_iff in H.
  exact (proj1 (initial_batch_admitted_properties _ _ (proj1 H))).
Qed.

Theorem retire_batch_projects_to_authority_run : forall requests a after,
  retire_batch a requests = Accepted after -> AuthorityRun a after.
Proof.
  intros requests a after H. apply retire_batch_accepted_iff in H. destruct H as [_ H].
  induction H.
  - constructor.
  - eapply ARExecute with (op := Retire (batch_target req) (batch_unit req));
      [exact H|exact IHAdmittedRetirementBatch].
Qed.

Theorem retire_batch_preserves_authority : forall requests a after,
  AuthorityInvariant a -> retire_batch a requests = Accepted after ->
  AuthorityInvariant after.
Proof.
  intros requests a after HI H. eapply admitted_runs_preserve_authority;
    [exact HI|eapply retire_batch_projects_to_authority_run; exact H].
Qed.

Theorem retire_batch_refusal_preserves_authority : forall requests a why after,
  AuthorityInvariant a -> retire_batch a requests = Refused why after ->
  AuthorityInvariant after.
Proof.
  intros requests a why after HI H.
  apply retire_batch_refusal_unchanged in H. subst; exact HI.
Qed.

Lemma accepted_node_retirement_rights_unchanged : forall a l Ul after,
  retire a (RetireNode l) Ul = Accepted after ->
  cleanup_rights after = cleanup_rights a.
Proof.
  intros a l Ul after H. unfold retire in H.
  repeat match type of H with
  | context [if ?b then _ else _] => destruct b eqn:?; try discriminate
  end. inversion H; reflexivity.
Qed.

Theorem retire_batch_node_rights_unchanged : forall requests a after,
  NodeOnlyBatch requests -> retire_batch a requests = Accepted after ->
  cleanup_rights after = cleanup_rights a.
Proof.
  intros requests a after HN H. apply retire_batch_accepted_iff in H. destruct H as [_ H].
  induction H; [reflexivity|]. inversion HN as [|? ? [l HL] HT]; subst.
  rewrite (IHAdmittedRetirementBatch HT).
  rewrite HL in H. eapply accepted_node_retirement_rights_unchanged; exact H.
Qed.

Lemma accepted_retirement_context_leases_unchanged : forall a t Ul after,
  retire a t Ul = Accepted after ->
  acting_context after = acting_context a /\
  active_leases after = active_leases a /\ next_lease after = next_lease a.
Proof.
  intros a t Ul after H. unfold retire in H.
  repeat match type of H with
  | context [if ?b then _ else _] => destruct b eqn:?; try discriminate
  end. inversion H; subst. repeat split; reflexivity.
Qed.

Theorem retire_batch_leases_unchanged : forall requests a after,
  retire_batch a requests = Accepted after ->
  acting_context after = acting_context a /\
  active_leases after = active_leases a /\ next_lease after = next_lease a.
Proof.
  intros requests a after H. apply retire_batch_accepted_iff in H. destruct H as [_ H].
  induction H; [repeat split; reflexivity|].
  destruct (accepted_retirement_context_leases_unchanged _ _ _ _ H)
    as [HC [HL HN]]. destruct IHAdmittedRetirementBatch as [HC' [HL' HN']].
  repeat split; congruence.
Qed.

(* A commit-stage failure retains its canonical reason and publishes only the
   original state. Preflight is mandatory, not a fallback to staged execution. *)
Theorem retire_batch_later_refusal : forall requests a why refused,
  InitialBatchAdmitted a requests ->
  retire_batch_stage a requests = Refused why refused ->
  retire_batch a requests = Refused why a.
Proof.
  intros requests a why refused HI HR. unfold retire_batch.
  apply initial_batch_check_admitted in HI. rewrite HI, HR. reflexivity.
Qed.

Theorem retire_batch_first_request_checks : forall a req rest after,
  retire_batch a (req :: rest) = Accepted after ->
  target_current (forest a) (batch_target req) = true /\
  target_authorized a (batch_target req) = true /\
  valid_unit (forest a) (batch_target req) (batch_unit req) = true /\
  unit_quiet a (batch_unit req) = true.
Proof.
  intros a req rest after H. apply retire_batch_initial_requests_checked in H.
  inversion H; assumption.
Qed.

Theorem retire_batch_initial_requests_exact : forall requests a after,
  Inv (forest a) -> retire_batch a requests = Accepted after ->
  Forall (fun req => UnitExact (forest a) (batch_target req) (batch_unit req)) requests.
Proof.
  intros requests a after HI H. apply retire_batch_initial_requests_checked in H.
  apply Forall_forall. intros req HR. apply Forall_forall with (x := req) in H; [|exact HR].
  destruct H as [HC [_ [HU _]]]. destruct (batch_target req) as [[n g]|[r g]].
  - eapply node_unit_sound; [exact HI|apply target_current_node; exact HC|exact HU].
  - eapply root_unit_sound; [exact HI|exact HU].
Qed.

Theorem retire_batch_first_request_exact : forall a req rest after,
  Inv (forest a) -> retire_batch a (req :: rest) = Accepted after ->
  UnitExact (forest a) (batch_target req) (batch_unit req).
Proof.
  intros a req rest after HI H.
  destruct (retire_batch_first_request_checks _ _ _ _ H) as [HC [_ [HU _]]].
  destruct (batch_target req) as [[n g]|[r g]].
  - eapply node_unit_sound; [exact HI|apply target_current_node; exact HC|exact HU].
  - eapply root_unit_sound; [exact HI|exact HU].
Qed.

Theorem retire_batch_projects_to_forest : forall requests a after,
  AuthorityInvariant a -> retire_batch a requests = Accepted after ->
  Steps (forest a) (forest after).
Proof.
  intros requests a after HI H. eapply admitted_runs_project_to_forest;
    [exact HI|eapply retire_batch_projects_to_authority_run; exact H].
Qed.

Lemma atomic_batch_root_invariant : AuthorityInvariant atomic_batch_root.
Proof.
  eapply create_root_preserves_authority;
    [apply empty_authority_invariant|exact (proj2_sig atomic_batch_root_witness)].
Qed.

Lemma atomic_batch_tree_invariant : AuthorityInvariant atomic_batch_tree.
Proof.
  eapply allocation_preserves_authority;
    [|exact (proj2_sig atomic_batch_tree_witness)].
  eapply allocation_preserves_authority;
    [|exact (proj2_sig atomic_batch_node1_witness)].
  eapply allocation_preserves_authority;
    [|exact (proj2_sig atomic_batch_node0_witness)].
  exact atomic_batch_root_invariant.
Qed.

Lemma atomic_batch_leased_invariant : forall kind,
  AuthorityInvariant (atomic_batch_leased kind).
Proof.
  intro kind. eapply begin_lease_preserves_authority;
    [exact atomic_batch_tree_invariant|exact (proj2_sig (atomic_batch_lease_witness kind))].
Qed.

Lemma atomic_batch_foreign_root_invariant : AuthorityInvariant atomic_batch_foreign_root.
Proof.
  eapply transfer_preserves_authority;
    [|exact (proj2_sig atomic_batch_foreign_root_witness)].
  eapply create_root_preserves_authority;
    [exact atomic_batch_tree_invariant|exact (proj2_sig atomic_batch_second_root_witness)].
Qed.

Lemma atomic_batch_after_nodes_invariant : AuthorityInvariant atomic_batch_after_nodes.
Proof.
  eapply retire_batch_preserves_authority;
    [exact atomic_batch_tree_invariant|exact (proj2_sig atomic_batch_success_witness)].
Qed.

Lemma atomic_batch_after_root_invariant : AuthorityInvariant atomic_batch_after_root.
Proof.
  eapply retire_batch_preserves_authority;
    [exact atomic_batch_after_nodes_invariant|exact (proj2_sig atomic_batch_root_end_witness)].
Qed.
