(*
  Pergyra Formal Semantics -- First composition over the shared core.
  Status: kernel-verified under Coq 8.18 (coqc + coqchk): compiles, closes with
  Qed, and adds 0 axioms -- the budget stays at SlotCalculus's two declared
  abstractions. Rocq 9.0.1 in CI remains the authority; 8.18 accepts the `Coq.`
  namespace prefix that Rocq 9 deprecates, so it cannot speak for that.

  This is the corpus's FIRST cross-file proof edge. Until now every .v Required
  only the Coq standard library, so no theorem built on another file's. Here the
  theorems are proved entirely in terms of PergyraCore's `step` / `steps`
  relation and its state constructors, imported -- not re-declared -- from
  PergyraCore.v. It demonstrates the vertical-composition pattern the rest of
  the migration follows: downstream reasoning stands on the one shared abstract
  machine rather than a private copy.

  What it proves: on the shared machine, an Empty slot the actor is authorised
  for can be acquired and then used, and acquire -> release is a valid two-step
  run. These are small, but they are genuine multi-step compositions of the
  imported step relation, not restatements.
*)

Require Import Coq.Lists.List.
Require Import Coq.Arith.PeanoNat.
Require Import PergyraCore.
Import ListNotations.

(* After filling slot s, the shared store reads Filled at s. Isolated so the
   step compositions below can discharge each SUse/SRelease typestate premise by
   one lemma application instead of re-unfolding the store map every time. *)
Lemma store_after_fill : forall c s,
  store (with_store c s Filled) s = Filled.
Proof.
  intros c s. unfold with_store, smap. simpl.
  rewrite Nat.eqb_refl. reflexivity.
Qed.

(* Composition 1: authority + Empty typestate is enough to acquire s and then
   use it -- the SUse premise is furnished by the SAcquire result state. *)
Theorem acquire_then_use : forall gz ge ga ct c s,
  has_cap c (ga s) ->
  store c s = Empty ->
  exists c1,
    step gz ge ga ct (ActAcquire s) c c1 /\
    step gz ge ga ct (ActUse s) c1 c1.
Proof.
  intros gz ge ga ct c s Hcap Hempty.
  exists (with_store c s Filled). split.
  - apply SAcquire; assumption.
  - apply SUse. apply store_after_fill.
Qed.

(* Composition 2: acquire -> release is a valid run of the shared multi-step
   relation, ending in the Released typestate. Chains two `step` edges through
   `steps`, discharging SRelease's Filled premise from the acquire result. *)
Theorem acquire_then_release_steps : forall gz ge ga ct c s,
  has_cap c (ga s) ->
  store c s = Empty ->
  steps gz ge ga ct c
        (with_store (with_store c s Filled) s Released).
Proof.
  intros gz ge ga ct c s Hcap Hempty.
  (* `eapply` leaves the intermediate configuration as an evar, and `assumption`
     will not instantiate one -- `eassumption` does. *)
  eapply SStep.
  - apply SAcquire; eassumption.
  - eapply SStep.
    + apply SRelease. apply store_after_fill.
    + apply SRefl.
Qed.
