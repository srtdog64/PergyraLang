(*
  Pergyra Formal Semantics -- Option try-propagation desugar equivalence
  Target: docs/147 §2 (the '?' let-initializer contract) and the 2026-07-03
          adoption waves that replaced IsSome/UnwrapOption rituals with '?'.
  Status: machine-verified (coqc, 0 admits / 0 axioms). All close with Qed.

  The surface contract under proof:

      let x: T = e?;  rest        (enclosing function returns Option<U>)

  desugars to "if e is Some, bind its payload and continue; if e is None,
  return None of the ENCLOSING option type". The adoption waves argued, with
  byte-diff probes, that this is exactly the 4-line ritual

      let o = e; if !IsSome(o) { return None; } let x = UnwrapOption(o); rest

  This file promotes that probe argument to theorems:

    1. try_ritual_equiv -- the desugar equals the ritual for every operand and
       every continuation, and for EVERY unwrap-default: the guard dominates
       the unwrap, so UnwrapOption's None branch is unobservable here (the
       runtime panic in pgy_option_unwrap is dead code under the guard).
    2. try_none_propagates / try_some_binds -- the two clauses, named.
    3. try_bind_assoc -- chained '?' lets flatten: nesting order of adopted
       try-sites cannot change the result (covers multi-'?' functions such as
       FindRoutineByOwnerName).
    4. Cross-payload propagation is a TYPING fact here: try_bind's result type
       option U is independent of the operand payload T (Option<Int> operand
       inside an Option<String> function -- the LLVM 2-field rebuild branch).
       The Result mirror try_bind_r shows the asymmetry: the error payload E
       must be SHARED between operand and result because the Err value itself
       flows (try_result_err_carries); None carries nothing, so Option is free.

  Negative scope: this models the PROPAGATION form only (enclosing function
  returns Option/Result). The checked-unwrap form ('?' outside such a
  function panics option-unwrap-none) is a runtime fail-closed contract in
  the CheckedArith/GuardCalculus style, not an equivalence, and is not
  modeled here. Nor does this prove the C/LLVM emitters implement try_bind --
  that is the backend-compare fixture try_operator_option's job.
*)

Section OptionTry.

Variables T U : Type.

(* Desugared '?': bind the Some payload into the continuation, or short-out
   with the enclosing function's None. *)
Definition try_bind (e : option T) (k : T -> option U) : option U :=
  match e with
  | Some x => k x
  | None => None
  end.

(* The ritual the adoption waves removed. `d` models UnwrapOption's
   unreachable-on-None output slot. *)
Definition is_some (e : option T) : bool :=
  match e with Some _ => true | None => false end.

Definition unwrap_or (e : option T) (d : T) : T :=
  match e with Some x => x | None => d end.

Definition ritual_bind (e : option T) (d : T) (k : T -> option U) : option U :=
  if is_some e then k (unwrap_or e d) else None.

Theorem try_ritual_equiv : forall e d k,
  try_bind e k = ritual_bind e d k.
Proof.
  intros [x|] d k; reflexivity.
Qed.

Theorem try_none_propagates : forall k,
  try_bind None k = None.
Proof. reflexivity. Qed.

Theorem try_some_binds : forall x k,
  try_bind (Some x) k = k x.
Proof. reflexivity. Qed.

End OptionTry.

(* Chained '?' sites flatten: adopting '?' at nested call layers (wave-1's
   RoutineNameEnd inside FindRoutineByOwnerName) cannot change the result. *)
Theorem try_bind_assoc : forall (A B C : Type)
    (e : option A) (k1 : A -> option B) (k2 : B -> option C),
  try_bind B C (try_bind A B e k1) k2
    = try_bind A C e (fun x => try_bind B C (k1 x) k2).
Proof.
  intros A B C [x|] k1 k2; reflexivity.
Qed.

(* Result mirror: the Err payload flows through propagation, so the error
   type E is shared between operand and result -- the typing asymmetry that
   makes Option's cross-payload propagation free and Result's constrained. *)
Section ResultTry.

Variables E A B : Type.

Definition try_bind_r (e : sum E A) (k : A -> sum E B) : sum E B :=
  match e with
  | inr a => k a
  | inl err => inl err
  end.

Theorem try_result_err_carries : forall (err : E) (k : A -> sum E B),
  try_bind_r (inl err) k = inl err.
Proof. reflexivity. Qed.

Theorem try_result_ok_binds : forall (a : A) (k : A -> sum E B),
  try_bind_r (inr a) k = k a.
Proof. reflexivity. Qed.

End ResultTry.
