(*
  Pergyra Formal Semantics - Mechanized Sketch
  Target: red-team R13 -- recovering from Int overflow without changing
  what `+` means.
  Status: proof-sketch; not beta-closure evidence unless checked by CI.

  `a + b` on Int wraps (pergyra.int.wrapping.i32.v1); `CheckedAdd` panics.
  A budget counter needs to see overflow as a value. This file shows the
  units behind the three APIs:

    (1) wrap_hides_overflow:
        two sums, one representable and one not, wrap to the same Int,
        so no function of the wrapped result can tell overflow apart.
        Detecting overflow needs its own unit: the representability of
        the exact sum.
    (2) try_add_sound / try_add_complete / try_add_none_iff:
        `TryAdd` returns the exact sum exactly when it is representable,
        and an error otherwise.
    (3) checked_is_try_then_panic:
        `CheckedAdd` is `TryAdd` followed by a panic on the error; it is
        a composition, not a unit.
    (4) wrap_agrees_in_range:
        on representable sums, wrapping and `TryAdd` give the same value,
        so adding the Try family changes no existing program.

  Units: the exact sum and its representability. `+`, `TryAdd` and
  `CheckedAdd` are compositions of them; the same holds for Sub, Mul and
  the Long variants with 64 in place of 32.

  Honest scope: 32-bit signed two's complement modeled on Z.
*)

Require Import Stdlib.Bool.Bool.
Require Import Stdlib.ZArith.ZArith.
Require Import Stdlib.micromega.Lia.
Open Scope Z_scope.

Definition min32 : Z := - 2 ^ 31.
Definition max32 : Z := 2 ^ 31 - 1.

Definition in32 (z : Z) : Prop := min32 <= z <= max32.

Definition in32b (z : Z) : bool := andb (min32 <=? z) (z <=? max32).

Definition wrap32 (z : Z) : Z :=
  let u := z mod 2 ^ 32 in
  if u <? 2 ^ 31 then u else u - 2 ^ 32.

Definition try_add (a b : Z) : option Z :=
  if in32b (a + b) then Some (a + b) else None.

Inductive Outcome := Value (v : Z) | Panic.

Definition checked_add (a b : Z) : Outcome :=
  match try_add a b with
  | Some v => Value v
  | None => Panic
  end.

Lemma in32b_spec : forall z, in32b z = true <-> in32 z.
Proof.
  intros z. unfold in32b, in32. split.
  - intros H. apply andb_prop in H. destruct H as [H1 H2].
    apply Z.leb_le in H1. apply Z.leb_le in H2. split; assumption.
  - intros [H1 H2]. apply andb_true_intro.
    split; apply Z.leb_le; assumption.
Qed.

(* ---- (1) wrapping hides overflow ---------------------------------- *)

Theorem wrap_hides_overflow :
  wrap32 (max32 + 1) = wrap32 (min32 + 0) /\
  ~ in32 (max32 + 1) /\ in32 (min32 + 0).
Proof.
  split; [vm_compute; reflexivity |].
  split; unfold in32, min32, max32.
  - intros [_ H]. vm_compute in H. apply H. reflexivity.
  - split; vm_compute; intros H; discriminate H.
Qed.

(* ---- (2) TryAdd is exactly representability ---------------------- *)

Theorem try_add_sound :
  forall a b v, try_add a b = Some v -> v = a + b /\ in32 v.
Proof.
  intros a b v. unfold try_add.
  destruct (in32b (a + b)) eqn:E; intros H; [| discriminate H].
  injection H as Hv. subst v. split; [reflexivity |].
  apply in32b_spec. exact E.
Qed.

Theorem try_add_complete :
  forall a b, in32 (a + b) -> try_add a b = Some (a + b).
Proof.
  intros a b H. unfold try_add.
  apply in32b_spec in H. rewrite H. reflexivity.
Qed.

Theorem try_add_none_iff :
  forall a b, try_add a b = None <-> ~ in32 (a + b).
Proof.
  intros a b. unfold try_add. split.
  - intros H Hin. apply in32b_spec in Hin. rewrite Hin in H. simpl in H.
    discriminate H.
  - intros Hn. destruct (in32b (a + b)) eqn:E; [| reflexivity].
    exfalso. apply Hn. apply in32b_spec. exact E.
Qed.

(* ---- (3) CheckedAdd is a composition ------------------------------ *)

Theorem checked_is_try_then_panic :
  forall a b,
    checked_add a b = Panic <-> try_add a b = None.
Proof.
  intros a b. unfold checked_add.
  destruct (try_add a b); split; intros H;
    try reflexivity; discriminate H.
Qed.

(* ---- (4) wrapping and TryAdd agree on representable sums ---------- *)

Lemma wrap32_in_range : forall z, in32 z -> wrap32 z = z.
Proof.
  intros z [Hlo Hhi]. unfold min32, max32 in *. unfold wrap32. cbv zeta.
  assert (H3132 : 2 ^ 32 = 2 * 2 ^ 31) by (vm_compute; reflexivity).
  destruct (Z_le_gt_dec 0 z) as [Hnn | Hneg].
  - rewrite (Z.mod_small z (2 ^ 32)) by lia.
    destruct (z <? 2 ^ 31) eqn:E; [reflexivity |].
    apply Z.ltb_ge in E. lia.
  - assert (Hm : z mod 2 ^ 32 = z + 2 ^ 32).
    { rewrite <- (Z.mod_add z 1 (2 ^ 32)) by lia.
      replace (z + 1 * 2 ^ 32) with (z + 2 ^ 32) by lia.
      apply Z.mod_small. lia. }
    rewrite Hm.
    destruct (z + 2 ^ 32 <? 2 ^ 31) eqn:E.
    + apply Z.ltb_lt in E. lia.
    + lia.
Qed.

Theorem wrap_agrees_in_range :
  forall a b v, try_add a b = Some v -> wrap32 (a + b) = v.
Proof.
  intros a b v H. destruct (try_add_sound a b v H) as [Hv Hin].
  subst v. apply wrap32_in_range. exact Hin.
Qed.
