(* Bounded refusal explanations, not a proof of the production EV producer.
   Allocation identities and retirement reuse OwnershipCleanCore. The admitted
   graph is finite, fixed, and contains unary Horn causes of may-sharing.
   Guard keys denote adverse summary facts (including unavailable facts).
   Rounds are synchronous; the production in-place schedule is NOT identified
   with these rounds. Concrete traces below are executions of this small model,
   not certificates of branch feasibility in an arbitrary Pergyra body. *)
From Stdlib Require Import Lists.List Bool.Bool Arith.PeanoNat micromega.Lia.
Require Import OwnershipCleanCore.
Import ListNotations.

Module RefusalExplanation.

Definition Summary := nat -> bool.
Definition enabled (s : Summary) (key : option nat) : bool :=
  match key with None => true | Some k => s k end.
Definition keys (key : option nat) : list nat :=
  match key with None => [] | Some k => [k] end.

Inductive SeedKind := ConstructorOverlap | UnconsumedAlias | RefOrigin
  | IndexOrigin | SharedResultField | UnavailableAnalysisFact.
Inductive TransferKind := Assignment | ArgumentToResult | FieldAccess.
Record Seed := seed {
  seed_kind : SeedKind; seed_at : nat; seed_guard : option nat
}.
Record Edge := edge {
  edge_kind : TransferKind; edge_from : nat; edge_to : nat;
  edge_guard : option nat
}.
Definition seed_eq_dec : forall a b : Seed, {a = b} + {a <> b}.
Proof. decide equality; decide equality; apply Nat.eq_dec. Defined.
Definition edge_eq_dec : forall a b : Edge, {a = b} + {a <> b}.
Proof. decide equality; decide equality; apply Nat.eq_dec. Defined.
Definition member {A} (eqd : forall a b : A, {a = b} + {a <> b})
    (x : A) (xs : list A) : bool := if in_dec eqd x xs then true else false.
Lemma member_spec : forall A eqd (x : A) xs,
  member eqd x xs = true <-> In x xs.
Proof. intros. unfold member. destruct (in_dec eqd x xs); intuition congruence. Qed.

(* A trace records actual relocation, not a may-relocate effect. Allocation
   identities are never reissued, so a stale descriptor cannot revive by ABA. *)
Inductive Op := Allocate (register : nat) (buffer : Block)
  | CopyDescriptor (destination source : nat)
  | Mutate (operation register : nat) (fresh : Block)
  | ReadDescriptor (register : nat).
Record HeapState := heap_state {
  registers : list (nat * Block); live : list Block; issued : list Block
}.
Definition empty_heap := heap_state [] [] [].
Fixpoint register_at (r : nat) (rs : list (nat * Block)) : option Block :=
  match rs with
  | [] => None
  | (x,b) :: rest => if Nat.eqb r x then Some b else register_at r rest
  end.
Inductive Outcome := Finished | InvalidTrace | StaleRead (reader : nat) (buffer : Block).
Inductive Transition := Continue (h : HeapState) | Stop (outcome : Outcome).
Definition execute (moves : nat -> bool) (h : HeapState) (op : Op) : Transition :=
  match op with
  | Allocate r b =>
      if bmem b (issued h) then Stop InvalidTrace else
      Continue (heap_state ((r,b)::registers h) (b::live h) (b::issued h))
  | CopyDescriptor dst src =>
      match register_at src (registers h) with
      | None => Stop InvalidTrace
      | Some b => if bmem b (live h) then
          Continue (heap_state ((dst,b)::registers h) (live h) (issued h))
          else Stop InvalidTrace
      end
  | Mutate op r fresh =>
      match register_at r (registers h) with
      | None => Stop InvalidTrace
      | Some old => if bmem old (live h) then
          if moves op then
            if bmem fresh (issued h) then Stop InvalidTrace else
            Continue (heap_state ((r,fresh)::registers h)
              (fresh :: free [old] (live h)) (fresh :: issued h))
          else Continue h
          else Stop InvalidTrace
      end
  | ReadDescriptor r =>
      match register_at r (registers h) with
      | None => Stop InvalidTrace
      | Some b => if bmem b (live h) then Continue h else Stop (StaleRead r b)
      end
  end.
Fixpoint run (moves : nat -> bool) (h : HeapState) (ops : list Op) : Outcome :=
  match ops with
  | [] => Finished
  | op :: rest => match execute moves h op with
      | Continue next => run moves next rest | Stop result => result end
  end.
Definition stale_at (reader : nat) (outcome : Outcome) : bool :=
  match outcome with StaleRead r _ => Nat.eqb reader r | _ => false end.
Lemma stale_spec : forall reader o,
  stale_at reader o = true <-> exists b, o = StaleRead reader b.
Proof.
  intros reader o. destruct o as [| |r b]; simpl.
  - split; [discriminate|intros [? H]; discriminate].
  - split; [discriminate|intros [? H]; discriminate].
  - rewrite Nat.eqb_eq. split; intros H.
    + subst. eauto.
    + destruct H as [? H]. inversion H. reflexivity.
Qed.

Theorem copied_descriptor_growth_stale : forall moves op old fresh,
  moves op = true -> old <> fresh ->
  run moves empty_heap
    [Allocate 0 old; CopyDescriptor 1 0; Mutate op 1 fresh; ReadDescriptor 0]
    = StaleRead 0 old.
Proof.
  intros moves op old fresh HM Hneq.
  assert (EE : beqb old old = true) by (apply beqb_true; reflexivity).
  assert (EF : beqb old fresh = false).
  { destruct (beqb old fresh) eqn:E; auto. apply beqb_true in E. contradiction. }
  assert (FE : beqb fresh old = false).
  { destruct (beqb fresh old) eqn:E; auto. apply beqb_true in E. congruence. }
  repeat progress (cbn [run empty_heap execute registers live issued
    register_at bmem free existsb filter Nat.eqb orb negb]; rewrite ?EE, ?HM, ?FE, ?EF).
  reflexivity.
Qed.

Record Boundary := boundary {
  boundary_site : nat; boundary_target : nat; boundary_effect : option nat;
  boundary_trace : list Op
}.
Record Program := program {
  seeds : list Seed; edges : list Edge; boundaries : list Boundary;
  relocates : nat -> bool
}.
Definition universe (p : Program) : list nat :=
  nodup Nat.eq_dec (map seed_at (seeds p) ++
    flat_map (fun e => [edge_from e; edge_to e]) (edges p)).
Lemma universe_nodup : forall p, NoDup (universe p).
Proof. intros. apply NoDup_nodup. Qed.
Lemma seed_in_universe : forall p r, In r (seeds p) -> In (seed_at r) (universe p).
Proof. intros. unfold universe. apply nodup_In, in_or_app. left. apply in_map. assumption. Qed.
Lemma edge_in_universe : forall p e, In e (edges p) ->
  In (edge_from e) (universe p) /\ In (edge_to e) (universe p).
Proof.
  intros. split; unfold universe; apply nodup_In, in_or_app; right;
    apply in_flat_map; exists e; split; auto; simpl; auto.
Qed.

Inductive Shared (p : Program) (s : Summary) : nat -> Prop :=
| SharedSeed : forall r, In r (seeds p) -> enabled s (seed_guard r) = true ->
    Shared p s (seed_at r)
| SharedTransfer : forall e, In e (edges p) -> enabled s (edge_guard e) = true ->
    Shared p s (edge_from e) -> Shared p s (edge_to e).

Inductive Derivation := BecauseSeed (r : Seed) | BecauseTransfer (e : Edge) (parent : Derivation).
Definition target (d : Derivation) : nat :=
  match d with BecauseSeed r => seed_at r | BecauseTransfer e _ => edge_to e end.
Fixpoint height (d : Derivation) : nat :=
  match d with BecauseSeed _ => 1 | BecauseTransfer _ parent => S (height parent) end.
Inductive ValidTree (p : Program) (s : Summary) : Derivation -> Prop :=
| ValidSeed : forall r, In r (seeds p) -> enabled s (seed_guard r) = true ->
    ValidTree p s (BecauseSeed r)
| ValidTransfer : forall e parent, In e (edges p) -> enabled s (edge_guard e) = true ->
    target parent = edge_from e -> ValidTree p s parent ->
    ValidTree p s (BecauseTransfer e parent).
Fixpoint check_tree (p : Program) (s : Summary) (d : Derivation) : bool :=
  match d with
  | BecauseSeed r => member seed_eq_dec r (seeds p) && enabled s (seed_guard r)
  | BecauseTransfer e parent => member edge_eq_dec e (edges p) &&
      enabled s (edge_guard e) && Nat.eqb (target parent) (edge_from e) &&
      check_tree p s parent
  end.
Theorem check_tree_spec : forall p s d, check_tree p s d = true <-> ValidTree p s d.
Proof.
  intros p s d. induction d; simpl; rewrite !andb_true_iff, member_spec.
  - split; intros H; [destruct H; constructor; assumption|inversion H; auto].
  - rewrite Nat.eqb_eq, IHd. split.
    + intros [[[H1 H2] H3] H4]. constructor; assumption.
    + intros H. inversion H; subst. auto.
Qed.
Theorem shared_has_derivation : forall p s x,
  Shared p s x <-> exists d, ValidTree p s d /\ target d = x.
Proof.
  intros p s x. split.
  - intros H. induction H.
    + exists (BecauseSeed r). split; [constructor; assumption|reflexivity].
    + destruct IHShared as [d [HD HT]]. exists (BecauseTransfer e d).
      split; [constructor; assumption|reflexivity].
  - intros [d [HD HT]]. subst. induction HD; simpl.
    + constructor; assumption.
    + eapply SharedTransfer; eauto. congruence.
Qed.

Definition seed_enabled (p : Program) (s : Summary) (x : nat) : bool :=
  existsb (fun r => Nat.eqb x (seed_at r) && enabled s (seed_guard r)) (seeds p).
Definition edge_enabled (p : Program) (s : Summary) (a : list nat) (x : nat) : bool :=
  existsb (fun e => Nat.eqb x (edge_to e) && enabled s (edge_guard e) &&
    member Nat.eq_dec (edge_from e) a) (edges p).
Definition step (p : Program) (s : Summary) (a : list nat) : list nat :=
  filter (fun x => member Nat.eq_dec x a || seed_enabled p s x ||
    edge_enabled p s a x) (universe p).
Fixpoint rounds (p : Program) (s : Summary) (n : nat) : list nat :=
  match n with 0 => [] | S k => step p s (rounds p s k) end.
Lemma step_spec : forall p s a x, In x (step p s a) <->
  In x (universe p) /\ (In x a \/
    (exists r, In r (seeds p) /\ x = seed_at r /\ enabled s (seed_guard r) = true) \/
    (exists e, In e (edges p) /\ x = edge_to e /\ enabled s (edge_guard e) = true /\ In (edge_from e) a)).
Proof.
  intros. unfold step, seed_enabled, edge_enabled.
  rewrite filter_In, !orb_true_iff, member_spec, !existsb_exists.
  repeat setoid_rewrite andb_true_iff. setoid_rewrite Nat.eqb_eq. setoid_rewrite member_spec.
  firstorder.
Qed.
Lemma rounds_universe : forall p s n, incl (rounds p s n) (universe p).
Proof. intros p s [|n] x H; [contradiction|apply step_spec in H; tauto]. Qed.
Lemma rounds_nodup : forall p s n, NoDup (rounds p s n).
Proof. intros p s [|n]; [constructor|apply NoDup_filter, universe_nodup]. Qed.
Lemma step_monotone : forall p s a b, incl a b -> incl (step p s a) (step p s b).
Proof. intros p s a b H x. rewrite !step_spec. firstorder. Qed.
Lemma rounds_ascend : forall p s n, incl (rounds p s n) (rounds p s (S n)).
Proof.
  intros p s n x H. apply step_spec. split.
  - eapply rounds_universe; eauto.
  - left. assumption.
Qed.
Definition fixed (p : Program) (s : Summary) (n : nat) :=
  incl (rounds p s (S n)) (rounds p s n).
Lemma fixed_next : forall p s n, fixed p s n -> fixed p s (S n).
Proof. intros. unfold fixed in *. cbn [rounds]. apply step_monotone. assumption. Qed.
Lemma not_fixed_grows : forall p s n, ~ fixed p s n ->
  length (rounds p s n) < length (rounds p s (S n)).
Proof.
  intros p s n H. destruct (Nat.lt_ge_cases (length (rounds p s n))
    (length (rounds p s (S n)))); auto. exfalso. apply H.
  apply NoDup_length_incl; [apply rounds_nodup|assumption|apply rounds_ascend].
Qed.
Lemma not_fixed_lower_bound : forall p s n, ~ fixed p s n ->
  n < length (rounds p s (S n)).
Proof.
  intros p s n. induction n; intros H.
  - pose proof (not_fixed_grows p s 0 H). cbn [rounds] in *. simpl in *. lia.
  - assert (HN : ~ fixed p s n) by (intro HF; apply H, fixed_next, HF).
    specialize (IHn HN). pose proof (not_fixed_grows p s (S n) H). lia.
Qed.
Lemma incl_decidable : forall a b : list nat, {incl a b} + {~ incl a b}.
Proof.
  induction a as [|x a IH]; intros b.
  - left. intros x H. contradiction.
  - destruct (in_dec Nat.eq_dec x b) as [HX|HX];
      destruct (IH b) as [HA|HA].
    + left. intros y [HY|HY]; subst; auto.
    + right. intro H. apply HA. intros y HY. apply H. right; assumption.
    + right. intro H. apply HX, H. left; reflexivity.
    + right. intro H. apply HX, H. left; reflexivity.
Defined.
Theorem finite_saturation : forall p s, fixed p s (length (universe p)).
Proof.
  intros p s. unfold fixed.
  destruct (incl_decidable (rounds p s (S (length (universe p))))
    (rounds p s (length (universe p)))); auto.
  exfalso. pose proof (not_fixed_lower_bound p s _ n).
  pose proof (NoDup_incl_length (rounds_nodup p s (S (length (universe p))))
    (rounds_universe p s (S (length (universe p))))). lia.
Qed.

Theorem rounds_derivation_height : forall p s n x,
  In x (rounds p s n) <->
  exists d, ValidTree p s d /\ target d = x /\ height d <= n.
Proof.
  intros p s n. induction n; intros x.
  - simpl. split; [contradiction|]. intros [d [_ [_ H]]]. destruct d; simpl in H; lia.
  - cbn [rounds]. rewrite step_spec. split.
    + intros [HU [HA|[[r [HR [HX HG]]]|[e [HE [HX [HG HA]]]]]]].
      * apply IHn in HA. destruct HA as [d [HD [HT HH]]]. exists d. repeat split; auto; lia.
      * exists (BecauseSeed r). split; [constructor; assumption|]. simpl. auto with arith.
      * apply IHn in HA. destruct HA as [d [HD [HT HH]]].
        exists (BecauseTransfer e d). split; [constructor; assumption|]. simpl. auto with arith.
    + intros [d [HD [HT HH]]]. destruct d as [r|e parent]; inversion HD; subst; simpl in *.
      * split; [apply seed_in_universe; assumption|right; left; eauto].
      * split; [exact (proj2 (edge_in_universe p e ltac:(assumption)))|].
        right; right. exists e. repeat split; auto. apply IHn.
        exists parent. repeat split; auto; lia.
Qed.

Definition saturated p s := rounds p s (length (universe p)).
Theorem saturation_is_sharing : forall p s x, In x (saturated p s) <-> Shared p s x.
Proof.
  intros p s x. split.
  - intros H. apply rounds_derivation_height in H. apply shared_has_derivation. firstorder.
  - intros H. induction H; unfold saturated in *; apply finite_saturation; apply step_spec.
    + split; [apply seed_in_universe; assumption|]. right; left. eauto.
    + split; [exact (proj2 (edge_in_universe p e ltac:(assumption)))|]. right; right. eauto.
Qed.
Definition Exclusive p s x := ~ Shared p s x.
Theorem exclusive_fixed_point : forall p s x,
  Exclusive p s x <->
  (forall r, In r (seeds p) -> seed_at r = x -> enabled s (seed_guard r) <> true) /\
  (forall e, In e (edges p) -> edge_to e = x -> enabled s (edge_guard e) = true ->
    Exclusive p s (edge_from e)).
Proof.
  intros p s x. split.
  - intros H. split.
    + intros r HR HX HG. apply H. subst x. constructor; assumption.
    + intros e HE HX HG HP. apply H. subst x. eapply SharedTransfer; eauto.
  - intros [HS HT] H. inversion H; subst.
    + eapply HS; eauto.
    + eapply HT; eauto.
Qed.
Theorem exclusive_greatest : forall p s (candidate : nat -> Prop),
  (forall x, candidate x ->
    (forall r, In r (seeds p) -> seed_at r = x -> enabled s (seed_guard r) <> true) /\
    (forall e, In e (edges p) -> edge_to e = x -> enabled s (edge_guard e) = true -> candidate (edge_from e))) ->
  forall x, candidate x -> Exclusive p s x.
Proof.
  intros p s candidate HC x HX HS. induction HS.
  - destruct (HC _ HX) as [HN _]. eapply HN; eauto.
  - apply IHHS. destruct (HC _ HX) as [_ HP]. eapply HP; eauto.
Qed.
Theorem first_round_is_minimum_height : forall p s n x,
  In x (rounds p s (S n)) -> ~ In x (rounds p s n) ->
  (exists d, ValidTree p s d /\ target d = x /\ height d = S n) /\
  (forall d, ValidTree p s d -> target d = x -> S n <= height d).
Proof.
  intros p s n x HY HN. apply rounds_derivation_height in HY.
  assert (LOW : forall d, ValidTree p s d -> target d = x -> S n <= height d).
  { intros d HD HT. destruct (Nat.le_gt_cases (S n) (height d)); auto.
    exfalso. apply HN, rounds_derivation_height. exists d. repeat split; auto; lia. }
  split; auto. destruct HY as [d [HD [HT HH]]]. exists d. repeat split; auto.
  specialize (LOW d HD HT). lia.
Qed.

(* A missing ExclusivityAt fact is an analysis limitation, not a claim of an
   unsafe source execution. Only the Hazard constructor certifies a stale run.
   The requested boundary and location are checked, not trusted from a witness. *)
Inductive Cause := Hazard | ExclusivityAt (fact : nat).
Record Witness := witness {
  witness_boundary : nat; witness_site : nat; witness_tree : Derivation;
  witness_cause : Cause
}.
Definition trace_outcome p b := run (relocates p) empty_heap (boundary_trace b).
Definition check_cause p b c :=
  match c with
  | Hazard => stale_at (boundary_target b) (trace_outcome p b)
  | ExclusivityAt x => Nat.eqb x (boundary_target b) && negb (stale_at (boundary_target b) (trace_outcome p b))
  end.
Definition check_witness p s requested w :=
  match nth_error (boundaries p) requested with
  | None => false
  | Some b => Nat.eqb (witness_boundary w) requested &&
      Nat.eqb (witness_site w) (boundary_site b) &&
      Nat.eqb (target (witness_tree w)) (boundary_target b) &&
      enabled s (boundary_effect b) && check_tree p s (witness_tree w) &&
      check_cause p b (witness_cause w)
  end.
Definition refusal p s requested :=
  match nth_error (boundaries p) requested with
  | None => false
  | Some b => enabled s (boundary_effect b) &&
      member Nat.eq_dec (boundary_target b) (saturated p s)
  end.
Inductive ValidCause p s b : Cause -> Prop :=
| ValidHazard : (exists buffer, trace_outcome p b = StaleRead (boundary_target b) buffer) -> ValidCause p s b Hazard
| ValidMissing : ~ Exclusive p s (boundary_target b) ->
    stale_at (boundary_target b) (trace_outcome p b) = false -> ValidCause p s b (ExclusivityAt (boundary_target b)).
Definition ValidWitness p s requested w := exists b,
  nth_error (boundaries p) requested = Some b /\
  witness_boundary w = requested /\ witness_site w = boundary_site b /\
  target (witness_tree w) = boundary_target b /\
  enabled s (boundary_effect b) = true /\ ValidTree p s (witness_tree w) /\
  ValidCause p s b (witness_cause w).

Theorem check_witness_sound : forall p s requested w,
  check_witness p s requested w = true -> ValidWitness p s requested w.
Proof.
  intros p s requested w. unfold check_witness.
  destruct (nth_error (boundaries p) requested) as [b|] eqn:HB; [|discriminate].
  rewrite !andb_true_iff, !Nat.eqb_eq, check_tree_spec.
  intros [[[[[HW HL] HT] HE] HD] HC]. exists b. repeat split; auto.
  destruct (witness_cause w) as [|x]; simpl in HC.
  - constructor. apply stale_spec. assumption.
  - apply andb_true_iff in HC as [HX HN]. apply Nat.eqb_eq in HX. subst x.
    constructor; [|apply negb_true_iff; assumption].
    intros H. apply H, shared_has_derivation. exists (witness_tree w). auto.
Qed.
Theorem check_witness_complete : forall p s requested w,
  ValidWitness p s requested w -> check_witness p s requested w = true.
Proof.
  intros p s requested w [b [HB [HW [HL [HT [HE [HD HC]]]]]]].
  unfold check_witness. rewrite HB. apply check_tree_spec in HD.
  rewrite HW, HL, HT, !Nat.eqb_refl, HE, HD. simpl.
  inversion HC as [Hrun | Hmissing Hsafe]; subst; simpl.
  - apply stale_spec. assumption.
  - rewrite Nat.eqb_refl, Hsafe. reflexivity.
Qed.
Theorem checked_witness_refuses : forall p s requested w,
  check_witness p s requested w = true -> refusal p s requested = true.
Proof.
  intros p s requested w H. apply check_witness_sound in H.
  destruct H as [b [HB [_ [_ [HT [HE [HD _]]]]]]].
  unfold refusal. rewrite HB, HE. simpl. apply member_spec, saturation_is_sharing,
    shared_has_derivation. exists (witness_tree w). auto.
Qed.
Theorem refusal_has_witness : forall p s requested,
  refusal p s requested = true -> exists w, check_witness p s requested w = true.
Proof.
  intros p s requested. unfold refusal.
  destruct (nth_error (boundaries p) requested) as [b|] eqn:HB; [|discriminate].
  rewrite andb_true_iff, member_spec, saturation_is_sharing, shared_has_derivation.
  intros [HE [d [HD HT]]].
  exists (witness requested (boundary_site b) d
    (if stale_at (boundary_target b) (trace_outcome p b) then Hazard else ExclusivityAt (boundary_target b))).
  apply check_witness_complete. exists b. cbn. repeat split; auto.
  destruct (stale_at (boundary_target b) (trace_outcome p b)) eqn:HS.
  - constructor. apply stale_spec; assumption.
  - constructor; auto. intros H. apply H, shared_has_derivation. eauto.
Qed.
Theorem witness_classified : forall p s requested w,
  check_witness p s requested w = true -> exists b,
  nth_error (boundaries p) requested = Some b /\
  ((witness_cause w = Hazard /\ exists buffer, trace_outcome p b = StaleRead (boundary_target b) buffer) \/
   (witness_cause w = ExclusivityAt (boundary_target b) /\
    ~ Exclusive p s (boundary_target b) /\ stale_at (boundary_target b) (trace_outcome p b) = false)).
Proof.
  intros p s requested w H. apply check_witness_sound in H.
  destruct H as [b [HB [_ [_ [_ [_ [_ HC]]]]]]]. exists b. split; auto.
  inversion HC; subst; eauto.
Qed.
Theorem hazard_and_missing_disjoint : forall p s b x,
  ValidCause p s b Hazard -> ~ ValidCause p s b (ExclusivityAt x).
Proof.
  intros p s b x HH HM. inversion HH as [HR | Hmissing Hfalse]; subst.
  inversion HM; subst. apply stale_spec in HR. congruence.
Qed.

Fixpoint tree_support (d : Derivation) : list nat :=
  match d with
  | BecauseSeed r => keys (seed_guard r)
  | BecauseTransfer e parent => keys (edge_guard e) ++ tree_support parent
  end.
Definition witness_support p requested w :=
  match nth_error (boundaries p) requested with
  | None => tree_support (witness_tree w)
  | Some b => keys (boundary_effect b) ++ tree_support (witness_tree w)
  end.
Definition agree_on (support : list nat) (s t : Summary) :=
  forall k, In k support -> s k = t k.
Lemma enabled_local : forall s t key, agree_on (keys key) s t -> enabled s key = enabled t key.
Proof. intros s t [k|] H; simpl; auto. apply H. simpl; auto. Qed.
Lemma tree_local : forall p s t d, agree_on (tree_support d) s t ->
  check_tree p s d = check_tree p t d.
Proof.
  intros p s t d. induction d; intros H; simpl in *.
  - rewrite (enabled_local s t (seed_guard r) H). reflexivity.
  - assert (HG : enabled s (edge_guard e) = enabled t (edge_guard e)).
    { apply enabled_local. intros k HK. apply H, in_or_app. auto. }
    rewrite HG, IHd; auto. intros k HK. apply H, in_or_app. auto.
Qed.
Theorem witness_sufficient : forall p s t requested w,
  agree_on (witness_support p requested w) s t ->
  check_witness p s requested w = check_witness p t requested w.
Proof.
  intros p s t requested w H. unfold check_witness, witness_support in *.
  destruct (nth_error (boundaries p) requested) as [b|]; auto.
  assert (HG : enabled s (boundary_effect b) = enabled t (boundary_effect b)).
  { apply enabled_local. intros k HK. apply H, in_or_app. auto. }
  rewrite HG, (tree_local p s t (witness_tree w)); auto.
  intros k HK. apply H, in_or_app. auto.
Qed.
Corollary witness_preserves_refusal : forall p s t requested w,
  check_witness p s requested w = true ->
  agree_on (witness_support p requested w) s t -> refusal p t requested = true.
Proof.
  intros p s t requested w HW HA. eapply checked_witness_refuses.
  rewrite <- (witness_sufficient p s t requested w HA). exact HW.
Qed.
Lemma disagreement_witness : forall ks s t, ~ agree_on ks s t ->
  exists k, In k ks /\ s k <> t k.
Proof.
  induction ks as [|k ks IH]; intros s t H.
  - exfalso. apply H. intros k HK. contradiction.
  - destruct (Bool.bool_dec (s k) (t k)) as [HE|HN].
    + destruct (IH s t) as [j [HJ HD]].
      * intro HA. apply H. intros j [E|E]; subst; auto.
      * exists j. split; auto. right; assumption.
    + exists k. split; auto. left; reflexivity.
Qed.
Theorem remote_break_attributed : forall p accepted refused requested,
  refusal p accepted requested = false -> refusal p refused requested = true ->
  exists w k, check_witness p refused requested w = true /\
    In k (witness_support p requested w) /\ accepted k <> refused k.
Proof.
  intros p accepted refused requested HA HR.
  destruct (refusal_has_witness p refused requested HR) as [w HW].
  assert (HN : ~ agree_on (witness_support p requested w) accepted refused).
  { intro HE. pose proof (witness_sufficient p accepted refused requested w HE) as EQ.
    rewrite <- EQ in HW. apply checked_witness_refuses in HW. congruence. }
  destruct (disagreement_witness _ _ _ HN) as [k [HK HD]]. exists w,k. auto.
Qed.

Definition fewer_adverse_facts (precise coarse : Summary) :=
  forall k, precise k = true -> coarse k = true.
Lemma enabled_monotone : forall precise coarse key,
  fewer_adverse_facts precise coarse -> enabled precise key = true -> enabled coarse key = true.
Proof. intros precise coarse [k|] H; simpl; auto. Qed.
Lemma sharing_monotone : forall p precise coarse x,
  fewer_adverse_facts precise coarse -> Shared p precise x -> Shared p coarse x.
Proof.
  intros p precise coarse x HS H. induction H.
  - constructor; eauto using enabled_monotone.
  - eapply SharedTransfer; eauto using enabled_monotone.
Qed.
Theorem fewer_effects_fewer_refusals : forall p precise coarse requested,
  fewer_adverse_facts precise coarse ->
  refusal p precise requested = true -> refusal p coarse requested = true.
Proof.
  intros p precise coarse requested H. unfold refusal.
  destruct (nth_error (boundaries p) requested) as [b|]; [|discriminate].
  rewrite !andb_true_iff, !member_spec, !saturation_is_sharing.
  intros [HE HS]. split; eauto using enabled_monotone, sharing_monotone.
Qed.

End RefusalExplanation.
