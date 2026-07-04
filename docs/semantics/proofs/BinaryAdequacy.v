(*
  Pergyra Formal Semantics -- Binary Adequacy of the Surface Verdict
  (WO-F1, second half)
  Target: docs/42 SS6 remaining obligation (b) + docs/semantics/20
          (fail-closed position): the surface's two-valued accept/reject
          decision coincides with the calculus guard -- no third state.
  Status: machine-verified (coqc, 0 admits / 0 axioms). All theorems close
          with Qed.

  AIRBinding.v proved the machine's guard is EXACTLY the guard computed
  from the AIR fact record (guard_air_faithful). That guard is a Prop.
  A surface, however, must DECIDE: a program point either compiles
  (accept) or gets a diagnostic (reject), and it must always answer.
  This file closes that gap:

    1. accept_adequate  -- a computable boolean verdict [accept] decides
                           EXACTLY the calculus guard:
                           accept = true <-> guard holds.
    2. reject_adequate  -- rejection is exactly guard failure:
                           accept = false <-> guard fails. Together with
                           (1): no silent third state, by construction.
    3. guard_decidable / verdict_is_binary
                        -- the guard is decidable, so a surface CAN always
                           answer accept-or-reject (never Unknown). This is
                           the formal license for the two-valued surface:
                           binary judgment loses nothing against the
                           calculus, because the calculus itself is
                           decidable at each action.

  The config/guard here is the AIRBinding.v guard verbatim (standalone
  re-declaration, one-file-per-proof convention); AIRBinding.v ties that
  guard to the whole-program machine, so adequacy composes:
  surface accept <-> guard_air <-> machine guard.

  Negative scope: this does NOT prove the C type-checker/emitter implements
  [accept] (that correspondence is the parity + smoke gate family), nor
  that diagnostics carry the right payload. It proves the two-valued
  surface CONTRACT is faithful to the calculus, i.e. binary adequacy of
  the judgment itself.
*)

Require Import Coq.Lists.List.
Require Import Coq.Arith.PeanoNat.
Require Import Coq.Bool.Bool.
Import ListNotations.

Section BinaryAdequacy.

Definition principal := nat.
Definition zone := nat.
Definition cap  := nat.
Definition eff  := nat.
Definition slot := nat.
Definition task := nat.

Inductive lcstate := Empty | Filled | Released.
Definition slot_store := slot -> lcstate.

Record effect_log_entry := mkLog { logged_eff : eff; before_store : slot_store }.

Record config := mkConfig {
  actor    : principal;
  holdings : principal -> list cap;
  here     : zone;
  elog     : list effect_log_entry;
  store    : slot_store;
  done     : list task
}.

Definition has_cap (c : config) (k : cap) : Prop := In k (holdings c (actor c)).

Inductive action :=
  | ActCross (z' : zone)
  | ActEmit (e : eff)
  | ActAcquire (s : slot)
  | ActUse (s : slot)
  | ActRelease (s : slot)
  | ActDelegate (b : principal) (k : cap)
  | ActRollback
  | ActRun (t : task).

Record AIRFacts := mkAIR {
  air_zone_gate    : zone -> cap;
  air_effect_gate  : eff  -> cap;
  air_acquire_gate : slot -> cap;
  air_comp_targets : eff  -> list slot;
  air_dep_graph    : task -> list task
}.

Definition ready_air (F : AIRFacts) (c : config) (t : task) : Prop :=
  forall x, In x (air_dep_graph F t) -> In x (done c).

(* The calculus guard -- verbatim AIRBinding.v / WholeProgramCore.v. *)
Definition guard_air (F : AIRFacts) (act : action) (c : config) : Prop :=
  match act with
  | ActCross z'     => has_cap c (air_zone_gate F z')
  | ActEmit e       => has_cap c (air_effect_gate F e)
  | ActAcquire s    => has_cap c (air_acquire_gate F s) /\ store c s = Empty
  | ActUse s        => store c s = Filled
  | ActRelease s    => store c s = Filled
  | ActDelegate _ k => has_cap c k
  | ActRollback     => exists e before rest,
                         elog c = mkLog e before :: rest /\
                         Forall (fun s => has_cap c (air_acquire_gate F s))
                                (air_comp_targets F e)
  | ActRun t        => ready_air F c t
  end.

(* ================================================================ *)
(* The computable surface verdict.                                  *)
(* ================================================================ *)

Definition capb (c : config) (k : cap) : bool :=
  existsb (Nat.eqb k) (holdings c (actor c)).

Definition lcstate_eqb (a b : lcstate) : bool :=
  match a, b with
  | Empty, Empty | Filled, Filled | Released, Released => true
  | _, _ => false
  end.

Definition accept (F : AIRFacts) (act : action) (c : config) : bool :=
  match act with
  | ActCross z'     => capb c (air_zone_gate F z')
  | ActEmit e       => capb c (air_effect_gate F e)
  | ActAcquire s    => capb c (air_acquire_gate F s)
                       && lcstate_eqb (store c s) Empty
  | ActUse s        => lcstate_eqb (store c s) Filled
  | ActRelease s    => lcstate_eqb (store c s) Filled
  | ActDelegate _ k => capb c k
  | ActRollback     => match elog c with
                       | [] => false
                       | mkLog e _ :: _ =>
                           forallb (fun s => capb c (air_acquire_gate F s))
                                   (air_comp_targets F e)
                       end
  | ActRun t        => forallb (fun x => existsb (Nat.eqb x) (done c))
                               (air_dep_graph F t)
  end.

(* ================================================================ *)
(* Correctness of the boolean pieces.                               *)
(* ================================================================ *)

Lemma existsb_in_nat : forall (l : list nat) (x : nat),
  existsb (Nat.eqb x) l = true <-> In x l.
Proof.
  intros l x; split.
  - intro H. apply existsb_exists in H. destruct H as [y [Hin Heq]].
    apply Nat.eqb_eq in Heq. subst; exact Hin.
  - intro H. apply existsb_exists. exists x.
    split; [exact H | apply Nat.eqb_refl].
Qed.

Lemma capb_iff : forall c k, capb c k = true <-> has_cap c k.
Proof. intros c k. unfold capb, has_cap. apply existsb_in_nat. Qed.

Lemma lcstate_eqb_iff : forall a b, lcstate_eqb a b = true <-> a = b.
Proof. intros a b; destruct a, b; simpl; split; intros; congruence. Qed.

(* ================================================================ *)
(* 1. Binary adequacy: accept decides exactly the guard.            *)
(* ================================================================ *)

Theorem accept_adequate : forall F act c,
  accept F act c = true <-> guard_air F act c.
Proof.
  intros F act c. destruct act; simpl.
  - apply capb_iff.
  - apply capb_iff.
  - (* Acquire *)
    rewrite andb_true_iff. split.
    + intros [Hc Hs].
      split; [apply capb_iff; exact Hc | apply lcstate_eqb_iff; exact Hs].
    + intros [Hc Hs].
      split; [apply capb_iff; exact Hc | apply lcstate_eqb_iff; exact Hs].
  - apply lcstate_eqb_iff.
  - apply lcstate_eqb_iff.
  - apply capb_iff.
  - (* Rollback *)
    destruct (elog c) as [| [e b] rest] eqn:Elog.
    + split.
      * intro H; discriminate H.
      * intros (e' & b' & r' & Hlog & _). discriminate Hlog.
    + split.
      * intro H. exists e, b, rest. split; [reflexivity |].
        apply Forall_forall. intros s Hs.
        rewrite forallb_forall in H.
        apply capb_iff. apply H. exact Hs.
      * intros (e' & b' & r' & Hlog & Hall).
        injection Hlog as He Hb Hr. subst.
        apply forallb_forall. intros s Hs. apply capb_iff.
        eapply Forall_forall in Hall; [exact Hall | exact Hs].
  - (* Run *)
    unfold ready_air. split.
    + intro H. rewrite forallb_forall in H. intros x Hx.
      apply existsb_in_nat. apply H. exact Hx.
    + intro H. apply forallb_forall. intros x Hx.
      apply existsb_in_nat. apply H. exact Hx.
Qed.

(* ================================================================ *)
(* 2. Rejection is exactly guard failure -- no silent third state.  *)
(* ================================================================ *)

Corollary reject_adequate : forall F act c,
  accept F act c = false <-> ~ guard_air F act c.
Proof.
  intros F act c. split.
  - intros Hf Hg. apply accept_adequate in Hg. rewrite Hg in Hf.
    discriminate Hf.
  - intro Hn. destruct (accept F act c) eqn:E; [ | reflexivity ].
    exfalso. apply Hn. apply accept_adequate. exact E.
Qed.

(* ================================================================ *)
(* 3. Decidability: a surface can ALWAYS answer accept-or-reject.   *)
(* ================================================================ *)

Corollary guard_decidable : forall F act c,
  {guard_air F act c} + {~ guard_air F act c}.
Proof.
  intros F act c. destruct (accept F act c) eqn:E.
  - left. apply accept_adequate. exact E.
  - right. apply reject_adequate. exact E.
Qed.

Corollary verdict_is_binary : forall F act c,
  guard_air F act c \/ ~ guard_air F act c.
Proof.
  intros F act c. destruct (guard_decidable F act c); [left | right]; assumption.
Qed.

End BinaryAdequacy.
