(*
  Pergyra Formal Semantics -- Mechanized Fragment (fourth corner)
  Target: docs/semantics/19 "Pergyra Abstract Machine Obligation"
  Status: proof-sketch; not beta-closure evidence unless checked by CI (coqc).

  Scope: the authority-check Step form of the abstract machine -- delegation of
  authority between principals (authorization-logic / object-capability lineage,
  docs/19 authority row; ABLP "A says s"). The state maps each principal to the
  capabilities it holds. A delegation step lets principal A grant capability k to
  principal B, gated on A actually holding k.

  Mechanized obligations (docs/19):
    - Authority soundness: a delegation requires the delegator to hold the
      capability (`delegation_requires_holding`) -- you cannot grant what you lack.
    - No privilege escalation / no ambient authority: delegation introduces no NEW
      capability into circulation; any capability a principal ends up holding was
      already held by some principal before the step (`no_privilege_escalation`).
      This is the transitive form of "authority is not created from nothing", the
      authority-axis counterpart of the no-ambient-authority property proved for
      the zone/effect corners.

  Negative scope: a flat principal->caps map, single-step delegation (the
  reflexive-transitive closure and revocation are follow-ons), and no binding to
  the live AIR authority facts yet (task #45 / docs/18).
*)

Require Import Coq.Lists.List.
Require Import Coq.Arith.PeanoNat.
Import ListNotations.

Section AuthorityDelegationCore.

Definition principal := nat.
Definition cap       := nat.

(* Who holds which capabilities. *)
Definition holdings := principal -> list cap.
Definition upd (h : holdings) (p : principal) (cs : list cap) : holdings :=
  fun x => if Nat.eqb x p then cs else h x.

Definition holds (h : holdings) (p : principal) (k : cap) : Prop := In k (h p).
Definition in_circulation (h : holdings) (k : cap) : Prop :=
  exists p, holds h p k.

(* A delegation step: A grants k to B, only if A holds k. *)
Inductive deleg : holdings -> principal -> principal -> cap -> holdings -> Prop :=
| Delegate : forall h a b k,
    holds h a k ->
    deleg h a b k (upd h b (k :: h b)).

(* ---- authority soundness: you can only delegate what you hold ---- *)

Theorem delegation_requires_holding : forall h a b k h',
  deleg h a b k h' -> holds h a k.
Proof. intros h a b k h' H. inversion H; subst. assumption. Qed.

(* ---- no privilege escalation: delegation creates no new capability ---- *)
(* Any capability in circulation after a delegation was already in circulation
   before it: delegation only redistributes existing authority. *)

Theorem no_privilege_escalation : forall h a b k h' k',
  deleg h a b k h' ->
  in_circulation h' k' ->
  in_circulation h k'.
Proof.
  intros h a b k h' k' Hd Hcirc.
  inversion Hd; subst.
  unfold in_circulation, holds, upd in *.
  destruct Hcirc as [p Hp].
  destruct (Nat.eqb p b) eqn:E.
  - (* p = b: Hp : In k' (k :: h b) *)
    simpl in Hp. destruct Hp as [Hk | Hp'].
    + (* k' = k: in circulation via the delegator a *)
      subst k'. exists a. exact H.
    + (* already held by b *)
      exists b. exact Hp'.
  - (* p <> b: Hp : In k' (h p) *)
    exists p. exact Hp.
Qed.

End AuthorityDelegationCore.
