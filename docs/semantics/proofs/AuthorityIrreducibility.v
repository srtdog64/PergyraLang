(*
  Pergyra Formal Semantics - Mechanized Sketch
  Target: docs/semantics/22 SS1.5 -- discharge the "authority is just
  capability x zone notation" reduction objection, at the model level.
  Status: proof-sketch; not beta-closure evidence unless checked by CI (coqc).

  semantics/22 rated the authority axis "partial" because its independent
  non-expressibility argument was unresolved: perhaps the authority verdict
  is a function of the capability facts and the zone facts, making the axis
  derived notation. This file refutes the reduction in the smallest honest
  model:

    (1) `delegation_distinguishes`: two configurations with IDENTICAL
        capability and zone projections but different authority verdicts
        -- the delegation chain is the distinguishing, load-bearing fact.
    (2) `authority_beyond_cap_zone`: therefore NO function of
        (capability, zone) computes the authority verdict.

  Lineage: the delegation DYNAMICS (holdings, no_privilege_escalation)
  are owned by AuthorityDelegationCore.v; FormalKernel.v separates
  authority from effect (authority_effect_not_aliases). This file owns
  only the cap x zone irreducibility claim.

  Honest scope: model-level separation (a counterexample pair), not a
  Felleisen macro-expressibility theorem. The model's authority verdict
  is delegation reachability from a designated root -- the minimal core
  of "authorized by": authority is a HISTORY of grants, and no snapshot
  of who-holds-what-capability-where reconstructs the grant graph.
*)

Require Import Coq.Lists.List.
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

Lemma ungranted_not_authorized : ~ authorized c_ungranted 0 1.
Proof.
  unfold authorized. simpl. intros H.
  inversion H as [| x y z Hin Hr]; subst.
  simpl in Hin. destruct Hin.
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

(* (2) Hence no function of (capability, zone) computes authority:
   the reduction objection is refuted. *)
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
