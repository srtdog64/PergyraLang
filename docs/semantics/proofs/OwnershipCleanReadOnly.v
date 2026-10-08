(* Read-only copy elision on the existing value language. A local alias is
   compiled away by root substitution (copy propagation); it is NOT a second
   owning descriptor or a runtime pointer/loan. The root's renamed uses remain
   visible to elab. Under value semantics an alias is observable only through
   a write, so the region check refuses exactly the statements that write the
   alias or the root; stores, pushes, copies and calls that only read them
   are accepted. *)
Require Import Stdlib.Lists.List Stdlib.Arith.PeanoNat Stdlib.Bool.Bool.
Require Import Stdlib.micromega.Lia.
Require Import OwnershipCleanCore OwnershipCleanComposition.
Import ListNotations.

Definition ro_name (alias root v : Var) := if Nat.eq_dec v alias then root else v.
Definition ro_writable (alias root dest : Var) :=
  negb (Nat.eqb dest alias || Nat.eqb dest root).

(* A statement is admitted when it does not write the alias or the root:
   not as a definition target, a push target, or an inout argument. SField
   still makes a fresh field VALUE. *)
Fixpoint ro_region (alias root : Var) (s : SStmt) : bool :=
  match s with
  | SSkip | SEmit _ => true
  | SDef dest _ _ | SField dest _ _ | SCopy dest _ | SPack dest _ | SPush dest _
  | SCall dest _ _ => ro_writable alias root dest
  | SCallIO _ z _ => ro_writable alias root z
  | SSeq a b | SIf _ a b => ro_region alias root a && ro_region alias root b
  | SWhile _ _ body => ro_region alias root body
  | SFocus _ _ _ _ | SUnpack _ _ | SRegion _ _ _ _ => false
  end.

Fixpoint ro_rename (alias root : Var) (s : SStmt) : SStmt :=
  let n := ro_name alias root in
  match s with
  | SSkip => SSkip
  | SDef dest f args => SDef dest f (map n args)
  | SCopy dest src => SCopy dest (n src)
  | SPack dest args => SPack dest (map n args)
  | SPush dest src => SPush dest (n src)
  | SField dest src i => SField dest (n src) i
  | SEmit v => SEmit (n v)
  | SSeq a b => SSeq (ro_rename alias root a) (ro_rename alias root b)
  | SIf c a b => SIf (n c) (ro_rename alias root a) (ro_rename alias root b)
  | SWhile c h b => SWhile (n c) (map n h) (ro_rename alias root b)
  | SCall dest g args => SCall dest g (map n args)
  | SCallIO g z args => SCallIO g z (map n args)
  | SFocus t y p b => SFocus t y p b
  | SUnpack y xs => SUnpack y xs
  | SRegion r f ys b => SRegion r f ys b
  end.

Inductive RORefusal := RODestinationIsRoot | RODestinationLiveOut |
  RODestinationBorrowed | ROUnsafeRegion.
Inductive ROAdmission := ROAccepted (rewritten : SStmt) | RORefused (reason : RORefusal).

Definition ro_admit alias root body L B : ROAdmission :=
  if Nat.eqb alias root then RORefused RODestinationIsRoot
  else if vmem alias L then RORefused RODestinationLiveOut
  else if vmem alias B then RORefused RODestinationBorrowed
  else if ro_region alias root body then ROAccepted (ro_rename alias root body)
  else RORefused ROUnsafeRegion.

Definition ro_agree alias root (original rewritten : SEnv) :=
  forall v, original v = rewritten (ro_name alias root v).

Lemma ro_writable_spec : forall a r d,
  ro_writable a r d = true -> d <> a /\ d <> r.
Proof.
  intros a r d H. unfold ro_writable in H.
  apply negb_true_iff in H. apply orb_false_iff in H.
  destruct H as [Ha Hr]. apply Nat.eqb_neq in Ha. apply Nat.eqb_neq in Hr. auto.
Qed.

Lemma ro_initial_agreement : forall a r sg v,
  a <> r -> sg r = Some v -> ro_agree a r (supd sg a v) sg.
Proof.
  intros a r sg v Hneq Hv z. unfold ro_name, supd.
  destruct (Nat.eq_dec z a); destruct (Nat.eq_dec a z); subst; congruence.
Qed.

Lemma ro_update_agreement : forall a r sg st d v,
  d <> a -> d <> r -> ro_agree a r sg st ->
  ro_agree a r (supd sg d v) (supd st d v).
Proof.
  intros a r sg st d v Hda Hdr Hagr z.
  unfold supd, ro_name. specialize (Hagr z). unfold ro_name in Hagr.
  destruct (Nat.eq_dec z a); destruct (Nat.eq_dec d z);
    destruct (Nat.eq_dec d r); subst; try congruence.
Qed.

Lemma ro_lookup_agreement : forall a r sg st args,
  ro_agree a r sg st ->
  slookup_all sg args = slookup_all st (map (ro_name a r) args).
Proof.
  intros a r sg st args H. induction args; simpl; [reflexivity|].
  rewrite (H a0), IHargs. reflexivity.
Qed.

Lemma ro_name_other : forall a r z, z <> a -> ro_name a r z = z.
Proof. intros a r z H. unfold ro_name. destruct (Nat.eq_dec z a); congruence. Qed.

Theorem ro_rename_execution : forall funs procs a r sg s sg' tr,
  sexec funs procs sg s sg' tr ->
  forall st, ro_region a r s = true -> ro_agree a r sg st ->
    exists st', sexec funs procs st (ro_rename a r s) st' tr /\ ro_agree a r sg' st'.
Proof.
  intros funs procs a r sg s sg' tr Run.
  induction Run; intros st Check Agree; simpl in Check; simpl.
  - exists st. split; [constructor|assumption].
  - apply ro_writable_spec in Check. destruct Check as [Hxa Hxr].
    exists (supd st x (f vs)). split.
    + apply SE_Def with (vs := vs). rewrite <- (ro_lookup_agreement a r sg st ys Agree). assumption.
    + apply ro_update_agreement; assumption.
  - apply ro_writable_spec in Check. destruct Check as [Hxa Hxr].
    exists (supd st x v). split.
    + apply SE_Copy. rewrite <- (Agree y). assumption.
    + apply ro_update_agreement; assumption.
  - apply ro_writable_spec in Check. destruct Check as [Hxa Hxr].
    exists (supd st x (SNode vs)). split.
    + apply SE_Pack. rewrite <- (ro_lookup_agreement a r sg st ys Agree). assumption.
    + apply ro_update_agreement; assumption.
  - apply ro_writable_spec in Check. destruct Check as [Hxa Hxr].
    exists (supd st x (SNode (cs ++ [v]))). split.
    + apply SE_Push.
      * rewrite <- (ro_name_other a r x Hxa), <- (Agree x). assumption.
      * rewrite <- (Agree y). assumption.
    + apply ro_update_agreement; assumption.
  - apply ro_writable_spec in Check. destruct Check as [Hxa Hxr].
    exists (supd st x v). split.
    + eapply SE_Field; [rewrite <- (Agree y); eassumption|eassumption].
    + apply ro_update_agreement; assumption.
  - exists st. split; [eapply SE_Emit; rewrite <- (Agree y); eassumption|assumption].
  - apply andb_true_iff in Check. destruct Check as [C1 C2].
    destruct (IHRun1 st C1 Agree) as [st1 [E1 A1]].
    destruct (IHRun2 st1 C2 A1) as [st2 [E2 A2]].
    exists st2. split; [eapply SE_Seq; eassumption|assumption].
  - apply andb_true_iff in Check. destruct Check as [C1 C2].
    destruct (IHRun st C1 Agree) as [st' [E A]]. exists st'. split; [|assumption].
    eapply SE_IfT; [rewrite <- (Agree c); eassumption|eassumption|eassumption].
  - apply andb_true_iff in Check. destruct Check as [C1 C2].
    destruct (IHRun st C2 Agree) as [st' [E A]]. exists st'. split; [|assumption].
    eapply SE_IfF; [rewrite <- (Agree c); eassumption|eassumption|eassumption].
  - exists st. split; [|assumption].
    eapply SE_WhileF; [rewrite <- (Agree c); eassumption|eassumption].
  - destruct (IHRun1 st Check Agree) as [st1 [E1 A1]].
    destruct (IHRun2 st1 Check A1) as [st2 [E2 A2]].
    exists st2. split; [|assumption].
    eapply SE_WhileT; [rewrite <- (Agree c); eassumption|eassumption|exact E1|exact E2|assumption].
  - (* a call reads its arguments' values; the callee frame is unchanged *)
    apply ro_writable_spec in Check. destruct Check as [Hxa Hxr].
    exists (supd st x v). split.
    + eapply SE_Call; [eassumption| rewrite <- (ro_lookup_agreement a r sg st ys Agree); eassumption|
                       eassumption| eassumption| eassumption].
    + apply ro_update_agreement; assumption.
  - (* an inout call on a third variable *)
    apply ro_writable_spec in Check. destruct Check as [Hza Hzr].
    exists (supd st z v). split.
    + eapply SE_CallIO; [eassumption| | rewrite <- (ro_lookup_agreement a r sg st ys Agree); eassumption|
                         eassumption| eassumption| eassumption].
      rewrite <- (ro_name_other a r z Hza), <- (Agree z). assumption.
    + apply ro_update_agreement; assumption.
  - (* a focus, an unpack, or a nested region in the region is refused *)
    discriminate.
  - discriminate.
  - discriminate.
Qed.

Lemma ro_admission_facts : forall a r body L B rewritten,
  ro_admit a r body L B = ROAccepted rewritten ->
  a <> r /\ vmem a L = false /\ vmem a B = false /\
  ro_region a r body = true /\ rewritten = ro_rename a r body.
Proof.
  intros. unfold ro_admit in H.
  destruct (Nat.eqb a r) eqn:Ear; try discriminate.
  destruct (vmem a L) eqn:EL; try discriminate.
  destruct (vmem a B) eqn:EB; try discriminate.
  destruct (ro_region a r body) eqn:ER; try discriminate.
  inversion H; subst. apply Nat.eqb_neq in Ear. auto.
Qed.

Theorem readonly_copy_elision : forall funs procs a r body L B rewritten sg sg' tr,
  ro_admit a r body L B = ROAccepted rewritten ->
  sexec funs procs sg (SSeq (SCopy a r) body) sg' tr ->
  exists st', sexec funs procs sg rewritten st' tr /\
    (forall v, v <> a -> sg' v = st' v).
Proof.
  intros funs procs a r body L B rewritten sg sg' tr Hadm Run.
  destruct (ro_admission_facts _ _ _ _ _ _ Hadm) as [Har [_ [_ [Check ->]]]].
  inversion Run; subst. match goal with
  | C : sexec _ _ _ (SCopy _ _) _ _ |- _ => inversion C; subst; clear C
  end. simpl in *.
  match goal with
  | E : sexec _ _ (supd sg a ?v) body sg' _ |- _ =>
      destruct (ro_rename_execution funs procs a r _ _ _ _ E sg Check)
        as [st' [Exec Agree]]
  end.
  - apply ro_initial_agreement; assumption.
  - exists st'. split; [assumption|]. intros q Hqa.
    specialize (Agree q). unfold ro_name in Agree.
    destruct (Nat.eq_dec q a); congruence.
Qed.

Theorem readonly_program_trace : forall funs procs prefix a r body rewritten sg sg' tr,
  ro_admit a r body [] [] = ROAccepted rewritten ->
  sexec funs procs sg (SSeq prefix (SSeq (SCopy a r) body)) sg' tr ->
  exists st', sexec funs procs sg (SSeq prefix rewritten) st' tr.
Proof.
  intros funs procs prefix a r body rewritten sg sg' tr Hadm Run.
  inversion Run; subst.
  match goal with
  | E : sexec _ _ ?mid (SSeq (SCopy a r) body) sg' _ |- _ =>
      destruct (readonly_copy_elision funs procs a r body [] [] rewritten mid sg' _ Hadm E)
        as [st' [E' _]]
  end.
  exists st'. eapply SE_Seq; [eassumption|eassumption|reflexivity].
Qed.

Definition elab_readonly_program M prefix a r body : option (TStmt * list Var) :=
  (* Do not erase an invalid original read, even if the alias is never used.
     The original closed program must first pass canonical admission. *)
  match elab_normalized M (SSeq prefix (SSeq (SCopy a r) body)) [] [] with
  | Some (_, []) =>
      match ro_admit a r body [] [] with
      | ROAccepted rewritten => elab_normalized M (SSeq prefix rewritten) [] []
      | RORefused _ => None
      end
  | _ => None
  end.

Theorem readonly_elaboration_frees_everything : forall M funs procs,
  (forall g d, funs g = Some d -> elab_fun M g d <> None) ->
  (forall g d, procs g = Some d -> elab_proc M g d <> None) ->
  forall prefix a r body t sg sg' tr,
    elab_readonly_program M prefix a r body = Some (t, []) ->
    sexec funs procs sg (SSeq prefix (SSeq (SCopy a r) body)) sg' tr ->
    exists n', texec (tfuns_of M funs) (tprocs_of M procs) [] [] [] 0 t [] [] [] n' tr.
Proof.
  intros M funs procs Hf Hp prefix a r body t sg sg' tr Hel Run.
  unfold elab_readonly_program in Hel.
  destruct (elab_normalized M (SSeq prefix (SSeq (SCopy a r) body)) [] [])
    as [[original [|v vs]]|] eqn:Original; try discriminate.
  destruct (ro_admit a r body [] []) eqn:Adm; try discriminate.
  destruct (readonly_program_trace funs procs prefix a r body rewritten sg sg' tr Adm Run)
    as [st' E].
  eapply normalized_closed_program_frees_everything; eassumption.
Qed.

Theorem readonly_requires_original_admission : forall M prefix a r body t Lin,
  elab_readonly_program M prefix a r body = Some (t, Lin) ->
  exists original, elab_normalized M (SSeq prefix (SSeq (SCopy a r) body)) [] [] = Some (original, []).
Proof.
  intros M prefix a r body t Lin E. unfold elab_readonly_program in E.
  destruct (elab_normalized M (SSeq prefix (SSeq (SCopy a r) body)) [] [])
    as [[original [|v vs]]|] eqn:Original; try discriminate.
  eexists; reflexivity.
Qed.

(* This certificate measures a path, not both sides of a branch. Unknown
   loop/call counts and unequal arm costs fail closed with None. *)
Fixpoint fixed_allocations (t : TStmt) : option nat :=
  match t with
  | TDef _ _ _ | TCopy _ _ | TPack _ _ | TField _ _ _ => Some 1
  | TSeq a b => match fixed_allocations a, fixed_allocations b with
      | Some x, Some y => Some (x + y) | _, _ => None end
  | TIf _ a b => match fixed_allocations a, fixed_allocations b with
      | Some x, Some y => if Nat.eqb x y then Some x else None | _, _ => None end
  | TWhile _ _ | TCall _ _ _ _ _ => None
  | TFocus _ _ _ b => fixed_allocations b
  | _ => Some 0
  end.

Theorem fixed_allocations_sound : forall tf tp rho beta H n t rho' beta' H' n' tr,
  texec tf tp rho beta H n t rho' beta' H' n' tr ->
  forall k, fixed_allocations t = Some k -> n' = n + k.
Proof.
  intros tf tp rho beta H n t rho' beta' H' n' tr Run.
  induction Run; intros budget Count; simpl in Count; try discriminate;
    try solve [inversion Count; lia].
  - destruct (fixed_allocations t1) as [k1|] eqn:C1; try discriminate.
    destruct (fixed_allocations t2) as [k2|] eqn:C2; try discriminate.
    inversion Count; subst. specialize (IHRun1 k1 eq_refl). specialize (IHRun2 k2 eq_refl). lia.
  - destruct (fixed_allocations t1) as [k1|] eqn:C1; try discriminate.
    destruct (fixed_allocations t2) as [k2|] eqn:C2; try discriminate.
    destruct (Nat.eqb k1 k2) eqn:E; try discriminate. inversion Count; subst.
    apply IHRun; reflexivity.
  - destruct (fixed_allocations t1) as [k1|] eqn:C1; try discriminate.
    destruct (fixed_allocations t2) as [k2|] eqn:C2; try discriminate.
    destruct (Nat.eqb k1 k2) eqn:E; try discriminate.
    apply Nat.eqb_eq in E. inversion Count; subst. apply IHRun; reflexivity.
  - (* a focus allocates exactly what its body allocates *)
    apply IHRun. exact Count.
Qed.

Definition ro_demo_prefix flag :=
  SSeq (SDef 0 (fun _ => SLeaf 7) []) (SDef 2 (fun _ => SLeaf flag) []).
Definition ro_demo_body := SSeq (SIf 2 (SEmit 1) (SEmit 0)) (SEmit 0).
Definition ro_demo_program flag := SSeq (ro_demo_prefix flag) (SSeq (SCopy 1 0) ro_demo_body).

Lemma ro_demo_source : forall flag, exists sg',
  sexec no_funs no_procs sempty (ro_demo_program flag) sg' [SLeaf 7; SLeaf 7].
Proof.
  intros flag. eexists. unfold ro_demo_program, ro_demo_prefix, ro_demo_body.
  eapply SE_Seq.
  - eapply SE_Seq; [apply SE_Def with (vs := []); reflexivity|
      apply SE_Def with (vs := []); reflexivity|reflexivity].
  - eapply SE_Seq; [eapply SE_Copy; reflexivity| |reflexivity].
    eapply SE_Seq.
    + destruct (truthy (SLeaf flag)) eqn:Guard.
      * eapply SE_IfT; [reflexivity|exact Guard|eapply SE_Emit; reflexivity].
      * eapply SE_IfF; [reflexivity|exact Guard|eapply SE_Emit; reflexivity].
    + eapply SE_Emit; reflexivity.
    + reflexivity.
  - reflexivity.
Qed.

Example ro_demo_costs : forall flag, exists original optimized,
  elab_normalized no_summaries (ro_demo_program flag) [] [] = Some (original, []) /\
  elab_readonly_program no_summaries (ro_demo_prefix flag) 1 0 ro_demo_body = Some (optimized, []) /\
  fixed_allocations original = Some 3 /\ fixed_allocations optimized = Some 2 /\
  count_copies original = 1 /\ count_copies optimized = 0.
Proof. intros. do 2 eexists. repeat split; reflexivity. Qed.

Theorem ro_demo_one_fewer_allocation : forall flag, exists original optimized,
  elab_normalized no_summaries (ro_demo_program flag) [] [] = Some (original, []) /\
  elab_readonly_program no_summaries (ro_demo_prefix flag) 1 0 ro_demo_body = Some (optimized, []) /\
  texec (tfuns_of no_summaries no_funs) (tprocs_of no_summaries no_procs)
    [] [] [] 0 original [] [] [] 3 [SLeaf 7; SLeaf 7] /\
  texec (tfuns_of no_summaries no_funs) (tprocs_of no_summaries no_procs)
    [] [] [] 0 optimized [] [] [] 2 [SLeaf 7; SLeaf 7].
Proof.
  intros flag. destruct (ro_demo_costs flag) as [original [optimized [Eo [Ep [Co [Cp _]]]]]].
  destruct (ro_demo_source flag) as [sg' Run].
  destruct (normalized_closed_program_frees_everything no_summaries no_funs no_procs
    (no_funs_ok no_summaries) (no_procs_ok no_summaries) _ _ _ _ _ Eo Run) as [n1 R1].
  destruct (readonly_elaboration_frees_everything no_summaries no_funs no_procs
    (no_funs_ok no_summaries) (no_procs_ok no_summaries) _ _ _ _ _ _ _ _ Ep Run) as [n2 R2].
  pose proof (fixed_allocations_sound _ _ _ _ _ _ _ _ _ _ _ _ R1 3 Co) as N1.
  pose proof (fixed_allocations_sound _ _ _ _ _ _ _ _ _ _ _ _ R2 2 Cp) as N2.
  simpl in N1, N2. subst n1 n2. exists original, optimized. auto.
Qed.

(* Writes to the alias or the root are refused wherever they occur. *)
Example ro_fail_closed_controls :
  ro_admit 1 0 (SDef 0 (fun _ => SLeaf 9) []) [] [] = RORefused ROUnsafeRegion /\
  ro_admit 1 0 (SDef 1 (fun _ => SLeaf 9) []) [] [] = RORefused ROUnsafeRegion /\
  ro_admit 1 0 (SPush 0 1) [] [] = RORefused ROUnsafeRegion /\
  ro_admit 1 0 (SPush 1 3) [] [] = RORefused ROUnsafeRegion /\
  ro_admit 1 0 (SCopy 0 3) [] [] = RORefused ROUnsafeRegion /\
  ro_admit 1 0 (SCall 1 0 [3]) [] [] = RORefused ROUnsafeRegion /\
  ro_admit 1 0 (SCallIO 0 0 [3]) [] [] = RORefused ROUnsafeRegion /\
  ro_admit 1 0 (SIf 2 (SEmit 1) (SPack 0 [3])) [] [] = RORefused ROUnsafeRegion /\
  ro_admit 1 0 (SEmit 1) [1] [] = RORefused RODestinationLiveOut /\
  ro_admit 1 0 (SEmit 1) [] [1] = RORefused RODestinationBorrowed /\
  ro_admit 1 1 (SEmit 1) [] [] = RORefused RODestinationIsRoot /\
  elab_readonly_program no_summaries SSkip 1 0 SSkip = None.
Proof. repeat split; reflexivity. Qed.

(* A denied root write is a semantic requirement, not just a conservative
   spelling check: unchecked renaming would change the observed value. *)
Definition ro_mutation_body := SSeq (SDef 0 (fun _ => SLeaf 9) []) (SEmit 1).

Example ro_mutation_changes_observation : exists original_final naive_final,
  sexec no_funs no_procs sempty
    (SSeq (SDef 0 (fun _ => SLeaf 7) []) (SSeq (SCopy 1 0) ro_mutation_body))
    original_final [SLeaf 7] /\
  sexec no_funs no_procs sempty
    (SSeq (SDef 0 (fun _ => SLeaf 7) []) (ro_rename 1 0 ro_mutation_body))
    naive_final [SLeaf 9] /\
  ro_admit 1 0 ro_mutation_body [] [] = RORefused ROUnsafeRegion.
Proof.
  do 2 eexists. split.
  - unfold ro_mutation_body. eapply SE_Seq.
    + apply SE_Def with (vs := []); reflexivity.
    + eapply SE_Seq; [eapply SE_Copy; reflexivity| |reflexivity].
      eapply SE_Seq; [apply SE_Def with (vs := []); reflexivity|
        eapply SE_Emit; reflexivity|reflexivity].
    + reflexivity.
  - split; [|reflexivity]. unfold ro_mutation_body. cbn [ro_rename ro_name].
    eapply SE_Seq; [apply SE_Def with (vs := []); reflexivity| |reflexivity].
    eapply SE_Seq; [apply SE_Def with (vs := []); reflexivity|
      eapply SE_Emit; reflexivity|reflexivity].
Qed.

Example ro_unknown_cost_is_not_zero :
  fixed_allocations (TWhile (Src 0) TSkip) = None /\
  fixed_allocations (TCall KFun (Src 0) 9 [] []) = None /\
  fixed_allocations (TIf (Src 0) TSkip (TCopy (Src 1) (Src 2))) = None.
Proof. repeat split; reflexivity. Qed.

(* Reads inside stores, pushes, copies and calls are accepted: the rewrite
   only renames them. *)
Example ro_value_semantics_accepts :
  ro_admit 1 0 (SPack 3 [1]) [] [] = ROAccepted (SPack 3 [0]) /\
  ro_admit 1 0 (SPush 3 1) [] [] = ROAccepted (SPush 3 0) /\
  ro_admit 1 0 (SCopy 3 1) [] [] = ROAccepted (SCopy 3 0) /\
  ro_admit 1 0 (SCall 3 0 [1]) [] [] = ROAccepted (SCall 3 0 [0]) /\
  ro_admit 1 0 (SCallIO 0 3 [1]) [] [] = ROAccepted (SCallIO 0 3 [0]).
Proof. repeat split; reflexivity. Qed.

(* A call in the overlap: the earlier bounded rule refused it; now it is
   accepted. The root is still needed after the sink call, so the copy moves
   to the call site; the rewrite does not add a copy. *)
Definition ro_call_funs : SFunTable :=
  fun g => if Nat.eqb g 0 then Some ([0], SPack 1 [0], 1) else None.
Definition ro_call_prefix := SDef 0 (fun _ => SLeaf 7) [].
Definition ro_call_body := SSeq (SCall 2 0 [1]) (SSeq (SEmit 2) (SEmit 0)).

Example ro_call_in_overlap :
  ro_admit 1 0 ro_call_body [] [] = ROAccepted (SSeq (SCall 2 0 [0]) (SSeq (SEmit 2) (SEmit 0))) /\
  (exists t, elab (infer_modes ro_call_funs no_procs no_summaries)
               (SSeq ro_call_prefix (SSeq (SCopy 1 0) ro_call_body)) [] [] = Some (t, []) /\
             count_copies t = 1) /\
  (exists t, elab_readonly_program (infer_modes ro_call_funs no_procs no_summaries)
               ro_call_prefix 1 0 ro_call_body = Some (t, []) /\ count_copies t = 1).
Proof. split; [reflexivity| split; eexists; split; reflexivity]. Qed.
