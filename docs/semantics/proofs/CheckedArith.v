(*
  CheckedArith.v  --  fail-closed checked integer division/modulo (no UB).

  Companion to docs/100b_beta_p0_semantics_systems_air.md (UB model) and to the
  runtime guards in src/runtime/pgy_runtime_lib_authority_file_core.h /
  pgy_runtime_panic_checked_inline.h.

  The "dark side" of dual C/LLVM emission: signed integer division in C is UB on
  exactly two inputs -- divide-by-zero (rhs = 0) and signed overflow
  (lhs = INT_MIN, rhs = -1, whose true quotient +2^31 is not representable). An
  optimizing backend may miscompile either into a silently wrong value or a
  hardware trap, which would invalidate every downstream slot/witness guarantee.

  Pergyra routes BOTH backends' division/modulo through one checked runtime
  helper that fail-closes (panics) on those inputs. This file models that helper
  over Z (with the i32 representable interval) and proves it is TOTAL and
  fail-closed:

    - it returns None (panic) on EXACTLY the two C-UB inputs and on nothing else
      [div_none_iff], so it never silently inherits backend UB;
    - whenever it returns a value, that value is the true truncated quotient
      [div_some_quot] AND is representable in i32 [div_some_representable], so no
      silent wrong answer and no out-of-range escape;
    - modulo only ever panics on divide-by-zero: INT_MIN % -1 returns 0 (the true
      remainder), never UB [mod_none_iff, mod_some_representable].

  Truncated (C) semantics are modelled with Z.quot / Z.rem (NOT Z.div / Z.mod,
  which floor). A single total spec consumed by both backends is exactly what the
  C == LLVM parity gate observes empirically.
*)

Require Import Coq.ZArith.ZArith.
Require Import Coq.ZArith.Zquot.
Require Import Coq.Bool.Bool.
Require Import Coq.micromega.Lia.
Open Scope Z_scope.

Definition INT_MIN : Z := -2147483648.
Definition INT_MAX : Z :=  2147483647.

Definition representable (v : Z) : Prop := INT_MIN <= v <= INT_MAX.

(* The two inputs on which C signed division/modulo is undefined behaviour. *)
Definition div_ub (a b : Z) : Prop := b = 0 \/ (a = INT_MIN /\ b = -1).

(* ---------------------------------------------------------------- *)
(* The checked helpers (mirror the C runtime functions).            *)
(* ---------------------------------------------------------------- *)

Definition checked_div (a b : Z) : option Z :=
  if b =? 0 then None
  else if andb (a =? INT_MIN) (b =? -1) then None
  else Some (Z.quot a b).

(* INT_MIN % -1 has true remainder 0 (representable); only /0 is UB for mod. *)
Definition checked_mod (a b : Z) : option Z :=
  if b =? 0 then None
  else if andb (a =? INT_MIN) (b =? -1) then Some 0
  else Some (Z.rem a b).

(* ---------------------------------------------------------------- *)
(* Magnitude helper lemmas.                                         *)
(* ---------------------------------------------------------------- *)

Lemma abs_int_min : Z.abs INT_MIN = 2147483648.
Proof. reflexivity. Qed.

Lemma quot_abs_le : forall a b, b <> 0 -> Z.abs (Z.quot a b) <= Z.abs a.
Proof.
  intros a b Hb.
  rewrite <- Z.quot_abs by exact Hb.
  assert (Hpos : 0 < Z.abs b).
  { destruct (Z.abs_spec b) as [[Hge He] | [Hlt He]]; lia. }
  apply Z.quot_le_upper_bound.
  - exact Hpos.
  - nia.
Qed.

Lemma abs_ge_2 : forall b, b <> 0 -> b <> 1 -> b <> -1 -> 2 <= Z.abs b.
Proof.
  intros b H0 H1 Hm1.
  destruct (Z.abs_spec b) as [[Hge He] | [Hlt He]]; lia.
Qed.

(* The heart of UB-freedom: outside the two UB inputs the truncated quotient of
   any representable operand is itself representable -- the guard catches the one
   overflowing case (INT_MIN / -1 = +2^31), and nothing else can escape i32. *)
Lemma quot_representable : forall a b,
  representable a -> b <> 0 -> ~ (a = INT_MIN /\ b = -1) ->
  representable (Z.quot a b).
Proof.
  intros a b Ha Hb Hne.
  unfold representable in *.
  pose proof (quot_abs_le a b Hb) as Hle.
  destruct (Z.eq_dec a INT_MIN) as [Heq | Hane].
  - (* a = INT_MIN, hence b <> -1 (and b <> 0). *)
    subst a.
    assert (Hbm1 : b <> -1) by (intro Hc; apply Hne; split; [reflexivity | exact Hc]).
    destruct (Z.eq_dec b 1) as [Hb1 | Hbn1].
    + subst b. rewrite Z.quot_1_r. unfold INT_MIN, INT_MAX in *. lia.
    + assert (Hge2 : 2 <= Z.abs b) by (apply abs_ge_2; auto).
      assert (Hqle : Z.abs (Z.quot INT_MIN b) <= 1073741824).
      { rewrite <- Z.quot_abs by auto.
        apply Z.quot_le_upper_bound; [lia |].
        rewrite abs_int_min. nia. }
      apply Z.abs_le in Hqle. unfold INT_MIN, INT_MAX in *. lia.
  - (* a <> INT_MIN, so |a| <= INT_MAX. *)
    assert (Haabs : Z.abs a <= 2147483647).
    { apply Z.abs_le. unfold INT_MIN, INT_MAX in *. lia. }
    assert (Hqabs : Z.abs (Z.quot a b) <= 2147483647) by lia.
    apply Z.abs_le in Hqabs. unfold INT_MIN, INT_MAX in *. lia.
Qed.

(* ---------------------------------------------------------------- *)
(* Boolean-guard bridging lemmas.                                   *)
(* ---------------------------------------------------------------- *)

Lemma guard_true_iff : forall a b,
  (andb (a =? INT_MIN) (b =? -1)) = true <-> (a = INT_MIN /\ b = -1).
Proof.
  intros a b. rewrite andb_true_iff, !Z.eqb_eq. tauto.
Qed.

Lemma guard_false_iff : forall a b,
  (andb (a =? INT_MIN) (b =? -1)) = false <-> ~ (a = INT_MIN /\ b = -1).
Proof.
  intros a b. split.
  - intros Hf [Ha Hb].
    assert (Ht : andb (a =? INT_MIN) (b =? -1) = true)
      by (apply guard_true_iff; split; assumption).
    rewrite Ht in Hf. discriminate.
  - intro Hn. destruct (andb (a =? INT_MIN) (b =? -1)) eqn:E; [|reflexivity].
    apply guard_true_iff in E. contradiction.
Qed.

(* ---------------------------------------------------------------- *)
(* Division: fail-closed exactly on UB; otherwise correct + in range. *)
(* ---------------------------------------------------------------- *)

Theorem div_none_iff : forall a b,
  checked_div a b = None <-> div_ub a b.
Proof.
  intros a b. unfold checked_div, div_ub.
  destruct (b =? 0) eqn:Hb0.
  - apply Z.eqb_eq in Hb0. split; [tauto | reflexivity].
  - apply Z.eqb_neq in Hb0.
    destruct (andb (a =? INT_MIN) (b =? -1)) eqn:Eg.
    + apply guard_true_iff in Eg. split; [tauto | reflexivity].
    + split.
      * discriminate.
      * intros [H | [Ha Hb]]; [contradiction|].
        exfalso. apply (proj1 (guard_false_iff a b) Eg). split; assumption.
Qed.

Theorem div_some_quot : forall a b v,
  checked_div a b = Some v -> v = Z.quot a b.
Proof.
  intros a b v H. unfold checked_div in H.
  destruct (b =? 0); [discriminate|].
  destruct (andb (a =? INT_MIN) (b =? -1)); [discriminate|].
  injection H; intro; subst; reflexivity.
Qed.

Theorem div_some_representable : forall a b v,
  representable a -> checked_div a b = Some v -> representable v.
Proof.
  intros a b v Ha H.
  assert (Hv : v = Z.quot a b) by (apply div_some_quot in H; exact H).
  assert (Hnone : checked_div a b <> None) by (rewrite H; discriminate).
  assert (Hnub : ~ div_ub a b) by (intro Hub; apply Hnone; apply div_none_iff; exact Hub).
  unfold div_ub in Hnub.
  assert (Hb : b <> 0) by tauto.
  assert (Hne : ~ (a = INT_MIN /\ b = -1)) by tauto.
  subst v. apply quot_representable; assumption.
Qed.

(* Totality: every input lands in a clean panic or a value -- never stuck/UB. *)
Theorem div_total : forall a b,
  checked_div a b = None \/ exists v, checked_div a b = Some v.
Proof.
  intros a b. destruct (checked_div a b) eqn:E.
  - right; exists z; reflexivity.
  - left; reflexivity.
Qed.

(* ---------------------------------------------------------------- *)
(* Modulo: panics ONLY on divide-by-zero; INT_MIN % -1 = 0 (no UB).  *)
(* ---------------------------------------------------------------- *)

Theorem mod_none_iff : forall a b,
  checked_mod a b = None <-> b = 0.
Proof.
  intros a b. unfold checked_mod.
  destruct (b =? 0) eqn:Hb0.
  - apply Z.eqb_eq in Hb0. split; [tauto | reflexivity].
  - apply Z.eqb_neq in Hb0.
    destruct (andb (a =? INT_MIN) (b =? -1)).
    + split; [discriminate | intro; contradiction].
    + split; [discriminate | intro; contradiction].
Qed.

Theorem mod_some_representable : forall a b v,
  representable a -> checked_mod a b = Some v -> representable v.
Proof.
  intros a b v Ha H. unfold checked_mod in H.
  destruct (b =? 0) eqn:Hb0; [discriminate|].
  apply Z.eqb_neq in Hb0.
  destruct (andb (a =? INT_MIN) (b =? -1)) eqn:Eg.
  - (* INT_MIN % -1 modelled as 0 *)
    injection H; intro; subst v. unfold representable, INT_MIN, INT_MAX. lia.
  - injection H; intro; subst v.
    destruct (Z_lt_le_dec (Z.abs a) (Z.abs b)) as [Hlt | Hle].
    + (* |a| < |b|: the remainder is a itself, which is representable. *)
      assert (Hra : Z.rem a b = a)
        by (apply Z.rem_small_iff; [exact Hb0 | exact Hlt]).
      rewrite Hra. exact Ha.
    + (* |b| <= |a|: |rem| < |b| <= |a| <= 2^31, so |rem| <= 2^31 - 1. *)
      pose proof (Z.rem_bound_abs a b Hb0) as Hrb.
      assert (Haabs : Z.abs a <= 2147483648).
      { apply Z.abs_le. unfold representable, INT_MIN, INT_MAX in Ha. lia. }
      assert (Hrabs : Z.abs (Z.rem a b) <= 2147483647) by lia.
      apply Z.abs_le in Hrabs. unfold representable, INT_MIN, INT_MAX. lia.
Qed.
