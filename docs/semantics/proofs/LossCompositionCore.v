(*
  Pergyra Formal Semantics -- Loss Composition Core

  Status: proof-sketch; not whole-language verification.
  Budget: 0 admits / 0 axioms.

  A local loss budget is not a global path budget. Lowering loss composes, and
  a compiler-derived mechanism is admissible only when it preserves the named
  observation class and stays within the declared observable-cost budget.
*)

Require Import Coq.Arith.PeanoNat.
Require Import Coq.micromega.Lia.

Section LossCompositionCore.

Record Mechanism := mkMechanism {
  mechanism_observation : nat;
  mechanism_cost : nat
}.

Inductive DerivedMechanism
  (source chosen : Mechanism) (cost_budget : nat) : Prop :=
  | derive_mechanism :
      mechanism_observation source = mechanism_observation chosen ->
      mechanism_cost chosen <= cost_budget ->
      DerivedMechanism source chosen cost_budget.

Theorem derived_mechanism_requires_observational_equivalence :
  forall source chosen budget,
    DerivedMechanism source chosen budget ->
    mechanism_observation source = mechanism_observation chosen.
Proof.
  intros source chosen budget Hderived.
  inversion Hderived; assumption.
Qed.

Theorem derived_mechanism_requires_cost_budget :
  forall source chosen budget,
    DerivedMechanism source chosen budget ->
    mechanism_cost chosen <= budget.
Proof.
  intros source chosen budget Hderived.
  inversion Hderived; assumption.
Qed.

Record LossVector := mkLossVector {
  semantic_loss : nat;
  provenance_loss : nat;
  timing_loss : nat;
  runtime_debt : nat
}.

Definition loss_zero : LossVector := mkLossVector 0 0 0 0.

Definition loss_compose (left right : LossVector) : LossVector :=
  mkLossVector
    (semantic_loss left + semantic_loss right)
    (provenance_loss left + provenance_loss right)
    (timing_loss left + timing_loss right)
    (runtime_debt left + runtime_debt right).

Definition loss_within (actual budget : LossVector) : Prop :=
  semantic_loss actual <= semantic_loss budget /\
  provenance_loss actual <= provenance_loss budget /\
  timing_loss actual <= timing_loss budget /\
  runtime_debt actual <= runtime_debt budget.

Definition PathBudgetAllows
  (first second budget : LossVector) : Prop :=
  loss_within (loss_compose first second) budget.

Theorem loss_compose_zero_left :
  forall loss, loss_compose loss_zero loss = loss.
Proof.
  intros [semantic provenance timing runtime].
  reflexivity.
Qed.

Theorem loss_compose_zero_right :
  forall loss, loss_compose loss loss_zero = loss.
Proof.
  intros [semantic provenance timing runtime].
  unfold loss_compose, loss_zero.
  simpl.
  f_equal; lia.
Qed.

Theorem loss_compose_associative :
  forall first second third,
    loss_compose (loss_compose first second) third =
    loss_compose first (loss_compose second third).
Proof.
  intros [fs fp ft fr] [ss sp st sr] [ts tp tt tr].
  unfold loss_compose.
  simpl.
  f_equal; lia.
Qed.

Theorem composed_budget_implies_each_component_bounded :
  forall first second budget,
    loss_within (loss_compose first second) budget ->
    loss_within first budget /\ loss_within second budget.
Proof.
  intros [fs fp ft fr] [ss sp st sr] [bs bp bt br] Hwithin.
  unfold loss_within, loss_compose in *.
  simpl in *.
  repeat split; lia.
Qed.

Definition one_semantic_loss : LossVector := mkLossVector 1 0 0 0.

Theorem local_budgets_do_not_imply_path_budget :
  loss_within one_semantic_loss one_semantic_loss /\
  loss_within one_semantic_loss one_semantic_loss /\
  ~ PathBudgetAllows one_semantic_loss one_semantic_loss one_semantic_loss.
Proof.
  unfold PathBudgetAllows, loss_within, loss_compose, one_semantic_loss.
  simpl.
  lia.
Qed.

Theorem path_budget_checks_composed_loss :
  forall first second budget,
    PathBudgetAllows first second budget ->
    loss_within (loss_compose first second) budget.
Proof.
  intros first second budget Hpath.
  exact Hpath.
Qed.

End LossCompositionCore.
