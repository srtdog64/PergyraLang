(*
  Pergyra Formal Semantics - Mechanized Sketch
  Target: docs/42 Keyword Orthogonality -- Axis Fact-Ownership
  Status: proof-sketch; not beta-closure evidence unless checked by CI (coqc).
  Scope: this file mechanizes the *fact-ownership* discipline of docs/42:
    every semantic question (fact) is owned by exactly one top-level axis,
    and a state transition attributed to one axis cannot silently change a
    fact owned by another axis. It turns the docs/42 SS0/SS2 prose
    ("different semantic axes must not silently own the same question";
     "intent is not a universal owner") into checked theorems.

  Negative scope: this file does NOT model the surface syntax, the type
  checker, the full verifier-graph resolution algorithm, effect propagation,
  or that the *implementation* actually enforces the single-writer discipline
  (StepBy). It proves the design is internally consistent (exactly-one-owner)
  and that the ownership discipline *entails* no-silent-override -- given the
  model. Mapping the model onto the real verifier graph, and proving
  confluence of multi-axis resolution, are separate obligations (see SS6).
*)

Require Import Coq.Init.Logic.

(* ========================================== *)
(* 1. Axes (docs/42 SS0 -- four top-level axes) *)
(* ========================================== *)

Inductive Axis : Type :=
  | AxResource      (* slot / own / ref / pin / unsafe / extern        *)
  | AxExecution     (* parallel / spawn / async / await / select / channel *)
  | AxDomain        (* subject / intent / zone / world / authority / relation / effect *)
  | AxTypeContract. (* class / struct / ability / role / where         *)

(* ========================================== *)
(* 2. Facts (the semantic questions a program answers)                *)
(* The intent clauses of docs/42 SS2, plus one carrier per other axis. *)
(* ========================================== *)

Inductive Fact : Type :=
  | FWho            (* who acts                  -> participant / subject *)
  | FWhere          (* where / within            -> zone / world boundary *)
  | FRequires       (* required ability          -> ability / capability  *)
  | FAuthorizedBy   (* authorized by             -> authority boundary     *)
  | FCauses         (* causes                    -> effect                 *)
  | FResourceHeld   (* which handle, which boundary -> resource axis       *)
  | FExecutionPlan  (* when / where / concurrency   -> execution axis      *)
  | FShape.         (* which shape / contract       -> type/contract axis  *)

(* ========================================== *)
(* 3. The docs/42 ownership table, as an inductive relation.          *)
(* Modeling this as a RELATION (not a function) is deliberate: it lets *)
(* uniqueness and totality become theorems about the *hand-written*    *)
(* table, rather than facts true by construction.                      *)
(* ========================================== *)

Inductive Owns : Axis -> Fact -> Prop :=
  | OwnWho          : Owns AxDomain       FWho
  | OwnWhere        : Owns AxDomain       FWhere
  | OwnRequires     : Owns AxTypeContract FRequires
  | OwnAuthorizedBy : Owns AxDomain       FAuthorizedBy
  | OwnCauses       : Owns AxDomain       FCauses
  | OwnResource     : Owns AxResource     FResourceHeld
  | OwnExecution    : Owns AxExecution    FExecutionPlan
  | OwnShape        : Owns AxTypeContract FShape.

(* ========================================== *)
(* 4. Theorem 1 -- Exactly One Owner          *)
(* ========================================== *)

(* 1a. Ownership is FUNCTIONAL: no fact is owned by two axes.
       This is the formal statement of docs/42 orthogonality --
       "different semantic axes must not silently own the same question". *)
Theorem ownership_unique :
  forall f a1 a2, Owns a1 f -> Owns a2 f -> a1 = a2.
Proof.
  intros f a1 a2 H1 H2.
  destruct H1; inversion H2; reflexivity.
Qed.

(* 1b. Ownership is TOTAL: every fact has an owner.
       This is the formal statement of "no lost meaning" -- every semantic
       question the language can pose is owned by some axis (none orphaned). *)
Theorem ownership_total :
  forall f, exists a, Owns a f.
Proof.
  intros f; destruct f; eexists; constructor.
Qed.

(* 1c. Combined: every fact has EXACTLY ONE owner. *)
Theorem ownership_exists_unique :
  forall f, exists! a, Owns a f.
Proof.
  intros f.
  destruct (ownership_total f) as [a Ha].
  exists a; split.
  - exact Ha.
  - intros a' Ha'. apply (ownership_unique f); assumption.
Qed.

(* ========================================== *)
(* 5. Theorem 2 -- No Silent Override         *)
(* The formal analog of the "no hidden control flow" rule (CLAUDE.md SS1.1) *)
(* lifted to the axis level: a transition attributed to axis [a] may write  *)
(* only facts [a] owns; therefore it provably preserves every fact owned    *)
(* by a different axis.                                                      *)
(* ========================================== *)

Definition Value := nat.
Definition FactState := Fact -> Value.

(* A transition attributed to axis [a]: it leaves untouched every fact
   it does not own. This is the single-writer discipline docs/42 SS2 imposes
   when it says intent "combines facts from other axes" but does not own them. *)
Definition StepBy (a : Axis) (st st' : FactState) : Prop :=
  forall f, ~ Owns a f -> st' f = st f.

(* 2. A step by axis [a] cannot silently change a fact owned by a
      *different* axis [a']. The change is observable only through [a']. *)
Theorem no_silent_override :
  forall a st st' f a',
    StepBy a st st' -> Owns a' f -> a <> a' -> st' f = st f.
Proof.
  intros a st st' f a' Hstep Howns Hneq.
  apply Hstep.
  intro Hown_a.
  apply Hneq.
  apply (ownership_unique f); assumption.
Qed.

(* 2b. Preservation COMPOSES across a trace: if no step in a two-step trace
       is attributed to [a'], then [a']'s facts survive the whole trace.
       (Generalizes to n steps by the same argument; the 2-step case is the
       induction step.) *)
Theorem no_silent_override_2step :
  forall a1 a2 st st1 st2 f a',
    StepBy a1 st st1 -> StepBy a2 st1 st2 ->
    Owns a' f -> a1 <> a' -> a2 <> a' ->
    st2 f = st f.
Proof.
  intros a1 a2 st st1 st2 f a' H1 H2 Howns Hn1 Hn2.
  assert (E2 : st2 f = st1 f) by (apply (no_silent_override a2 st1 st2 f a'); assumption).
  assert (E1 : st1 f = st f)  by (apply (no_silent_override a1 st  st1 f a'); assumption).
  rewrite E2; exact E1.
Qed.

(* ========================================== *)
(* 6. Next obligations (future sessions)      *)
(* ------------------------------------------ *)
(* - Confluence: independent steps by distinct axes touch disjoint fact      *)
(*   sets, so multi-axis verifier resolution is order-independent. Requires  *)
(*   modeling each axis update as a deterministic function of the facts it    *)
(*   reads, then proving the two interleavings agree.                         *)
(* - Adequacy: connect [Owns] to the actual keyword -> axis table in the      *)
(*   compiler, and [StepBy] to the verifier graph's write attribution, so     *)
(*   these theorems constrain the implementation and not only the model.      *)
(* - Effect propagation (FCauses) and projection (object/tobject) as a        *)
(*   read-only derived view, proving views never own a fact.                  *)
(* ========================================== *)
