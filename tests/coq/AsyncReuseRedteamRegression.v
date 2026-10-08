(* Permanent consumer of the existing async owner APIs.  The focused gate
   snapshots both these consumers and their owners before fresh compilation. *)
Require Import Stdlib.Lists.List Stdlib.Arith.PeanoNat Stdlib.micromega.Lia.
Import ListNotations.
Require Import AsyncScopeCore AsyncLifecycleCore ParallelSchedulingCore.
Require Import WitnessDataRace CoordinationCore AsyncContextCore CompensationCore.

Module Sc := AsyncScopeCore.
Module Life := AsyncLifecycleCore.
Module Sched := ParallelSchedulingCore.
Module Race := WitnessDataRace.
Module Coord := CoordinationCore.
Module Ctx := AsyncContextCore.
Module Comp := CompensationCore.

Definition scope_cancelled := Sc.mkConfig [(7, 1, Sc.Cancelled)]
  [1; 0] [(1, 0)] false.
Definition scope_completed := Sc.mkConfig [(7, 1, Sc.Done)]
  [1; 0] [(1, 0)] false.
Definition scope_closed_after_drain := Sc.mkConfig [(7, 1, Sc.Done)]
  [0] [] false.

(* A-F1: this is an actual reachable request, not an invented bad initial row. *)
Example cancellation_is_reachable_but_not_drained :
  Sc.steps Sc.empty_world scope_cancelled /\
  Sc.task_pending scope_cancelled 7 1 /\
  ~ (forall t, ~ Sc.task_pending scope_cancelled t 1).
Proof.
  split; [| split].
  - eapply Sc.steps_trans.
    { apply (Sc.step_open Sc.empty_world 1 0).
      - unfold Sc.scope_open, Sc.empty_world, Sc.root_scope. simpl.
        intros [H | H]; [discriminate | contradiction].
      - discriminate.
      - unfold Sc.scope_open. simpl. left. reflexivity. }
    eapply Sc.steps_trans.
    { apply (Sc.step_spawn _ 7 1).
      - unfold Sc.scope_open. simpl. left. reflexivity.
      - discriminate.
      - unfold Sc.task_fresh. simpl. intros s st Hin. contradiction. }
    eapply Sc.steps_trans.
    { apply (Sc.step_cancel _ 1 [1]).
      - unfold Sc.scope_open. simpl. left. reflexivity.
      - intro c. simpl. split.
        + intros [Heq | Hin]; [subst; apply Sc.desc_refl | contradiction].
        + intros Hd. inversion Hd; subst.
          * left. reflexivity.
          * simpl in H. destruct H as [Heq | Hin]; [| contradiction].
            inversion Heq; subst. left. reflexivity. }
    apply Sc.steps_refl.
  - exists Sc.Cancelled. split; [simpl; left; reflexivity | discriminate].
  - eapply Sc.cancellation_does_not_admit_close. simpl. left. reflexivity.
Qed.

Example cancellation_completion_close_executes :
  Sc.steps scope_cancelled scope_closed_after_drain.
Proof.
  eapply Sc.steps_trans with (g' := scope_completed).
  - apply (Sc.step_complete scope_cancelled 7). exists 1, Sc.Cancelled.
    split; [simpl; left; reflexivity | discriminate].
  - eapply Sc.steps_trans.
    + apply (Sc.step_close scope_completed 1).
      * discriminate.
      * unfold Sc.scope_open. simpl. left. reflexivity.
      * unfold Sc.task_pending. simpl. intros t [st [[Heq | Hin] Hp]].
        -- inversion Heq; subst. contradiction.
        -- contradiction.
      * simpl. intros c [Heq | Hin]; [inversion Heq | contradiction].
    + apply Sc.steps_refl.
Qed.

(* Completed identity remains retired even when the numeric scope is reopened. *)
Example completed_task_is_not_fresh :
  ~ Sc.task_fresh scope_closed_after_drain 7.
Proof. eapply Sc.completed_task_identity_cannot_be_reused. simpl. left. reflexivity. Qed.

Definition scope_reopened := Sc.mkConfig [] [2; 1; 0]
  [(2, 0); (1, 0)] false.

(* A-F2: close/reopen uses the SAME numeric id with a different parent. *)
Example scope_reuse_executes_without_old_ancestry :
  Sc.steps Sc.empty_world scope_reopened /\
  ~ In (2, 1) (Sc.sparent scope_reopened).
Proof.
  split.
  - eapply Sc.steps_trans.
    { apply (Sc.step_open Sc.empty_world 1 0).
      - unfold Sc.scope_open, Sc.empty_world, Sc.root_scope. simpl.
        intros [H | H]; [discriminate | contradiction].
      - discriminate.
      - unfold Sc.scope_open. simpl. left. reflexivity. }
    eapply Sc.steps_trans.
    { apply (Sc.step_open _ 2 1).
      - unfold Sc.scope_open. simpl. intros [H | [H | H]];
          try discriminate; contradiction.
      - discriminate.
      - unfold Sc.scope_open. simpl. left. reflexivity. }
    eapply Sc.steps_trans.
    { apply (Sc.step_close _ 2).
      - discriminate.
      - unfold Sc.scope_open. simpl. left. reflexivity.
      - unfold Sc.task_pending. simpl. intros t [st [Hin _]]. contradiction.
      - simpl. intros c [H | [H | H]];
          try (inversion H); contradiction. }
    eapply Sc.steps_trans.
    { apply (Sc.step_open _ 2 0).
      - unfold Sc.scope_open, Sc.remove_scope, Sc.root_scope. simpl.
        intros [H | [H | H]]; try discriminate; contradiction.
      - discriminate.
      - unfold Sc.scope_open, Sc.remove_scope, Sc.root_scope. simpl.
        right. left. reflexivity. }
    apply Sc.steps_refl.
  - simpl. intros [H | [H | H]]; try discriminate; contradiction.
Qed.

Example reused_scope_is_not_an_old_descendant :
  ~ Sc.desc scope_reopened 2 1.
Proof.
  intro Hd. inversion Hd; subst; simpl in H.
  destruct H as [Heq | [Heq | Hin]]; [| discriminate | contradiction].
  inversion Heq; subst. inversion H0; subst; simpl in H.
  destruct H as [HrootEdge | [HrootEdge | HrootAbsent]];
    try discriminate; contradiction.
Qed.

(* A-F3: two actual simultaneous awaits do not discharge one handle. *)
Example parallel_double_await_is_rejected :
  Life.runs [Life.EvAwait] Life.LLive Life.LRetired /\
  ~ Life.scope_closed (Life.parallel_merge Life.LLive Life.LRetired Life.LRetired) /\
  Life.parallel_merge Life.LLive Life.LLive Life.LRetired = Life.LRetired.
Proof.
  split; [| split].
  - eapply Life.RunsCons; [apply Life.StepAwait | apply Life.RunsNil].
  - apply Life.parallel_double_retirement_fails_closed.
  - reflexivity.
Qed.

Example already_retired_input_does_not_forge_a_conflict :
  Life.parallel_merge Life.LRetired Life.LRetired Life.LRetired = Life.LRetired.
Proof. reflexivity. Qed.

Definition stamp (t : Sched.Task) := t.
Definition any_await (_ _ : Sched.Task) : Prop := True.

(* A-F4: a genuine issued cyclic wait remains a deadlock, even below cap. *)
Example compensation_does_not_break_dependency_cycles :
  Sched.steps stamp any_await 2 Sched.PolCompensate
    (Sched.mkCfg [0; 1] [Sched.WRun []; Sched.WRun []] [])
    (Sched.mkCfg [] [Sched.WPark [0] 1; Sched.WPark [1] 0] []) /\
  Sched.stuck stamp any_await 2 Sched.PolCompensate
    (Sched.mkCfg [] [Sched.WPark [0] 1; Sched.WPark [1] 0] []).
Proof.
  destruct (Sched.cyclic_await_deadlocks_under_compensation
    stamp any_await 2 0 1 I I) as [Hrun [Hstuck _]]. auto.
Qed.

Definition at_spare_cap := Sched.mkCfg [100]
  [Sched.WPark [0] 100; Sched.WPark [1] 100; Sched.WPark [2] 100;
   Sched.WPark [3] 100; Sched.WPark [4] 100] [].

(* A-F5: cap exhaustion is explicit; an unconditional rescue claim is false. *)
Example spare_cap_stops_growth :
  Sched.stuck stamp any_await 1 Sched.PolCompensate at_spare_cap.
Proof.
  apply Sched.compensation_at_capacity_is_stuck.
  - reflexivity.
  - intros w [Heq | [Heq | [Heq | [Heq | [Heq | Hin]]]]];
      try contradiction; subst w; eauto.
Qed.

Example spare_rescues_queued_work_below_cap :
  Sched.step stamp any_await 1 Sched.PolCompensate
    (Sched.mkCfg [1] [Sched.WPark [0] 1] [])
    (Sched.mkCfg [1] [Sched.WPark [0] 1; Sched.WRun []] []).
Proof.
  apply Sched.StSpare.
  - reflexivity.
  - discriminate.
  - intros w [Heq | Hin]; [subst w; eauto | contradiction].
  - simpl. unfold Sched.worker_limit. lia.
Qed.

Example six_workers_cannot_be_reached_from_the_cap : forall after,
  Sched.steps stamp any_await 1 Sched.PolCompensate at_spare_cap after ->
  length (Sched.cworkers after) <= 5.
Proof.
  intros after Hrun. apply (Sched.run_preserves_worker_bound
    stamp any_await 1 Sched.PolCompensate at_spare_cap after).
  - simpl. unfold Sched.worker_limit. lia.
  - exact Hrun.
Qed.

(* A-F6: every reached parked target was issued; the old two-step ghost wait
   cannot reappear. Task 1 really is absent from the singleton running state. *)
Example no_nonexistent_await_target : forall after,
  Sched.steps stamp any_await 1 Sched.PolHelpFirst
    (Sched.mkCfg [0] [Sched.WRun []] []) after ->
  forall st tg, In (Sched.WPark st tg) (Sched.cworkers after) ->
    Sched.task_present after tg.
Proof.
  intros after Hrun. change (Sched.await_targets_present after).
  apply (Sched.run_preserves_await_targets stamp any_await 1 Sched.PolHelpFirst
    (Sched.mkCfg [0] [Sched.WRun []] []) after).
  - intros st tg [Heq | Hin]; [discriminate | contradiction].
  - exact Hrun.
Qed.

Example ghost_target_is_absent :
  ~ Sched.task_present (Sched.mkCfg [] [Sched.WRun [0]] []) 1.
Proof. unfold Sched.task_present, Sched.listed_tasks; simpl. intros [H | H]; [discriminate | contradiction]. Qed.

Example admitted_join_run_keeps_progress : forall after,
  Sched.steps stamp (fun a b => a < b) 1 Sched.PolHelpFirst
    (Sched.mkCfg [0] [Sched.WRun []] []) after ->
  Sched.cworkers after <> [] -> ~ Sched.final after ->
  exists next, Sched.step stamp (fun a b => a < b) 1
    Sched.PolHelpFirst after next.
Proof.
  intros after Hrun Hworkers Hnonfinal.
  apply (Sched.help_first_progress_from_run stamp (fun a b => a < b) 1
    (Sched.mkCfg [0] [Sched.WRun []] []) after).
  - repeat split.
    + intros w [Heq | Hin]; [subst w; apply Sched.ds_nil | contradiction].
    + intros st tg [Heq | Hin]; [discriminate | contradiction].
    + intros st tg [Heq | Hin]; [discriminate | contradiction].
    + intros _. exists []. simpl. left. reflexivity.
  - exact Hrun.
  - exact Hworkers.
  - intros a b Hnewer. exact Hnewer.
  - exact Hnonfinal.
Qed.

(* A-F7: releasing reader 1 never erases reader 2's live right. *)
Definition readers_release_one : list Race.Op :=
  [Race.OpAcqR 1 0; Race.OpAcqR 2 0; Race.OpRel 1 0].

Example foreign_reader_blocks_write_after_own_release :
  Race.run_prog [] readers_release_one = [(2, 0, Race.Rd)] /\
  ~ Race.op_guard (Race.run_prog [] readers_release_one) (Race.OpAcqW 3 0).
Proof.
  split; [reflexivity |]. intros Hfree. apply (Hfree 2 Race.Rd).
  unfold Race.holds. simpl. left. reflexivity.
Qed.

Example foreign_context_cannot_release :
  ~ Race.op_guard [(1, 0, Race.Rd); (2, 0, Race.Rd)] (Race.OpRel 3 0).
Proof.
  intros [m Hin]. unfold Race.holds in Hin. simpl in Hin.
  destruct Hin as [Heq | [Heq | Hin]]; try discriminate; contradiction.
Qed.

Example own_releases_eventually_allow_an_exclusive_writer :
  Race.well_typed []
    (readers_release_one ++ [Race.OpRel 2 0; Race.OpAcqW 3 0]) /\
  Race.run_prog []
    (readers_release_one ++ [Race.OpRel 2 0; Race.OpAcqW 3 0]) = [(3, 0, Race.Wr)].
Proof.
  split; [| reflexivity]. unfold readers_release_one; simpl.
  apply Race.wt_cons.
  - intros c Hw. unfold Race.writes, Race.holds in Hw. contradiction.
  - apply Race.wt_cons.
    + intros c Hw. unfold Race.writes, Race.holds in Hw. simpl in Hw.
      destruct Hw as [Heq | Hin]; [discriminate | contradiction].
    + apply Race.wt_cons.
      * exists Race.Rd. unfold Race.holds. simpl. right. left. reflexivity.
      * apply Race.wt_cons.
        -- exists Race.Rd. unfold Race.holds. simpl. left. reflexivity.
        -- apply Race.wt_cons.
           ++ intros c m Hin. unfold Race.holds in Hin. contradiction.
           ++ apply Race.wt_nil.
Qed.

(* A-F8: this vocabulary equivalence is an interface condition, not a proof
   about SlotCalculus token admission. There is intentionally no step_move. *)
Fail Check Race.step_move.

(* A-F10: no dependency-closed schedule can re-run a completed step. *)
Definition no_deps : Coord.deps := fun _ => [].

Example completed_step_cannot_run_again :
  ~ Coord.cruns no_deps [] [0; 0].
Proof.
  intro Hrun. pose proof (Coord.reachable_done_once no_deps _ Hrun) as Hnd.
  inversion Hnd; subst. apply H1. simpl. left. reflexivity.
Qed.

Example independent_steps_can_complete :
  Coord.cruns no_deps [] [1; 0].
Proof.
  eapply Coord.CStep.
  - apply (Coord.Run no_deps [] 0).
    + intros x Hin. contradiction.
    + simpl. tauto.
  - eapply Coord.CStep.
    + apply (Coord.Run no_deps [0] 1).
      * intros x Hin. contradiction.
      * simpl. intros [H | H]; [discriminate | contradiction].
    + apply Coord.CRefl.
Qed.

(* A-F9: identity copy is explicitly an interface specification. *)
Example capture_is_an_interface_copy : forall c, Ctx.capture_task c = c.
Proof. intros [m e b i]. reflexivity. Qed.
Check Ctx.executor_default_can_change_authority_identity.

(* A-F11: a user compensator need not satisfy the snapshot interface. *)
Definition snapshot_targets : Comp.comp_target := fun _ => [0].
Definition snapshot_before := Comp.mkConfig (fun _ => Comp.Empty) [].
Definition snapshot_forward := Comp.mkConfig
  (Comp.fill_targets (Comp.st snapshot_before) [0])
  [Comp.mkLog 0 (Comp.st snapshot_before)].

Example user_noop_compensation_is_not_a_snapshot_rollback :
  Comp.cstep snapshot_targets (Comp.Forward 0) snapshot_before snapshot_forward /\
  ~ Comp.cstep snapshot_targets Comp.Rollback snapshot_forward snapshot_forward.
Proof.
  split.
  - apply Comp.CDo. constructor; [reflexivity | constructor].
  - intro Hrollback.
    pose proof (Comp.rollback_restores_snapshot snapshot_targets snapshot_forward
      snapshot_forward 0 (Comp.st snapshot_before) [] 0 Hrollback eq_refl
      (or_introl eq_refl)) as Hrestored. discriminate Hrestored.
Qed.
