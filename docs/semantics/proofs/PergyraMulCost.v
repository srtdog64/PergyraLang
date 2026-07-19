(*
  PergyraMulCost.v -- the current multiplication boundary, stated honestly.

  The attached transpose argument is an asymptotic statement about a family of
  n-bit integer algorithms.  The beta Pergyra surface is different: Int is a
  fixed-width signed 32-bit value, and multiplication is a checked operation
  which either returns the mathematical product (when it is representable) or
  fails closed.  This file proves that contract and records the scope boundary
  explicitly.  It does NOT prove the tape no-coding-gain lemma, the matrix
  transposition lower bound, or a wall-clock speedup.

  The Coq model is for the explicit `CheckedMul` runtime boundary.  The bare
  `a * b` operator is a separate native machine-arithmetic lowering today and
  is not silently reclassified as checked by this proof.

  `pgy_int_mul_cost` is an abstract backend-step cost, not a claim about CPU
  cycles.  A real speed comparison belongs to the companion benchmark gate,
  which compares the generated C/LLVM executables with a hand-written C
  workload and checks output equality first.
*)

Require Import Coq.ZArith.ZArith.
Require Import Coq.Bool.Bool.
Require Import Coq.micromega.Lia.
Open Scope Z_scope.

Definition PGY_INT_MIN : Z := -2147483648.
Definition PGY_INT_MAX : Z :=  2147483647.

Definition representable (v : Z) : Prop :=
  PGY_INT_MIN <= v <= PGY_INT_MAX.

Definition representableb (v : Z) : bool :=
  (PGY_INT_MIN <=? v) && (v <=? PGY_INT_MAX).

Lemma representableb_true_iff : forall v,
  representableb v = true <-> representable v.
Proof.
  intro v. unfold representableb, representable.
  rewrite andb_true_iff, !Z.leb_le. tauto.
Qed.

(* This is the semantic contract of pgy_checked_mul_i32_export. *)
Definition checked_mul (a b : Z) : option Z :=
  if representableb (a * b)
  then Some (a * b)
  else None.

Theorem checked_mul_some_exact : forall a b v,
  checked_mul a b = Some v -> v = a * b.
Proof.
  intros a b v H.
  unfold checked_mul in H.
  destruct (representableb (a * b)) eqn:Hr; [|discriminate].
  injection H; intro; subst v; reflexivity.
Qed.

Theorem checked_mul_some_representable : forall a b v,
  checked_mul a b = Some v -> representable v.
Proof.
  intros a b v H.
  unfold checked_mul in H.
  destruct (representableb (a * b)) eqn:Hr; [|discriminate].
  injection H; intro; subst v.
  apply representableb_true_iff. exact Hr.
Qed.

Theorem checked_mul_none_iff : forall a b,
  checked_mul a b = None <-> ~ representable (a * b).
Proof.
  intros a b. unfold checked_mul.
  destruct (representableb (a * b)) eqn:Hr.
  - split; [discriminate |].
    intro Hn. exfalso. apply Hn.
    apply representableb_true_iff. exact Hr.
  - split.
    + intros _. intro Hrepr.
      apply representableb_true_iff in Hrepr.
      rewrite Hrepr in Hr. discriminate.
    + intros _. reflexivity.
Qed.

Theorem checked_mul_total : forall a b,
  checked_mul a b = None \/ exists v, checked_mul a b = Some v.
Proof.
  intros a b. destruct (checked_mul a b) eqn:H.
  - right. exists z. reflexivity.
  - left. reflexivity.
Qed.

(* ------------------------------------------------------------------ *)
(* Scope boundary: the current surface has one fixed 32-bit word.      *)
(* ------------------------------------------------------------------ *)

Definition pgy_int_width_bits : nat := 32%nat.
Definition pgy_int_operand_bits (_ : Z) : nat := pgy_int_width_bits.

Theorem pgy_int_operand_width_fixed : forall x,
  pgy_int_operand_bits x = 32%nat.
Proof. intro x. reflexivity. Qed.

(* Abstract compiler-step cost.  This is deliberately not a hardware-cycle
   model: checked overflow guards and backend/runtime call boundaries are
   measured by the executable benchmark, not smuggled into this theorem. *)
Definition pgy_int_mul_cost (_a _b : Z) : nat := 1%nat.

Theorem pgy_int_mul_cost_constant : forall a b,
  pgy_int_mul_cost a b = 1%nat.
Proof. intros a b. reflexivity. Qed.

Theorem pgy_int_cost_has_no_variable_bit_parameter : forall a b n,
  pgy_int_operand_bits a = n ->
  pgy_int_operand_bits b = n ->
  n = pgy_int_width_bits.
Proof.
  intros a b n Ha Hb. rewrite pgy_int_operand_width_fixed in Ha.
  symmetry. exact Ha.
Qed.
