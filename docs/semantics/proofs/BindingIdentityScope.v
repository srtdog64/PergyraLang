(*
  Pergyra Formal Semantics - Mechanized Sketch
  Target: harness PP-061 / PP-062 -- one rule for local names.
  Status: proof-sketch; not beta-closure evidence unless checked by CI.

  PP-061: an inner `let tag` under an outer `tag` crashed the default
  route's phi join, which keyed locals by name. PP-062: a second
  `let started` in the same scope was accepted by the default route and
  refused by native. This file shows:

    (1) ancestors_linear:
        the scopes enclosing a point form a chain (a scope has one
        parent), the fact everything below rests on.
    (2) unique_resolution:
        under the rule "no two bindings of one name in scopes where one
        encloses the other (the same scope included)", a name resolves to
        at most one binding identity at every point.
    (3) sibling_reuse_resolves:
        bindings of one name in sibling scopes never meet, so the rule
        keeps `for i` in two loops legal.
    (4) shadow_is_ambiguous / redeclare_is_ambiguous:
        both PP shapes put two identities under one name at one point;
        the single rule refuses both.
    (5) name_is_not_identity:
        two programs with the same names at the same point can hold one
        binding or two, so a table keyed by name (the phi join) cannot
        tell them apart. Lowering must key locals by binding identity
        even though the rule rejects the ambiguous source.

  Honest scope: scopes form a forest given by a parent function;
  declaration order inside a scope is not modeled (the rule does not
  depend on it).
*)

Require Import Coq.Lists.List.
Require Import Coq.Arith.PeanoNat.
Import ListNotations.

(* encloses p a b: scope a is b or an ancestor of b. *)
Inductive encloses (parent : nat -> option nat) : nat -> nat -> Prop :=
| enc_refl : forall a, encloses parent a a
| enc_step : forall a b p,
    parent b = Some p -> encloses parent a p -> encloses parent a b.

Record Decl := { d_name : nat; d_id : nat; d_scope : nat }.

Definition visible (parent : nat -> option nat) (d : Decl) (at_scope : nat)
    : Prop :=
  encloses parent (d_scope d) at_scope.

(* The one rule: same name, different identity => neither scope encloses
   the other. *)
Definition no_rebinding (parent : nat -> option nat) (ds : list Decl) : Prop :=
  forall d1 d2, In d1 ds -> In d2 ds ->
    d_name d1 = d_name d2 -> d_id d1 <> d_id d2 ->
    ~ encloses parent (d_scope d1) (d_scope d2) /\
    ~ encloses parent (d_scope d2) (d_scope d1).

(* ---- (1) enclosing scopes form a chain -------------------------- *)

Lemma ancestors_linear :
  forall parent a t, encloses parent a t ->
  forall b, encloses parent b t ->
    encloses parent a b \/ encloses parent b a.
Proof.
  intros parent a t Ha.
  induction Ha as [x | x y p Hpar Hxp IH].
  - intros b Hb. right. exact Hb.
  - intros b Hb.
    inversion Hb as [| b' y' p' Hpar' Hbp]; subst.
    + left. apply enc_step with p; assumption.
    + rewrite Hpar in Hpar'. injection Hpar' as Heq. subst p'.
      apply IH. exact Hbp.
Qed.

(* ---- (2) the rule makes resolution unique ----------------------- *)

Theorem unique_resolution :
  forall parent ds d1 d2 t,
    no_rebinding parent ds ->
    In d1 ds -> In d2 ds ->
    d_name d1 = d_name d2 ->
    visible parent d1 t -> visible parent d2 t ->
    d_id d1 = d_id d2.
Proof.
  intros parent ds d1 d2 t Hrule H1 H2 Hname V1 V2.
  destruct (Nat.eq_dec (d_id d1) (d_id d2)) as [Heq | Hne].
  - exact Heq.
  - exfalso.
    destruct (Hrule d1 d2 H1 H2 Hname Hne) as [N12 N21].
    unfold visible in V1, V2.
    destruct (ancestors_linear parent (d_scope d1) t V1 (d_scope d2) V2)
      as [E | E].
    + apply N12. exact E.
    + apply N21. exact E.
Qed.

(* ---- a small program shape: scope 1 and 2 are siblings under 0,
        scope 3 is inside 1 ------------------------------------------ *)

Definition tree (s : nat) : option nat :=
  match s with 1 => Some 0 | 2 => Some 0 | 3 => Some 1 | _ => None end.

Lemma sibling_scopes_apart : forall t,
  encloses tree 1 t -> encloses tree 2 t -> False.
Proof.
  intros t H1 H2.
  destruct (ancestors_linear tree 1 t H1 2 H2) as [E | E].
  - inversion E as [| a b p Hp Hrest]; subst. simpl in Hp.
    injection Hp as Hp0. subst p.
    inversion Hrest as [| a' b' p' Hp' _]; subst. simpl in Hp'. discriminate Hp'.
  - inversion E as [| a b p Hp Hrest]; subst. simpl in Hp.
    injection Hp as Hp0. subst p.
    inversion Hrest as [| a' b' p' Hp' _]; subst. simpl in Hp'. discriminate Hp'.
Qed.

(* ---- (3) sibling reuse is harmless ------------------------------ *)

Definition loop_i_first : Decl := {| d_name := 9; d_id := 100; d_scope := 1 |}.
Definition loop_i_second : Decl := {| d_name := 9; d_id := 101; d_scope := 2 |}.

Theorem sibling_reuse_resolves :
  forall t, ~ (visible tree loop_i_first t /\ visible tree loop_i_second t).
Proof.
  intros t [V1 V2]. unfold visible in V1, V2. simpl in V1, V2.
  exact (sibling_scopes_apart t V1 V2).
Qed.

(* ---- (4) both PP shapes are two identities under one name -------- *)

(* PP-061: outer `tag` in scope 1, inner `tag` in scope 3. *)
Definition outer_tag : Decl := {| d_name := 4; d_id := 200; d_scope := 1 |}.
Definition inner_tag : Decl := {| d_name := 4; d_id := 201; d_scope := 3 |}.

Lemma scope1_encloses_3 : encloses tree 1 3.
Proof. apply enc_step with 1; [reflexivity | apply enc_refl]. Qed.

Theorem shadow_is_ambiguous :
  visible tree outer_tag 3 /\ visible tree inner_tag 3 /\
  d_id outer_tag <> d_id inner_tag /\
  ~ no_rebinding tree [outer_tag; inner_tag].
Proof.
  unfold visible. simpl.
  split; [exact scope1_encloses_3 |].
  split; [apply enc_refl |].
  split.
  - discriminate.
  - intros Hrule.
    destruct (Hrule outer_tag inner_tag (or_introl eq_refl)
                (or_intror (or_introl eq_refl)) eq_refl
                ltac:(discriminate)) as [N _].
    apply N. exact scope1_encloses_3.
Qed.

(* PP-062: two `started` in the same scope 1. *)
Definition started_int : Decl := {| d_name := 5; d_id := 300; d_scope := 1 |}.
Definition started_text : Decl := {| d_name := 5; d_id := 301; d_scope := 1 |}.

Theorem redeclare_is_ambiguous :
  ~ no_rebinding tree [started_int; started_text].
Proof.
  intros Hrule.
  destruct (Hrule started_int started_text (or_introl eq_refl)
              (or_intror (or_introl eq_refl)) eq_refl
              ltac:(discriminate)) as [N _].
  apply N. apply enc_refl.
Qed.

(* ---- (5) a name does not determine the binding ------------------- *)

(* The names visible at scope 3 are the same in both programs; one has a
   single binding, the other two. Any lowering that maps a point and a
   name to one local cannot serve both. *)
Definition single_tag : list Decl := [outer_tag].
Definition double_tag : list Decl := [outer_tag; inner_tag].

Definition visible_ids (ds : list Decl) (name t : nat) : list nat :=
  map d_id (filter (fun d => andb (Nat.eqb (d_name d) name)
                                  (match d_scope d, t with
                                   | 1, 3 => true | 3, 3 => true | _, _ => false
                                   end)) ds).

Theorem name_is_not_identity :
  visible_ids single_tag 4 3 = [200] /\ visible_ids double_tag 4 3 = [200; 201].
Proof. split; reflexivity. Qed.
