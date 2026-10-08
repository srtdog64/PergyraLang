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
    (6) unique_resolution_ordered:
        with declaration order modeled (a binding is visible after its
        position; a scope spans a source interval), a weaker rule is
        enough: a binding may not reuse a name that an earlier binding in
        its own or an enclosing scope holds. The front ends enforce this
        rule. It still refuses both PP shapes, and it admits a name reused
        after the block that bound it has closed, which the order-free
        rule of (2) refuses (closed_block_reuse_admitted,
        closed_block_reuse_refused_order_free). The order-free rule
        implies it (order_free_rule_is_stronger).

  Honest scope: scopes form a forest given by a parent function. Sections
  (1)-(5) do not model declaration order; section (6) models it with a
  source position per binding and a source interval per scope.
*)

Require Import Stdlib.Lists.List.
Require Import Stdlib.Arith.PeanoNat.
Require Import Stdlib.micromega.Lia.
Require Import Stdlib.Bool.Bool.
Import ListNotations.

(* encloses p a b: scope a is b or an ancestor of b. *)
Inductive encloses (parent : nat -> option nat) : nat -> nat -> Prop :=
| enc_refl : forall a, encloses parent a a
| enc_step : forall a b p,
    parent b = Some p -> encloses parent a p -> encloses parent a b.

Lemma encloses_trans : forall parent a b c,
  encloses parent a b -> encloses parent b c -> encloses parent a c.
Proof.
  intros parent a b c Hab Hbc. induction Hbc.
  - exact Hab.
  - eapply enc_step; eauto.
Qed.

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

(* Finite operational projection of the same ancestry owner, never a table
   of the example's known scopes. Fuel bounds this projection only: full
   resolution requires a sufficient finite scope-chain bound from its owner. *)
Fixpoint encloses_b (fuel : nat) (parent : nat -> option nat) (a b : nat) : bool :=
  if Nat.eqb a b then true else
  match fuel with
  | 0 => false
  | S remaining =>
      match parent b with
      | None => false
      | Some p => encloses_b remaining parent a p
      end
  end.

Lemma encloses_b_sound : forall fuel parent a b,
  encloses_b fuel parent a b = true -> encloses parent a b.
Proof.
  induction fuel as [| fuel IH]; intros parent a b H;
    simpl in H; destruct (Nat.eqb a b) eqn:E.
  - apply Nat.eqb_eq in E. subst. constructor.
  - discriminate.
  - apply Nat.eqb_eq in E. subst. constructor.
  - destruct (parent b) as [p|] eqn:Hp; [| discriminate].
    eapply enc_step; [exact Hp |]. eapply IH; exact H.
Qed.

(* One sufficient, inspectable admission profile: parent ids decrease, so
   the current scope id bounds the whole walk. This is a model profile, not
   an unverified claim about front-end scope numbering. *)
Definition parent_descends (parent : nat -> option nat) : Prop :=
  forall child p, parent child = Some p -> p < child.

Lemma encloses_b_complete_bounded : forall parent a b,
  parent_descends parent -> encloses parent a b ->
  forall fuel, b <= fuel -> encloses_b fuel parent a b = true.
Proof.
  intros parent a b Hdesc Henc. induction Henc.
  - intros fuel Hbound. destruct fuel; simpl; rewrite Nat.eqb_refl; reflexivity.
  - intros fuel Hbound. pose proof (Hdesc b p H) as Hsmaller.
    destruct fuel as [| fuel]; [lia |]. simpl.
    destruct (Nat.eqb a b); [reflexivity |]. rewrite H.
    apply IHHenc. lia.
Qed.

Definition visible_ids (parent : nat -> option nat) (fuel : nat)
  (ds : list Decl) (name t : nat) : list nat :=
  map d_id (filter (fun d => andb (Nat.eqb (d_name d) name)
                                  (encloses_b fuel parent (d_scope d) t)) ds).

Theorem visible_ids_sound : forall parent fuel ds name t id,
  In id (visible_ids parent fuel ds name t) ->
  exists d, In d ds /\ d_id d = id /\ d_name d = name /\ visible parent d t.
Proof.
  intros parent fuel ds name t id Hin. unfold visible_ids in Hin.
  apply in_map_iff in Hin. destruct Hin as [d [Hid Hin]].
  apply filter_In in Hin. destruct Hin as [Hin Hb].
  apply andb_true_iff in Hb. destruct Hb as [Hname Hscope].
  apply Nat.eqb_eq in Hname. exists d. repeat split; try assumption.
  apply encloses_b_sound in Hscope. exact Hscope.
Qed.

Theorem visible_ids_complete_bounded : forall parent fuel ds name t d,
  parent_descends parent -> t <= fuel ->
  In d ds -> d_name d = name -> visible parent d t ->
  In (d_id d) (visible_ids parent fuel ds name t).
Proof.
  intros parent fuel ds name t d Hdesc Hbound Hin Hname Hvisible.
  unfold visible_ids. apply in_map. apply filter_In. split; [exact Hin |].
  apply andb_true_iff. split.
  - apply Nat.eqb_eq. exact Hname.
  - eapply encloses_b_complete_bounded; eauto.
Qed.

Theorem name_is_not_identity :
  visible_ids tree 3 single_tag 4 3 = [200] /\
  visible_ids tree 3 double_tag 4 3 = [200; 201].
Proof. split; reflexivity. Qed.

(* ---- (6) declaration order: the rule the front ends enforce ------- *)

Record ODecl := { o_name : nat; o_id : nat; o_scope : nat; o_pos : nat }.

(* Scope s spans the source positions [lo s, hi s). *)
Definition inside (lo hi : nat -> nat) (s q : nat) : Prop :=
  lo s <= q /\ q < hi s.

(* The scope tree and the source agree: an enclosed scope's span lies
   within the span of the scope that encloses it. *)
Definition spans_nest (parent : nat -> option nat) (lo hi : nat -> nat)
    : Prop :=
  forall a b, encloses parent a b -> lo a <= lo b /\ hi b <= hi a.

(* A declaration sits in its own scope, outside every scope nested in it. *)
Definition placed (parent : nat -> option nat) (lo hi : nat -> nat)
    (d : ODecl) : Prop :=
  inside lo hi (o_scope d) (o_pos d) /\
  forall s, encloses parent (o_scope d) s -> s <> o_scope d ->
    ~ inside lo hi s (o_pos d).

(* One declaration per source position. *)
Definition positions_distinct (ds : list ODecl) : Prop :=
  forall d1 d2, In d1 ds -> In d2 ds -> o_pos d1 = o_pos d2 ->
    o_id d1 = o_id d2.

(* A binding is visible at position q of scope t once declared, in every
   scope its own encloses. *)
Definition ovisible (parent : nat -> option nat) (d : ODecl) (t q : nat)
    : Prop :=
  encloses parent (o_scope d) t /\ o_pos d < q.

(* The rule: a binding may not reuse a name that an earlier binding in its
   own scope or an enclosing scope holds. *)
Definition no_rebinding_ordered (parent : nat -> option nat)
    (ds : list ODecl) : Prop :=
  forall d1 d2, In d1 ds -> In d2 ds ->
    o_name d1 = o_name d2 -> o_id d1 <> o_id d2 ->
    encloses parent (o_scope d1) (o_scope d2) -> o_pos d2 <= o_pos d1.

Theorem unique_resolution_ordered :
  forall parent lo hi ds d1 d2 t q,
    spans_nest parent lo hi ->
    (forall d, In d ds -> placed parent lo hi d) ->
    positions_distinct ds ->
    no_rebinding_ordered parent ds ->
    In d1 ds -> In d2 ds -> o_name d1 = o_name d2 ->
    inside lo hi t q ->
    ovisible parent d1 t q -> ovisible parent d2 t q ->
    o_id d1 = o_id d2.
Proof.
  intros parent lo hi ds d1 d2 t q Hnest Hplaced Hdist Hrule H1 H2 Hname
    Ht V1 V2.
  unfold ovisible in V1, V2.
  destruct V1 as [E1 P1]. destruct V2 as [E2 P2].
  unfold inside in Ht. destruct Ht as [_ Hq].
  destruct (Nat.eq_dec (o_id d1) (o_id d2)) as [Heq | Hne];
    [exact Heq | exfalso].
  (* The binding in the enclosing scope cannot be visible at q. *)
  assert (Key : forall a b, In a ds -> In b ds ->
      o_name a = o_name b -> o_id a <> o_id b ->
      encloses parent (o_scope b) t -> o_pos a < q ->
      encloses parent (o_scope a) (o_scope b) -> False).
  { intros a b Ha Hb Hn Hid Eb Pa Eab.
    pose proof (Hrule a b Ha Hb Hn Hid Eab) as Hle.
    destruct (Nat.eq_dec (o_scope a) (o_scope b)) as [Hs | Hs].
    - (* Same scope: the rule also runs the other way. *)
      assert (Eba : encloses parent (o_scope b) (o_scope a))
        by (rewrite Hs; apply enc_refl).
      pose proof (Hrule b a Hb Ha (eq_sym Hn)
                    (fun e => Hid (eq_sym e)) Eba) as Hle'.
      apply Hid. apply Hdist; [exact Ha | exact Hb | lia].
    - (* a sits outside b's span, which has closed before q. *)
      destruct (Hplaced a Ha) as [_ Hout].
      destruct (Hplaced b Hb) as [Hin _].
      unfold inside in Hin. destruct Hin as [Lb _].
      destruct (Hnest (o_scope b) t Eb) as [_ Hhi].
      apply (Hout (o_scope b) Eab (fun e => Hs (eq_sym e))).
      unfold inside. split; lia. }
  destruct (ancestors_linear parent (o_scope d1) t E1 (o_scope d2) E2)
    as [E | E].
  - exact (Key d1 d2 H1 H2 Hname Hne E2 P1 E).
  - exact (Key d2 d1 H2 H1 (eq_sym Hname) (fun e => Hne (eq_sym e))
             E1 P2 E).
Qed.

(* Both PP shapes stay refused. PP-061: `tag` at 12 in scope 1, then an
   inner `tag` at 24 in scope 3. *)
Definition outer_tag_at : ODecl :=
  {| o_name := 4; o_id := 200; o_scope := 1; o_pos := 12 |}.
Definition inner_tag_at : ODecl :=
  {| o_name := 4; o_id := 201; o_scope := 3; o_pos := 24 |}.

Theorem shadow_refused_ordered :
  ~ no_rebinding_ordered tree [outer_tag_at; inner_tag_at].
Proof.
  intros Hrule.
  pose proof (Hrule outer_tag_at inner_tag_at (or_introl eq_refl)
                (or_intror (or_introl eq_refl)) eq_refl ltac:(discriminate)
                scope1_encloses_3) as H.
  simpl in H. lia.
Qed.

(* PP-062: two `started` in scope 1. *)
Definition started_first : ODecl :=
  {| o_name := 5; o_id := 300; o_scope := 1; o_pos := 14 |}.
Definition started_second : ODecl :=
  {| o_name := 5; o_id := 301; o_scope := 1; o_pos := 16 |}.

Theorem redeclare_refused_ordered :
  ~ no_rebinding_ordered tree [started_first; started_second].
Proof.
  intros Hrule.
  pose proof (Hrule started_first started_second (or_introl eq_refl)
                (or_intror (or_introl eq_refl)) eq_refl ltac:(discriminate)
                (enc_refl tree 1)) as H.
  simpl in H. lia.
Qed.

(* A name reused after the block that bound it has closed: `x` at 22 in
   scope 3, then `x` at 35 in scope 1. *)
Definition x_inner : ODecl :=
  {| o_name := 7; o_id := 400; o_scope := 3; o_pos := 22 |}.
Definition x_later : ODecl :=
  {| o_name := 7; o_id := 401; o_scope := 1; o_pos := 35 |}.

Lemma not_encloses_3_1 : ~ encloses tree 3 1.
Proof.
  intros E.
  inversion E as [| a b p Hp Hrest]; subst. simpl in Hp.
  injection Hp as Hp0. subst p.
  inversion Hrest as [| a' b' p' Hp' _]; subst. simpl in Hp'.
  discriminate Hp'.
Qed.

Theorem closed_block_reuse_admitted :
  no_rebinding_ordered tree [x_inner; x_later].
Proof.
  intros d1 d2 H1 H2 Hn Hid E.
  destruct H1 as [H1 | [H1 | []]]; destruct H2 as [H2 | [H2 | []]];
    subst d1 d2; simpl in *.
  - exfalso. apply Hid. reflexivity.
  - exfalso. exact (not_encloses_3_1 E).
  - lia.
  - exfalso. apply Hid. reflexivity.
Qed.

Definition unordered (d : ODecl) : Decl :=
  {| d_name := o_name d; d_id := o_id d; d_scope := o_scope d |}.

Theorem closed_block_reuse_refused_order_free :
  ~ no_rebinding tree [unordered x_inner; unordered x_later].
Proof.
  intros Hrule.
  destruct (Hrule (unordered x_later) (unordered x_inner)
              (or_intror (or_introl eq_refl)) (or_introl eq_refl) eq_refl
              ltac:(discriminate)) as [N _].
  apply N. exact scope1_encloses_3.
Qed.

Theorem order_free_rule_is_stronger :
  forall parent ds,
    no_rebinding parent (map unordered ds) -> no_rebinding_ordered parent ds.
Proof.
  intros parent ds Hrule d1 d2 H1 H2 Hn Hid E.
  exfalso.
  destruct (Hrule (unordered d1) (unordered d2)
              (in_map unordered ds d1 H1) (in_map unordered ds d2 H2)
              Hn Hid) as [N _].
  exact (N E).
Qed.
