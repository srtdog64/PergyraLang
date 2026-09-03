(*
  AsyncScopeCore.v  --  structured task containment: a running task always
  has a live owning scope, so no task is orphaned.

  Companion to docs/204 §3.1 (structured spawn scope) and docs/113 §Future
  Await Contract. That contract now reads: a named Future must be retired
  (await or explicit own transfer) on every normal path before its scope
  exits, and `parallel` retains its own block join. AsyncLifecycleCore.v
  proves that rule for the checker's affine Future flow, one handle at a
  time. This file is the SCOPE TREE above that: many tasks under one scope,
  nested scopes, cancellation of a subtree, and detach as a capability.

  The model. A configuration holds task rows (task, owning scope, state), the
  list of OPEN scopes, the scope tree as (child, parent) edges, and whether
  the detach capability is held. The root scope is always open: it is the
  world's background, the only place a detached task may live.

    open s        open a fresh non-root scope under an open parent
    spawn t s     create a running task inside an OPEN non-root scope
    complete t    a task finishes
    cancel s      every running task in s or a descendant scope is cancelled
    close s       leave a scope: no task of s may still be running and no
                  child scope may still be open (join-before-continuation)
    detach t      move a running task to the root scope -- needs the cap

  The invariant [contained] is the no-orphan property: every running task's
  owning scope is open. Every structured step preserves it, so a structured
  run never reaches an orphan (Theorem [run_no_orphan]). Closing a scope
  therefore means everything inside it has stopped (Theorem
  [no_running_task_in_closed_scope]); cancellation reaches every descendant
  scope (Theorem [cancel_reaches_descendants]); and without the detach
  capability nothing ever reaches the background (Theorem
  [background_only_via_detach]) -- docs/204 §2.5's "detach is a capability".

  Refutation. [unstructured_step] is the rule the language had before the
  structured spawn lifecycle landed (commit cf66092b): a scope could be left
  with a task still running. [orphan_reachable_unstructured] builds that
  orphan in three steps. It is kept so the guard's necessity stays a theorem
  rather than a memory, and because the affine-flow rule is bounded to named
  handles: anonymous capture-bearing blocks and detach are outside it, and
  this file is where those must land.

  Negative scope. This is a scope/lifecycle model: no memory, no data races
  (WitnessDataRace.v), no scheduler progress (ParallelSchedulingCore.v), and
  no claim that the compiler enforces these guards for anything beyond the
  named-Future flow -- the scope-tree enforcement is the rung docs/204 §4
  item 2 asks for. Cancellation here is a state change, not preemption
  (docs/114 §5: cancellation is cooperative).
*)

Require Import Coq.Lists.List.
Require Import Coq.Arith.PeanoNat.
Require Import Coq.Bool.Bool.
Import ListNotations.

Definition Task  := nat.
Definition Scope := nat.

Inductive TaskState := Running | Done | Cancelled.

Definition TaskRow := (Task * Scope * TaskState)%type.

Record Config := mkConfig {
  tasks      : list TaskRow;
  open       : list Scope;
  sparent    : list (Scope * Scope);
  detach_cap : bool
}.

Definition root_scope : Scope := 0.

Definition task_running (g : Config) (t : Task) (s : Scope) : Prop :=
  In (t, s, Running) (tasks g).

Definition scope_open (g : Config) (s : Scope) : Prop := In s (open g).

(* ===================================================================== *)
(* 1. The invariant and what it rules out                                 *)
(* ===================================================================== *)

(* Containment: every running task's owning scope is open. *)
Definition contained (g : Config) : Prop :=
  forall t s, task_running g t s -> scope_open g s.

(* An orphan: a running task whose scope has already been left. *)
Definition orphan (g : Config) : Prop :=
  exists t s, task_running g t s /\ ~ scope_open g s.

Theorem contained_no_orphan : forall g, contained g -> ~ orphan g.
Proof.
  intros g Hc [t [s [Hrun Hclosed]]]. apply Hclosed. exact (Hc t s Hrun).
Qed.

(* Once a scope is closed nothing inside it runs: the generalisation of
   `parallel`'s join-before-continuation to every scope. *)
Theorem no_running_task_in_closed_scope : forall g s,
  contained g -> ~ scope_open g s -> forall t, ~ task_running g t s.
Proof.
  intros g s Hc Hclosed t Hrun. apply Hclosed. exact (Hc t s Hrun).
Qed.

(* ===================================================================== *)
(* 2. Scope tree and the structured steps                                 *)
(* ===================================================================== *)

(* [desc g c s]: scope c is s itself or a descendant of s. *)
Inductive desc (g : Config) : Scope -> Scope -> Prop :=
  | desc_refl : forall s, desc g s s
  | desc_step : forall c p s, In (c, p) (sparent g) -> desc g p s -> desc g c s.

Definition set_state (t : Task) (st : TaskState) (row : TaskRow) : TaskRow :=
  match row with
  | (t', s, _) => if Nat.eqb t' t then (t', s, st) else row
  end.

Definition cancel_row (cancelled : Scope -> bool) (row : TaskRow) : TaskRow :=
  match row with
  | (t, s, Running) => if cancelled s then (t, s, Cancelled) else row
  | _ => row
  end.

Definition move_to_root (t : Task) (row : TaskRow) : TaskRow :=
  match row with
  | (t', _, st) => if Nat.eqb t' t then (t', root_scope, st) else row
  end.

Definition remove_scope (s : Scope) (l : list Scope) : list Scope :=
  filter (fun x => negb (Nat.eqb x s)) l.

Definition in_list (cs : list Scope) (c : Scope) : bool :=
  existsb (Nat.eqb c) cs.

Inductive step : Config -> Config -> Prop :=
  | step_open : forall g s p,
      ~ scope_open g s -> s <> root_scope -> scope_open g p ->
      step g (mkConfig (tasks g) (s :: open g) ((s, p) :: sparent g) (detach_cap g))
  | step_spawn : forall g t s,
      scope_open g s -> s <> root_scope ->
      step g (mkConfig ((t, s, Running) :: tasks g) (open g) (sparent g) (detach_cap g))
  | step_complete : forall g t,
      step g (mkConfig (map (set_state t Done) (tasks g)) (open g) (sparent g) (detach_cap g))
  | step_cancel : forall g s (cs : list Scope),
      (forall c, In c cs <-> desc g c s) ->
      step g (mkConfig (map (cancel_row (in_list cs)) (tasks g)) (open g) (sparent g) (detach_cap g))
  | step_close : forall g s,
      s <> root_scope ->
      (forall t, ~ task_running g t s) ->
      (forall c, In (c, s) (sparent g) -> ~ scope_open g c) ->
      step g (mkConfig (tasks g) (remove_scope s (open g)) (sparent g) (detach_cap g))
  | step_detach : forall g t,
      detach_cap g = true ->
      step g (mkConfig (map (move_to_root t) (tasks g)) (open g) (sparent g) (detach_cap g)).

Inductive steps : Config -> Config -> Prop :=
  | steps_refl  : forall g, steps g g
  | steps_trans : forall g g' g'', step g g' -> steps g' g'' -> steps g g''.

(* ===================================================================== *)
(* 3. Preservation                                                        *)
(* ===================================================================== *)

(* The root scope is always open; the well-formedness the runtime keeps. *)
Definition root_open (g : Config) : Prop := scope_open g root_scope.

Lemma in_remove_scope : forall s l x,
  In x (remove_scope s l) <-> (In x l /\ x <> s).
Proof.
  intros s l x. unfold remove_scope. rewrite filter_In. split.
  - intros [Hin Hb]. split. exact Hin.
    intro Heq. subst x. rewrite Nat.eqb_refl in Hb. discriminate.
  - intros [Hin Hne]. split. exact Hin.
    apply negb_true_iff. apply Nat.eqb_neq. exact Hne.
Qed.

Lemma running_after_set_state : forall g t st t' s,
  In (t', s, Running) (map (set_state t st) (tasks g)) ->
  In (t', s, Running) (tasks g) \/ (st = Running /\ In (t', s, Running) (tasks g))
  \/ (st = Running /\ exists st0, In (t', s, st0) (tasks g)).
Proof.
  intros g t st t' s Hin. apply in_map_iff in Hin.
  destruct Hin as [[[t0 s0] st0] [Heq Hin0]]. unfold set_state in Heq. simpl in Heq.
  destruct (Nat.eqb t0 t) eqn:E.
  - inversion Heq; subst. right. right. split. reflexivity. exists st0. exact Hin0.
  - inversion Heq; subst. left. exact Hin0.
Qed.

Lemma running_after_complete : forall g t t' s,
  In (t', s, Running) (map (set_state t Done) (tasks g)) ->
  In (t', s, Running) (tasks g).
Proof.
  intros g t t' s Hin.
  destruct (running_after_set_state g t Done t' s Hin)
    as [H | [[Hd _] | [Hd _]]]; [exact H | discriminate Hd | discriminate Hd].
Qed.

Lemma running_after_cancel : forall g f t s,
  In (t, s, Running) (map (cancel_row f) (tasks g)) ->
  In (t, s, Running) (tasks g) /\ f s = false.
Proof.
  intros g f t s Hin. apply in_map_iff in Hin.
  destruct Hin as [[[t0 s0] st0] [Heq Hin0]]. unfold cancel_row in Heq.
  destruct st0; simpl in Heq.
  - destruct (f s0) eqn:E.
    + inversion Heq.
    + inversion Heq; subst. split. exact Hin0. exact E.
  - inversion Heq.
  - inversion Heq.
Qed.

Lemma running_after_detach : forall g t t' s,
  In (t', s, Running) (map (move_to_root t) (tasks g)) ->
  s = root_scope \/ In (t', s, Running) (tasks g).
Proof.
  intros g t t' s Hin. apply in_map_iff in Hin.
  destruct Hin as [[[t0 s0] st0] [Heq Hin0]]. unfold move_to_root in Heq. simpl in Heq.
  destruct (Nat.eqb t0 t) eqn:E.
  - inversion Heq; subst. left. reflexivity.
  - inversion Heq; subst. right. exact Hin0.
Qed.

Theorem step_preserves_root_open : forall g g',
  root_open g -> step g g' -> root_open g'.
Proof.
  intros g g' Hr Hs. unfold root_open, scope_open in *. destruct Hs; simpl.
  - right. exact Hr.
  - exact Hr.
  - exact Hr.
  - exact Hr.
  - apply in_remove_scope. split. exact Hr. intro Heq. apply H. symmetry. exact Heq.
  - exact Hr.
Qed.

Theorem step_preserves_contained : forall g g',
  root_open g -> contained g -> step g g' -> contained g'.
Proof.
  intros g g' Hr Hc Hs. destruct Hs; unfold contained, task_running, scope_open in *; simpl.
  - (* open: tasks unchanged, open grew *)
    intros t s0 Hin. right. exact (Hc t s0 Hin).
  - (* spawn: the new task sits in an open scope *)
    intros t0 s0 [Heq | Hin].
    + inversion Heq; subst. exact H.
    + exact (Hc t0 s0 Hin).
  - (* complete: a Done row is not running *)
    intros t' s0 Hin. apply running_after_complete in Hin. exact (Hc t' s0 Hin).
  - (* cancel: surviving running rows were running before *)
    intros t s0 Hin. apply running_after_cancel in Hin. destruct Hin as [Hin _].
    exact (Hc t s0 Hin).
  - (* close: the guard says nothing in s was running *)
    intros t s0 Hin. apply in_remove_scope. split.
    + exact (Hc t s0 Hin).
    + intro Heq. subst s0. exact (H0 t Hin).
  - (* detach: the moved task lands in the always-open root *)
    intros t' s0 Hin. apply running_after_detach in Hin. destruct Hin as [Heq | Hin].
    + subst s0. exact Hr.
    + exact (Hc t' s0 Hin).
Qed.

Definition wf (g : Config) : Prop := root_open g /\ contained g.

Theorem step_preserves_wf : forall g g', wf g -> step g g' -> wf g'.
Proof.
  intros g g' [Hr Hc] Hs. split.
  - exact (step_preserves_root_open g g' Hr Hs).
  - exact (step_preserves_contained g g' Hr Hc Hs).
Qed.

(* MAIN: a structured run never reaches an orphan. *)
Theorem run_no_orphan : forall g g', wf g -> steps g g' -> ~ orphan g'.
Proof.
  intros g g' Hwf Hrun. apply contained_no_orphan.
  induction Hrun as [g | g g' g'' Hs Hrun IH].
  - exact (proj2 Hwf).
  - apply IH. exact (step_preserves_wf g g' Hwf Hs).
Qed.

(* ===================================================================== *)
(* 4. Cancellation reaches every descendant scope                         *)
(* ===================================================================== *)

Lemma in_list_true : forall cs c, In c cs -> in_list cs c = true.
Proof.
  intros cs c Hin. unfold in_list. apply existsb_exists.
  exists c. split. exact Hin. apply Nat.eqb_refl.
Qed.

Theorem cancel_reaches_descendants : forall g s cs,
  (forall c, In c cs <-> desc g c s) ->
  forall c, desc g c s ->
  forall t, ~ task_running
    (mkConfig (map (cancel_row (in_list cs)) (tasks g)) (open g) (sparent g) (detach_cap g))
    t c.
Proof.
  intros g s cs Hcs c Hdesc t Hrun.
  unfold task_running in Hrun. simpl in Hrun.
  apply running_after_cancel in Hrun. destruct Hrun as [_ Hfalse].
  rewrite (in_list_true cs c (proj2 (Hcs c) Hdesc)) in Hfalse. discriminate.
Qed.

(* ===================================================================== *)
(* 5. The background is reachable only through the detach capability     *)
(* ===================================================================== *)

Definition no_background_task (g : Config) : Prop :=
  forall t st, ~ In (t, root_scope, st) (tasks g).

Lemma step_keeps_cap : forall g g', step g g' -> detach_cap g' = detach_cap g.
Proof. intros g g' Hs. destruct Hs; reflexivity. Qed.

Lemma row_after_set_state : forall g t st t' s st',
  In (t', s, st') (map (set_state t st) (tasks g)) ->
  exists st0, In (t', s, st0) (tasks g).
Proof.
  intros g t st t' s st' Hin. apply in_map_iff in Hin.
  destruct Hin as [[[t0 s0] st0] [Heq Hin0]]. unfold set_state in Heq. simpl in Heq.
  destruct (Nat.eqb t0 t); inversion Heq; subst; eexists; exact Hin0.
Qed.

Lemma row_after_cancel : forall g f t s st,
  In (t, s, st) (map (cancel_row f) (tasks g)) ->
  exists st0, In (t, s, st0) (tasks g).
Proof.
  intros g f t s st Hin. apply in_map_iff in Hin.
  destruct Hin as [[[t0 s0] st0] [Heq Hin0]]. unfold cancel_row in Heq. simpl in Heq.
  destruct st0; try (destruct (f s0)); inversion Heq; subst; eexists; exact Hin0.
Qed.

Theorem step_preserves_no_background : forall g g',
  detach_cap g = false -> no_background_task g -> step g g' -> no_background_task g'.
Proof.
  intros g g' Hcap Hnb Hs. destruct Hs; unfold no_background_task in *; simpl.
  - exact Hnb.
  - intros t0 st [Heq | Hin].
    + inversion Heq; subst. apply H0. reflexivity.
    + exact (Hnb t0 st Hin).
  - intros t' st Hin. apply row_after_set_state in Hin. destruct Hin as [st0 Hin].
    exact (Hnb t' st0 Hin).
  - intros t st Hin. apply row_after_cancel in Hin. destruct Hin as [st0 Hin].
    exact (Hnb t st0 Hin).
  - exact Hnb.
  - rewrite Hcap in H. discriminate.
Qed.

(* MAIN: without the detach capability no run ever puts a task in the
   background. Detach is a permission, not a syntax. *)
Theorem background_only_via_detach : forall g g',
  detach_cap g = false -> no_background_task g -> steps g g' -> no_background_task g'.
Proof.
  intros g g' Hcap Hnb Hrun.
  induction Hrun as [g | g g' g'' Hs Hrun IH].
  - exact Hnb.
  - apply IH.
    + rewrite (step_keeps_cap g g' Hs). exact Hcap.
    + exact (step_preserves_no_background g g' Hcap Hnb Hs).
Qed.

(* ===================================================================== *)
(* 6. Refutation: the current contract admits an orphan                   *)
(*                                                                        *)
(* docs/113: a still-live named future is not rejected when its function  *)
(* exits. In the model that is a close with no join guard. Three steps    *)
(* from the empty world reach a running task whose scope is gone.         *)
(* ===================================================================== *)

Inductive unstructured_step : Config -> Config -> Prop :=
  | ustep_structured : forall g g', step g g' -> unstructured_step g g'
  | ustep_exit_without_join : forall g s,
      s <> root_scope ->
      unstructured_step g
        (mkConfig (tasks g) (remove_scope s (open g)) (sparent g) (detach_cap g)).

Inductive unstructured_steps : Config -> Config -> Prop :=
  | usteps_refl  : forall g, unstructured_steps g g
  | usteps_trans : forall g g' g'',
      unstructured_step g g' -> unstructured_steps g' g'' -> unstructured_steps g g''.

Definition empty_world : Config := mkConfig [] [root_scope] [] false.

Definition orphan_world : Config :=
  mkConfig [(7, 1, Running)] (remove_scope 1 [1; root_scope]) [(1, root_scope)] false.

Ltac root := unfold root_scope in *; simpl in *.

Theorem orphan_reachable_unstructured :
  wf empty_world /\ unstructured_steps empty_world orphan_world /\ orphan orphan_world.
Proof.
  split; [| split].
  - split.
    + unfold root_open, scope_open. simpl. left. reflexivity.
    + unfold contained, task_running. simpl. intros t s Hin. contradiction.
  - (* open scope 1 under the root, spawn task 7 in it, exit without joining *)
    eapply usteps_trans.
    + apply ustep_structured. apply (step_open empty_world 1 root_scope).
      * unfold scope_open. root. intros [H | H]; [discriminate H | exact H].
      * root. discriminate.
      * unfold scope_open. simpl. left. reflexivity.
    + eapply usteps_trans.
      * apply ustep_structured. apply (step_spawn _ 7 1).
        -- unfold scope_open. simpl. left. reflexivity.
        -- root. discriminate.
      * eapply usteps_trans.
        -- apply (ustep_exit_without_join _ 1). root. discriminate.
        -- apply usteps_refl.
  - exists 7, 1. split.
    + unfold task_running. simpl. left. reflexivity.
    + unfold scope_open. root. intros [H | H]; [discriminate H | exact H].
Qed.

(* ===================================================================== *)
(* 7. Non-vacuity: the discipline lets a scope run to completion          *)
(* ===================================================================== *)

Definition finished_world : Config :=
  mkConfig [(7, 1, Done)] (remove_scope 1 [1; root_scope]) [(1, root_scope)] false.

Theorem structured_run_exists : steps empty_world finished_world.
Proof.
  eapply steps_trans.
  - apply (step_open empty_world 1 root_scope).
    + unfold scope_open. root. intros [H | H]; [discriminate H | exact H].
    + root. discriminate.
    + unfold scope_open. simpl. left. reflexivity.
  - eapply steps_trans.
    + apply (step_spawn _ 7 1).
      * unfold scope_open. simpl. left. reflexivity.
      * root. discriminate.
    + eapply steps_trans.
      * apply (step_complete _ 7).
      * eapply steps_trans.
        -- apply (step_close _ 1).
           ++ root. discriminate.
           ++ unfold task_running. simpl. intros t [H | H]; [discriminate H | exact H].
           ++ simpl. intros c [H | H]; [| contradiction].
              injection H as Hc Hp. root. discriminate Hp.
        -- apply steps_refl.
Qed.
