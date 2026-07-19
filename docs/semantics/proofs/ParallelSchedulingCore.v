(*
  ParallelSchedulingCore.v  --  why the pool cannot deadlock, and what each
  competing await policy costs.

  Companion to docs/186 (parallel implementation plan P-A1/P-B1), and to the
  three runtime mechanisms it models:
    - help-first await          src/runtime/pgy_parallel_task_ops.h (pgy_await)
    - queue drain on help       src/runtime/pgy_parallel_pool_lifecycle.h
                                (pgy_pool_help_run_one)
    - compensation spare worker src/runtime/pgy_parallel_pool_lifecycle.h
                                (pgy_pool_spawn_spare_locked)

  The problem. A pool has a FIXED number of worker threads. If awaiting a task
  parks the worker, a fan-out whose children are still queued can park every
  worker -- and the queued children, whose completion is the only thing that
  would wake anyone, have no thread left to run on. This is not theoretical: it
  was witnessed RED as a hang by the nested-fan-out fixture (WO-RT-3) and again,
  in its channel form, by tests/channel_pool_starvation_probe.sh (WO-RT-5). Both
  mechanisms above were written in response. This file states the model in which
  each of them is the right answer, and proves it.

  The model. A worker is a STACK of task frames, so that help-nesting -- running
  a queued task on top of the frame that is awaiting -- is representable rather
  than abstracted away. `WRun []` is an idle worker. Three await policies share
  one step relation:

    PolParkOnly     await always parks the worker        (classic bounded pool)
    PolHelpFirst    await drains the queue first, and    (Pergyra join lane)
                    parks only when the queue is empty
    PolCompensate   await parks, but queued work with    (Pergyra channel lane)
                    no runner left adds a spare worker

  The axis that decides everything is `push`, the order in which frames were
  pushed, together with the hypothesis

    spawn_tree :  awaits h t  ->  push h < push t

  which says a task only ever awaits something pushed AFTER it. That is exactly
  true when a task awaits only tasks it spawned -- the join lane -- and exactly
  false for a channel receive, where the producer being waited on may have been
  pushed long before the waiter.

  Mechanized obligations:
    - [help_first_progress]  under PolHelpFirst with spawn-tree awaits, no
      non-final configuration is stuck: some rule always applies. Bounded
      workers, unbounded nesting, no deadlock.
    - [park_only_deadlocks]  PolParkOnly reaches a stuck configuration in three
      steps from a one-worker pool. The WO-RT-3 hang, machine-checked.
    - [cyclic_await_deadlocks]  PolHelpFirst ALSO reaches a stuck configuration
      once awaits may cycle, and [cyclic_await_breaks_spawn_tree] shows a cycle
      is precisely a spawn_tree violation. So the hypothesis carrying
      help_first_progress is load-bearing, not decoration.
    - [help_in_cyclic_wait_self_deadlocks]  the specific failure the runtime
      comment records: helping inside a cyclic wait buries the awaited task
      UNDER its helper on the same worker's stack. Stuck -- and impossible under
      spawn_tree, which is why helping is correct on the join lane and was
      refuted on the channel lane.
    - [compensation_moves_where_the_others_stick]  one configuration, three
      verdicts: stuck under PolParkOnly, stuck under PolHelpFirst, steps under
      PolCompensate.
    - [help_first_preserves_queue_runner] / [help_first_preserves_desc_stacks]
      the two invariants that carry the progress proof are preserved by every
      rule, so the theorem applies to reachable configurations and not only to
      hand-written ones.

  The scorecard is therefore not "ours is better". It is that the three policies
  have different domains, and the runtime uses each one where its hypothesis
  holds.

  Negative scope. This is a scheduling-progress model, not a memory model: no
  atomics, no happens-before, no C11 ordering; `push` abstracts a real clock.
  Progress means "some rule applies" -- absence of deadlock -- not termination
  and not fairness, so a starving-but-stepping schedule is outside the model.
  Two of the four invariants consumed by [help_first_progress] (park
  well-formedness and target location) are structural bookkeeping and are
  asserted rather than derived; the two that carry the argument are proved
  preserved. Binding this model to the C and LLVM emitters remains what the
  parallel gates check empirically.
*)

Require Import Coq.Lists.List.
Require Import Coq.Arith.PeanoNat.
Require Import Coq.micromega.Lia.
Import ListNotations.

Section ParallelScheduling.

Definition Task := nat.

(* The order in which frames were pushed onto a worker. A real execution reads
   this off a monotone clock; here it is an arbitrary assignment, constrained
   only by the hypotheses each theorem states. *)
Variable push : Task -> nat.

(* [awaits h t]: the task h is blocked on the completion of t. *)
Variable awaits : Task -> Task -> Prop.

(* ===================================================================== *)
(* 1. Configurations                                                      *)
(* ===================================================================== *)

(* A worker is a stack of frames; the head is what it executes. [] = idle.
   A parked worker keeps its stack -- that is the entire cost of parking. *)
Inductive WState : Type :=
  | WRun  (stack : list Task)
  | WPark (stack : list Task) (target : Task).

Record Config : Type := mkCfg {
  cqueued  : list Task;
  cworkers : list WState;
  cdone    : list Task
}.

Definition stack_of (w : WState) : list Task :=
  match w with WRun st => st | WPark st _ => st end.

Definition push_head (w : WState) : nat :=
  match stack_of w with [] => 0 | h :: _ => push h end.

(* ===================================================================== *)
(* 2. Policies                                                            *)
(* ===================================================================== *)

Inductive Policy : Type :=
  | PolParkOnly
  | PolHelpFirst
  | PolCompensate.

Definition pol_helps (p : Policy) : bool :=
  match p with PolHelpFirst => true | _ => false end.

Definition pol_compensates (p : Policy) : bool :=
  match p with PolCompensate => true | _ => false end.

(* The whole content of "help FIRST": under PolHelpFirst a worker may park only
   with an empty queue. The other policies park unconditionally. *)
Definition pol_park_ok (p : Policy) (q : list Task) : Prop :=
  match p with PolHelpFirst => q = [] | _ => True end.

(* ===================================================================== *)
(* 3. The step relation                                                   *)
(* ===================================================================== *)

Inductive step (p : Policy) : Config -> Config -> Prop :=

  (* An idle worker takes a queued task. *)
  | StTake : forall q t pre post dn,
      step p (mkCfg (t :: q) (pre ++ WRun []  :: post) dn)
             (mkCfg q        (pre ++ WRun [t] :: post) dn)

  (* A running task spawns a child into the queue. The child is fresh: not
     already queued, not done, and not on any stack. *)
  | StSpawn : forall q c h rest pre post dn,
      ~ In c q ->
      ~ In c dn ->
      (forall w, In w (pre ++ WRun (h :: rest) :: post) -> ~ In c (stack_of w)) ->
      step p (mkCfg q        (pre ++ WRun (h :: rest) :: post) dn)
             (mkCfg (c :: q) (pre ++ WRun (h :: rest) :: post) dn)

  (* A running task completes; its frame pops and the worker resumes whatever
     was underneath. *)
  | StFinish : forall q h rest pre post dn,
      step p (mkCfg q (pre ++ WRun (h :: rest) :: post) dn)
             (mkCfg q (pre ++ WRun rest        :: post) (h :: dn))

  (* HELP-FIRST. Rather than park, the awaiting worker pops a queued task and
     runs it nested above its own frame. The pushed frame is newer than
     everything already on the stack: that is what `push h < push t` records. *)
  | StHelp : forall q t h rest tg pre post dn,
      pol_helps p = true ->
      awaits h tg -> ~ In tg dn ->
      push h < push t ->
      step p (mkCfg (t :: q) (pre ++ WRun (h :: rest)      :: post) dn)
             (mkCfg q        (pre ++ WRun (t :: h :: rest) :: post) dn)

  (* PARK. The worker blocks, still holding its stack. *)
  | StPark : forall q h rest tg pre post dn,
      pol_park_ok p q ->
      awaits h tg -> ~ In tg dn ->
      step p (mkCfg q (pre ++ WRun  (h :: rest)    :: post) dn)
             (mkCfg q (pre ++ WPark (h :: rest) tg :: post) dn)

  (* WAKE. *)
  | StWake : forall q st tg pre post dn,
      In tg dn ->
      step p (mkCfg q (pre ++ WPark st tg :: post) dn)
             (mkCfg q (pre ++ WRun st     :: post) dn)

  (* COMPENSATION. Work is queued and every worker is blocked: add a runner
     rather than nest one. Nesting is what self-deadlocks a cyclic wait -- see
     help_in_cyclic_wait_self_deadlocks. *)
  | StSpare : forall q ws dn,
      pol_compensates p = true ->
      q <> [] ->
      (forall w, In w ws -> exists st tg, w = WPark st tg) ->
      step p (mkCfg q ws                dn)
             (mkCfg q (ws ++ [WRun []]) dn).

Inductive steps (p : Policy) : Config -> Config -> Prop :=
  | steps_refl : forall c, steps p c c
  | steps_more : forall a b c, step p a b -> steps p b c -> steps p a c.

Definition stuck (p : Policy) (c : Config) : Prop := ~ exists c', step p c c'.

Definition final (c : Config) : Prop :=
  cqueued c = [] /\ Forall (fun w => w = WRun []) (cworkers c).

(* ===================================================================== *)
(* 4. Progress for the join lane (PolHelpFirst + spawn-tree awaits)       *)
(* ===================================================================== *)

(* A stack's head was pushed last, hence carries the largest stamp. *)
Definition head_is_max (w : WState) : Prop :=
  match stack_of w with
  | []     => True
  | h :: _ => forall x, In x (stack_of w) -> push x <= push h
  end.

Lemma in_middle : forall (x : WState) pre post, In x (pre ++ x :: post).
Proof. intros x pre post. apply in_or_app. right. left. reflexivity. Qed.

Definition push_bound (ws : list WState) : nat :=
  fold_right (fun w acc => Nat.max (push_head w) acc) 0 ws.

Lemma push_head_le_bound : forall ws w,
  In w ws -> push_head w <= push_bound ws.
Proof.
  induction ws as [| a ws IH]; intros w Hin; simpl in *.
  - contradiction.
  - destruct Hin as [Heq | Hin].
    + subst a. apply Nat.le_max_l.
    + apply (Nat.le_trans _ (push_bound ws)).
      * apply IH. exact Hin.
      * apply Nat.le_max_r.
Qed.

(* Either some worker has a running frame, or every worker is idle or parked. *)
Lemma worker_split : forall ws,
  (exists h rest, In (WRun (h :: rest)) ws)
  \/ (forall w, In w ws -> w = WRun [] \/ exists st tg, w = WPark st tg).
Proof.
  induction ws as [| w ws IH].
  - right. intros w Hin. contradiction.
  - destruct IH as [[h [rest Hin]] | Hall].
    + left. exists h, rest. right. exact Hin.
    + destruct w as [st | st tg].
      * destruct st as [| h rest].
        -- right. intros w0 [Heq | Hin].
           ++ subst w0. left. reflexivity.
           ++ apply Hall. exact Hin.
        -- left. exists h, rest. left. reflexivity.
      * right. intros w0 [Heq | Hin].
        -- subst w0. right. exists st, tg. reflexivity.
        -- apply Hall. exact Hin.
Qed.

Lemma parked_or_all_idle : forall ws,
  (forall w, In w ws -> w = WRun [] \/ exists st tg, w = WPark st tg) ->
  (exists st tg, In (WPark st tg) ws)
  \/ Forall (fun w => w = WRun []) ws.
Proof.
  induction ws as [| w ws IH]; intro Hall.
  - right. apply Forall_nil.
  - assert (Hsub : forall w0, In w0 ws ->
                     w0 = WRun [] \/ exists st tg, w0 = WPark st tg).
    { intros w0 Hin0. apply Hall. right. exact Hin0. }
    destruct (Hall w (or_introl eq_refl)) as [Hidle | [st [tg Hpark]]].
    + destruct (IH Hsub) as [[st [tg Hin]] | Hf].
      * left. exists st, tg. right. exact Hin.
      * right. apply Forall_cons; [exact Hidle | exact Hf].
    + left. exists st, tg. left. exact Hpark.
Qed.

(* Either some parked worker's target has completed (so a wake is available),
   or no parked target has completed at all. *)
Lemma parked_done_or_none : forall ws dn,
  (exists st tg, In (WPark st tg) ws /\ In tg dn)
  \/ (forall st tg, In (WPark st tg) ws -> ~ In tg dn).
Proof.
  induction ws as [| w ws IH]; intro dn.
  - right. intros st tg Hin. contradiction.
  - destruct (IH dn) as [[st [tg [Hin Hd]]] | Hall].
    + left. exists st, tg. split; [right; exact Hin | exact Hd].
    + destruct w as [st0 | st0 tg0].
      * right. intros st tg [Heq | Hin];
          [discriminate | exact (Hall st tg Hin)].
      * destruct (in_dec Nat.eq_dec tg0 dn) as [Hd | Hnd].
        -- left. exists st0, tg0. split; [left; reflexivity | exact Hd].
        -- right. intros st tg [Heq | Hin].
           ++ injection Heq as Hs Ht. subst. exact Hnd.
           ++ exact (Hall st tg Hin).
Qed.

(* The heart of the progress argument. In an all-parked, empty-queue
   configuration each parked worker's target lives on ANOTHER parked worker's
   stack -- and spawn_tree makes that other worker's head strictly newer. So
   "waits-for" strictly increases push_head, which a finite worker list cannot
   sustain. *)
Lemma parked_has_newer_parked : forall ws h rest tg,
  (forall a b, awaits a b -> push a < push b) ->
  (forall w, In w ws -> head_is_max w) ->
  (forall h' rest', ~ In (WRun (h' :: rest')) ws) ->
  (forall st' tg', In (WPark st' tg') ws ->
      exists w, In w ws /\ In tg' (stack_of w)) ->
  awaits h tg ->
  In (WPark (h :: rest) tg) ws ->
  exists st' tg', In (WPark st' tg') ws
                  /\ push h < push_head (WPark st' tg').
Proof.
  intros ws h rest tg Hspawn Hmax Hnorun Hloc Haw Hin.
  destruct (Hloc (h :: rest) tg Hin) as [w' [Hin' Hon']].
  destruct w' as [st' | st' tg'].
  - (* the target would sit on a RUNNING worker -- excluded in this case *)
    exfalso. simpl in Hon'.
    destruct st' as [| h' rest']; [contradiction |].
    apply (Hnorun h' rest'). exact Hin'.
  - simpl in Hon'.
    destruct st' as [| h' rest']; [contradiction |].
    exists (h' :: rest'), tg'. split; [exact Hin' |].
    assert (Hle : push tg <= push h').
    { specialize (Hmax _ Hin'). unfold head_is_max in Hmax. simpl in Hmax.
      apply Hmax. exact Hon'. }
    specialize (Hspawn _ _ Haw). unfold push_head. simpl. lia.
Qed.

Lemma no_parked_worker : forall bound ws st tg,
  (forall a b, awaits a b -> push a < push b) ->
  (forall w, In w ws -> head_is_max w) ->
  (forall h' rest', ~ In (WRun (h' :: rest')) ws) ->
  (forall st' tg', In (WPark st' tg') ws ->
      exists w, In w ws /\ In tg' (stack_of w)) ->
  (forall st' tg', In (WPark st' tg') ws ->
      exists h' rest', st' = h' :: rest' /\ awaits h' tg') ->
  In (WPark st tg) ws ->
  push_bound ws - push_head (WPark st tg) <= bound ->
  False.
Proof.
  induction bound as [| b IH];
    intros ws st tg Hspawn Hmax Hnorun Hloc Hwf Hin Hbnd.
  - destruct (Hwf _ _ Hin) as [h [rest [Hst Haw]]]. subst st.
    destruct (parked_has_newer_parked ws h rest tg
                Hspawn Hmax Hnorun Hloc Haw Hin) as [st' [tg' [Hin' Hlt]]].
    pose proof (push_head_le_bound ws _ Hin') as Hle.
    unfold push_head in Hbnd. simpl in Hbnd. lia.
  - destruct (Hwf _ _ Hin) as [h [rest [Hst Haw]]]. subst st.
    destruct (parked_has_newer_parked ws h rest tg
                Hspawn Hmax Hnorun Hloc Haw Hin) as [st' [tg' [Hin' Hlt]]].
    pose proof (push_head_le_bound ws _ Hin') as Hle.
    apply (IH ws st' tg' Hspawn Hmax Hnorun Hloc Hwf Hin').
    unfold push_head in Hbnd. simpl in Hbnd. lia.
Qed.

(* MAIN. Under help-first with spawn-tree awaits, a non-final configuration
   always has a step: bounded workers, unbounded nesting, no deadlock. *)
Theorem help_first_progress : forall q ws dn,
  ws <> [] ->
  (forall a b, awaits a b -> push a < push b) ->
  (forall w, In w ws -> head_is_max w) ->
  (forall st tg, In (WPark st tg) ws ->
      exists h rest, st = h :: rest /\ awaits h tg) ->
  (forall st tg, In (WPark st tg) ws -> ~ In tg dn ->
      In tg q \/ (exists w, In w ws /\ In tg (stack_of w))) ->
  (q <> [] -> exists st, In (WRun st) ws) ->
  ~ final (mkCfg q ws dn) ->
  exists c', step PolHelpFirst (mkCfg q ws dn) c'.
Proof.
  intros q ws dn Hne Hspawn Hmax Hwf Hloc Hrunner Hnf.
  destruct (worker_split ws) as [[h [rest Hin]] | Hall].
  - (* something is running: it can always finish *)
    apply in_split in Hin. destruct Hin as [pre [post Heq]].
    rewrite Heq. eexists. apply StFinish.
  - (* nothing running: every worker is idle or parked *)
    assert (Hnorun : forall h' rest', ~ In (WRun (h' :: rest')) ws).
    { intros h' rest' Hbad.
      destruct (Hall _ Hbad) as [He | [st' [tg' He]]]; discriminate. }
    destruct q as [| t q'].
    + (* empty queue *)
      destruct (parked_done_or_none ws dn) as [[st [tg [Hin Hd]]] | Hnodone].
      * (* a target has completed: wake *)
        apply in_split in Hin. destruct Hin as [pre [post Heq]].
        rewrite Heq. eexists. apply StWake. exact Hd.
      * destruct (parked_or_all_idle ws Hall) as [[st [tg Hin]] | Hidle].
        -- (* nobody can move -- excluded by the spawn-tree hypothesis *)
           exfalso.
           assert (Hloc' : forall st' tg', In (WPark st' tg') ws ->
                     exists w, In w ws /\ In tg' (stack_of w)).
           { intros st' tg' Hin'.
             destruct (Hloc st' tg' Hin' (Hnodone st' tg' Hin')) as [Hq | Hs].
             - contradiction.
             - exact Hs. }
           apply (no_parked_worker (push_bound ws) ws st tg
                    Hspawn Hmax Hnorun Hloc' Hwf Hin).
           lia.
        -- (* every worker idle and the queue empty: that is final *)
           exfalso. apply Hnf. split; [reflexivity | exact Hidle].
    + (* the queue is non-empty, so some worker is a WRun -- and with nothing
         running, it must be idle *)
      assert (Hq : t :: q' <> []) by discriminate.
      destruct (Hrunner Hq) as [st Hin].
      destruct (Hall _ Hin) as [He | [st' [tg' He]]]; [| discriminate].
      injection He as Hst. subst st.
      apply in_split in Hin. destruct Hin as [pre [post Heq]].
      rewrite Heq. eexists. apply StTake.
Qed.

(* ===================================================================== *)
(* 5. What the competing policies cost                                    *)
(* ===================================================================== *)

(* If every worker is parked and nothing has completed, only compensation can
   move: every other rule needs a running or idle worker. *)
Lemma all_parked_is_stuck : forall p q ws,
  pol_compensates p = false ->
  (forall w, In w ws -> exists st tg, w = WPark st tg) ->
  stuck p (mkCfg q ws []).
Proof.
  intros p q ws Hcomp Hall [c' Hstep].
  assert (Hnorun : forall st, ~ In (WRun st) ws).
  { intros st Hin. destruct (Hall _ Hin) as [st' [tg' He]]. discriminate. }
  inversion Hstep; subst;
    try (eapply Hnorun; apply in_middle);
    try congruence;
    try (match goal with H : In _ nil |- _ => destruct H end).
Qed.

(* --- (a) the classic bounded pool: park-only ------------------------- *)

(* Three steps from a one-worker pool holding a task that fans out: take the
   task, spawn a child, await it. The worker is now parked on a child that is
   sitting in the queue with no thread left to run it. This is the WO-RT-3
   hang. *)
Theorem park_only_deadlocks : forall h tg,
  awaits h tg -> h <> tg ->
  steps PolParkOnly (mkCfg [h] [WRun []] []) (mkCfg [tg] [WPark [h] tg] [])
  /\ stuck   PolParkOnly (mkCfg [tg] [WPark [h] tg] [])
  /\ ~ final (mkCfg [tg] [WPark [h] tg] []).
Proof.
  intros h tg Haw Hne. split; [| split].
  - apply (steps_more _ _ (mkCfg [] [WRun [h]] [])).
    { apply (StTake PolParkOnly [] h [] []). }
    apply (steps_more _ _ (mkCfg [tg] [WRun [h]] [])).
    { apply (StSpawn PolParkOnly [] tg h [] [] []).
      - intro Hbad. contradiction.
      - intro Hbad. contradiction.
      - intros w [Heq | Hbad]; [| contradiction].
        subst w. simpl. intros [Heq | Hbad]; [| contradiction].
        apply Hne. exact Heq. }
    apply (steps_more _ _ (mkCfg [tg] [WPark [h] tg] [])).
    { apply (StPark PolParkOnly [tg] h [] tg [] []).
      - exact I.
      - exact Haw.
      - intro Hbad. contradiction. }
    apply steps_refl.
  - apply all_parked_is_stuck; [reflexivity |].
    intros w [Heq | Hbad]; [| contradiction].
    subst w. exists [h], tg. reflexivity.
  - intros [Hq _]. discriminate Hq.
Qed.

(* --- (b) help-first is not enough once awaits may cycle -------------- *)

(* A cycle in the await graph IS a spawn-tree violation -- so this is exactly
   the hypothesis that help_first_progress rests on, and exactly what a channel
   wait gives up. *)
Theorem cyclic_await_breaks_spawn_tree : forall a b,
  awaits a b -> awaits b a ->
  ~ (forall x y, awaits x y -> push x < push y).
Proof.
  intros a b Hab Hba Hst.
  pose proof (Hst _ _ Hab). pose proof (Hst _ _ Hba). lia.
Qed.

(* With cyclic awaits, help-first parks legally (the queue really is empty when
   each worker parks) and still deadlocks. *)
Theorem cyclic_await_deadlocks : forall a b,
  awaits a b -> awaits b a ->
  steps PolHelpFirst (mkCfg [a; b] [WRun []; WRun []] [])
                     (mkCfg [] [WPark [a] b; WPark [b] a] [])
  /\ stuck   PolHelpFirst (mkCfg [] [WPark [a] b; WPark [b] a] [])
  /\ ~ final (mkCfg [] [WPark [a] b; WPark [b] a] []).
Proof.
  intros a b Hab Hba. split; [| split].
  - apply (steps_more _ _ (mkCfg [b] [WRun [a]; WRun []] [])).
    { apply (StTake PolHelpFirst [b] a [] [WRun []]). }
    apply (steps_more _ _ (mkCfg [] [WRun [a]; WRun [b]] [])).
    { apply (StTake PolHelpFirst [] b [WRun [a]] []). }
    apply (steps_more _ _ (mkCfg [] [WPark [a] b; WRun [b]] [])).
    { apply (StPark PolHelpFirst [] a [] b [] [WRun [b]]).
      - reflexivity.
      - exact Hab.
      - intro Hbad. contradiction. }
    apply (steps_more _ _ (mkCfg [] [WPark [a] b; WPark [b] a] [])).
    { apply (StPark PolHelpFirst [] b [] a [WPark [a] b] []).
      - reflexivity.
      - exact Hba.
      - intro Hbad. contradiction. }
    apply steps_refl.
  - apply all_parked_is_stuck; [reflexivity |].
    intros w [Heq | [Heq | Hbad]]; [| | contradiction].
    + subst w. exists [a], b. reflexivity.
    + subst w. exists [b], a. reflexivity.
  - intros [_ HF]. inversion HF as [| w ws Hw Hrest]. discriminate Hw.
Qed.

(* The runtime's own refutation, as a theorem. Helping inside a cyclic wait
   pushes the helper ABOVE the very task being awaited, on the same worker: the
   worker is now parked on something buried under its own frame, and nothing
   can pop it. Under spawn_tree this stack cannot exist -- a stack head is the
   newest frame, but an awaited task must be newer than its awaiter. *)
Theorem help_in_cyclic_wait_self_deadlocks : forall c p0,
  awaits c p0 ->
  stuck   PolHelpFirst (mkCfg [] [WPark [c; p0] p0] [])
  /\ ~ final (mkCfg [] [WPark [c; p0] p0] [])
  /\ (head_is_max (WPark [c; p0] p0) ->
      ~ (forall x y, awaits x y -> push x < push y)).
Proof.
  intros c p0 Haw. split; [| split].
  - apply all_parked_is_stuck; [reflexivity |].
    intros w [Heq | Hbad]; [| contradiction].
    subst w. exists [c; p0], p0. reflexivity.
  - intros [_ HF]. inversion HF as [| w ws Hw Hrest]. discriminate Hw.
  - intros Hmax Hst. unfold head_is_max in Hmax. simpl in Hmax.
    assert (Hle : push p0 <= push c) by (apply Hmax; right; left; reflexivity).
    specialize (Hst _ _ Haw). lia.
Qed.

(* --- (c) compensation moves where both of the others stick ----------- *)

(* One configuration -- a queued task and the only worker parked -- three
   verdicts. This is the WO-RT-5 channel witness. *)
Theorem compensation_moves_where_the_others_stick : forall t st tg,
  stuck PolParkOnly  (mkCfg [t] [WPark st tg] [])
  /\ stuck PolHelpFirst (mkCfg [t] [WPark st tg] [])
  /\ exists c', step PolCompensate (mkCfg [t] [WPark st tg] []) c'.
Proof.
  intros t st tg. split; [| split].
  - apply all_parked_is_stuck; [reflexivity |].
    intros w [Heq | Hbad]; [| contradiction]. subst w. exists st, tg. reflexivity.
  - apply all_parked_is_stuck; [reflexivity |].
    intros w [Heq | Hbad]; [| contradiction]. subst w. exists st, tg. reflexivity.
  - eexists. apply StSpare.
    + reflexivity.
    + discriminate.
    + intros w [Heq | Hbad]; [| contradiction]. subst w. exists st, tg. reflexivity.
Qed.

(* --- (d) the two policies on the SAME configuration ------------------ *)

Lemma singleton_app_cons : forall (x y : WState) pre post,
  pre ++ y :: post = [x] -> pre = [] /\ post = [] /\ y = x.
Proof.
  intros x y pre post H.
  destruct pre as [| a pre]; simpl in H.
  - injection H as Hy Hp. split; [reflexivity | split; [exact Hp | exact Hy]].
  - injection H as Ha Hp. destruct pre; simpl in Hp; discriminate.
Qed.

(* The WO-RT-3 shape: one worker running a task that awaits a child which is
   still QUEUED. Help-first cannot produce a parked worker from here at all --
   StPark is the only rule that parks and its guard is exactly "the queue is
   empty". So the state that deadlocks park-only is not merely avoided by luck;
   it is unreachable. *)
Lemma help_first_never_parks_with_work : forall h t c',
  step PolHelpFirst (mkCfg [t] [WRun [h]] []) c' ->
  forall st tg, ~ In (WPark st tg) (cworkers c').
Proof.
  intros h t c' Hstep st tg Hin.
  inversion Hstep; subst; simpl in *;
    repeat (match goal with
            | H : _ ++ _ :: _ = [_] |- _ =>
                apply singleton_app_cons in H; destruct H as [? [? ?]]; subst
            | H : [_] = _ ++ _ :: _ |- _ =>
                symmetry in H; apply singleton_app_cons in H;
                destruct H as [? [? ?]]; subst
            end);
    simpl in *;
    try discriminate;
    try (destruct Hin as [Hbad | Hbad]; [discriminate | contradiction]).
Qed.

(* ===================================================================== *)
(* 6. The invariants carrying the progress proof are inductive            *)
(* ===================================================================== *)

(* (i) Help-first never parks while work is queued. This is what makes the
   all-parked-with-nonempty-queue configuration -- the park-only deadlock --
   unreachable on the join lane. *)
Definition queue_has_runner (c : Config) : Prop :=
  cqueued c <> [] -> exists st, In (WRun st) (cworkers c).

Lemma help_first_preserves_queue_runner : forall c c',
  queue_has_runner c -> step PolHelpFirst c c' -> queue_has_runner c'.
Proof.
  intros c c' Hinv Hstep.
  destruct Hstep; unfold queue_has_runner in *; simpl in *; intro Hq.
  - exists [t]. apply in_middle.
  - exists (h :: rest). apply in_middle.
  - exists rest. apply in_middle.
  - exists (t :: h :: rest). apply in_middle.
  - simpl in H. subst q. contradiction.
  - exists st. apply in_middle.
  - discriminate H.
Qed.

(* (ii) Stacks are strictly descending in push order: the head is the newest
   frame. Unlike head_is_max this survives a pop, so it is the inductive form. *)
Inductive desc_stack : list Task -> Prop :=
  | ds_nil  : desc_stack []
  | ds_one  : forall t, desc_stack [t]
  | ds_cons : forall a b r,
      push b < push a -> desc_stack (b :: r) -> desc_stack (a :: b :: r).

Lemma desc_stack_head_max : forall st, desc_stack st -> head_is_max (WRun st).
Proof.
  intros st Hd. unfold head_is_max. simpl. induction Hd.
  - exact I.
  - intros x [Heq | Hf]; [subst; apply Nat.le_refl | contradiction].
  - intros x [Heq | Hin].
    + subst. apply Nat.le_refl.
    + simpl in IHHd. specialize (IHHd x Hin). lia.
Qed.

Lemma desc_stack_pop : forall h rest, desc_stack (h :: rest) -> desc_stack rest.
Proof.
  intros h rest Hd. inversion Hd; subst.
  - apply ds_nil.
  - assumption.
Qed.

Definition desc_stacks (c : Config) : Prop :=
  forall w, In w (cworkers c) -> desc_stack (stack_of w).

Lemma in_app_cons_cases : forall (w : WState) pre x post,
  In w (pre ++ x :: post) -> w = x \/ In w (pre ++ post).
Proof.
  intros w pre x post Hin.
  apply in_app_or in Hin. destruct Hin as [Hpre | [Heq | Hpost]].
  - right. apply in_or_app. left. exact Hpre.
  - left. exact (eq_sym Heq).
  - right. apply in_or_app. right. exact Hpost.
Qed.

Lemma help_first_preserves_desc_stacks : forall c c',
  desc_stacks c -> step PolHelpFirst c c' -> desc_stacks c'.
Proof.
  intros c c' Hinv Hstep.
  destruct Hstep; unfold desc_stacks in *; simpl in *;
    intros w Hin; apply in_app_cons_cases in Hin; destruct Hin as [Heq | Hin].
  - subst w. apply ds_one.
  - apply Hinv. apply in_or_app.
    apply in_app_or in Hin. destruct Hin as [Hp | Hp];
      [left; exact Hp | right; right; exact Hp].
  - subst w. apply (Hinv (WRun (h :: rest))). apply in_middle.
  - apply Hinv. apply in_or_app.
    apply in_app_or in Hin. destruct Hin as [Hp | Hp];
      [left; exact Hp | right; right; exact Hp].
  - subst w. simpl. apply (desc_stack_pop h).
    apply (Hinv (WRun (h :: rest))). apply in_middle.
  - apply Hinv. apply in_or_app.
    apply in_app_or in Hin. destruct Hin as [Hp | Hp];
      [left; exact Hp | right; right; exact Hp].
  - subst w. simpl. apply ds_cons; [exact H2 |].
    apply (Hinv (WRun (h :: rest))). apply in_middle.
  - apply Hinv. apply in_or_app.
    apply in_app_or in Hin. destruct Hin as [Hp | Hp];
      [left; exact Hp | right; right; exact Hp].
  - subst w. simpl. apply (Hinv (WRun (h :: rest))). apply in_middle.
  - apply Hinv. apply in_or_app.
    apply in_app_or in Hin. destruct Hin as [Hp | Hp];
      [left; exact Hp | right; right; exact Hp].
  - subst w. simpl. apply (Hinv (WPark st tg)). apply in_middle.
  - apply Hinv. apply in_or_app.
    apply in_app_or in Hin. destruct Hin as [Hp | Hp];
      [left; exact Hp | right; right; exact Hp].
  - discriminate H.
  - discriminate H.
Qed.

End ParallelScheduling.

(* ===================================================================== *)
(* 7. Adequacy: the progress hypotheses are jointly satisfiable           *)
(*                                                                        *)
(* A progress theorem whose hypotheses contradict each other proves        *)
(* nothing. Instantiate at push := identity and awaits := (<), which is a  *)
(* legal spawn tree, on a configuration that exercises the two structural  *)
(* hypotheses non-vacuously: worker 0 is parked awaiting task 1, and       *)
(* worker 1 is running task 1.                                            *)
(* ===================================================================== *)

Example help_first_progress_is_not_vacuous :
  exists c',
    step (fun x : Task => x) (fun a b : Task => a < b) PolHelpFirst
      (mkCfg [] [WPark [0] 1; WRun [1]] []) c'.
Proof.
  apply (help_first_progress (fun x : Task => x) (fun a b : Task => a < b)).
  - discriminate.
  - intros a b H. exact H.
  - intros w Hin. destruct Hin as [Heq | [Heq | Hf]]; try contradiction; subst w.
    + unfold head_is_max. simpl. intros x [Hx | Hf]; [subst x | contradiction].
      apply Nat.le_refl.
    + unfold head_is_max. simpl. intros x [Hx | Hf]; [subst x | contradiction].
      apply Nat.le_refl.
  - intros st tg Hin. destruct Hin as [Heq | [Heq | Hf]]; try contradiction.
    + injection Heq as Hs Ht. subst. exists 0, [].
      split; [reflexivity | lia].
    + discriminate.
  - intros st tg Hin Hnd. destruct Hin as [Heq | [Heq | Hf]]; try contradiction.
    + injection Heq as Hs Ht. subst. right. exists (WRun [1]).
      split; [right; left; reflexivity | left; reflexivity].
    + discriminate.
  - intro Hq. exfalso. apply Hq. reflexivity.
  - intros [_ HF]. inversion HF as [| x l Hx Hl]. discriminate Hx.
Qed.

(* ===================================================================== *)
(* 8. Scorecard                                                           *)
(*                                                                        *)
(*                        nested fan-out    cyclic wait    threads used    *)
(*                        (spawn tree)      (channel)                      *)
(*   -------------------  ---------------   ------------   -------------   *)
(*   PolParkOnly          DEADLOCK          DEADLOCK       bounded         *)
(*                        park_only_                                       *)
(*                        deadlocks                                        *)
(*   PolHelpFirst         progress          DEADLOCK       bounded         *)
(*                        help_first_       cyclic_await_                  *)
(*                        progress          deadlocks                      *)
(*   PolCompensate        progress          progress       bounded + spares *)
(*                                          compensation_                  *)
(*                                          moves_...                      *)
(*                                                                          *)
(* The runtime runs help-first on the join lane, where spawn_tree holds and  *)
(* no spare thread is ever needed, and compensation on the channel lane,     *)
(* where it does not. Neither mechanism is redundant and neither generalises *)
(* to the other's lane: help_in_cyclic_wait_self_deadlocks is the proof that *)
(* helping a cyclic wait is not merely unhelpful but fatal, which is what    *)
(* the WO-RT-5 backpressure gate found empirically before this model existed.*)
(* ===================================================================== *)
