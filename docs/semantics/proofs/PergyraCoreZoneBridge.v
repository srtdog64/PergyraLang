(*
  Pergyra Formal Semantics -- Zone corner, re-derived on the shared core.
  Status: kernel-verified under Coq 8.18 (coqc + coqchk): compiles, closes with
  Qed, and adds 0 axioms -- the budget stays at SlotCalculus's two declared
  abstractions. Rocq 9.0.1 in CI remains the authority; 8.18 accepts the `Coq.`
  namespace prefix that Rocq 9 deprecates, so it cannot speak for that.

  ZoneCrossingCore.v proves three obligations (no ambient authority, capability
  soundness of a crossing, fail-closed) on its OWN 2-field machine
  (config := held authority + residence), disconnected from every other proof.
  This file re-derives those same guarantees from PergyraCore's richer shared
  machine, restricted to the ActCross step -- so the zone corner's guarantees
  become corollaries of the one abstract machine the whole corpus is migrating
  onto, not a private island.

  It is ADDITIVE: ZoneCrossingCore.v is left untouched (its model is a different,
  smaller config, so folding it in is a refinement, not a rename -- that rewrite
  is the next step once a prover is in the loop). This bridge establishes the
  corner -> shared-root edge now, without risking a currently-green file.

  What holds on PergyraCore.step at ActCross:
    - cross_preserves_holdings / _actor: a crossing changes only residence.
    - cross_no_ambient_authority: no capability is gained by moving.
    - cross_residence_authorized: the entered zone witnesses its entry cap.
    - cross_fail_closed: without the entry cap, no crossing is derivable.
*)

Require Import Coq.Lists.List.
Require Import PergyraCore.
Import ListNotations.

(* A crossing changes only residence: authority distribution and the acting
   principal are invariant (the "no ambient authority" core). *)
Lemma cross_preserves_holdings : forall gz ge ga ct a b z',
  step gz ge ga ct (ActCross z') a b -> holdings b = holdings a.
Proof.
  intros gz ge ga ct a b z' H. inversion H; subst.
  unfold with_zone. reflexivity.
Qed.

Lemma cross_preserves_actor : forall gz ge ga ct a b z',
  step gz ge ga ct (ActCross z') a b -> actor b = actor a.
Proof.
  intros gz ge ga ct a b z' H. inversion H; subst.
  unfold with_zone. reflexivity.
Qed.

(* No ambient authority: any capability the actor holds after a crossing was
   already held before it. Mirrors ZoneCrossingCore.no_ambient_authority, now on
   the shared machine's has_cap (In k (holdings c (actor c))). *)
Theorem cross_no_ambient_authority : forall gz ge ga ct a b z' k,
  step gz ge ga ct (ActCross z') a b -> has_cap b k -> has_cap a k.
Proof.
  intros gz ge ga ct a b z' k H Hk. unfold has_cap in *.
  rewrite (cross_preserves_holdings gz ge ga ct a b z' H) in Hk.
  rewrite (cross_preserves_actor gz ge ga ct a b z' H) in Hk.
  exact Hk.
Qed.

(* The crossing witnesses the entry capability the ZoneGraph requires. *)
Lemma cross_witnesses_cap : forall gz ge ga ct a b z',
  step gz ge ga ct (ActCross z') a b -> has_cap a (gz z').
Proof.
  intros gz ge ga ct a b z' H. inversion H; subst. assumption.
Qed.

(* Capability soundness: the resulting residence is authorized -- the actor holds
   the entry cap for the zone it now resides in. Mirrors
   ZoneCrossingCore.crossing_capability_sound. *)
Theorem cross_residence_authorized : forall gz ge ga ct a b z',
  step gz ge ga ct (ActCross z') a b -> has_cap a (gz (here b)).
Proof.
  intros gz ge ga ct a b z' H. inversion H; subst.
  unfold with_zone; simpl. assumption.
Qed.

(* Fail-closed: if the actor does not hold the entry cap for z', no crossing into
   z' is derivable -- the machine is stuck rather than entering unauthorized.
   Mirrors ZoneCrossingCore.fail_closed_crossing. *)
Theorem cross_fail_closed : forall gz ge ga ct a z',
  ~ has_cap a (gz z') ->
  ~ (exists b, step gz ge ga ct (ActCross z') a b).
Proof.
  intros gz ge ga ct a z' Hno [b Hstep].
  apply Hno. exact (cross_witnesses_cap gz ge ga ct a b z' Hstep).
Qed.
