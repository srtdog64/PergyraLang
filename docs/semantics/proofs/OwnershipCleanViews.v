(* A local write-through ghost oracle over OwnershipCleanCore's heap.
   StaticView names the intended compiler evidence, but current_view/issuance
   inspect ghost rho/H: this is NOT a discharged static liveness algorithm.
   Linear evidence threading/snapshot binding and the production static
   issuer remain OPEN. Evidence is not a new owning descriptor,
   cleanup ledger or runtime generation field. The scalar update rule does
   not cover replacement of heap-owning payloads; that requires type glue.
   Production liveness issuance and graph/backing binding remain separate. *)
Require Import Stdlib.Lists.List Stdlib.Arith.PeanoNat Stdlib.Bool.Bool.
Require Import Stdlib.Sorting.Permutation Stdlib.micromega.Lia.
Require Import OwnershipTeardownAuthority.
Require Import OwnershipCleanCore.
Import ListNotations.

Record StaticView := mkStaticView {
  view_ticket : nat;
  view_owner : TVar;
  view_blocks : list Block;
  view_start : nat;
  view_length : nat;
  view_write : bool;
  view_kind : LeaseKind
}.

Record StaticViews := mkStaticViews {
  view_next : nat;
  view_rows : list StaticView
}.

Definition empty_views := mkStaticViews 0 [].

Fixpoint find_view (id : nat) (rows : list StaticView) : option StaticView :=
  match rows with
  | [] => None
  | v :: rest => if Nat.eqb id (view_ticket v) then Some v else find_view id rest
  end.

Lemma find_view_issued : forall rows id v,
  find_view id rows = Some v -> In v rows /\ view_ticket v = id.
Proof.
  induction rows as [|head rest IH]; intros id v E; simpl in E; [discriminate|].
  destruct (Nat.eqb id (view_ticket head)) eqn:Ei.
  - inversion E; subst. apply Nat.eqb_eq in Ei. split; [left; reflexivity|symmetry; exact Ei].
  - destruct (IH _ _ E) as [Hi Hv]. split; [right; exact Hi|exact Hv].
Qed.

Definition block_eq_dec : forall a b : Block, {a = b} + {a <> b}.
Proof. decide equality; apply Nat.eq_dec. Defined.

Definition current_view (rho : TEnv) (H : list Block) (v : StaticView) : bool :=
  match tlookup (view_owner v) rho with
  | Some (SNode cs, bs) =>
      if list_eq_dec block_eq_dec bs (view_blocks v)
      then Nat.leb (view_start v + view_length v) (length cs) &&
           forallb (fun b => bmem b H) bs
      else false
  | _ => false
  end.

(* Issuance reads the current OWNED binding and its live footprint. A source
   name or descriptor alone cannot issue this evidence. Tickets monotonically
   increase in the static schedule; ending a view does not recycle its ID. *)
Definition issue_view (s : StaticViews) rho H root start count writable kind
    : option (StaticViews * nat) :=
  match tlookup root rho with
  | Some (SNode cs, bs) =>
      if Nat.leb (start + count) (length cs) && forallb (fun b => bmem b H) bs
      then let v := mkStaticView (view_next s) root bs start count writable kind in
           Some (mkStaticViews (S (view_next s)) (v :: view_rows s), view_next s)
      else None
  | _ => None
  end.

Definition end_view (s : StaticViews) id : option StaticViews :=
  match find_view id (view_rows s) with
  | None => None
  | Some _ => Some (mkStaticViews (view_next s)
                (filter (fun v => negb (Nat.eqb id (view_ticket v))) (view_rows s)))
  end.

Definition read_view (s : StaticViews) rho H id index : option SVal :=
  match find_view id (view_rows s) with
  | None => None
  | Some v =>
      if current_view rho H v && Nat.ltb index (view_length v)
      then match tlookup (view_owner v) rho with
           | Some (SNode cs, _) => nth_error cs (view_start v + index)
           | _ => None
           end
      else None
  end.

Definition scalar_put (v : SVal) index k : SVal :=
  match v with
  | SNode cs => match nth_error cs index with
                | Some (SLeaf _) => SNode (replace_nth cs index (SLeaf k))
                | _ => v
                end
  | _ => v
  end.

Fixpoint scalar_write_owner (rho : TEnv) (root : TVar) index k : TEnv :=
  match rho with
  | [] => []
  | (z, (v, bs)) :: rest =>
      (z, (if tvar_eq_dec z root then scalar_put v index k else v, bs)) ::
      scalar_write_owner rest root index k
  end.

Definition write_view (s : StaticViews) rho H id index k : option TEnv :=
  match find_view id (view_rows s) with
  | None => None
  | Some v =>
      if view_write v && current_view rho H v && Nat.ltb index (view_length v)
      then match tlookup (view_owner v) rho with
           | Some (SNode cs, _) =>
               match nth_error cs (view_start v + index) with
               | Some (SLeaf _) => Some (scalar_write_owner rho (view_owner v) (view_start v + index) k)
               | _ => None
               end
           | _ => None
           end
      else None
  end.

(* Every structural owner footprint affected by a call/drop/relocation must
   be named by its admitted effect fact. This checks that fact's view side;
   it does not infer a call's mutation footprint from its symbol spelling. *)
Definition structural_admitted (s : StaticViews) (affected : list TVar) :=
  forallb (fun v => if in_dec tvar_eq_dec (view_owner v) affected then false else true)
    (view_rows s).

(* Concrete retirement consumer for a local backing. No caller-supplied
   effect list: the dropped owner itself is the affected footprint. The
   existing core invariant supplies the block uniqueness/live checks. *)
Definition drop_view_backing (s : StaticViews) rho H root : option (TEnv * list Block) :=
  if structural_admitted s [root]
  then match tlookup root rho with
       | Some (_, bs) => Some (tremove root rho, free bs H)
       | None => None
       end
  else None.

Definition views_wf (s : StaticViews) : Prop :=
  NoDup (map view_ticket (view_rows s)) /\
  Forall (fun v => view_ticket v < view_next s) (view_rows s).

Lemma empty_views_wf : views_wf empty_views.
Proof. split; constructor. Qed.

Theorem issue_view_is_current : forall s rho H root start count writable kind after id,
  issue_view s rho H root start count writable kind = Some (after, id) ->
  id = view_next s /\ view_next after = S (view_next s) /\
  exists v, find_view id (view_rows after) = Some v /\ current_view rho H v = true.
Proof.
  intros s rho H root start count writable kind after id E. unfold issue_view in E.
  destruct (tlookup root rho) as [[[n|cs] bs]|] eqn:Er; try discriminate.
  destruct (Nat.leb (start + count) (length cs) && forallb (fun b => bmem b H) bs)
    eqn:Ec; [|discriminate].
  inversion E; subst. split; [reflexivity|]. split; [reflexivity|].
  exists (mkStaticView (view_next s) root bs start count writable kind).
  split; [simpl; rewrite Nat.eqb_refl; reflexivity|].
  unfold current_view; simpl. rewrite Er.
  destruct (list_eq_dec block_eq_dec bs bs); [exact Ec|contradiction].
Qed.

Theorem issue_view_preserves_fresh_tickets : forall s rho H root start count writable kind after id,
  views_wf s -> issue_view s rho H root start count writable kind = Some (after, id) -> views_wf after.
Proof.
  intros s rho H root start count writable kind after id [HN HF] E.
  unfold issue_view in E. destruct (tlookup root rho) as [[[n|cs] bs]|]; try discriminate.
  destruct (Nat.leb (start + count) (length cs) && forallb (fun b => bmem b H) bs); [|discriminate].
  inversion E; subst. unfold views_wf; simpl. split.
  - constructor; [|exact HN]. intro Hi. apply in_map_iff in Hi.
    destruct Hi as [v [He Hv]]. apply Forall_forall with (x := v) in HF; [|exact Hv]. lia.
  - constructor; [simpl; lia|]. apply Forall_forall. intros v Hv.
    apply Forall_forall with (x := v) in HF; [|exact Hv]. simpl. lia.
Qed.

Lemma replace_nth_same_length : forall (A : Type) (xs : list A) i value,
  length (replace_nth xs i value) = length xs.
Proof.
  intros A xs. induction xs as [|x xs IH]; intros [|i] value; simpl; try reflexivity.
  f_equal. apply IH.
Qed.

Lemma replace_nth_at_index : forall (A : Type) (xs : list A) i old value,
  nth_error xs i = Some old -> nth_error (replace_nth xs i value) i = Some value.
Proof.
  intros A xs. induction xs as [|x xs IH]; intros [|i] old value E; simpl in *; try discriminate.
  - reflexivity.
  - eapply IH; exact E.
Qed.

Lemma scalar_put_size : forall v index k, vsize (scalar_put v index k) = vsize v.
Proof.
  intros [n|cs] index k; simpl; [reflexivity|].
  destruct (nth_error cs index) as [[n|children]|] eqn:E; try reflexivity.
  pose proof (replace_sizes cs index (SLeaf n) (SLeaf k) E) as H.
  simpl in *. lia.
Qed.

Lemma scalar_write_lookup : forall rho root index k z,
  tlookup z (scalar_write_owner rho root index k) =
  match tlookup z rho with
  | None => None
  | Some (v, bs) => Some (if tvar_eq_dec z root then scalar_put v index k else v, bs)
  end.
Proof.
  induction rho as [|[a [v bs]] rest IH]; intros root index k z; simpl; [reflexivity|].
  destruct (tvar_eq_dec a z) as [E|E]; [subst; reflexivity|exact (IH _ _ _ _)].
Qed.

Lemma scalar_write_dom : forall rho root index k,
  dom (scalar_write_owner rho root index k) = dom rho.
Proof.
  induction rho as [|[a [v bs]] rest IH]; intros; simpl; [reflexivity|].
  unfold dom in *. simpl in *. f_equal. apply IH.
Qed.

Lemma scalar_write_heap : forall rho root index k,
  heap_of (scalar_write_owner rho root index k) = heap_of rho.
Proof.
  induction rho as [|[a [v bs]] rest IH]; intros; simpl; [reflexivity|].
  unfold heap_of in *. simpl in *. rewrite IH; reflexivity.
Qed.

Theorem scalar_write_preserves_cleanup_invariant : forall rho beta H n R root index k,
  INV rho beta H n R -> INV (scalar_write_owner rho root index k) beta H n R.
Proof.
  intros rho beta H n R root index k [HN [HH [HP [HF [HB [LR LB]]]]]].
  unfold INV. rewrite scalar_write_dom, scalar_write_heap.
  repeat split; try assumption.
  intros z c bs E. rewrite scalar_write_lookup in E.
  destruct (tlookup z rho) as [[v bs']|] eqn:Ez; [|discriminate].
  destruct (tvar_eq_dec z root); inversion E; subst.
  - rewrite scalar_put_size. eapply LR; exact Ez.
  - eapply LR; exact Ez.
Qed.

(* Source-view writes update the backing value, not an independent copied
   view value. This local rule supplies CORR as well as the heap frame. *)
Theorem scalar_write_preserves_source_frame : forall sg rho beta L B x c bs index k,
  CORR sg rho beta L B -> tlookup (Src x) rho = Some (c,bs) ->
  CORR (supd sg x (scalar_put c index k))
       (scalar_write_owner rho (Src x) index k) beta L B.
Proof.
  intros sg rho beta L B x c bs index k HC Er.
  destruct HC as [HO [HB [TR [TB [VO VB]]]]].
  assert (Hxb : ~ In x B).
  { assert (Hown : tlookup (Src x) rho <> None) by congruence.
    destruct (proj1 (HO x) Hown) as [_ Hxb]. exact Hxb. }
  unfold CORR. split; [|split; [exact HB|split; [|split; [exact TB|split]]]].
  - intros z. rewrite <- (HO z), scalar_write_lookup.
    destruct (tlookup (Src z) rho) as [[v blocks]|]; simpl.
    + split; intros _ E; discriminate.
    + tauto.
  - intros z. rewrite scalar_write_lookup, TR. reflexivity.
  - intros z v blocks Ez. rewrite scalar_write_lookup in Ez.
    destruct (tlookup (Src z) rho) as [[old oldblocks]|] eqn:Eold; [|discriminate].
    destruct (tvar_eq_dec (Src z) (Src x)) as [Eq|Neq].
    + inversion Eq; subst z. rewrite Er in Eold. inversion Eold; subst old oldblocks.
      inversion Ez; subst v blocks. apply supd_same.
    + inversion Ez; subst v blocks. rewrite supd_other.
      * eapply VO; exact Eold.
      * intro Eq; subst z. apply Neq; reflexivity.
  - intros z v blocks Ez. assert (HzB : In z B).
    { destruct (proj1 (HB z) ltac:(congruence)) as [_ HzB]. exact HzB. }
    rewrite supd_other; [eapply VB; exact Ez|intro Eq; subst z; contradiction].
Qed.

Theorem admitted_view_write_has_source_frame : forall s sg rho beta H L B id index k rho' view x,
  find_view id (view_rows s) = Some view -> view_owner view = Src x ->
  CORR sg rho beta L B -> write_view s rho H id index k = Some rho' ->
  exists c, sg x = Some c /\
    CORR (supd sg x (scalar_put c (view_start view + index) k)) rho' beta L B.
Proof.
  intros s sg rho beta H L B id index k rho' view x Ef Ex HC E.
  unfold write_view in E. rewrite Ef in E.
  destruct (view_write view && current_view rho H view && Nat.ltb index (view_length view)); [|discriminate].
  rewrite Ex in E. destruct (tlookup (Src x) rho) as [[[n|cs] bs]|] eqn:Er; try discriminate.
  destruct (nth_error cs (view_start view + index)) as [[old|children]|]; try discriminate.
  inversion E; subst rho'.
  exists (SNode cs). split; [destruct HC as [_ [_ [_ [_ [VO _]]]]]; eapply VO; exact Er|].
  eapply scalar_write_preserves_source_frame; eassumption.
Qed.

Theorem admitted_view_write_preserves_storage : forall s rho H id index k rho',
  write_view s rho H id index k = Some rho' ->
  dom rho' = dom rho /\ heap_of rho' = heap_of rho /\
  forall beta n R, INV rho beta H n R -> INV rho' beta H n R.
Proof.
  intros s rho H id index k rho' E. unfold write_view in E.
  destruct (find_view id (view_rows s)); [|discriminate].
  destruct (view_write s0 && current_view rho H s0 && Nat.ltb index (view_length s0)); [|discriminate].
  destruct (tlookup (view_owner s0) rho) as [[[n|cs] bs]|]; try discriminate.
  destruct (nth_error cs (view_start s0 + index)) as [[n|children]|]; try discriminate.
  inversion E; subst. split; [apply scalar_write_dom|split; [apply scalar_write_heap|]].
  intros; apply scalar_write_preserves_cleanup_invariant; assumption.
Qed.

Theorem admitted_view_write_is_write_through : forall s rho H id index k rho',
  write_view s rho H id index k = Some rho' -> read_view s rho' H id index = Some (SLeaf k).
Proof.
  intros s rho H id index k rho' E. unfold write_view in E.
  destruct (find_view id (view_rows s)) as [v|] eqn:Ev; [|discriminate].
  destruct (view_write v && current_view rho H v && Nat.ltb index (view_length v))
    eqn:Ec; [|discriminate].
  apply andb_true_iff in Ec. destruct Ec as [Ec Ei].
  apply andb_true_iff in Ec. destruct Ec as [Ew Ecurrent].
  destruct (tlookup (view_owner v) rho) as [[[n|cs] bs]|] eqn:Er; try discriminate.
  destruct (nth_error cs (view_start v + index)) as [[old|children]|] eqn:Eold; try discriminate.
  inversion E; subst. unfold read_view. rewrite Ev.
  assert (Er' : tlookup (view_owner v)
      (scalar_write_owner rho (view_owner v) (view_start v + index) k) =
      Some (SNode (replace_nth cs (view_start v + index) (SLeaf k)), bs)).
  { rewrite scalar_write_lookup, Er. destruct (tvar_eq_dec (view_owner v) (view_owner v));
      [simpl; rewrite Eold; reflexivity|contradiction]. }
  assert (Ecur' : current_view
      (scalar_write_owner rho (view_owner v) (view_start v + index) k) H v = true).
  { unfold current_view in *. rewrite Er in Ecurrent. rewrite Er'.
    rewrite replace_nth_same_length. exact Ecurrent. }
  rewrite Ecur', Ei. simpl. rewrite Er'. eapply replace_nth_at_index; exact Eold.
Qed.

Theorem active_view_blocks_structural_change : forall s affected v,
  In v (view_rows s) -> In (view_owner v) affected -> structural_admitted s affected = false.
Proof.
  intros s affected v Hv Ho. unfold structural_admitted.
  destruct (forallb _ (view_rows s)) eqn:E; [|reflexivity].
  apply forallb_forall with (x := v) in E; [|exact Hv].
  destruct (in_dec tvar_eq_dec (view_owner v) affected); [discriminate|contradiction].
Qed.

Theorem active_view_blocks_backing_drop : forall s rho H v,
  In v (view_rows s) -> drop_view_backing s rho H (view_owner v) = None.
Proof.
  intros s rho H v Hv. unfold drop_view_backing.
  rewrite (active_view_blocks_structural_change s [view_owner v] v Hv (or_introl eq_refl)). reflexivity.
Qed.

Theorem admitted_backing_drop_refines_core : forall tf tp s rho beta H n R root rho' H',
  INV rho beta H n R -> drop_view_backing s rho H root = Some (rho',H') ->
  texec tf tp rho beta H n (TDrop root) rho' beta H' n [] /\ INV rho' beta H' n R.
Proof.
  intros tf tp s rho beta H n R root rho' H' HI E. unfold drop_view_backing in E.
  destruct (structural_admitted s [root]); [|discriminate].
  destruct (tlookup root rho) as [[v bs]|] eqn:Er; [|discriminate].
  inversion E; subst.
  destruct (inv_drop rho beta H n R root v bs HI Er) as [Hlive [Hunique Hafter]].
  split; [eapply TE_Drop; eassumption|exact Hafter].
Qed.

Theorem ended_view_absent : forall s id after,
  end_view s id = Some after -> find_view id (view_rows after) = None.
Proof.
  intros s id after E. unfold end_view in E.
  destruct (find_view id (view_rows s)); [|discriminate]. inversion E; subst; simpl.
  clear E. induction (view_rows s) as [|v rest IH]; simpl; [reflexivity|].
  destruct (Nat.eqb id (view_ticket v)) eqn:Ev; simpl; [exact IH|].
  rewrite Ev. exact IH.
Qed.

Theorem ended_view_cannot_read_or_write : forall s id after rho H index k,
  end_view s id = Some after ->
  read_view after rho H id index = None /\ write_view after rho H id index k = None.
Proof.
  intros. unfold read_view, write_view. rewrite (ended_view_absent _ _ _ H0). auto.
Qed.

Theorem end_view_preserves_ticket_frontier : forall s id after,
  end_view s id = Some after -> view_next after = view_next s.
Proof.
  intros s id after E. unfold end_view in E.
  destruct (find_view id (view_rows s)); inversion E; reflexivity.
Qed.

Lemma find_view_filtered_old : forall rows id,
  find_view id rows = None -> forall removed,
  find_view id (filter (fun v => negb (Nat.eqb removed (view_ticket v))) rows) = None.
Proof.
  induction rows as [|v rest IH]; intros id E removed; simpl in *; [reflexivity|].
  destruct (Nat.eqb id (view_ticket v)) eqn:Ei; [discriminate|].
  specialize (IH id E removed). destruct (Nat.eqb removed (view_ticket v)); simpl; [exact IH|].
  rewrite Ei. exact IH.
Qed.

Inductive ViewSchedule : StaticViews -> StaticViews -> Prop :=
| VS_Refl : forall s, ViewSchedule s s
| VS_Issue : forall s mid after rho H root start count writable kind id,
    ViewSchedule s mid -> issue_view mid rho H root start count writable kind = Some (after, id) ->
    ViewSchedule s after
| VS_End : forall s mid after id,
    ViewSchedule s mid -> end_view mid id = Some after -> ViewSchedule s after.

Theorem old_absent_ticket_never_reissued : forall s after id,
  ViewSchedule s after -> id < view_next s -> find_view id (view_rows s) = None ->
  id < view_next after /\ find_view id (view_rows after) = None.
Proof.
  intros s after id Run. induction Run; intros Hold Habs.
  - auto.
  - destruct (IHRun Hold Habs) as [Hlt Hnone].
    unfold issue_view in H0. destruct (tlookup root rho) as [[[n|cs] bs]|]; try discriminate.
    destruct (Nat.leb (start + count) (length cs) && forallb (fun b => bmem b H) bs); [|discriminate].
    inversion H0; subst. simpl. split; [lia|].
    destruct (Nat.eqb id (view_next mid)) eqn:E; [apply Nat.eqb_eq in E; lia|exact Hnone].
  - destruct (IHRun Hold Habs) as [Hlt Hnone].
    unfold end_view in H. destruct (find_view id0 (view_rows mid)); [|discriminate].
    inversion H; subst; simpl. split; [exact Hlt|]. apply find_view_filtered_old; exact Hnone.
Qed.

Theorem ended_ticket_never_usable_again : forall s id ended later,
  views_wf s -> end_view s id = Some ended -> ViewSchedule ended later ->
  find_view id (view_rows later) = None.
Proof.
  intros s id ended later [HN HF] He Run.
  assert (Hissued : exists v, find_view id (view_rows s) = Some v).
  { unfold end_view in He. destruct (find_view id (view_rows s)); [eexists; reflexivity|discriminate]. }
  destruct Hissued as [v Hv]. destruct (find_view_issued _ _ _ Hv) as [Hin Hid].
  apply Forall_forall with (x := v) in HF; [|exact Hin].
  rewrite Hid in HF. pose proof (end_view_preserves_ticket_frontier _ _ _ He) as Hfrontier.
  destruct (old_absent_ticket_never_reissued _ _ _ Run
    (eq_ind_r (fun bound => id < bound) HF Hfrontier) (ended_view_absent _ _ _ He)) as [_ Habs].
  exact Habs.
Qed.

(* No graph handle is fabricated here. At a resource boundary, apply the
   existing active_descendant_loan_or_pin_blocks theorem only after proving
   a binding to a lease actually issued by that authority owner. There is no
   view-to-Link coercion or graph-composition theorem in this supplement. *)
