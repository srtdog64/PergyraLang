(* Feasibility model for owner-directed cleanup across a typed View and a
   scoped raw-pointer route. The machine address is NOT the owner identity:
   the allocation epoch and holder are separate facts. This is a model, not
   a refinement proof for the current Pergyra compiler, runtime, C, or LLVM.

   In particular, unsafe foreign writes, arbitrary pointer arithmetic,
   concurrency, and the production provenance issuer are outside this model.
   Owns is ghost evidence: affine production issuance is an open refinement
   obligation. No axiom or admitted proof supplies those missing facts. *)
Require Import Stdlib.Arith.PeanoNat Stdlib.Bool.Bool.

Inductive PointerRole := Owns (holder : nat) |
  Borrows (holder scope : nat) | Opaque.

Record RawPointer := mkRawPointer {
  raw_address : nat;
  raw_epoch : nat;
  raw_role : PointerRole
}.

Record Region := mkRegion {
  region_address : nat;
  region_epoch : nat;
  region_live : bool;
  region_holder : option nat;
  view_open : bool;
  raw_open : bool;
  raw_scope_generation : nat;
  release_count : nat
}.

(* Allocation returns an owner-bearing raw result. A borrowed projection
   keeps the address and provenance but receives no release authority. *)
Definition allocate_raw (address epoch holder : nat) : Region * RawPointer :=
  (mkRegion address epoch true (Some holder) false false 0 0,
   mkRawPointer address epoch (Owns holder)).

Definition opaque_raw (address : nat) : RawPointer :=
  mkRawPointer address 0 Opaque.

Definition same_allocation (s : Region) (p : RawPointer) : bool :=
  Nat.eqb (region_address s) (raw_address p) &&
  Nat.eqb (region_epoch s) (raw_epoch p).

Definition held_by (s : Region) (holder : nat) : bool :=
  match region_holder s with
  | Some current => Nat.eqb current holder
  | None => false
  end.

(* The View is the checked low-level access route. It suspends owner movement
   and release while active; a lexical exit closes the View. *)
Definition begin_view (s : Region) (holder : nat) : option Region :=
  if region_live s && held_by s holder && negb (view_open s) && negb (raw_open s)
  then Some (mkRegion (region_address s) (region_epoch s) true
              (region_holder s) true false (raw_scope_generation s)
              (release_count s))
  else None.

Definition view_access (s : Region) : bool :=
  region_live s && view_open s.

Definition end_view (s : Region) : option Region :=
  if view_open s
  then Some (mkRegion (region_address s) (region_epoch s) (region_live s)
              (region_holder s) false (raw_open s)
              (raw_scope_generation s) (release_count s))
  else None.

(* The capability authorizes entry, never ownership creation. A managed raw
   read additionally requires the original allocation epoch to remain live.
   Opaque foreign use may have separate unsafe semantics, but this model
   cannot claim its memory as compiler-owned. *)
Definition begin_raw (s : Region) (holder : nat) (raw_capability : bool)
    : option Region :=
  if raw_capability && region_live s && held_by s holder &&
     negb (view_open s) && negb (raw_open s)
  then Some (mkRegion (region_address s) (region_epoch s) true
              (region_holder s) false true (S (raw_scope_generation s))
              (release_count s))
  else None.

Definition managed_raw_access (s : Region) (p : RawPointer) : bool :=
  region_live s && raw_open s && same_allocation s p &&
  match raw_role p with
  | Owns holder => held_by s holder
  | Borrows holder scope =>
      held_by s holder && Nat.eqb scope (raw_scope_generation s)
  | Opaque => false
  end.

(* Borrow issuance checks the pointer being borrowed. A stale borrowed
   pointer cannot be laundered into a fresh scope ticket. *)
Definition borrow_raw (s : Region) (p : RawPointer) : option RawPointer :=
  if managed_raw_access s p
  then match raw_role p with
       | Owns holder | Borrows holder _ =>
           Some (mkRawPointer (raw_address p) (raw_epoch p)
                   (Borrows holder (raw_scope_generation s)))
       | Opaque => None
       end
  else None.

Definition end_raw (s : Region) : option Region :=
  if raw_open s
  then Some (mkRegion (region_address s) (region_epoch s) (region_live s)
              (region_holder s) (view_open s) false
              (raw_scope_generation s) (release_count s))
  else None.

(* Cleanup belongs to the holder fact. An address, View, or borrow cannot
   create a second release. The zero count also rejects a repeated cleanup. *)
Definition cleanup_holder (s : Region) (holder : nat) : option Region :=
  if region_live s && held_by s holder && negb (view_open s) &&
     negb (raw_open s) && Nat.eqb (release_count s) 0
  then Some (mkRegion (region_address s) (region_epoch s) false
              None false false (raw_scope_generation s) 1)
  else None.

Definition cleanup_raw (s : Region) (p : RawPointer) : option Region :=
  match raw_role p with
  | Owns holder =>
      if same_allocation s p then cleanup_holder s holder else None
  | Borrows _ _ | Opaque => None
  end.

(* Moving the owner changes the holder fact. The old pointer's address and
   epoch still match, but its authority no longer does. *)
Definition transfer_holder (s : Region) (from to : nat) : option Region :=
  if region_live s && held_by s from && negb (view_open s) &&
     negb (raw_open s) && negb (Nat.eqb from to)
  then Some (mkRegion (region_address s) (region_epoch s) true
              (Some to) false false (raw_scope_generation s)
              (release_count s))
  else None.

(* Only a verified move may issue a new owner-bearing pointer. Re-tagging an
   address alone is not an ownership operation. *)
Definition transfer_raw (s : Region) (p : RawPointer) (to : nat)
    : option (Region * RawPointer) :=
  match raw_role p with
  | Owns from =>
      if same_allocation s p
      then match transfer_holder s from to with
           | Some moved =>
               Some (moved, mkRawPointer (raw_address p) (raw_epoch p)
                            (Owns to))
           | None => None
           end
      else None
  | Borrows _ _ | Opaque => None
  end.

Inductive ExitKind := Normal | Return | Break | Error.

(* The compiler inserts this transition on each admitted exit. The caller
   supplies no manual Drop. A live lexical borrow must end before this step. *)
Definition elaborate_exit (_ : ExitKind) (s : Region) (p : RawPointer)
    : option Region := cleanup_raw s p.

Theorem borrowed_raw_never_releases : forall s p q,
  borrow_raw s p = Some q -> cleanup_raw s q = None.
Proof.
  intros s [a e role] q H. unfold borrow_raw in H.
  destruct (managed_raw_access s (mkRawPointer a e role)); try discriminate.
  destruct role as [holder|holder scope|]; try discriminate;
    inversion H; reflexivity.
Qed.

Theorem borrowed_raw_cannot_transfer : forall s p q to,
  borrow_raw s p = Some q -> transfer_raw s q to = None.
Proof.
  intros s [a e role] q to H. unfold borrow_raw in H.
  destruct (managed_raw_access s (mkRawPointer a e role)); try discriminate.
  destruct role as [holder|holder scope|]; try discriminate;
    inversion H; reflexivity.
Qed.

Theorem opaque_raw_never_releases : forall s a,
  cleanup_raw s (opaque_raw a) = None.
Proof. reflexivity. Qed.

Theorem opaque_raw_cannot_transfer : forall s a to,
  transfer_raw s (opaque_raw a) to = None.
Proof. reflexivity. Qed.

Theorem same_address_does_not_determine_release_authority : forall a e h,
  let s := mkRegion a e true (Some h) false false 1 0 in
  let owned := mkRawPointer a e (Owns h) in
  let borrowed := mkRawPointer a e (Borrows h 1) in
  let released := mkRegion a e false None false false 1 1 in
  raw_address owned = raw_address borrowed /\
  raw_epoch owned = raw_epoch borrowed /\
  cleanup_raw s owned = Some released /\ cleanup_raw s borrowed = None.
Proof.
  intros a e h. unfold cleanup_raw, cleanup_holder, same_allocation,
    held_by; simpl. repeat rewrite Nat.eqb_refl.
  repeat split; reflexivity.
Qed.

Theorem raw_capability_does_not_create_owner : forall a e,
  begin_raw (mkRegion a e true None false false 0 0) 7 true = None.
Proof. reflexivity. Qed.

Theorem raw_capability_required : forall a e h,
  begin_raw (fst (allocate_raw a e h)) h false = None.
Proof. reflexivity. Qed.

Theorem active_raw_prevents_premature_release : forall a e h,
  let s := mkRegion a e true (Some h) false true 1 0 in
  cleanup_raw s (mkRawPointer a e (Owns h)) = None.
Proof. intros a e h. unfold cleanup_raw, same_allocation, cleanup_holder, held_by; simpl.
  repeat rewrite Nat.eqb_refl. reflexivity. Qed.

Theorem view_prevents_premature_release : forall a e h,
  let s := mkRegion a e true (Some h) true false 0 0 in
  cleanup_raw s (mkRawPointer a e (Owns h)) = None.
Proof. intros a e h. unfold cleanup_raw, same_allocation, cleanup_holder, held_by; simpl.
  repeat rewrite Nat.eqb_refl. reflexivity. Qed.

Theorem managed_raw_access_requires_live_scope_and_epoch : forall s p,
  managed_raw_access s p = true ->
  region_live s = true /\ raw_open s = true /\
  same_allocation s p = true /\ raw_role p <> Opaque.
Proof.
  intros s p H. unfold managed_raw_access in H.
  apply andb_true_iff in H as [Hmatch Hrole].
  apply andb_true_iff in Hmatch as [Hscope Hsame].
  apply andb_true_iff in Hscope as [Hlive Hraw].
  repeat split; try assumption.
  destruct (raw_role p); try discriminate; congruence.
Qed.

Theorem cleanup_exactly_once : forall s p after,
  cleanup_raw s p = Some after ->
  region_live after = false /\ region_holder after = None /\
  release_count after = 1 /\ cleanup_raw after p = None.
Proof.
  intros [a e live holder view raw scope count] [pa pe role] after H.
  unfold cleanup_raw in H; simpl in H.
  destruct role as [h| |]; try discriminate.
  destruct (same_allocation (mkRegion a e live holder view raw scope count)
              (mkRawPointer pa pe (Owns h))) eqn:Hsame; try discriminate.
  unfold cleanup_holder in H; simpl in H.
  destruct (live && held_by (mkRegion a e live holder view raw scope count) h &&
            negb view && negb raw && Nat.eqb count 0) eqn:Hguard;
    try discriminate.
  inversion H; subst. simpl. repeat split; try reflexivity.
  unfold cleanup_raw. simpl. destruct (same_allocation
    (mkRegion a e false None false false scope 1)
    (mkRawPointer pa pe (Owns h))); reflexivity.
Qed.

(* The exact same address can be reused for a different allocation. Address
   equality cannot authorize either access or cleanup of the old pointer. *)
Theorem stale_epoch_refuses_use_and_cleanup : forall a old fresh h,
  old <> fresh ->
  let reused := mkRegion a fresh true (Some h) false true 1 0 in
  let stale := mkRawPointer a old (Owns h) in
  managed_raw_access reused stale = false /\ cleanup_raw reused stale = None.
Proof.
  intros a old fresh h Hneq. simpl.
  unfold managed_raw_access, cleanup_raw, same_allocation; simpl.
  rewrite Nat.eqb_refl.
  apply Nat.eqb_neq in Hneq. rewrite Nat.eqb_sym in Hneq.
  rewrite Hneq. simpl. split; reflexivity.
Qed.

(* A borrowed raw pointer is tied to the issuing lexical raw scope. Reopening
   unsafe(raw) for the same live owner issues a different scope generation. *)
Theorem borrowed_raw_expires_at_scope_end : forall a e h,
  let owned := snd (allocate_raw a e h) in
  let issuing := mkRegion a e true (Some h) false true 1 0 in
  let later := mkRegion a e true (Some h) false true 2 0 in
  let borrowed := mkRawPointer a e (Borrows h 1) in
  borrow_raw issuing owned = Some borrowed /\
  managed_raw_access issuing borrowed = true /\
  managed_raw_access later borrowed = false /\
  borrow_raw later borrowed = None /\
  cleanup_raw later borrowed = None.
Proof.
  intros a e h. unfold allocate_raw, borrow_raw, managed_raw_access,
    cleanup_raw, same_allocation, held_by; simpl.
  repeat rewrite Nat.eqb_refl. repeat split; reflexivity.
Qed.

(* A View operation, then a raw operation, then compiler-inserted cleanup:
   neither route asks the programmer to restore a separate owning wrapper. *)
Theorem view_then_raw_cleanup_on_every_exit : forall a e h k,
  let s := fst (allocate_raw a e h) in
  let p := snd (allocate_raw a e h) in
  let viewed := mkRegion a e true (Some h) true false 0 0 in
  let rawed := mkRegion a e true (Some h) false true 1 0 in
  let closed_raw := mkRegion a e true (Some h) false false 1 0 in
  let released := mkRegion a e false None false false 1 1 in
  begin_view s h = Some viewed /\ view_access viewed = true /\
  end_view viewed = Some s /\ begin_raw s h true = Some rawed /\
  managed_raw_access rawed p = true /\ end_raw rawed = Some closed_raw /\
  elaborate_exit k closed_raw p = Some released /\
  cleanup_raw released p = None.
Proof.
  intros a e h [| | |];
    unfold allocate_raw, begin_view, view_access, end_view, begin_raw,
      managed_raw_access, end_raw, elaborate_exit, cleanup_raw,
      cleanup_holder, same_allocation, held_by;
    simpl; repeat rewrite Nat.eqb_refl; repeat split; reflexivity.
Qed.

Theorem transfer_revokes_old_owner : forall a e old new,
  old <> new ->
  let initial := fst (allocate_raw a e old) in
  let old_pointer := snd (allocate_raw a e old) in
  let moved := mkRegion a e true (Some new) false false 0 0 in
  let new_pointer := mkRawPointer a e (Owns new) in
  let released := mkRegion a e false None false false 0 1 in
  transfer_raw initial old_pointer new = Some (moved, new_pointer) /\
  cleanup_raw moved old_pointer = None /\
  cleanup_raw moved new_pointer = Some released.
Proof.
  intros a e old new Hneq. simpl.
  unfold transfer_raw, transfer_holder, cleanup_raw, cleanup_holder,
    same_allocation, held_by; simpl.
  apply Nat.eqb_neq in Hneq.
  assert (Hback : Nat.eqb new old = false) by
    (rewrite Nat.eqb_sym; exact Hneq).
  rewrite Hneq, Hback. repeat rewrite Nat.eqb_refl.
  repeat split; reflexivity.
Qed.

Theorem transfer_revokes_old_raw_access : forall a e old new,
  old <> new ->
  let reopened := mkRegion a e true (Some new) false true 1 0 in
  let old_pointer := snd (allocate_raw a e old) in
  let new_pointer := mkRawPointer a e (Owns new) in
  managed_raw_access reopened old_pointer = false /\
  managed_raw_access reopened new_pointer = true.
Proof.
  intros a e old new Hneq.
  unfold managed_raw_access, allocate_raw,
    same_allocation, held_by; simpl.
  apply Nat.eqb_neq in Hneq.
  assert (Hback : Nat.eqb new old = false) by
    (rewrite Nat.eqb_sym; exact Hneq).
  rewrite Hback. repeat rewrite Nat.eqb_refl.
  split; reflexivity.
Qed.
