(*
  Pergyra Formal Semantics - Mechanized Sketch
  Target: red-team R11 -- what `Now()` returns.
  Status: proof-sketch; not beta-closure evidence unless checked by CI.

  `Now()` read GetTickCount64 on Windows and CLOCK_REALTIME on POSIX and
  narrowed both to a 32-bit Int. On Linux it went negative in CI. This
  file separates the units a clock API needs:

    (1) monotonic_not_from_wall / wall_not_from_monotonic:
        an elapsed-time clock and a wall clock are independent readings;
        neither is a function of the other (a wall clock can be set back,
        and two machines agree on elapsed time but not on the date).
    (2) int32_breaks_monotonic:
        narrowing a monotonic reading to a signed 32-bit Int makes it go
        backwards at 2^31 ms (about 24.8 days).
    (3) long_keeps_monotonic:
        a signed 64-bit reading is the value itself for every reading a
        program can see (below 2^63 ms), so monotonic stays monotonic.

  The adopted Now contract is Long milliseconds from a monotonic host clock.
  pgy_runtime_host_clock.h is the shared C/linked sample owner. This model
  checks range/unit properties, not host hardware or runtime refinement.
  A separate UnixTimeMs wall-clock API remains a proposal, not implemented
  merely by this theorem; neither clock can be recovered from the other.

  Honest scope: readings are integers in milliseconds; a trace is a pair
  of functions from event index to reading.
*)

Require Import Stdlib.ZArith.ZArith.
Require Import Stdlib.micromega.Lia.
Open Scope Z_scope.

Record Trace := {
  mono : nat -> Z;   (* elapsed-time clock *)
  wall : nat -> Z    (* wall clock, may be adjusted *)
}.

Definition monotonic (f : nat -> Z) : Prop :=
  forall i j, (i <= j)%nat -> f i <= f j.

(* Two traces with the same elapsed time; in the second the wall clock was
   set back by an hour between the events. *)
Definition steady : Trace := {| mono := fun i => Z.of_nat i * 1000;
                                wall := fun i => 1000000 + Z.of_nat i * 1000 |}.
Definition set_back : Trace := {| mono := fun i => Z.of_nat i * 1000;
                                  wall := fun i => if Nat.eqb i 0%nat then 1000000
                                                   else 1000000 - 3600000 |}.

(* ---- (1) the two clocks are independent -------------------------- *)

Theorem monotonic_not_from_wall :
  (forall i, mono steady i = mono set_back i) /\
  wall steady 1%nat <> wall set_back 1%nat.
Proof.
  split.
  - intros i. reflexivity.
  - vm_compute. intros H. discriminate H.
Qed.

Definition other_machine : Trace := {| mono := fun i => 5 + Z.of_nat i * 1000;
                                       wall := wall steady |}.

Theorem wall_not_from_monotonic :
  (forall i, wall steady i = wall other_machine i) /\
  mono steady 0%nat <> mono other_machine 0%nat.
Proof.
  split.
  - intros i. reflexivity.
  - vm_compute. intros H. discriminate H.
Qed.

Theorem set_back_wall_not_monotonic : ~ monotonic (wall set_back).
Proof.
  intros H. unfold monotonic in H.
  specialize (H 0%nat 1%nat ltac:(lia)). simpl in H. lia.
Qed.

(* ---- (2) a 32-bit Int breaks monotonicity ------------------------ *)

Definition signed (bits : Z) (z : Z) : Z :=
  let m := 2 ^ bits in
  let u := z mod m in
  if u <? 2 ^ (bits - 1) then u else u - m.

Theorem int32_breaks_monotonic :
  2147483647 <= 2147483648 /\
  signed 32 2147483648 < signed 32 2147483647.
Proof. split; [lia | vm_compute; reflexivity]. Qed.

(* ---- (3) a 64-bit Long keeps it ----------------------------------- *)

Lemma signed64_identity :
  forall z, 0 <= z < 2 ^ 63 -> signed 64 z = z.
Proof.
  intros z Hz. unfold signed. cbv zeta.
  assert (Hm : z mod 2 ^ 64 = z).
  { apply Z.mod_small. split; [lia |].
    assert (H6364 : 2 ^ 63 < 2 ^ 64) by (vm_compute; reflexivity).
    lia. }
  rewrite Hm.
  replace (64 - 1) with 63 by lia.
  destruct (z <? 2 ^ 63) eqn:E.
  - reflexivity.
  - apply Z.ltb_ge in E. lia.
Qed.

Theorem long_keeps_monotonic :
  forall f : nat -> Z,
    monotonic f ->
    (forall i, 0 <= f i < 2 ^ 63) ->
    monotonic (fun i => signed 64 (f i)).
Proof.
  intros f Hmono Hrange i j Hij. unfold monotonic in Hmono. cbv beta.
  rewrite (signed64_identity (f i) (Hrange i)).
  rewrite (signed64_identity (f j) (Hrange j)).
  apply Hmono. exact Hij.
Qed.
