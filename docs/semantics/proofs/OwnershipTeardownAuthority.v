(* One admission boundary over OwnershipTeardown, not a second forest.
   Context identity and the state ledger are trusted machine state. References
   and requests may be copied; neither can mint a ledger entry. The bounded
   unit verifier below is a model algorithm, not the production indexed walker.
   Loans/pins here protect lifetime, not concurrent read/write exclusivity. *)
Require Import OwnershipTeardown.
Require Import Stdlib.Lists.List Stdlib.Arith.PeanoNat Stdlib.Bool.Bool.
Require Import Stdlib.micromega.Lia.
Import ListNotations.

Inductive RetirementTarget := RetireNode (l : Link) | RetireRoot (l : Link).

Definition unit_member_expected (s : St) (t : RetirementTarget)
    (Ul : list nat) (x : nat) : bool :=
  match t with
  | RetireNode l =>
      if Nat.eqb x (fst l) then true else
      match s_node (heap s x) with
      | Some nd => match owner nd with OParent p => inU Ul p | ORoot _ => false end
      | None => false
      end
  | RetireRoot l =>
      match s_node (heap s x) with
      | Some nd => match owner nd with
                   | ORoot r => Nat.eqb r (fst l)
                   | OParent p => inU Ul p
                   end
      | None => false
      end
  end.

Definition valid_unit (s : St) (t : RetirementTarget) (Ul : list nat) : bool :=
  unit_unique Ul &&
  forallb (fun x => Nat.ltb x (bound s)) Ul &&
  forallb (fun x => Bool.eqb (inU Ul x) (unit_member_expected s t Ul x))
    (seq 0 (bound s)).

Definition UnitExact (s : St) (t : RetirementTarget) (Ul : list nat) : Prop :=
  NoDup Ul /\ forall x, In x Ul <->
    match t with RetireNode l => in_sub (heap s) (fst l) x
               | RetireRoot l => under_root (heap s) (fst l) x end.

Lemma valid_unit_parts : forall s t Ul, valid_unit s t Ul = true ->
  NoDup Ul /\ (forall x, In x Ul -> x < bound s) /\
  (forall x, x < bound s ->
    inU Ul x = unit_member_expected s t Ul x).
Proof.
  intros s t Ul H. unfold valid_unit in H.
  repeat rewrite andb_true_iff in H. destruct H as [[Hu Hb] He].
  split; [apply unit_unique_spec; exact Hu |]. split.
  - intros x Hx. apply forallb_forall with (x := x) in Hb; [|exact Hx].
    apply Nat.ltb_lt; exact Hb.
  - intros x Hx. apply forallb_forall with (x := x) in He.
    + apply Bool.eqb_true_iff; exact He.
    + apply in_seq. lia.
Qed.

Lemma root_unit_sound : forall s r g Ul, Inv s ->
  valid_unit s (RetireRoot (r,g)) Ul = true ->
  UnitExact s (RetireRoot (r,g)) Ul.
Proof.
  intros s r g Ul HI HV. destruct (valid_unit_parts _ _ _ HV) as [HN [HB HE]].
  destruct HI as [HO [_ [_ [_ [_ Hbound]]]]]. split; [exact HN |].
  assert (ER : forall x, rooted (heap s) x ->
    (In x Ul <-> under_root (heap s) r x)).
  { intros x HX. induction HX as [x nd q Hx Hq | x nd p Hx Hp HR IH].
    - specialize (HE x (Hbound x (ex_intro _ nd Hx))).
      unfold unit_member_expected in HE. cbn in HE. rewrite Hx, Hq in HE.
      rewrite <- inU_spec, HE, Nat.eqb_eq. split.
      + intro E; subst q. eapply URHere; eauto.
      + intro H. destruct (under_root_inv _ _ _ H) as [nd' [Hx' [Eq | [p' [Eq _]]]]];
        rewrite Hx in Hx'; injection Hx' as E; subst nd'; rewrite Hq in Eq; congruence.
    - specialize (HE x (Hbound x (ex_intro _ nd Hx))).
      unfold unit_member_expected in HE. cbn in HE. rewrite Hx, Hp in HE.
      rewrite <- inU_spec, HE, inU_spec, IH. split.
      + intro H; eapply URDown; eauto.
      + intro H. destruct (under_root_inv _ _ _ H) as [nd' [Hx' [Eq | [p' [Eq Hu]]]]];
        rewrite Hx in Hx'; injection Hx' as E; subst nd'; rewrite Hp in Eq.
        * discriminate.
        * injection Eq as E; subst p'; exact Hu. }
  intros x. split.
  - intro Hx. specialize (HE x (HB x Hx)).
    assert (HL : live (heap s) x).
    { destruct (s_node (heap s x)) as [nd|] eqn:E; [exists nd; exact E |].
      unfold unit_member_expected in HE; cbn in HE; rewrite E in HE.
      rewrite (proj2 (inU_spec Ul x) Hx) in HE; discriminate. }
    apply (proj1 (ER x (HO x HL))); exact Hx.
  - intro Hx. apply (proj2 (ER x (under_root_rooted _ _ _ Hx))); exact Hx.
Qed.

Lemma node_unit_sound : forall s n g Ul, Inv s -> resolves (heap s) (n,g) ->
  valid_unit s (RetireNode (n,g)) Ul = true ->
  UnitExact s (RetireNode (n,g)) Ul.
Proof.
  intros s n g Ul HI Hres HV. destruct (valid_unit_parts _ _ _ HV) as [HN [HB HE]].
  destruct HI as [HO [_ [_ [_ [_ Hbound]]]]]. split; [exact HN |].
  assert (ER : forall x, rooted (heap s) x ->
    (In x Ul <-> in_sub (heap s) n x)).
  { intros x HX. induction HX as [x nd q Hx Hq | x nd p Hx Hp HR IH].
    - specialize (HE x (Hbound x (ex_intro _ nd Hx))).
      unfold unit_member_expected in HE; cbn in HE.
      destruct (Nat.eqb x n) eqn:En.
      + apply Nat.eqb_eq in En; subst x. rewrite <- inU_spec, HE.
        split; [intros _; constructor | intros _; reflexivity].
      + rewrite Hx, Hq in HE. rewrite <- inU_spec, HE.
        split; [discriminate | intro H].
        destruct (in_sub_inv _ _ _ H) as [Eq | [nd' [p' [Hx' [Hp' _]]]]].
        * apply Nat.eqb_neq in En; contradiction.
        * rewrite Hx in Hx'; injection Hx' as E; subst nd'; rewrite Hq in Hp'; discriminate.
    - specialize (HE x (Hbound x (ex_intro _ nd Hx))).
      unfold unit_member_expected in HE; cbn in HE.
      destruct (Nat.eqb x n) eqn:En.
      + apply Nat.eqb_eq in En; subst x. rewrite <- inU_spec, HE.
        split; [intros _; constructor | intros _; reflexivity].
      + rewrite Hx, Hp in HE. rewrite <- inU_spec, HE, inU_spec, IH.
        split; [intro H; eapply SubDown; eauto | intro H].
        destruct (in_sub_inv _ _ _ H) as [Eq | [nd' [p' [Hx' [Hp' Hsub]]]]].
        * apply Nat.eqb_neq in En; contradiction.
        * rewrite Hx in Hx'; injection Hx' as E; subst nd'; rewrite Hp in Hp'.
          injection Hp' as E; subst p'; exact Hsub. }
  intros x. split.
  - intro Hx. specialize (HE x (HB x Hx)).
    assert (HL : live (heap s) x).
    { destruct (Nat.eq_dec x n) as [-> | Hne]; [exact (proj2 Hres) |].
      destruct (s_node (heap s x)) as [nd|] eqn:E; [exists nd; exact E |].
      unfold unit_member_expected in HE; cbn in HE.
      assert (En : Nat.eqb x n = false) by (apply Nat.eqb_neq; exact Hne).
      rewrite En, E in HE; rewrite (proj2 (inU_spec Ul x) Hx) in HE; discriminate. }
    apply (proj1 (ER x (HO x HL))); exact Hx.
  - intro Hx. apply (proj2 (ER x (HO x (in_sub_live _ _ _ (proj2 Hres) Hx)))); exact Hx.
Qed.

(* The checker is complete for the canonical forest unit certificates. Its
   finite full-bound scan must not be mistaken for CL4's indexed unit walker. *)
Lemma valid_unit_complete : forall s t Ul, Inv s ->
  (match t with RetireNode l => resolves (heap s) l | RetireRoot _ => True end) ->
  UnitExact s t Ul -> valid_unit s t Ul = true.
Proof.
  intros s t Ul HI HR [HN HU]. destruct HI as [HO [_ [_ [_ [_ HB]]]]].
  unfold valid_unit. repeat rewrite andb_true_iff. split; [split |].
  - apply unit_unique_spec; exact HN.
  - apply forallb_forall. intros x Hx. apply Nat.ltb_lt. apply HB.
    apply HU in Hx. destruct t as [[n g]|[r g]]; cbn in *.
    + exact (in_sub_live _ _ _ (proj2 HR) Hx).
    + exact (under_root_live _ _ _ Hx).
  - apply forallb_forall. intros x _. apply Bool.eqb_true_iff.
    apply eq_true_iff_eq. rewrite inU_spec, HU.
    destruct t as [[n g]|[r g]]; cbn in *; unfold unit_member_expected; cbn.
    + destruct (Nat.eqb x n) eqn:En.
      * apply Nat.eqb_eq in En; subst x. split; [intros _; reflexivity | intros _; constructor].
      * destruct (s_node (heap s x)) as [nd|] eqn:E.
        -- destruct (owner nd) as [r|p] eqn:Ep.
           ++ split; [intro H | discriminate].
              destruct (in_sub_inv _ _ _ H) as [Eq | [nd' [p' [E' [Ep' _]]]]].
              ** apply Nat.eqb_neq in En; contradiction.
              ** rewrite E in E'; injection E' as Et; subst nd'; rewrite Ep in Ep'; discriminate.
           ++ rewrite inU_spec, HU. cbn. split.
              ** intro H. destruct (in_sub_inv _ _ _ H) as [Eq | [nd' [p' [E' [Ep' Hsub]]]]].
                 --- apply Nat.eqb_neq in En; contradiction.
                 --- rewrite E in E'; injection E' as Et; subst nd'; rewrite Ep in Ep'.
                     injection Ep' as Et; subst p'; exact Hsub.
              ** intro H; eapply SubDown; eauto.
        -- split; [intro H | discriminate].
           destruct (in_sub_inv _ _ _ H) as [Eq | [nd' [p' [E' _]]]].
           ++ apply Nat.eqb_neq in En; contradiction.
           ++ congruence.
    + destruct (s_node (heap s x)) as [nd|] eqn:E.
      * destruct (owner nd) as [q|p] eqn:Ep.
        -- rewrite Nat.eqb_eq. split.
           ++ intro H. destruct (under_root_inv _ _ _ H) as [nd' [E' [Ep' | [p' [Ep' _]]]]];
              rewrite E in E'; injection E' as Et; subst nd'; rewrite Ep in Ep'; congruence.
           ++ intro Eq; subst q; eapply URHere; eauto.
        -- rewrite inU_spec, HU. cbn. split.
           ++ intro H. destruct (under_root_inv _ _ _ H) as [nd' [E' [Ep' | [p' [Ep' Hroot]]]]];
              rewrite E in E'; injection E' as Et; subst nd'; rewrite Ep in Ep'.
              ** discriminate.
              ** injection Ep' as Et; subst p'; exact Hroot.
           ++ intro H; eapply URDown; eauto.
      * split; [intro H | discriminate]. destruct (under_root_live _ _ _ H) as [nd E']; congruence.
Qed.

Inductive LeaseKind := BorrowLease | PinLease.
Record Lease := mkLease { lease_id : nat; lease_holder : nat;
                         lease_target : Link; lease_kind : LeaseKind }.
Record AuthorityState := mkAuthorityState {
  forest : St;
  acting_context : nat;
  (* One authoritative cell per root: epoch and holder, not a copyable permit. *)
  cleanup_rights : nat -> option (nat * nat);
  active_leases : list Lease;
  next_lease : nat
}.

Definition put_right (rs : nat -> option (nat * nat)) r value :=
  fun q => if Nat.eqb q r then value else rs q.

Definition holds_cleanup (a : AuthorityState) (l : Link) : bool :=
  check_root (forest a) l &&
  match cleanup_rights a (fst l) with
  | Some (epoch, holder) => Nat.eqb epoch (snd l) && Nat.eqb holder (acting_context a)
  | None => false
  end.

Fixpoint node_root (fuel : nat) (h : Heap) (x : nat) : option nat :=
  match fuel with
  | 0 => None
  | S f => match s_node (h x) with
           | None => None
           | Some nd => match owner nd with
                        | ORoot r => Some r
                        | OParent p => node_root f h p
                        end
           end
  end.

Lemma node_root_sound : forall fuel h x r, node_root fuel h x = Some r -> under_root h r x.
Proof.
  induction fuel as [|f IH]; intros h x r H; [discriminate |].
  cbn in H. destruct (s_node (h x)) as [nd|] eqn:E; [|discriminate].
  destruct (owner nd) as [q|p] eqn:Ep.
  - injection H as Et; subst q; eapply URHere; eauto.
  - eapply URDown; eauto.
Qed.

Definition target_authorized (a : AuthorityState) (t : RetirementTarget) : bool :=
  match t with
  | RetireRoot l => holds_cleanup a l
  | RetireNode l =>
      match node_root (bound (forest a)) (heap (forest a)) (fst l) with
      | Some r => holds_cleanup a (r, root_gen (forest a) r)
      | None => false
      end
  end.

Definition target_current (s : St) (t : RetirementTarget) : bool :=
  match t with RetireRoot l => check_root s l
  | RetireNode l => match resolve_node (heap s) l with Some _ => true | None => false end
  end.

Definition unit_quiet (a : AuthorityState) (Ul : list nat) : bool :=
  forallb (fun lease => negb (inU Ul (fst (lease_target lease)))) (active_leases a).

Inductive AuthorityFailure := StaleIdentity | MissingCleanupRight | InvalidUnit
  | ActiveLease | RootAlreadyLive | RightAlreadyIssued | OccupiedSlot | MissingLease | WrongLeaseHolder.
Inductive AuthorityResult := Accepted (after : AuthorityState)
  | Refused (why : AuthorityFailure) (unchanged : AuthorityState).

Definition retire (a : AuthorityState) (t : RetirementTarget) (Ul : list nat) : AuthorityResult :=
  if target_current (forest a) t then
    if target_authorized a t then
      if valid_unit (forest a) t Ul then
        if unit_quiet a Ul then
          Accepted (mkAuthorityState
            (match t with RetireNode _ => teardown (forest a) Ul
                          | RetireRoot l => root_drop (forest a) (fst l) Ul end)
            (acting_context a)
            (match t with RetireNode _ => cleanup_rights a
                          | RetireRoot l => put_right (cleanup_rights a) (fst l) None end)
            (active_leases a) (next_lease a))
        else Refused ActiveLease a
      else Refused InvalidUnit a
    else Refused MissingCleanupRight a
  else Refused StaleIdentity a.

Definition create_root (a : AuthorityState) (r : nat) : AuthorityResult :=
  if inU (roots (forest a)) r then Refused RootAlreadyLive a else
  match cleanup_rights a r with
  | Some _ => Refused RightAlreadyIssued a
  | None => Accepted (mkAuthorityState
      (mkSt (heap (forest a)) (rix (forest a)) (kids (forest a))
        (r :: roots (forest a)) (bound (forest a)) (root_gen (forest a)))
      (acting_context a)
      (put_right (cleanup_rights a) r (Some (root_gen (forest a) r, acting_context a)))
      (active_leases a) (next_lease a))
  end.

Definition transfer_cleanup (a : AuthorityState) (l : Link) (new_holder : nat) : AuthorityResult :=
  if target_current (forest a) (RetireRoot l) then
    if holds_cleanup a l then
      Accepted (mkAuthorityState (forest a) (acting_context a)
        (put_right (cleanup_rights a) (fst l) (Some (snd l, new_holder)))
        (active_leases a) (next_lease a))
    else Refused MissingCleanupRight a
  else Refused StaleIdentity a.

(* Pin/loan identifiers are machine-issued and never reset, including reuse.
   Ending a lease requires its holder, not just a copied node or lease number. *)
Definition begin_lease (a : AuthorityState) (l : Link) (kind : LeaseKind)
    (borrower : nat) : AuthorityResult :=
  if target_current (forest a) (RetireNode l) then
    if target_authorized a (RetireNode l) then
      Accepted (mkAuthorityState (forest a) (acting_context a) (cleanup_rights a)
        (mkLease (next_lease a) borrower l kind :: active_leases a)
        (S (next_lease a)))
    else Refused MissingCleanupRight a
  else Refused StaleIdentity a.

Definition end_lease (a : AuthorityState) (id : nat) : AuthorityResult :=
  match find (fun lease => Nat.eqb (lease_id lease) id) (active_leases a) with
  | None => Refused MissingLease a
  | Some lease =>
      if Nat.eqb (lease_holder lease) (acting_context a) then
        Accepted (mkAuthorityState (forest a) (acting_context a) (cleanup_rights a)
          (filter (fun e => negb (Nat.eqb (lease_id e) id)) (active_leases a)) (next_lease a))
      else Refused WrongLeaseHolder a
  end.

(* Allocation is checked against the same root ledger. Cross-holder reparenting
   and mutable access are not exported by this bounded lifetime boundary. *)
Definition allocate_owned (a : AuthorityState) (i : nat) (o : Owner) (g : nat) : AuthorityResult :=
  let target := match o with ORoot r => RetireRoot (r,g) | OParent p => RetireNode (p,g) end in
  if target_current (forest a) target then
    if target_authorized a target then
      match s_node (heap (forest a) i) with
      | Some _ => Refused OccupiedSlot a
      | None => Accepted (mkAuthorityState
          (mkSt (upd (heap (forest a)) i
              (mkSlot (s_gen (heap (forest a) i)) (Some (mkNode o empty_fields))))
            (rix (forest a)) (kids_add (kids (forest a)) o i) (roots (forest a))
            (Nat.max (bound (forest a)) (S i)) (root_gen (forest a)))
          (acting_context a) (cleanup_rights a) (active_leases a) (next_lease a))
      end
    else Refused MissingCleanupRight a
  else Refused StaleIdentity a.

Lemma target_current_node : forall s l, target_current s (RetireNode l) = true -> resolves (heap s) l.
Proof.
  intros s l H. cbn in H. destruct (resolve_node (heap s) l) as [nd|] eqn:E; [|discriminate].
  apply resolve_node_some in E. destruct E as [Hg Hnd]. split; [exact Hg | exists nd; exact Hnd].
Qed.

Lemma holds_cleanup_spec : forall a r g, holds_cleanup a (r,g) = true <->
  root_resolves (roots (forest a)) (root_gen (forest a)) (r,g) /\
  cleanup_rights a r = Some (g, acting_context a).
Proof.
  intros a r g. unfold holds_cleanup; cbn. rewrite andb_true_iff, check_root_spec.
  destruct (cleanup_rights a r) as [[epoch holder]|] eqn:E.
  - rewrite andb_true_iff, !Nat.eqb_eq. split.
    + intros [Hr [Eg Ec]]; subst; auto.
    + intros [Hr H]; injection H as Eg Ec; auto.
  - split; [intros [_ H]; discriminate | intros [_ H]; discriminate].
Qed.

Theorem copied_reference_is_not_cleanup_authority : forall a r g,
  cleanup_rights a r <> Some (g, acting_context a) -> holds_cleanup a (r,g) = false.
Proof.
  intros a r g H. destruct (holds_cleanup a (r,g)) eqn:E; [|reflexivity].
  apply holds_cleanup_spec in E. tauto.
Qed.

(* Proof-case elimination only; no execution policy or alternative machine. *)
Ltac authority_result_cases H :=
  repeat match type of H with
  | context [if ?b then _ else _] => destruct b eqn:?
  | context [match ?v with Some _ => _ | None => _ end] => destruct v eqn:?
  end; try discriminate.

Theorem refusal_preserves_every_state_component : forall a t Ul why after,
  retire a t Ul = Refused why after -> after = a.
Proof.
  intros a t Ul why after H. unfold retire in H.
  repeat match type of H with context [if ?b then _ else _] => destruct b eqn:? end;
    inversion H; reflexivity.
Qed.

Theorem accepted_retirement_checks_whole_unit : forall a t Ul after,
  retire a t Ul = Accepted after ->
  target_current (forest a) t = true /\ target_authorized a t = true /\
  valid_unit (forest a) t Ul = true /\ unit_quiet a Ul = true.
Proof.
  intros a t Ul after H. unfold retire in H.
  destruct (target_current (forest a) t) eqn:Ec; [|discriminate].
  destruct (target_authorized a t) eqn:Ea; [|discriminate].
  destruct (valid_unit (forest a) t Ul) eqn:Eu; [|discriminate].
  destruct (unit_quiet a Ul) eqn:El; [|discriminate]. auto.
Qed.

Theorem accepted_retirement_is_canonical_step : forall a t Ul after,
  Inv (forest a) -> retire a t Ul = Accepted after ->
  match t with
  | RetireNode (n,g) => Step (forest a) (OpRelease n g Ul) (forest after)
  | RetireRoot (r,g) => Step (forest a) (OpRootDrop r g Ul) (forest after)
  end.
Proof.
  intros a t Ul after HI H.
  pose proof (accepted_retirement_checks_whole_unit _ _ _ _ H) as [Ec [_ [Eu _]]].
  unfold retire in H. rewrite Ec in H.
  destruct (target_authorized a t); [|discriminate]. rewrite Eu in H.
  destruct (unit_quiet a Ul); [|discriminate]. injection H as E; subst after.
  destruct t as [[n g]|[r g]]; cbn.
  - pose proof (target_current_node _ _ Ec) as Hr.
    destruct (node_unit_sound _ _ _ _ HI Hr Eu) as [HN HU].
    apply StRelease; [exact Hr | apply (proj1 HI), (proj2 Hr) | exact HU | exact HN].
  - destruct (root_unit_sound _ _ _ _ HI Eu) as [HN HU].
    apply StRootDrop; [apply check_root_spec; exact Ec | exact HU | exact HN].
Qed.

Theorem accepted_retirement_preserves_forest : forall a t Ul after,
  Inv (forest a) -> retire a t Ul = Accepted after -> Inv (forest after).
Proof.
  intros a t Ul after HI H. pose proof (accepted_retirement_is_canonical_step _ _ _ _ HI H) as HS.
  destruct t as [[n g]|[r g]]; eapply inv_step; eauto.
Qed.

Theorem active_descendant_loan_or_pin_blocks : forall a t Ul lease,
  In lease (active_leases a) -> In (fst (lease_target lease)) Ul ->
  forall after, retire a t Ul <> Accepted after.
Proof.
  intros a t Ul lease HL HU after H.
  destruct (accepted_retirement_checks_whole_unit _ _ _ _ H) as [_ [_ [_ HQ]]].
  unfold unit_quiet in HQ. apply forallb_forall with (x := lease) in HQ; [|exact HL].
  rewrite (proj2 (inU_spec _ _) HU) in HQ; discriminate.
Qed.

Theorem root_retirement_consumes_responsibility : forall a r g Ul after,
  retire a (RetireRoot (r,g)) Ul = Accepted after -> cleanup_rights after r = None.
Proof.
  intros a r g Ul after H. unfold retire in H.
  repeat match type of H with context [if ?b then _ else _] => destruct b eqn:? end;
    try discriminate. injection H as E; subst after.
  cbn. unfold put_right; rewrite Nat.eqb_refl; reflexivity.
Qed.

Theorem transfer_moves_not_copies : forall a r g c after,
  transfer_cleanup a (r,g) c = Accepted after ->
  cleanup_rights a r = Some (g, acting_context a) /\
  cleanup_rights after r = Some (g,c) /\
  (c <> acting_context a -> holds_cleanup after (r,g) = false) /\
  (forall q, q <> r -> cleanup_rights after q = cleanup_rights a q).
Proof.
  intros a r g c after H. unfold transfer_cleanup in H.
  destruct (target_current (forest a) (RetireRoot (r,g))); [|discriminate].
  destruct (holds_cleanup a (r,g)) eqn:EH; [|discriminate].
  apply holds_cleanup_spec in EH. injection H as E; subst after. cbn.
  split; [exact (proj2 EH) |]. unfold put_right. rewrite Nat.eqb_refl.
  split; [reflexivity |]. split.
  - intro Hne. apply copied_reference_is_not_cleanup_authority. cbn. unfold put_right.
    rewrite Nat.eqb_refl. intro E; injection E as Et; congruence.
  - intros q Hq. assert (E : Nat.eqb q r = false) by (apply Nat.eqb_neq; exact Hq).
    rewrite E; reflexivity.
Qed.

Theorem root_creation_issues_to_acting_owner : forall a r after,
  create_root a r = Accepted after ->
  cleanup_rights a r = None /\
  cleanup_rights after r = Some (root_gen (forest a) r, acting_context a) /\
  holds_cleanup after (r, root_gen (forest a) r) = true.
Proof.
  intros a r after H. unfold create_root in H.
  destruct (inU (roots (forest a)) r) eqn:Er; [discriminate |].
  destruct (cleanup_rights a r) eqn:Eright; [discriminate |].
  injection H as E; subst after. split; [reflexivity |].
  cbn. unfold put_right. rewrite Nat.eqb_refl. split; [reflexivity |].
  unfold holds_cleanup, check_root; cbn. unfold put_right; rewrite !Nat.eqb_refl; reflexivity.
Qed.

Theorem legitimate_owner_root_end_executes : forall a r g,
  Inv (forest a) -> holds_cleanup a (r,g) = true ->
  (forall lease, In lease (active_leases a) -> ~ under_root (heap (forest a)) r (fst (lease_target lease))) ->
  exists Ul after, retire a (RetireRoot (r,g)) Ul = Accepted after /\ Inv (forest after).
Proof.
  intros a r g HI HH HQ. pose proof HH as HH'. apply holds_cleanup_spec in HH'.
  destruct HH' as [[Hr Hg] _]. cbn in Hr, Hg.
  destruct (root_drop_always_succeeds _ _ HI Hr) as [Ul [HU [HS _]]].
  assert (HN : NoDup Ul) by (inversion HS; assumption).
  assert (HV : valid_unit (forest a) (RetireRoot (r,g)) Ul = true).
  { apply valid_unit_complete; [exact HI | exact I | split; assumption]. }
  assert (HL : unit_quiet a Ul = true).
  { unfold unit_quiet. apply forallb_forall. intros lease Hlease.
    apply negb_true_iff, inU_false. intro Hin. apply (HQ lease Hlease), HU; exact Hin. }
  assert (HC : target_current (forest a) (RetireRoot (r,g)) = true).
  { apply check_root_spec; split; assumption. }
  change (target_authorized a (RetireRoot (r,g)) = true) in HH.
  exists Ul, (mkAuthorityState (root_drop (forest a) r Ul) (acting_context a)
    (put_right (cleanup_rights a) r None) (active_leases a) (next_lease a)).
  assert (HE : retire a (RetireRoot (r,g)) Ul = Accepted
    (mkAuthorityState (root_drop (forest a) r Ul) (acting_context a)
      (put_right (cleanup_rights a) r None) (active_leases a) (next_lease a))).
  { unfold retire. rewrite HC, HH, HV, HL; reflexivity. }
  split; [exact HE | eapply accepted_retirement_preserves_forest; eauto].
Qed.

(* Empty protected state is the issuer entry point. No admission operation
   imports an arbitrary holder-chosen grant or raw destructive forest step. *)
Definition empty_authority (context : nat) : AuthorityState :=
  mkAuthorityState st_empty context (fun _ => None) [] 0.

Definition RightsCoherent (a : AuthorityState) : Prop := forall r,
  match cleanup_rights a r with
  | Some (g, _) => In r (roots (forest a)) /\ root_gen (forest a) r = g
  | None => ~ In r (roots (forest a))
  end.
Definition LeaseFresh (a : AuthorityState) : Prop :=
  NoDup (map lease_id (active_leases a)) /\
  forall e, In e (active_leases a) -> lease_id e < next_lease a.
Definition LeasesLive (a : AuthorityState) : Prop :=
  forall e, In e (active_leases a) -> resolves (heap (forest a)) (lease_target e).
Definition AuthorityInvariant (a : AuthorityState) : Prop :=
  Inv (forest a) /\ RightsCoherent a /\ LeaseFresh a /\ LeasesLive a.

Lemma empty_authority_invariant : forall c, AuthorityInvariant (empty_authority c).
Proof.
  intros c. split; [apply inv_empty |]. split; [intros r H; exact H |].
  split; [split; [constructor | intros e H; destruct H] | intros e H; destruct H].
Qed.

Lemma create_root_step : forall a r after, create_root a r = Accepted after ->
  Step (forest a) (OpRootNew r) (forest after).
Proof.
  intros a r after H. unfold create_root in H.
  destruct (inU (roots (forest a)) r) eqn:E; [discriminate |].
  destruct (cleanup_rights a r); [discriminate |]. injection H as Et; subst after.
  apply StRootNew, inU_false; exact E.
Qed.

Lemma allocate_owned_step : forall a i o g after, allocate_owned a i o g = Accepted after ->
  Step (forest a) (OpAlloc i o g) (forest after).
Proof.
  intros a i o g after H. unfold allocate_owned in H.
  destruct (target_current _ _) eqn:E; [|discriminate].
  destruct (target_authorized _ _); [|discriminate].
  destruct (s_node (heap (forest a) i)) eqn:Ei; [discriminate |].
  injection H as Et; subst after. apply StAlloc; [exact Ei |].
  destruct o; cbn in *.
  - apply check_root_spec; exact E.
  - apply target_current_node; exact E.
Qed.

Lemma resolves_outside_teardown : forall s Ul l, resolves (heap s) l ->
  ~ In (fst l) Ul -> resolves (teardown_heap (heap s) (rix s) Ul) l.
Proof.
  intros s Ul [x g] [Hg [nd Hnd]] Hout.
  change (s_gen (heap s x) = g) in Hg. change (~ In x Ul) in Hout.
  change (s_gen (teardown_heap (heap s) (rix s) Ul x) = g /\
    live (teardown_heap (heap s) (rix s) Ul) x).
  pose proof (proj2 (inU_false Ul x) Hout) as EB.
  pose proof (teardown_out_node (heap s) (rix s) Ul x nd EB Hnd) as Hnode.
  split.
  - rewrite Hnode; cbn; exact Hg.
  - eexists. rewrite Hnode; reflexivity.
Qed.

Theorem create_root_preserves_authority : forall a r after,
  AuthorityInvariant a -> create_root a r = Accepted after -> AuthorityInvariant after.
Proof.
  intros a r after [HI [HC [HF HL]]] H.
  pose proof (create_root_step _ _ _ H) as HS.
  split; [eapply inv_step; eauto |].
  unfold create_root in H. destruct (inU _ _) eqn:E; [discriminate |].
  destruct (cleanup_rights a r) eqn:Er; [discriminate |]. injection H as Et; subst after.
  split.
  - intros q. cbn. unfold put_right. destruct (Nat.eqb q r) eqn:Eq.
    + apply Nat.eqb_eq in Eq; subst q. auto.
    + specialize (HC q). destruct (cleanup_rights a q) as [[g c]|]; cbn in *.
      * tauto.
      * intros [Eqr|Hin]; [apply Nat.eqb_neq in Eq; congruence | contradiction].
  - split; assumption.
Qed.

Theorem transfer_preserves_authority : forall a l c after,
  AuthorityInvariant a -> transfer_cleanup a l c = Accepted after -> AuthorityInvariant after.
Proof.
  intros a [r g] c after [HI [HC [HF HL]]] H. unfold transfer_cleanup in H.
  destruct (target_current _ _); [|discriminate].
  destruct (holds_cleanup a (r,g)) eqn:EH; [|discriminate].
  pose proof (proj1 (holds_cleanup_spec _ _ _) EH) as [HR _].
  injection H as Et; subst after. split; [exact HI |]. split.
  - intros q. cbn. unfold put_right. destruct (Nat.eqb q r) eqn:Eq.
    + apply Nat.eqb_eq in Eq; subst q; exact HR.
    + exact (HC q).
  - split; assumption.
Qed.

Theorem begin_lease_preserves_authority : forall a l k borrower after,
  AuthorityInvariant a -> begin_lease a l k borrower = Accepted after -> AuthorityInvariant after.
Proof.
  intros a l k borrower after [HI [HC [[HN HF] HL]]] H. unfold begin_lease in H.
  destruct (target_current _ _) eqn:E; [|discriminate].
  authority_result_cases H; injection H as Et; subst after.
  split; [exact HI |]. split; [exact HC |]. split.
  - split.
    + cbn. constructor; [|exact HN]. intro Hin. apply in_map_iff in Hin.
      destruct Hin as [e [Eq He]]. specialize (HF e He); lia.
    + intros e [Eq|Hin].
      * subst e; cbn; lia.
      * specialize (HF e Hin); cbn; lia.
  - intros e [Eq|Hin].
    + subst e; cbn. apply target_current_node; exact E.
    + exact (HL e Hin).
Qed.

Lemma lease_id_filter_unique : forall leases id, NoDup (map lease_id leases) ->
  NoDup (map lease_id (filter (fun e => negb (Nat.eqb (lease_id e) id)) leases)).
Proof.
  intros leases id H. induction leases as [|e es IH]; cbn in *; [constructor |].
  inversion H as [|n ns Hnot Hnd]; subst.
  destruct (negb (Nat.eqb (lease_id e) id)); cbn.
  - constructor; [|apply IH; exact Hnd]. intro Hin; apply Hnot.
    apply in_map_iff in Hin. destruct Hin as [e' [Eq He]].
    apply filter_In in He. apply in_map_iff. exists e'; tauto.
  - apply IH; exact Hnd.
Qed.

Theorem end_lease_preserves_authority : forall a id after,
  AuthorityInvariant a -> end_lease a id = Accepted after -> AuthorityInvariant after.
Proof.
  intros a id after [HI [HC [[HN HF] HL]]] H. unfold end_lease in H.
  destruct (find _ _) as [e|]; [|discriminate].
  destruct (Nat.eqb _ _); [|discriminate]. injection H as Et; subst after.
  split; [exact HI |]. split; [exact HC |]. split.
  - split; [apply lease_id_filter_unique; exact HN |].
    intros e' Hin. apply filter_In in Hin. exact (HF e' (proj1 Hin)).
  - intros e' Hin. apply filter_In in Hin. exact (HL e' (proj1 Hin)).
Qed.

Theorem allocation_preserves_authority : forall a i o g after,
  AuthorityInvariant a -> allocate_owned a i o g = Accepted after -> AuthorityInvariant after.
Proof.
  intros a i o g after [HI [HC [HF HL]]] H.
  pose proof (allocate_owned_step _ _ _ _ _ H) as HS.
  split; [eapply inv_step; eauto |]. unfold allocate_owned in H.
  destruct (target_current _ _); [|discriminate].
  destruct (target_authorized _ _); [|discriminate].
  destruct (s_node (heap (forest a) i)) eqn:Ei; [discriminate |].
  injection H as Et; subst after. split; [exact HC |]. split; [exact HF |].
  intros e Hin. specialize (HL e Hin). unfold resolves in HL.
  destruct (lease_target e) as [x gx] eqn:E; cbn [fst snd] in HL.
  assert (Hne : x <> i).
  { intro Eq; subst x. destruct HL as [_ [nd Hnd]]; congruence. }
  change (s_gen (upd (heap (forest a)) i
    (mkSlot (s_gen (heap (forest a) i)) (Some (mkNode o empty_fields))) x) = gx /\
    live (upd (heap (forest a)) i
    (mkSlot (s_gen (heap (forest a) i)) (Some (mkNode o empty_fields)))) x).
  unfold upd. assert (Eb : Nat.eqb x i = false) by (apply Nat.eqb_neq; exact Hne).
  split; [rewrite Eb; exact (proj1 HL) |].
  destruct HL as [_ [nd Hnd]]. exists nd; cbn; rewrite Eb; exact Hnd.
Qed.

Theorem retirement_preserves_authority : forall a t Ul after,
  AuthorityInvariant a -> retire a t Ul = Accepted after -> AuthorityInvariant after.
Proof.
  intros a t Ul after [HI [HC [HF HL]]] H.
  split; [eapply accepted_retirement_preserves_forest; eauto |].
  pose proof (accepted_retirement_checks_whole_unit _ _ _ _ H) as [EC [EA [EU EQ]]].
  unfold retire in H. rewrite EC, EA, EU, EQ in H. injection H as Et; subst after.
  split.
  - destruct t as [[n g]|[r g]]; [exact HC |].
    intros q. cbn. unfold put_right. destruct (Nat.eqb q r) eqn:Eqr.
    + apply Nat.eqb_eq in Eqr; subst q. apply remove_In.
    + apply Nat.eqb_neq in Eqr. specialize (HC q).
      destruct (cleanup_rights a q) as [[epoch c]|].
      * destruct HC as [Hr Hg]. split; [apply in_in_remove; assumption | exact Hg].
      * intro Hin. apply HC. exact (proj1 (in_remove _ _ _ _ Hin)).
  - split; [exact HF |]. intros e Hin.
    assert (Hout : ~ In (fst (lease_target e)) Ul).
    { unfold unit_quiet in EQ. apply forallb_forall with (x := e) in EQ; [|exact Hin].
      apply negb_true_iff, inU_false in EQ; exact EQ. }
    destruct t as [[n g]|[r g]]; cbn; eapply resolves_outside_teardown; eauto.
Qed.

Inductive AuthorityOp := CreateRoot (r : nat) | AllocateOwned (i : nat) (o : Owner) (g : nat)
  | Retire (t : RetirementTarget) (Ul : list nat) | TransferCleanup (l : Link) (to_context : nat)
  | BeginLease (l : Link) (kind : LeaseKind) (borrower : nat) | EndLease (id : nat).
Definition execute (a : AuthorityState) (op : AuthorityOp) : AuthorityResult :=
  match op with
  | CreateRoot r => create_root a r | AllocateOwned i o g => allocate_owned a i o g
  | Retire t Ul => retire a t Ul | TransferCleanup l c => transfer_cleanup a l c
  | BeginLease l k borrower => begin_lease a l k borrower | EndLease id => end_lease a id
  end.

(* Scheduling chooses an already identified trusted execution context. There
   is deliberately no caller-selection operation in AuthorityOp. Context-id
   issuance/reuse and native frame authenticity remain refinement obligations. *)
Definition in_context (a : AuthorityState) (c : nat) : AuthorityState :=
  mkAuthorityState (forest a) c (cleanup_rights a) (active_leases a) (next_lease a).
Inductive AuthorityRun : AuthorityState -> AuthorityState -> Prop :=
| ARRefl : forall a, AuthorityRun a a
| ARExecute : forall a op b c, execute a op = Accepted b -> AuthorityRun b c -> AuthorityRun a c
| ARContext : forall a context b, AuthorityRun (in_context a context) b -> AuthorityRun a b.

Theorem execute_preserves_authority : forall a op after,
  AuthorityInvariant a -> execute a op = Accepted after -> AuthorityInvariant after.
Proof.
  intros a op after HI H. destruct op; cbn in H.
  - eapply create_root_preserves_authority; eauto.
  - eapply allocation_preserves_authority; eauto.
  - eapply retirement_preserves_authority; eauto.
  - eapply transfer_preserves_authority; eauto.
  - eapply begin_lease_preserves_authority; eauto.
  - eapply end_lease_preserves_authority; eauto.
Qed.

Theorem admitted_runs_preserve_authority : forall a b,
  AuthorityInvariant a -> AuthorityRun a b -> AuthorityInvariant b.
Proof.
  intros a b HI HR. induction HR as [a|a op b c HE HR IH|a ctx b HR IH]; [exact HI | |].
  - apply IH. eapply execute_preserves_authority; eauto.
  - apply IH; exact HI.
Qed.

Theorem coherent_rights_and_fresh_leases_reachable : forall c after,
  AuthorityRun (empty_authority c) after -> RightsCoherent after /\ LeaseFresh after.
Proof.
  intros c after HR.
  destruct (admitted_runs_preserve_authority _ _ (empty_authority_invariant c) HR)
    as [_ [HC [HF _]]]. auto.
Qed.

Theorem other_context_cannot_end_lease : forall a id e,
  find (fun l => Nat.eqb (lease_id l) id) (active_leases a) = Some e ->
  lease_holder e <> acting_context a -> end_lease a id = Refused WrongLeaseHolder a.
Proof.
  intros a id e HF Hne. unfold end_lease. rewrite HF.
  assert (E : Nat.eqb (lease_holder e) (acting_context a) = false) by (apply Nat.eqb_neq; exact Hne).
  rewrite E; reflexivity.
Qed.

Theorem ended_lease_cannot_end_again : forall a id after,
  end_lease a id = Accepted after -> end_lease after id = Refused MissingLease after.
Proof.
  intros a id after H. unfold end_lease in H.
  destruct (find _ _) as [e|]; [|discriminate].
  destruct (Nat.eqb _ _); [|discriminate]. injection H as Et; subst after.
  unfold end_lease; cbn.
  assert (HN : find (fun l => Nat.eqb (lease_id l) id)
      (filter (fun e => negb (Nat.eqb (lease_id e) id)) (active_leases a)) = None).
  { induction (active_leases a) as [|l ls IH]; cbn; [reflexivity |].
    destruct (Nat.eqb (lease_id l) id) eqn:E; cbn; [exact IH |].
    rewrite E; exact IH. }
  rewrite HN; reflexivity.
Qed.

Lemma forest_steps_transitive : forall a b c, Steps a b -> Steps b c -> Steps a c.
Proof.
  intros a b c HAB HBC. induction HAB; [exact HBC |]. eapply StepsCons; eauto.
Qed.

Theorem execute_projects_to_forest : forall a op b,
  Inv (forest a) -> execute a op = Accepted b -> Steps (forest a) (forest b).
Proof.
  intros a op b HI HE. destruct op; cbn in HE.
  - eapply StepsCons; [eapply create_root_step; exact HE |constructor].
  - eapply StepsCons; [eapply allocate_owned_step; exact HE |constructor].
  - pose proof (accepted_retirement_is_canonical_step _ _ _ _ HI HE) as HS.
    destruct t as [[n g]|[r g]].
    + eapply StepsCons; [exact HS |constructor].
    + eapply StepsCons; [exact HS |constructor].
  - unfold transfer_cleanup in HE.
    destruct (target_current _ _); [|discriminate].
    destruct (holds_cleanup _ _); [|discriminate]. injection HE as E; subst b; constructor.
  - unfold begin_lease in HE. destruct (target_current _ _); [|discriminate].
    authority_result_cases HE; injection HE as E; subst b; constructor.
  - unfold end_lease in HE. destruct (find _ _); [|discriminate].
    destruct (Nat.eqb _ _); [|discriminate]. injection HE as E; subst b; constructor.
Qed.

Theorem admitted_runs_project_to_forest : forall a b,
  AuthorityInvariant a -> AuthorityRun a b -> Steps (forest a) (forest b).
Proof.
  intros a b HI HR. revert HI. induction HR as [a|a op b c HE HR IH|a ctx b HR IH]; intro HI.
  - constructor.
  - eapply forest_steps_transitive.
    + eapply execute_projects_to_forest; [exact (proj1 HI) |exact HE].
    + apply IH. eapply execute_preserves_authority; eauto.
  - apply IH; exact HI.
Qed.

Theorem consumed_root_never_readmitted : forall a r g Ul b c,
  AuthorityInvariant a -> retire a (RetireRoot (r,g)) Ul = Accepted b ->
  AuthorityRun b c -> forall unit,
  retire c (RetireRoot (r,g)) unit = Refused StaleIdentity c.
Proof.
  intros a r g Ul b c HI HE HR unit.
  pose proof (accepted_retirement_is_canonical_step _ _ _ _ (proj1 HI) HE) as HS.
  pose proof (retirement_preserves_authority _ _ _ _ HI HE) as HB.
  pose proof (admitted_runs_project_to_forest _ _ HB HR) as HF.
  pose proof (dropped_root_check_false_forever _ _ _ _ _ _ HS HF) as EC.
  unfold retire. change (target_current (forest c) (RetireRoot (r,g)) = false) in EC.
  rewrite EC; reflexivity.
Qed.

Theorem consumed_node_never_readmitted : forall a n g Ul b c,
  AuthorityInvariant a -> retire a (RetireNode (n,g)) Ul = Accepted b ->
  AuthorityRun b c -> forall unit,
  retire c (RetireNode (n,g)) unit = Refused StaleIdentity c.
Proof.
  intros a n g Ul b c HI HE HR unit.
  pose proof (accepted_retirement_is_canonical_step _ _ _ _ (proj1 HI) HE) as HS.
  pose proof (retirement_preserves_authority _ _ _ _ HI HE) as HB.
  pose proof (admitted_runs_project_to_forest _ _ HB HR) as HF.
  pose proof (accepted_retirement_checks_whole_unit _ _ _ _ HE) as [EC [_ [EU _]]].
  pose proof (target_current_node _ _ EC) as Hres.
  destruct (node_unit_sound _ _ _ _ (proj1 HI) Hres EU) as [_ HU].
  assert (HN : In n Ul) by (apply HU; constructor).
  pose proof (released_unit_local_read_none_forever _ _ _ _ _ _ _ _ HS Hres HN HF) as ED.
  unfold retire, target_current. rewrite ED; reflexivity.
Qed.

Theorem lease_issuance_requires_owner_approval : forall a l k borrower after,
  begin_lease a l k borrower = Accepted after ->
  target_authorized a (RetireNode l) = true /\
  active_leases after = mkLease (next_lease a) borrower l k :: active_leases a /\
  next_lease after = S (next_lease a).
Proof.
  intros a l k borrower after H. unfold begin_lease in H.
  destruct (target_current _ _); [|discriminate].
  authority_result_cases H; injection H as E; subst after.
  split; [first [reflexivity |assumption] |split; reflexivity].
Qed.

Theorem execute_refusal_preserves_state : forall a op why after,
  execute a op = Refused why after -> after = a.
Proof.
  intros a op why after H. destruct op; cbn in H;
    unfold create_root, allocate_owned, retire, transfer_cleanup, begin_lease, end_lease in H;
    authority_result_cases H; inversion H; reflexivity.
Qed.

Theorem execute_next_lease_monotone : forall a op after,
  execute a op = Accepted after -> next_lease a <= next_lease after.
Proof.
  intros a op after H. destruct op; cbn in H;
    unfold create_root, allocate_owned, retire, transfer_cleanup, begin_lease, end_lease in H;
    authority_result_cases H; injection H as E; subst after; cbn; lia.
Qed.

Theorem next_lease_monotone_runs : forall a b, AuthorityRun a b -> next_lease a <= next_lease b.
Proof.
  intros a b H. induction H as [a|a op b c HE HR IH|a ctx b HR IH]; cbn in *; try lia.
  pose proof (execute_next_lease_monotone _ _ _ HE); lia.
Qed.

Lemma execute_preserves_absent_older_lease : forall a op b id,
  id < next_lease a ->
  (forall e, In e (active_leases a) -> lease_id e <> id) ->
  execute a op = Accepted b -> forall e, In e (active_leases b) -> lease_id e <> id.
Proof.
  intros a op b id Hbound Habs HE. destruct op; cbn in HE;
    unfold create_root, allocate_owned, retire, transfer_cleanup, begin_lease, end_lease in HE;
    authority_result_cases HE; injection HE as E; subst b; cbn; try exact Habs.
  - intros e [Eq|Hin]; [subst e; cbn; lia |apply Habs; exact Hin].
  - intros e Hin. apply filter_In in Hin. apply Habs; exact (proj1 Hin).
Qed.

Lemma absent_older_lease_preserved_runs : forall a b id,
  id < next_lease a -> (forall e, In e (active_leases a) -> lease_id e <> id) ->
  AuthorityRun a b -> forall e, In e (active_leases b) -> lease_id e <> id.
Proof.
  intros a b id Hbound Habs HR. revert Hbound Habs.
  induction HR as [a|a op b c HE HR IH|a ctx b HR IH]; intros Hbound Habs.
  - exact Habs.
  - apply IH.
    + pose proof (execute_next_lease_monotone _ _ _ HE); lia.
    + eapply execute_preserves_absent_older_lease; eauto.
  - apply IH; assumption.
Qed.

Theorem ended_lease_identity_never_reissued : forall a id b c,
  AuthorityInvariant a -> end_lease a id = Accepted b -> AuthorityRun b c ->
  id < next_lease c /\
  ~ In id (map lease_id (active_leases c)) /\ end_lease c id = Refused MissingLease c.
Proof.
  intros a id b c [_ [_ [[_ HF] _]]] HE HR.
  unfold end_lease in HE. destruct (find _ _) as [e|] eqn:Ef; [|discriminate].
  pose proof Ef as Efind. apply find_some in Efind. destruct Efind as [Hin Eid].
  apply Nat.eqb_eq in Eid. specialize (HF e Hin).
  destruct (Nat.eqb _ _); [|discriminate]. injection HE as E; subst b.
  pose proof (next_lease_monotone_runs _ _ HR) as HM; cbn in HM.
  assert (Habs : forall e', In e' (active_leases c) -> lease_id e' <> id).
  { eapply absent_older_lease_preserved_runs; [| |exact HR].
    - cbn; lia.
    - intros e' Hmember. apply filter_In in Hmember. destruct Hmember as [_ Hkeep].
      apply negb_true_iff, Nat.eqb_neq in Hkeep; exact Hkeep. }
  split; [lia |]. split.
  - intro Hmember. apply in_map_iff in Hmember. destruct Hmember as [e' [Eq He']].
    apply (Habs e' He'); exact Eq.
  - unfold end_lease. assert (Hfind : find (fun l => Nat.eqb (lease_id l) id) (active_leases c) = None).
    { destruct (find (fun l => Nat.eqb (lease_id l) id) (active_leases c)) as [e'|] eqn:Ef'; [|reflexivity].
      apply find_some in Ef'. destruct Ef' as [Hmember Eq]. apply Nat.eqb_eq in Eq.
      exfalso; apply (Habs e' Hmember); exact Eq. }
    rewrite Hfind; reflexivity.
Qed.

Theorem execute_right_origin : forall a op b r g c,
  execute a op = Accepted b -> cleanup_rights b r = Some (g,c) ->
  cleanup_rights a r = Some (g,c) \/
  (op = CreateRoot r /\ g = root_gen (forest a) r /\ c = acting_context a) \/
  (exists old_epoch, op = TransferCleanup (r,old_epoch) c /\
    holds_cleanup a (r,old_epoch) = true /\ g = old_epoch).
Proof.
  intros a op b r g c HE HR. destruct op as [q|i o gp|t Ul|[q epoch] to|l k borrower|id]; cbn in HE.
  - unfold create_root in HE. authority_result_cases HE; injection HE as E; subst b.
    cbn in HR. unfold put_right in HR. destruct (Nat.eqb r q) eqn:Eq.
    + apply Nat.eqb_eq in Eq; subst q. injection HR as Eg Ec; subst g c. right; left; auto.
    + left; exact HR.
  - unfold allocate_owned in HE. authority_result_cases HE; injection HE as E; subst b. left; exact HR.
  - unfold retire in HE. authority_result_cases HE; injection HE as E; subst b.
    destruct t as [[n gn]|[q epoch]]; [left; exact HR |].
    cbn in HR. unfold put_right in HR. destruct (Nat.eqb r q); [discriminate |left; exact HR].
  - unfold transfer_cleanup in HE.
    destruct (target_current _ _); [|discriminate].
    destruct (holds_cleanup a (q,epoch)) eqn:EH; [|discriminate]. injection HE as E; subst b.
    cbn in HR. unfold put_right in HR. destruct (Nat.eqb r q) eqn:Eq.
    + apply Nat.eqb_eq in Eq; subst q. injection HR as Eg Ec; subst g c.
      right; right; exists epoch; auto.
    + left; exact HR.
  - unfold begin_lease in HE. authority_result_cases HE; injection HE as E; subst b. left; exact HR.
  - unfold end_lease in HE. authority_result_cases HE; injection HE as E; subst b. left; exact HR.
Qed.

(* Finite owner paths have no repeated node: the height of a node is unique
   because its owner is unique. This closes the bounded parent walk's liveness
   obligation without assuming a larger fuel budget or an acyclic path. *)
Inductive OwnerHeight (h : Heap) : nat -> nat -> Prop :=
| OHRoot : forall x nd r, s_node (h x) = Some nd -> owner nd = ORoot r -> OwnerHeight h x 1
| OHParent : forall x nd p d, s_node (h x) = Some nd -> owner nd = OParent p ->
    OwnerHeight h p d -> OwnerHeight h x (S d).

Lemma owner_height_unique : forall h x d e, OwnerHeight h x d -> OwnerHeight h x e -> d = e.
Proof.
  intros h x d e HD. revert e.
  induction HD as [x nd r Hx Ho|x nd p d Hx Ho HP IH]; intros e HE;
    inversion HE as [y nd' r' Hy Ho'|y nd' p' d' Hy Ho' HP']; subst;
    rewrite Hx in Hy; injection Hy as E; subst nd'; try reflexivity;
    try (rewrite Ho in Ho'; discriminate).
  rewrite Ho in Ho'. injection Ho' as Ep; subst p'.
  f_equal; apply IH; exact HP'.
Qed.

Lemma owner_root_path : forall h r x, under_root h r x -> exists path,
  NoDup path /\ node_root (length path) h x = Some r /\
  OwnerHeight h x (length path) /\
  (forall y, In y path -> live h y /\ exists d, OwnerHeight h y d /\ d <= length path).
Proof.
  intros h r x HR. induction HR as [x nd Hx Ho|x nd p Hx Ho HP IH].
  - exists [x]. split; [constructor; [intros H; destruct H |constructor] |].
    split; [cbn; rewrite Hx, Ho; reflexivity |].
    split; [eapply OHRoot; eauto |]. intros y [Eq|[]]; subst y.
    split; [exists nd; exact Hx |exists 1; split; [eapply OHRoot; eauto |cbn; lia]].
  - destruct IH as [path [HN [HW [HH HM]]]]. exists (x::path).
    assert (HXH : OwnerHeight h x (S (length path))) by (eapply OHParent; eauto).
    split.
    + constructor; [|exact HN]. intro Hin.
      destruct (HM x Hin) as [_ [d [HD Hle]]].
      pose proof (owner_height_unique _ _ _ _ HXH HD); lia.
    + split; [cbn; rewrite Hx, Ho; exact HW |]. split; [exact HXH |].
      intros y [Eq|Hin].
      * subst y. split; [exists nd; exact Hx |exists (S (length path)); cbn; auto].
      * destruct (HM y Hin) as [HL [d [HD Hle]]]. split; [exact HL |exists d; cbn; auto; lia].
Qed.

Lemma node_root_more_fuel : forall fuel h x r, node_root fuel h x = Some r ->
  forall more, fuel <= more -> node_root more h x = Some r.
Proof.
  induction fuel as [|f IH]; intros h x r H more Hle; [discriminate |].
  destruct more as [|m]; [lia |]. cbn in H |- *.
  destruct (s_node (h x)) as [nd|] eqn:E; [|discriminate].
  destruct (owner nd); [exact H |]. eapply IH; [exact H |lia].
Qed.

Theorem node_root_complete : forall s r x, Inv s -> under_root (heap s) r x ->
  node_root (bound s) (heap s) x = Some r.
Proof.
  intros s r x [_ [_ [_ [_ [_ HB]]]]] HR.
  destruct (owner_root_path _ _ _ HR) as [path [HN [HW [_ HM]]]].
  assert (Hlen : length path <= length (seq 0 (bound s))).
  { eapply NoDup_incl_length; [exact HN |]. intros y Hin.
    apply in_seq. destruct (HM y Hin) as [HL _]. specialize (HB y HL); lia. }
  rewrite length_seq in Hlen. eapply node_root_more_fuel; eauto.
Qed.

Theorem legitimate_owner_node_release_executes : forall a n g r,
  Inv (forest a) -> resolves (heap (forest a)) (n,g) ->
  under_root (heap (forest a)) r n -> holds_cleanup a (r, root_gen (forest a) r) = true ->
  (forall e, In e (active_leases a) -> ~ in_sub (heap (forest a)) n (fst (lease_target e))) ->
  exists Ul after, retire a (RetireNode (n,g)) Ul = Accepted after /\ Inv (forest after).
Proof.
  intros a n g r HI Hres Hroot Hright Hquiet.
  destruct (release_always_succeeds _ _ _ HI Hres) as [Ul [HU [HS _]]].
  assert (HN : NoDup Ul) by (inversion HS; assumption).
  assert (HV : valid_unit (forest a) (RetireNode (n,g)) Ul = true).
  { apply valid_unit_complete; [exact HI |exact Hres |split; assumption]. }
  assert (HC : target_current (forest a) (RetireNode (n,g)) = true).
  { destruct Hres as [Hg [nd Hnd]]. unfold target_current.
    rewrite (proj2 (resolve_node_some _ _ _) (conj Hg Hnd)); reflexivity. }
  assert (HA : target_authorized a (RetireNode (n,g)) = true).
  { unfold target_authorized; cbn [fst]. rewrite (node_root_complete _ _ _ HI Hroot); exact Hright. }
  assert (HL : unit_quiet a Ul = true).
  { unfold unit_quiet; apply forallb_forall; intros e Hin.
    apply negb_true_iff, inU_false. intro Hunit. apply (Hquiet e Hin), HU; exact Hunit. }
  exists Ul, (mkAuthorityState (teardown (forest a) Ul) (acting_context a)
    (cleanup_rights a) (active_leases a) (next_lease a)).
  assert (HE : retire a (RetireNode (n,g)) Ul = Accepted
    (mkAuthorityState (teardown (forest a) Ul) (acting_context a)
      (cleanup_rights a) (active_leases a) (next_lease a))).
  { unfold retire. rewrite HC, HA, HV, HL; reflexivity. }
  split; [exact HE |eapply accepted_retirement_preserves_forest; eauto].
Qed.
