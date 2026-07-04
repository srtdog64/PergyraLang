(*
  Pergyra Formal Semantics -- Guard Verdicts <-> Runtime Panic-Class Witnesses
  (docs/155 SS3: "GuardCalculus <-> implementation binding, widened" --
   the AIRBinding lineage applied to the fail-closed guard policy.)
  Status: machine-verified (coqc, 0 admits / 0 axioms). All theorems close
          with Qed.

  GuardCalculus.v proves the fail-closed POLICY sound (no_silent_ub) with
  its `coverage` hypothesis "empirically witnessed by the gates". This file
  narrows that model<->implementation gap one step, AIRBinding-style: it
  binds each op class of the guard policy to the NAMED runtime panic
  class(es) that witness it in the real runtime
  (src/runtime/pgy_runtime_panic_contract.h), and proves the shape
  properties the binding needs:

    1. guarded_ops_witnessed   -- every op class the pgy policy marks
                                  Guarded carries at least one named
                                  runtime panic class. A Guarded verdict
                                  with no emission identity would be an
                                  unfalsifiable claim; this rules it out.
    2. can_be_bad_has_witness  -- STRONGER: every op class that CAN
                                  misbehave carries a named witness, even
                                  where the verdict is Proven
                                  (OpSlotRelease keeps released-slot /
                                  double-release as always-on backstops).
                                  This is the defense-in-depth decision
                                  ("slot checks always-on"), mechanized.
    3. witness_disjoint        -- a panic class witnesses EXACTLY ONE op
                                  family: distinct failures cannot collapse
                                  into the same class. This is the
                                  "logs correspond 1:1 with code paths"
                                  discipline lifted to panic classes
                                  (diagnosability of the fail-close).
    4. unwitnessed_cannot_be_bad -- the only unwitnessed op class is one
                                  with no bad instances at all (OpPure).

  The companion smoke (formal_semantics_smoke.sh, GUARD_WITNESS_BINDING
  block) locks the OTHER half of the correspondence: each PanicClass row
  below must exist as a string constant in pgy_runtime_panic_contract.h.
  Renaming or removing a runtime panic class turns the smoke RED until the
  model row and the code re-align -- the vocabulary cannot drift silently.

  Negative scope: this does NOT prove the emitted guards FIRE correctly
  (that is the checkedarith-failclosed / memory-safety-failclosed /
  lifecycle gate fixtures), nor that guard emission covers every dynamic
  path (twin parity gates). Runtime panic classes OUTSIDE the GuardCalculus
  op universe (oom, authority-mismatch, capability-denied, budget-exceeded,
  internal-invariant, plus Result/Option unwrap reasons) belong to other
  fact families (allocation, authority, sandbox, host contracts) and are
  deliberately not modeled here -- listing them prevents the gap from being
  silent, it does not close it.
*)

Require Import Coq.Lists.List.
Import ListNotations.

Section GuardWitnessBinding.

(* ================================================================ *)
(* Minimal GuardCalculus slice (verbatim op universe + pgy policy).  *)
(* ================================================================ *)

Inductive OpClass : Type :=
  | OpDiv
  | OpIndex
  | OpAddMul
  | OpSecureToken
  | OpLifecycle
  | OpSlotRelease
  | OpPure.

Definition can_be_bad (o : OpClass) : bool :=
  match o with
  | OpPure => false
  | _ => true
  end.

Inductive Verdict : Type := Proven | Guarded | Rejected | Unhandled.

Definition pgy_policy (o : OpClass) : Verdict :=
  match o with
  | OpPure        => Proven
  | OpSlotRelease => Proven
  | _             => Guarded
  end.

(* ================================================================ *)
(* The runtime panic-class registry rows this model binds to.        *)
(* Source of truth: src/runtime/pgy_runtime_panic_contract.h --      *)
(* the string in the comment is the exact C constant the smoke greps.*)
(* ================================================================ *)

Inductive PanicClass : Type :=
  | PcDivideByZero          (* "divide-by-zero"          *)
  | PcArithmeticOverflow    (* "arithmetic-overflow"     *)
  | PcOutOfBounds           (* "out-of-bounds"           *)
  | PcInvalidSecureToken    (* "invalid-secure-token"    *)
  | PcInvalidLifecycleState (* "invalid-lifecycle-state" *)
  | PcReleasedSlot          (* "released-slot"           *)
  | PcDoubleRelease.        (* "double-release"          *)

(* The binding table: which named runtime panic classes witness each op
   class's fail-close. Guarded ops carry their primary witness;
   OpSlotRelease is Proven yet keeps its always-on backstops. *)
Definition witnesses (o : OpClass) : list PanicClass :=
  match o with
  | OpDiv         => [PcDivideByZero]
  | OpIndex       => [PcOutOfBounds]
  | OpAddMul      => [PcArithmeticOverflow]
  | OpSecureToken => [PcInvalidSecureToken]
  | OpLifecycle   => [PcInvalidLifecycleState]
  | OpSlotRelease => [PcReleasedSlot; PcDoubleRelease]
  | OpPure        => []
  end.

(* ================================================================ *)
(* 1. Every Guarded verdict names its emission.                      *)
(* ================================================================ *)

Theorem guarded_ops_witnessed :
  forall o, pgy_policy o = Guarded -> witnesses o <> [].
Proof.
  intros o H; destruct o; simpl in *; discriminate.
Qed.

(* ================================================================ *)
(* 2. Defense-in-depth: every UB-capable op class has a witness,     *)
(*    including the Proven ones (backstop).                          *)
(* ================================================================ *)

Theorem can_be_bad_has_witness :
  forall o, can_be_bad o = true -> witnesses o <> [].
Proof.
  intros o H; destruct o; simpl in *; discriminate.
Qed.

(* ================================================================ *)
(* 3. Diagnosability: a panic class witnesses exactly one op family. *)
(* ================================================================ *)

Theorem witness_disjoint :
  forall o1 o2 pc,
    In pc (witnesses o1) -> In pc (witnesses o2) -> o1 = o2.
Proof.
  intros o1 o2 pc H1 H2.
  destruct o1; destruct o2; try reflexivity;
    destruct pc; simpl in H1, H2;
    repeat (destruct H1 as [H1 | H1]; try discriminate H1);
    repeat (destruct H2 as [H2 | H2]; try discriminate H2);
    try contradiction.
Qed.

(* ================================================================ *)
(* 4. The only unwitnessed op class has no bad instances at all.     *)
(* ================================================================ *)

Theorem unwitnessed_cannot_be_bad :
  forall o, witnesses o = [] -> can_be_bad o = false.
Proof.
  intros o H; destruct o; simpl in *; try discriminate; reflexivity.
Qed.

(* Shape corollary tying back to the policy: an unwitnessed op is Proven
   (nothing is left Unhandled or silently Guarded-without-emission). *)
Corollary unwitnessed_is_proven :
  forall o, witnesses o = [] -> pgy_policy o = Proven.
Proof.
  intros o H; destruct o; simpl in *; try discriminate; reflexivity.
Qed.

End GuardWitnessBinding.
