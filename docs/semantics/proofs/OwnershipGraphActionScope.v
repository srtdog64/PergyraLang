(*
  Pergyra Formal Semantics -- action-scoped references and sagas over the
  graph store of OwnershipGraphLinks.
  Target: docs/semantics/29_action_scoped_references.md.

  Principle: no value carries a declared lifetime. Whether a node still
  exists is decided only by ownership events, a delete of its slot or a drop
  of its store. A link is a name, not a promise: using a stale one is an
  explicit refusal, never undefined behaviour. What a program may rely on
  instead is the extent of an action. A step acquires the links it reads
  when it starts and holds them until it ends; destroying a held node is
  refused meanwhile. Between the steps of a saga nothing is held, so a node
  deleted in between is discovered when the next step starts, and the saga
  compensates the steps it has completed.

  Mechanized here:
  - [spared_step], [spared_run_keeps_link], [stale_needs_a_destroyer]: a link
    that resolves keeps resolving, to the same blocks, through every
    operation that neither deletes its slot nor drops its store. Writes,
    inserts, table growth, new stores, borrows and their ends cannot make it
    stale.
  - [held_refuses_destroyers], [held_step], [held_run_keeps_link]: while its
    place is borrowed both destroyers are refused, so a held link resolves
    after every operation that does not end a borrow.
  - [admitted_step_never_fails_deref]: a step that acquires every link it
    reads and changes no borrow inside its body never fails a dereference.
    Its other outcomes are an acquisition refusal at its start or a refused
    body operation, where the destroyer's refusal is observable.
  - [run_step_restores_borrows]: a step releases exactly what it acquired,
    including after a partial acquisition.
  - [saga_never_fails_deref]: in a saga of admitted steps, whatever other
    code runs between the steps, no outcome is a failed dereference.
  - [run_body_receipt_sound], [run_body_receipt_projects]: a receipt counts
    the actually successful prefix, including reads, and its graph effects
    account for the final body state. The refused operation is not counted.
    [run_step_receipt_bound] bounds it by that invocation's body length.
  Falsifiers:
  - [neighbor_needs_its_own_acquisition]: holding A does not protect C, a
    node named by A's edge; reading C unacquired after it is deleted fails.
  - [unguarded_delete_breaks_hold]: a delete that ignores borrows makes a
    held link stale, so the borrow check is load-bearing.
  - [stale_at_step_start_compensates]: a node deleted between two steps is
    found at the next step's start and the earlier step is compensated.
  - [deleted_compensation_target_gets_stuck]: a node the compensation needs,
    deleted between the steps, leaves the saga observably stuck. Sparing
    compensation targets between steps is a premise, not a theorem here;
    Pergyra must discharge it by deletion authority, which is not bound.
  - [partial_forward_effect_is_reported], [noop_compensation_is_not_restore],
    [partial_compensation_preserves_both_failures]: failure can retain effects
    of the current forward or compensating step. CompensationFinished is a
    non-success outcome, not proof of restoration or permission to retry.

  Negative scope: sequential only, with no worker, async or FFI interleaving
  inside a step. Acquisition sets are given, not inferred, and dynamic
  traversal (acquiring a node on first touch) is not modelled. A
  compensation is an arbitrary admitted step; that it undoes its forward
  step is CompensationCore's interface obligation, not shown here. No
  surface syntax, compiler or runtime refinement, or cost is established.
*)

Require Import Stdlib.Lists.List.
Require Import Stdlib.Arith.PeanoNat.
Require Import Stdlib.Bool.Bool.
Require Import Stdlib.micromega.Lia.
Require Import OwnershipCleanCore.
Require Import OwnershipGraphLinks.
Import ListNotations.

(* ------------------------------------------------------------------ *)
(* Steps and sagas                                                      *)
(* ------------------------------------------------------------------ *)

(* A body operation is a machine operation or a dereference of a link. *)
Inductive BOp : Type := BDo (o : GOp) | BRead (l : Link).

(* A step names the links it acquires at its start, read (false) or write
   (true), and its body. *)
Record Step := mkStep { acq : list (Link * bool); body : list BOp }.

Inductive StepOut : Type :=
| StepDone
| AcqFailed (r : Refusal)
| BodyRefused (r : Refusal)
| DerefFailed (l : Link).

(* [completed] counts the successful body prefix, including reads. It is
   neither an undo log nor a count of persistent effects. Interpret it only
   against this invocation's body; never replay it as a recovery command. *)
Record StepReceipt := mkStepReceipt { step_out : StepOut; completed : nat }.

Definition record_success (r : GS * StepReceipt) : GS * StepReceipt :=
  (fst r, mkStepReceipt (step_out (snd r)) (S (completed (snd r)))).

Record SagaStep := mkSagaStep { forward : Step; compensation : Step }.

(* [k] is the forward step that failed and [j] the completed step whose
   compensation failed. Finishing compensations is not effect restoration.
   Both failure receipts survive a stuck compensation; neither outcome is
   SagaDone or permission to retry. *)
Inductive SagaOut : Type :=
| SagaDone
| SagaCompensationFinished (k : nat) (failed_forward : StepReceipt)
| SagaStuck (k j : nat) (failed_forward failed_compensation : StepReceipt).

Definition not_deref (o : StepOut) : Prop := forall l, o <> DerefFailed l.

Definition saga_deref_free (o : SagaOut) : Prop :=
  match o with
  | SagaDone => True
  | SagaCompensationFinished _ w => not_deref (step_out w)
  | SagaStuck _ _ w c => not_deref (step_out w) /\ not_deref (step_out c)
  end.

(* The two operations that can end a node: a delete of its slot and a drop
   of its store. Every other operation spares every link. *)
Definition spares (l : Link) (o : GOp) : bool :=
  match o with
  | ODelete l' => negb (Nat.eqb (lsid l') (lsid l) && Nat.eqb (lidx l') (lidx l))
  | ODrop sid => negb (Nat.eqb sid (lsid l))
  | _ => true
  end.

Definition keeps_borrows (o : GOp) : bool :=
  match o with OBegin _ _ | OEnd _ => false | _ => true end.

Definition ends_no_borrow (o : GOp) : bool :=
  match o with OEnd _ => false | _ => true end.

Definition holds (g : GS) (l : Link) : bool := existsb (same_place (lsid l) (lidx l)) (gbor g).

(* Admission: a body may read only what its step acquired, and may neither
   acquire nor release inside the body. *)
Definition bop_ok (ls : list Link) (b : BOp) : Prop :=
  match b with BDo o => keeps_borrows o = true | BRead l => In l ls end.

Definition step_ok (st : Step) : Prop := Forall (bop_ok (map fst (acq st))) (body st).

Definition LiveAt (ss : list Store) (l : Link) (bs : list Block) : Prop :=
  exists s sl nd, find_store (lsid l) ss = Some s /\ nth_error (sslots s) (lidx l) = Some sl /\
    sgen sl = lgen l /\ snode sl = Some nd /\ nblocks nd = bs.

(* A live link resolves to a node held in [bs]: the same identity, whatever
   writes have done to its data and edges. *)
Definition live_as (g : GS) (l : Link) (bs : list Block) : Prop :=
  exists nd, resolve g l = Found nd /\ nblocks nd = bs.

(* ------------------------------------------------------------------ *)
(* Generic lemmas                                                       *)
(* ------------------------------------------------------------------ *)

Lemma live_as_at : forall g l bs, live_as g l bs <-> LiveAt (gstores g) l bs.
Proof.
  intros g l bs. split.
  - intros [nd [Hr Hb]]. apply resolve_found_spec in Hr.
    destruct Hr as [s [sl [Ef [En [Eg Eo]]]]]. exists s, sl, nd.
    split; [exact Ef| split; [exact En| split; [exact Eg| split; [exact Eo| exact Hb]]]].
  - intros [s [sl [nd [Ef [En [Eg [Eo Hb]]]]]]]. exists nd. split; [|exact Hb].
    apply resolve_found_spec. exists s, sl.
    split; [exact Ef| split; [exact En| split; [exact Eg| exact Eo]]].
Qed.

Lemma resolve_stores : forall g g' l, gstores g' = gstores g -> resolve g' l = resolve g l.
Proof. intros g g' l E. unfold resolve. rewrite E. reflexivity. Qed.

Lemma live_at_set : forall ss s s' l bs,
  find_store (ssid s') ss = Some s -> LiveAt ss l bs ->
  (lsid l = ssid s' -> forall sl nd, nth_error (sslots s) (lidx l) = Some sl -> sgen sl = lgen l ->
     snode sl = Some nd -> nblocks nd = bs ->
     exists sl' nd', nth_error (sslots s') (lidx l) = Some sl' /\ sgen sl' = lgen l /\
       snode sl' = Some nd' /\ nblocks nd' = bs) ->
  LiveAt (set_store s' ss) l bs.
Proof.
  intros ss s s' l bs Ef [s0 [sl [nd [E0 [En [Eg [Eo Hb]]]]]]] Hc.
  destruct (Nat.eq_dec (lsid l) (ssid s')) as [Eq|Ne].
  - rewrite Eq, Ef in E0. injection E0 as E0. subst s0.
    destruct (Hc Eq sl nd En Eg Eo Hb) as [sl' [nd' [En' [Eg' [Eo' Hb']]]]].
    exists s', sl', nd'. rewrite Eq, (find_set_same _ _ _ Ef).
    split; [reflexivity| split; [exact En'| split; [exact Eg'| split; [exact Eo'| exact Hb']]]].
  - exists s0, sl, nd. rewrite (find_set_other _ _ _ Ne).
    split; [exact E0| split; [exact En| split; [exact Eg| split; [exact Eo| exact Hb]]]].
Qed.

Lemma holds_store_borrowed : forall g l, holds g l = true -> store_borrowed (lsid l) (gbor g) = true.
Proof.
  intros g l H. unfold holds, store_borrowed in *. apply existsb_exists in H.
  destruct H as [b [Hin Hb]]. apply existsb_exists. exists b. split; [exact Hin|].
  unfold same_place in Hb. apply andb_true_iff in Hb. exact (proj1 Hb).
Qed.

Lemma forallb_false_exists : forall (A : Type) (f : A -> bool) l,
  forallb f l = false -> exists x, In x l /\ f x = false.
Proof.
  intros A f l. induction l as [|a r IH]; cbn [forallb]; intros H; [discriminate H|].
  destruct (f a) eqn:Ea; cbn [andb] in H.
  - destruct (IH H) as [x [Hx Hf]]. exists x. split; [right; exact Hx| exact Hf].
  - exists a. split; [left; reflexivity| exact Ea].
Qed.

Section ActionScope.
Variable gmax : nat.

(* ------------------------------------------------------------------ *)
(* Only a destroyer makes a link stale                                  *)
(* ------------------------------------------------------------------ *)

Theorem spared_step : forall g o l bs, GInv gmax g -> live_as g l bs -> spares l o = true ->
  live_as (fst (gexec gmax g o)) l bs.
Proof.
  intros g o l bs Hinv Hl Hsp. apply live_as_at. apply live_as_at in Hl.
  destruct o as [tb|sid idx d es bs0 tb|l0|l0 wr|i|i d es|sid]; cbn [gexec].
  - unfold g_new. destruct (bmem tb (gheap g)); [exact Hl|].
    destruct Hl as [s [sl [nd [Ef Hrest]]]]. exists s, sl, nd. split; [|exact Hrest].
    destruct (find_store_some _ _ _ Ef) as [Hs Hsid].
    assert (Hne : Nat.eqb (gsid g) (lsid l) = false).
    { apply Nat.eqb_neq. pose proof (gi_sid_lt _ _ Hinv s Hs). lia. }
    cbn [fst gstores find_store ssid]. rewrite Hne. exact Ef.
  - unfold g_insert. destruct (find_store sid (gstores g)) as [s|] eqn:Ef; [|exact Hl].
    destruct (nth_error (sslots s) idx) as [sl|] eqn:En.
    + destruct (snode sl) eqn:Eo; [exact Hl|].
      destruct (Nat.ltb (sgen sl) gmax); [|exact Hl].
      destruct (bnodup bs0 && bfree bs0 (gheap g)); [|exact Hl].
      cbn [fst gstores]. apply (live_at_set _ s); [exact Ef| exact Hl|].
      intros _ sl0 nd0 En0 Eg0 Eo0 Hb0. cbn [sslots].
      destruct (Nat.eq_dec (lidx l) idx) as [Ei|Ei].
      * rewrite Ei, En in En0. injection En0 as En0. subst sl0. congruence.
      * exists sl0, nd0. rewrite (nth_upd_other _ _ _ _ _ Ei).
        split; [exact En0| split; [exact Eg0| split; [exact Eo0| exact Hb0]]].
    + destruct (store_borrowed sid (gbor g)); [exact Hl|].
      destruct (Nat.eqb idx (length (sslots s)) && Nat.ltb 0 gmax); [|exact Hl].
      destruct (bnodup (tb :: bs0) && bfree (tb :: bs0) (free [stab s] (gheap g))); [|exact Hl].
      cbn [fst gstores]. apply (live_at_set _ s); [exact Ef| exact Hl|].
      intros _ sl0 nd0 En0 Eg0 Eo0 Hb0. exists sl0, nd0. cbn [sslots].
      rewrite nth_error_app1 by (apply nth_error_Some; congruence).
      split; [exact En0| split; [exact Eg0| split; [exact Eo0| exact Hb0]]].
  - unfold g_delete. destruct (find_store (lsid l0) (gstores g)) as [s|] eqn:Ef; [|exact Hl].
    destruct (nth_error (sslots s) (lidx l0)) as [[gen [nd|]]|] eqn:En; try exact Hl.
    destruct (Nat.eqb gen (lgen l0)); [|exact Hl].
    destruct (existsb (same_place (lsid l0) (lidx l0)) (gbor g)); [exact Hl|].
    destruct (find_store_some _ _ _ Ef) as [_ Hsid].
    cbn [fst gstores]. apply (live_at_set _ s); [cbn [ssid]; rewrite Hsid; exact Ef| exact Hl|].
    intros Eq sl0 nd0 En0 Eg0 Eo0 Hb0. cbn [sslots ssid] in Eq |- *.
    destruct (Nat.eq_dec (lidx l) (lidx l0)) as [Ei|Ei].
    + exfalso. cbn [spares] in Hsp. rewrite Hsid in Eq. rewrite <- Eq, Ei, !Nat.eqb_refl in Hsp.
      cbn in Hsp. discriminate Hsp.
    + exists sl0, nd0. rewrite (nth_upd_other _ _ _ _ _ Ei).
      split; [exact En0| split; [exact Eg0| split; [exact Eo0| exact Hb0]]].
  - unfold g_begin. destruct (find_store (lsid l0) (gstores g)) as [s|]; [|exact Hl].
    destruct (follow s (lidx l0, lgen l0)); [|exact Hl].
    destruct (conflicts wr (lsid l0) (lidx l0) (gbor g)); exact Hl.
  - unfold g_end. destruct (Nat.ltb i (length (gbor g))); exact Hl.
  - unfold g_write. destruct (nth_error (gbor g) i) as [b|]; [|exact Hl].
    destruct (negb (bwr b)); [exact Hl|].
    destruct (find_store (bsid b) (gstores g)) as [s|] eqn:Ef; [|exact Hl].
    destruct (nth_error (sslots s) (bidx b)) as [[gen [nd|]]|] eqn:En; try exact Hl.
    destruct (find_store_some _ _ _ Ef) as [_ Hsid].
    cbn [fst gstores]. apply (live_at_set _ s); [cbn [ssid]; rewrite Hsid; exact Ef| exact Hl|].
    intros _ sl0 nd0 En0 Eg0 Eo0 Hb0. cbn [sslots].
    destruct (Nat.eq_dec (lidx l) (bidx b)) as [Ei|Ei].
    + rewrite Ei, En in En0. injection En0 as En0. subst sl0.
      cbn [sgen snode] in Eg0, Eo0. injection Eo0 as Eo0. subst nd0.
      exists (mkSlot gen (Some (mkNode d es (nblocks nd)))), (mkNode d es (nblocks nd)).
      rewrite Ei. split; [apply nth_upd_same; apply nth_error_Some; congruence|].
      split; [exact Eg0| split; [reflexivity| exact Hb0]].
    + exists sl0, nd0. rewrite (nth_upd_other _ _ _ _ _ Ei).
      split; [exact En0| split; [exact Eg0| split; [exact Eo0| exact Hb0]]].
  - unfold g_drop. destruct (find_store sid (gstores g)) as [s|]; [|exact Hl].
    destruct (store_borrowed sid (gbor g)); [exact Hl|].
    cbn [spares] in Hsp. apply negb_true_iff in Hsp. apply Nat.eqb_neq in Hsp.
    destruct Hl as [s0 [sl [nd [E0 Hrest]]]]. exists s0, sl, nd. split; [|exact Hrest].
    cbn [fst gstores]. rewrite find_del_other by (intros E; apply Hsp; symmetry; exact E).
    exact E0.
Qed.

Theorem spared_run_keeps_link : forall os g l bs, GInv gmax g -> live_as g l bs ->
  forallb (spares l) os = true -> live_as (grun gmax g os) l bs.
Proof.
  induction os as [|o r IH]; intros g l bs Hinv Hl Hs; [exact Hl|].
  cbn [forallb] in Hs. apply andb_true_iff in Hs. destruct Hs as [Ho Hr].
  cbn [grun]. apply IH; [apply ginv_step; exact Hinv| apply spared_step; assumption| exact Hr].
Qed.

(* A link goes stale only if the run deleted its slot or dropped its store. *)
Corollary stale_needs_a_destroyer : forall os g l nd r, GInv gmax g -> resolve g l = Found nd ->
  resolve (grun gmax g os) l = Missing r -> exists o, In o os /\ spares l o = false.
Proof.
  intros os g l nd r Hinv Hf Hm. destruct (forallb (spares l) os) eqn:E.
  - destruct (spared_run_keeps_link os g l (nblocks nd) Hinv (ex_intro _ nd (conj Hf eq_refl)) E)
      as [nd' [Hf' _]]. congruence.
  - exact (forallb_false_exists _ _ _ E).
Qed.

(* ------------------------------------------------------------------ *)
(* A held link cannot be destroyed                                      *)
(* ------------------------------------------------------------------ *)

Theorem held_refuses_destroyers : forall g l o, holds g l = true -> spares l o = false ->
  fst (gexec gmax g o) = g.
Proof.
  intros g l o Hh Hsp.
  destruct o as [tb|sid idx d es bs tb|l0|l0 wr|i|i d es|sid];
    try (cbn [spares] in Hsp; discriminate Hsp).
  - cbn [spares] in Hsp. apply negb_false_iff in Hsp. apply andb_true_iff in Hsp.
    destruct Hsp as [E1 E2]. apply Nat.eqb_eq in E1. apply Nat.eqb_eq in E2.
    apply delete_refused_while_borrowed. unfold holds in Hh. rewrite E1, E2. exact Hh.
  - cbn [spares] in Hsp. apply negb_false_iff in Hsp. apply Nat.eqb_eq in Hsp. subst sid.
    cbn [gexec]. unfold g_drop. destruct (find_store (lsid l) (gstores g)); [|reflexivity].
    rewrite (holds_store_borrowed _ _ Hh). reflexivity.
Qed.

Theorem held_step : forall g o l bs, GInv gmax g -> holds g l = true -> live_as g l bs ->
  live_as (fst (gexec gmax g o)) l bs.
Proof.
  intros g o l bs Hinv Hh Hl. destruct (spares l o) eqn:Hsp.
  - exact (spared_step g o l bs Hinv Hl Hsp).
  - rewrite (held_refuses_destroyers g l o Hh Hsp). exact Hl.
Qed.

Lemma keeps_borrows_gbor : forall g o, keeps_borrows o = true -> gbor (fst (gexec gmax g o)) = gbor g.
Proof.
  intros g o Hk.
  destruct o as [tb|sid idx d es bs tb|l0|l0 wr|i|i d es|sid];
    try (cbn [keeps_borrows] in Hk; discriminate Hk); cbn [gexec].
  - unfold g_new. destruct (bmem tb (gheap g)); reflexivity.
  - unfold g_insert. destruct (find_store sid (gstores g)) as [s|]; [|reflexivity].
    destruct (nth_error (sslots s) idx) as [sl|].
    + destruct (snode sl); [reflexivity|]. destruct (Nat.ltb (sgen sl) gmax); [|reflexivity].
      destruct (bnodup bs && bfree bs (gheap g)); reflexivity.
    + destruct (store_borrowed sid (gbor g)); [reflexivity|].
      destruct (Nat.eqb idx (length (sslots s)) && Nat.ltb 0 gmax); [|reflexivity].
      destruct (bnodup (tb :: bs) && bfree (tb :: bs) (free [stab s] (gheap g))); reflexivity.
  - unfold g_delete. destruct (find_store (lsid l0) (gstores g)) as [s|]; [|reflexivity].
    destruct (nth_error (sslots s) (lidx l0)) as [[gen [nd|]]|]; try reflexivity.
    destruct (Nat.eqb gen (lgen l0)); [|reflexivity].
    destruct (existsb (same_place (lsid l0) (lidx l0)) (gbor g)); reflexivity.
  - unfold g_write. destruct (nth_error (gbor g) i) as [b|]; [|reflexivity].
    destruct (negb (bwr b)); [reflexivity|].
    destruct (find_store (bsid b) (gstores g)) as [s|]; [|reflexivity].
    destruct (nth_error (sslots s) (bidx b)) as [[gen [nd|]]|]; reflexivity.
  - unfold g_drop. destruct (find_store sid (gstores g)) as [s|]; [|reflexivity].
    destruct (store_borrowed sid (gbor g)); reflexivity.
Qed.

(* Only ending a borrow can end a hold. *)
Lemma holds_step : forall g o l, ends_no_borrow o = true -> holds g l = true ->
  holds (fst (gexec gmax g o)) l = true.
Proof.
  intros g o l Hk Hh. destruct (keeps_borrows o) eqn:Ek.
  - unfold holds. rewrite (keeps_borrows_gbor g o Ek). exact Hh.
  - destruct o as [tb|sid idx d es bs tb|l0|l0 wr|i|i d es|sid];
      try (cbn [keeps_borrows] in Ek; discriminate Ek);
      [| cbn [ends_no_borrow] in Hk; discriminate Hk].
    cbn [gexec]. unfold g_begin. destruct (find_store (lsid l0) (gstores g)) as [s|]; [|exact Hh].
    destruct (follow s (lidx l0, lgen l0)); [|exact Hh].
    destruct (conflicts wr (lsid l0) (lidx l0) (gbor g)); [exact Hh|].
    unfold holds in *. cbn [fst gbor existsb]. rewrite Hh. apply orb_true_r.
Qed.

Theorem held_run_keeps_link : forall os g l bs, GInv gmax g -> holds g l = true -> live_as g l bs ->
  forallb ends_no_borrow os = true -> live_as (grun gmax g os) l bs /\ holds (grun gmax g os) l = true.
Proof.
  induction os as [|o r IH]; intros g l bs Hinv Hh Hl Hs; [split; assumption|].
  cbn [forallb] in Hs. apply andb_true_iff in Hs. destruct Hs as [Ho Hr]. cbn [grun].
  apply IH; [apply ginv_step; exact Hinv| apply holds_step; assumption|
             apply held_step; assumption| exact Hr].
Qed.

(* ------------------------------------------------------------------ *)
(* Steps                                                                *)
(* ------------------------------------------------------------------ *)

(* Acquire in order; stop at the first refusal. [n] counts the borrows
   taken so far. g_begin answers only GUnit or a refusal ([begin_shape]),
   so the last branch below is unreachable, not a fallback. *)
Fixpoint acquire (g : GS) (ls : list (Link * bool)) (n : nat) : GS * nat * option Refusal :=
  match ls with
  | [] => (g, n, None)
  | (l, wr) :: r =>
      match snd (gexec gmax g (OBegin l wr)) with
      | GUnit => acquire (fst (gexec gmax g (OBegin l wr))) r (S n)
      | GRefused rf => (fst (gexec gmax g (OBegin l wr)), n, Some rf)
      | _ => (fst (gexec gmax g (OBegin l wr)), n, Some RNoBorrow)
      end
  end.

(* Each borrow is taken at the front of the borrow list, so ending the
   front [n] times releases exactly the [n] taken. *)
Definition release (g : GS) (n : nat) : GS := grun gmax g (repeat (OEnd 0) n).

Fixpoint run_body (g : GS) (bs : list BOp) : GS * StepReceipt :=
  match bs with
  | [] => (g, mkStepReceipt StepDone 0)
  | BDo o :: r =>
      let result := gexec gmax g o in
      match snd result with
      | GRefused rf => (fst result, mkStepReceipt (BodyRefused rf) 0)
      | _ => record_success (run_body (fst result) r)
      end
  | BRead l :: r =>
      match resolve g l with
      | Found _ => record_success (run_body g r)
      | Missing _ => (g, mkStepReceipt (DerefFailed l) 0)
      end
  end.

(* Whatever the outcome, the step releases what it acquired. *)
Definition run_step (g : GS) (st : Step) : GS * StepReceipt :=
  match acquire g (acq st) 0 with
  | (g1, n, Some rf) => (release g1 n, mkStepReceipt (AcqFailed rf) 0)
  | (g1, n, None) => let result := run_body g1 (body st) in
                     (release (fst result) n, snd result)
  end.

(* This property belongs to a single graph operation. It must not be lifted
   to a failed body: its already successful prefix remains observable. *)
Lemma graph_refusal_unchanged : forall g o rf,
  snd (gexec gmax g o) = GRefused rf -> fst (gexec gmax g o) = g.
Proof.
  intros g o rf. destruct o;
    unfold gexec, g_new, g_insert, g_delete, g_begin, g_end, g_write, g_drop;
    repeat match goal with
    | |- context [match ?x with _ => _ end] => destruct x eqn:?
    end; cbn [refuse fst snd]; intros H; try discriminate H; reflexivity.
Qed.

(* Only successful operations occur in this relation; it cannot hide an
   ignored refusal. Reads are part of the prefix, but not heap mutations. *)
Inductive BodyPrefixExec : GS -> list BOp -> GS -> Prop :=
| PrefixEmpty : forall g, BodyPrefixExec g [] g
| PrefixDo : forall g o bs after,
    (forall rf, snd (gexec gmax g o) <> GRefused rf) ->
    BodyPrefixExec (fst (gexec gmax g o)) bs after ->
    BodyPrefixExec g (BDo o :: bs) after
| PrefixRead : forall g l nd bs after,
    resolve g l = Found nd -> BodyPrefixExec g bs after ->
    BodyPrefixExec g (BRead l :: bs) after.

Definition body_stop (g : GS) (out : StepOut) (rest : list BOp) : Prop :=
  match out with
  | StepDone => rest = []
  | AcqFailed _ => False
  | BodyRefused rf => exists o tail, rest = BDo o :: tail /\ snd (gexec gmax g o) = GRefused rf
  | DerefFailed l => exists rf tail, rest = BRead l :: tail /\ resolve g l = Missing rf
  end.

Theorem run_body_receipt_sound : forall bs g,
  exists prefix rest,
    bs = prefix ++ rest /\ completed (snd (run_body g bs)) = length prefix /\
    BodyPrefixExec g prefix (fst (run_body g bs)) /\
    body_stop (fst (run_body g bs)) (step_out (snd (run_body g bs))) rest.
Proof.
  induction bs as [|[o|l] bs IH]; intros g.
  - exists [], []. cbn. repeat split; constructor.
  - cbn [run_body]. destruct (snd (gexec gmax g o)) eqn:E;
      cbn [record_success fst snd completed step_out].
    + destruct (IH (fst (gexec gmax g o))) as [p [r [Eb [Ec [Hp Hs]]]]].
      exists (BDo o :: p), r. cbn [length app]. rewrite Ec.
      split; [now rewrite Eb|]. split; [reflexivity|]. split; [|exact Hs].
      apply PrefixDo; [intros rf H; rewrite E in H; discriminate H| exact Hp].
    + destruct (IH (fst (gexec gmax g o))) as [p [r [Eb [Ec [Hp Hs]]]]].
      exists (BDo o :: p), r. cbn [length app]. rewrite Ec.
      split; [now rewrite Eb|]. split; [reflexivity|]. split; [|exact Hs].
      apply PrefixDo; [intros rf H; rewrite E in H; discriminate H| exact Hp].
    + destruct (IH (fst (gexec gmax g o))) as [p [r [Eb [Ec [Hp Hs]]]]].
      exists (BDo o :: p), r. cbn [length app]. rewrite Ec.
      split; [now rewrite Eb|]. split; [reflexivity|]. split; [|exact Hs].
      apply PrefixDo; [intros rf H; rewrite E in H; discriminate H| exact Hp].
    + rewrite (graph_refusal_unchanged g o r E).
      exists [], (BDo o :: bs). cbn [app length body_stop].
      repeat split; [constructor| exists o, bs; split; [reflexivity| exact E]].
  - cbn [run_body]. destruct (resolve g l) as [nd|rf] eqn:E;
      cbn [record_success fst snd completed step_out].
    + destruct (IH g) as [p [r [Eb [Ec [Hp Hs]]]]].
      exists (BRead l :: p), r. cbn [length app]. rewrite Ec.
      split; [now rewrite Eb|]. split; [reflexivity|]. split; [|exact Hs].
      eapply PrefixRead; eauto.
    + exists [], (BRead l :: bs). cbn [app length body_stop].
      repeat split; [constructor| exists rf, bs; split; [reflexivity| exact E]].
Qed.

Theorem run_step_receipt_bound : forall g st,
  completed (snd (run_step g st)) <= length (body st).
Proof.
  intros g st. unfold run_step. destruct (acquire g (acq st) 0) as [[g1 n] ro].
  destruct ro; cbn [snd completed]; [lia|].
  destruct (run_body_receipt_sound (body st) g1) as [p [r [Eb [Ec _]]]].
  rewrite Ec, Eb, length_app. lia.
Qed.

Fixpoint body_machine_ops (bs : list BOp) : list GOp :=
  match bs with
  | [] => []
  | BDo o :: rest => o :: body_machine_ops rest
  | BRead _ :: rest => body_machine_ops rest
  end.

Lemma body_prefix_projects : forall g prefix after,
  BodyPrefixExec g prefix after ->
  after = grun gmax g (body_machine_ops prefix) /\
  Forall (fun result => forall rf, result <> GRefused rf)
    (gresults gmax g (body_machine_ops prefix)).
Proof.
  intros g prefix after H. induction H; cbn [body_machine_ops grun gresults].
  - split; [reflexivity| constructor].
  - destruct IHBodyPrefixExec as [He Hr]. split; [exact He| constructor; assumption].
  - exact IHBodyPrefixExec.
Qed.

Theorem run_body_receipt_projects : forall bs g,
  exists prefix rest, bs = prefix ++ rest /\
    completed (snd (run_body g bs)) = length prefix /\
    fst (run_body g bs) = grun gmax g (body_machine_ops prefix) /\
    Forall (fun result => forall rf, result <> GRefused rf)
      (gresults gmax g (body_machine_ops prefix)) /\
    body_stop (fst (run_body g bs)) (step_out (snd (run_body g bs))) rest.
Proof.
  intros bs g. destruct (run_body_receipt_sound bs g) as [p [r [Eb [Ec [Hp Hs]]]]].
  destruct (body_prefix_projects _ _ _ Hp) as [He Hr].
  exists p, r. repeat split; assumption.
Qed.

Theorem completed_step_accounts_for_whole_body : forall g st,
  step_out (snd (run_step g st)) = StepDone ->
  completed (snd (run_step g st)) = length (body st).
Proof.
  intros g st. unfold run_step. destruct (acquire g (acq st) 0) as [[g1 n] ro].
  destruct ro; cbn [snd step_out completed]; [discriminate|]. intros E.
  destruct (run_body_receipt_sound (body st) g1) as [p [r [Eb [Ec [_ Hs]]]]].
  rewrite E in Hs. cbn [body_stop] in Hs. subst r.
  rewrite app_nil_r in Eb. now rewrite Ec, Eb.
Qed.

Theorem acquisition_failure_has_no_body_prefix : forall g st rf,
  step_out (snd (run_step g st)) = AcqFailed rf -> completed (snd (run_step g st)) = 0.
Proof.
  intros g st rf. unfold run_step. destruct (acquire g (acq st) 0) as [[g1 n] ro].
  destruct ro; cbn [snd step_out completed]; [reflexivity|]. intros E.
  destruct (run_body_receipt_sound (body st) g1) as [p [r [_ [_ [_ Hs]]]]].
  rewrite E in Hs. contradiction.
Qed.

Lemma begin_shape : forall g l wr g' res, gexec gmax g (OBegin l wr) = (g', res) ->
  (res = GUnit /\ gstores g' = gstores g /\
     (exists b, gbor g' = b :: gbor g /\ same_place (lsid l) (lidx l) b = true) /\
     exists nd, resolve g l = Found nd) \/
  (exists rf, res = GRefused rf /\ g' = g).
Proof.
  intros g l wr g' res H. cbn [gexec] in H. unfold g_begin in H.
  destruct (find_store (lsid l) (gstores g)) as [s|] eqn:Ef.
  - destruct (follow s (lidx l, lgen l)) as [nd|] eqn:Efl.
    + destruct (conflicts wr (lsid l) (lidx l) (gbor g)).
      * right. unfold refuse in H. injection H as E1 E2. exists RConflict.
        split; [symmetry; exact E2| symmetry; exact E1].
      * left. injection H as E1 E2. subst g' res. split; [reflexivity|].
        split; [reflexivity|]. split.
        -- eexists. split; [reflexivity|]. unfold same_place. cbn [bsid bidx].
           rewrite !Nat.eqb_refl. reflexivity.
        -- exists nd. unfold resolve. rewrite Ef, Efl. reflexivity.
    + right. unfold refuse in H. injection H as E1 E2. exists RStale.
      split; [symmetry; exact E2| symmetry; exact E1].
  - right. unfold refuse in H. injection H as E1 E2. exists RNoStore.
    split; [symmetry; exact E2| symmetry; exact E1].
Qed.

Lemma acquire_spec : forall ls g n g1 n1 ro, GInv gmax g -> acquire g ls n = (g1, n1, ro) ->
  GInv gmax g1 /\ gstores g1 = gstores g /\
  (exists pre, gbor g1 = pre ++ gbor g /\ n1 = n + length pre) /\
  (forall l, holds g l = true -> holds g1 l = true) /\
  (ro = None -> forall l, In l (map fst ls) -> holds g1 l = true /\ exists nd, resolve g1 l = Found nd).
Proof.
  induction ls as [|[l0 wr] r IH]; intros g n g1 n1 ro Hinv H.
  - cbn [acquire] in H. injection H as E1 E2 E3. subst g1 n1 ro.
    split; [exact Hinv|]. split; [reflexivity|].
    split; [exists []; split; [reflexivity| cbn; lia]|].
    split; [intros l Hl; exact Hl| intros _ l Hl; destruct Hl].
  - cbn [acquire] in H. destruct (gexec gmax g (OBegin l0 wr)) as [g' res] eqn:Ex.
    cbn [fst snd] in H.
    pose proof (ginv_step gmax g (OBegin l0 wr) Hinv) as Hinv'. rewrite Ex in Hinv'. cbn [fst] in Hinv'.
    destruct (begin_shape g l0 wr g' res Ex) as [[Er [Es [[b [Eb Hb]] [nd Hf]]]] | [rf [Er Eg]]].
    + subst res. destruct (IH g' (S n) g1 n1 ro Hinv' H) as [Hi1 [Es1 [[pre [Eb1 En1]] [Hh1 Hn1]]]].
      split; [exact Hi1|]. split; [rewrite Es1; exact Es|].
      split; [exists (pre ++ [b]); split;
              [rewrite Eb1, Eb, <- app_assoc; reflexivity| rewrite En1, length_app; cbn; lia]|].
      split.
      * intros l Hl. apply Hh1. unfold holds in *. rewrite Eb. cbn [existsb]. rewrite Hl. apply orb_true_r.
      * intros Hro l Hl. cbn [map In fst] in Hl. destruct Hl as [El|Hl].
        -- subst l. split.
           ++ apply Hh1. unfold holds. rewrite Eb. cbn [existsb]. rewrite Hb. reflexivity.
           ++ exists nd. rewrite (resolve_stores g g1 l0); [exact Hf| rewrite Es1; exact Es].
        -- exact (Hn1 Hro l Hl).
    + subst res g'. injection H as E1 E2 E3. subst g1 n1 ro.
      split; [exact Hinv|]. split; [reflexivity|].
      split; [exists []; split; [reflexivity| cbn; lia]|].
      split; [intros l Hl; exact Hl| intros Hro; discriminate Hro].
Qed.

Definition HeldLive (L : list Link) (g : GS) : Prop :=
  GInv gmax g /\ forall l, In l L -> holds g l = true /\ exists nd, resolve g l = Found nd.

Lemma held_live_step : forall L g o, keeps_borrows o = true -> HeldLive L g ->
  HeldLive L (fst (gexec gmax g o)).
Proof.
  intros L g o Hk [Hinv Hl]. split; [apply ginv_step; exact Hinv|].
  intros l Hin. destruct (Hl l Hin) as [Hh [nd Hf]]. split.
  - unfold holds. rewrite (keeps_borrows_gbor g o Hk). exact Hh.
  - destruct (held_step g o l (nblocks nd) Hinv Hh (ex_intro _ nd (conj Hf eq_refl))) as [nd' [Hf' _]].
    exists nd'. exact Hf'.
Qed.

Lemma body_never_fails_deref : forall bs L g, HeldLive L g -> Forall (bop_ok L) bs ->
  (forall l, step_out (snd (run_body g bs)) <> DerefFailed l) /\ HeldLive L (fst (run_body g bs)).
Proof.
  induction bs as [|b r IH]; intros L g Hh Hok.
  - cbn [run_body fst snd step_out]. split; [intros l E; discriminate E| exact Hh].
  - pose proof (Forall_inv Hok) as Hb. pose proof (Forall_inv_tail Hok) as Hr.
    destruct b as [o|l0].
    + cbn [bop_ok] in Hb. pose proof (held_live_step L g o Hb Hh) as Hh'.
      cbn [run_body]. destruct (snd (gexec gmax g o)) as [l1|sid| |rf];
        cbn [record_success fst snd step_out].
      * apply IH; assumption.
      * apply IH; assumption.
      * apply IH; assumption.
      * split; [intros l E; discriminate E| exact Hh'].
    + cbn [bop_ok] in Hb. destruct Hh as [Hinv Hl]. destruct (Hl l0 Hb) as [_ [nd Hf]].
      cbn [run_body]. rewrite Hf. cbn [record_success fst snd step_out].
      apply IH; [split; assumption| exact Hr].
Qed.

Theorem admitted_step_never_fails_deref : forall g st l, GInv gmax g -> step_ok st ->
  step_out (snd (run_step g st)) <> DerefFailed l.
Proof.
  intros g st l Hinv Hok. unfold run_step.
  destruct (acquire g (acq st) 0) as [[g1 n] ro] eqn:Ea.
  destruct (acquire_spec (acq st) g 0 g1 n ro Hinv Ea) as [Hi1 [_ [_ [_ Hn]]]].
  destruct ro as [rf|]; cbn [snd].
  - discriminate.
  - exact (proj1 (body_never_fails_deref (body st) (map fst (acq st)) g1
                   (conj Hi1 (Hn eq_refl)) Hok) l).
Qed.

Lemma run_body_ginv : forall bs g, GInv gmax g -> GInv gmax (fst (run_body g bs)).
Proof.
  induction bs as [|[o|l] r IH]; intros g H; cbn [run_body].
  - exact H.
  - destruct (snd (gexec gmax g o)); cbn [record_success fst];
      try (apply IH); apply ginv_step; exact H.
  - destruct (resolve g l); cbn [record_success fst]; [apply IH; exact H| exact H].
Qed.

Lemma run_step_ginv : forall g st, GInv gmax g -> GInv gmax (fst (run_step g st)).
Proof.
  intros g st H. unfold run_step. destruct (acquire g (acq st) 0) as [[g1 n] ro] eqn:Ea.
  destruct (acquire_spec (acq st) g 0 g1 n ro H Ea) as [Hi1 _].
  destruct ro; cbn [fst]; unfold release; apply ginv_run;
    [exact Hi1| apply run_body_ginv; exact Hi1].
Qed.

Lemma release_pops : forall pre g rest, gbor g = pre ++ rest -> gbor (release g (length pre)) = rest.
Proof.
  induction pre as [|b pre IH]; intros g rest E; unfold release in *; cbn [length repeat grun].
  - exact E.
  - apply IH. cbn [gexec]. unfold g_end. rewrite E. cbn. reflexivity.
Qed.

Lemma body_keeps_borrows : forall bs L g, Forall (bop_ok L) bs -> gbor (fst (run_body g bs)) = gbor g.
Proof.
  induction bs as [|[o|l] r IH]; intros L g Hok; cbn [run_body].
  - reflexivity.
  - pose proof (Forall_inv Hok) as Hb. cbn [bop_ok] in Hb. pose proof (Forall_inv_tail Hok) as Hr.
    destruct (snd (gexec gmax g o)); cbn [record_success fst];
      try (rewrite (IH L _ Hr)); apply keeps_borrows_gbor; exact Hb.
  - pose proof (Forall_inv_tail Hok) as Hr.
    destruct (resolve g l); cbn [record_success fst]; [apply (IH L); exact Hr| reflexivity].
Qed.

Theorem run_step_restores_borrows : forall g st, GInv gmax g -> step_ok st ->
  gbor (fst (run_step g st)) = gbor g.
Proof.
  intros g st Hinv Hok. unfold run_step. destruct (acquire g (acq st) 0) as [[g1 n] ro] eqn:Ea.
  destruct (acquire_spec (acq st) g 0 g1 n ro Hinv Ea) as [_ [_ [[pre [Eb En]] _]]].
  cbn [Nat.add] in En. subst n.
  destruct ro; cbn [fst]; apply release_pops; [exact Eb|].
  rewrite (body_keeps_borrows (body st) (map fst (acq st)) g1 Hok). exact Eb.
Qed.

(* ------------------------------------------------------------------ *)
(* Sagas                                                                *)
(* ------------------------------------------------------------------ *)

Section Saga.
(* [gaps t] is what other code runs before the [t]-th step the saga runs,
   forward or compensating. It is arbitrary. *)
Variable gaps : nat -> list GOp.

Fixpoint compensate (t : nat) (g : GS) (done : list (nat * Step)) (k : nat) (why : StepReceipt)
    : GS * SagaOut :=
  match done with
  | [] => (g, SagaCompensationFinished k why)
  | (j, c) :: r =>
      let result := run_step (grun gmax g (gaps t)) c in
      match step_out (snd result) with
      | StepDone => compensate (S t) (fst result) r k why
      | _ => (fst result, SagaStuck k j why (snd result))
      end
  end.

(* [done] lists the completed steps, most recent first, with the
   compensation of each. *)
Fixpoint saga_from (t i : nat) (g : GS) (done : list (nat * Step)) (ss : list SagaStep)
    : GS * SagaOut :=
  match ss with
  | [] => (g, SagaDone)
  | s :: r =>
      let result := run_step (grun gmax g (gaps t)) (forward s) in
      match step_out (snd result) with
      | StepDone => saga_from (S t) (S i) (fst result) ((i, compensation s) :: done) r
      | _ => compensate (S t) (fst result) done i (snd result)
      end
  end.

Definition run_saga (g : GS) (ss : list SagaStep) : GS * SagaOut := saga_from 0 0 g [] ss.

Lemma compensate_ok : forall done t g k why, GInv gmax g -> not_deref (step_out why) ->
  Forall (fun jc => step_ok (snd jc)) done -> saga_deref_free (snd (compensate t g done k why)).
Proof.
  induction done as [|[j c] r IH]; intros t g k why Hinv Hw Hok; cbn [compensate].
  - exact Hw.
  - pose proof (Forall_inv Hok) as Hc. cbn [snd] in Hc. pose proof (Forall_inv_tail Hok) as Hr.
    pose proof (ginv_run gmax (gaps t) g Hinv) as Hg.
    destruct (step_out (snd (run_step (grun gmax g (gaps t)) c))) as [|rf|rf|l0] eqn:Eo.
    + apply IH; [apply run_step_ginv; exact Hg| exact Hw| exact Hr].
    + cbn [snd saga_deref_free]. split; [exact Hw| rewrite Eo; intros l E; discriminate E].
    + cbn [snd saga_deref_free]. split; [exact Hw| rewrite Eo; intros l E; discriminate E].
    + exfalso. exact (admitted_step_never_fails_deref _ _ l0 Hg Hc Eo).
Qed.

Lemma saga_from_ok : forall ss t i g done, GInv gmax g ->
  Forall (fun s => step_ok (forward s) /\ step_ok (compensation s)) ss ->
  Forall (fun jc => step_ok (snd jc)) done ->
  saga_deref_free (snd (saga_from t i g done ss)).
Proof.
  induction ss as [|s r IH]; intros t i g done Hinv Hss Hd; cbn [saga_from].
  - exact I.
  - pose proof (Forall_inv Hss) as [Hf Hc]. pose proof (Forall_inv_tail Hss) as Hr.
    pose proof (ginv_run gmax (gaps t) g Hinv) as Hg.
    destruct (step_out (snd (run_step (grun gmax g (gaps t)) (forward s)))) as [|rf|rf|l0] eqn:Eo.
    + apply IH; [apply run_step_ginv; exact Hg| exact Hr| constructor; [exact Hc| exact Hd]].
    + apply compensate_ok; [apply run_step_ginv; exact Hg| rewrite Eo; intros l E; discriminate E| exact Hd].
    + apply compensate_ok; [apply run_step_ginv; exact Hg| rewrite Eo; intros l E; discriminate E| exact Hd].
    + exfalso. exact (admitted_step_never_fails_deref _ _ l0 Hg Hf Eo).
Qed.

Theorem saga_never_fails_deref : forall g ss, GInv gmax g ->
  Forall (fun s => step_ok (forward s) /\ step_ok (compensation s)) ss ->
  saga_deref_free (snd (run_saga g ss)).
Proof. intros g ss H Hss. apply saga_from_ok; [exact H| exact Hss| constructor]. Qed.

End Saga.
End ActionScope.

(* ------------------------------------------------------------------ *)
(* Witnesses and falsifiers                                             *)
(* ------------------------------------------------------------------ *)

(* doc_build: A <-> B, A -> C, B -> C, in store 0. *)

(* The destroyer gets the refusal: deleting a held A fails, and A is still
   readable afterwards; deleting C, which is not held, succeeds. Both steps
   release their borrow. *)
Example destroyer_gets_the_refusal :
  let g := grun 3 gempty doc_build in
  let held_delete := mkStep [(linkA, false)] [BDo (ODelete linkA); BRead linkA] in
  let other_delete := mkStep [(linkA, false)] [BDo (ODelete linkC); BRead linkA] in
  snd (run_step 3 g held_delete) = mkStepReceipt (BodyRefused RBorrowed) 0 /\
  strip (resolve (fst (run_step 3 g held_delete)) linkA) = Some (10, [(1, 0); (2, 0)]) /\
  snd (run_step 3 g other_delete) = mkStepReceipt StepDone 2 /\
  gbor (fst (run_step 3 g held_delete)) = [] /\
  gbor (fst (run_step 3 g other_delete)) = [].
Proof. repeat split; reflexivity. Qed.

(* A failed acquisition releases the borrows already taken. *)
Example partial_acquisition_is_released :
  let g := grun 3 gempty (doc_build ++ [ODelete linkC]) in
  run_step 3 g (mkStep [(linkA, true); (linkC, false)] [BRead linkA]) =
    (g, mkStepReceipt (AcqFailed RStale) 0).
Proof. reflexivity. Qed.

(* Falsifier: holding A does not protect C, which A's edge names. Reading C
   without acquiring it is not admitted, and it fails once C is deleted. *)
Example neighbor_needs_its_own_acquisition :
  let st := mkStep [(linkA, false)] [BDo (ODelete linkC); BRead linkC] in
  ~ step_ok st /\ snd (run_step 3 (grun 3 gempty doc_build) st) =
    mkStepReceipt (DerefFailed linkC) 1.
Proof.
  split; [|reflexivity].
  unfold step_ok. intros H. apply Forall_inv_tail, Forall_inv in H. cbn in H.
  destruct H as [E|[]]. unfold linkA, linkC in E. discriminate E.
Qed.

(* Falsifier: a delete that ignores borrows makes a held link stale. *)
Definition g_delete_ignoring_holds (g : GS) (l : Link) : GS :=
  match find_store (lsid l) (gstores g) with
  | Some s =>
      match nth_error (sslots s) (lidx l) with
      | Some (mkSlot gen (Some nd)) =>
          if Nat.eqb gen (lgen l) then
            mkGS (set_store (mkStore (ssid s) (stab s) (upd (sslots s) (lidx l) (mkSlot (S gen) None)))
                            (gstores g))
                 (free (nblocks nd) (gheap g)) (gsid g) (gbor g) (gissued g)
          else g
      | _ => g
      end
  | None => g
  end.

Example unguarded_delete_breaks_hold :
  let g := grun 3 gempty (doc_build ++ [OBegin linkA false]) in
  holds g linkA = true /\
  fst (gexec 3 g (ODelete linkA)) = g /\
  holds (g_delete_ignoring_holds g linkA) linkA = true /\
  resolve (g_delete_ignoring_holds g linkA) linkA = Missing RStale.
Proof. repeat split; reflexivity. Qed.

(* A two-step saga: the first step rewrites A, and its compensation writes
   A back; the second step reads C. *)
Definition saga_write_a : SagaStep :=
  mkSagaStep (mkStep [(linkA, true)] [BDo (OWrite 0 20 [(1, 0); (2, 0)]); BRead linkA])
             (mkStep [(linkA, true)] [BDo (OWrite 0 10 [(1, 0); (2, 0)])]).
Definition saga_read_c : SagaStep :=
  mkSagaStep (mkStep [(linkC, false)] [BRead linkC]) (mkStep [] []).

Definition gaps_delete_c (t : nat) : list GOp :=
  match t with 1 => [ODelete linkC] | _ => [] end.
Definition gaps_delete_a_c (t : nat) : list GOp :=
  match t with 1 => [ODelete linkC; ODelete linkA] | _ => [] end.

Example saga_without_interference_completes :
  let r := run_saga 3 (fun _ => []) (grun 3 gempty doc_build) [saga_write_a; saga_read_c] in
  snd r = SagaDone /\ strip (resolve (fst r) linkA) = Some (20, [(1, 0); (2, 0)]) /\
  gbor (fst r) = [].
Proof. repeat split; reflexivity. Qed.

(* C is deleted between the steps by other code. The saga finds it when
   the second step starts, not inside a step, and compensates the first. *)
Example stale_at_step_start_compensates :
  let r := run_saga 3 gaps_delete_c (grun 3 gempty doc_build) [saga_write_a; saga_read_c] in
  snd r = SagaCompensationFinished 1 (mkStepReceipt (AcqFailed RStale) 0) /\
  strip (resolve (fst r) linkA) = Some (10, [(1, 0); (2, 0)]) /\
  gbor (fst r) = [].
Proof. repeat split; reflexivity. Qed.

(* Falsifier: A, which the compensation needs, is deleted between the
   steps as well; the compensation cannot start and the saga is stuck. *)
Example deleted_compensation_target_gets_stuck :
  snd (run_saga 3 gaps_delete_a_c (grun 3 gempty doc_build) [saga_write_a; saga_read_c]) =
    SagaStuck 1 0 (mkStepReceipt (AcqFailed RStale) 0) (mkStepReceipt (AcqFailed RStale) 0).
Proof. reflexivity. Qed.

(* A partial forward step is not on the completed-step compensation stack.
   A full-step compensation is not safe to call on an arbitrary prefix. *)
Definition partial_node : Link := mkLink 0 3 0.
Definition absent_node : Link := mkLink 0 99 0.
Definition partial_forward : Step := mkStep []
  [BDo (OInsert 0 3 99 [] [(100, 0)] (101, 0)); BDo (ODelete absent_node)].
Definition undo_partial_node : Step := mkStep [] [BDo (ODelete partial_node)].

Example partial_forward_effect_is_reported :
  let g := grun 3 gempty doc_build in
  let result := run_saga 3 (fun _ => []) g [mkSagaStep partial_forward undo_partial_node] in
  step_ok partial_forward /\ step_ok undo_partial_node /\
  snd result = SagaCompensationFinished 0 (mkStepReceipt (BodyRefused RStale) 1) /\
  strip (resolve (fst result) partial_node) = Some (99, []) /\
  gbor (fst result) = [] /\
  strip (resolve (fst (run_step 3 (fst result) undo_partial_node)) partial_node) = None.
Proof.
  split; [unfold step_ok, partial_forward; repeat constructor|].
  split; [unfold step_ok, undo_partial_node; repeat constructor|].
  repeat split; reflexivity.
Qed.

Definition start_failure : SagaStep :=
  mkSagaStep (mkStep [(absent_node, false)] []) (mkStep [] []).
Definition noop_compensated_write : SagaStep :=
  mkSagaStep (forward saga_write_a) (mkStep [] []).

Example noop_compensation_is_not_restore :
  let g := grun 3 gempty doc_build in
  let result := run_saga 3 (fun _ => []) g [noop_compensated_write; start_failure] in
  snd result = SagaCompensationFinished 1 (mkStepReceipt (AcqFailed RStale) 0) /\
  strip (resolve g linkA) = Some (10, [(1, 0); (2, 0)]) /\
  strip (resolve (fst result) linkA) = Some (20, [(1, 0); (2, 0)]).
Proof. repeat split; reflexivity. Qed.

Definition partly_compensated_write : SagaStep :=
  mkSagaStep (forward saga_write_a)
    (mkStep [(linkA, true)] [BDo (OWrite 0 15 []); BDo (ODelete absent_node)]).

Example partial_compensation_preserves_both_failures :
  let result := run_saga 3 (fun _ => []) (grun 3 gempty doc_build)
    [partly_compensated_write; start_failure] in
  snd result = SagaStuck 1 0 (mkStepReceipt (AcqFailed RStale) 0)
    (mkStepReceipt (BodyRefused RStale) 1) /\
  strip (resolve (fst result) linkA) = Some (15, []) /\ gbor (fst result) = [].
Proof. repeat split; reflexivity. Qed.

Example receipt_counts_successful_reads_and_writes :
  let g := grun 3 gempty doc_build in
  let st := mkStep [(linkA, true)]
    [BRead linkA; BDo (OWrite 0 15 []); BDo (ODelete absent_node); BRead linkA] in
  snd (run_step 3 g st) = mkStepReceipt (BodyRefused RStale) 2 /\
  strip (resolve (fst (run_step 3 g st)) linkA) = Some (15, []) /\
  snd (run_step 3 g (mkStep [(linkA, false)] [BRead linkA])) = mkStepReceipt StepDone 1.
Proof. repeat split; reflexivity. Qed.
