(*
  Independent consumer of OwnershipGraphActionScope. It restates the claims
  the focused gate defends in explicit form, without the owner's summary
  predicates, and pins the admission and falsifier observations. Weakening an
  owner statement or emptying its admission breaks this file.
*)

Require Import Stdlib.Lists.List.
Require Import OwnershipCleanCore.
Require Import OwnershipGraphLinks.
Require Import OwnershipGraphActionScope.
Import ListNotations.

Lemma audit_admitted_step_never_fails_deref : forall gmax g st l,
  GInv gmax g -> step_ok st -> step_out (snd (run_step gmax g st)) <> DerefFailed l.
Proof. exact admitted_step_never_fails_deref. Qed.

Lemma audit_saga_never_fails_deref : forall gmax gaps g ss, GInv gmax g ->
  Forall (fun s => step_ok (forward s) /\ step_ok (compensation s)) ss ->
  (forall k w l, snd (run_saga gmax gaps g ss) = SagaCompensationFinished k w ->
    step_out w <> DerefFailed l) /\
  (forall k j w c l, snd (run_saga gmax gaps g ss) = SagaStuck k j w c ->
    step_out w <> DerefFailed l /\ step_out c <> DerefFailed l).
Proof.
  intros gmax gaps g ss H Hss. pose proof (saga_never_fails_deref gmax gaps g ss H Hss) as D.
  split; intros * E; rewrite E in D; [exact (D l)| split; [exact (proj1 D l)| exact (proj2 D l)]].
Qed.

Lemma audit_run_step_restores_borrows : forall gmax g st,
  GInv gmax g -> step_ok st -> gbor (fst (run_step gmax g st)) = gbor g.
Proof. exact run_step_restores_borrows. Qed.

(* State/result projection uses the imported graph machine, not only the
   action owner's BodyPrefixExec or body_stop predicates. *)
Lemma audit_body_receipt_projects : forall gmax bs g,
  exists prefix rest, bs = prefix ++ rest /\
    completed (snd (run_body gmax g bs)) = length prefix /\
    fst (run_body gmax g bs) = grun gmax g (body_machine_ops prefix) /\
    Forall (fun result => forall rf, result <> GRefused rf)
      (gresults gmax g (body_machine_ops prefix)) /\
    match step_out (snd (run_body gmax g bs)) with
    | StepDone => rest = []
    | AcqFailed _ => False
    | BodyRefused rf => exists o tail, rest = BDo o :: tail /\
        snd (gexec gmax (fst (run_body gmax g bs)) o) = GRefused rf
    | DerefFailed l => exists rf tail, rest = BRead l :: tail /\
        resolve (fst (run_body gmax g bs)) l = Missing rf
    end.
Proof. exact run_body_receipt_projects. Qed.

Lemma audit_receipt_extent : forall gmax g st,
  completed (snd (run_step gmax g st)) <= length (body st) /\
  (step_out (snd (run_step gmax g st)) = StepDone ->
    completed (snd (run_step gmax g st)) = length (body st)) /\
  (forall rf, step_out (snd (run_step gmax g st)) = AcqFailed rf ->
    completed (snd (run_step gmax g st)) = 0).
Proof.
  intros. split; [apply run_step_receipt_bound|]. split;
    [apply completed_step_accounts_for_whole_body| apply acquisition_failure_has_no_body_prefix].
Qed.

Lemma audit_spared_run_keeps_link : forall gmax os g l nd, GInv gmax g -> resolve g l = Found nd ->
  forallb (spares l) os = true ->
  exists nd', resolve (grun gmax g os) l = Found nd' /\ nblocks nd' = nblocks nd.
Proof.
  intros gmax os g l nd H Hf Hs.
  exact (spared_run_keeps_link gmax os g l (nblocks nd) H (ex_intro _ nd (conj Hf eq_refl)) Hs).
Qed.

Lemma audit_held_run_keeps_link : forall gmax os g l nd, GInv gmax g -> holds g l = true ->
  resolve g l = Found nd -> forallb ends_no_borrow os = true ->
  exists nd', resolve (grun gmax g os) l = Found nd' /\ nblocks nd' = nblocks nd.
Proof.
  intros gmax os g l nd H Hh Hf Hs.
  exact (proj1 (held_run_keeps_link gmax os g l (nblocks nd) H Hh (ex_intro _ nd (conj Hf eq_refl)) Hs)).
Qed.

(* The admission accepts a step that reads what it acquired and refuses one
   that reads anything else or ends a borrow inside its body, so the
   theorems above are not vacuous. Only a delete of the slot or a drop of the
   store counts as a destroyer. *)
Example audit_admission_and_destroyers :
  step_ok (mkStep [(linkA, false)] [BRead linkA; BDo (ODelete linkC)]) /\
  ~ step_ok (mkStep [(linkA, false)] [BRead linkC]) /\
  ~ step_ok (mkStep [(linkA, false)] [BDo (OEnd 0)]) /\
  spares linkA (ODelete linkA) = false /\ spares linkA (ODrop 0) = false /\
  spares linkA (ODelete linkC) = true /\ spares linkA (OWrite 0 1 []) = true /\
  holds (grun 3 gempty (doc_build ++ [OBegin linkA false])) linkA = true /\
  holds (grun 3 gempty doc_build) linkA = false.
Proof.
  split.
  - unfold step_ok. constructor; [cbn; left; reflexivity| constructor; [reflexivity| constructor]].
  - split.
    + unfold step_ok. intros H. apply Forall_inv in H. cbn in H.
      destruct H as [E|[]]. unfold linkA, linkC in E. discriminate E.
    + split; [unfold step_ok; intros H; apply Forall_inv in H; cbn in H; discriminate H|].
      repeat split; reflexivity.
Qed.

Example audit_holding_moves_the_failure :
  let g := grun 3 gempty doc_build in
  snd (run_step 3 g (mkStep [(linkA, false)] [BDo (ODelete linkA); BRead linkA])) =
    mkStepReceipt (BodyRefused RBorrowed) 0 /\
  snd (run_step 3 g (mkStep [(linkA, false)] [BDo (ODelete linkC); BRead linkC])) =
    mkStepReceipt (DerefFailed linkC) 1 /\
  resolve (g_delete_ignoring_holds (grun 3 g [OBegin linkA false]) linkA) linkA = Missing RStale.
Proof. repeat split; reflexivity. Qed.

(* These programs are admitted; residual effects are not a malformed-input
   loophole. The empty compensation really does finish without restoring A. *)
Example audit_effect_counterexamples_admitted :
  GInv 3 (grun 3 gempty doc_build) /\
  Forall (fun s => step_ok (forward s) /\ step_ok (compensation s))
    [mkSagaStep partial_forward undo_partial_node;
     noop_compensated_write; partly_compensated_write; start_failure].
Proof.
  split; [apply ginv_run; apply ginv_empty|].
  unfold partial_forward, undo_partial_node, noop_compensated_write,
    partly_compensated_write, start_failure, saga_write_a, step_ok;
    cbn [forward compensation body acq map fst]; repeat constructor.
Qed.

Example audit_partial_forward_not_rolled_back :
  let result := run_saga 3 (fun _ => []) (grun 3 gempty doc_build)
    [mkSagaStep partial_forward undo_partial_node] in
  snd result = SagaCompensationFinished 0 (mkStepReceipt (BodyRefused RStale) 1) /\
  strip (resolve (fst result) (mkLink 0 3 0)) = Some (99, []) /\ gbor (fst result) = [].
Proof. repeat split; reflexivity. Qed.

Example audit_noop_compensation_keeps_effect :
  let g := grun 3 gempty doc_build in
  let result := run_saga 3 (fun _ => []) g [noop_compensated_write; start_failure] in
  snd result = SagaCompensationFinished 1 (mkStepReceipt (AcqFailed RStale) 0) /\
  strip (resolve g linkA) = Some (10, [(1, 0); (2, 0)]) /\
  strip (resolve (fst result) linkA) = Some (20, [(1, 0); (2, 0)]).
Proof. repeat split; reflexivity. Qed.

Example audit_partial_compensation_keeps_original_failure :
  let result := run_saga 3 (fun _ => []) (grun 3 gempty doc_build)
    [partly_compensated_write; start_failure] in
  snd result = SagaStuck 1 0 (mkStepReceipt (AcqFailed RStale) 0)
    (mkStepReceipt (BodyRefused RStale) 1) /\
  strip (resolve (fst result) linkA) = Some (15, []) /\ gbor (fst result) = [].
Proof. repeat split; reflexivity. Qed.

Example audit_reads_are_not_mutation_counts :
  let g := grun 3 gempty doc_build in
  let st := mkStep [(linkA, false)] [BRead linkA; BRead linkA] in
  snd (run_step 3 g st) = mkStepReceipt StepDone 2 /\ fst (run_step 3 g st) = g.
Proof. repeat split; reflexivity. Qed.

Example audit_outer_hold_survives_partial_body :
  let g := grun 3 gempty (doc_build ++ [OBegin linkB false]) in
  let st := mkStep [(linkA, true)] [BDo (OWrite 0 15 []); BDo (ODelete absent_node)] in
  snd (run_step 3 g st) = mkStepReceipt (BodyRefused RStale) 1 /\
  gbor (fst (run_step 3 g st)) = gbor g /\ length (gbor g) = 1.
Proof. repeat split; reflexivity. Qed.

Example audit_store_identity_is_not_reused :
  let g := grun 3 gempty
    [ONew (0, 0); OInsert 0 0 7 [] [(1, 0)] (2, 0); ODrop 0;
     ONew (0, 0); OInsert 1 0 9 [] [(1, 0)] (2, 0)] in
  gsid g = 2 /\ resolve g (mkLink 0 0 0) = Missing RNoStore /\
  strip (resolve g (mkLink 1 0 0)) = Some (9, []).
Proof. repeat split; reflexivity. Qed.

Example audit_saga_outcomes :
  let g := grun 3 gempty doc_build in
  snd (run_saga 3 (fun _ => []) g [saga_write_a; saga_read_c]) = SagaDone /\
  snd (run_saga 3 gaps_delete_c g [saga_write_a; saga_read_c]) =
    SagaCompensationFinished 1 (mkStepReceipt (AcqFailed RStale) 0) /\
  strip (resolve (fst (run_saga 3 gaps_delete_c g [saga_write_a; saga_read_c])) linkA) =
    Some (10, [(1, 0); (2, 0)]) /\
  snd (run_saga 3 gaps_delete_a_c g [saga_write_a; saga_read_c]) =
    SagaStuck 1 0 (mkStepReceipt (AcqFailed RStale) 0) (mkStepReceipt (AcqFailed RStale) 0).
Proof. repeat split; reflexivity. Qed.
