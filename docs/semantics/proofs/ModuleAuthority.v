(*
  Pergyra module authority boundary model.

  Scope: the semantic core of the planned `use MODULE;` surface (docs/202),
  proven before any compiler code exists. A link is an ordered list of
  modules whose imports may only reach earlier positions, so import cycles
  are impossible by construction; a name resolves only when exactly one
  module in the link exports it; a module may re-export only authority it
  owns or received from its imports, so linking can never amplify
  authority; and every authority visible on any re-export surface of a
  well-formed link is rooted in some owning module.

  Load surface: the chain and fanout generators build links of any size n;
  well-formedness and resolution are proven for all n, and executable
  witnesses at n = 16 evaluate the boolean checkers by computation.
  Rejection witnesses show a cycle, an authority-from-nowhere, and an
  ambiguous export each fail the same checkers.

  It does not prove: the parser or linker implementation, the
  file-to-module mapping, runtime behavior, or link-time performance.
  Performance is owned by the implementation gate battery planned in
  docs/202.
*)

Require Import Coq.Lists.List.
Require Import Coq.Arith.PeanoNat.
Require Import Coq.Arith.Wf_nat.
Require Import Coq.Bool.Bool.
Require Import Coq.micromega.Lia.
Import ListNotations.

Definition Name := nat.
Definition Authority := nat.

Record PgyModule : Type := mkModule {
  m_imports : list nat;
  m_exports : list Name;
  m_owns : list Authority;
  m_reexports : list Authority
}.

Definition Link := list PgyModule.

(* ---------------------------------------------------------------- *)
(* Stratification: imports reach only strictly earlier positions.   *)
(* ---------------------------------------------------------------- *)

Fixpoint stratified_from (i : nat) (l : Link) : bool :=
  match l with
  | [] => true
  | m :: rest =>
      forallb (fun j => j <? i) (m_imports m) && stratified_from (S i) rest
  end.

Definition link_stratified (l : Link) : bool := stratified_from 0 l.

Definition imports_edge (l : Link) (i j : nat) : bool :=
  match nth_error l i with
  | Some m => existsb (fun k => Nat.eqb k j) (m_imports m)
  | None => false
  end.

Lemma stratified_from_nth :
  forall l b k m,
    stratified_from b l = true ->
    nth_error l k = Some m ->
    forallb (fun j => j <? b + k) (m_imports m) = true.
Proof.
  induction l as [| m0 rest IH]; intros b k m Hs Hn.
  - destruct k; discriminate Hn.
  - simpl in Hs. apply andb_true_iff in Hs. destruct Hs as [Hh Ht].
    destruct k as [| k'].
    + simpl in Hn. inversion Hn; subst.
      rewrite Nat.add_0_r. exact Hh.
    + simpl in Hn.
      specialize (IH (S b) k' m Ht Hn).
      replace (b + S k') with (S b + k') by lia.
      exact IH.
Qed.

Theorem stratified_edge_lt :
  forall l i j,
    link_stratified l = true ->
    imports_edge l i j = true ->
    j < i.
Proof.
  intros l i j Hs He.
  unfold imports_edge in He.
  destruct (nth_error l i) as [m |] eqn:Hn; [| discriminate He].
  apply existsb_exists in He.
  destruct He as [k [Hin Hkj]].
  apply Nat.eqb_eq in Hkj; subst k.
  pose proof (stratified_from_nth l 0 i m Hs Hn) as Hall.
  rewrite forallb_forall in Hall.
  specialize (Hall j Hin).
  apply Nat.ltb_lt in Hall.
  lia.
Qed.

Inductive import_path (l : Link) : nat -> nat -> Prop :=
  | path_step : forall i j,
      imports_edge l i j = true -> import_path l i j
  | path_trans : forall i j k,
      imports_edge l i j = true -> import_path l j k -> import_path l i k.

Theorem import_path_descends :
  forall l i j,
    link_stratified l = true ->
    import_path l i j ->
    j < i.
Proof.
  intros l i j Hs Hp.
  induction Hp.
  - eapply stratified_edge_lt; eauto.
  - pose proof (stratified_edge_lt l i j Hs H). lia.
Qed.

Theorem import_acyclic :
  forall l i,
    link_stratified l = true ->
    ~ import_path l i i.
Proof.
  intros l i Hs Hp.
  pose proof (import_path_descends l i i Hs Hp). lia.
Qed.

(* ---------------------------------------------------------------- *)
(* Resolution: a name resolves iff exactly one module exports it.   *)
(* ---------------------------------------------------------------- *)

Definition module_exports (m : PgyModule) (n : Name) : bool :=
  existsb (fun x => Nat.eqb x n) (m_exports m).

Definition exports_at (l : Link) (i : nat) (n : Name) : bool :=
  match nth_error l i with
  | Some m => module_exports m n
  | None => false
  end.

Definition exporters (l : Link) (n : Name) : list nat :=
  filter (fun i => exports_at l i n) (seq 0 (length l)).

Definition resolve (l : Link) (n : Name) : option nat :=
  match exporters l n with
  | [i] => Some i
  | _ => None
  end.

Lemma exporters_sound :
  forall l n i,
    In i (exporters l n) ->
    exports_at l i n = true.
Proof.
  intros l n i Hin.
  unfold exporters in Hin.
  apply filter_In in Hin.
  destruct Hin as [_ Hp]. exact Hp.
Qed.

Lemma exports_at_bound :
  forall l i n,
    exports_at l i n = true ->
    i < length l.
Proof.
  intros l i n H.
  unfold exports_at in H.
  destruct (nth_error l i) as [m |] eqn:Hn; [| discriminate H].
  apply nth_error_Some. rewrite Hn. discriminate.
Qed.

Lemma exporters_complete :
  forall l n i,
    exports_at l i n = true ->
    In i (exporters l n).
Proof.
  intros l n i H.
  unfold exporters.
  apply filter_In.
  split; [| exact H].
  apply in_seq.
  pose proof (exports_at_bound l i n H). lia.
Qed.

Theorem resolve_sound :
  forall l n i,
    resolve l n = Some i ->
    exports_at l i n = true.
Proof.
  intros l n i H.
  unfold resolve in H.
  destruct (exporters l n) as [| a rest] eqn:He; [discriminate H |].
  destruct rest; [| discriminate H].
  inversion H; subst.
  apply exporters_sound. rewrite He. left. reflexivity.
Qed.

Lemma singleton_of_unique :
  forall (xs : list nat) (i : nat),
    NoDup xs ->
    In i xs ->
    (forall j, In j xs -> j = i) ->
    xs = [i].
Proof.
  intros xs i Hnd Hin Huniq.
  destruct xs as [| a rest].
  - destruct Hin.
  - assert (Ha : a = i) by (apply Huniq; left; reflexivity).
    subst a.
    destruct rest as [| b rest'].
    + reflexivity.
    + assert (Hb : b = i) by (apply Huniq; right; left; reflexivity).
      subst b.
      inversion Hnd; subst.
      exfalso. apply H1. left. reflexivity.
Qed.

Theorem resolve_finds :
  forall l n i,
    exports_at l i n = true ->
    (forall j, exports_at l j n = true -> j = i) ->
    resolve l n = Some i.
Proof.
  intros l n i Hi Huniq.
  unfold resolve.
  assert (Hxs : exporters l n = [i]).
  { apply singleton_of_unique.
    - apply NoDup_filter. apply seq_NoDup.
    - apply exporters_complete. exact Hi.
    - intros j Hj. apply Huniq. apply exporters_sound. exact Hj. }
  rewrite Hxs. reflexivity.
Qed.

(* Encapsulation: a name no module exports cannot resolve at all.   *)
Theorem hidden_name_unresolvable :
  forall l n,
    (forall i, exports_at l i n = false) ->
    resolve l n = None.
Proof.
  intros l n Hnone.
  unfold resolve.
  assert (He : exporters l n = []).
  { unfold exporters.
    induction (seq 0 (length l)) as [| a rest IH]; simpl.
    - reflexivity.
    - rewrite Hnone. exact IH. }
  rewrite He. reflexivity.
Qed.

(* Ambiguity: two distinct exporters leave the name unresolvable.   *)
Theorem ambiguous_name_unresolvable :
  forall l n i j,
    i <> j ->
    exports_at l i n = true ->
    exports_at l j n = true ->
    resolve l n = None.
Proof.
  intros l n i j Hne Hi Hj.
  unfold resolve.
  destruct (exporters l n) as [| a rest] eqn:He; [reflexivity |].
  destruct rest as [| b rest']; [| reflexivity].
  exfalso.
  assert (Hii : In i (exporters l n)) by (apply exporters_complete; exact Hi).
  assert (Hji : In j (exporters l n)) by (apply exporters_complete; exact Hj).
  rewrite He in Hii, Hji.
  destruct Hii as [Hii | []]; destruct Hji as [Hji | []].
  subst. apply Hne. reflexivity.
Qed.

(* Extension stability: appending a module that does not export n   *)
(* leaves the resolution of n untouched.                            *)

Lemma filter_ext_in :
  forall (f g : nat -> bool) (xs : list nat),
    (forall x, In x xs -> f x = g x) ->
    filter f xs = filter g xs.
Proof.
  intros f g xs H.
  induction xs as [| a rest IH]; simpl.
  - reflexivity.
  - rewrite (H a (or_introl eq_refl)).
    rewrite IH; [reflexivity |].
    intros x Hx. apply H. right. exact Hx.
Qed.

Lemma exports_at_app_left :
  forall l m i n,
    i < length l ->
    exports_at (l ++ [m]) i n = exports_at l i n.
Proof.
  intros l m i n Hi.
  unfold exports_at.
  rewrite nth_error_app1 by exact Hi.
  reflexivity.
Qed.

Theorem resolve_extension_stable :
  forall l m n,
    module_exports m n = false ->
    resolve (l ++ [m]) n = resolve l n.
Proof.
  intros l m n Hfresh.
  unfold resolve.
  assert (He : exporters (l ++ [m]) n = exporters l n).
  { unfold exporters.
    rewrite app_length. simpl.
    replace (length l + 1) with (S (length l)) by lia.
    rewrite seq_S. simpl.
    rewrite filter_app. simpl.
    assert (Hlast : exports_at (l ++ [m]) (0 + length l) n = false).
    { unfold exports_at. simpl.
      rewrite nth_error_app2 by lia.
      rewrite Nat.sub_diag. simpl. exact Hfresh. }
    simpl in Hlast. rewrite Hlast.
    rewrite app_nil_r.
    apply filter_ext_in.
    intros x Hx. apply in_seq in Hx.
    apply exports_at_app_left. lia. }
  rewrite He. reflexivity.
Qed.

(* ---------------------------------------------------------------- *)
(* Authority: re-export only what is owned or received; every       *)
(* visible authority is rooted in an owner.                         *)
(* ---------------------------------------------------------------- *)

Definition owns_auth (m : PgyModule) (a : Authority) : bool :=
  existsb (fun x => Nat.eqb x a) (m_owns m).

Definition reexports_auth (m : PgyModule) (a : Authority) : bool :=
  existsb (fun x => Nat.eqb x a) (m_reexports m).

Definition received (l : Link) (m : PgyModule) : list Authority :=
  flat_map (fun j =>
              match nth_error l j with
              | Some mj => m_reexports mj
              | None => []
              end)
           (m_imports m).

Definition module_authority_wf (l : Link) (m : PgyModule) : bool :=
  forallb (fun a =>
             owns_auth m a
             || existsb (fun x => Nat.eqb x a) (received l m))
          (m_reexports m).

Definition link_wf (l : Link) : bool :=
  link_stratified l && forallb (module_authority_wf l) l.

Definition rooted (l : Link) (a : Authority) : bool :=
  existsb (fun m => owns_auth m a) l.

Lemma link_wf_stratified :
  forall l, link_wf l = true -> link_stratified l = true.
Proof.
  intros l H. unfold link_wf in H.
  apply andb_true_iff in H. destruct H as [Hs _]. exact Hs.
Qed.

Lemma link_wf_module :
  forall l i m,
    link_wf l = true ->
    nth_error l i = Some m ->
    module_authority_wf l m = true.
Proof.
  intros l i m H Hn.
  unfold link_wf in H.
  apply andb_true_iff in H. destruct H as [_ Ha].
  rewrite forallb_forall in Ha.
  apply Ha. eapply nth_error_In. exact Hn.
Qed.

Lemma rooted_of_owner :
  forall l i m a,
    nth_error l i = Some m ->
    owns_auth m a = true ->
    rooted l a = true.
Proof.
  intros l i m a Hn Ho.
  unfold rooted.
  apply existsb_exists.
  exists m. split; [eapply nth_error_In; eauto | exact Ho].
Qed.

Theorem authority_rooted :
  forall l,
    link_wf l = true ->
    forall i, forall m a,
      nth_error l i = Some m ->
      reexports_auth m a = true ->
      rooted l a = true.
Proof.
  intros l Hwf i.
  induction i as [i IH] using lt_wf_ind.
  intros m a Hn Hre.
  pose proof (link_wf_module l i m Hwf Hn) as Hm.
  unfold module_authority_wf in Hm.
  rewrite forallb_forall in Hm.
  unfold reexports_auth in Hre.
  apply existsb_exists in Hre.
  destruct Hre as [x [Hxin Hxa]].
  apply Nat.eqb_eq in Hxa; subst x.
  specialize (Hm a Hxin).
  apply orb_true_iff in Hm.
  destruct Hm as [Ho | Hr].
  - eapply rooted_of_owner; eauto.
  - apply existsb_exists in Hr.
    destruct Hr as [y [Hyin Hya]].
    apply Nat.eqb_eq in Hya; subst y.
    unfold received in Hyin.
    apply in_flat_map in Hyin.
    destruct Hyin as [j [Hjimp Hjre]].
    destruct (nth_error l j) as [mj |] eqn:Hnj; [| destruct Hjre].
    assert (Hedge : imports_edge l i j = true).
    { unfold imports_edge. rewrite Hn.
      apply existsb_exists. exists j.
      split; [exact Hjimp | apply Nat.eqb_refl]. }
    assert (Hlt : j < i).
    { eapply stratified_edge_lt; eauto.
      apply link_wf_stratified. exact Hwf. }
    apply (IH j Hlt mj a Hnj).
    unfold reexports_auth.
    apply existsb_exists. exists a.
    split; [exact Hjre | apply Nat.eqb_refl].
Qed.

(* Contrapositive: an unrooted authority can appear on no re-export *)
(* surface anywhere in a well-formed link.                          *)
Theorem unrooted_invisible :
  forall l a,
    link_wf l = true ->
    rooted l a = false ->
    forall i m,
      nth_error l i = Some m ->
      reexports_auth m a = false.
Proof.
  intros l a Hwf Hun i m Hn.
  destruct (reexports_auth m a) eqn:Hre; [| reflexivity].
  pose proof (authority_rooted l Hwf i m a Hn Hre) as Hr.
  rewrite Hun in Hr. discriminate Hr.
Qed.

(* ---------------------------------------------------------------- *)
(* Load surface: links of any size.                                 *)
(* ---------------------------------------------------------------- *)

(* chain n: module 0 owns authority 0; module i imports i-1 and     *)
(* passes authority 0 along; every module exports its own name.     *)
Definition chain_module (i : nat) : PgyModule :=
  match i with
  | 0 => mkModule [] [0] [0] [0]
  | S k => mkModule [k] [S k] [] [0]
  end.

Definition chain (n : nat) : Link := map chain_module (seq 0 n).

(* fanout n: module 0 owns authority 0; modules 1..n each import   *)
(* module 0 directly.                                               *)
Definition fanout (n : nat) : Link :=
  mkModule [] [0] [0] [0]
    :: map (fun i => mkModule [0] [S i] [] [0]) (seq 0 n).

Lemma nth_error_seq_map :
  forall (f : nat -> PgyModule) b n k,
    k < n ->
    nth_error (map f (seq b n)) k = Some (f (b + k)).
Proof.
  intros f b n.
  revert b.
  induction n as [| n' IH]; intros b k Hk.
  - lia.
  - destruct k as [| k']; simpl.
    + rewrite Nat.add_0_r. reflexivity.
    + rewrite IH by lia.
      replace (S b + k') with (b + S k') by lia.
      reflexivity.
Qed.

Lemma chain_stratified_from :
  forall n b,
    stratified_from b (map chain_module (seq b n)) = true.
Proof.
  induction n as [| n' IH]; intros b; simpl.
  - reflexivity.
  - apply andb_true_iff. split.
    + destruct b as [| b']; simpl.
      * reflexivity.
      * rewrite andb_true_r. apply Nat.ltb_lt. lia.
    + apply IH.
Qed.

Theorem chain_wf : forall n, link_wf (chain n) = true.
Proof.
  intros n.
  unfold link_wf. apply andb_true_iff. split.
  - unfold link_stratified, chain. apply chain_stratified_from.
  - apply forallb_forall.
    intros m Hm.
    unfold chain in Hm.
    apply in_map_iff in Hm.
    destruct Hm as [i [Hf Hi]].
    apply in_seq in Hi. simpl in Hi.
    subst m.
    destruct i as [| k]; unfold module_authority_wf; simpl.
    + reflexivity.
    + unfold received. simpl.
      unfold chain.
      rewrite nth_error_seq_map by lia.
      simpl.
      destruct k as [| k']; simpl; reflexivity.
Qed.

Theorem chain_resolves_every_name :
  forall n i,
    i < n ->
    resolve (chain n) i = Some i.
Proof.
  intros n i Hi.
  assert (Hexp : forall j, exports_at (chain n) j i = true -> j = i).
  { intros j Hj.
    unfold exports_at, chain in Hj.
    destruct (nth_error (map chain_module (seq 0 n)) j) as [m |] eqn:Hn;
      [| discriminate Hj].
    assert (Hjn : j < n).
    { assert (j < length (map chain_module (seq 0 n))).
      { apply nth_error_Some. rewrite Hn. discriminate. }
      rewrite map_length, seq_length in H. exact H. }
    rewrite nth_error_seq_map in Hn by exact Hjn.
    inversion Hn; subst m.
    unfold module_exports in Hj.
    apply existsb_exists in Hj.
    destruct Hj as [x [Hxin Hxeq]].
    apply Nat.eqb_eq in Hxeq; subst x.
    destruct j as [| k]; simpl in Hxin;
      destruct Hxin as [Hx | []]; lia. }
  apply resolve_finds.
  - unfold exports_at, chain.
    rewrite nth_error_seq_map by exact Hi.
    replace (0 + i) with i by lia.
    cbn beta iota.
    unfold module_exports.
    apply existsb_exists.
    exists i. split.
    + destruct i as [| k]; simpl; left; reflexivity.
    + apply Nat.eqb_refl.
  - exact Hexp.
Qed.

Lemma fanout_stratified : forall n, link_stratified (fanout n) = true.
Proof.
  intros n.
  unfold link_stratified, fanout. simpl.
  assert (H : forall k b, b > 0 ->
    stratified_from b (map (fun i => mkModule [0] [S i] [] [0]) (seq (b - 1) k)) = true).
  { induction k as [| k' IH]; intros b Hb; simpl.
    - reflexivity.
    - apply andb_true_iff. split.
      + simpl. rewrite andb_true_r. apply Nat.ltb_lt. lia.
      + replace (S (b - 1)) with (S b - 1) by lia.
        apply IH. lia. }
  specialize (H n 1).
  simpl in H.
  replace (1 - 1) with 0 in H by lia.
  apply H. lia.
Qed.

Theorem fanout_wf : forall n, link_wf (fanout n) = true.
Proof.
  intros n.
  unfold link_wf. apply andb_true_iff. split.
  - apply fanout_stratified.
  - apply forallb_forall.
    intros m Hm.
    simpl in Hm.
    destruct Hm as [Hm | Hm].
    + subst m. unfold module_authority_wf. simpl. reflexivity.
    + apply in_map_iff in Hm.
      destruct Hm as [i [Hf _]].
      subst m.
      unfold module_authority_wf. simpl. reflexivity.
Qed.

(* Executable witnesses at n = 16: the checkers hold by evaluation *)
(* on links past the ten-module load line.                          *)
Example chain_16_wf : link_wf (chain 16) = true.
Proof. vm_compute. reflexivity. Qed.

Example chain_16_deep_resolution : resolve (chain 16) 15 = Some 15.
Proof. vm_compute. reflexivity. Qed.

Example chain_16_rooted : rooted (chain 16) 0 = true.
Proof. vm_compute. reflexivity. Qed.

Example fanout_16_wf : link_wf (fanout 16) = true.
Proof. vm_compute. reflexivity. Qed.

Example fanout_16_wide_resolution : resolve (fanout 16) 16 = Some 16.
Proof. vm_compute. reflexivity. Qed.

(* Rejection witnesses: the same checkers refuse the three failure *)
(* shapes the surface design must never admit.                      *)
Example cycle_rejected :
  link_stratified
    [ mkModule [1] [0] [0] [0] ; mkModule [0] [1] [] [] ] = false.
Proof. vm_compute. reflexivity. Qed.

Example authority_from_nowhere_rejected :
  link_wf [ mkModule [] [] [] [7] ] = false.
Proof. vm_compute. reflexivity. Qed.

Example ambiguous_export_unresolvable :
  resolve [ mkModule [] [5] [] [] ; mkModule [] [5] [] [] ] 5 = None.
Proof. vm_compute. reflexivity. Qed.
