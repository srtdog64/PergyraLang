(*
  Pergyra Formal Semantics - Mechanized Sketch
  Target: harness PP-065 -- `authority agent requires Ability`.
  Status: proof-sketch; not beta-closure evidence unless checked by CI.

  Native refused a zone authority whose slot subject lacked the required
  role; the default route accepted it. This file shows the verdict needs
  two independent units and that checking one of them fails open:

    (1) identity_does_not_decide / witness_does_not_decide:
        the actor being the authority slot's subject and the subject type
        holding the required ability witness are independent facts.
    (2) identity_only_fails_open:
        an admission that checks identity alone accepts an actor the full
        rule rejects -- the default-route defect.
    (3) authorized_iff:
        the full rule is exactly the conjunction.
    (4) static_check_suffices:
        checking the witness once per declaration (the slot's subject type
        against the `requires` list) makes the witness conjunct hold for
        every actor that can occupy the slot, so the runtime verdict
        reduces to identity. This is why the check belongs to declaration
        admission, not to each call.

  Honest scope: abilities and types are numbers; `has_role` is the role
  table; one authority slot per zone.
*)

Require Import Stdlib.Lists.List.
Import ListNotations.

Record Zone := {
  holder    : nat;          (* identity bound to the authority slot *)
  slot_type : nat;          (* declared subject type of that slot *)
  requires  : list nat      (* abilities the authority requires *)
}.

Record Actor := { actor_id : nat; actor_type : nat }.

Definition has_all (has_role : nat -> nat -> Prop) (ty : nat) (reqs : list nat)
    : Prop :=
  forall a, In a reqs -> has_role ty a.

Definition authorized (has_role : nat -> nat -> Prop) (z : Zone) (x : Actor)
    : Prop :=
  actor_id x = holder z /\ has_all has_role (actor_type x) (requires z).

Definition identity_only (z : Zone) (x : Actor) : Prop :=
  actor_id x = holder z.

(* Type 1 holds ability 5; type 2 holds nothing. *)
Definition roles (ty a : nat) : Prop := ty = 1 /\ a = 5.

Definition zone5 : Zone := {| holder := 7; slot_type := 2; requires := [5] |}.

Definition held_unqualified : Actor := {| actor_id := 7; actor_type := 2 |}.
Definition qualified_stranger : Actor := {| actor_id := 8; actor_type := 1 |}.

Lemma type2_lacks_5 : ~ has_all roles 2 [5].
Proof.
  unfold has_all, roles. intros H.
  destruct (H 5 (or_introl eq_refl)) as [Hty _]. discriminate Hty.
Qed.

Lemma type1_has_5 : has_all roles 1 [5].
Proof.
  unfold has_all, roles. intros a Hin. simpl in Hin.
  destruct Hin as [Ha | []]. split; [reflexivity | symmetry; exact Ha].
Qed.

(* ---- (1) neither unit decides the verdict ---------------------- *)

Theorem identity_does_not_decide :
  identity_only zone5 held_unqualified /\
  ~ authorized roles zone5 held_unqualified.
Proof.
  split.
  - reflexivity.
  - intros [_ Hw]. apply type2_lacks_5. exact Hw.
Qed.

Theorem witness_does_not_decide :
  has_all roles (actor_type qualified_stranger) (requires zone5) /\
  ~ authorized roles zone5 qualified_stranger.
Proof.
  split.
  - exact type1_has_5.
  - intros [Hid _]. simpl in Hid. discriminate Hid.
Qed.

(* ---- (2) checking identity alone fails open --------------------- *)

Theorem identity_only_fails_open :
  exists has_role z x,
    identity_only z x /\ ~ authorized has_role z x.
Proof.
  exists roles, zone5, held_unqualified.
  exact identity_does_not_decide.
Qed.

(* ---- (3) the rule is exactly the conjunction -------------------- *)

Theorem authorized_iff :
  forall has_role z x,
    authorized has_role z x <->
    identity_only z x /\ has_all has_role (actor_type x) (requires z).
Proof. intros. unfold authorized, identity_only. tauto. Qed.

(* ---- (4) the declaration check covers every occupant ------------ *)

Theorem static_check_suffices :
  forall has_role z x,
    has_all has_role (slot_type z) (requires z) ->
    actor_type x = slot_type z ->
    identity_only z x ->
    authorized has_role z x.
Proof.
  intros has_role z x Hstatic Hty Hid.
  unfold authorized. unfold identity_only in Hid.
  split.
  - exact Hid.
  - rewrite Hty. exact Hstatic.
Qed.
