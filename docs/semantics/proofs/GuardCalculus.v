(*
  Pergyra Formal Semantics - Mechanized Sketch
  Target: docs/semantics/20_minimal_verification_position.md
          (fail-closed guard calculus; synthesis fragment #2)
  Status: proof-sketch; not beta-closure evidence unless checked by CI (coqc).

  Scope: this file mechanizes the "minimal verification" position: the
  language's no-UB guarantee is delivered not by a global flow-sensitive
  static proof (the Rust borrow/lifetime discipline) but by a PER-OPERATION
  policy in which every UB-capable operation class is either statically
  Proven safe, dynamically Guarded (fail-closed panic), or Rejected at
  compile time. Three theorems give the position its formal content:

    1. no_silent_ub      -- coverage + kept static promises entail that no
                            execution reaches silent UB: every out-of-domain
                            step is a Panic transition, never corruption.
    2. coverage_is_local -- the entire proof obligation is decided by a
                            finite per-operation table (a fold over the op
                            universe); no program text, flow analysis, or
                            interprocedural reasoning appears in the
                            obligation. This locality is the formal content
                            of "minimal".
    3. guarded_more_permissive_at_equal_safety
                         -- an idealized static-only discipline (no runtime
                            guards) that is sound must REJECT every program
                            containing an op that could misbehave on some
                            input; the guarded policy accepts and runs those
                            programs, panicking exactly on the bad dynamic
                            instances and never reaching UB. Same end
                            guarantee, strictly larger accepted set.

  The model is deliberately honest about the price of the position: the
  Proven verdict carries a PROMISE obligation (proven ops are never
  out-of-domain at runtime), and a broken promise IS modeled as silent UB.
  So the trusted base is exactly: the finite coverage table + the small
  Proven set's promises. In the real compiler those are discharged by the
  always-on runtime guards (memory_safety_failclosed, checked-arith,
  secure-token-reuse, lifecycle gates) and by the static own/ref release
  tracking; the gates are the empirical witnesses of this file's `coverage`
  hypothesis.

  This is also synthesis fragment #2 (after the zone<->ambient fragment):
  slot release, secure tokens, lifecycle states, bounds, and checked
  arithmetic all instantiate the SAME OpClass/guard structure below --
  one umbrella theorem covers every fail-closed axis at once.

  Negative scope: this file does NOT model the surface syntax, the type
  checker, aliasing, concurrency/data races (the channel-only cross-world
  contract is a separate obligation), guard implementation correctness
  (twin-gated separately), or performance. "Static-only" below is the
  idealized zero-runtime-check discipline, not Rust as shipped (Rust itself
  guards bounds/overflow at runtime; the disagreement is only about WHICH
  classes carry the heavyweight static treatment).
*)

Require Import Coq.Lists.List.
Require Import Coq.Bool.Bool.
Import ListNotations.

(* ================================================================ *)
(* 1. Operation classes -- each maps to a real UB-capable lowering  *)
(*    and to the runtime panic class / gate that witnesses it.      *)
(* ================================================================ *)

Inductive OpClass : Type :=
  | OpDiv          (* a / b, a % b        -> class=divide-by-zero / division overflow *)
  | OpIndex        (* arr[i], ArraySet    -> class=out-of-bounds                      *)
  | OpAddMul       (* CheckedAdd/Mul      -> class=arithmetic-overflow                *)
  | OpSecureToken  (* secure read/write   -> class=invalid-secure-token               *)
  | OpLifecycle    (* state-gated method  -> class=invalid-lifecycle-state            *)
  | OpSlotRelease  (* own/ref release     -> statically proven (interprocedural)      *)
  | OpPure.        (* total ops           -> no out-of-domain instance exists         *)

Definition all_ops : list OpClass :=
  [OpDiv; OpIndex; OpAddMul; OpSecureToken; OpLifecycle; OpSlotRelease; OpPure].

Lemma ops_complete : forall o : OpClass, In o all_ops.
Proof. intro o; destruct o; simpl; tauto. Qed.

(* Whether SOME dynamic instance of the class can be out-of-domain.
   Pure ops cannot; slot release is made safe for every reachable instance
   by the static layer (that is exactly its promise, below). *)
Definition can_be_bad (o : OpClass) : bool :=
  match o with
  | OpPure => false
  | _ => true
  end.

(* ================================================================ *)
(* 2. Policy: the per-op verdict table.                             *)
(* ================================================================ *)

Inductive Verdict : Type := Proven | Guarded | Rejected | Unhandled.

Definition verdict_eqb (a b : Verdict) : bool :=
  match a, b with
  | Proven, Proven | Guarded, Guarded
  | Rejected, Rejected | Unhandled, Unhandled => true
  | _, _ => false
  end.

Definition Policy := OpClass -> Verdict.

(* The whole obligation, as data: no op class is left Unhandled. *)
Definition coverage (p : Policy) : Prop :=
  forall o : OpClass, p o <> Unhandled.

(* ================================================================ *)
(* 3. Programs and the fail-closed operational semantics.           *)
(*    A program is a finite trace of dynamic instances: the op      *)
(*    class plus whether THIS instance is out-of-domain ("bad").    *)
(* ================================================================ *)

Definition Instance : Type := (OpClass * bool)%type.
Definition Program := list Instance.

Inductive Outcome : Type := OK | Panic | UB.

(* One honest asymmetry: a Proven op whose instance is nevertheless bad is
   SILENT UB -- the static layer lied and no guard exists to catch it. This
   is what makes the Proven promise a real obligation, not bookkeeping. *)
Fixpoint run (p : Policy) (prog : Program) : Outcome :=
  match prog with
  | [] => OK
  | (o, bad) :: rest =>
      match p o, bad with
      | Unhandled, true => UB
      | Proven,    true => UB
      | Guarded,   true => Panic
      | Rejected,  _    => Panic (* unreachable under well_formed *)
      | _,         false => run p rest
      end
  end.

(* Compile-time facts about a program. *)
Definition well_formed (p : Policy) (prog : Program) : Prop :=
  forall o b, In (o, b) prog -> p o <> Rejected.

(* The static layer's kept promise: instances of Proven ops are never bad.
   In the real compiler this is the interprocedural own/ref release
   discipline (and the definition of pure ops). *)
Definition proven_promise (p : Policy) (prog : Program) : Prop :=
  forall o b, In (o, b) prog -> p o = Proven -> b = false.

(* Instances are meaningful: an op that CANNOT be bad has no bad instance. *)
Definition instances_meaningful (prog : Program) : Prop :=
  forall o b, In (o, b) prog -> b = true -> can_be_bad o = true.

(* ================================================================ *)
(* 4. Theorem 1 -- fail-closed soundness (no silent UB).            *)
(* ================================================================ *)

Theorem no_silent_ub :
  forall (p : Policy) (prog : Program),
    coverage p ->
    proven_promise p prog ->
    run p prog <> UB.
Proof.
  intros p prog Hcov.
  induction prog as [| [o bad] rest IH]; intros Hprom; simpl.
  - discriminate.
  - assert (Hrest : proven_promise p rest).
    { intros o' b' Hin Hp. apply (Hprom o' b'); [right; exact Hin | exact Hp]. }
    destruct (p o) eqn:Hp; destruct bad eqn:Hb;
      try (apply IH; exact Hrest);
      try discriminate.
    + (* Proven, true: contradicts the kept promise *)
      exfalso.
      assert (true = false) as Hcontra.
      { apply (Hprom o true); [left; reflexivity | exact Hp]. }
      discriminate Hcontra.
    + (* Unhandled, true: contradicts coverage *)
      exfalso. apply (Hcov o). exact Hp.
Qed.

(* ================================================================ *)
(* 5. Theorem 2 -- the obligation is LOCAL: a fold over the finite  *)
(*    op universe decides it. No program text, no flow analysis.    *)
(* ================================================================ *)

Definition coverage_check (p : Policy) : bool :=
  forallb (fun o => negb (verdict_eqb (p o) Unhandled)) all_ops.

Lemma verdict_eqb_true : forall a b, verdict_eqb a b = true -> a = b.
Proof. intros a b; destruct a; destruct b; simpl; intro H; try discriminate; reflexivity. Qed.

Lemma verdict_eqb_refl : forall a, verdict_eqb a a = true.
Proof. destruct a; reflexivity. Qed.

Theorem coverage_is_local :
  forall p : Policy, coverage_check p = true <-> coverage p.
Proof.
  intro p; split.
  - intros Hchk o Ho.
    assert (Hin := ops_complete o).
    unfold coverage_check in Hchk.
    rewrite forallb_forall in Hchk.
    specialize (Hchk o Hin).
    rewrite Ho in Hchk. simpl in Hchk. discriminate.
  - intro Hcov.
    unfold coverage_check.
    rewrite forallb_forall.
    intros o _.
    destruct (p o) eqn:Hp; simpl; try reflexivity.
    exfalso. apply (Hcov o). exact Hp.
Qed.

(* ================================================================ *)
(* 6. The concrete Pergyra policy, and the umbrella corollary that  *)
(*    covers every fail-closed axis at once (synthesis content).    *)
(* ================================================================ *)

Definition pgy_policy : Policy :=
  fun o =>
    match o with
    | OpPure        => Proven   (* total: no bad instance exists            *)
    | OpSlotRelease => Proven   (* static own/ref interprocedural tracking  *)
    | _             => Guarded  (* always-on fail-closed runtime guards     *)
    end.

Lemma pgy_coverage : coverage pgy_policy.
Proof. intro o; destruct o; simpl; discriminate. Qed.

(* The static layer's promise, restated for the concrete policy: pure ops
   are never bad (by meaning), and the own/ref discipline keeps release
   instances in-domain.  The second conjunct is the REAL residual static
   obligation of the whole position. *)
Definition pgy_promises (prog : Program) : Prop :=
  instances_meaningful prog /\
  (forall b, In (OpSlotRelease, b) prog -> b = false).

Corollary pergyra_no_silent_ub :
  forall prog : Program,
    pgy_promises prog ->
    run pgy_policy prog <> UB.
Proof.
  intros prog [Hmean Hrel].
  apply no_silent_ub.
  - exact pgy_coverage.
  - intros o b Hin Hp.
    destruct o; simpl in Hp; try discriminate.
    + (* OpSlotRelease *) apply (Hrel b). exact Hin.
    + (* OpPure: a bad instance would contradict can_be_bad = false *)
      destruct b; [ | reflexivity ].
      assert (Hc : can_be_bad OpPure = true) by (apply (Hmean OpPure true Hin); reflexivity).
      simpl in Hc. discriminate.
Qed.

(* ================================================================ *)
(* 7. Theorem 3 -- against the idealized static-only discipline:    *)
(*    equal end guarantee, strictly smaller accepted program set.   *)
(* ================================================================ *)

Definition static_only (p : Policy) : Prop :=
  forall o, p o = Proven \/ p o = Rejected.

(* Sound static-only: Proven may be granted only to ops with NO bad
   instance on ANY input (there is no guard to catch a miss). *)
Definition static_sound (p : Policy) : Prop :=
  forall o, p o = Proven -> can_be_bad o = false.

(* A sound static-only policy must reject every op class that could
   misbehave on some input -- even when the actual dynamic instance is
   fine. *)
Theorem static_only_must_reject :
  forall (p : Policy) (o : OpClass),
    static_only p -> static_sound p ->
    can_be_bad o = true ->
    p o = Rejected.
Proof.
  intros p o Honly Hsound Hbad.
  destruct (Honly o) as [Hp | Hp]; [ | exact Hp].
  exfalso. rewrite (Hsound o Hp) in Hbad. discriminate.
Qed.

(* The guarded policy runs the same (good-instance) program to completion,
   and even the bad-instance run fails CLOSED, never silently. *)
Theorem guarded_more_permissive_at_equal_safety :
  forall o : OpClass,
    can_be_bad o = true ->
    pgy_policy o = Guarded ->
    (forall p, static_only p -> static_sound p ->
       ~ well_formed p [(o, false)]) /\
    run pgy_policy [(o, false)] = OK /\
    run pgy_policy [(o, true)]  = Panic.
Proof.
  intros o Hbad Hg. repeat split.
  - intros p Honly Hsound Hwf.
    apply (Hwf o false); [left; reflexivity | ].
    apply static_only_must_reject; assumption.
  - simpl. rewrite Hg. reflexivity.
  - simpl. rewrite Hg. reflexivity.
Qed.
