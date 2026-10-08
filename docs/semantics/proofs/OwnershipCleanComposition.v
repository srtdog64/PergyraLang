(* Verified composition over the canonical cleanup machine, not a second
   ownership semantics. Sequential composition is the unit-valued fragment
   of state-and-trace bind; a conditional selects exactly one continuation.
   Only administrative Skip nodes are erased. Drop is NOT idempotent. *)
Require Import Stdlib.Lists.List Stdlib.micromega.Lia.
Require Import OwnershipCleanCore.
Import ListNotations.

Definition clean_seq (a b : TStmt) : TStmt :=
  match a, b with
  | TSkip, _ => b
  | _, TSkip => a
  | _, _ => TSeq a b
  end.

Fixpoint normalize_cleanup (t : TStmt) : TStmt :=
  match t with
  | TSeq a b => clean_seq (normalize_cleanup a) (normalize_cleanup b)
  | TIf c a b => TIf c (normalize_cleanup a) (normalize_cleanup b)
  | TWhile c b => TWhile c (normalize_cleanup b)
  | _ => t
  end.

(* Modes, liveness, admission refusals and the owner remain canonical. *)
Definition elab_normalized M s L B : option (TStmt * list Var) :=
  match elab M s L B with
  | Some (t, Lin) => Some (normalize_cleanup t, Lin)
  | None => None
  end.

Section Composition.
Variable tf tp : TTable.

Definition cleanup_equiv (a b : TStmt) : Prop :=
  forall rho beta H n rho' beta' H' n' tr,
    texec tf tp rho beta H n a rho' beta' H' n' tr <->
    texec tf tp rho beta H n b rho' beta' H' n' tr.

Lemma cleanup_equiv_refl : forall a, cleanup_equiv a a.
Proof. unfold cleanup_equiv; tauto. Qed.

Lemma cleanup_equiv_trans : forall a b c,
  cleanup_equiv a b -> cleanup_equiv b c -> cleanup_equiv a c.
Proof.
  intros a b c Hab Hbc rho beta H n rho' beta' H' n' tr.
  rewrite (Hab rho beta H n rho' beta' H' n' tr). apply Hbc.
Qed.

Lemma cleanup_left_unit : forall t, cleanup_equiv (TSeq TSkip t) t.
Proof.
  intros t rho beta H n rho' beta' H' n' tr. split; intro E.
  - inversion E; subst. match goal with
    | Hs : texec _ _ _ _ _ _ TSkip _ _ _ _ _ |- _ => inversion Hs; subst
    end. simpl in *. assumption.
  - eapply TE_Seq; [apply TE_Skip|exact E|reflexivity].
Qed.

Lemma cleanup_right_unit : forall t, cleanup_equiv (TSeq t TSkip) t.
Proof.
  intros t rho beta H n rho' beta' H' n' tr. split; intro E.
  - inversion E; subst. match goal with
    | Hs : texec _ _ _ _ _ _ TSkip _ _ _ _ _ |- _ => inversion Hs; subst
    end. rewrite app_nil_r. assumption.
  - eapply TE_Seq; [exact E|apply TE_Skip|symmetry; apply app_nil_r].
Qed.

Lemma cleanup_associative : forall a b c,
  cleanup_equiv (TSeq (TSeq a b) c) (TSeq a (TSeq b c)).
Proof.
  intros a b c rho beta H n rho' beta' H' n' tr. split; intro E;
    inversion E; subst.
  - match goal with
    | Eab : texec _ _ _ _ _ _ (TSeq a b) _ _ _ _ _ |- _ => inversion Eab; subst
    end.
    eapply TE_Seq; [eassumption|eapply TE_Seq; eauto|].
    rewrite app_assoc. reflexivity.
  - match goal with
    | Ebc : texec _ _ _ _ _ _ (TSeq b c) _ _ _ _ _ |- _ => inversion Ebc; subst
    end.
    eapply TE_Seq; [eapply TE_Seq; eauto|eassumption|].
    rewrite app_assoc. reflexivity.
Qed.

Lemma cleanup_seq_congr : forall a a' b b',
  cleanup_equiv a a' -> cleanup_equiv b b' ->
  cleanup_equiv (TSeq a b) (TSeq a' b').
Proof.
  unfold cleanup_equiv. intros a a' b b' Ha Hb rho beta H n rho' beta' H' n' tr.
  split; intro E; inversion E; subst.
  - eapply TE_Seq; [apply Ha; eassumption|apply Hb; eassumption|reflexivity].
  - eapply TE_Seq; [apply Ha; eassumption|apply Hb; eassumption|reflexivity].
Qed.

Lemma cleanup_if_congr : forall c a a' b b',
  cleanup_equiv a a' -> cleanup_equiv b b' ->
  cleanup_equiv (TIf c a b) (TIf c a' b').
Proof.
  unfold cleanup_equiv. intros c a a' b b' Ha Hb rho beta H n rho' beta' H' n' tr.
  split; intro E; inversion E; subst.
  - eapply TE_IfT; try eassumption. apply Ha; assumption.
  - eapply TE_IfF; try eassumption. apply Hb; assumption.
  - eapply TE_IfT; try eassumption. apply Ha; assumption.
  - eapply TE_IfF; try eassumption. apply Hb; assumption.
Qed.

Lemma cleanup_while_forward : forall c b b',
  (forall rho beta H n rho' beta' H' n' tr,
    texec tf tp rho beta H n b rho' beta' H' n' tr ->
    texec tf tp rho beta H n b' rho' beta' H' n' tr) ->
  forall rho beta H n rho' beta' H' n' tr,
    texec tf tp rho beta H n (TWhile c b) rho' beta' H' n' tr ->
    texec tf tp rho beta H n (TWhile c b') rho' beta' H' n' tr.
Proof.
  intros c b b' Hb rho beta H n rho' beta' H' n' tr E.
  remember (TWhile c b) as loop eqn:Eq. induction E; inversion Eq; subst.
  - eapply TE_WhileF; eassumption.
  - eapply TE_WhileT with (rho1 := rho1) (beta1 := beta1) (H1 := H1) (n1 := n1)
      (cv := cv) (bs := bs) (tr1 := tr1) (tr2 := tr2).
    + exact H0.
    + exact H3.
    + exact H4.
    + apply Hb; exact E1.
    + apply IHE2; reflexivity.
    + reflexivity.
Qed.

Lemma cleanup_while_congr : forall c b b',
  cleanup_equiv b b' -> cleanup_equiv (TWhile c b) (TWhile c b').
Proof.
  intros c b b' Hb. unfold cleanup_equiv in *.
  intros. split; eapply cleanup_while_forward; intros; apply Hb; eassumption.
Qed.

(* Read the guard once, then run one arm and its continuation. This is not
   permission to sequence both arms or to hoist a continuation before c. *)
Theorem cleanup_branch_bind : forall c a b k,
  cleanup_equiv (TSeq (TIf c a b) k) (TIf c (TSeq a k) (TSeq b k)).
Proof.
  intros c a b k rho beta H n rho' beta' H' n' tr. split; intro E.
  - inversion E; subst. match goal with
    | Ei : texec _ _ _ _ _ _ (TIf c a b) _ _ _ _ _ |- _ => inversion Ei; subst
    end.
    + eapply TE_IfT; try eassumption. eapply TE_Seq; eauto.
    + eapply TE_IfF; try eassumption. eapply TE_Seq; eauto.
  - inversion E; subst.
    + match goal with
      | Es : texec _ _ _ _ _ _ (TSeq a k) _ _ _ _ _ |- _ => inversion Es; subst
      end. eapply TE_Seq; [eapply TE_IfT; eauto|eassumption|reflexivity].
    + match goal with
      | Es : texec _ _ _ _ _ _ (TSeq b k) _ _ _ _ _ |- _ => inversion Es; subst
      end. eapply TE_Seq; [eapply TE_IfF; eauto|eassumption|reflexivity].
Qed.

Lemma clean_seq_equiv : forall a b, cleanup_equiv (TSeq a b) (clean_seq a b).
Proof.
  intros a b. destruct a; destruct b; simpl;
    try apply cleanup_left_unit; try apply cleanup_right_unit;
    apply cleanup_equiv_refl.
Qed.

(* Exact equivalence, not just equality of successful stdout: invalid guards,
   drops and calls cannot become newly successful. Callee tables are kept
   unchanged; normalizing every callee table is a separate refinement. *)
Theorem normalize_cleanup_equiv : forall t, cleanup_equiv t (normalize_cleanup t).
Proof.
  induction t; simpl; try apply cleanup_equiv_refl.
  - eapply cleanup_equiv_trans; [apply cleanup_seq_congr; eassumption|].
    apply clean_seq_equiv.
  - apply cleanup_if_congr; assumption.
  - apply cleanup_while_congr; assumption.
Qed.
End Composition.

Fixpoint cleanup_nodes (t : TStmt) : nat :=
  match t with
  | TSeq a b | TIf _ a b => 1 + cleanup_nodes a + cleanup_nodes b
  | TWhile _ b => 1 + cleanup_nodes b
  | _ => 1
  end.

(* Syntax-site accounting is deliberately not a dynamic execution cost. *)
Fixpoint primitive_sites (weight : TStmt -> nat) (t : TStmt) : nat :=
  match t with
  | TSkip => 0
  | TSeq a b | TIf _ a b => primitive_sites weight a + primitive_sites weight b
  | TWhile _ b => primitive_sites weight b
  | _ => weight t
  end.

Lemma clean_seq_nodes : forall a b,
  cleanup_nodes (clean_seq a b) <= 1 + cleanup_nodes a + cleanup_nodes b.
Proof. destruct a; destruct b; simpl; lia. Qed.

Theorem normalize_cleanup_nodes : forall t,
  cleanup_nodes (normalize_cleanup t) <= cleanup_nodes t.
Proof.
  induction t; simpl; try lia.
  pose proof (clean_seq_nodes (normalize_cleanup t1) (normalize_cleanup t2)). lia.
Qed.

Lemma clean_seq_sites : forall w a b,
  primitive_sites w (clean_seq a b) = primitive_sites w a + primitive_sites w b.
Proof. intros w; destruct a; destruct b; simpl; lia. Qed.

Theorem normalize_cleanup_sites : forall w t,
  primitive_sites w (normalize_cleanup t) = primitive_sites w t.
Proof.
  intros w t. induction t; simpl; try reflexivity;
    try rewrite clean_seq_sites; congruence.
Qed.

Lemma clean_seq_copies : forall a b,
  count_copies (clean_seq a b) = count_copies a + count_copies b.
Proof. destruct a; destruct b; simpl; lia. Qed.

Theorem normalize_cleanup_copies : forall t,
  count_copies (normalize_cleanup t) = count_copies t.
Proof. induction t; simpl; try reflexivity; try rewrite clean_seq_copies; congruence. Qed.

Theorem elab_normalized_refusal : forall M s L B,
  elab_normalized M s L B = None <-> elab M s L B = None.
Proof. intros. unfold elab_normalized. destruct (elab M s L B) as [[t Lin]|]; split; congruence. Qed.

Theorem elab_normalized_sound : forall M funs procs,
  (forall g d, funs g = Some d -> elab_fun M g d <> None) ->
  (forall g d, procs g = Some d -> elab_proc M g d <> None) ->
  forall sg s sg' tr,
    sexec funs procs sg s sg' tr ->
    forall L B t Lin rho beta H n R,
      elab_normalized M s L B = Some (t, Lin) ->
      INV rho beta H n R -> CORR sg rho beta Lin B ->
      exists rho' beta' H' n',
        texec (tfuns_of M funs) (tprocs_of M procs) rho beta H n t rho' beta' H' n' tr /\
        INV rho' beta' H' n' R /\ CORR sg' rho' beta' L B.
Proof.
  intros M funs procs Hf Hp sg s sg' tr Hs L B t Lin rho beta H n R Hel Hi Hc.
  unfold elab_normalized in Hel.
  destruct (elab M s L B) as [[raw Lin0]|] eqn:E; try discriminate.
  inversion Hel; subst.
  destruct (elab_sound M funs procs Hf Hp sg s sg' tr Hs
    L B raw Lin rho beta H n R E Hi Hc) as [rho' [beta' [H' [n' [Hex Hr]]]]].
  exists rho', beta', H', n'. split; [|exact Hr].
  apply (normalize_cleanup_equiv _ _ raw). exact Hex.
Qed.

Theorem normalized_closed_program_frees_everything : forall M funs procs,
  (forall g d, funs g = Some d -> elab_fun M g d <> None) ->
  (forall g d, procs g = Some d -> elab_proc M g d <> None) ->
  forall s t sg sg' tr,
    elab_normalized M s [] [] = Some (t, []) -> sexec funs procs sg s sg' tr ->
    exists n', texec (tfuns_of M funs) (tprocs_of M procs) [] [] [] 0 t [] [] [] n' tr.
Proof.
  intros M funs procs Hf Hp s t sg sg' tr Hel Hs.
  unfold elab_normalized in Hel. destruct (elab M s [] []) as [[raw Lin]|] eqn:E; try discriminate.
  inversion Hel; subst.
  destruct (closed_program_frees_everything M funs procs Hf Hp s raw sg sg' tr E Hs) as [n' Hrun].
  exists n'. apply (normalize_cleanup_equiv _ _ raw). exact Hrun.
Qed.

(* Falsifiers live on the SAME texec relation. Empty arms do not make a
   missing guard valid, and mutual exclusion does not mean idempotence. *)
Definition composition_no_targets : TTable := fun _ => None.

Example empty_branch_guard_cannot_be_erased :
  ~ cleanup_equiv composition_no_targets composition_no_targets
      (TIf (Src 0) TSkip TSkip) TSkip.
Proof.
  intro Eq. assert (E : texec composition_no_targets composition_no_targets
    [] [] [] 0 (TIf (Src 0) TSkip TSkip) [] [] [] 0 []).
  { apply Eq. apply TE_Skip. }
  inversion E; subst; discriminate.
Qed.

Definition composition_env : TEnv := [(Src 0, (SLeaf 1, [(0, 0)])); (Src 1, (SLeaf 7, [(1, 0)]))].
Definition composition_after_drop : TEnv := [(Src 0, (SLeaf 1, [(0, 0)]))].

Example exclusive_branch_drops_once :
  texec composition_no_targets composition_no_targets composition_env [] [(0, 0); (1, 0)] 2
    (TIf (Src 0) (TDrop (Src 1)) (TDrop (Src 1)))
    composition_after_drop [] [(0, 0)] 2 [].
Proof.
  eapply TE_IfT with (cv := SLeaf 1) (bs := [(0, 0)]); try reflexivity.
  - unfold incl; simpl; intuition.
  - change composition_after_drop with (tremove (Src 1) composition_env).
    change [(0, 0)] with (free [(1, 0)] [(0, 0); (1, 0)]).
    eapply TE_Drop with (c := SLeaf 7) (bs := [(1, 0)]); try reflexivity.
    + unfold incl; simpl; intuition.
    + repeat constructor; simpl; intuition.
Qed.

Example sequential_double_drop_refused : forall rho beta H n tr,
  ~ texec composition_no_targets composition_no_targets composition_env [] [(0, 0); (1, 0)] 2
      (TSeq (TDrop (Src 1)) (TDrop (Src 1))) rho beta H n tr.
Proof.
  intros rho beta H n tr E. inversion E; subst.
  repeat match goal with
  | Ed : texec _ _ _ _ _ _ (TDrop _) _ _ _ _ _ |- _ => inversion Ed; subst; clear Ed
  end.
  simpl in *. congruence.
Qed.

Example emit_is_not_idempotent :
  ~ cleanup_equiv composition_no_targets composition_no_targets
      (TSeq (TEmit (Src 1)) (TEmit (Src 1))) (TEmit (Src 1)).
Proof.
  intro Eq. assert (E : texec composition_no_targets composition_no_targets
    composition_env [] [(0, 0); (1, 0)] 2 (TSeq (TEmit (Src 1)) (TEmit (Src 1)))
    composition_env [] [(0, 0); (1, 0)] 2 [SLeaf 7]).
  { apply Eq. eapply TE_Emit with (bs := [(1, 0)]); [reflexivity|unfold incl; simpl; intuition]. }
  inversion E; subst.
  repeat match goal with
  | Ee : texec _ _ _ _ _ _ (TEmit _) _ _ _ _ _ |- _ => inversion Ee; subst; clear Ee
  end.
  simpl in *. congruence.
Qed.

(* Drops of distinct variables commute exactly, so an implementation may
   emit one release set in any order. *)
Lemma tremove_cons : forall x k v r,
  tremove x ((k, v) :: r) = if tvar_eq_dec k x then tremove x r else (k, v) :: tremove x r.
Proof. reflexivity. Qed.

Lemma tremove_comm : forall x y rho, tremove x (tremove y rho) = tremove y (tremove x rho).
Proof.
  intros x y rho. induction rho as [|[k v] r IH]; [reflexivity|].
  rewrite !tremove_cons.
  destruct (tvar_eq_dec k y) as [Ey|Ey]; destruct (tvar_eq_dec k x) as [Ex|Ex];
    rewrite ?tremove_cons;
    repeat match goal with |- context[tvar_eq_dec k ?z] => destruct (tvar_eq_dec k z); try congruence end;
    try rewrite IH; reflexivity.
Qed.

Lemma free_comm : forall b1 b2 H, free b1 (free b2 H) = free b2 (free b1 H).
Proof.
  intros b1 b2 H. unfold free. induction H as [|h t IH]; simpl; [reflexivity|].
  destruct (bmem h b2) eqn:E2; destruct (bmem h b1) eqn:E1; simpl;
    try rewrite E1; try rewrite E2; simpl; try rewrite IH; reflexivity.
Qed.

Lemma free_incl_other : forall b1 b2 H,
  incl b1 H -> incl b2 (free b1 H) -> incl b1 (free b2 H).
Proof.
  intros b1 b2 H H1 H2 z Hz. unfold free. apply filter_In. split; [apply H1; exact Hz|].
  destruct (bmem z b2) eqn:E; [|reflexivity]. exfalso.
  apply bmem_true in E. specialize (H2 z E). unfold free in H2. apply filter_In in H2.
  destruct H2 as [_ F]. assert (Eb : bmem z b1 = true) by (apply bmem_true; exact Hz).
  rewrite Eb in F. discriminate.
Qed.

Lemma free_incl_sub : forall b1 b2 H, incl b2 (free b1 H) -> incl b2 H.
Proof. intros b1 b2 H Hs z Hz. specialize (Hs z Hz). unfold free in Hs. apply filter_In in Hs. apply Hs. Qed.

Lemma drop_drop_forward : forall tf tp x y rho beta H n rho' beta' H' n' tr,
  x <> y ->
  texec tf tp rho beta H n (TSeq (TDrop x) (TDrop y)) rho' beta' H' n' tr ->
  texec tf tp rho beta H n (TSeq (TDrop y) (TDrop x)) rho' beta' H' n' tr.
Proof.
  intros tf tp x y rho beta H n rho' beta' H' n' tr Hxy E.
  inversion E; subst.
  repeat match goal with
  | Ed : texec _ _ _ _ _ _ (TDrop _) _ _ _ _ _ |- _ => inversion Ed; subst; clear Ed
  end.
  match goal with
  | Lx : tlookup x rho = Some (?cx, ?bsx), Ix : incl ?bsx H, Nx : NoDup ?bsx,
    Ly : tlookup y (tremove x rho) = Some (?cy, ?bsy), Iy : incl ?bsy (free ?bsx H), Ny : NoDup ?bsy |- _ =>
      rewrite tlookup_tremove in Ly; destruct (tvar_eq_dec x y) as [F|_]; [congruence|];
      rewrite tremove_comm, free_comm;
      eapply TE_Seq;
        [ eapply TE_Drop; [exact Ly| exact (free_incl_sub _ _ _ Iy)| exact Ny]
        | eapply TE_Drop; [rewrite tlookup_tremove; destruct (tvar_eq_dec y x) as [F2|_];
                             [congruence| exact Lx]
                          | exact (free_incl_other _ _ _ Ix Iy)| exact Nx]
        | reflexivity ]
  end.
Qed.

Theorem drops_commute : forall tf tp x y, x <> y ->
  cleanup_equiv tf tp (TSeq (TDrop x) (TDrop y)) (TSeq (TDrop y) (TDrop x)).
Proof.
  intros tf tp x y Hxy rho beta H n rho' beta' H' n' tr. split; intro E.
  - apply drop_drop_forward; assumption.
  - apply drop_drop_forward; [congruence| assumption].
Qed.
