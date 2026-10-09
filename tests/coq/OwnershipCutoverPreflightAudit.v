(* Independent consumers of the importing preflight, not compiler closure. *)
Require Import Stdlib.Lists.List Stdlib.Arith.PeanoNat.
Require Import OwnershipTeardownAuthority.
Require Import OwnershipCleanCore OwnershipCleanExits.
Require Import OwnershipCleanCallRecovery OwnershipCleanViews.
Require Import OwnershipCleanCallLowering OwnershipCleanViewScope.
Require Import OwnershipCleanDirectControl.
Import ListNotations.

Definition audit_uniform_recovery : forall M funs procs,
  (forall g d, funs g = Some d -> elab_fun M g d <> None) ->
  (forall g d, procs g = Some d -> elab_proc M g d <> None) ->
  forall body K B t Lin sg sg' tr o rho beta H n R,
  xelab M body K (recovery_exits K) B = Some (t, Lin) ->
  xsexec funs procs sg body sg' tr o -> continuing_outcome o = true ->
  INV rho beta H n R -> CORR sg rho beta Lin B ->
  exists rho' beta' H' n',
    xtexec (tfuns_of M funs) (tprocs_of M procs) rho beta H n t rho' beta' H' n' tr o /\
    INV rho' beta' H' n' R /\ CORR sg' rho' beta' K B :=
  continuing_exit_recovers_live_set.

Definition audit_write_frame : forall rho beta H n R root index k,
  INV rho beta H n R -> INV (scalar_write_owner rho root index k) beta H n R :=
  scalar_write_preserves_cleanup_invariant.

Definition audit_write_observed : forall s rho H id index k rho',
  write_view s rho H id index k = Some rho' -> read_view s rho' H id index = Some (SLeaf k) :=
  admitted_view_write_is_write_through.

Definition audit_ticket_not_resurrected : forall s after id,
  ViewSchedule s after -> id < view_next s -> find_view id (view_rows s) = None ->
  id < view_next after /\ find_view id (view_rows after) = None := old_absent_ticket_never_reissued.

Definition audit_ended_schedule : forall s id ended later,
  views_wf s -> end_view s id = Some ended -> ViewSchedule ended later ->
  find_view id (view_rows later) = None := ended_ticket_never_usable_again.

Definition audit_backing_drop : forall tf tp s rho beta H n R root rho' H',
  INV rho beta H n R -> drop_view_backing s rho H root = Some (rho',H') ->
  texec tf tp rho beta H n (TDrop root) rho' beta H' n [] /\ INV rho' beta H' n R :=
  admitted_backing_drop_refines_core.

Definition audit_source_view_frame : forall sg rho beta L B x c bs index k,
  CORR sg rho beta L B -> tlookup (Src x) rho = Some (c,bs) ->
  CORR (supd sg x (scalar_put c index k))
    (scalar_write_owner rho (Src x) index k) beta L B := scalar_write_preserves_source_frame.

Definition audit_admitted_source_view_frame : forall s sg rho beta H L B id index k rho' view x,
  find_view id (view_rows s) = Some view -> view_owner view = Src x ->
  CORR sg rho beta L B -> write_view s rho H id index k = Some rho' ->
  exists c, sg x = Some c /\
    CORR (supd sg x (scalar_put c (view_start view + index) k)) rho' beta L B :=
  admitted_view_write_has_source_frame.

Definition audit_admitted_storage_frame : forall s rho H id index k rho',
  write_view s rho H id index k = Some rho' ->
  dom rho' = dom rho /\ heap_of rho' = heap_of rho /\
  forall beta n R, INV rho beta H n R -> INV rho' beta H n R :=
  admitted_view_write_preserves_storage.

Definition audit_alias_refusal : forall scope g bundle ios args packet x,
  In x ios -> In x args -> normalize_multi_call scope g bundle ios args packet = None :=
  inout_argument_alias_refused.

Definition audit_inout_preservation_filter : forall scope bundle ios args packet B,
  multi_call_admitted scope bundle ios args packet = true ->
  (forall x, In x ios -> ~ In x B) -> filter (keep (bundle :: args) B) ios = [] :=
  admitted_inout_pack_has_no_preservation_copies.

Definition audit_first_pack_elaboration : forall M scope bundle ios args packet B t Lin,
  multi_call_admitted scope bundle ios args packet = true ->
  (forall x, In x ios -> ~ In x B) ->
  elab M (SPack bundle ios) (bundle :: args) B = Some (t,Lin) -> count_copies t = 0 :=
  admitted_first_pack_has_no_copies.

Example output_admission_controls :
  (exists t K, compile_recovery_package no_summaries [] ONorm [0;1] 2 3 4 5 = Some (t,K)) /\
  (exists t K, compile_recovery_package no_summaries [] ORet [0;1] 2 3 4 5 = Some (t,K)) /\
  (exists t K, compile_recovery_package no_summaries [] OErr [0;1] 2 3 4 5 = Some (t,K)) /\
  compile_recovery_package no_summaries [] OBrk [0;1] 2 3 4 5 = None /\
  compile_recovery_package no_summaries [] OCont [0;1] 2 3 4 5 = None /\
  compile_recovery_package no_summaries [] OErr [0;0] 2 3 4 5 = None /\
  compile_recovery_package no_summaries [] OErr [0;1] 1 3 4 5 = None /\
  compile_recovery_package no_summaries [] OErr [0;1] 2 0 4 5 = None.
Proof. repeat split; try reflexivity; eexists; eexists; reflexivity. Qed.

Definition audit_proc_body : SStmt :=
  SSeq (SEmit 13) (SSeq (SUnpack 10 [11;12])
    (SSeq (SDef 11 (fun _ => SLeaf 9) [])
      (SSeq (SDef 14 (fun _ => SLeaf 3) []) (SPack 10 [11;12;14])))).
Definition audit_procs : SProcTable :=
  fun g => if Nat.eqb g 0 then Some (10, [13], audit_proc_body) else None.
Definition audit_modes : Modes :=
  {| fmodes := fun _ => None;
     pmodes := fun g => if Nat.eqb g 0 then Some [false] else None |}.

Lemma audit_procs_ok : forall g d, audit_procs g = Some d -> elab_proc audit_modes g d <> None.
Proof.
  intros g d E. unfold audit_procs in E.
  destruct (Nat.eqb g 0) eqn:Eg; [|discriminate]. inversion E; subst.
  apply Nat.eqb_eq in Eg; subst. vm_compute; discriminate.
Qed.

Example caller_refusal_controls :
  normalize_multi_call [0;1;3] 0 7 [0;0] [] 2 = None /\
  normalize_multi_call [0;1;3] 0 7 [0;1] [] 0 = None /\
  normalize_multi_call [0;1;3] 0 0 [0;1] [] 2 = None /\
  normalize_multi_call [0;1;3] 0 7 [0;1] [0] 2 = None /\
  normalize_multi_call [0;1;3] 0 7 [0;1] [7] 2 = None /\
  normalize_multi_call [0;1;3] 0 7 [0;1] [2] 2 = None /\
  normalize_multi_call [0;1;9] 0 7 [0;1] [] 9 = None /\
  normalize_multi_call [0;1;7] 0 7 [0;1] [] 2 = None /\
  normalize_multi_call [0] 0 7 [0;1] [] 2 = None /\
  normalize_multi_call [0;1] 0 7 [0;1] [3] 2 = None /\
  elab no_summaries (multi_call_body 0 7 [0;1] [] 2) [] [] = None.
Proof. repeat split; reflexivity. Qed.

Definition audit_proc_input := supd (supd sempty 13 (SLeaf 42)) 10 (SNode [SLeaf 1;SLeaf 2]).
Definition audit_proc_output :=
  supd (supd (supd_list audit_proc_input [11;12] [SLeaf 1;SLeaf 2]) 11 (SLeaf 9)) 14 (SLeaf 3).

Lemma audit_proc_exec : sexec no_funs audit_procs audit_proc_input audit_proc_body
  (supd audit_proc_output 10 (SNode [SLeaf 9;SLeaf 2;SLeaf 3])) [SLeaf 42].
Proof.
  unfold audit_proc_body. eapply SE_Seq with (tr1 := [SLeaf 42]) (tr2 := []);
    [apply SE_Emit; reflexivity| |reflexivity].
  eapply SE_Seq with (tr1 := []) (tr2 := []).
  - eapply SE_Unpack; reflexivity.
  - eapply SE_Seq with (tr1 := []) (tr2 := []).
    + eapply SE_Def with (vs := []); reflexivity.
    + eapply SE_Seq with (tr1 := []) (tr2 := []).
      * eapply SE_Def with (vs := []); reflexivity.
      * eapply SE_Pack; reflexivity.
      * reflexivity.
    + reflexivity.
  - reflexivity.
Qed.

Definition audit_caller_input := supd (supd (supd sempty 0 (SLeaf 1)) 1 (SLeaf 2)) 3 (SLeaf 42).
Definition audit_caller_output :=
  supd_list (supd (supd audit_caller_input 7 (SNode [SLeaf 1;SLeaf 2])) 7
    (SNode [SLeaf 9;SLeaf 2;SLeaf 3])) [0;1;2] [SLeaf 9;SLeaf 2;SLeaf 3].
Definition audit_caller :=
  SSeq (SDef 0 (fun _ => SLeaf 1) [])
    (SSeq (SDef 1 (fun _ => SLeaf 2) [])
      (SSeq (SDef 3 (fun _ => SLeaf 42) [])
        (SSeq (multi_call_body 0 7 [0;1] [3] 2)
          (SSeq (SEmit 0) (SSeq (SEmit 1) (SEmit 2)))))).

Example borrowed_caller_uses_admitted_normalization :
  normalize_multi_call [0;1;3] 0 7 [0;1] [3] 2 = Some (multi_call_body 0 7 [0;1] [3] 2) /\
  filter (keep [7;3] [3]) [0;1] = [].
Proof. split; reflexivity. Qed.

Lemma audit_caller_source : sexec no_funs audit_procs sempty audit_caller audit_caller_output
  [SLeaf 42;SLeaf 9;SLeaf 2;SLeaf 3].
Proof.
  unfold audit_caller. eapply SE_Seq with (tr1 := []) (tr2 := [SLeaf 42;SLeaf 9;SLeaf 2;SLeaf 3]).
  - eapply SE_Def with (vs := []); reflexivity.
  - eapply SE_Seq with (tr1 := []) (tr2 := [SLeaf 42;SLeaf 9;SLeaf 2;SLeaf 3]).
    + eapply SE_Def with (vs := []); reflexivity.
    + eapply SE_Seq with (tr1 := []) (tr2 := [SLeaf 42;SLeaf 9;SLeaf 2;SLeaf 3]);
        [eapply SE_Def with (vs := []); reflexivity| |reflexivity].
      eapply SE_Seq with (sg2 := audit_caller_output) (tr1 := [SLeaf 42]) (tr2 := [SLeaf 9;SLeaf 2;SLeaf 3]).
      * eapply multi_call_pack_call_unpack with (scope := [0;1;3]) (g := 0) (bundle := 7) (ios := [0;1]) (args := [3])
          (packet := 2) (normalized := multi_call_body 0 7 [0;1] [3] 2)
          (sgc := supd audit_proc_output 10 (SNode [SLeaf 9;SLeaf 2;SLeaf 3]))
          (values := [SLeaf 1;SLeaf 2]) (vs := [SLeaf 42]) (io := 10) (ps := [13])
          (recovered := [SLeaf 9;SLeaf 2]) (outcome := SLeaf 3)
          (body := audit_proc_body); try reflexivity; try (simpl; intuition discriminate).
        exact audit_proc_exec.
      * eapply SE_Seq with (tr1 := [SLeaf 9]) (tr2 := [SLeaf 2;SLeaf 3]).
        -- apply SE_Emit; reflexivity.
        -- eapply SE_Seq with (tr1 := [SLeaf 2]) (tr2 := [SLeaf 3]);
             try (apply SE_Emit; reflexivity); reflexivity.
        -- reflexivity.
      * reflexivity.
    + reflexivity.
  - reflexivity.
Qed.

Definition audit_exit_prefix : SStmt :=
  SSeq (SDef 0 (fun _ => SLeaf 7) [])
    (SSeq (SDef 1 (fun _ => SLeaf 8) [])
      (SSeq (SDef 2 (fun _ => SLeaf 9) []) (SDef 6 (fun _ => SLeaf 66) []))).
Definition audit_exit_env :=
  supd (supd (supd (supd sempty 0 (SLeaf 7)) 1 (SLeaf 8)) 2 (SLeaf 9)) 6 (SLeaf 66).
Definition audit_exit_body o := XSeq (XS audit_exit_prefix)
  (match o with ONorm => XS SSkip | ORet => XReturn | OErr => XThrow
                | OBrk => XBreak | OCont => XContinue end).

Lemma audit_exit_body_source : forall o, continuing_outcome o = true ->
  xsexec no_funs no_procs sempty (audit_exit_body o) audit_exit_env [] o.
Proof.
  intros o Ho. unfold audit_exit_body. eapply XE_SeqN with (tr1 := []) (tr2 := []).
  - apply XE_S. unfold audit_exit_prefix, audit_exit_env.
    eapply SE_Seq with (tr1 := []) (tr2 := []); [eapply SE_Def with (vs := []); reflexivity| |reflexivity].
    eapply SE_Seq with (tr1 := []) (tr2 := []); [eapply SE_Def with (vs := []); reflexivity| |reflexivity].
    eapply SE_Seq with (tr1 := []) (tr2 := []);
      [eapply SE_Def with (vs := []); reflexivity|eapply SE_Def with (vs := []); reflexivity|reflexivity].
  - destruct o; simpl in Ho; try discriminate; constructor; constructor.
  - reflexivity.
Qed.

Definition audit_switch_body := XSeq (XS audit_exit_prefix) (XIf 9 XReturn XThrow).
Definition audit_switch_input (choose : bool) := supd sempty 9 (SLeaf (if choose then 1 else 0)).
Definition audit_switch_output (choose : bool) :=
  supd (supd (supd (supd (audit_switch_input choose) 0 (SLeaf 7)) 1 (SLeaf 8)) 2 (SLeaf 9)) 6 (SLeaf 66).

Lemma audit_switch_source : forall choose,
  xsexec no_funs no_procs (audit_switch_input choose) audit_switch_body
    (audit_switch_output choose) [] (if choose then ORet else OErr).
Proof.
  intros choose. unfold audit_switch_body. eapply XE_SeqN with (tr1 := []) (tr2 := []).
  - apply XE_S. unfold audit_exit_prefix, audit_switch_output.
    eapply SE_Seq with (tr1 := []) (tr2 := []); [eapply SE_Def with (vs := []); reflexivity| |reflexivity].
    eapply SE_Seq with (tr1 := []) (tr2 := []); [eapply SE_Def with (vs := []); reflexivity| |reflexivity].
    eapply SE_Seq with (tr1 := []) (tr2 := []);
      [eapply SE_Def with (vs := []); reflexivity|eapply SE_Def with (vs := []); reflexivity|reflexivity].
  - destruct choose.
    + eapply XE_IfT with (v := SLeaf 1); [reflexivity|reflexivity|constructor].
    + eapply XE_IfF with (v := SLeaf 0); [reflexivity|reflexivity|constructor].
  - reflexivity.
Qed.

Example adapter_control_and_borrow_controls :
  compile_recovery_adapter no_summaries [] (XSeq (XS SSkip) XBreak) [0;1] 2 3 4 5 = None /\
  compile_recovery_adapter no_summaries [] (XSeq (XS SSkip) XContinue) [0;1] 2 3 4 5 = None /\
  loop_control_bound false (XLoop 9 [] XBreak) = true /\
  (exists a, compile_recovery_adapter no_summaries [9] audit_switch_body [0;1] 2 3 4 5 = Some (a,[9])) /\
  compile_recovery_package no_summaries [0] OErr [0;1] 2 3 4 5 = None.
Proof. repeat split; try reflexivity; eexists; reflexivity. Qed.

Lemma audit_empty_invariant : INV [] [] [] 0 [].
Proof.
  split; [constructor|split; [constructor|split; [constructor|split; [|split; [|split]]]]].
  - intros b [].
  - intros z c bs F; discriminate.
  - intros z c bs F; discriminate.
  - intros z c bs F; discriminate.
Qed.

Lemma audit_empty_correspondence : CORR sempty [] [] [] [].
Proof.
  split; [|split; [|split; [|split; [|split]]]].
  - intros x. simpl. split; [intros F; exfalso; apply F; reflexivity|intros [[] _]].
  - intros x. simpl. split; [intros F; exfalso; apply F; reflexivity|intros [[] _]].
  - intros; reflexivity.
  - intros; reflexivity.
  - intros z c bs F; discriminate.
  - intros z c bs F; discriminate.
Qed.

Theorem single_adapter_executes_return_and_error : exists a,
  compile_recovery_adapter no_summaries [] audit_switch_body [0;1] 2 3 4 5 = Some (a,[9]) /\
  forall choose : bool, exists rho0 H0 n0 rho1 beta1 H1 n1 bs,
    texec (tfuns_of no_summaries no_funs) (tprocs_of no_summaries no_procs)
      [] [] [] 0 (TDef (Src 9) (fun _ => SLeaf (if choose then 1 else 0)) []) rho0 [] H0 n0 [] /\
    recovery_exec (tfuns_of no_summaries no_funs) (tprocs_of no_summaries no_procs)
      rho0 [] H0 n0 a rho1 beta1 H1 n1 [] /\ INV rho1 beta1 H1 n1 [] /\
    tlookup (Src 5) rho1 = Some (recovery_value (if choose then ORet else OErr)
      [SLeaf 7;SLeaf 8] (SLeaf 9),bs).
Proof.
  assert (Plan : exists a,
    compile_recovery_adapter no_summaries [] audit_switch_body [0;1] 2 3 4 5 = Some (a,[9]))
    by (eexists; reflexivity).
  destruct Plan as [a Ea]. exists a. split; [exact Ea|]. intros choose.
  destruct (core_def (tfuns_of no_summaries no_funs) (tprocs_of no_summaries no_procs)
    sempty [] [] [] 0 [] 9 (fun _ => SLeaf (if choose then 1 else 0)) [] [9] [] []
    audit_empty_invariant audit_empty_correspondence (fun H => H) (fun H => H) eq_refl)
    as [rho0 [H0 [n0 [Ed [I0 C0]]]]].
  change (CORR (audit_switch_input choose) rho0 [] [9] []) in C0.
  destruct (one_compiled_adapter_recovers_all_continuing_outcomes no_summaries no_funs no_procs
    (no_funs_ok no_summaries) (no_procs_ok no_summaries)
    [] audit_switch_body [0;1] 2 3 4 5 a [9] (audit_switch_input choose) (audit_switch_output choose)
    [SLeaf 7;SLeaf 8] (SLeaf 9) [] (if choose then ORet else OErr) rho0 [] H0 n0 [] Ea
    (audit_switch_source choose) eq_refl eq_refl I0 C0)
    as [rho1 [beta1 [H1 [n1 [sg1 [E1 [I1 [C1 V1]]]]]]]].
  destruct (corr_owned sg1 rho1 beta1 [5] [] 5 C1 (or_introl eq_refl) (fun H => H))
    as [v [bs [Ev Es]]].
  assert (v = recovery_value (if choose then ORet else OErr) [SLeaf 7;SLeaf 8] (SLeaf 9)) by congruence.
  subst v. exists rho0,H0,n0,rho1,beta1,H1,n1,bs.
  split; [exact Ed|]. split; [exact E1|]. split; assumption.
Qed.

Example output_shape_and_dispatch_controls :
  decode_recovery_value 2 (recovery_value OErr [SLeaf 7;SLeaf 8] (SLeaf 9)) =
    Some ([SLeaf 7;SLeaf 8],0,SLeaf 9) /\
  decode_recovery_value 1 (recovery_value OErr [SLeaf 7;SLeaf 8] (SLeaf 9)) = None /\
  decode_recovery_value 2 (SNode [SLeaf 7;SLeaf 8;SLeaf 9]) = None /\
  decode_recovery_value 2 (SNode [SLeaf 7;SLeaf 8;SNode [SLeaf 2;SLeaf 9]]) = None /\
  decode_recovery_value 2 (SLeaf 9) = None /\
  dispatch_recovery sempty [0;1] (recovery_value OErr [SLeaf 7;SLeaf 8] (SLeaf 9))
    (fun _ _ => None) (fun restored _ => slookup_all restored [0;1]) = Some (Some [SLeaf 7;SLeaf 8]) /\
  restore_recovery sempty [0;0] (recovery_value OErr [SLeaf 7;SLeaf 8] (SLeaf 9)) = None.
Proof. repeat split; reflexivity. Qed.

Theorem normal_return_and_handled_error_execute_recovery : forall o,
  continuing_outcome o = true ->
  exists tbody tpack rho1 beta1 H1 n1 rho2 beta2 H2 n2 bs,
    xtexec (tfuns_of no_summaries no_funs) (tprocs_of no_summaries no_procs)
      [] [] [] 0 tbody rho1 beta1 H1 n1 [] o /\
    texec (tfuns_of no_summaries no_funs) (tprocs_of no_summaries no_procs)
      rho1 beta1 H1 n1 tpack rho2 beta2 H2 n2 [] /\
    INV rho2 beta2 H2 n2 [] /\
    tlookup (Src 5) rho2 = Some (recovery_value o [SLeaf 7;SLeaf 8] (SLeaf 9),bs).
Proof.
  intros o Ho.
  assert (Plans : exists tpack K tbody,
    compile_recovery_package no_summaries [] o [0;1] 2 3 4 5 = Some (tpack,K) /\
    xelab no_summaries (audit_exit_body o) K (recovery_exits K) [] = Some (tbody,[])).
  { destruct o; simpl in Ho; try discriminate; eexists; eexists; eexists; split; reflexivity. }
  destruct Plans as [tpack [K [tbody [Hp Hb]]]].
  destruct (post_exit_packages_every_output no_summaries no_funs no_procs
    (no_funs_ok no_summaries) (no_procs_ok no_summaries)
    (audit_exit_body o) [0;1] 2 3 4 5 [] o tbody tpack [] K sempty audit_exit_env
    [SLeaf 7;SLeaf 8] (SLeaf 9) [] [] [] [] 0 [] Hp Hb
    (audit_exit_body_source o Ho) eq_refl eq_refl audit_empty_invariant audit_empty_correspondence)
    as [rho1 [beta1 [H1 [n1 [rho2 [beta2 [H2 [n2 [sg2 [E1 [E2 [HI [HC Hv]]]]]]]]]]]]].
  destruct (corr_owned sg2 rho2 beta2 [5] [] 5 HC (or_introl eq_refl) (fun H => H))
    as [v [bs [Ev Es]]]. assert (v = recovery_value o [SLeaf 7;SLeaf 8] (SLeaf 9)) by congruence.
  subst v. exists tbody,tpack,rho1,beta1,H1,n1,rho2,beta2,H2,n2,bs.
  split; [exact E1|]. split; [exact E2|]. split; assumption.
Qed.

Theorem two_inouts_and_independent_result_run_without_leaks : exists t n,
  elab audit_modes audit_caller [] [] = Some (t, []) /\ count_copies t = 0 /\
  texec (tfuns_of audit_modes no_funs) (tprocs_of audit_modes audit_procs)
    [] [] [] 0 t [] [] [] n [SLeaf 42;SLeaf 9;SLeaf 2;SLeaf 3].
Proof.
  assert (Hel : exists t, elab audit_modes audit_caller [] [] = Some (t, []) /\ count_copies t = 0).
  { eexists; split; reflexivity. }
  destruct Hel as [t [Et Ec]].
  destruct (closed_program_frees_everything audit_modes no_funs audit_procs
    (no_funs_ok audit_modes) audit_procs_ok _ _ _ _ _ Et audit_caller_source) as [n En].
  exists t,n. split; [exact Et|split; assumption].
Qed.

Definition audit_blocks : list Block := [(0,0);(0,1);(0,2)].
Definition audit_rho : TEnv := [(Src 0,(SNode [SLeaf 7;SLeaf 8], audit_blocks))].
Definition audit_view := mkStaticView 0 (Src 0) audit_blocks 0 2 true BorrowLease.
Definition audit_views := mkStaticViews 1 [audit_view].
Definition audit_readonly := mkStaticViews 1 [mkStaticView 0 (Src 0) audit_blocks 0 2 false BorrowLease].
Definition audit_pin := mkStaticViews 1 [mkStaticView 0 (Src 0) audit_blocks 0 2 true PinLease].
Definition audit_ended := mkStaticViews 1 [].

Example view_issuance_controls :
  issue_view empty_views audit_rho audit_blocks (Src 0) 0 2 true BorrowLease = Some (audit_views,0) /\
  issue_view empty_views audit_rho [] (Src 0) 0 2 true BorrowLease = None /\
  issue_view empty_views [] audit_blocks (Src 0) 0 2 true BorrowLease = None /\
  issue_view empty_views audit_rho audit_blocks (Src 0) 1 2 true BorrowLease = None.
Proof. repeat split; reflexivity. Qed.

Example view_access_controls :
  read_view audit_views audit_rho audit_blocks 0 1 = Some (SLeaf 8) /\
  write_view audit_views audit_rho audit_blocks 0 1 99 =
    Some [(Src 0,(SNode [SLeaf 7;SLeaf 99],audit_blocks))] /\
  write_view audit_readonly audit_rho audit_blocks 0 1 99 = None /\
  write_view audit_views audit_rho audit_blocks 0 2 99 = None /\
  write_view audit_views audit_rho [] 0 1 99 = None /\
  write_view audit_views [(Src 0,(SNode [SLeaf 7;SLeaf 8],[(1,0);(1,1);(1,2)]))]
    [(1,0);(1,1);(1,2)] 0 1 99 = None /\
  write_view audit_views [(Src 0,(SNode [SNode [];SLeaf 8],audit_blocks))] audit_blocks 0 0 99 = None.
Proof. repeat split; reflexivity. Qed.

Example view_structural_and_end_controls :
  structural_admitted audit_views [Src 0] = false /\
  structural_admitted audit_pin [Src 0] = false /\
  structural_admitted audit_views [Src 1] = true /\
  drop_view_backing audit_views audit_rho audit_blocks (Src 0) = None /\
  drop_view_backing audit_pin audit_rho audit_blocks (Src 0) = None /\
  drop_view_backing audit_ended audit_rho audit_blocks (Src 0) = Some ([],[]) /\
  end_view audit_views 0 = Some audit_ended /\
  read_view audit_ended audit_rho audit_blocks 0 1 = None /\
  write_view audit_ended audit_rho audit_blocks 0 1 99 = None /\
  issue_view audit_ended audit_rho audit_blocks (Src 0) 0 2 true BorrowLease =
    Some (mkStaticViews 2 [mkStaticView 1 (Src 0) audit_blocks 0 2 true BorrowLease],1) /\
  end_view audit_ended 0 = None.
Proof. repeat split; reflexivity. Qed.

(* The new exit route must use the core procedure table, not recovery_exec.
   Both outcomes return two inouts and a separate packet before observation. *)
Theorem audit_core_call_return_and_error : forall choose : bool, exists t n,
  elab witness_modes (witness_caller choose) [] [] = Some (t, []) /\ count_copies t = 0 /\
  texec (tfuns_of witness_modes no_funs) (tprocs_of witness_modes witness_procs)
    [] [] [] 0 t [] [] [] n
    [SLeaf 9; SLeaf 9; SLeaf 2;
     SNode [SLeaf (if choose then 1 else 0); SLeaf (if choose then 3 else 4)]].
Proof. exact witness_return_and_error_free_everything. Qed.

Example audit_core_adapter_refuses_alias_and_escaping_control :
  adapter_admitted 20 21 22 10 [13] [11; 12] [15] 14 23 24 witness_body = true /\
  adapter_admitted 20 21 22 10 [13] [11; 11] [15] 14 23 24 witness_body = false /\
  adapter_admitted 20 21 22 10 [13] [11; 12] [15] 11 23 24 witness_body = false /\
  adapter_admitted 20 21 22 10 [13] [11; 12] [13] 14 23 24 witness_body = false /\
  adapter_admitted 20 21 22 10 [13] [11; 12] [15] 14 23 24 XBreak = false /\
  adapter_admitted 20 21 22 10 [13] [11; 12] [15] 14 23 24 XContinue = false /\
  adapter_admitted 20 21 22 10 [13] [11; 12] [15] 14 23 24 (XS (SEmit 20)) = false.
Proof. repeat split; reflexivity. Qed.

Definition audit_scoped_view_shape : forall funs procs sg v y body sg' tr cs,
  view_admitted v y body = true -> sexec funs procs sg (view_scope v y body) sg' tr ->
  sg y = Some (SNode cs) ->
  exists cs', sg' y = Some (SNode cs') /\ length cs' = length cs :=
  view_scope_keeps_backing_shape.

(* The local view issuer is static and must refuse an invalidating use even
   when it occurs after a read, in a branch, or inside a loop. *)
Example audit_scoped_view_controls :
  view_admitted 5 0 (view_write 6 5 1 (fun _ => SLeaf 9) []) = true /\
  view_admitted 5 0 (SField 7 5 0) = true /\
  view_admitted 5 0 (SPush 0 8) = false /\
  view_admitted 5 0 (SCopy 9 0) = false /\
  view_admitted 5 0 (SCallIO 1 0 [8]) = false /\
  view_admitted 5 0 (SPush 5 8) = false /\
  view_admitted 5 0 (SCopy 9 5) = false /\
  view_admitted 5 0 (SCall 9 1 [5]) = false /\
  view_admitted 5 0 (SSeq (SField 7 5 0) (SPush 0 8)) = false /\
  view_admitted 5 0 (SIf 8 (SField 7 5 0) (SPush 0 8)) = false /\
  view_admitted 5 0 (SWhile 8 [] (SPush 0 8)) = false /\
  view_admitted 5 0 (view_write 6 5 1 (fun vs => SNode vs) [5]) = false /\
  view_admitted 5 0 (SFocus 6 5 [] SSkip) = false /\
  view_admitted 5 5 (SField 7 5 0) = false.
Proof. repeat split; reflexivity. Qed.

Theorem audit_scoped_view_writes_then_releases_without_leaks : exists t n,
  elab no_summaries view_program [] [] = Some (t, []) /\
  texec (tfuns_of no_summaries no_funs) (tprocs_of no_summaries no_procs)
    [] [] [] 0 t [] [] [] n [SLeaf 1; SNode [SLeaf 1; SLeaf 9; SLeaf 3; SLeaf 4]].
Proof. exact view_program_frees_everything. Qed.

Definition audit_direct_control_trace : forall funs procs sg s sg' tr o,
  xsexec funs procs sg s sg' tr o ->
  direct_run funs procs (direct_graph s) sg (DLocal []) sg' (DExit o) tr :=
  direct_program_trace.

Theorem audit_direct_exits_skip_the_suffix : forall choose : bool,
  direct_run no_funs no_procs (direct_graph direct_switch)
    (supd sempty 9 (SLeaf (if choose then 1 else 0))) (DLocal [])
    (supd sempty 9 (SLeaf (if choose then 1 else 0)))
    (DExit (if choose then ORet else OErr)) [].
Proof. exact direct_switch_return_and_error. Qed.

Example audit_direct_handler_and_local_loop_routes :
  direct_graph (XLoop 9 [] XBreak) (DLocal [false]) = Some (DIJump (DExit ONorm)) /\
  direct_graph (XLoop 9 [] XContinue) (DLocal [false]) = Some (DIJump (DLocal [])) /\
  direct_graph (XTry XThrow XReturn) (DLocal [false]) = Some (DIJump (DLocal [true])) /\
  direct_graph (XTry XThrow XReturn) (DLocal [true]) = Some (DIJump (DExit ORet)) /\
  direct_graph (XLoop 9 [] XBreak) (DLocal [true]) = None /\
  direct_graph (XReturn) (DLocal [false]) = None.
Proof. repeat split; reflexivity. Qed.
