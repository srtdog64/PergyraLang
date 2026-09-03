(*
  CapabilityFlowCore.v  --  capability non-forgery across task creation, and
  the lend/return discipline of a borrowed capability.

  Companion to docs/204 §2.4 (share/split/lend/move are carriage facts), §3.5
  (the runtime context is _Thread_local and is not propagated across spawn)
  and §5 theorem 1 (Capability Non-Forgery).

  Capabilities here are AUTHORITY BITS -- the mask a task may exercise
  (src/runtime/pgy_runtime_capability.h: PGY_CAP_IO_READ, NETWORK, CLOCK,
  RANDOM, ...). They are not slot access modes; exclusive access to a slot is
  WitnessDataRace.v's xor_mut, not this file.

  The model. A configuration holds the manifest (the sandbox grant, fixed for
  the run), each task's current mask, each task's parent, and the caps a task
  currently holds ON LOAN from its parent. Task creation carries the mask in
  one of three ways (docs/204 §2.4):

    share   the child gets a copy of the parent's mask       (read-like)
    lend    the child gets L, the parent gives L up until the child returns
    move    the child gets L, the parent gives L up for good

  and a child RETURNS its loan when it finishes (the join). A task may narrow
  its own mask but never a borrowed cap: a loan is returned, not discarded.

  What is proved.
    [run_bounded]          Non-Forgery: every reachable mask is inside the
                           manifest. Nothing a task holds was not granted.
    [child_within_parent]  at creation the child's mask is inside the parent's.
    [loan_uniquely_held]   while a cap is on loan exactly the borrower holds
                           it and the lender does not -- the loan is a unique
                           key, revoked from the lender for its duration.
    [return_restores]      returning the loan gives the lender its cap back.
    [lend_then_return_round_trip]  the discipline is satisfiable end to end.

  Refutation.
    [tls_default_forges]   the CURRENT runtime binds the capability context
                           per thread (_Thread_local) and does not propagate it
                           across spawn, so a pool worker evaluates against the
                           default context, whose mask is PGY_CAP_ALL. Modelled
                           as a spawn whose child mask is the full default,
                           this reaches a mask outside a narrowed manifest in
                           one step: the sandboxed program's worker holds
                           `network` although the manifest granted only
                           `io_read`. This is docs/204 §3.5's gap as a theorem.

  Negative scope. No enforcement claim: the runtime rung that binds the parent
  context into the child is docs/204 §4 item 1. Sub-lending a borrowed cap is
  outside the model (a lender lends only what it owns outright).
*)

Require Import Coq.Lists.List.
Require Import Coq.Arith.PeanoNat.
Require Import Coq.Bool.Bool.
Import ListNotations.

Definition Task := nat.
Definition Cap  := nat.
Definition Mask := Cap -> bool.

Record Cfg := mkCfg {
  manifest : Mask;                   (* the run's grant; never widened *)
  hold     : Task -> Mask;           (* what each task may exercise now *)
  owner_of : Task -> option Task;    (* the parent that created the task *)
  borrowed : Task -> Mask            (* caps held on loan from the parent *)
}.

Definition subset (a b : Mask) : Prop := forall c, a c = true -> b c = true.

Definition disjoint (a b : Mask) : Prop := forall c, a c = true -> b c = false.

Definition fresh (g : Cfg) (t : Task) : Prop :=
  (forall c, hold g t c = false) /\ owner_of g t = None /\
  (forall c, borrowed g t c = false) /\ (forall r, owner_of g r <> Some t).

Definition upd {A : Type} (f : Task -> A) (t : Task) (v : A) : Task -> A :=
  fun t' => if Nat.eqb t' t then v else f t'.

Definition mask_and_not (a b : Mask) : Mask := fun c => a c && negb (b c).
Definition mask_or      (a b : Mask) : Mask := fun c => a c || b c.
Definition mask_none    : Mask := fun _ => false.

(* ===================================================================== *)
(* 1. Steps                                                               *)
(* ===================================================================== *)

Inductive step : Cfg -> Cfg -> Prop :=
  | step_share : forall g p ch,
      fresh g ch -> p <> ch ->
      step g (mkCfg (manifest g)
                    (upd (hold g) ch (hold g p))
                    (upd (owner_of g) ch (Some p))
                    (borrowed g))
  | step_lend : forall g p ch (L : Mask),
      fresh g ch -> p <> ch ->
      subset L (hold g p) -> disjoint L (borrowed g p) ->
      step g (mkCfg (manifest g)
                    (upd (upd (hold g) p (mask_and_not (hold g p) L)) ch L)
                    (upd (owner_of g) ch (Some p))
                    (upd (borrowed g) ch L))
  | step_move : forall g p ch (L : Mask),
      fresh g ch -> p <> ch ->
      subset L (hold g p) -> disjoint L (borrowed g p) ->
      step g (mkCfg (manifest g)
                    (upd (upd (hold g) p (mask_and_not (hold g p) L)) ch L)
                    (upd (owner_of g) ch (Some p))
                    (borrowed g))
  | step_return : forall g p ch,
      owner_of g ch = Some p -> p <> ch ->
      step g (mkCfg (manifest g)
                    (upd (upd (hold g) p (mask_or (hold g p) (borrowed g ch)))
                         ch (mask_and_not (hold g ch) (borrowed g ch)))
                    (owner_of g)
                    (upd (borrowed g) ch mask_none))
  | step_narrow : forall g t (D : Mask),
      disjoint D (borrowed g t) ->
      step g (mkCfg (manifest g)
                    (upd (hold g) t (mask_and_not (hold g t) D))
                    (owner_of g)
                    (borrowed g)).

Inductive steps : Cfg -> Cfg -> Prop :=
  | steps_refl  : forall g, steps g g
  | steps_trans : forall g g' g'', step g g' -> steps g' g'' -> steps g g''.

(* ===================================================================== *)
(* 2. The invariants                                                      *)
(* ===================================================================== *)

(* Non-forgery: every held cap was granted by the manifest. *)
Definition bounded (g : Cfg) : Prop :=
  forall t c, hold g t c = true -> manifest g c = true.

(* A loan is held by the borrower ... *)
Definition loan_held (g : Cfg) : Prop :=
  forall ch c, borrowed g ch c = true -> hold g ch c = true.

(* ... and NOT by the lender while it is out. *)
Definition loan_exclusive (g : Cfg) : Prop :=
  forall p ch c, owner_of g ch = Some p -> borrowed g ch c = true ->
    hold g p c = false.

(* The same cap is never on loan to two children of one parent. *)
Definition no_double_loan (g : Cfg) : Prop :=
  forall p ch r c, owner_of g ch = Some p -> owner_of g r = Some p -> ch <> r ->
    borrowed g ch c = true -> borrowed g r c = false.

Definition wf (g : Cfg) : Prop :=
  bounded g /\ loan_held g /\ loan_exclusive g /\ no_double_loan g.

Lemma upd_same : forall A (f : Task -> A) t v, upd f t v t = v.
Proof. intros. unfold upd. rewrite Nat.eqb_refl. reflexivity. Qed.

Lemma upd_other : forall A (f : Task -> A) t v t', t' <> t -> upd f t v t' = f t'.
Proof.
  intros A f t v t' Hne. unfold upd.
  destruct (Nat.eqb t' t) eqn:E.
  - apply Nat.eqb_eq in E. contradiction.
  - reflexivity.
Qed.

Ltac by_eqb t u :=
  let H := fresh "E" in
  destruct (Nat.eqb t u) eqn:H;
  [ apply Nat.eqb_eq in H; subst; try rewrite Nat.eqb_refl in *
  | apply Nat.eqb_neq in H;
    let H' := fresh "EQ" in
    assert (H' : Nat.eqb t u = false) by (apply Nat.eqb_neq; exact H);
    try rewrite H' in *; clear H' ].

(* ===================================================================== *)
(* 3. Preservation of the invariants                                      *)
(* ===================================================================== *)

Lemma step_keeps_manifest : forall g g', step g g' -> manifest g' = manifest g.
Proof. intros g g' Hs. destruct Hs; reflexivity. Qed.

Theorem step_preserves_bounded : forall g g',
  wf g -> step g g' -> bounded g'.
Proof.
  intros g g' [Hb [Hh [Hx Hd]]] Hs.
  destruct Hs; unfold bounded in *; simpl; intros q c Hc; unfold upd in Hc.
  - (* share *)
    destruct (Nat.eqb q ch); [exact (Hb p c Hc) | exact (Hb q c Hc)].
  - (* lend *)
    destruct (Nat.eqb q ch). apply (Hb p c). apply H1. exact Hc.
    destruct (Nat.eqb q p).
    + unfold mask_and_not in Hc. apply andb_true_iff in Hc. destruct Hc as [Hc _].
      exact (Hb p c Hc).
    + exact (Hb q c Hc).
  - (* move *)
    destruct (Nat.eqb q ch). apply (Hb p c). apply H1. exact Hc.
    destruct (Nat.eqb q p).
    + unfold mask_and_not in Hc. apply andb_true_iff in Hc. destruct Hc as [Hc _].
      exact (Hb p c Hc).
    + exact (Hb q c Hc).
  - (* return *)
    destruct (Nat.eqb q ch).
    + unfold mask_and_not in Hc. apply andb_true_iff in Hc. destruct Hc as [Hc _].
      exact (Hb ch c Hc).
    + destruct (Nat.eqb q p).
      * unfold mask_or in Hc. apply orb_true_iff in Hc. destruct Hc as [Hc | Hc].
        -- exact (Hb p c Hc).
        -- exact (Hb ch c (Hh ch c Hc)).
      * exact (Hb q c Hc).
  - (* narrow *)
    destruct (Nat.eqb q t).
    + unfold mask_and_not in Hc. apply andb_true_iff in Hc. destruct Hc as [Hc _].
      exact (Hb t c Hc).
    + exact (Hb q c Hc).
Qed.

Theorem step_preserves_loan_held : forall g g',
  wf g -> step g g' -> loan_held g'.
Proof.
  intros g g' [Hb [Hh [Hx Hd]]] Hs.
  destruct Hs; unfold loan_held in *; simpl; intros ch' c Hc; unfold upd in *.
  - (* share: the child holds a copy; loans untouched *)
    by_eqb ch' ch.
    + destruct H as [_ [_ [Hnb _]]]. rewrite Hnb in Hc. discriminate.
    + exact (Hh ch' c Hc).
  - (* lend: the new borrower holds L; the lender was not borrowing L *)
    by_eqb ch' ch.
    + exact Hc.
    + by_eqb ch' p.
      * unfold mask_and_not. apply andb_true_iff. split.
        -- exact (Hh p c Hc).
        -- apply negb_true_iff.
           destruct (L c) eqn:EL; [| reflexivity].
           rewrite (H2 c EL) in Hc. discriminate.
      * exact (Hh ch' c Hc).
  - (* move: no loan is created; the lender was not borrowing L *)
    by_eqb ch' ch.
    + destruct H as [_ [_ [Hnb _]]]. rewrite Hnb in Hc. discriminate.
    + by_eqb ch' p.
      * unfold mask_and_not. apply andb_true_iff. split.
        -- exact (Hh p c Hc).
        -- apply negb_true_iff.
           destruct (L c) eqn:EL; [| reflexivity].
           rewrite (H2 c EL) in Hc. discriminate.
      * exact (Hh ch' c Hc).
  - (* return: the returned loan is gone; other loans keep their holder *)
    by_eqb ch' ch.
    + unfold mask_none in Hc. discriminate.
    + by_eqb ch' p.
      * unfold mask_or. apply orb_true_iff. left. exact (Hh p c Hc).
      * exact (Hh ch' c Hc).
  - (* narrow: a borrowed cap is never dropped *)
    by_eqb ch' t.
    + unfold mask_and_not. apply andb_true_iff. split.
      * exact (Hh t c Hc).
      * apply negb_true_iff.
        destruct (D c) eqn:ED; [| reflexivity].
        rewrite (H c ED) in Hc. discriminate.
    + exact (Hh ch' c Hc).
Qed.

Theorem step_preserves_loan_exclusive : forall g g',
  wf g -> step g g' -> loan_exclusive g'.
Proof.
  intros g g' [Hb [Hh [Hx Hd]]] Hs.
  destruct Hs; unfold loan_exclusive in *; simpl; intros p' ch' c Ho Hc;
    unfold upd in *.
  - (* share *)
    destruct H as [Hh0 [Ho0 [Hnb Hno]]].
    by_eqb ch' ch.
    + rewrite Hnb in Hc. discriminate.
    + by_eqb p' ch.
      * (* a loan whose lender is the fresh task: nobody was spawned by it *)
        exfalso. exact (Hno ch' Ho).
      * exact (Hx p' ch' c Ho Hc).
  - (* lend *)
    destruct H as [Hh0 [Ho0 [Hnb Hno]]].
    by_eqb ch' ch.
    + inversion Ho; subst p'. by_eqb p ch. exfalso; apply H0; reflexivity.
      rewrite Nat.eqb_refl. simpl.
      apply andb_false_iff. right. apply negb_false_iff. exact Hc.
    + by_eqb p' ch.
      * exfalso. exact (Hno ch' Ho).
      * by_eqb p' p.
        -- unfold mask_and_not. apply andb_false_iff. left. exact (Hx p ch' c Ho Hc).
        -- exact (Hx p' ch' c Ho Hc).
  - (* move *)
    destruct H as [Hh0 [Ho0 [Hnb Hno]]].
    by_eqb ch' ch.
    + rewrite Hnb in Hc. discriminate.
    + by_eqb p' ch.
      * exfalso. exact (Hno ch' Ho).
      * by_eqb p' p.
        -- unfold mask_and_not. apply andb_false_iff. left. exact (Hx p ch' c Ho Hc).
        -- exact (Hx p' ch' c Ho Hc).
  - (* return: p regains only what ch had; no other child had it (no double loan) *)
    by_eqb ch' ch.
    + unfold mask_none in Hc. discriminate.
    + by_eqb p' ch.
      * (* ch as a lender: its mask only shrank *)
        unfold mask_and_not. apply andb_false_iff. left. exact (Hx ch ch' c Ho Hc).
      * by_eqb p' p.
        -- unfold mask_or. apply orb_false_iff. split.
           ++ exact (Hx p ch' c Ho Hc).
           ++ exact (Hd p ch' ch c Ho H E Hc).
        -- exact (Hx p' ch' c Ho Hc).
  - (* narrow: masks only shrink *)
    by_eqb p' t.
    + unfold mask_and_not. apply andb_false_iff. left. exact (Hx t ch' c Ho Hc).
    + exact (Hx p' ch' c Ho Hc).
Qed.

Theorem step_preserves_no_double_loan : forall g g',
  wf g -> step g g' -> no_double_loan g'.
Proof.
  intros g g' [Hb [Hh [Hx Hd]]] Hs.
  destruct Hs; unfold no_double_loan in *; simpl; intros p' ch' r c Ho1 Ho2 Hne Hc;
    unfold upd in *.
  - (* share: no loan created *)
    destruct H as [Hh0 [Ho0 [Hnb Hno]]].
    by_eqb ch' ch. rewrite Hnb in Hc. discriminate.
    by_eqb r ch. exact (Hnb c).
    exact (Hd p' ch' r c Ho1 Ho2 Hne Hc).
  - (* lend: the new loan L was held by p, so no sibling had it on loan *)
    destruct H as [Hh0 [Ho0 [Hnb Hno]]].
    by_eqb ch' ch.
    + inversion Ho1; subst p'.
      by_eqb r ch. exfalso; apply Hne; reflexivity.
      (* r is an older child of p holding c on loan would mean hold p c = false *)
      destruct (borrowed g r c) eqn:Er; [| reflexivity].
      assert (Hc' := H1 c Hc). rewrite (Hx p r c Ho2 Er) in Hc'. discriminate.
    + by_eqb r ch.
      * inversion Ho2; subst p'.
        destruct (L c) eqn:EL; [| reflexivity].
        assert (Hc' := H1 c EL). rewrite (Hx p ch' c Ho1 Hc) in Hc'. discriminate.
      * exact (Hd p' ch' r c Ho1 Ho2 Hne Hc).
  - (* move: no loan created *)
    destruct H as [Hh0 [Ho0 [Hnb Hno]]].
    by_eqb ch' ch. rewrite Hnb in Hc. discriminate.
    by_eqb r ch. exact (Hnb c).
    exact (Hd p' ch' r c Ho1 Ho2 Hne Hc).
  - (* return: a loan only disappears *)
    by_eqb ch' ch. unfold mask_none in Hc. discriminate.
    by_eqb r ch. reflexivity.
    exact (Hd p' ch' r c Ho1 Ho2 Hne Hc).
  - (* narrow: loans untouched *)
    exact (Hd p' ch' r c Ho1 Ho2 Hne Hc).
Qed.

Theorem step_preserves_wf : forall g g', wf g -> step g g' -> wf g'.
Proof.
  intros g g' Hwf Hs. split; [| split; [| split]].
  - exact (step_preserves_bounded g g' Hwf Hs).
  - exact (step_preserves_loan_held g g' Hwf Hs).
  - exact (step_preserves_loan_exclusive g g' Hwf Hs).
  - exact (step_preserves_no_double_loan g g' Hwf Hs).
Qed.

Theorem run_wf : forall g g', wf g -> steps g g' -> wf g'.
Proof.
  intros g g' Hwf Hrun. induction Hrun as [g | g g' g'' Hs Hrun IH].
  - exact Hwf.
  - apply IH. exact (step_preserves_wf g g' Hwf Hs).
Qed.

(* ===================================================================== *)
(* 4. The theorems                                                        *)
(* ===================================================================== *)

(* MAIN (Non-Forgery): nothing a task ever holds was not granted. *)
Theorem run_bounded : forall g g', wf g -> steps g g' -> bounded g'.
Proof. intros g g' Hwf Hrun. exact (proj1 (run_wf g g' Hwf Hrun)). Qed.

(* At creation the child's mask is inside the parent's: share copies it,
   lend and move carve L out of it. *)
Theorem child_within_parent : forall g g' p ch,
  step g g' -> owner_of g ch = None -> owner_of g' ch = Some p ->
  subset (hold g' ch) (hold g p).
Proof.
  intros g g' p ch Hs Hnone Hown. destruct Hs; simpl in *; unfold upd in *;
    intros c Hc.
  - by_eqb ch ch0.
    + inversion Hown; subst. exact Hc.
    + rewrite Hnone in Hown. discriminate.
  - by_eqb ch ch0.
    + inversion Hown; subst. exact (H1 c Hc).
    + rewrite Hnone in Hown. discriminate.
  - by_eqb ch ch0.
    + inversion Hown; subst. exact (H1 c Hc).
    + rewrite Hnone in Hown. discriminate.
  - rewrite Hnone in Hown. discriminate.
  - rewrite Hnone in Hown. discriminate.
Qed.

(* MAIN (unique key): while c is on loan from p to ch, ch holds it and p
   does not. Exactly one of the two exercises it. *)
Theorem loan_uniquely_held : forall g g' p ch c,
  wf g -> steps g g' -> owner_of g' ch = Some p -> borrowed g' ch c = true ->
  hold g' ch c = true /\ hold g' p c = false.
Proof.
  intros g g' p ch c Hwf Hrun Ho Hc.
  destruct (run_wf g g' Hwf Hrun) as [_ [Hh [Hx _]]].
  split. exact (Hh ch c Hc). exact (Hx p ch c Ho Hc).
Qed.

(* Returning the loan gives the lender its cap back. *)
Theorem return_restores : forall g p ch c,
  owner_of g ch = Some p -> p <> ch -> borrowed g ch c = true ->
  hold (mkCfg (manifest g)
              (upd (upd (hold g) p (mask_or (hold g p) (borrowed g ch)))
                   ch (mask_and_not (hold g ch) (borrowed g ch)))
              (owner_of g)
              (upd (borrowed g) ch mask_none)) p c = true.
Proof.
  intros g p ch c Ho Hne Hc. simpl. unfold upd.
  by_eqb p ch. contradiction.
  rewrite Nat.eqb_refl. unfold mask_or. rewrite Hc. apply orb_true_r.
Qed.

(* ===================================================================== *)
(* 5. Non-vacuity: lend, run, return                                      *)
(* ===================================================================== *)

Definition only (c : Cap) : Mask := fun c' => Nat.eqb c' c.

(* Manifest {0,1}; task 1 holds both; nothing else exists. *)
Definition seed : Cfg :=
  mkCfg (fun c => Nat.ltb c 2)
        (fun t => if Nat.eqb t 1 then (fun c => Nat.ltb c 2) else mask_none)
        (fun _ => None)
        (fun _ => mask_none).

Lemma seed_wf : wf seed.
Proof.
  unfold wf, bounded, loan_held, loan_exclusive, no_double_loan, seed; simpl.
  split; [| split; [| split]].
  - intros t c Hc. destruct (Nat.eqb t 1). exact Hc. unfold mask_none in Hc. discriminate.
  - intros ch c Hc. unfold mask_none in Hc. discriminate.
  - intros p ch c Ho. discriminate.
  - intros p ch r c Ho. discriminate.
Qed.

(* Task 1 lends cap 0 to a fresh task 2, then task 2 returns it. *)
Theorem lend_then_return_round_trip :
  exists g', steps seed g' /\ hold g' 1 0 = true /\ hold g' 2 0 = false
             /\ borrowed g' 2 0 = false.
Proof.
  eexists. split.
  - eapply steps_trans.
    + apply (step_lend seed 1 2 (only 0)).
      * unfold fresh, seed; simpl. split; [| split; [| split]].
        -- intro c; reflexivity.
        -- reflexivity.
        -- intro c; reflexivity.
        -- intros r Hr; discriminate Hr.
      * discriminate.
      * unfold subset, only, seed; simpl. intros c Hc. apply Nat.eqb_eq in Hc. subst. reflexivity.
      * unfold disjoint, seed; simpl. intros c _. reflexivity.
    + eapply steps_trans.
      * apply (step_return _ 1 2).
        -- simpl. unfold upd. reflexivity.
        -- discriminate.
      * apply steps_refl.
  - simpl. unfold upd, mask_or, mask_and_not, mask_none, only. simpl.
    split; [| split]; reflexivity.
Qed.

(* ===================================================================== *)
(* 6. Refutation: the current per-thread default context forges           *)
(*                                                                        *)
(* src/runtime/pgy_runtime_context.h binds the capability context as       *)
(* _Thread_local; no spawn path propagates it, so a pool worker evaluates  *)
(* its gates against the default context, whose mask is PGY_CAP_ALL.       *)
(* Modelled: a spawn that gives the child the full default mask.            *)
(* ===================================================================== *)

Definition default_all : Mask := fun _ => true.

Inductive tls_step : Cfg -> Cfg -> Prop :=
  | tls_spawn : forall g p ch,
      fresh g ch -> p <> ch ->
      tls_step g (mkCfg (manifest g)
                        (upd (hold g) ch default_all)
                        (upd (owner_of g) ch (Some p))
                        (borrowed g)).

(* Sandbox manifest {io_read = 0}: task 1 may only read. *)
Definition sandbox : Cfg :=
  mkCfg (only 0)
        (fun t => if Nat.eqb t 1 then only 0 else mask_none)
        (fun _ => None)
        (fun _ => mask_none).

Definition network : Cap := 1.

Theorem tls_default_forges :
  bounded sandbox /\
  exists g', tls_step sandbox g' /\ hold g' 2 network = true /\ manifest g' network = false.
Proof.
  split.
  - unfold bounded, sandbox; simpl. intros t c Hc.
    destruct (Nat.eqb t 1). exact Hc. unfold mask_none in Hc. discriminate.
  - eexists. split.
    + apply (tls_spawn sandbox 1 2).
      * unfold fresh, sandbox; simpl. split; [| split; [| split]].
        -- intro c; reflexivity.
        -- reflexivity.
        -- intro c; reflexivity.
        -- intros r Hr; discriminate Hr.
      * discriminate.
    + simpl. unfold upd, default_all, only, network. simpl. split; reflexivity.
Qed.

(* The structured spawn forms cannot do this: from the same sandbox every
   structured step keeps the worker inside the manifest. *)
Corollary structured_spawn_cannot_forge : forall g',
  steps sandbox g' -> hold g' 2 network = true -> manifest g' network = true.
Proof.
  intros g' Hrun Hc.
  assert (Hwf : wf sandbox).
  { unfold wf, bounded, loan_held, loan_exclusive, no_double_loan, sandbox; simpl.
    split; [| split; [| split]].
    - intros t c Hc0. destruct (Nat.eqb t 1). exact Hc0. unfold mask_none in Hc0. discriminate.
    - intros ch c Hc0. unfold mask_none in Hc0. discriminate.
    - intros p ch c Ho. discriminate.
    - intros p ch r c Ho. discriminate. }
  exact (run_bounded sandbox g' Hwf Hrun 2 network Hc).
Qed.
