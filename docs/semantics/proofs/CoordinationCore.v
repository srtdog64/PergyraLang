(*
  Pergyra Formal Semantics -- Mechanized Fragment (coordination corner)
  Target: docs/semantics/19 "Pergyra Abstract Machine Obligation" (intent facet)
  Status: proof-sketch; not beta-closure evidence unless checked by CI (coqc).

  Scope: the coordination Step form -- the other intent-specific facet (the step
  dependency graph). Dataflow / Kahn Process Network lineage (docs/19 intent row):
  a step becomes ready only when all its dependencies are done, and execution
  records it as done. This replaces the position-ordered "sequence" view of intent
  steps (the CPU-sequential assumption flagged in docs/18) with an explicit
  partial order / readiness model.

  Mechanized obligations:
    - Readiness soundness / fail-closed: a step runs only when every dependency is
      already done (`run_requires_deps`) -- no step executes before its
      prerequisites.
    - Coordination determinism invariant (the KPN/dataflow core): any reachable
      done-set is dependency-closed -- a completed step always has all of its
      dependencies completed (`reachable_dep_closed`). The execution order respects
      the dependency graph regardless of which ready step is picked.

  Negative scope: a flat done-set + a static dependency map; no data values on the
  edges, no per-step effect/authority gating (that is the other corners), no
  binding to live AIR/MIR intent-step facts yet (task #45 / docs/18).
*)

Require Import Coq.Lists.List.
Import ListNotations.

Section CoordinationCore.

Definition step := nat.

(* The dependency graph: each step's prerequisite steps. *)
Definition deps := step -> list step.

(* The set of completed steps. *)
Definition done_set := list step.

(* A step is ready when all its dependencies are already done. *)
Definition ready (d : deps) (done : done_set) (s : step) : Prop :=
  forall x, In x (d s) -> In x done.

(* A coordination step: run a ready step, recording it as done. Fail-closed:
   there is no rule that runs a step before its dependencies. *)
Inductive crun (d : deps) : done_set -> done_set -> Prop :=
| Run : forall done s,
    ready d done s ->
    crun d done (s :: done).

Inductive cruns (d : deps) : done_set -> done_set -> Prop :=
| CRefl : forall done, cruns d done done
| CStep : forall a b c, crun d a b -> cruns d b c -> cruns d a c.

(* ---- readiness soundness / fail-closed ---- *)

Theorem run_requires_deps : forall d done s,
  crun d done (s :: done) -> ready d done s.
Proof. intros d done s H. inversion H; subst. assumption. Qed.

(* ---- dependency-closure invariant ---- *)

Definition dep_closed (d : deps) (done : done_set) : Prop :=
  forall s, In s done -> forall x, In x (d s) -> In x done.

Lemma empty_closed : forall d, dep_closed d [].
Proof. intros d s Hs. inversion Hs. Qed.

Lemma crun_preserves_closure : forall d done done',
  dep_closed d done -> crun d done done' -> dep_closed d done'.
Proof.
  intros d done done' Hcl H. inversion H; subst.
  unfold dep_closed in *. intros s0 Hs0 x Hx.
  simpl in Hs0. destruct Hs0 as [Heq | Hs0].
  - (* s0 is the newly run step: its deps are in done (readiness), hence in s::done *)
    subst s0. right. apply H0. exact Hx.
  - (* s0 already done: deps in done by the invariant, hence in s::done *)
    right. apply (Hcl s0 Hs0 x Hx).
Qed.

Lemma cruns_preserve_closure : forall d done done',
  dep_closed d done -> cruns d done done' -> dep_closed d done'.
Proof.
  intros d done done' Hcl H. induction H.
  - exact Hcl.
  - apply IHcruns. apply (crun_preserves_closure d a b Hcl H).
Qed.

(* MAIN: any execution reachable from the empty schedule is dependency-closed --
   no step is ever completed without all of its dependencies completed first. *)
Theorem reachable_dep_closed : forall d done,
  cruns d [] done -> dep_closed d done.
Proof.
  intros d done H.
  apply (cruns_preserve_closure d [] done (empty_closed d) H).
Qed.

End CoordinationCore.
