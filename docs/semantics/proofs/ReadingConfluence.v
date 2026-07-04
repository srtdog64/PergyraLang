(*
  Pergyra Formal Semantics -- Reading Confluence (WO-F1, first half)
  Target: docs/42 Keyword Orthogonality -- SS6 remaining obligation (a):
          "different axis reading orders converge to the same judgment".
  Status: machine-verified (coqc, 0 admits / 0 axioms). All theorems close
          with Qed.

  AxisOwnership.v proves the WRITE side of the discipline: updates by
  distinct axes commute (axis_updates_commute). This file proves the READ
  side: a *reading* of a program is the order in which a reader (human,
  verifier pass, tool) visits the axes to assemble the judgment. Because
  fact-ownership is functional (exactly one axis owns each fact), the
  assembled judgment is independent of the visiting order:

    1. reading_reads_owner    -- a reading that visits the owner of a fact
                                 reads exactly the state's value for it,
                                 wherever the owner sits in the order.
    2. read_order_irrelevant  -- ANY two complete readings (each axis
                                 visited at least once, in any order, with
                                 any duplication) assemble THE SAME
                                 judgment. This subsumes permutations:
                                 a permutation of a complete reading is a
                                 complete reading.
    3. incomplete_readings_can_disagree
                              -- the completeness hypothesis is load-bearing:
                                 dropping an axis admits readings that
                                 disagree. (Witness, not a scare quote.)

  The file is standalone (one-file-per-proof convention): it re-declares the
  minimal axis/fact/ownership config of AxisOwnership.v verbatim.

  Negative scope: this models the ownership TABLE's consequences for reading
  order; it does not model the C verifier's actual pass order, nor claim the
  implementation visits axes atomically. Mapping onto the real verifier
  passes stays with the parity/smoke gates.
*)

Require Import Coq.Lists.List.
Import ListNotations.

Section ReadingConfluence.

(* ================================================================ *)
(* Minimal config re-declaration (verbatim slice of AxisOwnership.v) *)
(* ================================================================ *)

Inductive Axis : Type :=
  | AxResource
  | AxExecution
  | AxDomain
  | AxTypeContract.

Inductive Fact : Type :=
  | FWho
  | FWhere
  | FRequires
  | FAuthorizedBy
  | FCauses
  | FResourceHeld
  | FExecutionPlan
  | FShape.

Inductive Owns : Axis -> Fact -> Prop :=
  | OwnWho          : Owns AxDomain       FWho
  | OwnWhere        : Owns AxDomain       FWhere
  | OwnRequires     : Owns AxTypeContract FRequires
  | OwnAuthorizedBy : Owns AxDomain       FAuthorizedBy
  | OwnCauses       : Owns AxDomain       FCauses
  | OwnResource     : Owns AxResource     FResourceHeld
  | OwnExecution    : Owns AxExecution    FExecutionPlan
  | OwnShape        : Owns AxTypeContract FShape.

Definition Value := nat.
Definition FactState := Fact -> Value.

(* The unique owner, as a function (the docs/42 SS2 table read column-wise). *)
Definition owner (f : Fact) : Axis :=
  match f with
  | FWho           => AxDomain
  | FWhere         => AxDomain
  | FRequires      => AxTypeContract
  | FAuthorizedBy  => AxDomain
  | FCauses        => AxDomain
  | FResourceHeld  => AxResource
  | FExecutionPlan => AxExecution
  | FShape         => AxTypeContract
  end.

(* The function agrees with the relation (adequacy of the reformulation). *)
Lemma owner_owns : forall f, Owns (owner f) f.
Proof. intro f; destruct f; simpl; constructor. Qed.

Lemma owns_owner : forall a f, Owns a f -> a = owner f.
Proof. intros a f H; destruct H; reflexivity. Qed.

(* Decidable ownership test, needed to *run* a reading. *)
Definition owns_b (a : Axis) (f : Fact) : bool :=
  match a, f with
  | AxDomain,       FWho           => true
  | AxDomain,       FWhere         => true
  | AxTypeContract, FRequires      => true
  | AxDomain,       FAuthorizedBy  => true
  | AxDomain,       FCauses        => true
  | AxResource,     FResourceHeld  => true
  | AxExecution,    FExecutionPlan => true
  | AxTypeContract, FShape         => true
  | _, _ => false
  end.

Lemma owns_b_true_iff : forall a f, owns_b a f = true <-> Owns a f.
Proof.
  intros a f; split.
  - destruct a, f; simpl; intro H; try discriminate; constructor.
  - intro H; destruct H; reflexivity.
Qed.

Lemma owns_b_owner : forall f, owns_b (owner f) f = true.
Proof. intro f; destruct f; reflexivity. Qed.

Lemma owns_b_false_of_neq : forall a f, a <> owner f -> owns_b a f = false.
Proof.
  intros a f Hneq.
  destruct (owns_b a f) eqn:E; [ | reflexivity ].
  exfalso. apply Hneq. apply owns_owner.
  apply owns_b_true_iff. exact E.
Qed.

(* ================================================================ *)
(* Readings: visiting the axes in a list order, assembling a partial *)
(* judgment. An axis contributes exactly the facts it owns.          *)
(* ================================================================ *)

(* The judgment assembled by a reading: the first visited axis that owns
   fact [f] supplies the state's value; axes that do not own [f] are
   silent on it. [None] = the reading never answered the question. *)
Fixpoint read_acc (order : list Axis) (st : FactState) (f : Fact)
  : option Value :=
  match order with
  | [] => None
  | a :: rest => if owns_b a f then Some (st f) else read_acc rest st f
  end.

(* A COMPLETE reading visits every axis at least once (any order, any
   duplication). A permutation of a complete reading is complete, so
   order-irrelevance over complete readings subsumes permutation
   order-irrelevance. *)
Definition complete (order : list Axis) : Prop :=
  forall a : Axis, In a order.

(* ================================================================ *)
(* 1. A reading that visits the owner reads the state's value.       *)
(* ================================================================ *)

Theorem reading_reads_owner :
  forall order st f,
    In (owner f) order ->
    read_acc order st f = Some (st f).
Proof.
  intros order st f.
  induction order as [| a rest IH]; intro Hin.
  - inversion Hin.
  - simpl. destruct (owns_b a f) eqn:E.
    + reflexivity.
    + apply IH. destruct Hin as [Heq | Hin'].
      * exfalso. subst a. rewrite owns_b_owner in E. discriminate.
      * exact Hin'.
Qed.

(* Silence is exactly "the owner was never visited". *)
Theorem reading_silent_iff_owner_missing :
  forall order st f,
    read_acc order st f = None <-> ~ In (owner f) order.
Proof.
  intros order st f. split.
  - intros Hnone Hin.
    rewrite (reading_reads_owner order st f Hin) in Hnone. discriminate.
  - intro Hnotin.
    induction order as [| a rest IH].
    + reflexivity.
    + simpl. destruct (owns_b a f) eqn:E.
      * exfalso. apply Hnotin. left.
        apply owns_owner. apply owns_b_true_iff. exact E.
      * apply IH. intro Hin. apply Hnotin. right. exact Hin.
Qed.

(* ================================================================ *)
(* 2. read_order_irrelevant -- confluence of complete readings.      *)
(* ================================================================ *)

Theorem read_order_irrelevant :
  forall o1 o2 st,
    complete o1 -> complete o2 ->
    forall f, read_acc o1 st f = read_acc o2 st f.
Proof.
  intros o1 o2 st H1 H2 f.
  rewrite (reading_reads_owner o1 st f (H1 (owner f))).
  rewrite (reading_reads_owner o2 st f (H2 (owner f))).
  reflexivity.
Qed.

(* The judgment a complete reading assembles is total: every semantic
   question is answered (the reading-side face of ownership totality). *)
Corollary complete_reading_total :
  forall order st f,
    complete order ->
    read_acc order st f = Some (st f).
Proof.
  intros order st f Hc. apply reading_reads_owner. apply Hc.
Qed.

(* ================================================================ *)
(* 3. Completeness is load-bearing: incomplete readings can disagree. *)
(* ================================================================ *)

Theorem incomplete_readings_can_disagree :
  exists (o1 o2 : list Axis) (st : FactState) (f : Fact),
    read_acc o1 st f <> read_acc o2 st f.
Proof.
  exists [AxDomain], [], (fun _ => 0), FWho.
  simpl. discriminate.
Qed.

End ReadingConfluence.
