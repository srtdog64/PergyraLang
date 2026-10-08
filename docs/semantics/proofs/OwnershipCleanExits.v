(* Exits for the ownership-clean core: break, continue, return and error
   edges. A layer of structured control with exits sits on top of the
   core's statements and reuses its machine, invariants and elaboration;
   it adds no heap, no ownership rule and no second cleanup authority.

   Every exit has a target live set: the loop's continuation for break, the
   loop head for continue, the routine's result for return, the handler's
   live-in for an error. Backward liveness uses that set as the live-after
   set of the exit, so the ordinary settles release, on the way to the exit,
   exactly the variables the target does not need. The theorem
   [xelab_sound] states that for every outcome the target has the same
   outcome, the same trace, and exactly the outcome's target set bound; no
   runtime flag is needed.

   A partially built aggregate is a set of separate owned variables until
   the pack that builds it, so an error between the parts releases each
   built part ([error_releases_partial_parts]). An error that leaves a
   routine, and unwinding across calls, are outside this layer. *)
Require Import Stdlib.Lists.List Stdlib.Arith.PeanoNat Stdlib.Bool.Bool.
Require Import Stdlib.Sorting.Permutation Stdlib.micromega.Lia.
Require Import OwnershipCleanCore.
Import ListNotations.

Inductive XStmt : Type :=
| XS (s : SStmt)
| XSeq (a b : XStmt)
| XIf (c : Var) (a b : XStmt)
| XLoop (c : Var) (head : list Var) (body : XStmt)
| XBreak
| XContinue
| XReturn
| XThrow
| XTry (body handler : XStmt).

Inductive Outcome : Type := ONorm | OBrk | OCont | ORet | OErr.

Section XSource.
Variable funs : SFunTable.
Variable procs : SProcTable.

Inductive xsexec : SEnv -> XStmt -> SEnv -> list SVal -> Outcome -> Prop :=
| XE_S : forall sg s sg' tr, sexec funs procs sg s sg' tr -> xsexec sg (XS s) sg' tr ONorm
| XE_SeqN : forall sg1 sg2 sg3 a b tr1 tr2 tr o,
    xsexec sg1 a sg2 tr1 ONorm -> xsexec sg2 b sg3 tr2 o -> tr = tr1 ++ tr2 ->
    xsexec sg1 (XSeq a b) sg3 tr o
| XE_SeqX : forall sg sg' a b tr o,
    xsexec sg a sg' tr o -> o <> ONorm -> xsexec sg (XSeq a b) sg' tr o
| XE_IfT : forall sg sg' c a b v tr o,
    sg c = Some v -> truthy v = true -> xsexec sg a sg' tr o -> xsexec sg (XIf c a b) sg' tr o
| XE_IfF : forall sg sg' c a b v tr o,
    sg c = Some v -> truthy v = false -> xsexec sg b sg' tr o -> xsexec sg (XIf c a b) sg' tr o
| XE_LoopF : forall sg c h b v,
    sg c = Some v -> truthy v = false -> xsexec sg (XLoop c h b) sg [] ONorm
| XE_LoopT : forall sg sg1 sg2 c h b v tr1 tr2 tr o1 o,
    sg c = Some v -> truthy v = true -> xsexec sg b sg1 tr1 o1 -> (o1 = ONorm \/ o1 = OCont) ->
    xsexec sg1 (XLoop c h b) sg2 tr2 o -> tr = tr1 ++ tr2 -> xsexec sg (XLoop c h b) sg2 tr o
| XE_LoopB : forall sg sg1 c h b v tr,
    sg c = Some v -> truthy v = true -> xsexec sg b sg1 tr OBrk -> xsexec sg (XLoop c h b) sg1 tr ONorm
| XE_LoopX : forall sg sg1 c h b v tr o,
    sg c = Some v -> truthy v = true -> xsexec sg b sg1 tr o -> (o = ORet \/ o = OErr) ->
    xsexec sg (XLoop c h b) sg1 tr o
| XE_Break : forall sg, xsexec sg XBreak sg [] OBrk
| XE_Continue : forall sg, xsexec sg XContinue sg [] OCont
| XE_Return : forall sg, xsexec sg XReturn sg [] ORet
| XE_Throw : forall sg, xsexec sg XThrow sg [] OErr
| XE_TryN : forall sg sg' b h tr o,
    xsexec sg b sg' tr o -> o <> OErr -> xsexec sg (XTry b h) sg' tr o
| XE_TryE : forall sg sg1 sg2 b h tr1 tr2 tr o,
    xsexec sg b sg1 tr1 OErr -> xsexec sg1 h sg2 tr2 o -> tr = tr1 ++ tr2 ->
    xsexec sg (XTry b h) sg2 tr o.
End XSource.

(* Target control: a loop repeats its body until the body breaks; the
   elaborator puts the loop test inside the body. *)
Inductive XT : Type :=
| XTS (t : TStmt)
| XTSeq (a b : XT)
| XTIf (c : TVar) (a b : XT)
| XTLoop (body : XT)
| XTBreak
| XTContinue
| XTReturn
| XTThrow
| XTTry (body handler : XT).

Section XTarget.
Variable tf : TTable.
Variable tp : TTable.

Inductive xtexec : TEnv -> TEnv -> list Block -> nat -> XT ->
                   TEnv -> TEnv -> list Block -> nat -> list SVal -> Outcome -> Prop :=
| XTE_S : forall rho beta H n t rho' beta' H' n' tr,
    texec tf tp rho beta H n t rho' beta' H' n' tr ->
    xtexec rho beta H n (XTS t) rho' beta' H' n' tr ONorm
| XTE_SeqN : forall rho1 beta1 H1 n1 rho2 beta2 H2 n2 rho3 beta3 H3 n3 a b tr1 tr2 tr o,
    xtexec rho1 beta1 H1 n1 a rho2 beta2 H2 n2 tr1 ONorm ->
    xtexec rho2 beta2 H2 n2 b rho3 beta3 H3 n3 tr2 o -> tr = tr1 ++ tr2 ->
    xtexec rho1 beta1 H1 n1 (XTSeq a b) rho3 beta3 H3 n3 tr o
| XTE_SeqX : forall rho beta H n rho' beta' H' n' a b tr o,
    xtexec rho beta H n a rho' beta' H' n' tr o -> o <> ONorm ->
    xtexec rho beta H n (XTSeq a b) rho' beta' H' n' tr o
| XTE_IfT : forall rho beta H n rho' beta' H' n' c a b cv bs tr o,
    tread c rho beta = Some (cv, bs) -> incl bs H -> truthy cv = true ->
    xtexec rho beta H n a rho' beta' H' n' tr o ->
    xtexec rho beta H n (XTIf c a b) rho' beta' H' n' tr o
| XTE_IfF : forall rho beta H n rho' beta' H' n' c a b cv bs tr o,
    tread c rho beta = Some (cv, bs) -> incl bs H -> truthy cv = false ->
    xtexec rho beta H n b rho' beta' H' n' tr o ->
    xtexec rho beta H n (XTIf c a b) rho' beta' H' n' tr o
| XTE_LoopN : forall rho beta H n rho1 beta1 H1 n1 rho2 beta2 H2 n2 b tr1 tr2 tr o1 o,
    xtexec rho beta H n b rho1 beta1 H1 n1 tr1 o1 -> (o1 = ONorm \/ o1 = OCont) ->
    xtexec rho1 beta1 H1 n1 (XTLoop b) rho2 beta2 H2 n2 tr2 o -> tr = tr1 ++ tr2 ->
    xtexec rho beta H n (XTLoop b) rho2 beta2 H2 n2 tr o
| XTE_LoopB : forall rho beta H n rho1 beta1 H1 n1 b tr,
    xtexec rho beta H n b rho1 beta1 H1 n1 tr OBrk ->
    xtexec rho beta H n (XTLoop b) rho1 beta1 H1 n1 tr ONorm
| XTE_LoopX : forall rho beta H n rho1 beta1 H1 n1 b tr o,
    xtexec rho beta H n b rho1 beta1 H1 n1 tr o -> (o = ORet \/ o = OErr) ->
    xtexec rho beta H n (XTLoop b) rho1 beta1 H1 n1 tr o
| XTE_Break : forall rho beta H n, xtexec rho beta H n XTBreak rho beta H n [] OBrk
| XTE_Continue : forall rho beta H n, xtexec rho beta H n XTContinue rho beta H n [] OCont
| XTE_Return : forall rho beta H n, xtexec rho beta H n XTReturn rho beta H n [] ORet
| XTE_Throw : forall rho beta H n, xtexec rho beta H n XTThrow rho beta H n [] OErr
| XTE_TryN : forall rho beta H n rho' beta' H' n' b h tr o,
    xtexec rho beta H n b rho' beta' H' n' tr o -> o <> OErr ->
    xtexec rho beta H n (XTTry b h) rho' beta' H' n' tr o
| XTE_TryE : forall rho beta H n rho1 beta1 H1 n1 rho2 beta2 H2 n2 b h tr1 tr2 tr o,
    xtexec rho beta H n b rho1 beta1 H1 n1 tr1 OErr ->
    xtexec rho1 beta1 H1 n1 h rho2 beta2 H2 n2 tr2 o -> tr = tr1 ++ tr2 ->
    xtexec rho beta H n (XTTry b h) rho2 beta2 H2 n2 tr o.
End XTarget.

(* The live set each exit leads to. *)
Record Exits : Type := { xbrk : list Var; xcont : list Var; xret : list Var; xerr : list Var }.

Definition xtarget (X : Exits) (L : list Var) (o : Outcome) : list Var :=
  match o with
  | ONorm => L
  | OBrk => xbrk X
  | OCont => xcont X
  | ORet => xret X
  | OErr => xerr X
  end.

Fixpoint xelab (M : Modes) (s : XStmt) (L : list Var) (X : Exits) (B : list Var)
    : option (XT * list Var) :=
  match s with
  | XS s0 => match elab M s0 L B with Some (t, Lin) => Some (XTS t, Lin) | None => None end
  | XSeq a b =>
      match xelab M b L X B with
      | None => None
      | Some (tb, Lb) =>
          match xelab M a Lb X B with
          | None => None
          | Some (ta, La) => Some (XTSeq ta tb, La)
          end
      end
  | XIf c a b =>
      match xelab M a L X B, xelab M b L X B with
      | Some (ta, La), Some (tb, Lb) =>
          let Lin := c :: La ++ Lb in
          Some (XTIf (Src c) (XTSeq (XTS (settle Lin La B)) ta) (XTSeq (XTS (settle Lin Lb B)) tb), Lin)
      | _, _ => None
      end
  | XLoop c h b =>
      match xelab M b h {| xbrk := L; xcont := h; xret := xret X; xerr := xerr X |} B with
      | None => None
      | Some (tb, Lb) =>
          if inclb Lb h && inclb L h && vmem c h
          then Some (XTLoop (XTIf (Src c) (XTSeq (XTS (settle h Lb B)) tb)
                                          (XTSeq (XTS (settle h L B)) XTBreak)), h)
          else None
      end
  | XBreak => Some (XTBreak, xbrk X)
  | XContinue => Some (XTContinue, xcont X)
  | XReturn => Some (XTReturn, xret X)
  | XThrow => Some (XTThrow, xerr X)
  | XTry b hd =>
      match xelab M hd L X B with
      | None => None
      | Some (th, Lh) =>
          match xelab M b L {| xbrk := xbrk X; xcont := xcont X; xret := xret X; xerr := Lh |} B with
          | None => None
          | Some (tb, Lb) => Some (XTTry tb th, Lb)
          end
      end
  end.

Lemma xtarget_exit : forall X L1 L2 o, o <> ONorm -> xtarget X L1 o = xtarget X L2 o.
Proof. intros X L1 L2 o Ho. destruct o; [contradiction| reflexivity..]. Qed.

Section XSoundness.
Variable M : Modes.
Variable funs : SFunTable.
Variable procs : SProcTable.
Hypothesis funs_ok : forall g d, funs g = Some d -> elab_fun M g d <> None.
Hypothesis procs_ok : forall g d, procs g = Some d -> elab_proc M g d <> None.

Let TF := tfuns_of M funs.
Let TP := tprocs_of M procs.

Lemma settle_step : forall A L B sg rho beta H n R,
  incl L A -> INV rho beta H n R -> CORR sg rho beta A B ->
  exists rho' beta' H', xtexec TF TP rho beta H n (XTS (settle A L B)) rho' beta' H' n [] ONorm /\
                        INV rho' beta' H' n R /\ CORR sg rho' beta' L B.
Proof.
  intros A L B sg rho beta H n R Hs Hi Hc.
  destruct (settle_exec TF TP A L B sg rho beta H n R Hs Hi Hc) as [rho' [beta' [H' [E [I C]]]]].
  exists rho', beta', H'. split; [apply XTE_S; exact E| split; assumption].
Qed.

Theorem xelab_sound : forall sg s sg' tr o,
  xsexec funs procs sg s sg' tr o ->
  forall L X B t Lin rho beta H n R,
    xelab M s L X B = Some (t, Lin) -> INV rho beta H n R -> CORR sg rho beta Lin B ->
    exists rho' beta' H' n',
      xtexec TF TP rho beta H n t rho' beta' H' n' tr o /\
      INV rho' beta' H' n' R /\ CORR sg' rho' beta' (xtarget X L o) B.
Proof.
  intros sg s sg' tr o Hs.
  induction Hs as
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
    intros L X B t Lin rho beta H n R Hel Hinv HC; simpl in Hel.
  - (* exit-free statement *)
    destruct (elab M s0 L B) as [[t0 L0]|] eqn:E; [|discriminate]. inversion Hel; subst t Lin.
    destruct (elab_sound M funs procs funs_ok procs_ok sg s0 sg' tr Hs0 L B t0 L0 rho beta H n R E Hinv HC)
      as [rho' [beta' [H' [n' [Ex [I C]]]]]].
    exists rho', beta', H', n'. split; [apply XTE_S; exact Ex| split; assumption].
  - (* sequence, first part completes *)
    destruct (xelab M b L X B) as [[tb Lb]|] eqn:Eb; [|discriminate].
    destruct (xelab M a Lb X B) as [[ta La]|] eqn:Ea; [|discriminate]. inversion Hel; subst t Lin.
    destruct (IHa Lb X B ta La rho beta H n R Ea Hinv HC) as [rho1 [beta1 [H1 [n1 [E1 [I1 C1]]]]]].
    destruct (IHb L X B tb Lb rho1 beta1 H1 n1 R Eb I1 C1) as [rho2 [beta2 [H2 [n2 [E2 [I2 C2]]]]]].
    exists rho2, beta2, H2, n2. split; [eapply XTE_SeqN; eassumption| split; assumption].
  - (* sequence, first part exits *)
    destruct (xelab M b L X B) as [[tb Lb]|] eqn:Eb; [|discriminate].
    destruct (xelab M a Lb X B) as [[ta La]|] eqn:Ea; [|discriminate]. inversion Hel; subst t Lin.
    destruct (IHa Lb X B ta La rho beta H n R Ea Hinv HC) as [rho1 [beta1 [H1 [n1 [E1 [I1 C1]]]]]].
    exists rho1, beta1, H1, n1. split; [eapply XTE_SeqX; eassumption| split; [exact I1|]].
    rewrite (xtarget_exit X L Lb o Ho). exact C1.
  - (* if, true *)
    destruct (xelab M a L X B) as [[ta La]|] eqn:Ea; [|discriminate].
    destruct (xelab M b L X B) as [[tb Lb]|] eqn:Eb; [|discriminate]. inversion Hel; subst t Lin.
    destruct (corr_read sg rho beta _ B c HC) as [cv [bs [Hl Hcv]]]; [left; reflexivity|].
    assert (cv = v) by congruence. subst cv.
    destruct (settle_step (c :: La ++ Lb) La B sg rho beta H n R) as [rho1 [beta1 [H1 [E1 [I1 C1]]]]];
      [intros z Hz; right; apply in_or_app; left; exact Hz| exact Hinv| exact HC|].
    destruct (IHa L X B ta La rho1 beta1 H1 n R Ea I1 C1) as [rho2 [beta2 [H2 [n2 [E2 [I2 C2]]]]]].
    exists rho2, beta2, H2, n2. split; [|split; assumption].
    eapply XTE_IfT; [exact Hl| eapply inv_read; eassumption| exact Ht|].
    eapply XTE_SeqN; [exact E1| exact E2| reflexivity].
  - (* if, false *)
    destruct (xelab M a L X B) as [[ta La]|] eqn:Ea; [|discriminate].
    destruct (xelab M b L X B) as [[tb Lb]|] eqn:Eb; [|discriminate]. inversion Hel; subst t Lin.
    destruct (corr_read sg rho beta _ B c HC) as [cv [bs [Hl Hcv]]]; [left; reflexivity|].
    assert (cv = v) by congruence. subst cv.
    destruct (settle_step (c :: La ++ Lb) Lb B sg rho beta H n R) as [rho1 [beta1 [H1 [E1 [I1 C1]]]]];
      [intros z Hz; right; apply in_or_app; right; exact Hz| exact Hinv| exact HC|].
    destruct (IHb L X B tb Lb rho1 beta1 H1 n R Eb I1 C1) as [rho2 [beta2 [H2 [n2 [E2 [I2 C2]]]]]].
    exists rho2, beta2, H2, n2. split; [|split; assumption].
    eapply XTE_IfF; [exact Hl| eapply inv_read; eassumption| exact Ht|].
    eapply XTE_SeqN; [exact E1| exact E2| reflexivity].
  - (* loop test fails: release what the continuation does not need, then leave *)
    destruct (xelab M b h {| xbrk := L; xcont := h; xret := xret X; xerr := xerr X |} B)
      as [[tb Lb]|] eqn:Eb; [|discriminate].
    destruct (inclb Lb h && inclb L h && vmem c h) eqn:Hck; [|discriminate].
    apply andb_true_iff in Hck. destruct Hck as [Hck Hch]. apply andb_true_iff in Hck. destruct Hck as [HLb HL].
    inversion Hel; subst t Lin.
    destruct (corr_read sg rho beta _ B c HC) as [cv [bs [Hl Hcv]]]; [apply vmem_true; exact Hch|].
    assert (cv = v) by congruence. subst cv.
    destruct (settle_step h L B sg rho beta H n R (inclb_spec L h HL) Hinv HC)
      as [rho1 [beta1 [H1 [E1 [I1 C1]]]]].
    exists rho1, beta1, H1, n. split; [|split; assumption].
    apply XTE_LoopB. eapply XTE_IfF; [exact Hl| eapply inv_read; eassumption| exact Ht|].
    eapply XTE_SeqN; [exact E1| apply XTE_Break| reflexivity].
  - (* loop body completes or continues: back at the head *)
    destruct (xelab M b h {| xbrk := L; xcont := h; xret := xret X; xerr := xerr X |} B)
      as [[tb Lb]|] eqn:Eb; [|discriminate].
    destruct (inclb Lb h && inclb L h && vmem c h) eqn:Hck; [|discriminate].
    pose proof Hck as Hck0.
    apply andb_true_iff in Hck. destruct Hck as [Hck Hch]. apply andb_true_iff in Hck. destruct Hck as [HLb HL].
    inversion Hel; subst t Lin.
    destruct (corr_read sg rho beta _ B c HC) as [cv [bs [Hl Hcv]]]; [apply vmem_true; exact Hch|].
    assert (cv = v) by congruence. subst cv.
    destruct (settle_step h Lb B sg rho beta H n R (inclb_spec Lb h HLb) Hinv HC)
      as [rho1 [beta1 [H1 [E1 [I1 C1]]]]].
    destruct (IHb h _ B tb Lb rho1 beta1 H1 n R Eb I1 C1) as [rho2 [beta2 [H2 [n2 [E2 [I2 C2]]]]]].
    assert (Hhead : xtarget {| xbrk := L; xcont := h; xret := xret X; xerr := xerr X |} h o1 = h)
      by (destruct Ho1; subst o1; reflexivity).
    rewrite Hhead in C2.
    assert (Eh : xelab M (XLoop c h b) L X B =
                 Some (XTLoop (XTIf (Src c) (XTSeq (XTS (settle h Lb B)) tb)
                                             (XTSeq (XTS (settle h L B)) XTBreak)), h))
      by (simpl; rewrite Eb, Hck0; reflexivity).
    destruct (IHloop L X B _ h rho2 beta2 H2 n2 R Eh I2 C2) as [rho3 [beta3 [H3 [n3 [E3 [I3 C3]]]]]].
    exists rho3, beta3, H3, n3. split; [|split; assumption].
    eapply XTE_LoopN; [| exact Ho1| exact E3| exact Htr].
    eapply XTE_IfT; [exact Hl| eapply inv_read; eassumption| exact Ht|].
    destruct Ho1 as [Ho1|Ho1]; subst o1.
    + eapply XTE_SeqN; [exact E1| exact E2| reflexivity].
    + eapply XTE_SeqN; [exact E1| exact E2| reflexivity].
  - (* break: the continuation's live set is already the only one bound *)
    destruct (xelab M b h {| xbrk := L; xcont := h; xret := xret X; xerr := xerr X |} B)
      as [[tb Lb]|] eqn:Eb; [|discriminate].
    destruct (inclb Lb h && inclb L h && vmem c h) eqn:Hck; [|discriminate].
    apply andb_true_iff in Hck. destruct Hck as [Hck Hch]. apply andb_true_iff in Hck. destruct Hck as [HLb HL].
    inversion Hel; subst t Lin.
    destruct (corr_read sg rho beta _ B c HC) as [cv [bs [Hl Hcv]]]; [apply vmem_true; exact Hch|].
    assert (cv = v) by congruence. subst cv.
    destruct (settle_step h Lb B sg rho beta H n R (inclb_spec Lb h HLb) Hinv HC)
      as [rho1 [beta1 [H1 [E1 [I1 C1]]]]].
    destruct (IHb h _ B tb Lb rho1 beta1 H1 n R Eb I1 C1) as [rho2 [beta2 [H2 [n2 [E2 [I2 C2]]]]]].
    exists rho2, beta2, H2, n2. split; [|split; [exact I2| exact C2]].
    apply XTE_LoopB. eapply XTE_IfT; [exact Hl| eapply inv_read; eassumption| exact Ht|].
    eapply XTE_SeqN; [exact E1| exact E2| reflexivity].
  - (* return or error out of the loop *)
    destruct (xelab M b h {| xbrk := L; xcont := h; xret := xret X; xerr := xerr X |} B)
      as [[tb Lb]|] eqn:Eb; [|discriminate].
    destruct (inclb Lb h && inclb L h && vmem c h) eqn:Hck; [|discriminate].
    apply andb_true_iff in Hck. destruct Hck as [Hck Hch]. apply andb_true_iff in Hck. destruct Hck as [HLb HL].
    inversion Hel; subst t Lin.
    destruct (corr_read sg rho beta _ B c HC) as [cv [bs [Hl Hcv]]]; [apply vmem_true; exact Hch|].
    assert (cv = v) by congruence. subst cv.
    destruct (settle_step h Lb B sg rho beta H n R (inclb_spec Lb h HLb) Hinv HC)
      as [rho1 [beta1 [H1 [E1 [I1 C1]]]]].
    destruct (IHb h _ B tb Lb rho1 beta1 H1 n R Eb I1 C1) as [rho2 [beta2 [H2 [n2 [E2 [I2 C2]]]]]].
    exists rho2, beta2, H2, n2. split; [|split; [exact I2|]].
    + eapply XTE_LoopX; [| exact Ho]. eapply XTE_IfT; [exact Hl| eapply inv_read; eassumption| exact Ht|].
      eapply XTE_SeqN; [exact E1| exact E2| reflexivity].
    + destruct Ho as [Ho|Ho]; subst o; exact C2.
  - inversion Hel; subst t Lin. exists rho, beta, H, n. split; [apply XTE_Break| split; assumption].
  - inversion Hel; subst t Lin. exists rho, beta, H, n. split; [apply XTE_Continue| split; assumption].
  - inversion Hel; subst t Lin. exists rho, beta, H, n. split; [apply XTE_Return| split; assumption].
  - inversion Hel; subst t Lin. exists rho, beta, H, n. split; [apply XTE_Throw| split; assumption].
  - (* try, no error escapes the body *)
    destruct (xelab M h L X B) as [[th Lh]|] eqn:Eh; [|discriminate].
    destruct (xelab M b L {| xbrk := xbrk X; xcont := xcont X; xret := xret X; xerr := Lh |} B)
      as [[tb Lb]|] eqn:Eb; [|discriminate].
    inversion Hel; subst t Lin.
    destruct (IHb L _ B tb Lb rho beta H n R Eb Hinv HC) as [rho1 [beta1 [H1 [n1 [E1 [I1 C1]]]]]].
    exists rho1, beta1, H1, n1. split; [apply XTE_TryN; [exact E1| exact Ho]| split; [exact I1|]].
    destruct o; [exact C1| exact C1| exact C1| exact C1| contradiction].
  - (* try, the body raises: the handler starts from its own live-in *)
    destruct (xelab M h L X B) as [[th Lh]|] eqn:Eh; [|discriminate].
    destruct (xelab M b L {| xbrk := xbrk X; xcont := xcont X; xret := xret X; xerr := Lh |} B)
      as [[tb Lb]|] eqn:Eb; [|discriminate].
    inversion Hel; subst t Lin.
    destruct (IHb L _ B tb Lb rho beta H n R Eb Hinv HC) as [rho1 [beta1 [H1 [n1 [E1 [I1 C1]]]]]].
    simpl in C1.
    destruct (IHh L X B th Lh rho1 beta1 H1 n1 R Eh I1 C1) as [rho2 [beta2 [H2 [n2 [E2 [I2 C2]]]]]].
    exists rho2, beta2, H2, n2. split; [eapply XTE_TryE; eassumption| split; assumption].
Qed.

(* A closed program frees everything, whichever exit it takes, when every
   exit target is empty. *)
Definition no_exits : Exits := {| xbrk := []; xcont := []; xret := []; xerr := [] |}.

Theorem x_closed_frees_everything : forall s t sg sg' tr o,
  xelab M s [] no_exits [] = Some (t, []) -> xsexec funs procs sg s sg' tr o ->
  exists n', xtexec TF TP [] [] [] 0 t [] [] [] n' tr o.
Proof.
  intros s t sg sg' tr o Hel Hs.
  assert (Hinv0 : INV [] [] [] 0 []).
  { split; [constructor|split; [constructor|split; [constructor|split; [|split; [|split]]]]].
    - intros b [].
    - intros z c bs F. discriminate.
    - intros z c bs F. discriminate.
    - intros z c bs F. discriminate. }
  assert (HC0 : CORR sg [] [] [] []).
  { split; [|split; [|split; [|split; [|split]]]].
    - intros x. simpl. split; [intros F; exfalso; apply F; reflexivity| intros [[] _]].
    - intros x. simpl. split; [intros F; exfalso; apply F; reflexivity| intros [[] _]].
    - intros x. reflexivity.
    - intros x. reflexivity.
    - intros x c bs F. discriminate.
    - intros x c bs F. discriminate. }
  destruct (xelab_sound sg s sg' tr o Hs [] no_exits [] t [] [] [] [] 0 [] Hel Hinv0 HC0)
    as [rho' [beta' [H' [n' [Hex [Hinv' HC']]]]]].
  assert (Ht : xtarget no_exits [] o = []) by (destruct o; reflexivity).
  rewrite Ht in HC'.
  destruct (corr_empty_envs sg' rho' beta' HC') as [E1 E2]. subst rho' beta'.
  destruct Hinv' as [_ [_ [HP _]]]. simpl in HP. apply Permutation_sym in HP.
  apply Permutation_nil in HP. subst H'.
  exists n'. exact Hex.
Qed.

(* A routine body ends with exactly its result bound, whether it falls off
   its end or returns early. *)
Theorem routine_exit_is_uniform : forall body ret t Lin sg sg' tr o rho beta H n R,
  xelab M body [ret] {| xbrk := []; xcont := []; xret := [ret]; xerr := [] |} [] = Some (t, Lin) ->
  xsexec funs procs sg body sg' tr o -> (o = ONorm \/ o = ORet) ->
  INV rho beta H n R -> CORR sg rho beta Lin [] ->
  exists rho' beta' H' n',
    xtexec TF TP rho beta H n t rho' beta' H' n' tr o /\
    INV rho' beta' H' n' R /\ CORR sg' rho' beta' [ret] [].
Proof.
  intros body ret t Lin sg sg' tr o rho beta H n R Hel Hs Ho Hinv HC.
  destruct (xelab_sound sg body sg' tr o Hs [ret] _ [] t Lin rho beta H n R Hel Hinv HC)
    as [rho' [beta' [H' [n' [E [I C]]]]]].
  exists rho', beta', H', n'. split; [exact E| split; [exact I|]].
  destruct Ho; subst o; exact C.
Qed.
End XSoundness.

(* ------------------------------------------------------------------ *)
(* Witnesses                                                            *)
(* ------------------------------------------------------------------ *)

Fixpoint xdrops (t : XT) : list TVar :=
  let fix tdrops (u : TStmt) : list TVar :=
    match u with
    | TDrop z => [z]
    | TSeq a b | TIf _ a b => tdrops a ++ tdrops b
    | TWhile _ b | TFocus _ _ _ b => tdrops b
    | _ => []
    end in
  match t with
  | XTS u => tdrops u
  | XTSeq a b | XTIf _ a b | XTTry a b => xdrops a ++ xdrops b
  | XTLoop b => xdrops b
  | _ => []
  end.

(* An error between building two parts and packing them: the handler
   starts with nothing live, so both built parts are released on the way to
   the throw, and the program frees everything. *)
Definition partial_build : XStmt :=
  XTry (XSeq (XS (SDef 0 (fun _ => SLeaf 1) []))
       (XSeq (XS (SDef 1 (fun _ => SLeaf 2) []))
       (XSeq (XS (SDef 2 (fun _ => SLeaf 0) []))
       (XSeq (XIf 2 (XS SSkip) XThrow)
             (XS (SSeq (SPack 3 [0; 1]) (SEmit 3)))))))
       (XS SSkip).

Example partial_build_elaborates :
  exists t, xelab no_summaries partial_build [] no_exits [] = Some (t, []) /\
    In (Src 0) (xdrops t) /\ In (Src 1) (xdrops t).
Proof. eexists. split; [reflexivity|]. split; simpl; tauto. Qed.

Lemma partial_build_source :
  exists sg', xsexec no_funs no_procs sempty partial_build sg' [] ONorm.
Proof.
  eexists. unfold partial_build.
  eapply XE_TryE with (tr1 := []) (tr2 := []); [| apply XE_S; constructor| reflexivity].
  eapply XE_SeqN with (tr1 := []) (tr2 := []);
    [apply XE_S; apply SE_Def with (vs := []); reflexivity| |reflexivity].
  eapply XE_SeqN with (tr1 := []) (tr2 := []);
    [apply XE_S; apply SE_Def with (vs := []); reflexivity| |reflexivity].
  eapply XE_SeqN with (tr1 := []) (tr2 := []);
    [apply XE_S; apply SE_Def with (vs := []); reflexivity| |reflexivity].
  eapply XE_SeqX; [| discriminate].
  eapply XE_IfF; [reflexivity| reflexivity| apply XE_Throw].
Qed.

Theorem error_releases_partial_parts : exists t n',
  xelab no_summaries partial_build [] no_exits [] = Some (t, []) /\
  xtexec (tfuns_of no_summaries no_funs) (tprocs_of no_summaries no_procs) [] [] [] 0 t [] [] [] n' [] ONorm.
Proof.
  destruct partial_build_elaborates as [t [Ht _]].
  destruct partial_build_source as [sg' Hs].
  destruct (x_closed_frees_everything no_summaries no_funs no_procs
              (no_funs_ok no_summaries) (no_procs_ok no_summaries) partial_build t _ _ _ _ Ht Hs)
    as [n' Hex].
  exists t, n'. split; assumption.
Qed.

(* An early return releases everything except the result: both branches of
   this routine body end with only the result variable 9 bound. *)
Definition early_return_body : XStmt :=
  XSeq (XS (SDef 0 (fun _ => SLeaf 1) []))
  (XSeq (XS (SDef 1 (fun _ => SLeaf 2) []))
  (XSeq (XS (SDef 2 (fun _ => SLeaf 1) []))
  (XSeq (XIf 2 (XSeq (XS (SCopy 9 0)) XReturn) (XS SSkip))
        (XS (SCopy 9 1))))).

Example early_return_elaborates :
  exists t, xelab no_summaries early_return_body [9]
              {| xbrk := []; xcont := []; xret := [9]; xerr := [] |} [] = Some (t, []) /\
    In (Src 1) (xdrops t) /\ In (Src 0) (xdrops t).
Proof. eexists. split; [reflexivity|]. split; simpl; tauto. Qed.

(* A break leaves the loop with exactly the loop's continuation live: the
   loop-local value is released on the way to the break. *)
Definition break_body : XStmt :=
  XSeq (XS (SDef 0 (fun _ => SLeaf 1) []))
  (XSeq (XLoop 0 [0] (XSeq (XS (SDef 1 (fun _ => SLeaf 5) [])) (XSeq (XS (SEmit 1)) XBreak)))
        (XS (SEmit 0))).

Example break_elaborates :
  exists t, xelab no_summaries break_body [] no_exits [] = Some (t, []) /\ In (Src 1) (xdrops t).
Proof. eexists. split; [reflexivity|]. simpl; tauto. Qed.
