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
    (3) unique_drops_once / alias_drops_twice are two-binding contrasts only.
        transfer_trace_retires_once proves actual sequential retirement over
        arbitrary transfer/drop traces, including repeated bindings.
    (4) move_and_clone_differ:
        after a move the source is gone; after a clone the source stays
        and does not see writes to the copy. Neither can stand in for the
        other, so {move, clone} is the basis and alias is outside it.

  This contrast does not prescribe user annotation or cloning ceremony.
  Compiler-owned value-copy/cleanup inference remains the source contract.

  Honest scope: bindings and storages are numbers; contents are one
  number; freshness of a cloned storage is a hypothesis.
*)

Require Import Stdlib.Lists.List.
Require Import Stdlib.Arith.PeanoNat.
Require Import Stdlib.micromega.Lia.
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

(* The preceding table is an interface/inventory constraint only. It does
   not prove admission, owner uniqueness or executable semantic correctness. *)

Record Heap := {
  owner   : nat -> option nat;   (* binding -> storage it owns *)
  content : nat -> nat           (* storage -> contents *)
}.

Definition unique_owner (h : Heap) : Prop :=
  forall x y s, owner h x = Some s -> owner h y = Some s -> x = y.

Definition set_owner (f : nat -> option nat) (x : nat) (v : option nat)
    : nat -> option nat :=
  fun z => if Nat.eqb z x then v else f z.

Inductive TransferResult := MoveApplied | MoveSelf | MoveMissingSource
  | MoveDestinationOwned | StorageRetired | RetireMissingOwner.

Definition move (h : Heap) (src dst : nat) : Heap * TransferResult :=
  if Nat.eqb src dst then (h,MoveSelf) else
  match owner h src, owner h dst with
  | Some s, None =>
      ({| owner := set_owner (set_owner (owner h) dst (Some s)) src None;
          content := content h |},MoveApplied)
  | None, _ => (h,MoveMissingSource)
  | Some _, Some _ => (h,MoveDestinationOwned)
  end.

Theorem self_move_is_identity : forall h src, move h src src = (h,MoveSelf).
Proof. intros. unfold move. rewrite Nat.eqb_refl. reflexivity. Qed.

Theorem occupied_destination_refuses_move : forall h src dst s,
  owner h dst = Some s -> fst (move h src dst) = h.
Proof.
  intros h src dst s H. unfold move. destruct (Nat.eqb src dst); auto.
  rewrite H. destruct (owner h src); reflexivity.
Qed.

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
    unique_owner (fst (move h src dst)).
Proof.
  intros h src dst U Hdst x y s.
  unfold move. destruct (Nat.eqb src dst); [apply U |].
  destruct (owner h src) as [storage |] eqn:Hsrc; [| apply U].
  rewrite Hdst. unfold set_owner. simpl.
  destruct (Nat.eqb x src) eqn:Exs; [intros Hx; discriminate Hx |].
  destruct (Nat.eqb y src) eqn:Eys; [intros _ Hy; discriminate Hy |].
  destruct (Nat.eqb x dst) eqn:Exd; destruct (Nat.eqb y dst) eqn:Eyd;
    intros Hx Hy.
  - apply Nat.eqb_eq in Exd. apply Nat.eqb_eq in Eyd.
    rewrite Exd, Eyd. reflexivity.
  - (* x now holds src's storage; y already owned it *)
    apply Nat.eqb_neq in Eys. exfalso. apply Eys.
    apply (U y src s); [exact Hy |]. injection Hx as <-. exact Hsrc.
  - apply Nat.eqb_neq in Exs. exfalso. apply Exs.
    apply (U x src s); [exact Hx |]. injection Hy as <-. exact Hsrc.
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
  owner (fst (move h0 0 1)) 0 = None /\
  owner (clone h0 0 1 20) 0 = Some 10 /\
  content (clone h0 0 1 20) 20 = content h0 10.
Proof. split; [reflexivity | split; reflexivity]. Qed.

(* Sequential accounting consumes ownership when emitting a retirement.
   Unlike `frees`, it does not count a repeated binding twice from a frozen
   pre-state. The ledger records logical storage identities, not raw addresses. *)
Record TransferLedger := {
  ledger_heap : Heap;
  retired : list nat
}.

Inductive TransferAction := Transfer (src dst : nat) | Retire (binding : nat).

Definition transfer_exec (l : TransferLedger) (a : TransferAction) : TransferLedger * TransferResult :=
  match a with
  | Transfer src dst =>
      let outcome := move (ledger_heap l) src dst in
      ({| ledger_heap := fst outcome; retired := retired l |},snd outcome)
  | Retire b =>
      match owner (ledger_heap l) b with
      | None => (l,RetireMissingOwner)
      | Some s =>
          ({| ledger_heap := {| owner := set_owner (owner (ledger_heap l)) b None;
                                content := content (ledger_heap l) |};
              retired := s :: retired l |},StorageRetired)
      end
  end.

Fixpoint transfer_run (l : TransferLedger) (actions : list TransferAction) :=
  match actions with
  | [] => l
  | a :: rest => transfer_run (fst (transfer_exec l a)) rest
  end.

Definition ledger_inv (l : TransferLedger) : Prop :=
  unique_owner (ledger_heap l) /\ NoDup (retired l) /\
  (forall s, In s (retired l) -> forall b, owner (ledger_heap l) b <> Some s).

Lemma moved_owner_origin : forall h src dst b s,
  owner (fst (move h src dst)) b = Some s -> exists old, owner h old = Some s.
Proof.
  intros h src dst b s. unfold move.
  destruct (Nat.eqb src dst); [intros H; eauto |].
  destruct (owner h src) as [t |] eqn:Hsrc; [| intros H; exists b; exact H].
  destruct (owner h dst); [intros H; exists b; exact H |].
  simpl. unfold set_owner. destruct (Nat.eqb b src); [discriminate |].
  destruct (Nat.eqb b dst); intros H.
  - inversion H; subst. exists src. exact Hsrc.
  - exists b. exact H.
Qed.

Lemma guarded_move_unique : forall h src dst,
  unique_owner h -> unique_owner (fst (move h src dst)).
Proof.
  intros h src dst Hunique. destruct (owner h dst) as [s |] eqn:Hdst.
  - rewrite (occupied_destination_refuses_move h src dst s Hdst). exact Hunique.
  - apply move_keeps_unique; assumption.
Qed.

Lemma ledger_step_preserves : forall l a, ledger_inv l -> ledger_inv (fst (transfer_exec l a)).
Proof.
  intros [h rs] a [Hu [Hnd Hret]]. destruct a as [src dst | b]; simpl in *.
  - split; [apply guarded_move_unique; exact Hu |]. split; [exact Hnd |].
    intros s Hin x Howner.
    destruct (moved_owner_origin h src dst x s Howner) as [old Hold].
    exact (Hret s Hin old Hold).
  - destruct (owner h b) as [s |] eqn:Hown; [| repeat split; assumption].
    simpl. split.
    + intros x y t. unfold set_owner. simpl.
      destruct (Nat.eqb x b); [discriminate |].
      destruct (Nat.eqb y b); [intros _ H; discriminate |]. apply Hu.
    + split.
      * constructor; [| exact Hnd]. intros Hin. exact (Hret s Hin b Hown).
      * intros t Hin x Hx. unfold set_owner in Hx. simpl in Hx.
        destruct (Nat.eqb x b) eqn:Ex; [discriminate |].
        destruct Hin as [Heq | Hin].
        -- subst t. apply Nat.eqb_neq in Ex. apply Ex. exact (Hu x b s Hx Hown).
        -- exact (Hret t Hin x Hx).
Qed.

Theorem transfer_trace_retires_once : forall l actions,
  ledger_inv l -> ledger_inv (transfer_run l actions).
Proof.
  intros l actions. revert l. induction actions; intros l Hinv; simpl; auto.
  apply IHactions. apply ledger_step_preserves. exact Hinv.
Qed.

Example repeated_drop_and_self_move_release_once :
  retired (transfer_run {| ledger_heap := h0; retired := [] |}
    [Transfer 0 0; Transfer 0 1; Retire 0; Retire 1; Retire 1; Retire 0]) = [10].
Proof. reflexivity. Qed.

Theorem unique_initial_ledger : forall h,
  unique_owner h -> ledger_inv {| ledger_heap := h; retired := [] |}.
Proof.
  intros h Hu. split; [exact Hu |]. split; [constructor |].
  intros s Hin. contradiction.
Qed.

Corollary unique_arbitrary_trace_retirement : forall h actions,
  unique_owner h ->
  NoDup (retired (transfer_run {| ledger_heap := h; retired := [] |} actions)).
Proof.
  intros h actions Hu.
  pose proof (transfer_trace_retires_once _ actions (unique_initial_ledger h Hu)) as H.
  exact (proj1 (proj2 H)).
Qed.

Example alias_sequentially_retires_twice :
  retired (transfer_run {| ledger_heap := alias h0 0 1; retired := [] |}
    [Retire 0; Retire 1]) = [10;10].
Proof. reflexivity. Qed.

Print Assumptions transfer_trace_retires_once.
