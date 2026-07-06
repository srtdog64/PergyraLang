(*
  Pergyra Formal Semantics - Mechanized Sketch
  Target: docs/173 INT-4 -- the cross-intent conflict kernel.
  Status: proof-sketch; not beta-closure evidence unless checked by CI (coqc).

  IntentSpine.v covers ONE intent (obligations -> guard-freedom ->
  composition). This file covers MANY: the runtime admission rule of
  pgy_intent_enter_export (src/runtime/pgy_runtime_lib_set_intent_trace_
  exports.c -- the code read and hardened in the F1 fix, fe70f180) is
  transcribed as `conflict_guard`, and the static separation evidence of
  the docs/167 B-axis conflict graph is shown to make it unfireable:

    (1) `separated_trace_conflict_free`: a trace in which every admitted
        intent is statically separated from every co-active intent never
        fires the admission conflict guard. Separation evidence =
        subject-disjointness, static nesting (active is an ancestor of
        the candidate), or declared mutual concurrency. Ordering evidence
        appears implicitly: pairs that are never co-active are never
        constrained at all.
    (2) `priority_waives_only_one_order` (design theorem for docs/167):
        priority is deliberately NOT separation evidence -- the same two
        declarations pass admission in one activation order and fire the
        guard in the other. Conflict-graph edges must therefore never be
        erased on priority alone; priority is a runtime tiebreak.
    (3) `conflict_guard_real`: non-vacuity -- overlapping, unwaived
        intents do fire the guard (fail-closed is not decorative).

  Modeling notes (honest scope):
  - The ancestor relation is a section parameter: statically it comes
    from lexical intent nesting; at runtime from the parent chain whose
    cycle-freedom F1 bounded (and IntentSpine.no_dep_cycle excludes
    statically for checked coordination facts).
  - Admission asymmetry is modeled faithfully: the waiver checks whether
    the ACTIVE intent is an ancestor of the CANDIDATE (as the runtime
    does), not the symmetric closure.
  - Leave-order and handle reuse are not modeled; the registry is a bag
    of active declarations. INT-4's semantic pass (computing static
    co-activity from parallel/spawn structure) is implementation work,
    not claimed here.
*)

Require Import Coq.Lists.List.
Require Import Coq.Arith.PeanoNat.
Require Import Lia.
Import ListNotations.

Definition subject := nat.

Record IntentDecl := {
  d_handle     : nat;
  d_subjects   : list subject;
  d_concurrent : bool;
  d_priority   : nat
}.

Definition overlap (a b : IntentDecl) : Prop :=
  exists s, In s (d_subjects a) /\ In s (d_subjects b).

Section Registry.

(* Static nesting: anc x y = intent x is an ancestor of intent y. *)
Variable anc : nat -> nat -> Prop.

(* The runtime admission guard, transcribed from pgy_intent_enter_export:
   candidate `cand` is rejected against active entry `act` when subjects
   overlap and no waiver applies -- act is not an ancestor of cand, the
   two are not both declared concurrent, and cand does not outrank act. *)
Definition conflict_guard (act cand : IntentDecl) : Prop :=
  overlap act cand
  /\ ~ anc (d_handle act) (d_handle cand)
  /\ ~ (d_concurrent act = true /\ d_concurrent cand = true)
  /\ ~ (d_priority cand > d_priority act).

(* Static separation evidence for "cand admitted while act is active".
   Priority is deliberately absent -- see priority_waives_only_one_order. *)
Definition sep_when_active (act cand : IntentDecl) : Prop :=
  (forall s, In s (d_subjects act) -> ~ In s (d_subjects cand))
  \/ anc (d_handle act) (d_handle cand)
  \/ (d_concurrent act = true /\ d_concurrent cand = true).

Lemma separated_no_guard :
  forall act cand, sep_when_active act cand -> ~ conflict_guard act cand.
Proof.
  intros act cand Hsep [Hov [Hanc [Hconc _]]].
  destruct Hsep as [Hdisj | [Hanc' | Hcc]].
  - destruct Hov as [s [Hin1 Hin2]]. exact (Hdisj s Hin1 Hin2).
  - exact (Hanc Hanc').
  - exact (Hconc Hcc).
Qed.

(* Registry traces: Enter admits a candidate against the current active
   set; Leave removes by handle. *)
Inductive ev : Type :=
| EvEnter (d : IntentDecl)
| EvLeave (h : nat).

Definition drop (h : nat) (act : list IntentDecl) : list IntentDecl :=
  filter (fun a => negb (Nat.eqb (d_handle a) h)) act.

(* Statically separated trace: every admission is separated from every
   co-active intent. Pairs that never become co-active are unconstrained
   (that is the ordering-evidence case of docs/167). *)
Inductive trace_ok : list IntentDecl -> list ev -> Prop :=
| tr_nil : forall act, trace_ok act []
| tr_enter : forall act d rest,
    (forall a, In a act -> sep_when_active a d) ->
    trace_ok (d :: act) rest ->
    trace_ok act (EvEnter d :: rest)
| tr_leave : forall act h rest,
    trace_ok (drop h act) rest ->
    trace_ok act (EvLeave h :: rest).

Inductive no_conflict_fires : list IntentDecl -> list ev -> Prop :=
| ncf_nil : forall act, no_conflict_fires act []
| ncf_enter : forall act d rest,
    (forall a, In a act -> ~ conflict_guard a d) ->
    no_conflict_fires (d :: act) rest ->
    no_conflict_fires act (EvEnter d :: rest)
| ncf_leave : forall act h rest,
    no_conflict_fires (drop h act) rest ->
    no_conflict_fires act (EvLeave h :: rest).

(* THE theorem (INT-4 static face): separated traces never fire the
   runtime admission conflict guard -- the cross-intent analogue of
   IntentSpine.checked_intent_guard_free. *)
Theorem separated_trace_conflict_free :
  forall act evs, trace_ok act evs -> no_conflict_fires act evs.
Proof.
  intros act evs H.
  induction H as [ act
                 | act d rest Hsep Hrest IH
                 | act h rest Hrest IH ].
  - constructor.
  - constructor.
    + intros a Ha. apply separated_no_guard. apply Hsep. exact Ha.
    + exact IH.
  - constructor. exact IH.
Qed.

End Registry.

(* Non-vacuity: overlapping, unwaived intents DO fire the guard. *)
Example conflict_guard_real :
  conflict_guard (fun _ _ => False)
    {| d_handle := 1; d_subjects := [7]; d_concurrent := false; d_priority := 5 |}
    {| d_handle := 2; d_subjects := [7]; d_concurrent := false; d_priority := 5 |}.
Proof.
  split; [| split; [| split]].
  - exists 7. simpl. auto.
  - simpl. intros F. exact F.
  - simpl. intros [Ha _]. discriminate Ha.
  - simpl. lia.
Qed.

(* Design theorem for docs/167 B-axis: priority waives admission in ONE
   activation order only, so it is not symmetric separation evidence and
   conflict-graph edges must not be erased on priority alone. *)
Example priority_waives_only_one_order :
  let hi := {| d_handle := 1; d_subjects := [7];
               d_concurrent := false; d_priority := 9 |} in
  let lo := {| d_handle := 2; d_subjects := [7];
               d_concurrent := false; d_priority := 1 |} in
  ~ conflict_guard (fun _ _ => False) lo hi
  /\ conflict_guard (fun _ _ => False) hi lo.
Proof.
  split.
  - intros [_ [_ [_ Hpri]]]. apply Hpri. simpl. lia.
  - split; [| split; [| split]].
    + exists 7. simpl. auto.
    + simpl. intros F. exact F.
    + simpl. intros [Ha _]. discriminate Ha.
    + simpl. lia.
Qed.
