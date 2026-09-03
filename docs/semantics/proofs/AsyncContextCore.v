(*
  Pergyra async runtime-context carriage core.

  This bounded model corresponds to pgy_runtime_context_capture_task(), the
  six execution lanes, coroutine yield/await rebinding, and surrounding TLS
  restoration.  Runtime source remains the authority; this file proves the
  consequences of exact parent capture and never models executor-default reads
  as a legal task transition.

  Capability masks are kept opaque as natural numbers.  Exact preservation of
  both masks is stronger than any bit-level no-widening statement and avoids
  inventing a second capability algebra here.
*)

Require Import Coq.Arith.PeanoNat.

Section AsyncContextCore.

Record RuntimeContext : Type := mkRuntimeContext {
  manifest_mask : nat;
  environment_mask : nat;
  budget_owner : nat;
  instance_identity : nat
}.

Definition capture_task (parent : RuntimeContext) : RuntimeContext :=
  mkRuntimeContext
    (manifest_mask parent)
    (environment_mask parent)
    (budget_owner parent)
    (instance_identity parent).

Theorem capture_preserves_exact_authority : forall parent,
  manifest_mask (capture_task parent) = manifest_mask parent /\
  environment_mask (capture_task parent) = environment_mask parent.
Proof.
  intros [manifest environment budget instance]. simpl. auto.
Qed.

Theorem capture_preserves_exact_budget_owner : forall parent,
  budget_owner (capture_task parent) = budget_owner parent.
Proof.
  intros [manifest environment budget instance]. reflexivity.
Qed.

Theorem capture_preserves_instance_identity : forall parent,
  instance_identity (capture_task parent) = instance_identity parent.
Proof.
  intros [manifest environment budget instance]. reflexivity.
Qed.

Inductive ExecutionLane : Type :=
  | LaneInline
  | LanePinnedZone
  | LaneBlockingPool
  | LaneLocalAsync
  | LaneWorkerPool
  | LaneMovableScheduler.

(* A lane may choose where work runs; it may not rewrite task authority. *)
Inductive lane_resume
  : ExecutionLane -> RuntimeContext -> RuntimeContext -> Prop :=
  | ResumeCaptured : forall lane task,
      lane_resume lane task task.

Theorem lane_resume_preserves_context : forall lane before after,
  lane_resume lane before after -> before = after.
Proof.
  intros lane before after H. inversion H. reflexivity.
Qed.

Theorem lane_resume_cannot_widen_masks : forall lane before after,
  lane_resume lane before after ->
  manifest_mask after = manifest_mask before /\
  environment_mask after = environment_mask before.
Proof.
  intros lane before after H. inversion H; subst. auto.
Qed.

Inductive SuspensionBoundary : Type :=
  | YieldBoundary
  | AwaitBoundary.

(* Coroutine suspension restores scheduler TLS while parked, then rebinds this
   exact captured context on resume.  The task-visible before/after state is
   therefore identical. *)
Inductive suspension_resume
  : SuspensionBoundary -> RuntimeContext -> RuntimeContext -> Prop :=
  | ResumeSuspended : forall boundary task,
      suspension_resume boundary task task.

Theorem suspension_resume_preserves_authority_and_budget :
  forall boundary before after,
    suspension_resume boundary before after ->
    manifest_mask after = manifest_mask before /\
    environment_mask after = environment_mask before /\
    budget_owner after = budget_owner before /\
    instance_identity after = instance_identity before.
Proof.
  intros boundary before after H. inversion H; subst.
  repeat split; reflexivity.
Qed.

(* A completed task returns to the context that surrounded its execution. *)
Inductive task_boundary
  : RuntimeContext -> RuntimeContext -> RuntimeContext -> Prop :=
  | RunCapturedTask : forall surrounding task,
      task_boundary surrounding task surrounding.

Theorem task_return_restores_surrounding_context :
  forall surrounding task after,
    task_boundary surrounding task after -> after = surrounding.
Proof.
  intros surrounding task after H. inversion H. reflexivity.
Qed.

(* Load-bearing counterexample: substituting an executor's default context for
   parent capture can change both authority and quantitative identity. *)
Example executor_default_can_change_authority_identity :
  let parent := mkRuntimeContext 1 1 7 9 in
  let executor_default := mkRuntimeContext 3 3 11 0 in
  manifest_mask (capture_task parent) <>
    manifest_mask executor_default /\
  environment_mask (capture_task parent) <>
    environment_mask executor_default /\
  budget_owner (capture_task parent) <>
    budget_owner executor_default /\
  instance_identity (capture_task parent) <>
    instance_identity executor_default.
Proof.
  simpl. repeat split; discriminate.
Qed.

End AsyncContextCore.
