(* Exit elimination for multi-output callees.

   A callee body with early return, handled error and local break/continue
   (an XStmt of OwnershipCleanExits) is lowered to an exit-free SStmt that
   records the outcome in a compiler-issued status variable. The lowered
   body, wrapped by an adapter that unpacks the inout bundle and repacks
   every inout with the ordinary outcome packet, is an ordinary entry of the
   core's SProcTable. The caller side is the bundle normalization of
   OwnershipCleanCallRecovery. Their composition is a plain core SStmt, so
   target soundness is the core's elab_sound: no catch rule or other target
   construct is added, and RecoveryAdapter/recovery_exec is not on this path.

   Proved here:
   - [sexec_frame]: an exit-free statement that does not mention a set of
     variables runs the same way with those variables changed.
   - [lower_sim]: every source run of a body is simulated by its lowering,
     with the same trace, the same final values outside the issued names,
     and the outcome recorded as a status code.
   - [adapter_exec]: an admitted adapter returns every inout formal and the
     outcome packet on every continuing exit.
   - [lowered_multi_call_recovers]: the caller restores every inout actual
     and receives the packet before any handler runs, for normal return,
     early return and handled error alike.
   - [lowered_multi_call_sound]: the same call in the ownership machine,
     with the core invariant and correspondence preserved.
   - Concrete witnesses: a callee with a local loop and a return/error
     switch elaborates, and both the return run and the error run of the
     whole caller execute in the machine and free every block.

   Not proved here: source-place disjointness beyond variable identity,
   expression evaluation order above SStmt, erasure of the proof bundle in
   the production pointer ABI, and the cost of the status/guard variables. *)
Require Import Stdlib.Lists.List Stdlib.Arith.PeanoNat Stdlib.Bool.Bool.
Require Import Stdlib.micromega.Lia.
Require Import OwnershipCleanCore OwnershipCleanExits OwnershipCleanCallRecovery.
Import ListNotations.

(* ------------------------------------------------------------------ *)
(* Variables a statement mentions                                       *)
(* ------------------------------------------------------------------ *)

(* Loop heads are elaboration hints and are not read by the source
   semantics, so they are not mentions. *)
Fixpoint svars (s : SStmt) : list Var :=
  match s with
  | SSkip => []
  | SDef x _ ys | SPack x ys | SCall x _ ys => x :: ys
  | SCopy x y | SPush x y | SField x y _ => [x; y]
  | SEmit y => [y]
  | SSeq a b => svars a ++ svars b
  | SIf c a b => c :: svars a ++ svars b
  | SWhile c _ b => c :: svars b
  | SCallIO _ z ys => z :: ys
  | SFocus t y _ b => t :: y :: svars b
  | SUnpack y xs => y :: xs
  | SRegion x _ ys b => x :: ys ++ svars b
  end.

Fixpoint xvars (s : XStmt) : list Var :=
  match s with
  | XS s0 => svars s0
  | XSeq a b | XTry a b => xvars a ++ xvars b
  | XIf c a b => c :: xvars a ++ xvars b
  | XLoop c _ b => c :: xvars b
  | XBreak | XContinue | XReturn | XThrow => []
  end.

Definition sfresh (F : list Var) (s : SStmt) : Prop := forall z, In z F -> ~ In z (svars s).
Definition xfresh (F : list Var) (s : XStmt) : Prop := forall z, In z F -> ~ In z (xvars s).

(* Two environments agree outside F. *)
Definition agree (F : list Var) (sg sgl : SEnv) : Prop := forall y, ~ In y F -> sgl y = sg y.

Lemma agree_refl : forall F sg, agree F sg sg.
Proof. intros F sg y _. reflexivity. Qed.

Lemma agree_supd : forall F sg sgl x v, agree F sg sgl -> agree F (supd sg x v) (supd sgl x v).
Proof.
  intros F sg sgl x v H y Hy. unfold supd.
  destruct (Nat.eq_dec x y); [reflexivity|apply H; exact Hy].
Qed.

Lemma agree_set : forall F sg sgl z v, In z F -> agree F sg sgl -> agree F sg (supd sgl z v).
Proof.
  intros F sg sgl z v Hz H y Hy. rewrite supd_other by (intro E; subst; contradiction).
  apply H; exact Hy.
Qed.

Lemma agree_supd_list : forall F xs cs sg sgl,
  agree F sg sgl -> agree F (supd_list sg xs cs) (supd_list sgl xs cs).
Proof.
  intros F xs. induction xs as [|x xs IH]; intros cs sg sgl H; destruct cs as [|c cs];
    simpl; try exact H.
  apply agree_supd. apply IH. exact H.
Qed.

Lemma agree_lookup_all : forall F sg sgl ys, agree F sg sgl ->
  (forall y, In y ys -> ~ In y F) -> slookup_all sgl ys = slookup_all sg ys.
Proof.
  intros F sg sgl ys H Hy. induction ys as [|y ys IH]; simpl; [reflexivity|].
  rewrite (H y (Hy y (or_introl eq_refl))).
  rewrite IH by (intros z Hz; apply Hy; right; exact Hz). reflexivity.
Qed.

Lemma fresh_head : forall (F : list Var) (x : Var) (l : list Var), (forall z, In z F -> ~ In z (x :: l)) -> ~ In x F.
Proof. intros F x l H Hx. apply (H x Hx). left; reflexivity. Qed.

Lemma fresh_tail : forall (F : list Var) (x : Var) (l : list Var), (forall z, In z F -> ~ In z (x :: l)) ->
  forall z, In z F -> ~ In z l.
Proof. intros F x l H z Hz Hl. apply (H z Hz). right; exact Hl. Qed.

Lemma fresh_appl : forall (F l1 l2 : list Var), (forall z, In z F -> ~ In z (l1 ++ l2)) ->
  forall z, In z F -> ~ In z l1.
Proof. intros F l1 l2 H z Hz Hl. apply (H z Hz). apply in_or_app; left; exact Hl. Qed.

Lemma fresh_appr : forall (F l1 l2 : list Var), (forall z, In z F -> ~ In z (l1 ++ l2)) ->
  forall z, In z F -> ~ In z l2.
Proof. intros F l1 l2 H z Hz Hl. apply (H z Hz). apply in_or_app; right; exact Hl. Qed.

Lemma fresh_members : forall (F l : list Var), (forall z, In z F -> ~ In z l) -> forall y, In y l -> ~ In y F.
Proof. intros F l H y Hy Hf. exact (H y Hf Hy). Qed.

(* ------------------------------------------------------------------ *)
(* Frame                                                                *)
(* ------------------------------------------------------------------ *)

Lemma sexec_frame : forall funs procs sg s sg' tr,
  sexec funs procs sg s sg' tr ->
  forall F sgl, sfresh F s -> agree F sg sgl ->
  exists sgl', sexec funs procs sgl s sgl' tr /\ agree F sg' sgl' /\
    forall z, In z F -> sgl' z = sgl z.
Proof.
  intros funs procs sg s sg' tr Hs.
  induction Hs as
    [ sg
    | sg x f ys vs Hl
    | sg x y v Hy
    | sg x ys vs Hl
    | sg x y cs v Hx Hy
    | sg x y i cs v Hy Hi
    | sg y v Hy
    | sg1 sg2 sg3 s1 s2 tr1 tr2 tr H1 IH1 H2 IH2 Htr
    | sg sg' c s1 s2 v tr Hc Ht H1 IH1
    | sg sg' c s1 s2 v tr Hc Ht H2 IH2
    | sg c h b v Hc Ht
    | sg sg1 sg2 c h b v tr1 tr2 tr Hc Ht Hb IHb Hw IHw Htr
    | sg x g ys vs ps body ret sgc tr v Hg Hl Hlen Hb IHb Hr
    | sg g z ys vz vs io ps body sgc tr v Hg Hz Hl Hlen Hb IHb Hr
    | sg t y p s c cv sg' tr cv' c' Hy Hget Hs IHs Ht Hset
    | sg y xs cs Hy Hlen
    | sg x f ys s vs sg' tr Hl Hs IHs ];
    intros F sgl Hf Ha; unfold sfresh in Hf; cbn [svars] in Hf.
  - exists sgl. split; [constructor|split; [exact Ha|intros; reflexivity]].
  - pose proof (fresh_head _ _ _ Hf) as Hx.
    pose proof (fresh_members _ _ (fresh_tail _ _ _ Hf)) as Hys.
    exists (supd sgl x (f vs)). split.
    + apply SE_Def. rewrite (agree_lookup_all F sg sgl ys Ha Hys). exact Hl.
    + split; [apply agree_supd; exact Ha|].
      intros z Hz. apply supd_other. intro E; subst; contradiction.
  - pose proof (fresh_head _ _ _ Hf) as Hx.
    assert (Hy' : ~ In y F) by (apply (fresh_members _ _ (fresh_tail _ _ _ Hf)); left; reflexivity).
    exists (supd sgl x v). split.
    + apply SE_Copy. rewrite (Ha y Hy'). exact Hy.
    + split; [apply agree_supd; exact Ha|].
      intros z Hz. apply supd_other. intro E; subst; contradiction.
  - pose proof (fresh_head _ _ _ Hf) as Hx.
    pose proof (fresh_members _ _ (fresh_tail _ _ _ Hf)) as Hys.
    exists (supd sgl x (SNode vs)). split.
    + apply SE_Pack. rewrite (agree_lookup_all F sg sgl ys Ha Hys). exact Hl.
    + split; [apply agree_supd; exact Ha|].
      intros z Hz. apply supd_other. intro E; subst; contradiction.
  - pose proof (fresh_head _ _ _ Hf) as Hx'.
    assert (Hy' : ~ In y F) by (apply (fresh_members _ _ (fresh_tail _ _ _ Hf)); left; reflexivity).
    exists (supd sgl x (SNode (cs ++ [v]))). split.
    + apply SE_Push; [rewrite (Ha x Hx'); exact Hx|rewrite (Ha y Hy'); exact Hy].
    + split; [apply agree_supd; exact Ha|].
      intros z Hz. apply supd_other. intro E; subst; contradiction.
  - pose proof (fresh_head _ _ _ Hf) as Hx.
    assert (Hy' : ~ In y F) by (apply (fresh_members _ _ (fresh_tail _ _ _ Hf)); left; reflexivity).
    exists (supd sgl x v). split.
    + eapply SE_Field; [rewrite (Ha y Hy'); exact Hy|exact Hi].
    + split; [apply agree_supd; exact Ha|].
      intros z Hz. apply supd_other. intro E; subst; contradiction.
  - pose proof (fresh_head _ _ _ Hf) as Hy'.
    exists sgl. split; [apply SE_Emit; rewrite (Ha y Hy'); exact Hy|split; [exact Ha|intros; reflexivity]].
  - destruct (IH1 F sgl (fresh_appl _ _ _ Hf) Ha) as [sgl2 [E1 [A1 U1]]].
    destruct (IH2 F sgl2 (fresh_appr _ _ _ Hf) A1) as [sgl3 [E2 [A2 U2]]].
    exists sgl3. split; [eapply SE_Seq; [exact E1|exact E2|exact Htr]|].
    split; [exact A2|]. intros z Hz. rewrite U2 by exact Hz. apply U1; exact Hz.
  - pose proof (fresh_head _ _ _ Hf) as Hc'.
    pose proof (fresh_tail _ _ _ Hf) as Hr.
    destruct (IH1 F sgl (fresh_appl _ _ _ Hr) Ha) as [sgl' [E [A U]]].
    exists sgl'. split; [eapply SE_IfT; [rewrite (Ha c Hc'); exact Hc|exact Ht|exact E]|].
    split; assumption.
  - pose proof (fresh_head _ _ _ Hf) as Hc'.
    pose proof (fresh_tail _ _ _ Hf) as Hr.
    destruct (IH2 F sgl (fresh_appr _ _ _ Hr) Ha) as [sgl' [E [A U]]].
    exists sgl'. split; [eapply SE_IfF; [rewrite (Ha c Hc'); exact Hc|exact Ht|exact E]|].
    split; assumption.
  - pose proof (fresh_head _ _ _ Hf) as Hc'.
    exists sgl. split; [eapply SE_WhileF; [rewrite (Ha c Hc'); exact Hc|exact Ht]|].
    split; [exact Ha|intros; reflexivity].
  - pose proof (fresh_head _ _ _ Hf) as Hc'.
    destruct (IHb F sgl (fresh_tail _ _ _ Hf) Ha) as [sgl1 [E1 [A1 U1]]].
    destruct (IHw F sgl1 Hf A1) as [sgl2 [E2 [A2 U2]]].
    exists sgl2. split; [eapply SE_WhileT; [rewrite (Ha c Hc'); exact Hc|exact Ht|exact E1|exact E2|exact Htr]|].
    split; [exact A2|]. intros z Hz. rewrite U2 by exact Hz. apply U1; exact Hz.
  - pose proof (fresh_head _ _ _ Hf) as Hx.
    pose proof (fresh_members _ _ (fresh_tail _ _ _ Hf)) as Hys.
    exists (supd sgl x v). split.
    + eapply SE_Call; [exact Hg| |exact Hlen|exact Hb|exact Hr].
      rewrite (agree_lookup_all F sg sgl ys Ha Hys). exact Hl.
    + split; [apply agree_supd; exact Ha|].
      intros z' Hz'. apply supd_other. intro E; subst; contradiction.
  - pose proof (fresh_head _ _ _ Hf) as Hz'.
    pose proof (fresh_members _ _ (fresh_tail _ _ _ Hf)) as Hys.
    exists (supd sgl z v). split.
    + eapply SE_CallIO; [exact Hg|rewrite (Ha z Hz'); exact Hz| |exact Hlen|exact Hb|exact Hr].
      rewrite (agree_lookup_all F sg sgl ys Ha Hys). exact Hl.
    + split; [apply agree_supd; exact Ha|].
      intros z0 Hz0. apply supd_other. intro E; subst; contradiction.
  - pose proof (fresh_head _ _ _ Hf) as Ht'.
    pose proof (fresh_tail _ _ _ Hf) as Hr.
    pose proof (fresh_head _ _ _ Hr) as Hy'.
    destruct (IHs F (supd sgl t cv) (fresh_tail _ _ _ Hr) (agree_supd F sg sgl t cv Ha))
      as [sgl1 [E [A U]]].
    exists (supd sgl1 y c'). split.
    + eapply SE_Focus; [rewrite (Ha y Hy'); exact Hy|exact Hget|exact E| |exact Hset].
      rewrite (A t Ht'). exact Ht.
    + split; [apply agree_supd; exact A|].
      intros z Hz. rewrite supd_other by (intro E'; subst; contradiction).
      rewrite U by exact Hz. apply supd_other. intro E'; subst; contradiction.
  - pose proof (fresh_head _ _ _ Hf) as Hy'.
    pose proof (fresh_tail _ _ _ Hf) as Hxs.
    exists (supd_list sgl xs cs). split.
    + apply SE_Unpack; [rewrite (Ha y Hy'); exact Hy|exact Hlen].
    + split; [apply agree_supd_list; exact Ha|].
      intros z Hz. apply supd_list_out. exact (Hxs z Hz).
  - pose proof (fresh_head _ _ _ Hf) as Hx.
    pose proof (fresh_tail _ _ _ Hf) as Hr.
    pose proof (fresh_members _ _ (fresh_appl _ _ _ Hr)) as Hys.
    destruct (IHs F (supd sgl x (f vs)) (fresh_appr _ _ _ Hr) (agree_supd F sg sgl x (f vs) Ha))
      as [sgl1 [E [A U]]].
    exists sgl1. split.
    + eapply SE_Region; [|exact E]. rewrite (agree_lookup_all F sg sgl ys Ha Hys). exact Hl.
    + split; [exact A|]. intros z Hz. rewrite U by exact Hz.
      apply supd_other. intro E'; subst; contradiction.
Qed.

(* ------------------------------------------------------------------ *)
(* Lowering                                                             *)
(* ------------------------------------------------------------------ *)

Definition status_code (o : Outcome) : nat :=
  match o with ONorm => 0 | OBrk => 1 | OCont => 2 | ORet => 3 | OErr => 4 end.

Definition set_status (k : nat) : list SVal -> SVal := fun _ => SLeaf k.

Definition status_test (P : nat -> bool) : list SVal -> SVal :=
  fun vs => match vs with [SLeaf k] => SLeaf (if P k then 1 else 0) | _ => SLeaf 0 end.

Definition truth_flag : list SVal -> SVal :=
  fun vs => match vs with [v] => SLeaf (if truthy v then 1 else 0) | _ => SLeaf 0 end.

Definition is_norm (k : nat) : bool := Nat.eqb k 0.
Definition is_err (k : nat) : bool := Nat.eqb k 4.
(* The loop keeps running after a normal or continue body. *)
Definition runs_on (k : nat) : bool := Nat.eqb k 0 || Nat.eqb k 2.
(* The loop consumes its own break and continue. *)
Definition loop_consumed (k : nat) : bool := Nat.eqb k 1 || Nat.eqb k 2.

Lemma status_test_val : forall P k, status_test P [SLeaf k] = SLeaf (if P k then 1 else 0).
Proof. reflexivity. Qed.

Lemma truth_flag_val : forall v, truth_flag [v] = SLeaf (if truthy v then 1 else 0).
Proof. reflexivity. Qed.

Definition guard (st gd : Var) (P : nat -> bool) (t : SStmt) : SStmt :=
  SSeq (SDef gd (status_test P) [st]) (SIf gd t SSkip).

(* After the body: compute whether to run again, then consume break and
   continue. The condition is read only when the loop would run again. *)
Definition loop_tail (st gd w c : Var) : SStmt :=
  SSeq (SDef gd (status_test runs_on) [st])
    (SSeq (SIf gd (SDef w truth_flag [c]) (SDef w (set_status 0) []))
      (guard st gd loop_consumed (SDef st (set_status 0) []))).

Definition loop_step (st gd w c : Var) (lb : SStmt) : SStmt := SSeq lb (loop_tail st gd w c).

(* [keep] is added to every lowered loop head: the routine's outputs and
   initialized locals stay live across the status-guarded paths. *)
Fixpoint lower (st gd w : Var) (keep : list Var) (s : XStmt) : SStmt :=
  match s with
  | XS s0 => s0
  | XSeq a b => SSeq (lower st gd w keep a) (guard st gd is_norm (lower st gd w keep b))
  | XIf c a b => SIf c (lower st gd w keep a) (lower st gd w keep b)
  | XLoop c h b =>
      SSeq (SDef w truth_flag [c])
        (SWhile w (w :: st :: c :: h ++ keep) (loop_step st gd w c (lower st gd w keep b)))
  | XBreak => SDef st (set_status 1) []
  | XContinue => SDef st (set_status 2) []
  | XReturn => SDef st (set_status 3) []
  | XThrow => SDef st (set_status 4) []
  | XTry b h =>
      SSeq (lower st gd w keep b)
        (guard st gd is_err (SSeq (SDef st (set_status 0) []) (lower st gd w keep h)))
  end.

(* ------------------------------------------------------------------ *)
(* Gadget executions                                                    *)
(* ------------------------------------------------------------------ *)

Lemma slookup_all_one : forall sg a va, sg a = Some va -> slookup_all sg [a] = Some [va].
Proof. intros sg a va H. simpl. rewrite H. reflexivity. Qed.

Lemma slookup_all_two : forall sg a b va vb, sg a = Some va -> sg b = Some vb ->
  slookup_all sg [a; b] = Some [va; vb].
Proof. intros sg a b va vb Ha Hb. simpl. rewrite Ha, Hb. reflexivity. Qed.

Lemma slookup_all_one_inv : forall sg a va, slookup_all sg [a] = Some [va] -> sg a = Some va.
Proof.
  intros sg a va H. simpl in H. destruct (sg a); [|discriminate].
  injection H as H. subst. reflexivity.
Qed.

Lemma slookup_all_two_inv : forall sg a b va vb, slookup_all sg [a; b] = Some [va; vb] ->
  sg a = Some va /\ sg b = Some vb.
Proof.
  intros sg a b va vb H. simpl in H. destruct (sg a); [|discriminate].
  destruct (sg b); [|discriminate]. injection H as H1 H2. subst. split; reflexivity.
Qed.

Lemma in3_1 : forall (a b c : Var), In a [a; b; c]. Proof. intros; left; reflexivity. Qed.
Lemma in3_2 : forall (a b c : Var), In b [a; b; c]. Proof. intros; right; left; reflexivity. Qed.
Lemma in3_3 : forall (a b c : Var), In c [a; b; c]. Proof. intros; right; right; left; reflexivity. Qed.

Lemma set_status_exec : forall funs procs st gd w sg sgl k, agree [st; gd; w] sg sgl ->
  sexec funs procs sgl (SDef st (set_status k) []) (supd sgl st (SLeaf k)) [] /\
  agree [st; gd; w] sg (supd sgl st (SLeaf k)) /\ supd sgl st (SLeaf k) st = Some (SLeaf k).
Proof.
  intros funs procs st gd w sg sgl k Ha.
  split; [apply (SE_Def funs procs sgl st (set_status k) [] []); reflexivity|].
  split; [apply agree_set; [apply in3_1|exact Ha]|apply supd_same].
Qed.

Lemma guard_skip : forall funs procs st gd w, st <> gd ->
  forall P t sg sgl k, agree [st; gd; w] sg sgl -> sgl st = Some (SLeaf k) -> P k = false ->
  exists sgl', sexec funs procs sgl (guard st gd P t) sgl' [] /\ agree [st; gd; w] sg sgl' /\
    sgl' st = Some (SLeaf k).
Proof.
  intros funs procs st gd w Hsg P t sg sgl k Ha Hk Hp.
  exists (supd sgl gd (status_test P [SLeaf k])). split.
  - eapply SE_Seq with (tr1 := []) (tr2 := []).
    + apply SE_Def. apply slookup_all_one. exact Hk.
    + eapply SE_IfF; [apply supd_same|rewrite status_test_val, Hp; reflexivity|apply SE_Skip].
    + reflexivity.
  - split; [apply agree_set; [apply in3_2|exact Ha]|].
    rewrite supd_other by congruence. exact Hk.
Qed.

Lemma guard_run : forall funs procs st gd P t sgl k sgl' tr, sgl st = Some (SLeaf k) -> P k = true ->
  sexec funs procs (supd sgl gd (status_test P [SLeaf k])) t sgl' tr ->
  sexec funs procs sgl (guard st gd P t) sgl' tr.
Proof.
  intros funs procs st gd P t sgl k sgl' tr Hk Hp E.
  eapply SE_Seq with (tr1 := []) (tr2 := tr).
  - apply SE_Def. apply slookup_all_one. exact Hk.
  - eapply SE_IfT; [apply supd_same|rewrite status_test_val, Hp; reflexivity|exact E].
  - reflexivity.
Qed.

Lemma guard_entry : forall st gd w, st <> gd ->
  forall sg sgl k v, agree [st; gd; w] sg sgl -> sgl st = Some (SLeaf k) ->
  agree [st; gd; w] sg (supd sgl gd v) /\ supd sgl gd v st = Some (SLeaf k).
Proof.
  intros st gd w Hsg sg sgl k v Ha Hk. split; [apply agree_set; [apply in3_2|exact Ha]|].
  rewrite supd_other by congruence. exact Hk.
Qed.

Lemma loop_tail_exec : forall funs procs st gd w, st <> gd -> st <> w -> gd <> w ->
  forall c sg sgl k, ~ In c [st; gd; w] -> agree [st; gd; w] sg sgl -> sgl st = Some (SLeaf k) ->
  (runs_on k = true -> exists cv, sg c = Some cv) ->
  exists X, sexec funs procs sgl (loop_tail st gd w c) X [] /\ agree [st; gd; w] sg X /\
    X st = Some (SLeaf (if loop_consumed k then 0 else k)) /\
    (runs_on k = false -> X w = Some (SLeaf 0)) /\
    (forall cv, runs_on k = true -> sg c = Some cv -> X w = Some (SLeaf (if truthy cv then 1 else 0))).
Proof.
  intros funs procs st gd w Hsg Hsw Hgw c sg sgl k Hc Ha Hk Hrun.
  assert (Hcg : c <> gd) by (intro E; subst; apply Hc; apply in3_2).
  assert (Hcw : c <> w) by (intro E; subst; apply Hc; apply in3_3).
  set (E1 := supd sgl gd (status_test runs_on [SLeaf k])).
  assert (A1 : agree [st; gd; w] sg E1) by (apply agree_set; [apply in3_2|exact Ha]).
  assert (K1 : E1 st = Some (SLeaf k)) by (unfold E1; rewrite supd_other by congruence; exact Hk).
  assert (Def1 : sexec funs procs sgl (SDef gd (status_test runs_on) [st]) E1 [])
    by (apply SE_Def; apply slookup_all_one; exact Hk).
  destruct (runs_on k) eqn:Er.
  - destruct (Hrun eq_refl) as [cv Hcv].
    set (E2 := supd E1 w (truth_flag [cv])).
    assert (A2 : agree [st; gd; w] sg E2) by (apply agree_set; [apply in3_3|exact A1]).
    assert (K2 : E2 st = Some (SLeaf k)) by (unfold E2; rewrite supd_other by congruence; exact K1).
    assert (W2 : E2 w = Some (SLeaf (if truthy cv then 1 else 0)))
      by (unfold E2; rewrite supd_same; reflexivity).
    assert (If2 : sexec funs procs E1 (SIf gd (SDef w truth_flag [c]) (SDef w (set_status 0) [])) E2 []).
    { eapply SE_IfT; [unfold E1; apply supd_same|rewrite status_test_val, Er; reflexivity|].
      apply SE_Def. apply slookup_all_one. unfold E1. rewrite supd_other by congruence.
      rewrite (Ha c Hc). exact Hcv. }
    set (E3 := supd E2 gd (status_test loop_consumed [SLeaf k])).
    assert (A3 : agree [st; gd; w] sg E3) by (apply agree_set; [apply in3_2|exact A2]).
    assert (K3 : E3 st = Some (SLeaf k)) by (unfold E3; rewrite supd_other by congruence; exact K2).
    assert (W3 : E3 w = Some (SLeaf (if truthy cv then 1 else 0)))
      by (unfold E3; rewrite supd_other by congruence; exact W2).
    destruct (loop_consumed k) eqn:El.
    + exists (supd E3 st (SLeaf 0)). split.
      * eapply SE_Seq with (tr1 := []) (tr2 := []); [exact Def1| |reflexivity].
        eapply SE_Seq with (tr1 := []) (tr2 := []); [exact If2| |reflexivity].
        eapply guard_run; [exact K2|exact El|].
        apply (SE_Def funs procs E3 st (set_status 0) [] []); reflexivity.
      * split; [apply agree_set; [apply in3_1|exact A3]|].
        split; [apply supd_same|].
        split; [intro F'; discriminate|].
        intros cv' _ Hcv'. rewrite Hcv in Hcv'. injection Hcv' as Hcv'. subst cv'.
        rewrite supd_other by congruence. exact W3.
    + exists E3. split.
      * eapply SE_Seq with (tr1 := []) (tr2 := []); [exact Def1| |reflexivity].
        eapply SE_Seq with (tr1 := []) (tr2 := []); [exact If2| |reflexivity].
        eapply SE_Seq with (tr1 := []) (tr2 := []).
        -- apply SE_Def. apply slookup_all_one. exact K2.
        -- eapply SE_IfF; [unfold E3; apply supd_same|rewrite status_test_val, El; reflexivity|apply SE_Skip].
        -- reflexivity.
      * split; [exact A3|]. split; [exact K3|].
        split; [intro F'; discriminate|].
        intros cv' _ Hcv'. rewrite Hcv in Hcv'. injection Hcv' as Hcv'. subst cv'. exact W3.
  - set (E2 := supd E1 w (set_status 0 [])).
    assert (A2 : agree [st; gd; w] sg E2) by (apply agree_set; [apply in3_3|exact A1]).
    assert (K2 : E2 st = Some (SLeaf k)) by (unfold E2; rewrite supd_other by congruence; exact K1).
    assert (W2 : E2 w = Some (SLeaf 0)) by (unfold E2; rewrite supd_same; reflexivity).
    assert (If2 : sexec funs procs E1 (SIf gd (SDef w truth_flag [c]) (SDef w (set_status 0) [])) E2 []).
    { eapply SE_IfF; [unfold E1; apply supd_same|rewrite status_test_val, Er; reflexivity|].
      apply (SE_Def funs procs E1 w (set_status 0) [] []); reflexivity. }
    set (E3 := supd E2 gd (status_test loop_consumed [SLeaf k])).
    assert (A3 : agree [st; gd; w] sg E3) by (apply agree_set; [apply in3_2|exact A2]).
    assert (K3 : E3 st = Some (SLeaf k)) by (unfold E3; rewrite supd_other by congruence; exact K2).
    assert (W3 : E3 w = Some (SLeaf 0)) by (unfold E3; rewrite supd_other by congruence; exact W2).
    destruct (loop_consumed k) eqn:El.
    + exists (supd E3 st (SLeaf 0)). split.
      * eapply SE_Seq with (tr1 := []) (tr2 := []); [exact Def1| |reflexivity].
        eapply SE_Seq with (tr1 := []) (tr2 := []); [exact If2| |reflexivity].
        eapply guard_run; [exact K2|exact El|].
        apply (SE_Def funs procs E3 st (set_status 0) [] []); reflexivity.
      * split; [apply agree_set; [apply in3_1|exact A3]|].
        split; [apply supd_same|].
        split; [intros _; rewrite supd_other by congruence; exact W3|].
        intros cv' F'; discriminate.
    + exists E3. split.
      * eapply SE_Seq with (tr1 := []) (tr2 := []); [exact Def1| |reflexivity].
        eapply SE_Seq with (tr1 := []) (tr2 := []); [exact If2| |reflexivity].
        eapply SE_Seq with (tr1 := []) (tr2 := []).
        -- apply SE_Def. apply slookup_all_one. exact K2.
        -- eapply SE_IfF; [unfold E3; apply supd_same|rewrite status_test_val, El; reflexivity|apply SE_Skip].
        -- reflexivity.
      * split; [exact A3|]. split; [exact K3|].
        split; [intros _; exact W3|].
        intros cv' F'; discriminate.
Qed.

(* ------------------------------------------------------------------ *)
(* Simulation                                                           *)
(* ------------------------------------------------------------------ *)

Section Simulation.
Variable funs : SFunTable.
Variable procs : SProcTable.
Variable keep : list Var.

Definition LSIM (st gd w : Var) (sg : SEnv) (s : XStmt) (sg' : SEnv) (tr : list SVal) (o : Outcome) :=
  forall sgl, agree [st; gd; w] sg sgl -> sgl st = Some (SLeaf 0) ->
  exists sgl', sexec funs procs sgl (lower st gd w keep s) sgl' tr /\
    agree [st; gd; w] sg' sgl' /\ sgl' st = Some (SLeaf (status_code o)).

(* The loop after its entry test: the run flag already holds the test. *)
Definition WSIM (st gd w : Var) (sg : SEnv) (s : XStmt) (sg' : SEnv) (tr : list SVal) (o : Outcome) :=
  match s with
  | XLoop c h b =>
      forall sgl, agree [st; gd; w] sg sgl -> sgl st = Some (SLeaf 0) ->
      (forall cv, sg c = Some cv -> sgl w = Some (SLeaf (if truthy cv then 1 else 0))) ->
      exists sgl', sexec funs procs sgl
          (SWhile w (w :: st :: c :: h ++ keep) (loop_step st gd w c (lower st gd w keep b))) sgl' tr /\
        agree [st; gd; w] sg' sgl' /\ sgl' st = Some (SLeaf (status_code o))
  | _ => True
  end.

Lemma loop_reads_condition : forall sg c h b sg' tr o,
  xsexec funs procs sg (XLoop c h b) sg' tr o -> exists v, sg c = Some v.
Proof. intros sg c h b sg' tr o H. inversion H; subst; eexists; eassumption. Qed.

Lemma nodup3 : forall (a b c : Var), NoDup [a; b; c] -> a <> b /\ a <> c /\ b <> c.
Proof.
  intros a b c H. inversion H as [|? ? Ha Hr]; subst. inversion Hr as [|? ? Hb _]; subst.
  split; [intro E; subst; apply Ha; left; reflexivity|].
  split; [intro E; subst; apply Ha; right; left; reflexivity|].
  intro E; subst; apply Hb; left; reflexivity.
Qed.

Lemma loop_entry : forall st gd w sg c h b sg' tr o,
  NoDup [st; gd; w] -> ~ In c [st; gd; w] ->
  (exists v, sg c = Some v) -> WSIM st gd w sg (XLoop c h b) sg' tr o ->
  LSIM st gd w sg (XLoop c h b) sg' tr o.
Proof.
  intros st gd w sg c h b sg' tr o HN Hc [v Hv] HW sgl Ha Hst.
  destruct (nodup3 _ _ _ HN) as [Hsg [Hsw Hgw]].
  set (E1 := supd sgl w (truth_flag [v])).
  assert (A1 : agree [st; gd; w] sg E1) by (apply agree_set; [right; right; left; reflexivity|exact Ha]).
  assert (K1 : E1 st = Some (SLeaf 0)) by (unfold E1; rewrite supd_other by congruence; exact Hst).
  assert (W1 : forall cv, sg c = Some cv -> E1 w = Some (SLeaf (if truthy cv then 1 else 0))).
  { intros cv Hcv. rewrite Hv in Hcv. injection Hcv as Hcv. subst cv. unfold E1. apply supd_same. }
  destruct (HW E1 A1 K1 W1) as [sgl' [E [A K]]].
  exists sgl'. split; [|split; assumption].
  cbn [lower]. eapply SE_Seq with (tr1 := []) (tr2 := tr); [|exact E|reflexivity].
  apply SE_Def. apply slookup_all_one. rewrite (Ha c Hc). exact Hv.
Qed.

Theorem lower_sim_both : forall sg s sg' tr o, xsexec funs procs sg s sg' tr o ->
  forall st gd w, NoDup [st; gd; w] -> xfresh [st; gd; w] s ->
  LSIM st gd w sg s sg' tr o /\ WSIM st gd w sg s sg' tr o.
Proof.
  intros sg s sg' tr o Hx.
  induction Hx as
    [ sg s0 sg' tr Hs0
    | sg1 sg2 sg3 a b tr1 tr2 tr o Ha IHa Hb IHb Htr
    | sg sg' a b tr o Ha IHa Ho
    | sg sg' c a b v tr o Hc Ht Ha IHa
    | sg sg' c a b v tr o Hc Ht Hb IHb
    | sg c h b v Hc Ht
    | sg sg1 sg2 c h b v tr1 tr2 tr o1 o Hc Ht Hb IHb Ho1 Hloop IHloop Htr
    | sg sg1 c h b v tr Hc Ht Hb IHb
    | sg sg1 c h b v tr o Hc Ht Hb IHb Ho
    | sg | sg | sg | sg
    | sg sg' b h tr o Hb IHb Ho
    | sg sg1 sg2 b h tr1 tr2 tr o Hb IHb Hh IHh Htr ];
    intros st gd w HN Hf; unfold xfresh in Hf; cbn [xvars] in Hf;
    destruct (nodup3 _ _ _ HN) as [Hsg [Hsw Hgw]].
  - (* exit-free statement *)
    split; [|exact I]. intros sgl Ha Hst.
    destruct (sexec_frame funs procs sg s0 sg' tr Hs0 [st; gd; w] sgl Hf Ha) as [sgl' [E [A U]]].
    exists sgl'. split; [exact E|split; [exact A|]].
    rewrite U by (left; reflexivity). exact Hst.
  - (* sequence, first part normal *)
    split; [|exact I]. intros sgl Hag Hst.
    destruct (proj1 (IHa st gd w HN (fresh_appl _ _ _ Hf)) sgl Hag Hst) as [sgl1 [E1 [A1 K1]]].
    simpl in K1.
    set (G := supd sgl1 gd (status_test is_norm [SLeaf 0])).
    destruct (guard_entry st gd w Hsg sg2 sgl1 0 (status_test is_norm [SLeaf 0]) A1 K1)
      as [AG KG].
    destruct (proj1 (IHb st gd w HN (fresh_appr _ _ _ Hf)) G AG KG) as [sgl2 [E2 [A2 K2]]].
    exists sgl2. split; [|split; assumption].
    cbn [lower]. eapply SE_Seq; [exact E1| |exact Htr].
    eapply guard_run; [exact K1|reflexivity|exact E2].
  - (* sequence, first part exits *)
    split; [|exact I]. intros sgl Hag Hst.
    destruct (proj1 (IHa st gd w HN (fresh_appl _ _ _ Hf)) sgl Hag Hst) as [sgl1 [E1 [A1 K1]]].
    assert (Hn : is_norm (status_code o) = false) by (destruct o; try reflexivity; contradiction).
    destruct (guard_skip funs procs st gd w Hsg is_norm (lower st gd w keep b) sg' sgl1 (status_code o) A1 K1 Hn)
      as [sgl2 [E2 [A2 K2]]].
    exists sgl2. split; [|split; assumption].
    cbn [lower]. eapply SE_Seq with (tr1 := tr) (tr2 := []); [exact E1|exact E2|rewrite app_nil_r; reflexivity].
  - (* if, true *)
    split; [|exact I]. intros sgl Hag Hst.
    pose proof (fresh_head _ _ _ Hf) as Hc'.
    destruct (proj1 (IHa st gd w HN (fresh_appl _ _ _ (fresh_tail _ _ _ Hf))) sgl Hag Hst)
      as [sgl' [E [A K]]].
    exists sgl'. split; [|split; assumption].
    cbn [lower]. eapply SE_IfT; [rewrite (Hag c Hc'); exact Hc|exact Ht|exact E].
  - (* if, false *)
    split; [|exact I]. intros sgl Hag Hst.
    pose proof (fresh_head _ _ _ Hf) as Hc'.
    destruct (proj1 (IHb st gd w HN (fresh_appr _ _ _ (fresh_tail _ _ _ Hf))) sgl Hag Hst)
      as [sgl' [E [A K]]].
    exists sgl'. split; [|split; assumption].
    cbn [lower]. eapply SE_IfF; [rewrite (Hag c Hc'); exact Hc|exact Ht|exact E].
  - (* loop, test false *)
    pose proof (fresh_head _ _ _ Hf) as Hc'.
    assert (HW : WSIM st gd w sg (XLoop c h b) sg [] ONorm).
    { cbn [WSIM]. intros sgl Hag Hst Hw.
      exists sgl. split; [|split; [exact Hag|exact Hst]].
      eapply SE_WhileF; [rewrite (Hw v Hc), Ht; reflexivity|reflexivity]. }
    split; [|exact HW]. apply loop_entry; [exact HN|exact Hc'|exists v; exact Hc|exact HW].
  - (* loop, body normal or continue, then the rest of the loop *)
    pose proof (fresh_head _ _ _ Hf) as Hc'.
    pose proof (fresh_tail _ _ _ Hf) as Hfb.
    assert (HW : WSIM st gd w sg (XLoop c h b) sg2 tr o).
    { cbn [WSIM]. intros sgl Hag Hst Hw.
      destruct (proj1 (IHb st gd w HN Hfb) sgl Hag Hst) as [sgl1 [E1 [A1 K1]]].
      assert (Hr : runs_on (status_code o1) = true) by (destruct Ho1; subst; reflexivity).
      destruct (loop_reads_condition _ _ _ _ _ _ _ Hloop) as [cv1 Hcv1].
      destruct (loop_tail_exec funs procs st gd w Hsg Hsw Hgw c sg1 sgl1 (status_code o1) Hc' A1 K1
        (fun _ => ex_intro _ cv1 Hcv1)) as [X [EX [AX [KX [_ WX]]]]].
      assert (KX0 : X st = Some (SLeaf 0)) by (rewrite KX; destruct Ho1; subst; reflexivity).
      destruct (proj2 (IHloop st gd w HN Hf) X AX KX0 (fun cv H => WX cv Hr H)) as [sgl2 [E2 [A2 K2]]].
      exists sgl2. split; [|split; assumption].
      eapply SE_WhileT; [rewrite (Hw v Hc), Ht; reflexivity|reflexivity| |exact E2|].
      - eapply SE_Seq; [exact E1|exact EX|reflexivity].
      - rewrite app_nil_r. exact Htr. }
    split; [|exact HW]. apply loop_entry; [exact HN|exact Hc'|exists v; exact Hc|exact HW].
  - (* loop, body breaks *)
    pose proof (fresh_head _ _ _ Hf) as Hc'.
    pose proof (fresh_tail _ _ _ Hf) as Hfb.
    assert (HW : WSIM st gd w sg (XLoop c h b) sg1 tr ONorm).
    { cbn [WSIM]. intros sgl Hag Hst Hw.
      destruct (proj1 (IHb st gd w HN Hfb) sgl Hag Hst) as [sgl1 [E1 [A1 K1]]].
      destruct (loop_tail_exec funs procs st gd w Hsg Hsw Hgw c sg1 sgl1 1 Hc' A1 K1
        (fun F' => match Bool.diff_false_true F' with end)) as [X [EX [AX [KX [WX _]]]]].
      exists X. split; [|split; [exact AX|exact KX]].
      eapply SE_WhileT; [rewrite (Hw v Hc), Ht; reflexivity|reflexivity| | |].
      - eapply SE_Seq; [exact E1|exact EX|reflexivity].
      - eapply SE_WhileF; [rewrite (WX eq_refl); reflexivity|reflexivity].
      - rewrite !app_nil_r. reflexivity. }
    split; [|exact HW]. apply loop_entry; [exact HN|exact Hc'|exists v; exact Hc|exact HW].
  - (* loop, body returns or raises *)
    pose proof (fresh_head _ _ _ Hf) as Hc'.
    pose proof (fresh_tail _ _ _ Hf) as Hfb.
    assert (HW : WSIM st gd w sg (XLoop c h b) sg1 tr o).
    { cbn [WSIM]. intros sgl Hag Hst Hw.
      destruct (proj1 (IHb st gd w HN Hfb) sgl Hag Hst) as [sgl1 [E1 [A1 K1]]].
      assert (Hr : runs_on (status_code o) = false) by (destruct Ho; subst; reflexivity).
      assert (Hl : loop_consumed (status_code o) = false) by (destruct Ho; subst; reflexivity).
      destruct (loop_tail_exec funs procs st gd w Hsg Hsw Hgw c sg1 sgl1 (status_code o) Hc' A1 K1
        (fun F' => match Bool.diff_false_true (eq_trans (eq_sym Hr) F') with end))
        as [X [EX [AX [KX [WX _]]]]].
      exists X. split; [|split; [exact AX|rewrite KX, Hl; reflexivity]].
      eapply SE_WhileT; [rewrite (Hw v Hc), Ht; reflexivity|reflexivity| | |].
      - eapply SE_Seq; [exact E1|exact EX|reflexivity].
      - eapply SE_WhileF; [rewrite (WX Hr); reflexivity|reflexivity].
      - rewrite !app_nil_r. reflexivity. }
    split; [|exact HW]. apply loop_entry; [exact HN|exact Hc'|exists v; exact Hc|exact HW].
  - (* break *)
    split; [|exact I]. intros sgl Hag Hst.
    destruct (set_status_exec funs procs st gd w sg sgl 1 Hag) as [E [A K]].
    exists (supd sgl st (SLeaf 1)). split; [exact E|split; [exact A|exact K]].
  - (* continue *)
    split; [|exact I]. intros sgl Hag Hst.
    destruct (set_status_exec funs procs st gd w sg sgl 2 Hag) as [E [A K]].
    exists (supd sgl st (SLeaf 2)). split; [exact E|split; [exact A|exact K]].
  - (* return *)
    split; [|exact I]. intros sgl Hag Hst.
    destruct (set_status_exec funs procs st gd w sg sgl 3 Hag) as [E [A K]].
    exists (supd sgl st (SLeaf 3)). split; [exact E|split; [exact A|exact K]].
  - (* raise *)
    split; [|exact I]. intros sgl Hag Hst.
    destruct (set_status_exec funs procs st gd w sg sgl 4 Hag) as [E [A K]].
    exists (supd sgl st (SLeaf 4)). split; [exact E|split; [exact A|exact K]].
  - (* try, body does not raise *)
    split; [|exact I]. intros sgl Hag Hst.
    destruct (proj1 (IHb st gd w HN (fresh_appl _ _ _ Hf)) sgl Hag Hst) as [sgl1 [E1 [A1 K1]]].
    assert (He : is_err (status_code o) = false) by (destruct o; try reflexivity; contradiction).
    destruct (guard_skip funs procs st gd w Hsg is_err
      (SSeq (SDef st (set_status 0) []) (lower st gd w keep h)) sg' sgl1 (status_code o) A1 K1 He)
      as [sgl2 [E2 [A2 K2]]].
    exists sgl2. split; [|split; assumption].
    cbn [lower]. eapply SE_Seq with (tr1 := tr) (tr2 := []); [exact E1|exact E2|rewrite app_nil_r; reflexivity].
  - (* try, body raises, handler runs *)
    split; [|exact I]. intros sgl Hag Hst.
    destruct (proj1 (IHb st gd w HN (fresh_appl _ _ _ Hf)) sgl Hag Hst) as [sgl1 [E1 [A1 K1]]].
    simpl in K1.
    set (G := supd sgl1 gd (status_test is_err [SLeaf 4])).
    destruct (guard_entry st gd w Hsg sg1 sgl1 4 (status_test is_err [SLeaf 4]) A1 K1)
      as [AG KG].
    destruct (set_status_exec funs procs st gd w sg1 G 0 AG) as [ES [AS KS]].
    destruct (proj1 (IHh st gd w HN (fresh_appr _ _ _ Hf)) (supd G st (SLeaf 0)) AS KS)
      as [sgl2 [E2 [A2 K2]]].
    exists sgl2. split; [|split; assumption].
    cbn [lower]. eapply SE_Seq; [exact E1| |exact Htr].
    eapply guard_run; [exact K1|reflexivity|].
    eapply SE_Seq with (tr1 := []) (tr2 := tr2); [exact ES|exact E2|reflexivity].
Qed.

Theorem lower_sim : forall st gd w s, NoDup [st; gd; w] -> xfresh [st; gd; w] s ->
  forall sg sg' tr o, xsexec funs procs sg s sg' tr o ->
  forall sgl, agree [st; gd; w] sg sgl -> sgl st = Some (SLeaf 0) ->
  exists sgl', sexec funs procs sgl (lower st gd w keep s) sgl' tr /\
    agree [st; gd; w] sg' sgl' /\ sgl' st = Some (SLeaf (status_code o)).
Proof.
  intros st gd w s HN Hf sg sg' tr o Hx.
  exact (proj1 (lower_sim_both sg s sg' tr o Hx st gd w HN Hf)).
Qed.

End Simulation.

(* ------------------------------------------------------------------ *)
(* Extra bindings do not change a run                                   *)
(* ------------------------------------------------------------------ *)

Definition extends (sg sgl : SEnv) : Prop := forall x v, sg x = Some v -> sgl x = Some v.

Lemma extends_supd : forall sg sgl x v, extends sg sgl -> extends (supd sg x v) (supd sgl x v).
Proof.
  intros sg sgl x v H y u E. unfold supd in *.
  destruct (Nat.eq_dec x y); [exact E|apply H; exact E].
Qed.

Lemma extends_supd_list : forall xs cs sg sgl, extends sg sgl ->
  extends (supd_list sg xs cs) (supd_list sgl xs cs).
Proof.
  induction xs as [|x xs IH]; intros cs sg sgl H; destruct cs as [|c cs]; simpl; try exact H.
  apply extends_supd. apply IH. exact H.
Qed.

Lemma extends_lookup_all : forall sg sgl ys vs, extends sg sgl ->
  slookup_all sg ys = Some vs -> slookup_all sgl ys = Some vs.
Proof.
  intros sg sgl ys. induction ys as [|y ys IH]; intros vs H E; simpl in *; [exact E|].
  destruct (sg y) as [v|] eqn:Ey; [|discriminate].
  destruct (slookup_all sg ys) as [r|] eqn:Er; [|discriminate].
  rewrite (H y v Ey). rewrite (IH r H eq_refl). exact E.
Qed.

Lemma sexec_extend : forall funs procs sg s sg' tr, sexec funs procs sg s sg' tr ->
  forall sgl, extends sg sgl -> exists sgl', sexec funs procs sgl s sgl' tr /\ extends sg' sgl'.
Proof.
  intros funs procs sg s sg' tr Hs.
  induction Hs as
    [ sg
    | sg x f ys vs Hl
    | sg x y v Hy
    | sg x ys vs Hl
    | sg x y cs v Hx Hy
    | sg x y i cs v Hy Hi
    | sg y v Hy
    | sg1 sg2 sg3 s1 s2 tr1 tr2 tr H1 IH1 H2 IH2 Htr
    | sg sg' c s1 s2 v tr Hc Ht H1 IH1
    | sg sg' c s1 s2 v tr Hc Ht H2 IH2
    | sg c h b v Hc Ht
    | sg sg1 sg2 c h b v tr1 tr2 tr Hc Ht Hb IHb Hw IHw Htr
    | sg x g ys vs ps body ret sgc tr v Hg Hl Hlen Hb IHb Hr
    | sg g z ys vz vs io ps body sgc tr v Hg Hz Hl Hlen Hb IHb Hr
    | sg t y p s c cv sg' tr cv' c' Hy Hget Hs IHs Ht Hset
    | sg y xs cs Hy Hlen
    | sg x f ys s vs sg' tr Hl Hs IHs ];
    intros sgl He.
  - exists sgl. split; [constructor|exact He].
  - exists (supd sgl x (f vs)). split; [apply SE_Def; exact (extends_lookup_all _ _ _ _ He Hl)|].
    apply extends_supd; exact He.
  - exists (supd sgl x v). split; [apply SE_Copy; exact (He _ _ Hy)|apply extends_supd; exact He].
  - exists (supd sgl x (SNode vs)). split; [apply SE_Pack; exact (extends_lookup_all _ _ _ _ He Hl)|].
    apply extends_supd; exact He.
  - exists (supd sgl x (SNode (cs ++ [v]))).
    split; [apply SE_Push; [exact (He _ _ Hx)|exact (He _ _ Hy)]|apply extends_supd; exact He].
  - exists (supd sgl x v). split; [eapply SE_Field; [exact (He _ _ Hy)|exact Hi]|apply extends_supd; exact He].
  - exists sgl. split; [apply SE_Emit; exact (He _ _ Hy)|exact He].
  - destruct (IH1 sgl He) as [sgl2 [E1 X1]]. destruct (IH2 sgl2 X1) as [sgl3 [E2 X2]].
    exists sgl3. split; [eapply SE_Seq; [exact E1|exact E2|exact Htr]|exact X2].
  - destruct (IH1 sgl He) as [sgl' [E X]].
    exists sgl'. split; [eapply SE_IfT; [exact (He _ _ Hc)|exact Ht|exact E]|exact X].
  - destruct (IH2 sgl He) as [sgl' [E X]].
    exists sgl'. split; [eapply SE_IfF; [exact (He _ _ Hc)|exact Ht|exact E]|exact X].
  - exists sgl. split; [eapply SE_WhileF; [exact (He _ _ Hc)|exact Ht]|exact He].
  - destruct (IHb sgl He) as [sgl1 [E1 X1]]. destruct (IHw sgl1 X1) as [sgl2 [E2 X2]].
    exists sgl2. split; [eapply SE_WhileT; [exact (He _ _ Hc)|exact Ht|exact E1|exact E2|exact Htr]|exact X2].
  - exists (supd sgl x v). split; [|apply extends_supd; exact He].
    eapply SE_Call; [exact Hg|exact (extends_lookup_all _ _ _ _ He Hl)|exact Hlen|exact Hb|exact Hr].
  - exists (supd sgl z v). split; [|apply extends_supd; exact He].
    eapply SE_CallIO; [exact Hg|exact (He _ _ Hz)|exact (extends_lookup_all _ _ _ _ He Hl)|exact Hlen|exact Hb|exact Hr].
  - destruct (IHs (supd sgl t cv) (extends_supd _ _ _ _ He)) as [sgl1 [E X]].
    exists (supd sgl1 y c'). split; [|apply extends_supd; exact X].
    eapply SE_Focus; [exact (He _ _ Hy)|exact Hget|exact E|exact (X _ _ Ht)|exact Hset].
  - exists (supd_list sgl xs cs). split; [apply SE_Unpack; [exact (He _ _ Hy)|exact Hlen]|].
    apply extends_supd_list; exact He.
  - destruct (IHs (supd sgl x (f vs)) (extends_supd _ _ _ _ He)) as [sgl1 [E X]].
    exists sgl1. split; [eapply SE_Region; [exact (extends_lookup_all _ _ _ _ He Hl)|exact E]|exact X].
Qed.

Lemma xsexec_extend : forall funs procs sg s sg' tr o, xsexec funs procs sg s sg' tr o ->
  forall sgl, extends sg sgl -> exists sgl', xsexec funs procs sgl s sgl' tr o /\ extends sg' sgl'.
Proof.
  intros funs procs sg s sg' tr o Hx.
  induction Hx as
    [ sg s0 sg' tr Hs0
    | sg1 sg2 sg3 a b tr1 tr2 tr o Ha IHa Hb IHb Htr
    | sg sg' a b tr o Ha IHa Ho
    | sg sg' c a b v tr o Hc Ht Ha IHa
    | sg sg' c a b v tr o Hc Ht Hb IHb
    | sg c h b v Hc Ht
    | sg sg1 sg2 c h b v tr1 tr2 tr o1 o Hc Ht Hb IHb Ho1 Hloop IHloop Htr
    | sg sg1 c h b v tr Hc Ht Hb IHb
    | sg sg1 c h b v tr o Hc Ht Hb IHb Ho
    | sg | sg | sg | sg
    | sg sg' b h tr o Hb IHb Ho
    | sg sg1 sg2 b h tr1 tr2 tr o Hb IHb Hh IHh Htr ];
    intros sgl He.
  - destruct (sexec_extend _ _ _ _ _ _ Hs0 sgl He) as [sgl' [E X]].
    exists sgl'. split; [apply XE_S; exact E|exact X].
  - destruct (IHa sgl He) as [sgl2 [E1 X1]]. destruct (IHb sgl2 X1) as [sgl3 [E2 X2]].
    exists sgl3. split; [eapply XE_SeqN; [exact E1|exact E2|exact Htr]|exact X2].
  - destruct (IHa sgl He) as [sgl' [E X]].
    exists sgl'. split; [apply XE_SeqX; [exact E|exact Ho]|exact X].
  - destruct (IHa sgl He) as [sgl' [E X]].
    exists sgl'. split; [eapply XE_IfT; [exact (He _ _ Hc)|exact Ht|exact E]|exact X].
  - destruct (IHb sgl He) as [sgl' [E X]].
    exists sgl'. split; [eapply XE_IfF; [exact (He _ _ Hc)|exact Ht|exact E]|exact X].
  - exists sgl. split; [eapply XE_LoopF; [exact (He _ _ Hc)|exact Ht]|exact He].
  - destruct (IHb sgl He) as [sgl1 [E1 X1]]. destruct (IHloop sgl1 X1) as [sgl2 [E2 X2]].
    exists sgl2. split; [eapply XE_LoopT; [exact (He _ _ Hc)|exact Ht|exact E1|exact Ho1|exact E2|exact Htr]|exact X2].
  - destruct (IHb sgl He) as [sgl1 [E X]].
    exists sgl1. split; [eapply XE_LoopB; [exact (He _ _ Hc)|exact Ht|exact E]|exact X].
  - destruct (IHb sgl He) as [sgl1 [E X]].
    exists sgl1. split; [eapply XE_LoopX; [exact (He _ _ Hc)|exact Ht|exact E|exact Ho]|exact X].
  - exists sgl. split; [apply XE_Break|exact He].
  - exists sgl. split; [apply XE_Continue|exact He].
  - exists sgl. split; [apply XE_Return|exact He].
  - exists sgl. split; [apply XE_Throw|exact He].
  - destruct (IHb sgl He) as [sgl' [E X]].
    exists sgl'. split; [apply XE_TryN; [exact E|exact Ho]|exact X].
  - destruct (IHb sgl He) as [sgl1 [E1 X1]]. destruct (IHh sgl1 X1) as [sgl2 [E2 X2]].
    exists sgl2. split; [eapply XE_TryE; [exact E1|exact E2|exact Htr]|exact X2].
Qed.

(* ------------------------------------------------------------------ *)
(* Callee adapter                                                       *)
(* ------------------------------------------------------------------ *)

Definition outcome_tag : list SVal -> SVal :=
  fun vs => match vs with [SLeaf k] => SLeaf (if Nat.eqb k 4 then 0 else 1) | _ => SLeaf 1 end.

Lemma outcome_tag_code : forall o, outcome_tag [SLeaf (status_code o)] = SLeaf (recovery_tag o).
Proof. destruct o; reflexivity. Qed.

(* Status-guarded paths are not distinguished by liveness, so the
   ordinary value and every body local start bound to a unit leaf. *)
Fixpoint init_locals (xs : list Var) : SStmt :=
  match xs with
  | [] => SSkip
  | x :: r => SSeq (SDef x (set_status 0) []) (init_locals r)
  end.

Fixpoint supd_zero (sg : SEnv) (xs : list Var) : SEnv :=
  match xs with
  | [] => sg
  | x :: r => supd_zero (supd sg x (SLeaf 0)) r
  end.

Lemma init_locals_exec : forall funs procs xs sg,
  sexec funs procs sg (init_locals xs) (supd_zero sg xs) [].
Proof.
  intros funs procs xs. induction xs as [|x r IH]; intros sg; simpl; [constructor|].
  eapply SE_Seq with (tr1 := []) (tr2 := []);
    [apply (SE_Def funs procs sg x (set_status 0) [] []); reflexivity|apply IH|reflexivity].
Qed.

Lemma supd_zero_out : forall xs sg z, ~ In z xs -> supd_zero sg xs z = sg z.
Proof.
  induction xs as [|x r IH]; intros sg z Hz; simpl; [reflexivity|].
  rewrite IH by (intro H; apply Hz; right; exact H).
  apply supd_other. intro E; subst. apply Hz. left; reflexivity.
Qed.

Lemma supd_zero_extends : forall xs sg, (forall x, In x xs -> sg x = None) ->
  extends sg (supd_zero sg xs).
Proof.
  intros xs sg H y v E. destruct (in_dec Nat.eq_dec y xs) as [Hin|Hout].
  - rewrite (H y Hin) in E. discriminate.
  - rewrite supd_zero_out by exact Hout. exact E.
Qed.

(* The procedure body for SProcTable: unpack the inout bundle into the
   formals, initialize the value and locals, run the lowered body, then
   repack every formal and the outcome packet [tag; value]. *)
Definition adapter_body (st gd w io : Var) (ios locals : list Var) (value tag packet : Var)
    (body : XStmt) : SStmt :=
  SSeq (SUnpack io ios)
    (SSeq (init_locals (value :: locals))
      (SSeq (SDef st (set_status 0) [])
        (SSeq (lower st gd w (ios ++ value :: locals) body)
          (SSeq (SDef tag outcome_tag [st])
            (SSeq (SPack packet [tag; value]) (SPack io (ios ++ [packet]))))))).

(* Issued names, bundle, value, locals, inout formals and readonly
   parameters are distinct; issued names and the bundle do not occur in
   the body; the body has no unbound break/continue. *)
Definition adapter_admitted (st gd w io : Var) (ps ios locals : list Var) (value tag packet : Var)
    (body : XStmt) : bool :=
  nodupb (st :: gd :: w :: tag :: packet :: io :: value :: locals ++ ios ++ ps) &&
  forallb (fun z => negb (vmem z (xvars body))) [st; gd; w; tag; packet; io] &&
  loop_control_bound false body.

Lemma vmem_true : forall x l, vmem x l = true -> In x l.
Proof. intros x l H. unfold vmem in H. destruct (in_dec Nat.eq_dec x l); [exact i|discriminate]. Qed.

Lemma forallb_absent : forall (l zs : list Var),
  forallb (fun z => negb (vmem z l)) zs = true -> forall z, In z zs -> ~ In z l.
Proof.
  intros l zs H z Hz Hl. apply forallb_forall with (x := z) in H; [|exact Hz].
  unfold vmem in H. destruct (in_dec Nat.eq_dec z l); [discriminate|contradiction].
Qed.

Lemma slookup_all_supd_list_self : forall sg xs cs, NoDup xs -> length xs = length cs ->
  slookup_all (supd_list sg xs cs) xs = Some cs.
Proof.
  intros sg xs. induction xs as [|x xs IH]; intros cs HN Hl; destruct cs as [|c cs];
    simpl in *; try discriminate; [reflexivity|].
  inversion HN as [|? ? Hx HN']; subst. injection Hl as Hl.
  rewrite supd_same. rewrite slookup_all_update_outside by exact Hx.
  rewrite (IH cs HN' Hl). reflexivity.
Qed.

Lemma slookup_all_app_inv : forall sg xs ys cs ds,
  slookup_all sg (xs ++ ys) = Some (cs ++ ds) -> length xs = length cs ->
  slookup_all sg xs = Some cs /\ slookup_all sg ys = Some ds.
Proof.
  intros sg xs. induction xs as [|x xs IH]; intros ys cs ds H Hl; destruct cs as [|c cs];
    simpl in *; try discriminate; [split; [reflexivity|exact H]|].
  injection Hl as Hl. destruct (sg x) as [vx|]; [|discriminate].
  destruct (slookup_all sg (xs ++ ys)) as [vs|] eqn:E; [|discriminate].
  injection H as Hc Hvs. subst vx.
  destruct (IH ys cs ds) as [H1 H2]; [rewrite E, Hvs; reflexivity|exact Hl|].
  rewrite H1. split; [reflexivity|exact H2].
Qed.

Lemma nodup_split_ne : forall (l1 l2 : list Var) a b, NoDup (l1 ++ l2) -> In a l1 -> In b l2 -> a <> b.
Proof.
  intros l1 l2 a b H Ha Hb E. subst b.
  destruct (NoDup_app_split Var l1 l2 H) as [_ [_ Hd]]. exact (Hd a Ha Hb).
Qed.

(* The facts the adapter proofs read off its admission. *)
Lemma adapter_admitted_facts : forall st gd w io ps ios locals value tag packet body,
  adapter_admitted st gd w io ps ios locals value tag packet body = true ->
  NoDup [st; gd; w] /\ xfresh [st; gd; w] body /\ loop_control_bound false body = true /\
  ~ In value [st; gd; w] /\ tag <> value /\ ~ In packet ios /\ ~ In tag ios /\
  (forall y, In y ios -> ~ In y [st; gd; w]) /\
  (forall x, In x (value :: locals) -> ~ In x ios /\ x <> io /\ ~ In x ps).
Proof.
  intros st gd w io ps ios locals value tag packet body Hadm.
  unfold adapter_admitted in Hadm.
  apply andb_true_iff in Hadm. destruct Hadm as [Hadm Hloop].
  apply andb_true_iff in Hadm. destruct Hadm as [Hnd Hfr].
  apply nodupb_spec in Hnd.
  pose proof (forallb_absent _ _ Hfr) as Hfresh.
  destruct (NoDup_app_split Var ([st; gd; w; tag; packet; io; value] ++ locals) (ios ++ ps) Hnd)
    as [Hpre [_ Hdisj]].
  destruct (NoDup_app_split Var [st; gd; w; tag; packet; io; value] locals Hpre) as [HN7 [_ Hdl]].
  assert (Pre : forall a, In a [st; gd; w; tag; packet; io; value] -> In a ([st; gd; w; tag; packet; io; value] ++ locals))
    by (intros a Ha; apply in_or_app; left; exact Ha).
  assert (Dios : forall a, In a [st; gd; w; tag; packet; io; value] -> ~ In a ios)
    by (intros a Ha Hi; apply (Hdisj a (Pre a Ha)); apply in_or_app; left; exact Hi).
  split; [exact (proj1 (NoDup_app_split Var [st; gd; w] [tag; packet; io; value] HN7))|].
  split; [intros z Hz; apply Hfresh; simpl in Hz |- *; tauto|].
  split; [exact Hloop|].
  split.
  { intro Hin. destruct (NoDup_app_split Var [st; gd; w] [tag; packet; io; value] HN7) as [_ [_ Hd]].
    apply (Hd value Hin). simpl; auto 10. }
  split; [apply (nodup_split_ne [st; gd; w; tag] [packet; io; value] tag value HN7); simpl; auto 10|].
  split; [apply Dios; simpl; auto 10|].
  split; [apply Dios; simpl; auto 10|].
  split; [intros y Hy Hin; apply (Dios y); [simpl in Hin |- *; tauto|exact Hy]|].
  intros x Hx. simpl in Hx. destruct Hx as [Ex|Hx].
  - subst x. split; [apply Dios; simpl; auto 10|]. split.
    + intro E. apply (nodup_split_ne [st; gd; w; tag; packet; io] [value] io value HN7);
        [simpl; auto 10|simpl; auto 10|symmetry; exact E].
    + intro Hp. apply (Hdisj value (Pre value ltac:(simpl; auto 10))). apply in_or_app; right; exact Hp.
  - split; [intro Hi; apply (Hdisj x); [apply in_or_app; right; exact Hx|apply in_or_app; left; exact Hi]|].
    split.
    + intro E. subst x. apply (Hdl io); [simpl; auto 10|exact Hx].
    + intro Hp. apply (Hdisj x); [apply in_or_app; right; exact Hx|apply in_or_app; right; exact Hp].
Qed.

Theorem adapter_exec : forall funs procs st gd w io ps ios locals value tag packet body
    sg0 values sgx tr o outs v,
  adapter_admitted st gd w io ps ios locals value tag packet body = true ->
  sg0 io = Some (SNode values) -> length ios = length values ->
  (forall x, In x (value :: locals) -> sg0 x = None) ->
  xsexec funs procs (supd_list sg0 ios values) body sgx tr o ->
  slookup_all sgx ios = Some outs -> sgx value = Some v ->
  continuing_outcome o = true /\
  exists sgc, sexec funs procs sg0 (adapter_body st gd w io ios locals value tag packet body) sgc tr /\
    sgc io = Some (recovery_value o outs v).
Proof.
  intros funs procs st gd w io ps ios locals value tag packet body sg0 values sgx tr o outs v
    Hadm Hio Hlen Hfree Hx Hios Hv.
  destruct (adapter_admitted_facts _ _ _ _ _ _ _ _ _ _ _ Hadm)
    as [HN3 [Hxf [Hloop [Hvf [Htv [Npk [Ntg [Hiosf Hloc]]]]]]]].
  split; [exact (admitted_body_has_continuing_exit _ _ _ _ _ _ _ Hloop Hx)|].
  set (E1 := supd_list sg0 ios values).
  set (Ei := supd_zero E1 (value :: locals)).
  assert (X1 : extends E1 Ei).
  { apply supd_zero_extends. intros x Hx'. unfold E1.
    rewrite supd_list_out by exact (proj1 (Hloc x Hx')). exact (Hfree x Hx'). }
  destruct (xsexec_extend _ _ _ _ _ _ _ Hx Ei X1) as [sgx' [Hx' Xs]].
  set (E2 := supd Ei st (SLeaf 0)).
  assert (A2 : agree [st; gd; w] Ei E2) by (apply agree_set; [apply in3_1|apply agree_refl]).
  destruct (lower_sim funs procs (ios ++ value :: locals) st gd w body HN3 Hxf Ei sgx' tr o Hx' E2 A2
    (supd_same _ _ _)) as [sgl' [Elow [Alow Klow]]].
  set (E3 := supd sgl' tag (outcome_tag [SLeaf (status_code o)])).
  set (pk := SNode [outcome_tag [SLeaf (status_code o)]; v]).
  set (E4 := supd E3 packet pk).
  assert (V3 : E3 value = Some v).
  { unfold E3. rewrite supd_other by exact Htv. rewrite (Alow value Hvf). exact (Xs _ _ Hv). }
  assert (L4 : slookup_all E4 ios = Some outs).
  { unfold E4, E3. rewrite (slookup_all_update_outside _ _ _ _ Npk).
    rewrite (slookup_all_update_outside _ _ _ _ Ntg).
    rewrite (agree_lookup_all _ _ _ ios Alow Hiosf). exact (extends_lookup_all _ _ _ _ Xs Hios). }
  exists (supd E4 io (SNode (outs ++ [pk]))). split.
  - unfold adapter_body.
    eapply SE_Seq with (tr1 := []) (tr2 := tr); [apply SE_Unpack; [exact Hio|exact Hlen]| |reflexivity].
    eapply SE_Seq with (tr1 := []) (tr2 := tr); [apply init_locals_exec| |reflexivity].
    eapply SE_Seq with (tr1 := []) (tr2 := tr);
      [apply (SE_Def funs procs Ei st (set_status 0) [] []); reflexivity| |reflexivity].
    eapply SE_Seq with (tr1 := tr) (tr2 := []); [exact Elow| |rewrite app_nil_r; reflexivity].
    eapply SE_Seq with (tr1 := []) (tr2 := []);
      [apply SE_Def; apply slookup_all_one; exact Klow| |reflexivity].
    eapply SE_Seq with (tr1 := []) (tr2 := []);
      [apply SE_Pack; apply slookup_all_two; [apply supd_same|exact V3]| |reflexivity].
    apply SE_Pack. apply slookup_all_append; [exact L4|apply slookup_all_one; apply supd_same].
  - rewrite supd_same. unfold pk. rewrite outcome_tag_code. reflexivity.
Qed.

(* ------------------------------------------------------------------ *)
(* Caller and callee together                                           *)
(* ------------------------------------------------------------------ *)

Lemma normalize_bundle_not_argument : forall scope g bundle ios args packet normalized,
  normalize_multi_call scope g bundle ios args packet = Some normalized -> ~ In bundle args.
Proof.
  intros scope g bundle ios args packet normalized E Hin.
  unfold normalize_multi_call, multi_call_admitted in E.
  destruct (nodupb (bundle :: packet :: ios) && forallb (fun x => negb (vmem x ios)) args &&
    negb (vmem bundle args || vmem packet args)) eqn:Ea.
  - apply andb_true_iff in Ea. destruct Ea as [_ Ea]. apply negb_true_iff in Ea.
    apply orb_false_iff in Ea. destruct Ea as [Ea _]. unfold vmem in Ea.
    destruct (in_dec Nat.eq_dec bundle args); [discriminate|contradiction].
  - simpl in E. discriminate.
Qed.

Lemma nodup_snoc : forall (l : list Var) a, NoDup l -> ~ In a l -> NoDup (l ++ [a]).
Proof.
  induction l as [|x l IH]; intros a H Ha; simpl; [constructor; [intros []|constructor]|].
  inversion H as [|? ? Hx Hl]; subst. constructor.
  - intro Hin. apply in_app_or in Hin. destruct Hin as [Hin|[E|[]]]; [contradiction|].
    subst. apply Ha. left; reflexivity.
  - apply IH; [exact Hl|]. intro Hin. apply Ha. right; exact Hin.
Qed.

Lemma normalize_outputs_distinct : forall scope g bundle ios args packet normalized,
  normalize_multi_call scope g bundle ios args packet = Some normalized -> NoDup (ios ++ [packet]).
Proof.
  intros scope g bundle ios args packet normalized E.
  unfold normalize_multi_call, multi_call_admitted in E.
  destruct (nodupb (bundle :: packet :: ios)) eqn:Ea; [|simpl in E; discriminate].
  apply nodupb_spec in Ea. inversion Ea as [|? ? _ Ea']; subst.
  inversion Ea' as [|? ? Hp Hios]; subst.
  apply nodup_snoc; [exact Hios|exact Hp].
Qed.

Lemma sbind_none : forall ps vs x, ~ In x ps -> sbind ps vs x = None.
Proof.
  intros ps vs x H. destruct (sbind ps vs x) eqn:E; [|reflexivity].
  exfalso. apply H. exact (sbind_in _ _ _ _ E).
Qed.

Theorem lowered_multi_call_recovers : forall funs procs scope g bundle ios args packet normalized
    sg values vs io ps st gd w fios locals value tag fpacket body sgx tr o outs v,
  normalize_multi_call scope g bundle ios args packet = Some normalized ->
  procs g = Some (io, ps, adapter_body st gd w io fios locals value tag fpacket body) ->
  adapter_admitted st gd w io ps fios locals value tag fpacket body = true ->
  slookup_all sg ios = Some values -> slookup_all sg args = Some vs -> length ps = length vs ->
  length fios = length values ->
  xsexec funs procs (supd_list (supd (sbind ps vs) io (SNode values)) fios values) body sgx tr o ->
  slookup_all sgx fios = Some outs -> sgx value = Some v ->
  continuing_outcome o = true /\
  exists sgf, sexec funs procs sg normalized sgf tr /\
    slookup_all sgf ios = Some outs /\ sgf packet = Some (SNode [SLeaf (recovery_tag o); v]).
Proof.
  intros funs procs scope g bundle ios args packet normalized sg values vs io ps st gd w fios
    locals value tag fpacket body sgx tr o outs v Hn Hg Hadm Hios Hargs Hps Hflen Hx Houts Hv.
  destruct (adapter_admitted_facts _ _ _ _ _ _ _ _ _ _ _ Hadm) as [_ [_ [_ [_ [_ [_ [_ [_ Hloc]]]]]]]].
  assert (Hfree : forall x, In x (value :: locals) -> supd (sbind ps vs) io (SNode values) x = None).
  { intros x Hx'. destruct (Hloc x Hx') as [_ [Hio Hps']].
    rewrite supd_other by (intro E; apply Hio; symmetry; exact E). exact (sbind_none _ _ _ Hps'). }
  destruct (adapter_exec funs procs st gd w io ps fios locals value tag fpacket body
    (supd (sbind ps vs) io (SNode values)) values sgx tr o outs v Hadm (supd_same _ _ _) Hflen Hfree
    Hx Houts Hv) as [Ho [sgc [Ec Vc]]].
  split; [exact Ho|].
  assert (Hlen : length ios = length outs).
  { rewrite (lookup_all_length _ _ _ Hios), <- Hflen. exact (lookup_all_length _ _ _ Houts). }
  pose proof (multi_call_pack_call_unpack funs procs sg scope g bundle ios args packet values vs io ps
    (adapter_body st gd w io fios locals value tag fpacket body) sgc tr outs
    (SNode [SLeaf (recovery_tag o); v]) normalized Hn (normalize_bundle_not_argument _ _ _ _ _ _ _ Hn)
    Hg Hios Hargs Hps Hlen Ec Vc) as Ecall.
  eexists. split; [exact Ecall|].
  assert (Hlen2 : length (ios ++ [packet]) = length (outs ++ [SNode [SLeaf (recovery_tag o); v]]))
    by (rewrite !length_app; simpl; lia).
  pose proof (slookup_all_supd_list_self
    (supd (supd sg bundle (SNode values)) bundle (SNode (outs ++ [SNode [SLeaf (recovery_tag o); v]])))
    (ios ++ [packet]) (outs ++ [SNode [SLeaf (recovery_tag o); v]])
    (normalize_outputs_distinct _ _ _ _ _ _ _ Hn) Hlen2) as Hall.
  destruct (slookup_all_app_inv _ _ _ _ _ Hall Hlen) as [H1 H2].
  split; [exact H1|]. exact (slookup_all_one_inv _ _ _ H2).
Qed.

(* The same call in the ownership machine: the core's elab_sound applies
   to the caller statement, with the lowered adapter in the procedure
   table. The restored inouts are owned by the caller afterwards. *)
Theorem lowered_multi_call_sound : forall M funs procs,
  (forall g d, funs g = Some d -> elab_fun M g d <> None) ->
  (forall g d, procs g = Some d -> elab_proc M g d <> None) ->
  forall scope g bundle ios args packet normalized sg values vs io ps st gd w fios locals value tag
    fpacket body sgx tr o outs v L B t Lin rho beta H n R,
  normalize_multi_call scope g bundle ios args packet = Some normalized ->
  procs g = Some (io, ps, adapter_body st gd w io fios locals value tag fpacket body) ->
  adapter_admitted st gd w io ps fios locals value tag fpacket body = true ->
  slookup_all sg ios = Some values -> slookup_all sg args = Some vs -> length ps = length vs ->
  length fios = length values ->
  xsexec funs procs (supd_list (supd (sbind ps vs) io (SNode values)) fios values) body sgx tr o ->
  slookup_all sgx fios = Some outs -> sgx value = Some v ->
  elab M normalized L B = Some (t, Lin) -> INV rho beta H n R -> CORR sg rho beta Lin B ->
  exists sgf rho' beta' H' n',
    texec (tfuns_of M funs) (tprocs_of M procs) rho beta H n t rho' beta' H' n' tr /\
    INV rho' beta' H' n' R /\ CORR sgf rho' beta' L B /\
    slookup_all sgf ios = Some outs /\ sgf packet = Some (SNode [SLeaf (recovery_tag o); v]).
Proof.
  intros M funs procs Hf Hp scope g bundle ios args packet normalized sg values vs io ps st gd w fios
    locals value tag fpacket body sgx tr o outs v L B t Lin rho beta H n R
    Hn Hg Hadm Hios Hargs Hps Hflen Hx Houts Hv Hel HI HC.
  destruct (lowered_multi_call_recovers funs procs scope g bundle ios args packet normalized sg values vs
    io ps st gd w fios locals value tag fpacket body sgx tr o outs v Hn Hg Hadm Hios Hargs Hps Hflen Hx
    Houts Hv) as [_ [sgf [Es [Ho Hpk]]]].
  destruct (elab_sound M funs procs Hf Hp sg normalized sgf tr Es L B t Lin rho beta H n R Hel HI HC)
    as [rho' [beta' [H' [n' [Et [I' C']]]]]].
  exists sgf, rho', beta', H', n'.
  split; [exact Et|split; [exact I'|split; [exact C'|split; [exact Ho|exact Hpk]]]].
Qed.

(* ------------------------------------------------------------------ *)
(* Witnesses: a callee with a local loop and a return/error switch       *)
(* ------------------------------------------------------------------ *)

(* Callee: formals 11, 12 (inout), 13 (readonly selector), value 14,
   loop condition 15; issued names st=20 gd=21 w=22 tag=23 packet=24,
   bundle formal 10. It sets 11, runs a loop that emits 11 and breaks,
   then returns 3 when 13 is true and raises with payload 4 otherwise. *)
Definition witness_body : XStmt :=
  XSeq (XS (SDef 11 (fun _ => SLeaf 9) []))
    (XSeq (XS (SDef 15 (fun _ => SLeaf 1) []))
      (XSeq (XLoop 15 [13] (XSeq (XS (SEmit 11)) XBreak))
        (XIf 13 (XSeq (XS (SDef 14 (fun _ => SLeaf 3) [])) XReturn)
                (XSeq (XS (SDef 14 (fun _ => SLeaf 4) [])) XThrow)))).

Definition witness_adapter : SStmt := adapter_body 20 21 22 10 [11; 12] [15] 14 23 24 witness_body.

Definition witness_procs : SProcTable :=
  fun g => if Nat.eqb g 0 then Some (10, [13], witness_adapter) else None.

Definition witness_modes : Modes :=
  {| fmodes := fun _ => None; pmodes := fun g => if Nat.eqb g 0 then Some [false] else None |}.

Example witness_adapter_admitted :
  adapter_admitted 20 21 22 10 [13] [11; 12] [15] 14 23 24 witness_body = true.
Proof. reflexivity. Qed.

(* Refusals: an issued name used by the body, a repeated formal, the
   value aliasing an inout, a local aliasing a readonly parameter, and an
   escaping break. *)
Example witness_adapter_refusals :
  adapter_admitted 15 21 22 10 [13] [11; 12] [15] 14 23 24 witness_body = false /\
  adapter_admitted 20 21 22 10 [13] [11; 11] [15] 14 23 24 witness_body = false /\
  adapter_admitted 20 21 22 10 [13] [11; 12] [15] 11 23 24 witness_body = false /\
  adapter_admitted 20 21 22 10 [13] [11; 12] [13] 14 23 24 witness_body = false /\
  adapter_admitted 20 21 22 10 [13] [11; 12] [15] 14 23 24 (XSeq witness_body XBreak) = false.
Proof. repeat split; reflexivity. Qed.

Lemma witness_procs_ok : forall g d, witness_procs g = Some d -> elab_proc witness_modes g d <> None.
Proof.
  intros g d E. unfold witness_procs in E.
  destruct (Nat.eqb g 0) eqn:Eg; [|discriminate]. inversion E; subst.
  apply Nat.eqb_eq in Eg; subst. vm_compute. discriminate.
Qed.

Definition witness_entry (choose : bool) : SEnv :=
  supd_list (supd (sbind [13] [SLeaf (if choose then 1 else 0)]) 10 (SNode [SLeaf 1; SLeaf 2]))
    [11; 12] [SLeaf 1; SLeaf 2].

Ltac witness_prefix :=
  eapply XE_SeqN with (tr1 := []) (tr2 := [SLeaf 9]);
    [apply XE_S; apply (SE_Def no_funs witness_procs _ 11 _ [] []); reflexivity| |reflexivity];
  eapply XE_SeqN with (tr1 := []) (tr2 := [SLeaf 9]);
    [apply XE_S; apply (SE_Def no_funs witness_procs _ 15 _ [] []); reflexivity| |reflexivity];
  eapply XE_SeqN with (tr1 := [SLeaf 9]) (tr2 := []);
    [ eapply XE_LoopB; [reflexivity|reflexivity|];
      eapply XE_SeqN with (tr1 := [SLeaf 9]) (tr2 := []);
        [apply XE_S; apply SE_Emit; reflexivity|apply XE_Break|reflexivity]
    | |reflexivity].

Lemma witness_body_runs : forall choose : bool, exists sgx,
  xsexec no_funs witness_procs (witness_entry choose) witness_body sgx [SLeaf 9]
    (if choose then ORet else OErr) /\
  slookup_all sgx [11; 12] = Some [SLeaf 9; SLeaf 2] /\
  sgx 14 = Some (SLeaf (if choose then 3 else 4)).
Proof.
  intros choose. unfold witness_body. destruct choose; eexists; split.
  - witness_prefix.
    eapply XE_IfT; [reflexivity|reflexivity|].
    eapply XE_SeqN with (tr1 := []) (tr2 := []);
      [apply XE_S; apply (SE_Def no_funs witness_procs _ 14 _ [] []); reflexivity|apply XE_Return|reflexivity].
  - split; reflexivity.
  - witness_prefix.
    eapply XE_IfF; [reflexivity|reflexivity|].
    eapply XE_SeqN with (tr1 := []) (tr2 := []);
      [apply XE_S; apply (SE_Def no_funs witness_procs _ 14 _ [] []); reflexivity|apply XE_Throw|reflexivity].
  - split; reflexivity.
Qed.

(* Caller: actuals 0, 1 (inout), 3 (selector); bundle 7, packet 2. *)
Definition witness_caller (choose : bool) : SStmt :=
  SSeq (SDef 0 (fun _ => SLeaf 1) [])
    (SSeq (SDef 1 (fun _ => SLeaf 2) [])
      (SSeq (SDef 3 (fun _ => SLeaf (if choose then 1 else 0)) [])
        (SSeq (multi_call_body 0 7 [0; 1] [3] 2)
          (SSeq (SEmit 0) (SSeq (SEmit 1) (SEmit 2)))))).

Lemma witness_caller_runs : forall choose : bool, exists sgf,
  sexec no_funs witness_procs sempty (witness_caller choose) sgf
    [SLeaf 9; SLeaf 9; SLeaf 2; SNode [SLeaf (if choose then 1 else 0); SLeaf (if choose then 3 else 4)]].
Proof.
  intros choose.
  destruct (witness_body_runs choose) as [sgx [Hx [Ho Hv]]].
  destruct (lowered_multi_call_recovers no_funs witness_procs [0; 1; 3] 0 7 [0; 1] [3] 2
    (multi_call_body 0 7 [0; 1] [3] 2)
    (supd (supd (supd sempty 0 (SLeaf 1)) 1 (SLeaf 2)) 3 (SLeaf (if choose then 1 else 0)))
    [SLeaf 1; SLeaf 2] [SLeaf (if choose then 1 else 0)]
    10 [13] 20 21 22 [11; 12] [15] 14 23 24 witness_body sgx [SLeaf 9] (if choose then ORet else OErr)
    [SLeaf 9; SLeaf 2] (SLeaf (if choose then 3 else 4))
    eq_refl eq_refl eq_refl eq_refl eq_refl eq_refl eq_refl Hx Ho Hv)
    as [_ [sgf [Ecall [Hout Hpk]]]].
  destruct (slookup_all_two_inv _ _ _ _ _ Hout) as [P0 P1].
  exists sgf. unfold witness_caller.
  eapply SE_Seq with (tr1 := []); [apply (SE_Def no_funs witness_procs sempty 0 _ [] []); reflexivity| |reflexivity].
  eapply SE_Seq with (tr1 := []); [apply (SE_Def no_funs witness_procs _ 1 _ [] []); reflexivity| |reflexivity].
  eapply SE_Seq with (tr1 := []); [apply (SE_Def no_funs witness_procs _ 3 _ [] []); reflexivity| |reflexivity].
  eapply SE_Seq with (tr1 := [SLeaf 9]); [exact Ecall| |reflexivity].
  eapply SE_Seq with (tr1 := [SLeaf 9]); [apply SE_Emit; exact P0| |reflexivity].
  eapply SE_Seq with (tr1 := [SLeaf 2]); [apply SE_Emit; exact P1| |reflexivity].
  apply SE_Emit. rewrite Hpk. destruct choose; reflexivity.
Qed.

(* Both the early-return run and the handled-error run of the whole
   caller elaborate without a copy and free every block. *)
Theorem witness_return_and_error_free_everything : forall choose : bool, exists t n,
  elab witness_modes (witness_caller choose) [] [] = Some (t, []) /\ count_copies t = 0 /\
  texec (tfuns_of witness_modes no_funs) (tprocs_of witness_modes witness_procs)
    [] [] [] 0 t [] [] [] n
    [SLeaf 9; SLeaf 9; SLeaf 2; SNode [SLeaf (if choose then 1 else 0); SLeaf (if choose then 3 else 4)]].
Proof.
  intros choose.
  assert (Hel : exists t, elab witness_modes (witness_caller choose) [] [] = Some (t, []) /\
    count_copies t = 0) by (destruct choose; eexists; split; reflexivity).
  destruct Hel as [t [Et Ec]].
  destruct (witness_caller_runs choose) as [sgf Es].
  destruct (closed_program_frees_everything witness_modes no_funs witness_procs
    (no_funs_ok witness_modes) witness_procs_ok _ _ _ _ _ Et Es) as [n En].
  exists t, n. split; [exact Et|split; assumption].
Qed.
