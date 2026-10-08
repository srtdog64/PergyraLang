(*
  Pergyra Formal Semantics - Mechanized Sketch
  Target: harness PP-063 -- a `String` returned by an `extern` function.
  Status: proof-sketch; not beta-closure evidence unless checked by CI.

  The compiler used the returned `char*` as the Pergyra string itself. A
  foreign pointer comes with an obligation the signature `-> String` does
  not state: the callee keeps it (borrowed: the caller copies, never
  frees) or hands it over (owned: the caller copies and releases it once
  through the callee's release function). This file shows:

    (1) obligation_not_in_signature:
        two foreign returns with the same signature and the same bytes
        can carry different obligations, so the annotation is
        information the type does not hold.
    (2) borrowed_as_owned_double_frees / owned_as_borrowed_leaks:
        guessing either way breaks one of them: a borrowed pointer
        treated as owned is released twice, an owned one treated as
        borrowed is never released.
    (3) lanes_release_exactly:
        `-> ref String` (copy, no release) and `-> own String release F`
        (copy, release once) meet their obligation exactly.
    (4) boundary_value_survives_reuse / raw_address_reads_reused_storage:
        the boundary constructs an owned byte value, rather than retaining
        an address. Executable free/reuse transitions contrast that value
        with the rejected raw-address design after the same address is reused.

  So the two lanes are compositions of two units -- copy at the boundary
  and the release obligation -- and a `-> String` with no lane is refused.

  Honest scope: bytes are one number; memory maps raw addresses to live
  contents. Release-count lane theorems are interface accounting, not allocator
  refinement. The executable memory transitions below model write/free/reuse;
  actual foreign code and the compiler boundary still require source gates.
*)

Require Import Stdlib.Arith.PeanoNat.
Require Import Stdlib.Lists.List.
Import ListNotations.

Inductive Obligation := CalleeKeeps | CallerReleases.

Record ForeignReturn := {
  ptr        : nat;
  bytes      : nat;          (* contents at the return *)
  obligation : Obligation;
  signature  : nat           (* the declared extern signature *)
}.

Inductive Lane := RefLane | OwnLane.

(* Releases the caller performs for a lane. *)
Definition caller_releases (l : Lane) : nat :=
  match l with RefLane => 0 | OwnLane => 1 end.

(* Releases the foreign side performs on its own. *)
Definition callee_releases (o : Obligation) : nat :=
  match o with CalleeKeeps => 1 | CallerReleases => 0 end.

Definition total_releases (r : ForeignReturn) (l : Lane) : nat :=
  caller_releases l + callee_releases (obligation r).

(* A foreign allocation must be released exactly once. *)
Definition sound (r : ForeignReturn) (l : Lane) : Prop :=
  total_releases r l = 1.

Definition borrowed_ret : ForeignReturn :=
  {| ptr := 40; bytes := 7; obligation := CalleeKeeps; signature := 3 |}.
Definition owned_ret : ForeignReturn :=
  {| ptr := 40; bytes := 7; obligation := CallerReleases; signature := 3 |}.

(* ---- (1) the obligation is not in the signature ------------------ *)

Theorem obligation_not_in_signature :
  signature borrowed_ret = signature owned_ret /\
  bytes borrowed_ret = bytes owned_ret /\
  obligation borrowed_ret <> obligation owned_ret.
Proof. split; [reflexivity | split; [reflexivity | discriminate]]. Qed.

(* ---- (2) guessing either way is unsound -------------------------- *)

Theorem borrowed_as_owned_double_frees :
  total_releases borrowed_ret OwnLane = 2.
Proof. reflexivity. Qed.

Theorem owned_as_borrowed_leaks :
  total_releases owned_ret RefLane = 0.
Proof. reflexivity. Qed.

Theorem no_lane_fits_both :
  forall l : Lane, ~ (sound borrowed_ret l /\ sound owned_ret l).
Proof.
  intros l [Hb Ho]. destruct l; unfold sound, total_releases in *;
    simpl in *; discriminate.
Qed.

(* ---- (3) the matching lane releases exactly once ----------------- *)

Definition lane_for (o : Obligation) : Lane :=
  match o with CalleeKeeps => RefLane | CallerReleases => OwnLane end.

Theorem lanes_release_exactly :
  forall r, sound r (lane_for (obligation r)).
Proof.
  intros r. unfold sound, total_releases.
  destruct (obligation r); reflexivity.
Qed.

(* ---- (4) the copy is independent of foreign memory ---------------- *)

Definition Memory := nat -> option nat.

(* Both lanes copy the bytes when the call returns. *)
Definition boundary_copy (m : Memory) (r : ForeignReturn) : option nat :=
  m (ptr r).

Definition foreign_write (m : Memory) (p v : nat) : Memory :=
  fun q => if Nat.eqb q p then Some v else m q.

Definition foreign_free (m : Memory) (p : nat) : Memory :=
  fun q => if Nat.eqb q p then None else m q.

(* Legacy single-call projection; not the storage-reuse safety theorem. *)
Theorem copy_survives_foreign_change :
  forall (m : Memory) r v,
    m (ptr r) = Some (bytes r) ->
    let pergyra := boundary_copy m r in
    let m1 := foreign_write m (ptr r) v in
    let m2 := foreign_free m1 (ptr r) in
    pergyra = Some (bytes r) /\ m2 (ptr r) = None.
Proof.
  intros m r v Hm. cbv zeta.
  unfold boundary_copy, foreign_free, foreign_write. cbv beta.
  rewrite Nat.eqb_refl. split; [exact Hm | reflexivity].
Qed.

Inductive StringValue := CopiedString (contents : nat) | RawPointerString (address : nat).

Definition boundary_value (m : Memory) (r : ForeignReturn) : option StringValue :=
  match boundary_copy m r with
  | Some contents => Some (CopiedString contents)
  | None => None
  end.

Definition string_read (m : Memory) (v : StringValue) : option nat :=
  match v with CopiedString contents => Some contents | RawPointerString p => m p end.

Inductive ForeignAction := ForeignWrite (p v : nat) | ForeignFree (p : nat)
  | ForeignReuse (p v : nat).
Inductive ForeignResult := ForeignChanged | ForeignReleased | ForeignReused
  | ForeignNotLive | ForeignStillLive.

Definition foreign_exec (m : Memory) (a : ForeignAction) : Memory * ForeignResult :=
  match a with
  | ForeignWrite p v =>
      match m p with Some _ => (foreign_write m p v, ForeignChanged)
      | None => (m, ForeignNotLive) end
  | ForeignFree p =>
      match m p with Some _ => (foreign_free m p, ForeignReleased)
      | None => (m, ForeignNotLive) end
  | ForeignReuse p v =>
      match m p with None => (foreign_write m p v, ForeignReused)
      | Some _ => (m, ForeignStillLive) end
  end.

Fixpoint foreign_run (m : Memory) (actions : list ForeignAction) : Memory :=
  match actions with [] => m | a :: rest => foreign_run (fst (foreign_exec m a)) rest end.

Theorem copied_value_read_independent : forall m r value,
  boundary_value m r = Some value ->
  forall later, string_read later value = boundary_copy m r.
Proof.
  intros m r value H later. unfold boundary_value in H.
  destruct (boundary_copy m r) eqn:Hcopy; inversion H; subst.
  simpl. reflexivity.
Qed.

Theorem boundary_value_survives_reuse : forall m r value contents replacement,
  m (ptr r) = Some contents -> boundary_value m r = Some value ->
  let later := foreign_run m [ForeignFree (ptr r); ForeignReuse (ptr r) replacement] in
  string_read later value = Some contents /\
  string_read later (RawPointerString (ptr r)) = Some replacement.
Proof.
  intros m r value contents replacement Hlive Hcopy. cbv zeta.
  split.
  - rewrite (copied_value_read_independent m r value Hcopy).
    exact Hlive.
  - simpl. rewrite Hlive. simpl.
    unfold foreign_free. rewrite Nat.eqb_refl. simpl.
    unfold foreign_write. rewrite Nat.eqb_refl. reflexivity.
Qed.

Example raw_address_reads_reused_storage :
  let m : Memory := fun p => if Nat.eqb p 40 then Some 7 else None in
  let later := foreign_run m [ForeignFree 40; ForeignReuse 40 99] in
  boundary_value m borrowed_ret = Some (CopiedString 7) /\
  string_read later (CopiedString 7) = Some 7 /\
  string_read later (RawPointerString 40) = Some 99 /\
  string_read later (RawPointerString 40) <> Some 7.
Proof. repeat split; try reflexivity; discriminate. Qed.

Theorem missing_foreign_storage_refuses_copy : forall m r,
  m (ptr r) = None -> boundary_value m r = None.
Proof. intros m r H. unfold boundary_value, boundary_copy. rewrite H. reflexivity. Qed.

Theorem repeated_free_refused : forall m p contents,
  m p = Some contents ->
  snd (foreign_exec (fst (foreign_exec m (ForeignFree p))) (ForeignFree p)) = ForeignNotLive.
Proof.
  intros m p contents H. unfold foreign_exec at 2. rewrite H. simpl.
  unfold foreign_exec, foreign_free. rewrite Nat.eqb_refl. reflexivity.
Qed.

Print Assumptions boundary_value_survives_reuse.
