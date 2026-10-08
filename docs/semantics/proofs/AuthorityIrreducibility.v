(*
  Pergyra Formal Semantics - Mechanized Sketch
  Target: distinguish a free grant-history field from a capability projection
  in an unrestricted record model; not a language-axis irreducibility proof.
  Status: proof-sketch; not beta-closure evidence unless checked by CI (coqc).

  semantics/22 rated the authority axis "partial" because its independent
  non-expressibility argument was unresolved: perhaps the authority verdict
  is a function of the capability facts and the zone facts, making the axis
  derived notation. This file refutes the reduction in the smallest honest
  unrestricted model (its examples need not be valid granted configurations):

    (1) `delegation_distinguishes`: two configurations with IDENTICAL
        capability and zone projections but different authority verdicts
        -- the delegation chain is the distinguishing, load-bearing fact.
    (2) `authority_beyond_cap_zone`: therefore NO function of
        (capability, zone) computes the authority verdict on ALL arbitrary
        records. This says nothing about the grant-consistent subset.

  Lineage: the delegation DYNAMICS (holdings, no_privilege_escalation)
  are owned by AuthorityDelegationCore.v; FormalKernel.v separates
  authority from effect (authority_effect_not_aliases). This file owns
  only the cap x zone irreducibility claim.

  Honest scope: unrestricted record separation (a counterexample pair), not a
  Felleisen macro-expressibility theorem. The model's authority verdict
  is delegation reachability from a designated root -- the minimal core
  of "authorized by". If capability is defined to follow the root's grants,
  its projection already computes this verdict; the old pair cannot justify
  upgrading authority's language expressibility rating.
*)

Require Import Stdlib.Lists.List.
Import ListNotations.

Record Config := {
  cap  : nat -> bool;        (* capability mask per actor  *)
  zone : nat -> nat;         (* zone assignment per actor  *)
  dele : list (nat * nat)    (* delegation grants: from -> to *)
}.

(* Authority verdict: the root's authority reaches the actor through the
   delegation chain (AuthorityDelegationCore lineage, reachability core). *)
Inductive reach (D : list (nat * nat)) : nat -> nat -> Prop :=
| reach_refl : forall x, reach D x x
| reach_step : forall x y z, In (x, y) D -> reach D y z -> reach D x z.

Definition authorized (c : Config) (root actor : nat) : Prop :=
  reach (dele c) root actor.

(* Two configurations, identical in every capability and zone fact,
   differing only in the delegation chain. *)
Definition c_granted : Config :=
  {| cap := fun _ => true; zone := fun _ => 0; dele := [(0, 1)] |}.

Definition c_ungranted : Config :=
  {| cap := fun _ => true; zone := fun _ => 0; dele := [] |}.

Lemma granted_authorized : authorized c_granted 0 1.
Proof.
  unfold authorized. simpl.
  eapply reach_step.
  - simpl. left. reflexivity.
  - constructor.
Qed.

(* This is an explicit interface consistency contract, not a preservation
   result for AuthorityDelegationCore or the runtime. *)
Definition grant_consistent (c : Config) (root : nat) : Prop :=
  forall actor, cap c actor = true <-> authorized c root actor.

Theorem consistent_authority_is_cap_projection : forall c root actor,
  grant_consistent c root ->
  authorized c root actor <-> cap c actor = true.
Proof.
  intros c root actor Hconsistent. symmetry. apply Hconsistent.
Qed.

Lemma ungranted_not_authorized : ~ authorized c_ungranted 0 1.
Proof.
  unfold authorized. simpl. intros H.
  inversion H as [| x y z Hin Hr]; subst.
  simpl in Hin. destruct Hin.
Qed.

Theorem old_ungranted_pair_is_inconsistent : ~ grant_consistent c_ungranted 0.
Proof.
  intros Hconsistent. apply ungranted_not_authorized.
  apply (proj1 (Hconsistent 1)). reflexivity.
Qed.

(* (1) The distinguishing pair: same capability facts, same zone facts,
   different authority verdict. *)
Theorem delegation_distinguishes :
  cap c_granted = cap c_ungranted
  /\ zone c_granted = zone c_ungranted
  /\ authorized c_granted 0 1
  /\ ~ authorized c_ungranted 0 1.
Proof.
  split; [reflexivity | split; [reflexivity | split]].
  - exact granted_authorized.
  - exact ungranted_not_authorized.
Qed.

(* (2) No such function exists on unrestricted arbitrary records. This
   counterexample is NOT evidence about the grant-consistent language. *)
Theorem authority_beyond_cap_zone :
  ~ (exists F : (nat -> bool) -> (nat -> nat) -> nat -> nat -> Prop,
       forall c root actor,
         authorized c root actor <-> F (cap c) (zone c) root actor).
Proof.
  intros [F HF].
  assert (H1 : F (cap c_granted) (zone c_granted) 0 1).
  { apply HF. exact granted_authorized. }
  change (cap c_granted) with (cap c_ungranted) in H1.
  change (zone c_granted) with (zone c_ungranted) in H1.
  apply ungranted_not_authorized.
  apply HF. exact H1.
Qed.
