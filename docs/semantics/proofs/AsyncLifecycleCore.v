(*
  Pergyra structured async lifecycle core.

  This is a bounded model of the named Future/RemoteFuture lifecycle owned by
  src/semantic/type_checker_future_lifecycle.c.  It proves containment for the
  beta-stable named-spawn surface; it does not make `async` a lifetime owner and
  does not prove scheduler termination, fairness, detached-capture safety, or
  whole-language memory safety.

  Live implementation states:
    Absent / Live / Retired / Diverged

  Modeled transitions:
    spawn:    Absent -> Live
    suspend:  state preserving
    Cancel:   Live -> Live (request only)
    await:    Live -> Retired
    transfer: Live -> Retired (explicit own Future parameter)

  The trace theorem below is the structured-containment claim: a trace that
  starts Live and reaches an admissible scope exit contains await or transfer.
*)

Require Import Coq.Lists.List.
Import ListNotations.

Section AsyncLifecycleCore.

Inductive LifetimeState : Type :=
  | LAbsent
  | LLive
  | LRetired
  | LDiverged.

Inductive AsyncEvent : Type :=
  | EvSpawn
  | EvSuspend
  | EvCancel
  | EvAwait
  | EvTransfer.

Inductive lifetime_step : AsyncEvent -> LifetimeState -> LifetimeState -> Prop :=
  | StepSpawn : lifetime_step EvSpawn LAbsent LLive
  | StepSuspend : forall s, lifetime_step EvSuspend s s
  | StepCancel : lifetime_step EvCancel LLive LLive
  | StepAwait : lifetime_step EvAwait LLive LRetired
  | StepTransfer : lifetime_step EvTransfer LLive LRetired.

Definition scope_closed (s : LifetimeState) : Prop :=
  s = LAbsent \/ s = LRetired.

Theorem suspend_preserves_lifetime : forall before after,
  lifetime_step EvSuspend before after -> before = after.
Proof.
  intros before after H. inversion H. reflexivity.
Qed.

Theorem suspend_does_not_discharge_live : forall after,
  lifetime_step EvSuspend LLive after -> ~ scope_closed after.
Proof.
  intros after Hstep Hclosed.
  inversion Hstep; subst.
  destruct Hclosed as [H | H]; discriminate.
Qed.

Theorem cancel_is_request_only : forall before after,
  lifetime_step EvCancel before after ->
  before = LLive /\ after = LLive.
Proof.
  intros before after H. inversion H; subst. auto.
Qed.

Theorem cancel_cannot_close_scope : forall after,
  lifetime_step EvCancel LLive after -> ~ scope_closed after.
Proof.
  intros after Hstep Hclosed.
  apply cancel_is_request_only in Hstep.
  destruct Hstep as [_ Hafter]. subst after.
  destruct Hclosed as [H | H]; discriminate.
Qed.

Theorem await_consumes_exactly_one_live_handle : forall before after,
  lifetime_step EvAwait before after ->
  before = LLive /\ after = LRetired.
Proof.
  intros before after H. inversion H; subst. auto.
Qed.

Theorem own_transfer_consumes_exactly_one_live_handle : forall before after,
  lifetime_step EvTransfer before after ->
  before = LLive /\ after = LRetired.
Proof.
  intros before after H. inversion H; subst. auto.
Qed.

Theorem retirement_requires_await_or_transfer : forall event before after,
  lifetime_step event before after ->
  before = LLive ->
  after = LRetired ->
  event = EvAwait \/ event = EvTransfer.
Proof.
  intros event before after Hstep Hbefore Hafter.
  destruct Hstep.
  - discriminate Hbefore.
  - rewrite Hbefore in Hafter. discriminate Hafter.
  - discriminate Hafter.
  - left. reflexivity.
  - right. reflexivity.
Qed.

Theorem retired_handle_cannot_be_consumed_again :
  ~ exists after,
      lifetime_step EvAwait LRetired after \/
      lifetime_step EvTransfer LRetired after.
Proof.
  intros [after [H | H]]; inversion H.
Qed.

Inductive runs : list AsyncEvent -> LifetimeState -> LifetimeState -> Prop :=
  | RunsNil : forall s, runs [] s s
  | RunsCons : forall event events before middle after,
      lifetime_step event before middle ->
      runs events middle after ->
      runs (event :: events) before after.

Theorem live_trace_to_closed_scope_has_retirement : forall events start finish,
  runs events start finish ->
  start = LLive ->
  scope_closed finish ->
  In EvAwait events \/ In EvTransfer events.
Proof.
  intros events start finish Hrun.
  induction Hrun as
      [s | event events before middle after Hstep Htail IH].
  - intros Hlive Hclosed. subst s.
    destruct Hclosed as [H | H]; discriminate.
  - intros Hlive Hclosed.
    destruct Hstep.
    + discriminate Hlive.
    + specialize (IH Hlive Hclosed).
      destruct IH as [Hawait | Htransfer].
      * left. simpl. right. exact Hawait.
      * right. simpl. right. exact Htransfer.
    + specialize (IH Hlive Hclosed).
      destruct IH as [Hawait | Htransfer].
      * left. simpl. right. exact Hawait.
      * right. simpl. right. exact Htransfer.
    + left. simpl. left. reflexivity.
    + right. simpl. left. reflexivity.
Qed.

(* Alternative CFG paths must agree.  Any disagreement is Diverged and cannot
   satisfy scope_closed, matching merge_resource_states_or. *)
Definition alternative_merge
  (left right : LifetimeState) : LifetimeState :=
  match left, right with
  | LAbsent, LAbsent => LAbsent
  | LLive, LLive => LLive
  | LRetired, LRetired => LRetired
  | LDiverged, LDiverged => LDiverged
  | _, _ => LDiverged
  end.

Theorem alternative_merge_retired_iff : forall left right,
  alternative_merge left right = LRetired <->
  left = LRetired /\ right = LRetired.
Proof.
  destruct left, right; simpl; intuition congruence.
Qed.

Theorem alternative_path_disagreement_fails_closed : forall left right,
  left <> right -> ~ scope_closed (alternative_merge left right).
Proof.
  destruct left, right; simpl; intros Hneq Hclosed;
    try (apply Hneq; reflexivity);
    destruct Hclosed as [H | H]; discriminate.
Qed.

(* Parallel arms are simultaneous, not alternative paths.  A retirement in
   either non-diverged arm contributes to the post-join state. *)
Definition parallel_merge
  (left right : LifetimeState) : LifetimeState :=
  match left with
  | LDiverged => LDiverged
  | LRetired =>
      match right with
      | LDiverged => LDiverged
      | _ => LRetired
      end
  | LLive =>
      match right with
      | LDiverged => LDiverged
      | LRetired => LRetired
      | _ => LLive
      end
  | LAbsent =>
      match right with
      | LDiverged => LDiverged
      | LRetired => LRetired
      | LLive => LLive
      | LAbsent => LAbsent
      end
  end.

Theorem parallel_retirement_contributes : forall left right,
  left <> LDiverged ->
  right <> LDiverged ->
  (left = LRetired \/ right = LRetired) ->
  parallel_merge left right = LRetired.
Proof.
  destruct left, right; simpl; intuition congruence.
Qed.

Example alternative_and_parallel_merges_are_distinct :
  alternative_merge LLive LRetired = LDiverged /\
  parallel_merge LLive LRetired = LRetired.
Proof. split; reflexivity. Qed.

End AsyncLifecycleCore.
