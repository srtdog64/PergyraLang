(*
  Pergyra Formal Semantics - Mechanized Sketch
  Target: docs/173 INT-4 -- the cross-intent conflict kernel.
  Status: proof-sketch; not beta-closure evidence unless checked by CI (coqc).

  IntentSpine.v covers ONE intent (obligations -> guard-freedom ->
  composition). This file covers MANY: the runtime admission rule of
  pgy_intent_enter_export (src/runtime/pgy_runtime_lib_set_intent_trace_
  exports.c -- the code read and hardened in the F1 fix, fe70f180) is
  transcribed as `conflict_guard`. Separation eliminates that guard only
  when nesting carries admitted RUNTIME ancestry, not lexical nesting alone:

    (1) `separated_trace_conflict_free`: a trace in which every admitted
        intent is statically separated from every co-active intent never
        fires the admission conflict guard. Separation evidence =
        subject-disjointness, admitted nesting (active is a LIVE ancestor of
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
  - The conditional separation lemma takes admitted ancestry as an interface
    premise. The executable registry ancestry below supplies it only from
    active entries; IntentSpine.no_dep_cycle covers a different relation.
  - Admission asymmetry is modeled faithfully: the waiver checks whether
    the ACTIVE intent is an ancestor of the CANDIDATE (as the runtime
    does), not the symmetric closure.
  - The int32 ABI uses nonrecycling authority-bearing handles, followed by
    explicit exhaustion. Registry storage may be reused, but numerical public
    identities may not: stale public exit/trace calls carry no generation.
    The executable walk fails closed on retired parents and nondecreasing links.
    INT-4 static co-activity and a full C source refinement remain separate.
*)

Require Import Stdlib.Lists.List.
Require Import Stdlib.Arith.PeanoNat.
Require Import Stdlib.Bool.Bool.
Require Import Stdlib.micromega.Lia.
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

(* Admitted runtime nesting, NOT an unchecked lexical relation. *)
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

(* The common runtime issuance owner has an absorbing exhausted sentinel.
   Trace counters are observational and deliberately not this operation. *)
Definition issue_handle (limit next : nat) : nat * nat :=
  if andb (0 <? next) (next <=? limit) then
    (next, if next =? limit then 0 else S next)
  else (0, next).

Theorem exhausted_handle_space_stays_exhausted : forall limit,
  issue_handle limit 0 = (0,0).
Proof. reflexivity. Qed.

Theorem issued_identity_advances_or_exhausts : forall limit next h after,
  issue_handle limit next = (h,after) -> h <> 0 ->
  h = next /\ h <= limit /\ (after = 0 \/ h < after).
Proof.
  intros limit next h after H Hnonzero. unfold issue_handle in H.
  destruct (andb (0 <? next) (next <=? limit)) eqn:E; [| inversion H; congruence].
  apply Bool.andb_true_iff in E. destruct E as [_ E]. apply Nat.leb_le in E.
  destruct (next =? limit); inversion H; subst.
  - split; [reflexivity |]. split; [exact E |]. left. reflexivity.
  - split; [reflexivity |]. split; [exact E |]. right. lia.
Qed.

Record LiveIntent := { live_decl : IntentDecl; live_parent : nat }.

Fixpoint lookup_live (live : list LiveIntent) (h : nat) : option LiveIntent :=
  match live with
  | [] => None
  | a :: rest => if d_handle (live_decl a) =? h then Some a else lookup_live rest h
  end.

Fixpoint ancestor_walk (fuel : nat) (live : list LiveIntent)
    (cursor target : nat) : bool :=
  match fuel with
  | 0 => false
  | S rest =>
      match lookup_live live cursor with
      | None => false
      | Some a =>
          if cursor =? target then true
          else if live_parent a <? cursor then
            ancestor_walk rest live (live_parent a) target else false
      end
  end.

Definition runtime_ancestor (live : list LiveIntent) (current target : nat) : bool :=
  ancestor_walk (length live) live current target.

Definition registry_conflict_guard (live : list LiveIntent) (current : nat) :=
  conflict_guard (fun h _ => runtime_ancestor live current h = true).

Lemma lookup_live_has_identity : forall live h a,
  lookup_live live h = Some a -> In a live /\ d_handle (live_decl a) = h.
Proof.
  induction live as [| x rest IH]; intros h a H; simpl in H; [discriminate |].
  destruct (d_handle (live_decl x) =? h) eqn:E.
  - inversion H; subst. apply Nat.eqb_eq in E.
    split; [left; reflexivity | exact E].
  - destruct (IH h a H) as [Hin Heq]. split; [right; exact Hin | exact Heq].
Qed.

Theorem ancestor_waiver_requires_live_identity : forall fuel live current target,
  ancestor_walk fuel live current target = true ->
  exists a, In a live /\ d_handle (live_decl a) = target.
Proof.
  induction fuel as [| fuel IH]; intros live current target H; [discriminate |].
  simpl in H. destruct (lookup_live live current) as [a |] eqn:E; [| discriminate].
  destruct (current =? target) eqn:Eq.
  - apply Nat.eqb_eq in Eq. subst. exists a. apply lookup_live_has_identity in E. exact E.
  - destruct (live_parent a <? current); [eapply IH; exact H | discriminate].
Qed.

Theorem retired_parent_is_not_a_waiver : forall live current target,
  (forall a, In a live -> d_handle (live_decl a) <> target) ->
  runtime_ancestor live current target = false.
Proof.
  intros live current target Hnot. unfold runtime_ancestor.
  destruct (ancestor_walk (length live) live current target) eqn:E; [| reflexivity].
  destruct (ancestor_waiver_requires_live_identity _ _ _ _ E) as [a [Hin Heq]].
  exfalso. exact (Hnot a Hin Heq).
Qed.

(* The multi-step registry identity rule. high is the largest public identity
   ever issued, including retired entries. Leave never rolls that frontier back.
   The concrete common C owner encodes `high = limit` as next_handle = 0. *)
Record RegistryState := {
  registry_live : list LiveIntent;
  registry_high : nat
}.

Inductive identity_step (limit : nat) : RegistryState -> RegistryState -> Prop :=
| IdentityEnter : forall live high d parent,
    d_handle d = S high -> S high <= limit -> parent <= high ->
    identity_step limit {| registry_live := live; registry_high := high |}
      {| registry_live := {| live_decl := d; live_parent := parent |} :: live;
         registry_high := S high |}
| IdentityLeave : forall live high h,
    identity_step limit {| registry_live := live; registry_high := high |}
      {| registry_live := filter (fun a => negb (d_handle (live_decl a) =? h)) live;
         registry_high := high |}.

Lemma issue_handle_implements_identity_enter : forall limit live high d parent,
  high < limit -> parent <= high ->
  d_handle d = fst (issue_handle limit (S high)) ->
  identity_step limit {| registry_live := live; registry_high := high |}
    {| registry_live := {| live_decl := d; live_parent := parent |} :: live;
       registry_high := S high |}.
Proof.
  intros limit live high d parent Hroom Hparent Hid.
  assert (Epositive : (0 <? S high) = true) by (apply Nat.ltb_lt; lia).
  assert (Eroom : (S high <=? limit) = true) by (apply Nat.leb_le; lia).
  unfold issue_handle in Hid. rewrite Epositive, Eroom in Hid. simpl in Hid.
  apply IdentityEnter; [exact Hid | lia | exact Hparent].
Qed.

(* This projection deliberately overapproximates admission: subject/conflict
   rejection can remove enter steps, never authorize identity recycling. *)
Inductive identity_run (limit : nat) : RegistryState -> RegistryState -> Prop :=
| IdentityNil : forall r, identity_run limit r r
| IdentityCons : forall before mid after,
    identity_step limit before mid -> identity_run limit mid after ->
    identity_run limit before after.

Lemma registry_frontier_monotone : forall limit before after,
  identity_run limit before after -> registry_high before <= registry_high after.
Proof.
  intros limit before after H. induction H; [lia |].
  inversion H; subst; simpl in *; lia.
Qed.

Theorem old_public_identity_never_reissued : forall limit before after old live high d parent,
  identity_run limit before after -> old <= registry_high before ->
  after = {| registry_live := live; registry_high := high |} ->
  identity_step limit after
    {| registry_live := {| live_decl := d; live_parent := parent |} :: live;
       registry_high := S high |} -> d_handle d <> old.
Proof.
  intros limit before after old live high d parent Hrun Hold Hafter Hstep.
  pose proof (registry_frontier_monotone _ _ _ Hrun) as Hmono.
  subst after. inversion Hstep; subst; simpl in *; lia.
Qed.

Example non_lifo_retirement_does_not_bind_new_entry :
  let child := {| live_decl := {| d_handle := 2; d_subjects := [7];
      d_concurrent := false; d_priority := 0 |}; live_parent := 1 |} in
  let unrelated := {| live_decl := {| d_handle := 3; d_subjects := [9];
      d_concurrent := false; d_priority := 0 |}; live_parent := 0 |} in
  runtime_ancestor [unrelated; child] 2 3 = false /\
  runtime_ancestor [unrelated; child] 2 1 = false /\
  runtime_ancestor [unrelated; child] 2 2 = true.
Proof. repeat split; reflexivity. Qed.

Print Assumptions old_public_identity_never_reissued.
Print Assumptions ancestor_waiver_requires_live_identity.

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
