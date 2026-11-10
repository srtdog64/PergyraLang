(* Affine issuance and total local-exit cleanup over the existing raw Region
   machine. Body instructions contain no Drop. This proves arbitrary finite
   checked traces, not just one chosen pointer example. The final release is
   related to OwnershipCleanCore's disjoint heap transition below.

   Scope: one closed local allocation, scalar access admission only. Production
   MIR, machine loads/stores, external effects, and concurrency remain outside
   this model. There are no assumptions about a successful production issuer. *)
Require Import Stdlib.Arith.PeanoNat Stdlib.Bool.Bool Stdlib.Lists.List.
Require Import Stdlib.Sorting.Permutation.
Require Import RawPointerOwnershipClean OwnershipCleanCore.
Import ListNotations.

Definition PointerStore := nat -> option RawPointer.
Definition pointer_empty : PointerStore := fun _ => None.
Definition pointer_put (ps : PointerStore) x p : PointerStore :=
  fun y => if Nat.eqb x y then Some p else ps y.
Definition pointer_clear (ps : PointerStore) x : PointerStore :=
  fun y => if Nat.eqb x y then None else ps y.

Lemma pointer_put_same : forall ps x p, pointer_put ps x p x = Some p.
Proof. intros; unfold pointer_put; rewrite Nat.eqb_refl; reflexivity. Qed.
Lemma pointer_put_other : forall ps x p y,
  x <> y -> pointer_put ps x p y = ps y.
Proof. intros; unfold pointer_put; apply Nat.eqb_neq in H; rewrite H; reflexivity. Qed.
Lemma pointer_clear_same : forall ps x, pointer_clear ps x x = None.
Proof. intros; unfold pointer_clear; rewrite Nat.eqb_refl; reflexivity. Qed.
Lemma pointer_clear_other : forall ps x y,
  x <> y -> pointer_clear ps x y = ps y.
Proof. intros; unfold pointer_clear; apply Nat.eqb_neq in H; rewrite H; reflexivity. Qed.

Record RawMachine := mkRawMachine {
  rm_region : Region;
  rm_store : PointerStore
}.

Definition owner_pointer (r : Region) h : RawPointer :=
  mkRawPointer (region_address r) (region_epoch r) (Owns h).

Record RawLive (m : RawMachine) : Prop := {
  raw_live_region : region_live (rm_region m) = true;
  raw_live_count : release_count (rm_region m) = 0;
  raw_live_owner : exists h,
    region_holder (rm_region m) = Some h /\
    rm_store m h = Some (owner_pointer (rm_region m) h);
  raw_affine_owner : forall x p h,
    rm_store m x = Some p -> raw_role p = Owns h ->
    x = h /\ region_holder (rm_region m) = Some h /\
    p = owner_pointer (rm_region m) h
}.

Definition initial_raw_machine a e h : RawMachine :=
  mkRawMachine (fst (allocate_raw a e h))
    (pointer_put pointer_empty h (snd (allocate_raw a e h))).

Theorem allocation_issues_one_owner : forall a e h,
  RawLive (initial_raw_machine a e h).
Proof.
  intros a e h. constructor; simpl; try reflexivity.
  - exists h. split; [reflexivity|apply pointer_put_same].
  - intros x p owner Hp Hrole. unfold pointer_put in Hp; simpl in Hp.
    destruct (Nat.eqb h x) eqn:Hx; [|discriminate].
    apply Nat.eqb_eq in Hx. inversion Hp; subst p.
    inversion Hrole; subst owner. subst x. repeat split; reflexivity.
Qed.

Theorem live_store_has_unique_owner : forall m x p h y q k,
  RawLive m -> rm_store m x = Some p -> raw_role p = Owns h ->
  rm_store m y = Some q -> raw_role q = Owns k -> x = y /\ h = k.
Proof.
  intros m x p h y q k Hlive Hx Hp Hy Hq.
  destruct (raw_affine_owner _ Hlive _ _ _ Hx Hp) as [Ex [Eh _]].
  destruct (raw_affine_owner _ Hlive _ _ _ Hy Hq) as [Ey [Ek _]].
  rewrite Eh in Ek. inversion Ek. subst. split; congruence.
Qed.

(* Region-only operations preserve ownership identity, even though they can
   change lexical scope state. They cannot reconstruct or mint an owner. *)
Lemma region_frame_preserves_owner : forall r ps r',
  RawLive (mkRawMachine r ps) ->
  region_address r' = region_address r -> region_epoch r' = region_epoch r ->
  region_live r' = region_live r -> region_holder r' = region_holder r ->
  release_count r' = release_count r -> RawLive (mkRawMachine r' ps).
Proof.
  intros r ps r' [Hl Hc [h [Hh Hp]] Ha] Eaddr Eepoch Elive Eholder Ecount.
  assert (Ep : forall k, owner_pointer r' k = owner_pointer r k).
  { intro k. unfold owner_pointer. rewrite Eaddr, Eepoch. reflexivity. }
  constructor; simpl in *.
  - rewrite Elive. exact Hl.
  - rewrite Ecount. exact Hc.
  - exists h. rewrite Eholder, Ep. split; assumption.
  - intros x p k Hx Hr. destruct (Ha _ _ _ Hx Hr) as [Ex [Eh Ep0]].
    rewrite Eholder, Ep. repeat split; assumption.
Qed.

Definition nonowning_pointer p : Prop := forall h, raw_role p <> Owns h.

Lemma borrow_issues_no_owner : forall r p q,
  borrow_raw r p = Some q -> nonowning_pointer q.
Proof.
  intros r p q H. unfold borrow_raw in H.
  destruct (managed_raw_access r p); [|discriminate].
  destruct (raw_role p) as [h|h scope|]; try discriminate;
    inversion H; subst q; intros k; discriminate.
Qed.

Lemma insert_nonowner_preserves : forall r ps dst p,
  RawLive (mkRawMachine r ps) -> ps dst = None -> nonowning_pointer p ->
  RawLive (mkRawMachine r (pointer_put ps dst p)).
Proof.
  intros r ps dst p [Hl Hc [h [Hh Hp]] Ha] Hdst Hnon.
  simpl in *.
  assert (Edst : dst <> h) by (intro E; subst; rewrite Hdst in Hp; discriminate).
  constructor; simpl in *; try assumption.
  - exists h. split; [exact Hh|rewrite pointer_put_other by exact Edst; exact Hp].
  - intros x q k Hx Hr. unfold pointer_put in Hx.
    destruct (Nat.eqb dst x) eqn:E.
    + inversion Hx; subst q. exfalso. exact (Hnon k Hr).
    + exact (Ha _ _ _ Hx Hr).
Qed.

Inductive RawInstruction :=
| RIEnterView (owner : nat)
| RILeaveView
| RIEnterRaw (owner : nat) (capability : bool)
| RILeaveRaw
| RIBorrow (source destination : nat)
| RICopy (source destination : nat)
| RIMove (source destination : nat)
| RIRawAccess (pointer : nat)
| RIViewAccess.

Definition raw_step (m : RawMachine) (i : RawInstruction) : option RawMachine :=
  let r := rm_region m in let ps := rm_store m in
  match i with
  | RIEnterView x =>
      match begin_view r x with Some r' => Some (mkRawMachine r' ps) | None => None end
  | RILeaveView =>
      match end_view r with Some r' => Some (mkRawMachine r' ps) | None => None end
  | RIEnterRaw x cap =>
      match begin_raw r x cap with Some r' => Some (mkRawMachine r' ps) | None => None end
  | RILeaveRaw =>
      match end_raw r with Some r' => Some (mkRawMachine r' ps) | None => None end
  | RIBorrow src dst =>
      match ps src, ps dst with
      | Some p, None => match borrow_raw r p with
                       | Some q => Some (mkRawMachine r (pointer_put ps dst q))
                       | None => None end
      | _, _ => None end
  | RICopy src dst =>
      match ps src, ps dst with
      | Some p, None => match raw_role p with
                       | Owns _ => None
                       | _ => Some (mkRawMachine r (pointer_put ps dst p)) end
      | _, _ => None end
  | RIMove src dst =>
      match ps src, ps dst with
      | Some p, None => match transfer_raw r p dst with
                       | Some (r', q) => Some (mkRawMachine r'
                           (pointer_put (pointer_clear ps src) dst q))
                       | None => None end
      | _, _ => None end
  | RIRawAccess src =>
      match ps src with Some p => if managed_raw_access r p then Some m else None
                      | None => None end
  | RIViewAccess => if view_access r then Some m else None
  end.

Fixpoint run_raw_body (code : list RawInstruction) (m : RawMachine)
    : option RawMachine :=
  match code with
  | [] => Some m
  | i :: rest => match raw_step m i with
                 | Some next => run_raw_body rest next
                 | None => None end
  end.

Lemma affine_move_preserves : forall r ps src dst p r' q,
  RawLive (mkRawMachine r ps) -> ps src = Some p -> ps dst = None ->
  transfer_raw r p dst = Some (r', q) ->
  RawLive (mkRawMachine r'
    (pointer_put (pointer_clear ps src) dst q)).
Proof.
  intros [a e live holder view raw scope count] ps src dst p r' q
    Hlive Hsrc Hdst Hmove.
  destruct Hlive as [Hl Hc Hexists Ha]. simpl in *.
  subst live count.
  unfold transfer_raw in Hmove.
  destruct (raw_role p) as [from|from ticket|] eqn:Erole; try discriminate.
  destruct (Ha src p from Hsrc Erole) as [Esrc [Eholder Ep]].
  subst from p holder. unfold owner_pointer in Hmove; simpl in Hmove.
  unfold same_allocation, transfer_holder, held_by in Hmove; simpl in Hmove.
  repeat rewrite Nat.eqb_refl in Hmove.
  simpl in Hmove.
  destruct (negb view && negb raw && negb (Nat.eqb src dst)) eqn:Eguard;
    try discriminate.
  inversion Hmove; subst r' q; clear Hmove.
  constructor; simpl; try reflexivity.
  - exists dst. split; [reflexivity|apply pointer_put_same].
  - intros x p' k Hx Hr. unfold pointer_put in Hx.
    destruct (Nat.eqb dst x) eqn:Edx.
    + apply Nat.eqb_eq in Edx. subst x. inversion Hx; subst p'.
      inversion Hr; subst k. repeat split; reflexivity.
    + unfold pointer_clear in Hx. destruct (Nat.eqb src x) eqn:Esx;
        try discriminate.
      destruct (Ha _ _ _ Hx Hr) as [Ex [Eh Ep]].
      simpl in Eh. inversion Eh; subst k. subst x.
      rewrite Nat.eqb_refl in Esx. discriminate.
Qed.

Theorem raw_step_preserves_affine_owner : forall m i next,
  RawLive m -> raw_step m i = Some next -> RawLive next.
Proof.
  intros [[a e live holder view raw scope count] ps] i next Hlive Hstep.
  pose proof (raw_live_region _ Hlive) as Hl.
  pose proof (raw_live_count _ Hlive) as Hc. simpl in Hl, Hc. subst live count.
  destruct i as [x| |x cap| |src dst|src dst|src dst|src|];
    cbn [raw_step rm_store rm_region] in Hstep.
  - unfold begin_view in Hstep; simpl in Hstep.
    destruct (held_by (mkRegion a e true holder view raw scope 0) x &&
      negb view && negb raw); try discriminate.
    inversion Hstep; subst next. eapply region_frame_preserves_owner;
      [exact Hlive| | | | |]; reflexivity.
  - unfold end_view in Hstep; simpl in Hstep. destruct view; try discriminate.
    inversion Hstep; subst next. eapply region_frame_preserves_owner;
      [exact Hlive| | | | |]; reflexivity.
  - unfold begin_raw in Hstep; simpl in Hstep.
    destruct (cap && true &&
      held_by (mkRegion a e true holder view raw scope 0) x &&
      negb view && negb raw); try discriminate.
    inversion Hstep; subst next. eapply region_frame_preserves_owner;
      [exact Hlive| | | | |]; reflexivity.
  - unfold end_raw in Hstep; simpl in Hstep. destruct raw; try discriminate.
    inversion Hstep; subst next. eapply region_frame_preserves_owner;
      [exact Hlive| | | | |]; reflexivity.
  - destruct (ps src) as [p|] eqn:Hsrc; try discriminate.
    destruct (ps dst) as [old|] eqn:Hdst; try discriminate.
    destruct (borrow_raw (mkRegion a e true holder view raw scope 0) p)
      as [q|] eqn:Hborrow; try discriminate.
    inversion Hstep; subst next. eapply insert_nonowner_preserves;
      [exact Hlive|exact Hdst|eapply borrow_issues_no_owner; exact Hborrow].
  - destruct (ps src) as [p|] eqn:Hsrc; try discriminate.
    destruct (ps dst) as [old|] eqn:Hdst; try discriminate.
    destruct (raw_role p) as [h|h ticket|] eqn:Hr; try discriminate;
      inversion Hstep; subst next; eapply insert_nonowner_preserves;
      try exact Hlive; try exact Hdst; intros k; rewrite Hr; discriminate.
  - destruct (ps src) as [p|] eqn:Hsrc; try discriminate.
    destruct (ps dst) as [old|] eqn:Hdst; try discriminate.
    destruct (transfer_raw (mkRegion a e true holder view raw scope 0) p dst)
      as [[r' q]|] eqn:Hmove; try discriminate.
    inversion Hstep; subst next. eapply affine_move_preserves; eassumption.
  - destruct (ps src) as [p|] eqn:Hsrc; try discriminate.
    destruct (managed_raw_access (mkRegion a e true holder view raw scope 0) p);
      try discriminate. inversion Hstep; subst next. exact Hlive.
  - destruct (view_access (mkRegion a e true holder view raw scope 0));
      try discriminate. inversion Hstep; subst next. exact Hlive.
Qed.

Theorem arbitrary_checked_trace_preserves_owner : forall code m next,
  RawLive m -> run_raw_body code m = Some next -> RawLive next.
Proof.
  induction code as [|i rest IH]; intros m next Hlive Hrun; simpl in Hrun.
  - inversion Hrun; subst; exact Hlive.
  - destruct (raw_step m i) as [mid|] eqn:E; try discriminate.
    eapply IH; [eapply raw_step_preserves_affine_owner; eassumption|exact Hrun].
Qed.

Theorem owned_pointer_copy_is_refused : forall m src dst p h,
  rm_store m src = Some p -> raw_role p = Owns h ->
  raw_step m (RICopy src dst) = None.
Proof.
  intros m src dst p h Hsrc Hr. unfold raw_step. rewrite Hsrc.
  destruct (rm_store m dst); [reflexivity|rewrite Hr; reflexivity].
Qed.

Definition closed_region (r : Region) : Region :=
  mkRegion (region_address r) (region_epoch r) (region_live r)
    (region_holder r) false false (raw_scope_generation r) (release_count r).

(* These are compiler-inserted lexical exits, using the existing View/raw
   consumers. They perform no release and issue no owner. *)
Definition close_raw_scopes (r : Region) : option Region :=
  match (if view_open r then end_view r else Some r) with
  | Some r1 => if raw_open r1 then end_raw r1 else Some r1
  | None => None
  end.

Lemma close_raw_scopes_total : forall r,
  close_raw_scopes r = Some (closed_region r).
Proof.
  intros [a e live holder view raw scope count].
  destruct view, raw; reflexivity.
Qed.

Definition released_region (r : Region) : Region :=
  mkRegion (region_address r) (region_epoch r) false None false false
    (raw_scope_generation r) 1.

Definition automatic_raw_exit (kind : ExitKind) (m : RawMachine)
    : option RawMachine :=
  match close_raw_scopes (rm_region m) with
  | Some r => match region_holder r with
              | Some h => match rm_store m h with
                          | Some p => match elaborate_exit kind r p with
                                      | Some after => Some (mkRawMachine after pointer_empty)
                                      | None => None end
                          | None => None end
              | None => None end
  | None => None
  end.

Record RawRetired (m : RawMachine) : Prop := {
  raw_retired_region : region_live (rm_region m) = false;
  raw_retired_holder : region_holder (rm_region m) = None;
  raw_retired_count : release_count (rm_region m) = 1;
  raw_retired_store : forall x, rm_store m x = None
}.

Theorem automatic_exit_is_total : forall m kind,
  RawLive m -> automatic_raw_exit kind m =
    Some (mkRawMachine (released_region (rm_region m)) pointer_empty).
Proof.
  intros [r ps] kind [Hl Hc [h [Hh Hp]] Ha]. simpl in *.
  unfold automatic_raw_exit; simpl. rewrite close_raw_scopes_total.
  cbn [closed_region region_holder]. rewrite Hh, Hp.
  unfold elaborate_exit, cleanup_raw, owner_pointer, same_allocation,
    cleanup_holder, held_by. simpl.
  rewrite Hl, Hh, Hc. simpl. repeat rewrite Nat.eqb_refl. reflexivity.
Qed.

Lemma released_machine_is_retired : forall r,
  RawRetired (mkRawMachine (released_region r) pointer_empty).
Proof. intros r; constructor; reflexivity. Qed.

Theorem retired_machine_refuses_access : forall m p,
  RawRetired m -> managed_raw_access (rm_region m) p = false.
Proof.
  intros m p Hret. unfold managed_raw_access.
  rewrite (raw_retired_region _ Hret). reflexivity.
Qed.

Theorem retired_machine_refuses_second_exit : forall m kind,
  RawRetired m -> automatic_raw_exit kind m = None.
Proof.
  intros [r ps] kind Hret. unfold automatic_raw_exit; simpl.
  rewrite close_raw_scopes_total. cbn [closed_region region_holder].
  pose proof (raw_retired_holder _ Hret) as Hr. simpl in Hr.
  rewrite Hr. reflexivity.
Qed.

Theorem every_checked_body_has_one_automatic_cleanup : forall a e h code after kind,
  run_raw_body code (initial_raw_machine a e h) = Some after ->
  exists finished,
    automatic_raw_exit kind after = Some finished /\ RawRetired finished /\
    (forall p, managed_raw_access (rm_region finished) p = false) /\
    (forall again, automatic_raw_exit again finished = None).
Proof.
  intros a e h code after kind Hrun.
  assert (Hlive : RawLive after).
  { eapply arbitrary_checked_trace_preserves_owner;
      [apply allocation_issues_one_owner|exact Hrun]. }
  eexists. split; [apply automatic_exit_is_total; exact Hlive|].
  pose proof (released_machine_is_retired (rm_region after)) as Hr.
  split; [exact Hr|]. split.
  - intros p. apply retired_machine_refuses_access; exact Hr.
  - intros again. apply retired_machine_refuses_second_exit; exact Hr.
Qed.

Theorem body_trace_never_releases_early : forall a e h code after,
  run_raw_body code (initial_raw_machine a e h) = Some after ->
  region_live (rm_region after) = true /\ release_count (rm_region after) = 0.
Proof.
  intros a e h code after Hrun.
  assert (Hlive : RawLive after) by
    (eapply arbitrary_checked_trace_preserves_owner;
      [apply allocation_issues_one_owner|exact Hrun]).
  split; [exact (raw_live_region _ Hlive)|exact (raw_live_count _ Hlive)].
Qed.

Theorem omitting_exit_cleanup_leaks : forall a e h code after,
  run_raw_body code (initial_raw_machine a e h) = Some after -> ~ RawRetired after.
Proof.
  intros a e h code after Hrun Hret.
  destruct (body_trace_never_releases_early _ _ _ _ _ Hrun) as [Hl _].
  rewrite (raw_retired_region _ Hret) in Hl. discriminate.
Qed.

Theorem shallow_owner_copy_violates_affinity : forall a e src dst,
  src <> dst ->
  let m := initial_raw_machine a e src in
  ~ RawLive (mkRawMachine (rm_region m)
       (pointer_put (rm_store m) dst (owner_pointer (rm_region m) src))).
Proof.
  intros a e src dst Hneq m Hlive.
  assert (E : pointer_put (rm_store m) dst
    (owner_pointer (rm_region m) src) dst =
    Some (owner_pointer (rm_region m) src)) by apply pointer_put_same.
  destruct (raw_affine_owner _ Hlive dst _ src E eq_refl) as [Ex _].
  apply Hneq. symmetry. exact Ex.
Qed.

Theorem checked_move_removes_old_binding : forall m src dst next,
  raw_step m (RIMove src dst) = Some next ->
  rm_store next src = None /\ raw_step next (RIRawAccess src) = None.
Proof.
  intros [r ps] src dst next E. cbn [raw_step rm_region rm_store] in E.
  destruct (ps src) as [p|] eqn:Hsrc; try discriminate.
  destruct (ps dst) as [old|] eqn:Hdst; try discriminate.
  assert (Hneq : dst <> src) by (intro H; subst; rewrite Hsrc in Hdst; discriminate).
  destruct (transfer_raw r p dst) as [[r' q]|] eqn:Hmove; try discriminate.
  inversion E; subst next; clear E.
  assert (Hnone : pointer_put (pointer_clear ps src) dst q src = None).
  { rewrite pointer_put_other by exact Hneq. apply pointer_clear_same. }
  split; [exact Hnone|]. cbn [raw_step rm_region rm_store].
  rewrite Hnone. reflexivity.
Qed.

Definition raw_is_move i : bool :=
  match i with RIMove _ _ => true | _ => false end.

Lemma nonmove_retains_region_identity : forall m i next,
  raw_is_move i = false -> raw_step m i = Some next ->
  region_address (rm_region next) = region_address (rm_region m) /\
  region_epoch (rm_region next) = region_epoch (rm_region m) /\
  region_holder (rm_region next) = region_holder (rm_region m).
Proof.
  intros [r ps] i next Hnot Hstep.
  destruct i as [x| |x cap| |src dst|src dst|src dst|src|];
    try discriminate; cbn [raw_step rm_region rm_store] in Hstep.
  - unfold begin_view in Hstep.
    destruct (region_live r && held_by r x && negb (view_open r) && negb (raw_open r));
      try discriminate. inversion Hstep; subst; repeat split; reflexivity.
  - unfold end_view in Hstep. destruct (view_open r); try discriminate.
    inversion Hstep; subst; repeat split; reflexivity.
  - unfold begin_raw in Hstep.
    destruct (cap && region_live r && held_by r x &&
      negb (view_open r) && negb (raw_open r)); try discriminate.
    inversion Hstep; subst; repeat split; reflexivity.
  - unfold end_raw in Hstep. destruct (raw_open r); try discriminate.
    inversion Hstep; subst; repeat split; reflexivity.
  - destruct (ps src) as [p|]; try discriminate.
    destruct (ps dst); try discriminate.
    destruct (borrow_raw r p); try discriminate.
    inversion Hstep; subst; repeat split; reflexivity.
  - destruct (ps src) as [p|]; try discriminate.
    destruct (ps dst); try discriminate.
    destruct (raw_role p); try discriminate;
      inversion Hstep; subst; repeat split; reflexivity.
  - destruct (ps src) as [p|]; try discriminate.
    destruct (managed_raw_access r p); try discriminate.
    inversion Hstep; subst; repeat split; reflexivity.
  - destruct (view_access r); try discriminate.
    inversion Hstep; subst; repeat split; reflexivity.
Qed.

Lemma successful_move_identity : forall m src dst next,
  RawLive m -> raw_step m (RIMove src dst) = Some next ->
  region_holder (rm_region m) = Some src /\ src <> dst /\
  region_holder (rm_region next) = Some dst /\
  region_epoch (rm_region next) = region_epoch (rm_region m).
Proof.
  intros [r ps] src dst next Hlive E.
  cbn [raw_step rm_region rm_store] in E.
  destruct (ps src) as [p|] eqn:Hsrc; try discriminate.
  destruct (ps dst) as [old|] eqn:Hdst; try discriminate.
  assert (Hneq : src <> dst) by (intro H; subst; rewrite Hsrc in Hdst; discriminate).
  unfold transfer_raw in E.
  destruct (raw_role p) as [from|from ticket|] eqn:Hr; try discriminate.
  destruct (raw_affine_owner _ Hlive _ _ _ Hsrc Hr) as [Ex [Eh Ep]]. simpl in Ex, Eh.
  subst from. destruct (same_allocation r p); try discriminate.
  unfold transfer_holder in E.
  destruct (region_live r && held_by r src && negb (view_open r) &&
    negb (raw_open r) && negb (Nat.eqb src dst)); try discriminate.
  inversion E; subst next. simpl. repeat split; assumption || reflexivity.
Qed.

(* Ghost pointer bindings and leases erase; owner moves are the canonical
   machine's TMove. This is ownership-event refinement, not machine-load
   semantics. The footprint link below must be supplied at allocation. *)
Definition raw_core_instruction i : TStmt :=
  match i with RIMove src dst => TMove (Tmp dst) (Tmp src) | _ => TSkip end.

Fixpoint raw_core_body code : TStmt :=
  match code with [] => TSkip
  | i :: rest => TSeq (raw_core_instruction i) (raw_core_body rest) end.

Definition CoreOwnerMirror h (rho beta : TEnv) (value : TValue) : Prop :=
  (forall x, tlookup (Tmp x) rho =
    if Nat.eq_dec h x then Some value else None) /\
  (forall x, tlookup (Tmp x) beta = None).

Definition RawCoreLink m rho beta (value : TValue) : Prop :=
  exists h, region_holder (rm_region m) = Some h /\
    CoreOwnerMirror h rho beta value /\
    snd value = alloc (region_epoch (rm_region m)) (vsize (fst value)).

Lemma core_mirror_after_move : forall src dst rho beta value,
  CoreOwnerMirror src rho beta value ->
  CoreOwnerMirror dst ((Tmp dst, value) :: tremove (Tmp src) rho) beta value.
Proof.
  intros src dst rho beta value [Hr Hb]. split; [|exact Hb].
  intro x. rewrite lk_tmp_tmp. destruct (Nat.eq_dec dst x); [reflexivity|].
  rewrite tlookup_tremove. destruct (tvar_eq_dec (Tmp src) (Tmp x)) as [E|E].
  - reflexivity.
  - rewrite Hr. destruct (Nat.eq_dec src x); [subst; exfalso; apply E; reflexivity|reflexivity].
Qed.

Theorem checked_step_refines_core_ownership : forall m i next tf tp rho beta H n R value,
  RawLive m -> RawCoreLink m rho beta value -> INV rho beta H n R ->
  raw_step m i = Some next ->
  exists rho', texec tf tp rho beta H n (raw_core_instruction i) rho' beta H n [] /\
    INV rho' beta H n R /\ RawCoreLink next rho' beta value.
Proof.
  intros m i next tf tp rho beta H n R value Hl [h [Hh [Hmirror Hfoot]]] HI E.
  destruct i as [x| |x cap| |src dst|src dst|src dst|src|].
  all: try (match type of E with
    | raw_step ?m0 ?i0 = Some ?n0 =>
      destruct (nonmove_retains_region_identity m0 i0 n0 eq_refl E)
        as [Ha [He Hholder]]
    end;
    exists rho; split; [apply TE_Skip|]; split; [exact HI|];
    exists h; split; [rewrite Hholder; exact Hh|];
    split; [exact Hmirror|rewrite He; exact Hfoot]).
  destruct (successful_move_identity _ _ _ _ Hl E) as [Hsrc [Hneq [Hdst He]]].
  rewrite Hh in Hsrc. inversion Hsrc; subst h.
  destruct Hmirror as [Hlookup Hbeta].
  assert (Hsource : tlookup (Tmp src) rho = Some value).
  { rewrite Hlookup. destruct (Nat.eq_dec src src); [reflexivity|contradiction]. }
  assert (Hunbound : unbound (Tmp dst) rho beta).
  { split; [rewrite Hlookup; destruct (Nat.eq_dec src dst); [contradiction|reflexivity]|apply Hbeta]. }
  exists ((Tmp dst, value) :: tremove (Tmp src) rho).
  split; [apply TE_Move; assumption|]. split.
  - eapply inv_move; eassumption.
  - exists dst. split; [exact Hdst|]. split.
    + apply core_mirror_after_move. split; assumption.
    + rewrite He. exact Hfoot.
Qed.

Theorem checked_trace_refines_core_ownership : forall code m next tf tp rho beta H n R value,
  RawLive m -> RawCoreLink m rho beta value -> INV rho beta H n R ->
  run_raw_body code m = Some next ->
  exists rho', texec tf tp rho beta H n (raw_core_body code) rho' beta H n [] /\
    INV rho' beta H n R /\ RawCoreLink next rho' beta value.
Proof.
  induction code as [|i rest IH]; intros m next tf tp rho beta H n R value Hl HC HI E;
    simpl in E.
  - inversion E; subst. exists rho. split; [apply TE_Skip|]. split; assumption.
  - destruct (raw_step m i) as [mid|] eqn:Estep; try discriminate.
    destruct (checked_step_refines_core_ownership _ _ _ tf tp _ _ _ _ _ _ Hl HC HI Estep)
      as [rho1 [Ecore [HI1 HC1]]].
    destruct (IH mid next tf tp rho1 beta H n R value
      (raw_step_preserves_affine_owner _ _ _ Hl Estep) HC1 HI1 E)
      as [rho2 [Erest [HI2 HC2]]].
    exists rho2. split; [|split; assumption]. simpl.
    eapply TE_Seq; [exact Ecore|exact Erest|reflexivity].
Qed.

Theorem automatic_exit_refines_core_drop : forall m kind tf tp rho beta H n R value,
  RawLive m -> RawCoreLink m rho beta value -> INV rho beta H n R ->
  exists finished h,
    automatic_raw_exit kind m = Some finished /\ RawRetired finished /\
    texec tf tp rho beta H n (TDrop (Tmp h))
      (tremove (Tmp h) rho) beta (free (snd value) H) n [] /\
    INV (tremove (Tmp h) rho) beta (free (snd value) H) n R /\
    (forall y c bs, y <> Tmp h -> tlookup y rho = Some (c, bs) ->
      incl bs (free (snd value) H)).
Proof.
  intros m kind tf tp rho beta H n R [c bs] Hl [h [Hh [[Hr Hb] Hfoot]]] HI.
  assert (Hsource : tlookup (Tmp h) rho = Some (c, bs)).
  { rewrite Hr. destruct (Nat.eq_dec h h); [reflexivity|contradiction]. }
  destruct (inv_drop _ _ _ _ _ _ _ _ HI Hsource) as [Hbs [HN HIafter]].
  exists (mkRawMachine (released_region (rm_region m)) pointer_empty), h.
  split; [apply automatic_exit_is_total; exact Hl|].
  split; [apply released_machine_is_retired|].
  split; [apply TE_Drop with (c := c); assumption|].
  split; [exact HIafter|].
  intros y c' bs' Hneq Hy.
  assert (Hy' : tlookup y (tremove (Tmp h) rho) = Some (c', bs')).
  { rewrite tlookup_tremove. destruct (tvar_eq_dec (Tmp h) y);
      [congruence|exact Hy]. }
  eapply inv_read with (c := c'); [exact HIafter|].
  unfold tread. rewrite Hy'. reflexivity.
Qed.

Theorem checked_body_and_exit_refine_core : forall code m next kind tf tp rho beta H n R value,
  RawLive m -> RawCoreLink m rho beta value -> INV rho beta H n R ->
  run_raw_body code m = Some next ->
  exists finished h rho_mid,
    automatic_raw_exit kind next = Some finished /\ RawRetired finished /\
    texec tf tp rho beta H n
      (TSeq (raw_core_body code) (TDrop (Tmp h)))
      (tremove (Tmp h) rho_mid) beta (free (snd value) H) n [] /\
    INV (tremove (Tmp h) rho_mid) beta (free (snd value) H) n R /\
    (forall y c bs, y <> Tmp h -> tlookup y rho_mid = Some (c, bs) ->
      incl bs (free (snd value) H)).
Proof.
  intros code m next kind tf tp rho beta H n R value Hl HC HI E.
  destruct (checked_trace_refines_core_ownership _ _ _ tf tp _ _ _ _ _ _ Hl HC HI E)
    as [rho_mid [Ebody [HImid HCmid]]].
  destruct (automatic_exit_refines_core_drop next kind tf tp rho_mid beta H n R value
    (arbitrary_checked_trace_preserves_owner _ _ _ Hl E) HCmid HImid)
    as [finished [h [Eexit [Hret [Edrop [HIafter Hother]]]]]].
  exists finished, h, rho_mid. split; [exact Eexit|]. split; [exact Hret|].
  split; [|split; assumption].
  eapply TE_Seq; [exact Ebody|exact Edrop|reflexivity].
Qed.

Lemma empty_core_inv : forall n, INV [] [] [] n [].
Proof.
  intros n. unfold INV. repeat split; try constructor;
    intros; try contradiction; discriminate.
Qed.

Lemma initial_raw_core_link : forall a e h c,
  RawCoreLink (initial_raw_machine a e h)
    [(Tmp h, (c, alloc e (vsize c)))] [] (c, alloc e (vsize c)).
Proof.
  intros a e h c. exists h. split; [reflexivity|]. split; [|reflexivity].
  split; [|reflexivity]. intro x. rewrite lk_tmp_tmp.
  destruct (Nat.eq_dec h x); reflexivity.
Qed.

Lemma initial_raw_core_inv : forall e h c,
  INV [(Tmp h, (c, alloc e (vsize c)))] [] (alloc e (vsize c)) (S e) [].
Proof.
  intros e h c. pose proof (inv_alloc [] [] [] e [] (Tmp h) c
    (empty_core_inv e) (conj eq_refl eq_refl)) as HI.
  rewrite app_nil_r in HI. exact HI.
Qed.

Lemma free_whole_allocation : forall e size, free (alloc e size) (alloc e size) = [].
Proof.
  intros e size.
  assert (HP : Permutation (free (alloc e size) (alloc e size)) []).
  { apply free_perm; rewrite app_nil_r; [apply alloc_nodup|apply Permutation_refl]. }
  apply Permutation_nil. apply Permutation_sym. exact HP.
Qed.

Lemma empty_live_heap_has_no_owned_bindings : forall rho n,
  INV rho [] [] n [] -> rho = [].
Proof.
  intros [|[x [c bs]] rest] n HI; [reflexivity|].
  destruct HI as [HD [HH [HP [HF [HB [HL Hr]]]]]].
  simpl in HP. rewrite app_nil_r in HP. apply Permutation_nil in HP.
  apply app_eq_nil in HP as [Ebs Erest].
  specialize (HL x c bs).
  assert (Hlookup : tlookup x ((x, (c, bs)) :: rest) = Some (c, bs)).
  { rewrite tlookup_cons. destruct (tvar_eq_dec x x); [reflexivity|contradiction]. }
  specialize (HL Hlookup). rewrite Ebs in HL. destruct c; simpl in HL; discriminate.
Qed.

(* An arbitrary accepted raw body, allocated in the canonical ownership heap,
   has a complete core execution ending with no live block or owned binding.
   This is a constructive abstract-heap theorem, not an allocator assumption. *)
Theorem closed_raw_program_frees_canonical_heap : forall a e h c code next kind tf tp,
  run_raw_body code (initial_raw_machine a e h) = Some next ->
  exists finished final_holder,
    automatic_raw_exit kind next = Some finished /\ RawRetired finished /\
    texec tf tp [(Tmp h, (c, alloc e (vsize c)))] [] (alloc e (vsize c)) (S e)
      (TSeq (raw_core_body code) (TDrop (Tmp final_holder))) [] [] [] (S e) [].
Proof.
  intros a e h c code next kind tf tp E.
  destruct (checked_body_and_exit_refine_core code (initial_raw_machine a e h)
    next kind tf tp _ [] _ (S e) [] (c, alloc e (vsize c))
    (allocation_issues_one_owner a e h) (initial_raw_core_link a e h c)
    (initial_raw_core_inv e h c) E)
    as [finished [final_holder [rho_mid [Eexit [Hret [Ecore [HIafter Hother]]]]]]].
  simpl in Ecore, HIafter. rewrite free_whole_allocation in Ecore, HIafter.
  rewrite (empty_live_heap_has_no_owned_bindings _ _ HIafter) in Ecore.
  exists finished, final_holder. split; [exact Eexit|]. split; assumption.
Qed.

(* Admission is inhabited: both access routes, borrowed alias copying, two
   owner moves, and an early exit from an open raw scope. No Drop appears in
   this source instruction list. Old borrowed bindings remain in the store
   between scopes, so stale-scope rejection cannot rely on deleting them. *)
Definition raw_lifecycle_witness : list RawInstruction :=
  [RIEnterView 0; RIViewAccess; RILeaveView;
   RIEnterRaw 0 true; RIBorrow 0 1; RICopy 1 2;
   RIRawAccess 1; RIRawAccess 2; RILeaveRaw;
   RIMove 0 3; RIEnterRaw 3 true; RIRawAccess 3; RILeaveRaw;
   RIMove 3 4; RIEnterRaw 4 true; RIBorrow 4 5; RIRawAccess 5].

Lemma raw_lifecycle_witness_admitted : exists after,
  run_raw_body raw_lifecycle_witness (initial_raw_machine 100 7 0) = Some after.
Proof. vm_compute. eexists; reflexivity. Qed.

Theorem raw_lifecycle_witness_frees_heap : forall kind tf tp c,
  exists after finished h,
    run_raw_body raw_lifecycle_witness (initial_raw_machine 100 7 0) = Some after /\
    automatic_raw_exit kind after = Some finished /\ RawRetired finished /\
    texec tf tp [(Tmp 0, (c, alloc 7 (vsize c)))] [] (alloc 7 (vsize c)) 8
      (TSeq (raw_core_body raw_lifecycle_witness) (TDrop (Tmp h))) [] [] [] 8 [].
Proof.
  intros kind tf tp c. destruct raw_lifecycle_witness_admitted as [after E].
  destruct (closed_raw_program_frees_canonical_heap 100 7 0 c
    raw_lifecycle_witness after kind tf tp E) as [finished [h [Eexit [Hr Ec]]]].
  exists after, finished, h. split; [exact E|]. split; [exact Eexit|].
  split; assumption.
Qed.

Example expired_borrow_trace_refused :
  run_raw_body [RIEnterRaw 0 true; RIBorrow 0 1; RILeaveRaw;
    RIEnterRaw 0 true; RIRawAccess 1] (initial_raw_machine 100 7 0) = None.
Proof. reflexivity. Qed.

Example moved_owner_trace_refused :
  run_raw_body [RIMove 0 3; RIEnterRaw 3 true; RIRawAccess 0]
    (initial_raw_machine 100 7 0) = None.
Proof. reflexivity. Qed.

Example copied_owner_trace_refused :
  run_raw_body [RICopy 0 1] (initial_raw_machine 100 7 0) = None.
Proof. reflexivity. Qed.
