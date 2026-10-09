(* A writable view as a whole-backing focus.

   A write-through view v over a backing y is the core's own focus with
   the empty path, SFocus v y [] body, scoped from the view's creation to
   its last use. For the duration of the scope the backing is suspended:
   the source rule hands its value to v, the target rule moves y's exact
   blocks to v and removes y from the owned environment, and the core's
   elaboration refuses any live use of y inside the body. At the end the
   view's final value moves back into y with no copy. Consequently:

   - the backing outlives every view use: all uses are inside the scope,
     and y cannot be dropped, moved, grown or passed on while suspended;
   - element writes through the view are writes to the backing
     (write-through), and the old element is released by the core's
     ordinary settle when the element is redefined;
   - the static rule [view_admitted] additionally refuses any mention of
     the backing in the scope and any use of the view other than element
     read, element write and observation, so the view cannot grow,
     escape, be copied or be passed to a call;
   - [view_scope_keeps_backing_shape]: the backing has the same length
     after the scope, so a {data, length} descriptor taken at the start
     stays valid for the whole scope.

   This is a static issuer: the evidence is the scope placement plus the
   boolean admission and the core's elaboration; no runtime generation
   field, ticket ledger or dynamic oracle is consulted. It does not use
   the lease vocabulary of OwnershipTeardownAuthority.

   Developer-visible consequence: while a writable view is live, the
   backing itself cannot be named (read or written) in the scope. Read-only
   aliasing remains the business of OwnershipCleanReadOnly.

   Not covered: views returned from or stored by a routine, views passed
   to calls, several simultaneous writable views of one backing, index
   bounds of a sub-range view (a bounds failure is a source-level stop,
   not an ownership event), and the production descriptor refinement. *)
Require Import Stdlib.Lists.List Stdlib.Arith.PeanoNat Stdlib.Bool.Bool.
Require Import Stdlib.micromega.Lia.
Require Import OwnershipCleanCore OwnershipCleanCallLowering.
Import ListNotations.

Definition view_scope (v y : Var) (body : SStmt) : SStmt := SFocus v y [] body.

(* v[i] := f(ys): focus the element, redefine it. *)
Definition view_write (u v : Var) (i : nat) (f : list SVal -> SVal) (ys : list Var) : SStmt :=
  SFocus u v [i] (SDef u f ys).

(* Allowed uses of the view: element read (SField _ v _), element write
   (a one-index focus whose body does not mention v), and observation.
   Every other statement must not mention v. *)
Fixpoint view_ok (v : Var) (s : SStmt) : bool :=
  match s with
  | SSkip => true
  | SEmit _ => true
  | SField x _ _ => negb (Nat.eqb x v)
  | SFocus t y p b =>
      if Nat.eqb y v
      then Nat.eqb (length p) 1 && negb (Nat.eqb t v) && negb (vmem v (svars b))
      else negb (Nat.eqb t v) && view_ok v b
  | SSeq a b => view_ok v a && view_ok v b
  | SIf c a b => negb (Nat.eqb c v) && view_ok v a && view_ok v b
  | SWhile c _ b => negb (Nat.eqb c v) && view_ok v b
  | s0 => negb (vmem v (svars s0))
  end.

Definition view_admitted (v y : Var) (body : SStmt) : bool :=
  negb (Nat.eqb v y) && negb (vmem y (svars body)) && view_ok v body.

Lemma vmem_false_iff : forall x l, vmem x l = false <-> ~ In x l.
Proof.
  intros x l. unfold vmem. destruct (in_dec Nat.eq_dec x l); split; intro H;
    try discriminate; try contradiction; try reflexivity; exact n.
Qed.

Lemma neqb_false : forall a b, Nat.eqb a b = false -> a <> b.
Proof. intros a b H E. subst. rewrite Nat.eqb_refl in H. discriminate. Qed.

Lemma view_ok_absent : forall v s, ~ In v (svars s) -> view_ok v s = true.
Proof.
  intros v s. induction s as
    [ | x f ys | x y | x ys | x y | x y i | y | a IHa b IHb | c a IHa b IHb | c h b IHb
    | x g ys | g z ys | t y p b IHb | y xs | x f ys b IHb ];
    intros H; cbn [svars] in H; cbn [view_ok]; try reflexivity;
    try (apply negb_true_iff; apply vmem_false_iff; exact H).
  - apply negb_true_iff. apply Nat.eqb_neq. intro E; subst. apply H. left; reflexivity.
  - rewrite IHa, IHb; [reflexivity| |]; intro Hin; apply H; apply in_or_app; [right|left]; exact Hin.
  - assert (Hc : c <> v) by (intro E; subst; apply H; left; reflexivity).
    apply Nat.eqb_neq in Hc. rewrite Hc. simpl.
    rewrite IHa, IHb; [reflexivity| |]; intro Hin; apply H; right; apply in_or_app; [right|left]; exact Hin.
  - assert (Hc : c <> v) by (intro E; subst; apply H; left; reflexivity).
    apply Nat.eqb_neq in Hc. rewrite Hc. simpl. apply IHb. intro Hin; apply H; right; exact Hin.
  - assert (Hy : y <> v) by (intro E; subst; apply H; right; left; reflexivity).
    assert (Ht : t <> v) by (intro E; subst; apply H; left; reflexivity).
    apply Nat.eqb_neq in Hy. apply Nat.eqb_neq in Ht. rewrite Hy, Ht. simpl.
    apply IHb. intro Hin; apply H; right; right; exact Hin.
Qed.

Lemma replace_nth_length : forall (A : Type) (l : list A) i x, length (replace_nth l i x) = length l.
Proof.
  intros A l. induction l as [|a l IH]; intros [|i] x; simpl; try reflexivity. f_equal. apply IH.
Qed.

(* Every admitted use keeps the view's length. *)
Theorem view_ok_preserves_length : forall funs procs sg s sg' tr,
  sexec funs procs sg s sg' tr -> forall v cs, view_ok v s = true -> sg v = Some (SNode cs) ->
  exists cs', sg' v = Some (SNode cs') /\ length cs' = length cs.
Proof.
  intros funs procs sg s sg' tr Hs.
  induction Hs as
    [ sg
    | sg x f ys vs Hl
    | sg x y vv Hy
    | sg x ys vs Hl
    | sg x y cs0 vv Hx Hy
    | sg x y i cs0 vv Hy Hi
    | sg y vv Hy
    | sg1 sg2 sg3 s1 s2 tr1 tr2 tr H1 IH1 H2 IH2 Htr
    | sg sg' c s1 s2 vv tr Hc Ht H1 IH1
    | sg sg' c s1 s2 vv tr Hc Ht H2 IH2
    | sg c h b vv Hc Ht
    | sg sg1 sg2 c h b vv tr1 tr2 tr Hc Ht Hb IHb Hw IHw Htr
    | sg x g ys vs ps body ret sgc tr vv Hg Hl Hlen Hb IHb Hr
    | sg g z ys vz vs io ps body sgc tr vv Hg Hz Hl Hlen Hb IHb Hr
    | sg t y p s c cv sg' tr cv' c' Hy Hget Hs IHs Ht Hset
    | sg y xs cs0 Hy Hlen
    | sg x f ys s vs sg' tr Hl Hs IHs ];
    intros v cs Hok Hv; cbn [view_ok] in Hok.
  - exists cs. split; [exact Hv|reflexivity].
  - apply negb_true_iff, vmem_false_iff in Hok.
    exists cs. split; [|reflexivity]. rewrite supd_other; [exact Hv|].
    intro E; subst; apply Hok; left; reflexivity.
  - apply negb_true_iff, vmem_false_iff in Hok.
    exists cs. split; [|reflexivity]. rewrite supd_other; [exact Hv|].
    intro E; subst; apply Hok; left; reflexivity.
  - apply negb_true_iff, vmem_false_iff in Hok.
    exists cs. split; [|reflexivity]. rewrite supd_other; [exact Hv|].
    intro E; subst; apply Hok; left; reflexivity.
  - apply negb_true_iff, vmem_false_iff in Hok.
    exists cs. split; [|reflexivity]. rewrite supd_other; [exact Hv|].
    intro E; subst; apply Hok; left; reflexivity.
  - apply negb_true_iff in Hok. apply neqb_false in Hok.
    exists cs. split; [|reflexivity]. rewrite supd_other; [exact Hv|exact Hok].
  - exists cs. split; [exact Hv|reflexivity].
  - apply andb_true_iff in Hok. destruct Hok as [Ha Hb].
    destruct (IH1 v cs Ha Hv) as [cs1 [E1 L1]]. destruct (IH2 v cs1 Hb E1) as [cs2 [E2 L2]].
    exists cs2. split; [exact E2|lia].
  - apply andb_true_iff in Hok. destruct Hok as [Hok _]. apply andb_true_iff in Hok.
    destruct Hok as [_ Ha]. exact (IH1 v cs Ha Hv).
  - apply andb_true_iff in Hok. destruct Hok as [_ Hb]. exact (IH2 v cs Hb Hv).
  - exists cs. split; [exact Hv|reflexivity].
  - assert (Hw' : view_ok v (SWhile c h b) = true) by (cbn [view_ok]; exact Hok).
    apply andb_true_iff in Hok. destruct Hok as [_ Hb'].
    destruct (IHb v cs Hb' Hv) as [cs1 [E1 L1]]. destruct (IHw v cs1 Hw' E1) as [cs2 [E2 L2]].
    exists cs2. split; [exact E2|lia].
  - apply negb_true_iff, vmem_false_iff in Hok.
    exists cs. split; [|reflexivity]. rewrite supd_other; [exact Hv|].
    intro E; subst; apply Hok; left; reflexivity.
  - apply negb_true_iff, vmem_false_iff in Hok.
    exists cs. split; [|reflexivity]. rewrite supd_other; [exact Hv|].
    intro E; subst; apply Hok; left; reflexivity.
  - destruct (Nat.eqb y v) eqn:Eyv.
    + (* element write through the view *)
      apply Nat.eqb_eq in Eyv. subst y.
      apply andb_true_iff in Hok. destruct Hok as [Hok _]. apply andb_true_iff in Hok.
      destruct Hok as [Hp _]. apply Nat.eqb_eq in Hp.
      destruct p as [|i [|]]; simpl in Hp; try discriminate.
      rewrite Hv in Hy. injection Hy as Hy. subst c.
      simpl in Hset. destruct (nth_error cs i) as [ci|] eqn:Ei; [|discriminate].
      injection Hset as Hset. subst c'.
      exists (replace_nth cs i cv'). split; [apply supd_same|apply replace_nth_length].
    + apply andb_true_iff in Hok. destruct Hok as [Ht' Hb'].
      apply negb_true_iff in Ht'. apply neqb_false in Ht'. apply neqb_false in Eyv.
      assert (Hv' : supd sg t cv v = Some (SNode cs)) by (rewrite supd_other by exact Ht'; exact Hv).
      destruct (IHs v cs Hb' Hv') as [cs1 [E1 L1]].
      exists cs1. split; [rewrite supd_other by exact Eyv; exact E1|exact L1].
  - apply negb_true_iff, vmem_false_iff in Hok.
    exists cs. split; [|reflexivity]. rewrite supd_list_out; [exact Hv|].
    intro Hin; apply Hok; right; exact Hin.
  - apply negb_true_iff, vmem_false_iff in Hok.
    assert (Hx : x <> v) by (intro E; subst; apply Hok; left; reflexivity).
    assert (Hb' : view_ok v s = true).
    { apply view_ok_absent. intro Hin. apply Hok. right. apply in_or_app. right. exact Hin. }
    assert (Hv' : supd sg x (f vs) v = Some (SNode cs)) by (rewrite supd_other by exact Hx; exact Hv).
    exact (IHs v cs Hb' Hv').
Qed.

(* The backing has the same length after an admitted view scope. *)
Theorem view_scope_keeps_backing_shape : forall funs procs sg v y body sg' tr cs,
  view_admitted v y body = true -> sexec funs procs sg (view_scope v y body) sg' tr ->
  sg y = Some (SNode cs) ->
  exists cs', sg' y = Some (SNode cs') /\ length cs' = length cs.
Proof.
  intros funs procs sg v y body sg' tr cs Hadm Hs Hy.
  unfold view_admitted in Hadm. apply andb_true_iff in Hadm. destruct Hadm as [Hadm Hok].
  unfold view_scope in Hs. inversion Hs as [| | | | | | | | | | | | | | sg0 t y0 p s c cv sg1 tr0 cv' c' Hy0 Hget Hb Ht Hset| |]; subst.
  rewrite Hy in Hy0. injection Hy0 as Hy0. subst c.
  simpl in Hget. injection Hget as Hget. subst cv.
  simpl in Hset. injection Hset as Hset. subst c'.
  destruct (view_ok_preserves_length funs procs _ _ _ _ Hb v cs Hok (supd_same _ _ _)) as [cs' [E L]].
  rewrite Ht in E. injection E as E. subst cv'.
  exists cs'. split; [apply supd_same|exact L].
Qed.

(* Static refusals of the admission. *)
Theorem view_refuses_backing_mention : forall v y body,
  In y (svars body) -> view_admitted v y body = false.
Proof.
  intros v y body Hin. unfold view_admitted.
  assert (Hm : vmem y (svars body) = true) by (unfold vmem; destruct (in_dec Nat.eq_dec y (svars body)); [reflexivity|contradiction]).
  rewrite Hm. simpl. rewrite andb_false_r. reflexivity.
Qed.

(* The same scope in the ownership machine: the core's soundness applies,
   and the backing is back with its length. *)
Theorem view_scope_sound : forall M funs procs,
  (forall g d, funs g = Some d -> elab_fun M g d <> None) ->
  (forall g d, procs g = Some d -> elab_proc M g d <> None) ->
  forall sg v y body sg' tr cs L B t Lin rho beta H n R,
  view_admitted v y body = true -> sexec funs procs sg (view_scope v y body) sg' tr ->
  sg y = Some (SNode cs) ->
  elab M (view_scope v y body) L B = Some (t, Lin) -> INV rho beta H n R -> CORR sg rho beta Lin B ->
  exists rho' beta' H' n' cs',
    texec (tfuns_of M funs) (tprocs_of M procs) rho beta H n t rho' beta' H' n' tr /\
    INV rho' beta' H' n' R /\ CORR sg' rho' beta' L B /\
    sg' y = Some (SNode cs') /\ length cs' = length cs.
Proof.
  intros M funs procs Hf Hp sg v y body sg' tr cs L B t Lin rho beta H n R Hadm Hs Hy Hel HI HC.
  destruct (elab_sound M funs procs Hf Hp sg _ sg' tr Hs L B t Lin rho beta H n R Hel HI HC)
    as [rho' [beta' [H' [n' [Et [I' C']]]]]].
  destruct (view_scope_keeps_backing_shape funs procs sg v y body sg' tr cs Hadm Hs Hy) as [cs' [E Lc]].
  exists rho', beta', H', n', cs'.
  split; [exact Et|split; [exact I'|split; [exact C'|split; [exact E|exact Lc]]]].
Qed.

(* ------------------------------------------------------------------ *)
(* Witnesses                                                            *)
(* ------------------------------------------------------------------ *)

(* arr(0) := [1, 2, 3]; view v(5) over arr { v[1] := 9; x(7) := v[0] };
   arr.push(4); emit x; emit arr. The view write is visible in arr, the
   push after the scope is allowed, and every block is freed. *)
Definition view_body : SStmt :=
  SSeq (view_write 6 5 1 (fun _ => SLeaf 9) []) (SField 7 5 0).

Definition view_program : SStmt :=
  SSeq (SDef 1 (fun _ => SLeaf 1) [])
    (SSeq (SDef 2 (fun _ => SLeaf 2) [])
      (SSeq (SDef 3 (fun _ => SLeaf 3) [])
        (SSeq (SPack 0 [1; 2; 3])
          (SSeq (view_scope 5 0 view_body)
            (SSeq (SDef 8 (fun _ => SLeaf 4) [])
              (SSeq (SPush 0 8) (SSeq (SEmit 7) (SEmit 0)))))))).

Example view_body_admitted : view_admitted 5 0 view_body = true.
Proof. reflexivity. Qed.

(* Refused by the admission: growing, copying, passing or transferring
   the view; naming the backing in the scope; growing the backing between
   two view uses. *)
Example view_refusals :
  view_admitted 5 0 (SPush 5 8) = false /\
  view_admitted 5 0 (SCopy 9 5) = false /\
  view_admitted 5 0 (SCall 9 0 [5]) = false /\
  view_admitted 5 0 (SUnpack 5 [9; 10]) = false /\
  view_admitted 5 0 (SPush 0 8) = false /\
  view_admitted 5 0 (SCopy 9 0) = false /\
  view_admitted 5 0 (SSeq (SField 7 5 0) (SSeq (SPush 0 8) (SField 9 5 1))) = false /\
  view_admitted 5 5 (SField 7 5 0) = false.
Proof. repeat split; reflexivity. Qed.

(* The core itself already refuses a live use of the suspended backing:
   growth, copy-out/transfer, and growth between two view uses. *)
Example core_refuses_backing_use_in_scope :
  elab no_summaries (view_scope 5 0 (SPush 0 8)) [0] [] = None /\
  elab no_summaries (view_scope 5 0 (SCopy 9 0)) [0; 9] [] = None /\
  elab no_summaries (view_scope 5 0 (SSeq (SField 7 5 0) (SSeq (SPush 0 8) (SField 9 5 1))))
    [0; 7; 9] [] = None.
Proof. repeat split; reflexivity. Qed.

Lemma view_program_source : exists sg', sexec no_funs no_procs sempty view_program sg'
  [SLeaf 1; SNode [SLeaf 1; SLeaf 9; SLeaf 3; SLeaf 4]].
Proof.
  eexists. unfold view_program.
  eapply SE_Seq with (tr1 := []); [apply (SE_Def no_funs no_procs sempty 1 _ [] []); reflexivity| |reflexivity].
  eapply SE_Seq with (tr1 := []); [apply (SE_Def no_funs no_procs _ 2 _ [] []); reflexivity| |reflexivity].
  eapply SE_Seq with (tr1 := []); [apply (SE_Def no_funs no_procs _ 3 _ [] []); reflexivity| |reflexivity].
  eapply SE_Seq with (tr1 := []); [apply SE_Pack; reflexivity| |reflexivity].
  eapply SE_Seq with (tr1 := []).
  - unfold view_scope, view_body, view_write.
    eapply SE_Focus; [reflexivity|reflexivity| | |].
    + eapply SE_Seq with (tr1 := []) (tr2 := []).
      * eapply SE_Focus; [reflexivity|reflexivity| | |].
        -- apply (SE_Def no_funs no_procs _ 6 _ [] []); reflexivity.
        -- reflexivity.
        -- reflexivity.
      * eapply SE_Field; reflexivity.
      * reflexivity.
    + reflexivity.
    + reflexivity.
  - eapply SE_Seq with (tr1 := []); [apply (SE_Def no_funs no_procs _ 8 _ [] []); reflexivity| |reflexivity].
    eapply SE_Seq with (tr1 := []); [eapply SE_Push; reflexivity| |reflexivity].
    eapply SE_Seq with (tr1 := [SLeaf 1]); [apply SE_Emit; reflexivity|apply SE_Emit; reflexivity|reflexivity].
  - reflexivity.
Qed.

Theorem view_program_frees_everything : exists t n,
  elab no_summaries view_program [] [] = Some (t, []) /\
  texec (tfuns_of no_summaries no_funs) (tprocs_of no_summaries no_procs)
    [] [] [] 0 t [] [] [] n [SLeaf 1; SNode [SLeaf 1; SLeaf 9; SLeaf 3; SLeaf 4]].
Proof.
  assert (Hel : exists t, elab no_summaries view_program [] [] = Some (t, [])) by (eexists; reflexivity).
  destruct Hel as [t Et].
  destruct view_program_source as [sg' Es].
  destruct (closed_program_frees_everything no_summaries no_funs no_procs
    (no_funs_ok no_summaries) (no_procs_ok no_summaries) _ _ _ _ _ Et Es) as [n En].
  exists t, n. split; assumption.
Qed.
