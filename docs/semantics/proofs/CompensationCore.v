(*
  Pergyra Formal Semantics -- Mechanized Fragment (compensation corner)
  Target: docs/semantics/19 "Pergyra Abstract Machine Obligation" (intent facet)
  Status: proof-sketch; not beta-closure evidence unless checked by CI (coqc).

  Scope: the compensation / rollback Step form -- the intent-specific facet that
  docs/19 flags as the hard COUPLING: compensation is sound only when it names
  both the effect to undo AND the typestate snapshot to restore. Saga lineage
  (Garcia-Molina & Salem; Bruni-Melgratti-Montanari).

  Model: a forward step `Forward e` records the full pre-forward store in the
  effect log and marks every slot in `comp_target e` as Filled. A `Rollback`
  step undoes the most recently logged effect by restoring every targeted slot
  from that logged pre-forward store and popping the log. `comp_target` is the
  explicit effect->list slot coupling.

  Mechanized obligations:
    - Fail-closed: a rollback requires a non-empty effect log -- you cannot undo
      what was never done (`rollback_requires_log`).
    - Snapshot-carrying compensation: rollback restores every target slot to
      the state recorded before the forward effect (`rollback_restores_snapshot`).
    - Compensation removes exactly the compensated effect from the log
      (`rollback_pops_log`).
    - Saga round-trip: a forward step followed by rollback restores each target
      slot to its pre-forward value (`do_then_rollback_restores`).

  Negative scope: this file still models LIFO compensation only. Binding
  `comp_target`, graphs, and holdings to live AIR/MIR owner facts remains the
  implementation adequacy task; the model alone must not be cited as closure.
*)

Require Import Coq.Lists.List.
Require Import Coq.Arith.PeanoNat.
Import ListNotations.

Section CompensationCore.

Definition slot := nat.
Definition eff  := nat.

Inductive lcstate := Empty | Filled | Released.

Definition store := slot -> lcstate.

Definition slot_in (s : slot) (ss : list slot) : bool :=
  existsb (fun x => Nat.eqb s x) ss.

Definition fill_targets (s0 : store) (targets : list slot) : store :=
  fun x => if slot_in x targets then Filled else s0 x.

Definition restore_targets
  (current : store) (before : store) (targets : list slot) : store :=
  fun x => if slot_in x targets then before x else current x.

Definition all_targets_empty (s0 : store) (targets : list slot) : Prop :=
  Forall (fun s => s0 s = Empty) targets.

Lemma slot_in_true : forall s targets,
  In s targets -> slot_in s targets = true.
Proof.
  intros s targets Hin. unfold slot_in.
  induction targets as [| x xs IH]; simpl in *.
  - contradiction.
  - destruct Hin as [Heq | Hin].
    + subst x. rewrite Nat.eqb_refl. reflexivity.
    + destruct (Nat.eqb s x) eqn:E.
      * reflexivity.
      * apply IH. exact Hin.
Qed.

(* The effect->slots coupling: which slots each effect touched. *)
Definition comp_target := eff -> list slot.

Record effect_log_entry := mkLog {
  logged_eff : eff;
  before_store : store
}.

Record config := mkConfig {
  st   : store;
  elog : list effect_log_entry
}.

Inductive caction := Forward (e : eff) | Rollback.

Inductive cstep (ct : comp_target) : caction -> config -> config -> Prop :=
| CDo : forall c e,
    all_targets_empty (st c) (ct e) ->
    cstep ct (Forward e) c
      (mkConfig
        (fill_targets (st c) (ct e))
        (mkLog e (st c) :: elog c))
| CComp : forall c e before rest,
    elog c = mkLog e before :: rest ->
    cstep ct Rollback c
      (mkConfig
        (restore_targets (st c) before (ct e))
        rest).

(* ---- fail-closed: cannot roll back an empty log ---- *)

Theorem rollback_requires_log : forall ct c c',
  cstep ct Rollback c c' -> elog c <> [].
Proof.
  intros ct c c' Hstep Hempty.
  inversion Hstep; subst.
  rewrite H in Hempty. discriminate.
Qed.

(* ---- compensation soundness: rollback restores logged pre-forward state ---- *)

Theorem rollback_restores_snapshot : forall ct c c' e before rest s,
  cstep ct Rollback c c' ->
  elog c = mkLog e before :: rest ->
  In s (ct e) ->
  st c' s = before s.
Proof.
  intros ct c c' e before rest s Hstep Helog Hin.
  inversion Hstep; subst.
  rewrite Helog in H.
  inversion H; subst.
  simpl. unfold restore_targets.
  rewrite slot_in_true by exact Hin.
  reflexivity.
Qed.

(* ---- compensation removes exactly the compensated effect ---- *)

Theorem rollback_pops_log : forall ct c c' e before rest,
  cstep ct Rollback c c' ->
  elog c = mkLog e before :: rest ->
  elog c' = rest.
Proof.
  intros ct c c' e before rest Hstep Helog.
  inversion Hstep; subst.
  rewrite Helog in H.
  inversion H; subst.
  simpl. reflexivity.
Qed.

Theorem forward_logs_before_state : forall ct c c' e,
  cstep ct (Forward e) c c' ->
  elog c' = mkLog e (st c) :: elog c.
Proof.
  intros ct c c' e Hstep.
  inversion Hstep; subst. simpl. reflexivity.
Qed.

(* ---- a Forward step then rollback restores each touched slot to pre-state ---- *)
(* The saga round-trip: do then compensate is identity on every target slot. *)

Theorem do_then_rollback_restores : forall ct c c1 c2 e s,
  cstep ct (Forward e) c c1 ->
  cstep ct Rollback c1 c2 ->
  In s (ct e) ->
  st c2 s = st c s.
Proof.
  intros ct c c1 c2 e s Hfwd Hroll Hin.
  apply (rollback_restores_snapshot ct c1 c2 e (st c) (elog c) s Hroll).
  - apply (forward_logs_before_state ct c c1 e Hfwd).
  - exact Hin.
Qed.

End CompensationCore.
