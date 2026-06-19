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
(* 7. Theorem 3 -- Confluence of independent axis updates                    *)
(* Independent steps by distinct axes touch disjoint fact sets (ownership is  *)
(* functional), so multi-axis verifier resolution is order-independent.       *)
(* ========================================== *)

(* Decidable equality on axes (a 4-constructor enumeration). *)
Lemma Axis_eq_dec : forall a b : Axis, {a = b} + {a <> b}.
Proof. decide equality. Qed.

(* The unique owner of a fact (existence packaged with the uniqueness clause). *)
Lemma owner_of :
  forall f, exists o, Owns o f /\ (forall b, Owns b f -> b = o).
Proof.
  intros f.
  destruct (ownership_total f) as [o Ho].
  exists o; split.
  - exact Ho.
  - intros b Hb. apply (ownership_unique f); assumption.
Qed.

(* If o is the unique owner of f and o <> x, then x does not own f. *)
Lemma not_owns_of_neq :
  forall f o x, (forall b, Owns b f -> b = o) -> o <> x -> ~ Owns x f.
Proof.
  intros f o x Huniq Hneq Hx. apply Hneq. symmetry. apply Huniq. exact Hx.
Qed.

(* A deterministic axis update: axis [a] overwrites every fact it owns with the
   value its resolution [wr] decided, and leaves every other fact untouched.
   [wr] models the verifier's resolved values for [a]'s facts; this refines
   StepBy with the value actually written. *)
Definition AxisUpdate (a : Axis) (wr : Fact -> Value) (st st' : FactState) : Prop :=
  forall f, (Owns a f -> st' f = wr f) /\ (~ Owns a f -> st' f = st f).

(* An AxisUpdate is in particular a StepBy step of the same axis (the model is a
   refinement of the SS5 single-writer discipline, not a different one). *)
Lemma axis_update_is_step :
  forall a wr st st', AxisUpdate a wr st st' -> StepBy a st st'.
Proof.
  intros a wr st st' H f Hnown. destruct (H f) as [_ Hpres]. apply Hpres. exact Hnown.
Qed.

(* Theorem 3: updates by DISTINCT axes commute. Resolving axis a then b yields
   the same FactState as b then a, fact-by-fact. Each fact has a single owner,
   so at most one of the two axes writes it and the other only preserves it. *)
Theorem axis_updates_commute :
  forall a b wra wrb st sab sba s_a s_b,
    a <> b ->
    AxisUpdate a wra st  s_a -> AxisUpdate b wrb s_a sab ->
    AxisUpdate b wrb st  s_b -> AxisUpdate a wra s_b sba ->
    forall f, sab f = sba f.
Proof.
  intros a b wra wrb st sab sba s_a s_b Hab Ha1 Hb2 Hb1 Ha2 f.
  destruct (owner_of f) as [o [Ho Huniq]].
  destruct (Ha1 f) as [Ha1w Ha1p].
  destruct (Hb2 f) as [Hb2w Hb2p].
  destruct (Hb1 f) as [Hb1w Hb1p].
  destruct (Ha2 f) as [Ha2w Ha2p].
  destruct (Axis_eq_dec o a) as [Eoa | Noa].
  - (* a owns f; b only preserves it *)
    subst o.
    assert (Hnb : ~ Owns b f) by (exact (not_owns_of_neq f a b Huniq Hab)).
    rewrite (Hb2p Hnb). rewrite (Ha1w Ho). rewrite (Ha2w Ho). reflexivity.
  - destruct (Axis_eq_dec o b) as [Eob | Nob].
    + (* b owns f; a only preserves it *)
      subst o.
      assert (Hna : ~ Owns a f) by (exact (not_owns_of_neq f b a Huniq Noa)).
      rewrite (Hb2w Ho). rewrite (Ha2p Hna). rewrite (Hb1w Ho). reflexivity.
    + (* a third axis owns f; both a and b only preserve it *)
      assert (Hna : ~ Owns a f) by (exact (not_owns_of_neq f o a Huniq Noa)).
      assert (Hnb : ~ Owns b f) by (exact (not_owns_of_neq f o b Huniq Nob)).
      rewrite (Hb2p Hnb). rewrite (Ha1p Hna).
      rewrite (Ha2p Hna). rewrite (Hb1p Hnb). reflexivity.
Qed.

(* ========================================== *)
(* 8. Adequacy (surface) -- the docs/42 SS1 keyword table is consistent       *)
(* with the SS2 fact-ownership table: a keyword can never sit on an axis that  *)
(* does not own the fact it introduces.                                       *)
(* ========================================== *)

(* A representative slice of the docs/42 SS1 keyword families. *)
Inductive Keyword : Type :=
  | KwSubject | KwIntentWho | KwZone | KwAuthority | KwEffect
  | KwAbility | KwSlot | KwParallel.

(* docs/42 SS1: the axis each keyword is classified under. *)
Definition keyword_axis (k : Keyword) : Axis :=
  match k with
  | KwSubject   => AxDomain
  | KwIntentWho => AxDomain
  | KwZone      => AxDomain
  | KwAuthority => AxDomain
  | KwEffect    => AxDomain
  | KwAbility   => AxTypeContract
  | KwSlot      => AxResource
  | KwParallel  => AxExecution
  end.

(* The fact each keyword introduces. *)
Definition keyword_fact (k : Keyword) : Fact :=
  match k with
  | KwSubject   => FWho
  | KwIntentWho => FWho
  | KwZone      => FWhere
  | KwAuthority => FAuthorizedBy
  | KwEffect    => FCauses
  | KwAbility   => FRequires
  | KwSlot      => FResourceHeld
  | KwParallel  => FExecutionPlan
  end.

(* Adequacy: the axis a keyword is classified under actually owns the fact that
   keyword introduces. The surface (SS1) and ownership (SS2) tables cannot
   silently drift apart -- if they did, this would fail to type-check. *)
Theorem keyword_axis_sound :
  forall k, Owns (keyword_axis k) (keyword_fact k).
Proof.
  intros k; destruct k; simpl; constructor.
Qed.

(* ========================================== *)
(* 9. Remaining obligations (future sessions) *)
(* ------------------------------------------ *)
(* - Reading confluence: Theorem 3 covers updates whose written values are     *)
(*   fixed per resolution (wr). The harder case is resolution that READS other *)
(*   axes' facts; show the read-set is disjoint from the other axis' write-set *)
(*   and recover commutation.                                                  *)
(* - Binary adequacy: SS8 proves surface<->ownership consistency INSIDE Coq.   *)
(*   The model<->actual-compiler link cannot be a Coq theorem. The keyword     *)
(*   layer of it is now a differential test --                                 *)
(*   tests/axis_keyword_adequacy_smoke.sh pins keyword_axis (here) against the *)
(*   docs/42 SS0 table and the compiler's recognized keywords (lexer reserved  *)
(*   + parser contextual), so drift in any layer fails the gate. STILL OPEN:   *)
(*   binding StepBy to the verifier graph's actual write attribution.          *)
(* - Effect (FCauses) and projection (object/tobject) as read-only derived     *)
(*   views, proving a view never owns a fact.                                  *)
(* ========================================== *)
