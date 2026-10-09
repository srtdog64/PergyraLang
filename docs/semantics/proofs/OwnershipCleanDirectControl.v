(* Direct source-control lowering, not a production backend certificate.

   A finite XStmt tree is addressed by structural paths. Instruction lookup
   emits ordinary core statements, jumps and condition branches only. Loop
   backedges and error handlers are labels, not status variables in SEnv.
   No local is initialized merely because a different arm might define it.

   direct_control_simulates_source proves forward trace/value/exit
   preservation. direct_control_has_core_reference connects that execution
   to the existing CL6 ordinary-core lowering, without a new catch rule.
   Reverse graph adequacy, physical pointer recovery and the MIR/emitter
   refinement remain OPEN. This module does not authorize automatic drops. *)
Require Import Stdlib.Lists.List Stdlib.Bool.Bool.
Require Import OwnershipCleanCore OwnershipCleanExits.
Require Import OwnershipCleanCallLowering.
Import ListNotations.

Inductive DLabel : Type :=
| DLocal (path : list bool)
| DExit (outcome : Outcome).

Inductive DInst : Type :=
| DIExec (statement : SStmt) (next : DLabel)
| DIJump (next : DLabel)
| DIBranch (condition : Var) (yes no : DLabel).

Definition DGraph := DLabel -> option DInst.
Definition DContinuations := Outcome -> DLabel.
Definition direct_exits : DContinuations := DExit.

Definition direct_normal (K : DContinuations) (next : DLabel) : DContinuations :=
  fun o => match o with ONorm => next | _ => K o end.

Definition direct_error (K : DContinuations) (handler : DLabel) : DContinuations :=
  fun o => match o with OErr => handler | _ => K o end.

Definition direct_loop (K : DContinuations) (head : DLabel) : DContinuations :=
  fun o => match o with
           | ONorm | OCont => head
           | OBrk => K ONorm
           | _ => K o
           end.

(* false selects the first child, true the second. Unused paths have no
   instruction; a missing instruction is never interpreted as fallthrough. *)
Fixpoint direct_at (s : XStmt) (prefix : list bool) (K : DContinuations)
    (path : list bool) {struct s} : option DInst :=
  let left := DLocal (prefix ++ [false]) in
  let right := DLocal (prefix ++ [true]) in
  match path with
  | [] =>
      Some (match s with
      | XS atom => DIExec atom (K ONorm)
      | XSeq _ _ | XTry _ _ => DIJump left
      | XIf c _ _ => DIBranch c left right
      | XLoop c _ _ => DIBranch c left (K ONorm)
      | XBreak => DIJump (K OBrk)
      | XContinue => DIJump (K OCont)
      | XReturn => DIJump (K ORet)
      | XThrow => DIJump (K OErr)
      end)
  | false :: rest =>
      match s with
      | XSeq a _ => direct_at a (prefix ++ [false]) (direct_normal K right) rest
      | XIf _ a _ => direct_at a (prefix ++ [false]) K rest
      | XLoop _ _ b => direct_at b (prefix ++ [false]) (direct_loop K (DLocal prefix)) rest
      | XTry b _ => direct_at b (prefix ++ [false]) (direct_error K right) rest
      | _ => None
      end
  | true :: rest =>
      match s with
      | XSeq _ b | XIf _ _ b | XTry _ b => direct_at b (prefix ++ [true]) K rest
      | _ => None
      end
  end.

Definition direct_graph (s : XStmt) : DGraph :=
  fun label => match label with
               | DLocal path => direct_at s [] direct_exits path
               | DExit _ => None
               end.

Definition direct_contains (P : DGraph) (s : XStmt) (prefix : list bool)
    (K : DContinuations) : Prop :=
  forall path, P (DLocal (prefix ++ path)) = direct_at s prefix K path.

Lemma direct_graph_contains : forall s, direct_contains (direct_graph s) s [] direct_exits.
Proof. intros s path. reflexivity. Qed.

Lemma direct_root : forall P s p K,
  direct_contains P s p K -> P (DLocal p) = direct_at s p K [].
Proof. intros P s p K H. specialize (H []). now rewrite app_nil_r in H. Qed.

Lemma direct_seq_left : forall P a b p K, direct_contains P (XSeq a b) p K ->
  direct_contains P a (p ++ [false]) (direct_normal K (DLocal (p ++ [true]))).
Proof. intros P a b p K H path. rewrite <- app_assoc. exact (H (false :: path)). Qed.
Lemma direct_seq_right : forall P a b p K, direct_contains P (XSeq a b) p K ->
  direct_contains P b (p ++ [true]) K.
Proof. intros P a b p K H path. rewrite <- app_assoc. exact (H (true :: path)). Qed.
Lemma direct_if_left : forall P c a b p K, direct_contains P (XIf c a b) p K ->
  direct_contains P a (p ++ [false]) K.
Proof. intros P c a b p K H path. rewrite <- app_assoc. exact (H (false :: path)). Qed.
Lemma direct_if_right : forall P c a b p K, direct_contains P (XIf c a b) p K ->
  direct_contains P b (p ++ [true]) K.
Proof. intros P c a b p K H path. rewrite <- app_assoc. exact (H (true :: path)). Qed.
Lemma direct_loop_body : forall P c h b p K, direct_contains P (XLoop c h b) p K ->
  direct_contains P b (p ++ [false]) (direct_loop K (DLocal p)).
Proof. intros P c h b p K H path. rewrite <- app_assoc. exact (H (false :: path)). Qed.
Lemma direct_try_body : forall P b h p K, direct_contains P (XTry b h) p K ->
  direct_contains P b (p ++ [false]) (direct_error K (DLocal (p ++ [true]))).
Proof. intros P b h p K H path. rewrite <- app_assoc. exact (H (false :: path)). Qed.
Lemma direct_try_handler : forall P b h p K, direct_contains P (XTry b h) p K ->
  direct_contains P h (p ++ [true]) K.
Proof. intros P b h p K H path. rewrite <- app_assoc. exact (H (true :: path)). Qed.

Section DirectExecution.
Variable funs : SFunTable.
Variable procs : SProcTable.
Variable P : DGraph.

(* A finite trace between two labels. Stopping at an internal label is
   useful for composition, not an assertion that it is a routine exit. *)
Inductive direct_run : SEnv -> DLabel -> SEnv -> DLabel -> list SVal -> Prop :=
| DR_Stop : forall sg label, direct_run sg label sg label []
| DR_Exec : forall sg sg1 sg2 here next finish atom tr1 tr2 tr,
    P here = Some (DIExec atom next) -> sexec funs procs sg atom sg1 tr1 ->
    direct_run sg1 next sg2 finish tr2 -> tr = tr1 ++ tr2 ->
    direct_run sg here sg2 finish tr
| DR_Jump : forall sg sg' here next finish tr,
    P here = Some (DIJump next) -> direct_run sg next sg' finish tr ->
    direct_run sg here sg' finish tr
| DR_True : forall sg sg' here c yes no finish value tr,
    P here = Some (DIBranch c yes no) -> sg c = Some value -> truthy value = true ->
    direct_run sg yes sg' finish tr -> direct_run sg here sg' finish tr
| DR_False : forall sg sg' here c yes no finish value tr,
    P here = Some (DIBranch c yes no) -> sg c = Some value -> truthy value = false ->
    direct_run sg no sg' finish tr -> direct_run sg here sg' finish tr.

Lemma direct_run_append : forall sg a sg1 b tr1,
  direct_run sg a sg1 b tr1 -> forall sg2 c tr2, direct_run sg1 b sg2 c tr2 ->
  direct_run sg a sg2 c (tr1 ++ tr2).
Proof.
  intros sg a sg1 b tr1 H. induction H; intros sg2' c' tr2' E.
  - exact E.
  - subst tr. eapply DR_Exec; [exact H|exact H0|apply IHdirect_run; exact E|].
    now rewrite app_assoc.
  - eapply DR_Jump; [exact H|apply IHdirect_run; exact E].
  - eapply DR_True; [exact H|exact H0|exact H1|apply IHdirect_run; exact E].
  - eapply DR_False; [exact H|exact H0|exact H1|apply IHdirect_run; exact E].
Qed.

Theorem direct_control_simulates_source : forall sg s sg' tr o,
  xsexec funs procs sg s sg' tr o -> forall p K, direct_contains P s p K ->
  direct_run sg (DLocal p) sg' (K o) tr.
Proof.
  intros sg s sg' tr o Hx. induction Hx as
    [sg atom sg' tr Ha
    |sg1 sg2 sg3 a b tr1 tr2 tr o Ha IHa Hb IHb Htr
    |sg sg' a b tr o Ha IHa Ho
    |sg sg' c a b v tr o Hc Ht Ha IHa
    |sg sg' c a b v tr o Hc Hf Hb IHb
    |sg c h b v Hc Hf
    |sg sg1 sg2 c h b v tr1 tr2 tr o1 o Hc Ht Hb IHb Ho Hw IHw Htr
    |sg sg1 c h b v tr Hc Ht Hb IHb
    |sg sg1 c h b v tr o Hc Ht Hb IHb Ho
    |sg|sg|sg|sg
    |sg sg' b h tr o Hb IHb Ho
    |sg sg1 sg2 b h tr1 tr2 tr o Hb IHb Hh IHh Htr]; intros p K HP.
  - eapply DR_Exec with (tr1 := tr) (tr2 := []);
      [exact (direct_root _ _ _ _ HP)|exact Ha|constructor|symmetry; apply app_nil_r].
  - eapply DR_Jump; [exact (direct_root _ _ _ _ HP)|]. subst tr.
    eapply direct_run_append.
    + exact (IHa _ _ (direct_seq_left _ _ _ _ _ HP)).
    + exact (IHb _ _ (direct_seq_right _ _ _ _ _ HP)).
  - eapply DR_Jump; [exact (direct_root _ _ _ _ HP)|].
    pose proof (IHa _ _ (direct_seq_left _ _ _ _ _ HP)) as E.
    destruct o; [contradiction|exact E..].
  - eapply DR_True; [exact (direct_root _ _ _ _ HP)|exact Hc|exact Ht|].
    exact (IHa _ _ (direct_if_left _ _ _ _ _ _ HP)).
  - eapply DR_False; [exact (direct_root _ _ _ _ HP)|exact Hc|exact Hf|].
    exact (IHb _ _ (direct_if_right _ _ _ _ _ _ HP)).
  - eapply DR_False; [exact (direct_root _ _ _ _ HP)|exact Hc|exact Hf|constructor].
  - eapply DR_True; [exact (direct_root _ _ _ _ HP)|exact Hc|exact Ht|]. subst tr.
    assert (Eb : direct_run sg (DLocal (p ++ [false])) sg1 (DLocal p) tr1).
    { pose proof (IHb _ _ (direct_loop_body _ _ _ _ _ _ HP)) as E.
      destruct Ho as [Eo|Eo]; subst o1; exact E. }
    eapply direct_run_append.
    + exact Eb.
    + exact (IHw p K HP).
  - eapply DR_True; [exact (direct_root _ _ _ _ HP)|exact Hc|exact Ht|].
    exact (IHb _ _ (direct_loop_body _ _ _ _ _ _ HP)).
  - eapply DR_True; [exact (direct_root _ _ _ _ HP)|exact Hc|exact Ht|].
    pose proof (IHb _ _ (direct_loop_body _ _ _ _ _ _ HP)) as E.
    destruct Ho as [Eo|Eo]; subst o; exact E.
  - eapply DR_Jump; [exact (direct_root _ _ _ _ HP)|constructor].
  - eapply DR_Jump; [exact (direct_root _ _ _ _ HP)|constructor].
  - eapply DR_Jump; [exact (direct_root _ _ _ _ HP)|constructor].
  - eapply DR_Jump; [exact (direct_root _ _ _ _ HP)|constructor].
  - eapply DR_Jump; [exact (direct_root _ _ _ _ HP)|].
    pose proof (IHb _ _ (direct_try_body _ _ _ _ _ HP)) as E.
    destruct o; [exact E..|contradiction].
  - eapply DR_Jump; [exact (direct_root _ _ _ _ HP)|]. subst tr.
    eapply direct_run_append.
    + exact (IHb _ _ (direct_try_body _ _ _ _ _ HP)).
    + exact (IHh _ _ (direct_try_handler _ _ _ _ _ HP)).
Qed.
End DirectExecution.

Theorem direct_program_trace : forall funs procs sg s sg' tr o,
  xsexec funs procs sg s sg' tr o ->
  direct_run funs procs (direct_graph s) sg (DLocal []) sg' (DExit o) tr.
Proof. intros. eapply direct_control_simulates_source; [exact H|apply direct_graph_contains]. Qed.

(* One source execution reaches the direct exit with no fresh SEnv names,
   and also has CL6's ordinary-core execution. This is forward refinement,
   not a converse assertion about arbitrary direct graph executions. *)
Theorem direct_control_has_core_reference : forall funs procs st gd w keep s,
  NoDup [st; gd; w] -> xfresh [st; gd; w] s ->
  forall sg sg' tr o, xsexec funs procs sg s sg' tr o ->
  forall sgl, agree [st; gd; w] sg sgl -> sgl st = Some (SLeaf 0) ->
  direct_run funs procs (direct_graph s) sg (DLocal []) sg' (DExit o) tr /\
  exists sgl', sexec funs procs sgl (lower st gd w keep s) sgl' tr /\
    agree [st; gd; w] sg' sgl' /\ sgl' st = Some (SLeaf (status_code o)).
Proof.
  intros funs procs st gd w keep s HN HF sg sg' tr o HX sgl HA HS.
  split; [apply direct_program_trace; exact HX|].
  eapply lower_sim; eauto.
Qed.

(* The two branch exits bypass the suffix and never allocate source-level
   status locals. A missing path remains None rather than a guessed return. *)
Definition direct_switch : XStmt :=
  XSeq (XIf 9 XReturn XThrow) (XS (SEmit 9)).

Example direct_switch_layout :
  direct_graph direct_switch (DLocal []) = Some (DIJump (DLocal [false])) /\
  direct_graph direct_switch (DLocal [false]) =
    Some (DIBranch 9 (DLocal [false; false]) (DLocal [false; true])) /\
  direct_graph direct_switch (DLocal [false; false]) = Some (DIJump (DExit ORet)) /\
  direct_graph direct_switch (DLocal [false; true]) = Some (DIJump (DExit OErr)) /\
  direct_graph direct_switch (DLocal [true]) = Some (DIExec (SEmit 9) (DExit ONorm)) /\
  direct_graph direct_switch (DLocal [true; false]) = None /\
  direct_graph direct_switch (DExit ORet) = None.
Proof. repeat split; reflexivity. Qed.

Theorem direct_switch_return_and_error : forall choose : bool,
  direct_run no_funs no_procs (direct_graph direct_switch)
    (supd sempty 9 (SLeaf (if choose then 1 else 0))) (DLocal [])
    (supd sempty 9 (SLeaf (if choose then 1 else 0)))
    (DExit (if choose then ORet else OErr)) [].
Proof.
  intros choose. apply direct_program_trace. unfold direct_switch.
  eapply XE_SeqX.
  - destruct choose.
    + eapply XE_IfT; [apply supd_same|reflexivity|constructor].
    + eapply XE_IfF; [apply supd_same|reflexivity|constructor].
  - destruct choose; discriminate.
Qed.

Example direct_local_control_layout :
  direct_graph (XLoop 9 [] XBreak) (DLocal [false]) = Some (DIJump (DExit ONorm)) /\
  direct_graph (XLoop 9 [] XContinue) (DLocal [false]) = Some (DIJump (DLocal [])) /\
  direct_graph (XTry XThrow XReturn) (DLocal [false]) = Some (DIJump (DLocal [true])) /\
  direct_graph (XTry XThrow XReturn) (DLocal [true]) = Some (DIJump (DExit ORet)).
Proof. repeat split; reflexivity. Qed.
