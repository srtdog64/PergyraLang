(*
  Pergyra Formal Semantics - Mechanized Sketch
  Target: harness PP-064 -- what a party role slot must carry.
  Status: proof-sketch; not beta-closure evidence unless checked by CI.

  `bind team.tank = WarriorTank;` chose a role but no subject, and a role
  body that read `self.hp` crashed on native. This file names the units a
  slot needs and shows each is irreducible:

    (1) identity_irreducible / witness_irreducible:
        subject identity and role witness are independent facts. Two
        slots with the same witness but different subjects answer
        differently, and so do two slots with the same subject but
        different witnesses. Neither is a function of the other.
    (2) role_only_has_no_self:
        a slot that carries only the witness (the old surface) has no
        `self` to read for any store -- it is not a smaller unit, it is
        an incomplete one.
    (3) borrow_keeps_identity / owned_copy_splits_identity:
        a slot that borrows the subject shows every write through the
        slot on the subject itself; a slot that owns a copy does not,
        so an owned copy splits one identity into two.
    (4) scoped_borrow_live / inner_subject_dangles:
        the scope rule "the subject's scope encloses the party's scope"
        keeps the borrowed subject live whenever the party is; the
        opposite nesting has a point where the party is live and the
        subject is gone.
    (5) slot_call_defined:
        so the party slot is the composition borrow(identity) x witness,
        and a call through an admitted slot always has a value.

  Honest scope: stores map identities to one field value; scopes are
  nesting depths; roles are functions of the subject state. No aliasing
  between parties, no concurrency, no drop model.
*)

Require Import Coq.Arith.PeanoNat.
Require Import Coq.micromega.Lia.

(* A store maps a subject identity to its state; None is a dead subject. *)
Definition Store := nat -> option nat.

(* A role witness is the method the role places on the subject type. *)
Definition Witness := nat -> nat.

Inductive Slot :=
| RoleOnly (w : Witness)                 (* old surface: no subject *)
| Borrowed (id : nat) (w : Witness)      (* subject identity + witness *)
| OwnedCopy (state : nat) (w : Witness). (* party owns a copy *)

(* Calling the role method through the slot. *)
Definition call (s : Store) (slot : Slot) : option nat :=
  match slot with
  | RoleOnly _ => None
  | Borrowed id w =>
      match s id with
      | Some v => Some (w v)
      | None => None
      end
  | OwnedCopy v w => Some (w v)
  end.

(* ---- (1) irreducibility ---------------------------------------- *)

Definition limit : Witness := fun v => v.
Definition doubled : Witness := fun v => v + v.
Definition store2 : Store :=
  fun id => match id with 0 => Some 3 | 1 => Some 7 | _ => None end.

Theorem identity_irreducible :
  call store2 (Borrowed 0 limit) <> call store2 (Borrowed 1 limit).
Proof. simpl. discriminate. Qed.

Theorem witness_irreducible :
  call store2 (Borrowed 0 limit) <> call store2 (Borrowed 0 doubled).
Proof. simpl. discriminate. Qed.

(* ---- (2) the role-only slot has no self ------------------------ *)

Theorem role_only_has_no_self :
  forall (s : Store) (w : Witness), call s (RoleOnly w) = None.
Proof. reflexivity. Qed.

(* ---- (3) borrow keeps identity; an owned copy splits it -------- *)

Definition write (s : Store) (id v : nat) : Store :=
  fun x => if Nat.eqb x id then Some v else s x.

(* A role method that sets the subject's field through the slot. *)
Definition write_through (s : Store) (slot : Slot) (v : nat)
    : Store * Slot :=
  match slot with
  | Borrowed id w => (write s id v, Borrowed id w)
  | OwnedCopy _ w => (s, OwnedCopy v w)
  | RoleOnly w => (s, RoleOnly w)
  end.

Theorem borrow_keeps_identity :
  forall s id w v,
    s id <> None ->
    let (s', slot') := write_through s (Borrowed id w) v in
    call s' slot' = option_map w (s' id).
Proof.
  intros s id w v _. simpl.
  unfold write. rewrite Nat.eqb_refl. reflexivity.
Qed.

(* The subject 0 keeps 3 while the owned copy reports 9: two identities. *)
Theorem owned_copy_splits_identity :
  let (s', slot') := write_through store2 (OwnedCopy 3 limit) 9 in
  s' 0 = Some 3 /\ call s' slot' = Some 9.
Proof. simpl. split; reflexivity. Qed.

(* ---- (4) the scope rule keeps a borrowed subject live ---------- *)

(* A binding declared at depth k is live at depth d when k <= d. *)
Definition live (k d : nat) : Prop := k <= d.

Theorem scoped_borrow_live :
  forall subject_depth party_depth d,
    subject_depth <= party_depth ->
    live party_depth d -> live subject_depth d.
Proof. unfold live. intros. lia. Qed.

Theorem inner_subject_dangles :
  exists subject_depth party_depth d,
    party_depth < subject_depth /\
    live party_depth d /\ ~ live subject_depth d.
Proof.
  exists 2, 1, 1. unfold live. repeat split; lia.
Qed.

(* ---- (5) composition -------------------------------------------- *)

(* A store keeps every subject that is live at the current depth. *)
Definition keeps_live (s : Store) (depth_of : nat -> nat) (d : nat) : Prop :=
  forall id, live (depth_of id) d -> s id <> None.

Theorem slot_call_defined :
  forall s depth_of d id w party_depth,
    keeps_live s depth_of d ->
    depth_of id <= party_depth ->
    live party_depth d ->
    exists v, call s (Borrowed id w) = Some v.
Proof.
  intros s depth_of d id w party_depth Hkeep Hscope Hparty.
  assert (Hlive : live (depth_of id) d) by (unfold live in *; lia).
  unfold keeps_live in Hkeep.
  specialize (Hkeep id Hlive).
  simpl. destruct (s id) as [v |] eqn:Hs.
  - exists (w v). reflexivity.
  - exfalso. apply Hkeep. reflexivity.
Qed.
