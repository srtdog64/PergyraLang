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
    (4) copy_survives_foreign_change:
        both lanes copy at the boundary, so a later foreign write or
        free does not change the Pergyra string. The raw pointer never
        becomes a Pergyra value.

  So the two lanes are compositions of two units -- copy at the boundary
  and the release obligation -- and a `-> String` with no lane is refused.

  Honest scope: strings are one number; memory is a map from pointer to
  contents; release counts stand in for the allocator.
*)

Require Import Coq.Arith.PeanoNat.

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
