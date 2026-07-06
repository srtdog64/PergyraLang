(*
  Pergyra Formal Semantics - Mechanized Sketch
  Target: docs/173 SS0-b + INT-5 -- the intent fact kernel.
  Status: proof-sketch; not beta-closure evidence unless checked by CI (coqc).

  docs/173 SS0-b bans a single monolithic `Intent` fact: the surface binder
  elaborates into per-family owner facts, and the M1 claim lives per fact.
  This file mechanizes that refinement:

    (1) Subfact kernels for the three obligation-bearing families this
        file owns -- IntentParticipantFact (INT-1 declared >= used),
        IntentCoordinationFact (INT-3 dependency order), and
        IntentCompensationFact (INT-2 coverage). The other verifier
        families already have their own mechanized cores and are NOT
        duplicated here: Boundary = ZoneCrossingCore.v, Authority =
        AuthorityDelegationCore.v, Effect = EffectAuthorityCore.v.
    (2) The spine: one identity binding the families. Composition
        theorems `one_intent_from_facts` / `intent_determined_by_facts`
        state that the separately-emitted families, joined on the shared
        spine id, reassemble into exactly one intent -- the subfacts
        BECOME one intent, and an intent is NOTHING BEYOND its families
        (no hidden monolith).
    (3) `checked_intent_guard_free` (the docs/173 INT-5 target): an
        intent whose static obligations hold never fires the runtime
        guards (undeclared-participant, missing-compensation, stuck-step)
        along any coordination-faithful schedule. The guards exist for
        UNCHECKED programs (non-vacuity lemmas); for checked intents they
        are erasable/amortizable (docs/142).
    (4) `no_dep_cycle`: coordination facts of a checked intent admit no
        dependency cycle -- the static exclusion, at the intent level, of
        the livelock class fixed at runtime in commit fe70f180 (F1).
    (5) `library_bucket_obligation_free`: the library-expressible bucket
        (Purpose/Trace payloads, modeled as the spine note) carries NO
        verifier obligation -- checking is invariant under any note. The
        6/2 bucket split of docs/173 SS0-b, as a theorem.

  Modeling notes (honest scope):
  - Dependencies are topologically numbered (prerequisite < dependent),
    the same generality-preserving trick as BasisCompleteness.v: every
    finite DAG admits such a numbering, and it carries acyclicity.
  - `sched_ok` models the codegen obligation (AIRBinding lineage): the
    emitter orders steps by the CoordinationFact and only compensates
    executed steps. Faithfulness of the real emitter to `sched_ok` is a
    gate obligation, not proven here.
  - The per-step used-participant set is taken as given (the closed set
    AFTER interprocedural propagation -- computing it is the semantic
    pass's job, capability-pass lineage).
  - Cross-intent conflict (INT-4) needs a multi-spine registry model and
    is the next rung (docs/167 B axis), not claimed here.
*)

Require Import Coq.Lists.List.
Require Import Lia.
Import ListNotations.

(* ====================================================== *)
(* 1. Subfact kernels (verifier bucket, docs/173 SS0-b)    *)
(* ====================================================== *)

Definition participant := nat.
Definition stepid := nat.

(* IntentParticipantFact: header-declared participants and the per-step
   used set (step clauses using:/who:/where:, after interprocedural
   closure). *)
Record ParticipantFact := {
  pf_declared : list participant;
  pf_used     : stepid -> list participant
}.

(* IntentCoordinationFact: declared dependencies (dependent, prerequisite). *)
Record CoordinationFact := {
  cf_deps : list (stepid * stepid)
}.

(* IntentCompensationFact: effectful steps and their compensation
   binding or explicit irreversible marker. *)
Record CompensationFact := {
  mf_effectful    : stepid -> bool;
  mf_comp         : stepid -> bool;
  mf_irreversible : stepid -> bool
}.

(* ====================================================== *)
(* 2. The spine: one identity binding the families         *)
(* ====================================================== *)

(* spine_note models the library bucket (Purpose/Trace payload): bound to
   the same identity, carrying no verifier obligation -- see
   library_bucket_obligation_free. *)
Record IntentSpine := {
  spine_id    : nat;
  spine_note  : nat;
  spine_steps : list stepid;
  spine_pf    : ParticipantFact;
  spine_cf    : CoordinationFact;
  spine_mf    : CompensationFact
}.

(* ====================================================== *)
(* 3. Static obligations (the INT rungs, per family)       *)
(* ====================================================== *)

(* INT-1: declared >= used, for every step of the intent. *)
Definition participants_covered (s : IntentSpine) : Prop :=
  forall st p,
    In st (spine_steps s) ->
    In p (pf_used (spine_pf s) st) ->
    In p (pf_declared (spine_pf s)).

(* INT-3: dependencies are topologically numbered (prerequisite strictly
   below dependent) -- the acyclicity carrier. *)
Definition deps_wf (s : IntentSpine) : Prop :=
  forall a b, In (a, b) (cf_deps (spine_cf s)) -> b < a.

(* INT-2: every effectful step has a compensation binding or is
   explicitly irreversible (fail-closed coverage). *)
Definition comp_covered (s : IntentSpine) : Prop :=
  forall st,
    In st (spine_steps s) ->
    mf_effectful (spine_mf s) st = true ->
    mf_comp (spine_mf s) st = true \/
    mf_irreversible (spine_mf s) st = true.

Definition intent_checked (s : IntentSpine) : Prop :=
  participants_covered s /\ deps_wf s /\ comp_covered s.

(* ====================================================== *)
(* 4. Runtime guards and coordination-faithful schedules   *)
(* ====================================================== *)

Inductive act : Type :=
| ARun  (st : stepid)
| AComp (st : stepid).

(* Guard: step touches a participant the intent never declared. *)
Definition guard_undeclared (s : IntentSpine) (a : act) : Prop :=
  match a with
  | ARun st => exists p, In p (pf_used (spine_pf s) st)
                    /\ ~ In p (pf_declared (spine_pf s))
  | AComp _ => False
  end.

(* Guard: rollback demanded on an effectful step with no compensation
   and no irreversible declaration. *)
Definition guard_missing_comp (s : IntentSpine) (a : act) : Prop :=
  match a with
  | AComp st => mf_effectful (spine_mf s) st = true
                /\ mf_comp (spine_mf s) st = false
                /\ mf_irreversible (spine_mf s) st = false
  | ARun _ => False
  end.

(* Guard: step runs before a declared prerequisite completed. *)
Definition guard_stuck (s : IntentSpine) (done : list stepid) (a : act) : Prop :=
  match a with
  | ARun st => exists pre, In (st, pre) (cf_deps (spine_cf s)) /\ ~ In pre done
  | AComp _ => False
  end.

(* Coordination-faithful schedule: the emitter runs steps only after
   their declared prerequisites and compensates only executed steps.
   This is what codegen derives from the CoordinationFact. *)
Inductive sched_ok (s : IntentSpine) : list stepid -> list act -> Prop :=
| sched_nil : forall done, sched_ok s done []
| sched_run : forall done st rest,
    (forall pre, In (st, pre) (cf_deps (spine_cf s)) -> In pre done) ->
    In st (spine_steps s) ->
    sched_ok s (st :: done) rest ->
    sched_ok s done (ARun st :: rest)
| sched_comp : forall done st rest,
    In st done ->
    In st (spine_steps s) ->
    sched_ok s done rest ->
    sched_ok s done (AComp st :: rest).

(* Guard-freedom along a schedule, action by action. *)
Inductive no_guard_fires (s : IntentSpine) : list stepid -> list act -> Prop :=
| ngf_nil : forall done, no_guard_fires s done []
| ngf_run : forall done st rest,
    ~ guard_undeclared s (ARun st) ->
    ~ guard_stuck s done (ARun st) ->
    no_guard_fires s (st :: done) rest ->
    no_guard_fires s done (ARun st :: rest)
| ngf_comp : forall done st rest,
    ~ guard_missing_comp s (AComp st) ->
    no_guard_fires s done rest ->
    no_guard_fires s done (AComp st :: rest).

(* ====================================================== *)
(* 5. THE theorem (INT-5): checked intents run guard-free  *)
(* ====================================================== *)

Theorem checked_intent_guard_free :
  forall s, intent_checked s ->
  forall done acts, sched_ok s done acts -> no_guard_fires s done acts.
Proof.
  intros s [Hpart [Hdeps Hcomp]] done acts H.
  induction H as [ done
                 | done st rest Hpre Hst Hrest IH
                 | done st rest Hdone Hst Hrest IH ].
  - constructor.
  - constructor.
    + intros [p [Hin Hnot]]. apply Hnot. eapply Hpart; eauto.
    + intros [pre [Hdep Hnd]]. apply Hnd. apply Hpre. exact Hdep.
    + exact IH.
  - constructor.
    + intros [Heff [Hc Hi]].
      destruct (Hcomp st Hst Heff) as [Hc' | Hi']; congruence.
    + exact IH.
Qed.

(* Non-vacuity: the guards are real for UNCHECKED intents (fail-closed
   is not decorative). *)
Lemma uncovered_comp_guard_fires :
  forall s st,
    mf_effectful (spine_mf s) st = true ->
    mf_comp (spine_mf s) st = false ->
    mf_irreversible (spine_mf s) st = false ->
    guard_missing_comp s (AComp st).
Proof. intros. simpl. repeat split; assumption. Qed.

Lemma undeclared_guard_fires :
  forall s st p,
    In p (pf_used (spine_pf s) st) ->
    ~ In p (pf_declared (spine_pf s)) ->
    guard_undeclared s (ARun st).
Proof. intros. simpl. exists p. auto. Qed.

(* ====================================================== *)
(* 6. Coordination acyclicity (the F1 class, statically)   *)
(* ====================================================== *)

Inductive dep_path (D : list (stepid * stepid)) : stepid -> stepid -> Prop :=
| dp_one  : forall a b, In (a, b) D -> dep_path D a b
| dp_more : forall a b c, In (a, b) D -> dep_path D b c -> dep_path D a c.

Lemma dep_path_decreases :
  forall (D : list (stepid * stepid)),
    (forall a b, In (a, b) D -> b < a) ->
    forall a b, dep_path D a b -> b < a.
Proof.
  intros D HD a b Hp.
  induction Hp.
  - apply HD; assumption.
  - apply HD in H. lia.
Qed.

(* A checked intent's coordination facts admit no dependency cycle:
   the intent-level static exclusion of the parent/dependency livelock
   class that F1 (commit fe70f180) bounded at runtime. *)
Theorem no_dep_cycle :
  forall s, deps_wf s ->
  forall a, ~ dep_path (cf_deps (spine_cf s)) a a.
Proof.
  intros s Hwf a Hp.
  apply dep_path_decreases in Hp; [lia | exact Hwf].
Qed.

(* ====================================================== *)
(* 7. Composition: the subfacts BECOME one intent          *)
(* ====================================================== *)

(* Emission produces SEPARATE per-family facts, each stamped with the
   spine id -- there is no monolithic Intent fact (docs/173 SS0-b). *)
Definition emit_steps         (s : IntentSpine) := (spine_id s, spine_steps s).
Definition emit_participant   (s : IntentSpine) := (spine_id s, spine_pf s).
Definition emit_coordination  (s : IntentSpine) := (spine_id s, spine_cf s).
Definition emit_compensation  (s : IntentSpine) := (spine_id s, spine_mf s).
Definition emit_note          (s : IntentSpine) := (spine_id s, spine_note s).

(* Attribution: every family of one spine carries the same identity --
   the binder's irreducible contribution. *)
Theorem facts_share_spine :
  forall s,
    fst (emit_steps s) = spine_id s /\
    fst (emit_participant s) = spine_id s /\
    fst (emit_coordination s) = spine_id s /\
    fst (emit_compensation s) = spine_id s /\
    fst (emit_note s) = spine_id s.
Proof. intros s. repeat split. Qed.

Definition reassemble
  (ns : nat * list stepid)
  (np : nat * ParticipantFact)
  (nc : nat * CoordinationFact)
  (nm : nat * CompensationFact)
  (nn : nat * nat) : IntentSpine :=
  {| spine_id    := fst ns;
     spine_note  := snd nn;
     spine_steps := snd ns;
     spine_pf    := snd np;
     spine_cf    := snd nc;
     spine_mf    := snd nm |}.

(* The separately-emitted families, joined on the shared id, reassemble
   into exactly the original intent. *)
Theorem one_intent_from_facts :
  forall s,
    reassemble (emit_steps s) (emit_participant s)
               (emit_coordination s) (emit_compensation s)
               (emit_note s) = s.
Proof. intros [id note steps pf cf mf]. reflexivity. Qed.

(* And the families determine the intent uniquely: an intent is nothing
   beyond its families (no hidden monolithic content). *)
Theorem intent_determined_by_facts :
  forall s1 s2,
    emit_steps s1 = emit_steps s2 ->
    emit_participant s1 = emit_participant s2 ->
    emit_coordination s1 = emit_coordination s2 ->
    emit_compensation s1 = emit_compensation s2 ->
    emit_note s1 = emit_note s2 ->
    s1 = s2.
Proof.
  intros [i1 n1 st1 p1 c1 m1] [i2 n2 st2 p2 c2 m2] H1 H2 H3 H4 H5.
  unfold emit_steps, emit_participant, emit_coordination,
         emit_compensation, emit_note in *.
  simpl in *.
  inversion H1. inversion H2. inversion H3. inversion H4. inversion H5.
  subst. reflexivity.
Qed.

(* ====================================================== *)
(* 8. The library bucket carries no obligation             *)
(* ====================================================== *)

Definition with_note (s : IntentSpine) (n : nat) : IntentSpine :=
  {| spine_id    := spine_id s;
     spine_note  := n;
     spine_steps := spine_steps s;
     spine_pf    := spine_pf s;
     spine_cf    := spine_cf s;
     spine_mf    := spine_mf s |}.

(* Changing the Purpose/Trace payload cannot affect checking: the 6/2
   verifier/library bucket split of docs/173 SS0-b, as a theorem. *)
Theorem library_bucket_obligation_free :
  forall s n, intent_checked s <-> intent_checked (with_note s n).
Proof.
  intros s n.
  unfold intent_checked, participants_covered, deps_wf, comp_covered.
  simpl. tauto.
Qed.
