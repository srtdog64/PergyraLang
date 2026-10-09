(* Multi-output call and continuing-exit recovery over the existing cleanup
   machine. The bundle is a proof representation, not a public tuple ABI.
   This layer proves packaging/recovery and the guarded exit epilogue. It
   does not lower arbitrary XStmt bodies into the normal-only SProcTable,
   prove source expression/place evaluation, or erase tuple allocation. *)
Require Import Stdlib.Lists.List Stdlib.Arith.PeanoNat Stdlib.Bool.Bool.
Require Import Stdlib.micromega.Lia.
Require Import OwnershipCleanCore OwnershipCleanExits.
Import ListNotations.

Definition continuing_outcome (o : Outcome) : bool :=
  match o with ONorm | ORet | OErr => true | _ => false end.

Definition recovery_tag (o : Outcome) : nat :=
  match o with OErr => 0 | _ => 1 end.

Definition recovery_exits (K : list Var) : Exits :=
  {| xbrk := []; xcont := []; xret := K; xerr := K |}.

Definition recovery_package (o : Outcome) (ios : list Var)
    (value tag packet bundle : Var) : SStmt :=
  SSeq (SDef tag (fun _ => SLeaf (recovery_tag o)) [])
    (SSeq (SPack packet [tag; value]) (SPack bundle (ios ++ [packet]))).

Definition recovery_value (o : Outcome) (values : list SVal) (v : SVal) :=
  SNode (values ++ [SNode [SLeaf (recovery_tag o); v]]).

(* Failed admission is distinct from a body with no normal return. *)
Definition compile_recovery_package M B o ios value tag packet bundle :=
  if continuing_outcome o && nodupb (bundle :: packet :: tag :: ios ++ [value]) &&
     forallb (fun x => negb (vmem x B)) (bundle :: packet :: tag :: ios ++ [value])
  then elab M (recovery_package o ios value tag packet bundle) [bundle] B
  else None.

Lemma slookup_all_update_outside : forall sg xs z v,
  ~ In z xs -> slookup_all (supd sg z v) xs = slookup_all sg xs.
Proof.
  intros sg xs. induction xs as [|x xs IH]; intros z v H; simpl; [reflexivity|].
  rewrite supd_other; [|intro E; subst; apply H; left; reflexivity].
  rewrite IH; [reflexivity|intro F; apply H; right; exact F].
Qed.

Lemma slookup_all_append : forall sg xs ys vs ws,
  slookup_all sg xs = Some vs -> slookup_all sg ys = Some ws ->
  slookup_all sg (xs ++ ys) = Some (vs ++ ws).
Proof.
  intros sg xs. induction xs as [|x xs IH]; intros ys vs ws Hx Hy; simpl in *.
  - inversion Hx; subst; exact Hy.
  - destruct (sg x), (slookup_all sg xs); try discriminate.
    inversion Hx; subst. rewrite (IH _ _ _ eq_refl Hy). reflexivity.
Qed.

Lemma lookup_all_length : forall sg xs vs,
  slookup_all sg xs = Some vs -> length xs = length vs.
Proof.
  intros sg xs. induction xs as [|x xs IH]; intros vs H; simpl in H.
  - inversion H; reflexivity.
  - destruct (sg x), (slookup_all sg xs) eqn:E; try discriminate.
    inversion H; subst; simpl. f_equal. apply IH; reflexivity.
Qed.

Theorem recovery_package_source : forall funs procs sg o ios value tag packet bundle values v,
  NoDup (bundle :: packet :: tag :: ios ++ [value]) ->
  slookup_all sg ios = Some values -> sg value = Some v ->
  exists sg', sexec funs procs sg (recovery_package o ios value tag packet bundle) sg' [] /\
    sg' bundle = Some (recovery_value o values v).
Proof.
  intros funs procs sg o ios value tag packet bundle values v HN Hi Hv.
  inversion HN as [|? ? Hb HN1]; subst.
  inversion HN1 as [|? ? Hp HN2]; subst.
  inversion HN2 as [|? ? Ht HN3]; subst.
  assert (Htv : tag <> value) by (intro E; subst; apply Ht; apply in_or_app; right; simpl; auto).
  assert (Hti : ~ In tag ios) by (intro H; apply Ht; apply in_or_app; left; exact H).
  assert (Hpi : ~ In packet ios) by (intro H; apply Hp; right; apply in_or_app; left; exact H).
  set (sg1 := supd sg tag (SLeaf (recovery_tag o))).
  set (sg2 := supd sg1 packet (SNode [SLeaf (recovery_tag o); v])).
  set (sg3 := supd sg2 bundle (recovery_value o values v)).
  exists sg3. split; [|apply supd_same].
  unfold recovery_package. eapply SE_Seq with (sg2 := sg1) (tr1 := []) (tr2 := []).
  - unfold sg1. eapply SE_Def with (vs := []); reflexivity.
  - eapply SE_Seq with (sg2 := sg2) (tr1 := []) (tr2 := []).
    + apply SE_Pack. simpl. unfold sg1. rewrite supd_same, supd_other by exact Htv.
      rewrite Hv. reflexivity.
    + apply SE_Pack. unfold recovery_value.
      apply slookup_all_append.
      * unfold sg2, sg1. rewrite !slookup_all_update_outside by assumption. exact Hi.
      * simpl. unfold sg2. rewrite supd_same. reflexivity.
    + reflexivity.
  - reflexivity.
Qed.

(* The caller first restores every inout and the ordinary outcome packet.
   Selection of a success/error continuation is a subsequent operation.
   scope is the authoritative pre-call binding set, not just the operands.
   Its complete, current production issuance remains a separate obligation. *)
Definition multi_call_admitted (scope : list Var) bundle ios args packet :=
  nodupb (bundle :: packet :: ios) &&
  forallb (fun x => negb (vmem x ios)) args &&
  negb (vmem bundle args || vmem packet args) &&
  inclb (ios ++ args) scope &&
  negb (vmem bundle scope || vmem packet scope).

Definition multi_call_body g bundle ios args packet : SStmt :=
  SSeq (SPack bundle ios)
    (SSeq (SCallIO g bundle args) (SUnpack bundle (ios ++ [packet]))).

Definition normalize_multi_call scope g bundle ios args packet : option SStmt :=
  if multi_call_admitted scope bundle ios args packet
  then Some (multi_call_body g bundle ios args packet) else None.

Theorem inout_argument_alias_refused : forall scope g bundle ios args packet x,
  In x ios -> In x args -> normalize_multi_call scope g bundle ios args packet = None.
Proof.
  intros scope g bundle ios args packet x Hi Ha. unfold normalize_multi_call, multi_call_admitted.
  destruct (nodupb (bundle :: packet :: ios) && forallb (fun x => negb (vmem x ios)) args)
    eqn:E; [|reflexivity].
  apply andb_true_iff in E. destruct E as [_ E].
  apply forallb_forall with (x := x) in E; [|exact Ha].
  unfold vmem in E. destruct (in_dec Nat.eq_dec x ios); [discriminate|contradiction].
Qed.

Theorem admitted_inout_pack_has_no_preservation_copies : forall scope bundle ios args packet B,
  multi_call_admitted scope bundle ios args packet = true ->
  (forall x, In x ios -> ~ In x B) -> filter (keep (bundle :: args) B) ios = [].
Proof.
  intros scope bundle ios args packet B E HB. unfold multi_call_admitted in E.
  apply andb_true_iff in E. destruct E as [E _].
  apply andb_true_iff in E. destruct E as [E _].
  apply andb_true_iff in E. destruct E as [E _]. apply andb_true_iff in E.
  destruct E as [HN HA]. apply nodupb_spec in HN. inversion HN as [|? ? Hbundle HN']; subst.
  assert (Hfalse : forall x, In x ios -> keep (bundle :: args) B x = false).
  { intros x Hx. assert (Hxb : x <> bundle) by (intro Ex; subst; apply Hbundle; right; exact Hx).
    assert (Hxa : ~ In x args).
    { intro H. apply forallb_forall with (x := x) in HA; [|exact H].
      unfold vmem in HA. destruct (in_dec Nat.eq_dec x ios); [discriminate|contradiction]. }
    unfold keep, vmem. destruct (in_dec Nat.eq_dec x (bundle :: args));
      [simpl in i; destruct i; [congruence|contradiction]|].
    destruct (in_dec Nat.eq_dec x B); [exfalso; apply (HB x Hx); assumption|reflexivity]. }
  clear - bundle ios args B Hfalse. revert Hfalse.
  induction ios as [|x rest IH]; intros Hfalse; [reflexivity|]. simpl.
  rewrite Hfalse by (left; reflexivity). apply IH.
  intros y Hy. apply Hfalse; right; exact Hy.
Qed.

Lemma cleanup_drops_have_no_copies : forall vars B, count_copies (drops vars B) = 0.
Proof.
  intros vars B. induction vars as [|x rest IH]; [reflexivity|].
  simpl. destruct (in_dec Nat.eq_dec x B); simpl; exact IH.
Qed.

(* Direct elaboration result for the first pack at its admitted call live set.
   This is not a whole-callee copy bound or tuple-allocation erasure proof. *)
Theorem admitted_first_pack_has_no_copies : forall M scope bundle ios args packet B t Lin,
  multi_call_admitted scope bundle ios args packet = true ->
  (forall x, In x ios -> ~ In x B) ->
  elab M (SPack bundle ios) (bundle :: args) B = Some (t,Lin) -> count_copies t = 0.
Proof.
  intros M scope bundle ios args packet B t Lin Ea HB Ee.
  pose proof (admitted_inout_pack_has_no_preservation_copies scope bundle ios args packet B Ea HB) as Ec.
  simpl in Ee. destruct (in_dec Nat.eq_dec bundle ios); [discriminate|].
  destruct (in_dec Nat.eq_dec bundle B); [discriminate|].
  destruct (nodupb ios); [|discriminate]. rewrite Ec in Ee. inversion Ee; subst t.
  simpl. unfold settle. apply cleanup_drops_have_no_copies.
Qed.

Theorem multi_call_pack_call_unpack : forall funs procs sg scope g bundle ios args packet
    values vs io ps body sgc tr recovered outcome normalized,
  normalize_multi_call scope g bundle ios args packet = Some normalized ->
  ~ In bundle args ->
  procs g = Some (io, ps, body) ->
  slookup_all sg ios = Some values -> slookup_all sg args = Some vs ->
  length ps = length vs -> length ios = length recovered ->
  sexec funs procs (supd (sbind ps vs) io (SNode values)) body sgc tr ->
  sgc io = Some (SNode (recovered ++ [outcome])) ->
  sexec funs procs sg normalized
    (supd_list (supd (supd sg bundle (SNode values)) bundle
                  (SNode (recovered ++ [outcome])))
                (ios ++ [packet]) (recovered ++ [outcome])) tr.
Proof.
  intros funs procs sg scope g bundle ios args packet values vs io ps body sgc tr recovered outcome normalized
    Hnormalize Hb Hg Hi Ha Hps Hlen Hbody Hret.
  unfold normalize_multi_call in Hnormalize.
  destruct (multi_call_admitted scope bundle ios args packet); [|discriminate].
  inversion Hnormalize; subst. unfold multi_call_body. eapply SE_Seq with (tr1 := []) (tr2 := tr).
  - apply SE_Pack; exact Hi.
  - eapply SE_Seq with (tr1 := tr) (tr2 := []).
    + eapply SE_CallIO; [exact Hg|apply supd_same| |exact Hps|exact Hbody|exact Hret].
      rewrite slookup_all_update_outside by exact Hb. exact Ha.
    + apply SE_Unpack; [apply supd_same|rewrite !length_app; simpl; lia].
    + rewrite app_nil_r; reflexivity.
  - reflexivity.
Qed.

Section RecoverySoundness.
Variable M : Modes.
Variable funs : SFunTable.
Variable procs : SProcTable.
Hypothesis funs_ok : forall g d, funs g = Some d -> elab_fun M g d <> None.
Hypothesis procs_ok : forall g d, procs g = Some d -> elab_proc M g d <> None.

(* Generalizes routine_exit_is_uniform to any recovery live set, including
   the independently owned return/error payload. Borrowed parameters retain
   the core's existing frame-heap discipline. *)
Theorem continuing_exit_recovers_live_set : forall body K B t Lin sg sg' tr o rho beta H n R,
  xelab M body K (recovery_exits K) B = Some (t, Lin) ->
  xsexec funs procs sg body sg' tr o -> continuing_outcome o = true ->
  INV rho beta H n R -> CORR sg rho beta Lin B ->
  exists rho' beta' H' n',
    xtexec (tfuns_of M funs) (tprocs_of M procs) rho beta H n t rho' beta' H' n' tr o /\
    INV rho' beta' H' n' R /\ CORR sg' rho' beta' K B.
Proof.
  intros body K B t Lin sg sg' tr o rho beta H n R Hel Hs Ho HI HC.
  destruct (xelab_sound M funs procs funs_ok procs_ok sg body sg' tr o Hs
    K (recovery_exits K) B t Lin rho beta H n R Hel HI HC)
    as [rho' [beta' [H' [n' [Hex [HI' HC']]]]]].
  exists rho', beta', H', n'. split; [exact Hex|split; [exact HI'|]].
  destruct o; simpl in Ho; try discriminate; exact HC'.
Qed.

(* Post-exit packaging lemma only: two executions are composed here. The
   operational RecoveryAdapter below consumes it; ordinary XTSeq would skip
   the second execution on OErr/ORet. This lemma alone is not catch syntax. *)
Theorem post_exit_packages_every_output : forall body ios value tag packet bundle B o
    tbody tpack Lin K sg sg' values v tr rho beta H n R,
  compile_recovery_package M B o ios value tag packet bundle = Some (tpack, K) ->
  xelab M body K (recovery_exits K) B = Some (tbody, Lin) ->
  xsexec funs procs sg body sg' tr o ->
  slookup_all sg' ios = Some values -> sg' value = Some v ->
  INV rho beta H n R -> CORR sg rho beta Lin B ->
  exists rho1 beta1 H1 n1 rho2 beta2 H2 n2 sg2,
    xtexec (tfuns_of M funs) (tprocs_of M procs) rho beta H n tbody rho1 beta1 H1 n1 tr o /\
    texec (tfuns_of M funs) (tprocs_of M procs) rho1 beta1 H1 n1 tpack rho2 beta2 H2 n2 [] /\
    INV rho2 beta2 H2 n2 R /\ CORR sg2 rho2 beta2 [bundle] B /\
    sg2 bundle = Some (recovery_value o values v).
Proof.
  intros body ios value tag packet bundle B o tbody tpack Lin K sg sg' values v tr
    rho beta H n R Hp Hb Hs Hi Hv HI HC.
  unfold compile_recovery_package in Hp.
  destruct (continuing_outcome o && nodupb (bundle :: packet :: tag :: ios ++ [value]) &&
    forallb (fun x => negb (vmem x B)) (bundle :: packet :: tag :: ios ++ [value]))
    eqn:Hadmit; [|discriminate].
  apply andb_true_iff in Hadmit. destruct Hadmit as [Hadmit HB].
  apply andb_true_iff in Hadmit. destruct Hadmit as [Ho Hnames].
  destruct (continuing_exit_recovers_live_set body K B tbody Lin sg sg' tr o
    rho beta H n R Hb Hs Ho HI HC) as [rho1 [beta1 [H1 [n1 [E1 [I1 C1]]]]]].
  destruct (recovery_package_source funs procs sg' o ios value tag packet bundle values v
    (nodupb_spec _ Hnames) Hi Hv) as [sg2 [Epack Hvalue]].
  destruct (elab_sound M funs procs funs_ok procs_ok _ _ _ _ Epack [bundle] B
    tpack K rho1 beta1 H1 n1 R Hp I1 C1)
    as [rho2 [beta2 [H2 [n2 [E2 [I2 C2]]]]]].
  exists rho1, beta1, H1, n1, rho2, beta2, H2, n2, sg2.
  split; [exact E1|]. split; [exact E2|]. split; [exact I2|]. split; assumption.
Qed.
End RecoverySoundness.

(* Reject only unbound loop exits; a loop consumes its own break/continue. *)
Fixpoint loop_control_bound (inside : bool) (s : XStmt) : bool :=
  match s with
  | XBreak | XContinue => inside
  | XSeq a b | XIf _ a b | XTry a b => loop_control_bound inside a && loop_control_bound inside b
  | XLoop _ _ body => loop_control_bound true body
  | _ => true
  end.

Lemma escaping_control_requires_loop : forall funs procs sg body sg' tr o,
  xsexec funs procs sg body sg' tr o -> forall inside,
  loop_control_bound inside body = true -> (o = OBrk \/ o = OCont) -> inside = true.
Proof.
  intros funs procs sg body sg' tr o E. induction E; intros inside Hbound Ho; simpl in Hbound;
    try (destruct Ho as [Ho|Ho]; discriminate);
    try (apply andb_true_iff in Hbound; destruct Hbound as [Ha Hb]);
    try (eapply IHE; eassumption); try (eapply IHE2; eassumption); try assumption.
  all: destruct H1 as [H1|H1]; destruct Ho as [Ho|Ho]; congruence.
Qed.

Theorem admitted_body_has_continuing_exit : forall funs procs sg body sg' tr o,
  loop_control_bound false body = true -> xsexec funs procs sg body sg' tr o ->
  continuing_outcome o = true.
Proof.
  intros funs procs sg body sg' tr o Hb E.
  destruct o; try reflexivity;
    pose proof (escaping_control_requires_loop _ _ _ _ _ _ _ E false Hb ltac:(auto)) as H;
    discriminate.
Qed.

Record RecoveryAdapter := mkRecoveryAdapter {
  recovery_body : XT;
  recovery_success : TStmt;
  recovery_failure : TStmt
}.

Definition recovery_epilogue (a : RecoveryAdapter) (o : Outcome) :=
  match o with OErr => recovery_failure a | _ => recovery_success a end.

(* A single adapter's operational catch boundary, over the SAME target heap.
   ORet is a normal callee return; OErr becomes a packaged handled outcome.
   Break/continue cannot cross this boundary. This is not XTSeq or TCall. *)
Inductive recovery_exec (tf tp : TTable) : TEnv -> TEnv -> list Block -> nat ->
    RecoveryAdapter -> TEnv -> TEnv -> list Block -> nat -> list SVal -> Prop :=
| RE_Caught : forall rho beta H n a rho1 beta1 H1 n1 tr o rho2 beta2 H2 n2,
    xtexec tf tp rho beta H n (recovery_body a) rho1 beta1 H1 n1 tr o ->
    continuing_outcome o = true ->
    texec tf tp rho1 beta1 H1 n1 (recovery_epilogue a o) rho2 beta2 H2 n2 [] ->
    recovery_exec tf tp rho beta H n a rho2 beta2 H2 n2 tr.

Definition compile_recovery_adapter M B body ios value tag packet bundle :=
  if loop_control_bound false body
  then match compile_recovery_package M B ONorm ios value tag packet bundle,
             compile_recovery_package M B OErr ios value tag packet bundle with
       | Some (tn,K), Some (te,Ke) =>
           if list_eq_dec Nat.eq_dec K Ke
           then match xelab M body K (recovery_exits K) B with
                | Some (tb,Lin) => Some (mkRecoveryAdapter tb tn te,Lin)
                | None => None
                end
           else None
       | _,_ => None
       end
  else None.

Lemma package_normal_and_return_same : forall M B ios value tag packet bundle,
  compile_recovery_package M B ORet ios value tag packet bundle =
  compile_recovery_package M B ONorm ios value tag packet bundle.
Proof. reflexivity. Qed.

Theorem one_compiled_adapter_recovers_all_continuing_outcomes : forall M funs procs,
  (forall g d, funs g = Some d -> elab_fun M g d <> None) ->
  (forall g d, procs g = Some d -> elab_proc M g d <> None) ->
  forall B body ios value tag packet bundle a Lin sg sg' values v tr o rho beta H n R,
  compile_recovery_adapter M B body ios value tag packet bundle = Some (a,Lin) ->
  xsexec funs procs sg body sg' tr o ->
  slookup_all sg' ios = Some values -> sg' value = Some v ->
  INV rho beta H n R -> CORR sg rho beta Lin B ->
  exists rho' beta' H' n' sg2,
    recovery_exec (tfuns_of M funs) (tprocs_of M procs) rho beta H n a rho' beta' H' n' tr /\
    INV rho' beta' H' n' R /\ CORR sg2 rho' beta' [bundle] B /\
    sg2 bundle = Some (recovery_value o values v).
Proof.
  intros M funs procs Hf Hp B body ios value tag packet bundle a Lin sg sg' values v tr o
    rho beta H n R E Hs Hi Hv HI HC.
  unfold compile_recovery_adapter in E. destruct (loop_control_bound false body) eqn:Hbound; [|discriminate].
  destruct (compile_recovery_package M B ONorm ios value tag packet bundle) as [[tn K]|] eqn:En;
    [|discriminate].
  destruct (compile_recovery_package M B OErr ios value tag packet bundle) as [[te Ke]|] eqn:Ee;
    [|discriminate].
  destruct (list_eq_dec Nat.eq_dec K Ke); [subst Ke|discriminate].
  destruct (xelab M body K (recovery_exits K) B) as [[tb L]|] eqn:Eb; [|discriminate].
  inversion E; subst a L.
  pose proof (admitted_body_has_continuing_exit _ _ _ _ _ _ _ Hbound Hs) as Ho.
  assert (Epack : compile_recovery_package M B o ios value tag packet bundle =
    Some (recovery_epilogue (mkRecoveryAdapter tb tn te) o,K)).
  { destruct o; simpl in Ho; try discriminate; [exact En|rewrite package_normal_and_return_same; exact En|exact Ee]. }
  destruct (post_exit_packages_every_output M funs procs Hf Hp body ios value tag packet bundle B o
    tb _ Lin K sg sg' values v tr rho beta H n R Epack Eb Hs Hi Hv HI HC)
    as [rho1 [beta1 [H1 [n1 [rho2 [beta2 [H2 [n2 [sg2 [E1 [E2 [I2 [C2 V2]]]]]]]]]]]]].
  exists rho2,beta2,H2,n2,sg2. split.
  - eapply RE_Caught; [exact E1|exact Ho|exact E2].
  - split; [exact I2|split; assumption].
Qed.

(* Explicit shape decoding: an arity/tag/packet mismatch is None, not stuck
   unpack or a guessed success. General language type schema is still OPEN. *)
Definition decode_recovery_value expected (v : SVal) : option (list SVal * nat * SVal) :=
  match v with
  | SNode fields =>
      if Nat.eqb (length fields) (S expected)
      then match nth_error fields expected with
           | Some (SNode [SLeaf tag;value]) =>
               if Nat.eqb tag 0 || Nat.eqb tag 1 then Some (firstn expected fields,tag,value) else None
           | _ => None
           end
      else None
  | _ => None
  end.

Theorem packaged_output_decodes : forall o values v,
  continuing_outcome o = true ->
  decode_recovery_value (length values) (recovery_value o values v) =
    Some (values,recovery_tag o,v).
Proof.
  intros o values v Ho. unfold decode_recovery_value, recovery_value.
  rewrite length_app; simpl. rewrite Nat.add_1_r, Nat.eqb_refl.
  rewrite nth_error_app2 by lia. rewrite Nat.sub_diag; simpl.
  rewrite firstn_app, firstn_all, Nat.sub_diag; simpl. rewrite app_nil_r.
  destruct o; simpl in Ho; try discriminate; reflexivity.
Qed.

(* Decoder dispatches only after it has restored the ordered inout bindings.
   The callbacks observe that restored environment, including on error. *)
Definition restore_recovery (sg : SEnv) (ios : list Var) returned :=
  if nodupb ios
  then match decode_recovery_value (length ios) returned with
       | Some (values,tag,payload) => Some (supd_list sg ios values,tag,payload)
       | None => None
       end
  else None.

Definition dispatch_recovery {A : Type} sg ios returned
    (success failure : SEnv -> SVal -> A) : option A :=
  match restore_recovery sg ios returned with
  | Some (restored,tag,payload) =>
      Some (if Nat.eqb tag 0 then failure restored payload else success restored payload)
  | None => None
  end.

Theorem dispatch_after_restore : forall (A : Type) sg ios o values v success failure,
  nodupb ios = true -> length ios = length values -> continuing_outcome o = true ->
  @dispatch_recovery A sg ios (recovery_value o values v) success failure =
    Some (match o with OErr => failure (supd_list sg ios values) v
                     | _ => success (supd_list sg ios values) v end).
Proof.
  intros A sg ios o values v success failure HN Hlen Ho.
  unfold dispatch_recovery, restore_recovery. rewrite HN, Hlen, packaged_output_decodes by exact Ho.
  destruct o; simpl in Ho; try discriminate; reflexivity.
Qed.
