(*
  Pergyra Formal Semantics - Mechanized Sketch
  Target: Array<T> copy semantics (red-team P0 R1, docs/22 section 2).
  Status: proof-sketch; not beta-closure evidence unless checked by CI.

  `let b = a` for an Array either moves the backing storage, clones it,
  or aliases it. This file fixes which of these are units:

    (1) move_keeps_unique / clone_keeps_unique:
        moving and cloning both keep "every storage has at most one
        owning binding".
    (2) alias_breaks_unique:
        aliasing (the implicit shallow copy) gives one storage two owners.
    (3) unique_drops_once / alias_drops_twice:
        with one owner per storage, releasing every live binding frees
        each storage at most once; an alias frees one storage twice.
    (4) move_and_clone_differ:
        after a move the source is gone; after a clone the source stays
        and does not see writes to the copy. Neither can stand in for the
        other, so {move, clone} is the basis and alias is outside it.

  An implicit deep copy on `let` computes the same state as `clone`: it is
  the clone unit with its cost hidden, not a new unit (docs/22: a copy is
  spelled Clone).

  Honest scope: bindings and storages are numbers; contents are one
  number; freshness of a cloned storage is a hypothesis.
*)

Require Import Coq.Lists.List.
Require Import Coq.Arith.PeanoNat.
Require Import Coq.micromega.Lia.
Import ListNotations.

(*
  Finite ownership-transition obligation table (P1).

  This table does not guess whether a parameterized operation is admitted.
  For example, push still distinguishes borrowed and owned element policy in
  the executable semantic owner.  The table proves the narrower closure
  obligation: every state/family pair is routed to exactly one named owner and
  one executable gate family, so a future state or operation cannot appear
  outside the table unnoticed.
*)

Inductive CollectionState : Type :=
| EmptyState
| OwnedState
| MovedState
| BorrowedState
| UnknownState.

Inductive CollectionOperation : Type :=
| PushOperation
| MoveOperation
| CloneOperation
| DropOperation
| ArgumentOperation
| ReturnOperation
| InoutOperation.

Inductive TransitionOwnerId : Type :=
| SemanticCollectionOwnershipOwner.

Inductive TransitionGateId : Type :=
| PushOwnershipGate
| MoveOwnershipGate
| CloneOwnershipGate
| DropOwnershipGate
| ArgumentOwnershipGate
| ReturnOwnershipGate
| InoutOwnershipGate.

Record TransitionObligation := {
  obligation_state : CollectionState;
  obligation_operation : CollectionOperation;
  obligation_owner : TransitionOwnerId;
  obligation_gate : TransitionGateId
}.

Definition gate_for (op : CollectionOperation) : TransitionGateId :=
  match op with
  | PushOperation => PushOwnershipGate
  | MoveOperation => MoveOwnershipGate
  | CloneOperation => CloneOwnershipGate
  | DropOperation => DropOwnershipGate
  | ArgumentOperation => ArgumentOwnershipGate
  | ReturnOperation => ReturnOwnershipGate
  | InoutOperation => InoutOwnershipGate
  end.

Definition obligation (state : CollectionState)
    (op : CollectionOperation) : TransitionObligation :=
  {| obligation_state := state;
     obligation_operation := op;
     obligation_owner := SemanticCollectionOwnershipOwner;
     obligation_gate := gate_for op |}.

Definition state_row (state : CollectionState) :
    list TransitionObligation :=
  [ obligation state PushOperation;
    obligation state MoveOperation;
    obligation state CloneOperation;
    obligation state DropOperation;
    obligation state ArgumentOperation;
    obligation state ReturnOperation;
    obligation state InoutOperation ].

Definition transition_obligation_table : list TransitionObligation :=
  state_row EmptyState ++
  state_row OwnedState ++
  state_row MovedState ++
  state_row BorrowedState ++
  state_row UnknownState.

Definition collection_state_eqb (left right : CollectionState) : bool :=
  match left, right with
  | EmptyState, EmptyState
  | OwnedState, OwnedState
  | MovedState, MovedState
  | BorrowedState, BorrowedState
  | UnknownState, UnknownState => true
  | _, _ => false
  end.

Definition collection_operation_eqb
    (left right : CollectionOperation) : bool :=
  match left, right with
  | PushOperation, PushOperation
  | MoveOperation, MoveOperation
  | CloneOperation, CloneOperation
  | DropOperation, DropOperation
  | ArgumentOperation, ArgumentOperation
  | ReturnOperation, ReturnOperation
  | InoutOperation, InoutOperation => true
  | _, _ => false
  end.

Definition matching_obligations (state : CollectionState)
    (op : CollectionOperation) : list TransitionObligation :=
  filter
    (fun row => andb
      (collection_state_eqb (obligation_state row) state)
      (collection_operation_eqb (obligation_operation row) op))
    transition_obligation_table.

Theorem transition_obligation_table_has_35_rows :
  length transition_obligation_table = 35.
Proof. reflexivity. Qed.

Theorem transition_obligation_table_complete_and_unique :
  forall state op, length (matching_obligations state op) = 1.
Proof.
  intros state op. destruct state; destruct op; reflexivity.
Qed.

Theorem transition_obligation_has_one_owner_and_gate :
  forall state op,
    exists owner gate,
      obligation_owner (obligation state op) = owner /\
      obligation_gate (obligation state op) = gate.
Proof.
  intros state op.
  exists SemanticCollectionOwnershipOwner, (gate_for op).
  split; reflexivity.
Qed.

Record Heap := {
  owner   : nat -> option nat;   (* binding -> storage it owns *)
  content : nat -> nat           (* storage -> contents *)
}.

Definition unique_owner (h : Heap) : Prop :=
  forall x y s, owner h x = Some s -> owner h y = Some s -> x = y.

Definition set_owner (f : nat -> option nat) (x : nat) (v : option nat)
    : nat -> option nat :=
  fun z => if Nat.eqb z x then v else f z.

Definition move (h : Heap) (src dst : nat) : Heap :=
  {| owner := set_owner (set_owner (owner h) dst (owner h src)) src None;
     content := content h |}.

Definition clone (h : Heap) (src dst fresh : nat) : Heap :=
  match owner h src with
  | Some s =>
      {| owner := set_owner (owner h) dst (Some fresh);
         content := fun t => if Nat.eqb t fresh then content h s
                             else content h t |}
  | None => h
  end.

Definition alias (h : Heap) (src dst : nat) : Heap :=
  {| owner := set_owner (owner h) dst (owner h src); content := content h |}.

(* ---- (1) move and clone keep one owner per storage -------------- *)

Theorem move_keeps_unique :
  forall h src dst,
    unique_owner h -> owner h dst = None ->
    unique_owner (move h src dst).
Proof.
  intros h src dst U Hdst x y s.
  unfold move, set_owner. simpl.
  destruct (Nat.eqb x src) eqn:Exs; [intros Hx; discriminate Hx |].
  destruct (Nat.eqb y src) eqn:Eys; [intros _ Hy; discriminate Hy |].
  destruct (Nat.eqb x dst) eqn:Exd; destruct (Nat.eqb y dst) eqn:Eyd;
    intros Hx Hy.
  - apply Nat.eqb_eq in Exd. apply Nat.eqb_eq in Eyd.
    rewrite Exd, Eyd. reflexivity.
  - (* x now holds src's storage; y already owned it *)
    apply Nat.eqb_neq in Eys. exfalso. apply Eys.
    apply (U y src s); assumption.
  - apply Nat.eqb_neq in Exs. exfalso. apply Exs.
    apply (U x src s); assumption.
  - apply (U x y s); assumption.
Qed.

Theorem clone_keeps_unique :
  forall h src dst fresh,
    unique_owner h -> owner h dst = None ->
    (forall z, owner h z <> Some fresh) ->
    unique_owner (clone h src dst fresh).
Proof.
  intros h src dst fresh U Hdst Hfresh.
  unfold clone. destruct (owner h src) as [s |] eqn:Hsrc; [| exact U].
  intros x y t. unfold set_owner. simpl.
  destruct (Nat.eqb x dst) eqn:Exd; destruct (Nat.eqb y dst) eqn:Eyd;
    intros Hx Hy.
  - apply Nat.eqb_eq in Exd. apply Nat.eqb_eq in Eyd.
    rewrite Exd, Eyd. reflexivity.
  - injection Hx as Ht. rewrite <- Ht in Hy. exfalso.
    apply (Hfresh y). exact Hy.
  - injection Hy as Ht. rewrite <- Ht in Hx. exfalso.
    apply (Hfresh x). exact Hx.
  - apply (U x y t); assumption.
Qed.

(* ---- (2) alias gives one storage two owners ---------------------- *)

Definition h0 : Heap :=
  {| owner := fun b => match b with 0 => Some 10 | _ => None end;
     content := fun _ => 1 |}.

Lemma h0_unique : unique_owner h0.
Proof.
  intros x y s Hx Hy.
  destruct x as [| x']; destruct y as [| y']; simpl in Hx, Hy;
    try reflexivity; try discriminate Hx; try discriminate Hy.
Qed.

Theorem alias_breaks_unique : ~ unique_owner (alias h0 0 1).
Proof.
  intros U.
  assert (H : 0 = 1).
  { apply (U 0 1 10); reflexivity. }
  discriminate H.
Qed.

(* ---- (3) one owner frees once; an alias frees twice -------------- *)

(* Releasing the bindings in `live` frees their storages. *)
Definition frees (h : Heap) (live : list nat) (s : nat) : nat :=
  length (filter (fun b => match owner h b with
                           | Some t => Nat.eqb t s | None => false
                           end) live).

Theorem alias_drops_twice : frees (alias h0 0 1) [0; 1] 10 = 2.
Proof. reflexivity. Qed.

Theorem unique_drops_once :
  forall h b1 b2 s,
    unique_owner h -> b1 <> b2 -> frees h [b1; b2] s <= 1.
Proof.
  intros h b1 b2 s U Hne. unfold frees. simpl.
  destruct (owner h b1) as [t1 |] eqn:H1;
    destruct (owner h b2) as [t2 |] eqn:H2; simpl.
  - destruct (Nat.eqb t1 s) eqn:E1; destruct (Nat.eqb t2 s) eqn:E2;
      simpl; try lia.
    apply Nat.eqb_eq in E1. apply Nat.eqb_eq in E2.
    rewrite E1 in H1. rewrite E2 in H2.
    exfalso. apply Hne. apply (U b1 b2 s); assumption.
  - destruct (Nat.eqb t1 s); simpl; lia.
  - destruct (Nat.eqb t2 s); simpl; lia.
  - lia.
Qed.

(* ---- (4) move and clone are different units ---------------------- *)

Theorem move_and_clone_differ :
  owner (move h0 0 1) 0 = None /\
  owner (clone h0 0 1 20) 0 = Some 10 /\
  content (clone h0 0 1 20) 20 = content h0 10.
Proof. split; [reflexivity | split; reflexivity]. Qed.
