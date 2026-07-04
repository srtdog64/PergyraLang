(*
  Pergyra Formal Semantics -- Generic axis-carriage law
  Target: docs/151 §0 (Generic Semantic Composition Matrix, core rule) and
          its formal shape: "carriage is monotone upward; the only sanctioned
          descent is an ERASE point that names what it drops."
  Status: machine-verified (coqc, 0 admits / 0 axioms). All close with Qed.

  The adopted surface law under proof:

      Generic<T> cannot hide T's semantic axes. To hide, you declare;
      the compiler proves.

  Model: a generic composition is a spine of constructor layers applied to a
  leaf type T. Each layer either PRESERVES carriage (contributing its own
  axis marks -- the ALLOW/GATE/DEFER verdicts of docs/151 §3, which differ in
  evidence timing but never in carriage) or ERASES (contributing its own
  marks and dropping a DECLARED set -- the ERASE verdict; the declared set is
  what docs/151 §3 requires the cell to name, bucketed per docs/14).

  Theorems:

    1. carriage_monotone        -- on an erase-free spine, every leaf axis
                                   survives to the root: wrappers cannot
                                   lose axes by accident.
    2. descent_is_declared      -- any leaf axis missing at the root is
                                   named by some ERASE layer's declared set:
                                   NO SILENT DESCENT (laundering impossible).
    3. erase_declared_scope     -- an axis no ERASE layer declares always
                                   survives: ERASE drops exactly what it
                                   names, nothing more.
    4. carriage_no_conjuring    -- every root axis comes from the leaf or
                                   from a layer's own marks: provenance is
                                   total, axes do not appear from nowhere.
    5. hiding_requires_declaration -- combining (2): a hidden axis implies
                                   the spine holds a declaring ERASE layer,
                                   and the spine is provably not erase-free.

  Negative scope: this file proves the carriage LAW, not the carriage MODE.
  Decision-0 of docs/151 §2 (positional / value-typed / runtime-tag) chooses
  where carriage physically lives; the law here holds over whichever mode is
  chosen, which is exactly why it is provable while Decision-0 is still open.
  Nor does this model cross-axis edges (docs/151 §4) or prove that the C and
  LLVM emitters implement the law -- that is the future matrix-lock gate's
  job, in the AIRBinding.v negative-scope tradition.
*)

Require Import Coq.Lists.List.
Require Import Coq.Arith.PeanoNat.
Import ListNotations.

(* Axis marks. Concrete nat for decidable equality; every statement is
   generic in the mark values (World/Zone/Actor/Auth/IntentEff/Site are
   just distinct numbers to this file). *)
Definition axis := nat.

(* One constructor layer of a generic composition.
   LPreserve own          : constructor adds its own marks, keeps T's.
   LErase    own declared : constructor adds its own marks and drops the
                            DECLARED set from T's carriage. *)
Inductive layer : Type :=
  | LPreserve : list axis -> layer
  | LErase : list axis -> list axis -> layer.

(* Innermost-first spine: Slot<View<T>> is [view-layer; slot-layer]. *)
Definition spine := list layer.

Fixpoint remove_all (drops : list axis) (c : list axis) : list axis :=
  match drops with
  | [] => c
  | d :: rest => remove_all rest (remove Nat.eq_dec d c)
  end.

Definition apply_layer (c : list axis) (l : layer) : list axis :=
  match l with
  | LPreserve own => own ++ c
  | LErase own declared => own ++ remove_all declared c
  end.

Fixpoint carriage (leaf : list axis) (s : spine) : list axis :=
  match s with
  | [] => leaf
  | l :: rest => carriage (apply_layer leaf l) rest
  end.

Fixpoint erase_free (s : spine) : Prop :=
  match s with
  | [] => True
  | LPreserve _ :: rest => erase_free rest
  | LErase _ _ :: rest => False
  end.

(* The union of every ERASE layer's declared set: the spine's stated
   erasure budget -- docs/151 §3's "ERASE names its bucket" ledger. *)
Fixpoint declared_drops (s : spine) : list axis :=
  match s with
  | [] => []
  | LPreserve _ :: rest => declared_drops rest
  | LErase _ declared :: rest => declared ++ declared_drops rest
  end.

Fixpoint own_marks (s : spine) : list axis :=
  match s with
  | [] => []
  | LPreserve own :: rest => own ++ own_marks rest
  | LErase own _ :: rest => own ++ own_marks rest
  end.

(* ---- helper lemmas about remove/remove_all ---- *)

Lemma in_remove_other : forall (d a : axis) (c : list axis),
  a <> d -> In a c -> In a (remove Nat.eq_dec d c).
Proof.
  intros d a c Hneq.
  induction c as [|x xs IH]; simpl; intros Hin.
  - exact Hin.
  - destruct (Nat.eq_dec d x) as [Hdx|Hdx].
    + subst x. apply IH.
      destruct Hin as [Hx|Hx].
      * exfalso. apply Hneq. symmetry. exact Hx.
      * exact Hx.
    + destruct Hin as [Hx|Hx].
      * left. exact Hx.
      * right. apply IH. exact Hx.
Qed.

Lemma in_remove_weaken : forall (d a : axis) (c : list axis),
  In a (remove Nat.eq_dec d c) -> In a c.
Proof.
  intros d a c.
  induction c as [|x xs IH]; simpl.
  - intro H. exact H.
  - destruct (Nat.eq_dec d x) as [Hdx|Hdx]; intro H.
    + right. apply IH. exact H.
    + destruct H as [Hx|Hx].
      * left. exact Hx.
      * right. apply IH. exact Hx.
Qed.

Lemma remove_all_keeps : forall (drops : list axis) (a : axis) (c : list axis),
  ~ In a drops -> In a c -> In a (remove_all drops c).
Proof.
  induction drops as [|d rest IH]; intros a c Hnin Hin; simpl.
  - exact Hin.
  - apply IH.
    + intro Hr. apply Hnin. right. exact Hr.
    + apply in_remove_other.
      * intro Heq. apply Hnin. left. symmetry. exact Heq.
      * exact Hin.
Qed.

Lemma remove_all_weaken : forall (drops : list axis) (c : list axis) (a : axis),
  In a (remove_all drops c) -> In a c.
Proof.
  induction drops as [|d rest IH]; intros c a Hin; simpl in Hin.
  - exact Hin.
  - apply in_remove_weaken with (d := d). apply IH. exact Hin.
Qed.

Lemma erase_free_no_declared : forall s : spine,
  erase_free s -> declared_drops s = [].
Proof.
  induction s as [|l rest IH]; intros H.
  - reflexivity.
  - destruct l as [own|own declared]; simpl in *.
    + apply IH. exact H.
    + destruct H.
Qed.

(* ---- the carriage law ---- *)

(* 3. ERASE drops exactly what it names: an axis outside every declared
   set survives the whole spine. This is the strongest positive form --
   the other survival statements are corollaries. *)
Theorem erase_declared_scope : forall (s : spine) (leaf : list axis) (a : axis),
  In a leaf -> ~ In a (declared_drops s) -> In a (carriage leaf s).
Proof.
  induction s as [|l rest IH]; intros leaf a Hin Hnd; simpl.
  - exact Hin.
  - destruct l as [own|own declared]; simpl in *.
    + apply IH.
      * apply in_or_app. right. exact Hin.
      * exact Hnd.
    + apply IH.
      * apply in_or_app. right.
        apply remove_all_keeps.
        -- intro Hd. apply Hnd. apply in_or_app. left. exact Hd.
        -- exact Hin.
      * intro Hr. apply Hnd. apply in_or_app. right. exact Hr.
Qed.

(* 1. Monotonicity: with no ERASE layer, wrapping cannot lose an axis.
   This is docs/151 §0's "carriage is monotone upward". *)
Theorem carriage_monotone : forall (s : spine) (leaf : list axis) (a : axis),
  erase_free s -> In a leaf -> In a (carriage leaf s).
Proof.
  intros s leaf a Hef Hin.
  apply erase_declared_scope.
  - exact Hin.
  - rewrite (erase_free_no_declared s Hef). intro Hf. exact Hf.
Qed.

(* 2. No silent descent: a leaf axis missing at the root was DECLARED by
   some ERASE layer. Laundering an axis through wrappers is impossible;
   the only way down is a named erase point. *)
Theorem descent_is_declared : forall (s : spine) (leaf : list axis) (a : axis),
  In a leaf -> ~ In a (carriage leaf s) -> In a (declared_drops s).
Proof.
  intros s leaf a Hin Hmiss.
  destruct (in_dec Nat.eq_dec a (declared_drops s)) as [Hd|Hd].
  - exact Hd.
  - exfalso. apply Hmiss. apply erase_declared_scope; assumption.
Qed.

(* 4. Provenance is total: every axis at the root is traceable to the leaf
   or to a constructor's own marks. Axes cannot be conjured -- the upward
   direction of the same honesty the descent theorems give downward. *)
Theorem carriage_no_conjuring : forall (s : spine) (leaf : list axis) (a : axis),
  In a (carriage leaf s) -> In a leaf \/ In a (own_marks s).
Proof.
  induction s as [|l rest IH]; intros leaf a Hin; simpl in *.
  - left. exact Hin.
  - destruct l as [own|own declared]; simpl in *.
    + apply IH in Hin. destruct Hin as [Hin|Hin].
      * apply in_app_or in Hin. destruct Hin as [Hin|Hin].
        -- right. apply in_or_app. left. exact Hin.
        -- left. exact Hin.
      * right. apply in_or_app. right. exact Hin.
    + apply IH in Hin. destruct Hin as [Hin|Hin].
      * apply in_app_or in Hin. destruct Hin as [Hin|Hin].
        -- right. apply in_or_app. left. exact Hin.
        -- left. apply remove_all_weaken with (drops := declared). exact Hin.
      * right. apply in_or_app. right. exact Hin.
Qed.

(* 5. The docs/151 §0 sentence, as one statement: hiding an axis forces a
   declaring ERASE layer into the spine -- and certifies the spine is not
   erase-free. "To hide, you declare." *)
Corollary hiding_requires_declaration : forall (s : spine) (leaf : list axis) (a : axis),
  In a leaf -> ~ In a (carriage leaf s) ->
  In a (declared_drops s) /\ ~ erase_free s.
Proof.
  intros s leaf a Hin Hmiss.
  assert (Hd : In a (declared_drops s)).
  { apply descent_is_declared with (leaf := leaf); assumption. }
  split.
  - exact Hd.
  - intro Hef. rewrite (erase_free_no_declared s Hef) in Hd. exact Hd.
Qed.
