(* Composition consumers over existing authorities. No third heap, allocator,
   graph interpreter, token verifier, or root-binding issuer is defined here.

   Reuse is the case every claim below has to survive. Physical storage is
   reused: a freed address can hold the next allocation at once.
   - The canonical machine stores no references. Every name's footprint is
     live, so a placement that maps a dead block and a live block to the same
     address is invisible to every name ([dead_block_unreferenced],
     [reuse_never_aliases_names], [canonical_reuse_witness]).
   - Graph links are stored references. A link that remembers storage (a
     block or an address) names whatever reuses that storage
     ([address_link_admits_reused_node]). Node identity is the graph model's
     (store, slot, generation) link, checked on every access
     ([stale_node_refused_under_live_root]).
   - Slot roots are reused too. A released slot is a tombstone that keeps its
     generation and is reclaimed at the next one, so the previous occupant's
     handle stays refused (SlotCalculus [stale_handle_never_admitted];
     [gen_one_reclaim_resurrects] is the earlier rule's counterexample).
   A fresh root generation says nothing about a node inside the root, and a
   live node says nothing about the root: access needs both checks
   ([root_validity_does_not_admit_stale_node],
   [node_validity_does_not_admit_stale_root]).

   Refusals marked (definitional) hold because the predicate states the
   requirement; they type-check the contract, they do not test a machine. *)
Require Import OwnershipCleanCore.
Require Import SlotCalculus.
Require Import OwnershipGraphLinks.
Require Import Stdlib.Lists.List.
Require Import Stdlib.Arith.PeanoNat.
Require Import Stdlib.Bool.Bool.
Require Import Stdlib.Sorting.Permutation.
Require Import Stdlib.micromega.Lia.
Import ListNotations.

Module OC := OwnershipCleanCore.
Module SC := SlotCalculus.
Module GL := OwnershipGraphLinks.

Definition RootBinding := (SC.Handle * OC.TVar)%type.

(* ------------------------------------------------------------------ *)
(* 1. The canonical machine is reuse-ready                              *)
(* ------------------------------------------------------------------ *)

(* A block outside the live heap is in no owner's and no borrower's
   footprint. *)
Theorem dead_block_unreferenced : forall rho beta H n R b,
  OC.INV rho beta H n R -> ~ In b H ->
  forall z c bs, OC.tread z rho beta = Some (c, bs) -> ~ In b bs.
Proof.
  intros rho beta H n R b Hi Hb z c bs Hr Hin. apply Hb.
  exact (OC.inv_read rho beta H n R z c bs Hi Hr b Hin).
Qed.

(* A physical placement maps abstract blocks to addresses. It only has to
   be injective on the live heap: a dead block may share its address with
   a live one. *)
Definition Addr := nat.
Definition placement (A : OC.Block -> Addr) (H : list OC.Block) : Prop :=
  forall b1 b2, In b1 H -> In b2 H -> A b1 = A b2 -> b1 = b2.

(* Two names that reach the same address reach the same block, under any
   placement that reuses dead addresses. *)
Theorem reuse_never_aliases_names : forall rho beta H n R A x y cx bx cy bys b1 b2,
  OC.INV rho beta H n R -> placement A H ->
  OC.tread x rho beta = Some (cx, bx) -> OC.tread y rho beta = Some (cy, bys) ->
  In b1 bx -> In b2 bys -> A b1 = A b2 -> b1 = b2.
Proof.
  intros rho beta H n R A x y cx bx cy bys b1 b2 Hi Hp Hx Hy H1 H2 Ha.
  apply Hp; [exact (OC.inv_read _ _ _ _ _ _ _ _ Hi Hx b1 H1)| exact (OC.inv_read _ _ _ _ _ _ _ _ Hi Hy b2 H2)| exact Ha].
Qed.

Lemma empty_canonical_invariant : forall n, OC.INV [] [] [] n [].
Proof.
  intros n. unfold OC.INV, OC.dom, OC.heap_of, OC.laid.
  simpl. repeat split; try constructor; intros; simpl in *;
    try contradiction; try discriminate.
Qed.

(* x is dropped and y is then stored at x's old address: the placement is
   injective on each live heap, and x no longer names anything. *)
Definition reuse_place (b : OC.Block) : Addr := 0.

Example canonical_reuse_witness : forall tf tp,
  OC.texec tf tp [(OC.Src 0, (OC.SLeaf 1, [(0, 0)]))] [] [(0, 0)] 1 (OC.TDrop (OC.Src 0)) [] [] [] 1 [] /\
  OC.INV [(OC.Src 1, (OC.SLeaf 2, [(1, 0)]))] [] [(1, 0)] 2 [] /\
  placement reuse_place [(0, 0)] /\ placement reuse_place [(1, 0)] /\
  reuse_place (0, 0) = reuse_place (1, 0) /\
  OC.tlookup (OC.Src 0) [(OC.Src 1, (OC.SLeaf 2, [(1, 0)]))] = None.
Proof.
  intros tf tp. split.
  - change (OC.texec tf tp [(OC.Src 0, (OC.SLeaf 1, [(0, 0)]))] [] [(0, 0)] 1 (OC.TDrop (OC.Src 0))
      (OC.tremove (OC.Src 0) [(OC.Src 0, (OC.SLeaf 1, [(0, 0)]))]) [] (OC.free [(0, 0)] [(0, 0)]) 1 []).
    eapply OC.TE_Drop; [reflexivity| intros b Hb; exact Hb| repeat constructor; simpl; tauto].
  - split.
    + pose proof (OC.inv_alloc [] [] [] 1 [] (OC.Src 1) (OC.SLeaf 2)
        (empty_canonical_invariant 1) (conj eq_refl eq_refl)) as Hi.
      rewrite app_nil_r in Hi. exact Hi.
    + split; [intros b1 b2 [E1|[]] [E2|[]] _; congruence|].
      split; [intros b1 b2 [E1|[]] [E2|[]] _; congruence|].
      split; reflexivity.
Qed.

(* ------------------------------------------------------------------ *)
(* 2. Counterexample: an address is not a node identity                 *)
(* ------------------------------------------------------------------ *)

(* The earlier link view: a root handle and a block of the root's
   footprint. It is kept only to be refuted. *)
Definition AddressLink := (SC.Handle * OC.Block)%type.

Definition address_member (rho : OC.TEnv) (binding : option RootBinding) (link : AddressLink) : Prop :=
  exists handle x c bs,
    binding = Some (handle, x) /\ fst link = handle /\
    OC.tlookup x rho = Some (c, bs) /\ In (snd link) bs.

Definition demo_owner := SC.mkHandle 1 3.
Definition demo_binding : option RootBinding := Some (demo_owner, OC.Src 0).

(* The document graph before and after B is deleted and D is stored in B's
   exact block (4, 0). The root's footprint is the same list of blocks. *)
Definition graph_before := GL.grun 3 GL.gempty GL.doc_build.
Definition graph_after := GL.grun 3 GL.gempty (GL.doc_build ++ GL.reuse_ops).

Definition root_content := OC.SNode [OC.SLeaf 10; OC.SLeaf 11; OC.SLeaf 12].

Definition root_fp (g : GL.GS) : list OC.Block :=
  match GL.find_store 0 (GL.gstores g) with Some s => GL.store_fp s | None => [] end.

Definition root_env (g : GL.GS) : OC.TEnv := [(OC.Src 0, (root_content, root_fp g))].

(* Before and after the reuse, the address view admits (4, 0), and the node
   in (4, 0) has changed from B to D: the link would read D as if it were
   B, with no failure. The generation check refuses B's link. *)
Example address_link_admits_reused_node :
  address_member (root_env graph_before) demo_binding (demo_owner, (4, 0)) /\
  address_member (root_env graph_after) demo_binding (demo_owner, (4, 0)) /\
  GL.address_holder graph_before 0 (4, 0) = Some 11 /\
  GL.address_holder graph_after 0 (4, 0) = Some 13 /\
  GL.resolve graph_after GL.linkB = GL.Missing GL.RStale.
Proof.
  split; [exists demo_owner, (OC.Src 0), root_content, (root_fp graph_before);
          repeat split; simpl; tauto|].
  split; [exists demo_owner, (OC.Src 0), root_content, (root_fp graph_after);
          repeat split; simpl; tauto|].
  repeat split; reflexivity.
Qed.

(* ------------------------------------------------------------------ *)
(* 3. Access                                                            *)
(* ------------------------------------------------------------------ *)

(* AccessValid (doc 28): the root is live under its handle, the link names
   a current node of the store the root owns, and the root's canonical
   owner holds exactly that store's footprint. The binding itself is an
   admitted fact; its issuer is OPEN. *)
Definition AccessValid gmax (g : GL.GS) rho beta H n R (binding : option RootBinding)
    slots caps tok (l : GL.Link) (nd : GL.Node) : Prop :=
  GL.GInv gmax g /\ OC.INV rho beta H n R /\
  (exists handle x c s, binding = Some (handle, x) /\
     GL.find_store (GL.lsid l) (GL.gstores g) = Some s /\
     OC.tlookup x rho = Some (c, GL.store_fp s) /\
     SC.HandleRead slots caps handle tok) /\
  GL.resolve g l = GL.Found nd.

(* An admitted access reaches only the owner's live storage. *)
Theorem access_targets_owned_live_storage : forall gmax g rho beta H n R binding slots caps tok l nd,
  AccessValid gmax g rho beta H n R binding slots caps tok l nd ->
  forall b, In b (GL.nblocks nd) -> In b H /\ In b (GL.gheap g).
Proof.
  intros gmax g rho beta H n R binding slots caps tok l nd
    [Hg [Hi [[handle [x [c [s [_ [Hs [Hl _]]]]]]] Hr]]] b Hb.
  destruct (GL.resolve_in_store_fp gmax g l nd Hg Hr) as [s' [Hs' [Hin Hheap]]].
  rewrite Hs in Hs'. injection Hs' as Hs'. subst s'.
  split; [|exact (Hheap b Hb)].
  apply (OC.inv_read rho beta H n R x c (GL.store_fp s) Hi); [unfold OC.tread; rewrite Hl; reflexivity|].
  exact (Hin b Hb).
Qed.

(* A stale node link is refused whatever the root's state. *)
Theorem root_validity_does_not_admit_stale_node : forall gmax g rho beta H n R binding slots caps tok l nd,
  GL.stale g l -> ~ AccessValid gmax g rho beta H n R binding slots caps tok l nd.
Proof.
  intros gmax g rho beta H n R binding slots caps tok l nd Hst [_ [_ [_ Hr]]].
  destruct (GL.stale_missing g l Hst) as [r Hm]. congruence.
Qed.

(* A stale root handle refuses access whatever the node's state. *)
Theorem node_validity_does_not_admit_stale_root : forall gmax g rho beta H n R handle x slots caps tok l nd,
  SC.HandleStale slots handle ->
  ~ AccessValid gmax g rho beta H n R (Some (handle, x)) slots caps tok l nd.
Proof.
  intros gmax g rho beta H n R handle x slots caps tok l nd Hst
    [_ [_ [[handle' [x' [c [s [E [_ [_ Hread]]]]]]] _]]].
  injection E as E1 E2. subst handle'.
  destruct (SC.stale_handle_admits_nothing slots caps handle tok Hst) as [Hno _]. exact (Hno Hread).
Qed.

(* The concrete reuse: after D reuses B's block, B's link is refused even
   with a live root whose footprint still contains (4, 0); D's own link is
   admitted when the root's read guard holds. *)
Lemma graph_after_invariant : GL.GInv 3 graph_after.
Proof. apply GL.ginv_run. apply GL.ginv_empty. Qed.

Lemma graph_before_invariant : GL.GInv 3 graph_before.
Proof. apply GL.ginv_run. apply GL.ginv_empty. Qed.

Lemma graph_after_heap : GL.gheap graph_after = [(4, 0); (6, 0); (5, 0); (2, 0)].
Proof. reflexivity. Qed.

Lemma root_after_invariant : OC.INV (root_env graph_after) [] (GL.gheap graph_after) 10 [].
Proof.
  unfold OC.INV. split; [simpl; repeat constructor; simpl; tauto|].
  split; [exact (GL.gi_nodup 3 graph_after graph_after_invariant)|].
  split.
  - replace (OC.heap_of (root_env graph_after) ++ []) with (GL.gfp (GL.gstores graph_after)) by reflexivity.
    exact (GL.gi_perm 3 graph_after graph_after_invariant).
  - split; [intros b Hb; rewrite graph_after_heap in Hb; simpl in Hb; intuition (subst; simpl; lia)|].
    split; [intros z c bs F; discriminate|].
    split; [|intros z c bs F; discriminate].
    intros z c bs Hl. unfold root_env in Hl. rewrite OC.tlookup_cons in Hl.
    destruct (OC.tvar_eq_dec (OC.Src 0) z); [injection Hl as Hc Hb; subst; reflexivity| simpl in Hl; discriminate].
Qed.

Example stale_node_refused_under_live_root : forall slots caps tok nd,
  SC.HandleRead slots caps demo_owner tok ->
  ~ AccessValid 3 graph_after (root_env graph_after) [] (GL.gheap graph_after) 10 []
      demo_binding slots caps tok GL.linkB nd /\
  AccessValid 3 graph_after (root_env graph_after) [] (GL.gheap graph_after) 10 []
      demo_binding slots caps tok (GL.mkLink 0 1 1) (GL.mkNode 13 [] [(4, 0)]).
Proof.
  intros slots caps tok nd Hread. split.
  - intros [_ [_ [_ Hr]]]. vm_compute in Hr. discriminate.
  - split; [exact graph_after_invariant|]. split; [exact root_after_invariant|].
    split; [|reflexivity].
    exists demo_owner, (OC.Src 0), root_content.
    eexists. split; [reflexivity|]. split; [reflexivity|]. split; [reflexivity| exact Hread].
Qed.

(* A link into another store does not resolve under this root. *)
Theorem foreign_store_link_refused : forall gmax g rho beta H n R handle x c s s' slots caps tok l nd,
  OC.tlookup x rho = Some (c, GL.store_fp s') ->
  GL.find_store (GL.lsid l) (GL.gstores g) = Some s -> GL.store_fp s <> GL.store_fp s' ->
  ~ AccessValid gmax g rho beta H n R (Some (handle, x)) slots caps tok l nd.
Proof.
  intros gmax g rho beta H n R handle x c s s' slots caps tok l nd Hx Hs Hne
    [_ [_ [[handle' [x' [c' [s0 [E [Hs0 [Hl _]]]]]]] _]]].
  injection E as E1 E2. subst x'. rewrite Hs in Hs0. injection Hs0 as Hs0. subst s0.
  rewrite Hx in Hl. apply Hne. congruence.
Qed.

(* ------------------------------------------------------------------ *)
(* 4. Retirement, with the rest of the program live                     *)
(* ------------------------------------------------------------------ *)

(* The canonical heap splits into the graph heap and the storage of every
   other owner. *)
Definition HeapSplit (H : list OC.Block) (g : GL.GS) (Ho : list OC.Block) : Prop :=
  NoDup (GL.gheap g ++ Ho) /\ Permutation H (GL.gheap g ++ Ho).

Definition RetireReady gmax (g : GL.GS) sid rho beta H n R Ho (binding : option RootBinding)
    slots caps tok : Prop :=
  GL.GInv gmax g /\ OC.INV rho beta H n R /\ HeapSplit H g Ho /\
  exists handle x c s, binding = Some (handle, x) /\
    GL.find_store sid (GL.gstores g) = Some s /\
    OC.tlookup x rho = Some (c, GL.store_fp s) /\
    GL.store_borrowed sid (GL.gbor g) = false /\
    SC.HandleRelease slots caps handle tok.

Lemma free_disjoint : forall bs O, (forall b, In b O -> ~ In b bs) -> OC.free bs O = O.
Proof.
  intros bs O Hd. unfold OC.free. apply OC.filter_all. intros b Hb.
  destruct (OC.bmem b bs) eqn:E; [|reflexivity].
  apply OC.bmem_true in E. exfalso. exact (Hd b Hb E).
Qed.

Lemma free_split : forall bs G O,
  NoDup (G ++ O) -> (forall b, In b bs -> In b G) ->
  OC.free bs (G ++ O) = OC.free bs G ++ O.
Proof.
  intros bs G O HN Hsub. unfold OC.free at 1. rewrite filter_app. f_equal.
  apply free_disjoint. intros b Hb Hbs.
  destruct (OC.NoDup_app_split _ _ _ HN) as [_ [_ Hd]]. exact (Hd b (Hsub b Hbs) Hb).
Qed.

(* Whole-heap allocation consumers. gexec alone is only a graph fragment;
   composition consumes the required external footprint through gexec_framed.
   No global allocator, extra heap, or guessed root binding is defined here. *)
Lemma heap_split_keeps_external_storage : forall H g Ho,
  HeapSplit H g Ho -> forall b, In b Ho -> In b H /\ ~ In b (GL.gheap g).
Proof.
  intros H g Ho [HN HP] b Hb. split.
  - eapply Permutation_in; [apply Permutation_sym; exact HP|]. apply in_or_app. right. exact Hb.
  - intros Hg. exact (proj2 (proj2 (OC.NoDup_app_split _ _ _ HN)) b Hg Hb).
Qed.

Lemma free_empty_heap : forall H, OC.free [] H = H.
Proof. intros H. unfold OC.free. apply OC.filter_all. intros b _. reflexivity. Qed.

(* The delta uses the SAME canonical free operation on the whole heap.
   Post-state disjointness is supplied by the graph owner's pre-admission
   theorem, not assumed at an application call site. *)
Lemma heap_split_after_graph_delta : forall H g Ho g' added retired,
  HeapSplit H g Ho ->
  (forall b, In b retired -> In b (GL.gheap g)) ->
  GL.gheap g' = added ++ OC.free retired (GL.gheap g) ->
  NoDup (GL.gheap g' ++ Ho) ->
  HeapSplit (added ++ OC.free retired H) g' Ho.
Proof.
  intros H g Ho g' added retired [HN HP] Hsub Hdelta HN'.
  split; [exact HN'|]. rewrite Hdelta, <- app_assoc, <- (free_split _ _ _ HN Hsub).
  apply Permutation_app_head. apply OC.Permutation_filter_compat. exact HP.
Qed.

Theorem new_store_allocation_agrees_split : forall gmax g H Ho tb g' sid,
  GL.GInv gmax g -> HeapSplit H g Ho ->
  GL.gexec_framed gmax (Some Ho) g (GL.ONew tb) = (g', GL.GSid sid) ->
  GL.GInv gmax g' /\ HeapSplit (tb :: H) g' Ho.
Proof.
  intros gmax g H Ho tb g' sid Hi Hsplit E.
  destruct (GL.framed_step_preserves_ownership gmax Ho g (GL.ONew tb) Hi (proj1 Hsplit))
    as [Hi' HN']. rewrite E in Hi', HN'. cbn in Hi', HN'. split; [exact Hi'|].
  unfold GL.gexec_framed in E. destruct (GL.bfree (GL.allocation_blocks g (GL.ONew tb)) Ho);
    [|discriminate]. cbn [GL.gexec] in E. unfold GL.g_new in E.
  destruct (OC.bmem tb (GL.gheap g)); [discriminate|]. injection E as Eg _. subst g'.
  replace (tb :: H) with ([tb] ++ OC.free [] H) by (rewrite free_empty_heap; reflexivity).
  eapply heap_split_after_graph_delta; [exact Hsplit|intros b []| |exact HN'].
  cbn [GL.gheap]. rewrite free_empty_heap. reflexivity.
Qed.

Theorem vacant_insert_allocation_agrees_split : forall gmax g H Ho sid idx d es bs tb s sl g' l,
  GL.GInv gmax g -> HeapSplit H g Ho ->
  GL.find_store sid (GL.gstores g) = Some s ->
  nth_error (GL.sslots s) idx = Some sl -> GL.snode sl = None ->
  GL.gexec_framed gmax (Some Ho) g (GL.OInsert sid idx d es bs tb) = (g', GL.GLink l) ->
  GL.GInv gmax g' /\ HeapSplit (bs ++ H) g' Ho.
Proof.
  intros gmax g H Ho sid idx d es bs tb s sl g' l Hi Hsplit Hs Hsl Hvac E.
  destruct (GL.framed_step_preserves_ownership gmax Ho g (GL.OInsert sid idx d es bs tb)
    Hi (proj1 Hsplit)) as [Hi' HN']. rewrite E in Hi', HN'. cbn in Hi', HN'.
  split; [exact Hi'|]. unfold GL.gexec_framed in E.
  destruct (GL.bfree (GL.allocation_blocks g (GL.OInsert sid idx d es bs tb)) Ho);
    [|discriminate]. cbn [GL.gexec] in E. unfold GL.g_insert in E. rewrite Hs, Hsl, Hvac in E.
  destruct (Nat.ltb (GL.sgen sl) gmax); [|discriminate].
  destruct (GL.bnodup bs && GL.bfree bs (GL.gheap g)); [|discriminate].
  injection E as Eg _. subst g'.
  replace (bs ++ H) with (bs ++ OC.free [] H) by (rewrite free_empty_heap; reflexivity).
  eapply heap_split_after_graph_delta; [exact Hsplit|intros b []| |exact HN'].
  cbn [GL.gheap]. rewrite free_empty_heap. reflexivity.
Qed.

Theorem growth_allocation_agrees_split : forall gmax g H Ho sid idx d es bs tb s g' l,
  GL.GInv gmax g -> HeapSplit H g Ho ->
  GL.find_store sid (GL.gstores g) = Some s -> nth_error (GL.sslots s) idx = None ->
  GL.gexec_framed gmax (Some Ho) g (GL.OInsert sid idx d es bs tb) = (g', GL.GLink l) ->
  GL.GInv gmax g' /\ HeapSplit (bs ++ tb :: OC.free [GL.stab s] H) g' Ho.
Proof.
  intros gmax g H Ho sid idx d es bs tb s g' l Hi Hsplit Hs Hsl E.
  destruct (GL.framed_step_preserves_ownership gmax Ho g (GL.OInsert sid idx d es bs tb)
    Hi (proj1 Hsplit)) as [Hi' HN']. rewrite E in Hi', HN'. cbn in Hi', HN'.
  split; [exact Hi'|].
  assert (Htable : In (GL.stab s) (GL.gheap g)).
  { eapply Permutation_in; [apply Permutation_sym; exact (GL.gi_perm gmax g Hi)|].
    unfold GL.gfp. apply in_flat_map. exists s. split;
      [exact (proj1 (GL.find_store_some _ _ _ Hs))|left; reflexivity]. }
  unfold GL.gexec_framed in E.
  destruct (GL.bfree (GL.allocation_blocks g (GL.OInsert sid idx d es bs tb)) Ho);
    [|discriminate]. cbn [GL.gexec] in E. unfold GL.g_insert in E. rewrite Hs, Hsl in E.
  destruct (GL.store_borrowed sid (GL.gbor g)); [discriminate|].
  destruct (Nat.eqb idx (length (GL.sslots s)) && Nat.ltb 0 gmax); [|discriminate].
  destruct (GL.bnodup (tb :: bs) && GL.bfree (tb :: bs) (OC.free [GL.stab s] (GL.gheap g)));
    [|discriminate]. injection E as Eg _. subst g'.
  replace (bs ++ tb :: OC.free [GL.stab s] H)
    with ((bs ++ [tb]) ++ OC.free [GL.stab s] H) by (rewrite <- app_assoc; reflexivity).
  eapply heap_split_after_graph_delta; [exact Hsplit| | |exact HN'].
  - intros b [Hb|[]]. subst b. exact Htable.
  - cbn [GL.gheap]. rewrite <- app_assoc. reflexivity.
Qed.

(* Graph ODrop and canonical TDrop free the same footprint; the other owners'
   storage is untouched; the Slot root becomes a tombstone, so its handle is
   stale from then on. One retirement, three views. *)
Theorem retirement_agrees_split : forall tf tp gmax g sid rho beta H n R Ho binding slots caps tok,
  RetireReady gmax g sid rho beta H n R Ho binding slots caps tok ->
  exists handle x s slot,
    binding = Some (handle, x) /\ GL.find_store sid (GL.gstores g) = Some s /\
    snd (GL.gexec_framed gmax (Some Ho) g (GL.ODrop sid)) = GL.GUnit /\
    OC.texec tf tp rho beta H n (OC.TDrop x) (OC.tremove x rho) beta (OC.free (GL.store_fp s) H) n [] /\
    OC.INV (OC.tremove x rho) beta (OC.free (GL.store_fp s) H) n R /\
    GL.GInv gmax (fst (GL.gexec_framed gmax (Some Ho) g (GL.ODrop sid))) /\
    HeapSplit (OC.free (GL.store_fp s) H) (fst (GL.gexec_framed gmax (Some Ho) g (GL.ODrop sid))) Ho /\
    (forall b, In b (GL.store_fp s) -> ~ In b (OC.free (GL.store_fp s) H)) /\
    (forall b, In b Ho -> In b (OC.free (GL.store_fp s) H)) /\
    slots (SC.h_slot handle) = Some slot /\
    SC.Step slots caps (SC.update_heap slots (SC.h_slot handle)
                          (Some (SC.mkSlot 0 (SC.s_gen slot) SC.Unpinned false))) /\
    SC.HandleStale (SC.update_heap slots (SC.h_slot handle)
                      (Some (SC.mkSlot 0 (SC.s_gen slot) SC.Unpinned false))) handle.
Proof.
  intros tf tp gmax g sid rho beta H n R Ho binding slots caps tok
    [Hg [Hi [[HN HP] [handle [x [c [s [E [Hs [Hl [Hb Hr]]]]]]]]]]].
  destruct (OC.inv_drop rho beta H n R x c (GL.store_fp s) Hi Hl) as [Hincl [Hnd Ha]].
  destruct (GL.drop_releases_exactly gmax g sid s Hg Hs Hb) as [Hperm [_ Hgone]].
  assert (Hg' : GL.gheap (fst (GL.gexec_framed gmax (Some Ho) g (GL.ODrop sid))) = OC.free (GL.store_fp s) (GL.gheap g))
    by (cbn [GL.gexec_framed GL.allocation_blocks GL.bfree GL.gexec];
        unfold GL.g_drop; rewrite Hs, Hb; reflexivity).
  assert (Hsub : forall b, In b (GL.store_fp s) -> In b (GL.gheap g)).
  { intros b Hin. eapply Permutation_in; [apply Permutation_sym; exact (GL.gi_perm gmax g Hg)|].
    unfold GL.gfp. apply in_flat_map. exists s.
    split; [exact (proj1 (GL.find_store_some _ _ _ Hs))| exact Hin]. }
  assert (HfH : Permutation (OC.free (GL.store_fp s) H) (OC.free (GL.store_fp s) (GL.gheap g) ++ Ho)).
  { rewrite <- (free_split _ _ _ HN Hsub). apply OC.Permutation_filter_compat. exact HP. }
  destruct Hr as [slot [Hslot [Hlive [Hgen [Hpin [Hcap Hverify]]]]]].
  exists handle, x, s, slot. split; [exact E|]. split; [exact Hs|].
  split; [cbn [GL.gexec_framed GL.allocation_blocks GL.bfree GL.gexec];
          unfold GL.g_drop; rewrite Hs, Hb; reflexivity|].
  split; [eapply OC.TE_Drop; eassumption|].
  split; [exact Ha|].
  split; [apply GL.ginv_framed_step; exact Hg|].
  split.
  - unfold HeapSplit. rewrite Hg'. split.
    + rewrite <- (free_split _ _ _ HN Hsub). unfold OC.free. apply OC.NoDup_filter'. exact HN.
    + exact HfH.
  - split.
    + intros b Hin Hh. unfold OC.free in Hh. apply filter_In in Hh. destruct Hh as [_ Hh].
      apply negb_true_iff in Hh. assert (OC.bmem b (GL.store_fp s) = true) by (apply OC.bmem_true; exact Hin).
      congruence.
    + split.
      * intros b Hb'. eapply Permutation_in; [apply Permutation_sym; exact HfH|].
        apply in_or_app. right. exact Hb'.
      * split; [exact Hslot|]. split.
        -- eapply SC.Step_Release; [exact Hslot| exact Hlive| exact Hcap| rewrite <- Hgen; exact Hverify|
             exact Hpin| reflexivity].
        -- exists (SC.mkSlot 0 (SC.s_gen slot) SC.Unpinned false). unfold SC.update_heap.
           rewrite Nat.eqb_refl. split; [reflexivity|]. right. simpl. split; [exact Hgen| reflexivity].
Qed.

(* After the retirement each view refuses a second one: the canonical
   owner is unbound, the graph store is gone, and the Slot root is a
   tombstone. *)
Theorem retirement_consumes_once : forall gmax g sid rho handle x slot slots caps tok tf tp beta H n,
  snd (GL.gexec gmax g (GL.ODrop sid)) = GL.GUnit ->
  slots (SC.h_slot handle) = Some slot ->
  let slots' := SC.update_heap slots (SC.h_slot handle)
                  (Some (SC.mkSlot 0 (SC.s_gen slot) SC.Unpinned false)) in
  snd (GL.gexec gmax (fst (GL.gexec gmax g (GL.ODrop sid))) (GL.ODrop sid)) = GL.GRefused GL.RNoStore /\
  ~ SC.HandleRelease slots' caps handle tok /\
  (forall rho' beta' H' n' tr, ~ OC.texec tf tp (OC.tremove x rho) beta H n (OC.TDrop x) rho' beta' H' n' tr).
Proof.
  intros gmax g sid rho handle x slot slots caps tok tf tp beta H n Hd Hslot slots'.
  split.
  - cbn [GL.gexec] in Hd |- *. unfold GL.g_drop in Hd |- *.
    destruct (GL.find_store sid (GL.gstores g)) as [s|] eqn:Ef; [|discriminate].
    destruct (GL.store_borrowed sid (GL.gbor g)); [discriminate|]. simpl.
    change (GL.find_store sid (GL.del_store sid (GL.gstores g))) with
      (GL.find_store sid (GL.del_store sid (GL.gstores g))).
    rewrite GL.find_del_same. reflexivity.
  - split.
    + apply (SC.tombstone_release_impossible slots' caps handle
               (SC.mkSlot 0 (SC.s_gen slot) SC.Unpinned false) tok); [|reflexivity].
      unfold slots', SC.update_heap. rewrite Nat.eqb_refl. reflexivity.
    + intros rho' beta' H' n' tr Hex. inversion Hex; subst.
      match goal with Hl : OC.tlookup x (OC.tremove x rho) = Some _ |- _ =>
        rewrite OC.tlookup_tremove in Hl end.
      destruct (OC.tvar_eq_dec x x); [discriminate| contradiction].
Qed.

(* One block has one canonical owner, so a store's footprint has one
   retirement obligation: an explicit release and a synthesized drop cannot
   both own it. *)
Lemma tlookup_heap_of : forall rho x c bs,
  OC.tlookup x rho = Some (c, bs) -> forall b, In b bs -> In b (OC.heap_of rho).
Proof.
  induction rho as [|[k [c0 bs0]] r IH]; intros x c bs Hl b Hb; [discriminate|].
  rewrite OC.tlookup_cons in Hl. unfold OC.heap_of. simpl. apply in_or_app.
  destruct (OC.tvar_eq_dec k x) as [E|E].
  - injection Hl as Hc Hbs. subst. left. exact Hb.
  - right. exact (IH x c bs Hl b Hb).
Qed.

Theorem one_canonical_owner_per_block : forall rho beta H n R x y cx bx cy bys b,
  OC.INV rho beta H n R ->
  OC.tlookup x rho = Some (cx, bx) -> OC.tlookup y rho = Some (cy, bys) ->
  In b bx -> In b bys -> x = y.
Proof.
  intros rho beta H n R x y cx bx cy bys b Hi.
  destruct (OC.heap_is_live_footprint rho beta H n R Hi) as [_ HN].
  apply OC.NoDup_app_split in HN. destruct HN as [HN _]. clear Hi.
  revert x y cx bx cy bys b HN.
  induction rho as [|[k [c0 bs0]] r IH]; intros x y cx bx cy bys b HN Hx Hy Hbx Hby; [discriminate|].
  rewrite OC.tlookup_cons in Hx, Hy. unfold OC.heap_of in HN. simpl in HN.
  destruct (OC.NoDup_app_split _ _ _ HN) as [_ [HNr Hd]].
  destruct (OC.tvar_eq_dec k x) as [Ex|Ex]; destruct (OC.tvar_eq_dec k y) as [Ey|Ey].
  - congruence.
  - injection Hx as _ Hb0. subst bx. exfalso.
    exact (Hd b Hbx (tlookup_heap_of r y cy bys Hy b Hby)).
  - injection Hy as _ Hb0. subst bys. exfalso.
    exact (Hd b Hby (tlookup_heap_of r x cx bx Hx b Hbx)).
  - exact (IH x y cx bx cy bys b HNr Hx Hy Hbx Hby).
Qed.

(* The whole program keeps another value live while the graph is retired. *)
Definition other_env (g : GL.GS) : OC.TEnv :=
  [(OC.Src 0, (root_content, root_fp g)); (OC.Src 1, (OC.SLeaf 5, [(9, 0)]))].

Lemma graph_before_heap : GL.gheap graph_before = [(6, 0); (5, 0); (4, 0); (2, 0)].
Proof. reflexivity. Qed.

Lemma other_env_invariant :
  OC.INV (other_env graph_before) [] (GL.gheap graph_before ++ [(9, 0)]) 10 [].
Proof.
  unfold OC.INV. split; [simpl; repeat constructor; simpl; intuition discriminate|].
  split; [rewrite graph_before_heap; simpl; repeat constructor; simpl; intuition discriminate|].
  split.
  - replace (OC.heap_of (other_env graph_before) ++ [])
      with (GL.gfp (GL.gstores graph_before) ++ [(9, 0)]) by reflexivity.
    apply Permutation_app_tail. exact (GL.gi_perm 3 graph_before graph_before_invariant).
  - split; [intros b Hb; rewrite graph_before_heap in Hb; simpl in Hb; intuition (subst; simpl; lia)|].
    split; [intros z c bs F; discriminate|].
    split; [|intros z c bs F; discriminate].
    intros z c bs Hl. unfold other_env in Hl. rewrite OC.tlookup_cons in Hl.
    destruct (OC.tvar_eq_dec (OC.Src 0) z); [injection Hl as Hc Hb; subst; reflexivity|].
    rewrite OC.tlookup_cons in Hl.
    destruct (OC.tvar_eq_dec (OC.Src 1) z); [injection Hl as Hc Hb; subst; reflexivity| simpl in Hl; discriminate].
Qed.

Example retirement_with_other_values_live : forall slots caps tok,
  SC.HandleRelease slots caps demo_owner tok ->
  RetireReady 3 graph_before 0 (other_env graph_before) [] (GL.gheap graph_before ++ [(9, 0)]) 10 []
    [(9, 0)] demo_binding slots caps tok.
Proof.
  intros slots caps tok Hr. split; [exact graph_before_invariant|].
  split; [exact other_env_invariant|].
  split; [split; [rewrite graph_before_heap; repeat constructor; simpl; intuition discriminate|
                  apply Permutation_refl]|].
  exists demo_owner, (OC.Src 0), root_content.
  eexists. split; [reflexivity|]. split; [reflexivity|]. split; [reflexivity|].
  split; [reflexivity| exact Hr].
Qed.

(* ------------------------------------------------------------------ *)
(* 5. Refusals carried over from the existing guards                    *)
(* ------------------------------------------------------------------ *)

(* (definitional) No binding, no access and no retirement. *)
Theorem missing_binding_refuses_access : forall gmax g rho beta H n R slots caps tok l nd,
  ~ AccessValid gmax g rho beta H n R None slots caps tok l nd.
Proof.
  intros gmax g rho beta H n R slots caps tok l nd [_ [_ [[handle [x [c [s [E _]]]]] _]]].
  discriminate.
Qed.

Theorem missing_binding_refuses_retirement : forall gmax g sid rho beta H n R Ho slots caps tok,
  ~ RetireReady gmax g sid rho beta H n R Ho None slots caps tok.
Proof.
  intros gmax g sid rho beta H n R Ho slots caps tok [_ [_ [_ [handle [x [c [s [E _]]]]]]]].
  discriminate.
Qed.

(* (definitional) A live graph borrow refuses retirement. *)
Theorem graph_borrow_refuses_retirement : forall gmax g sid rho beta H n R Ho binding slots caps tok,
  GL.store_borrowed sid (GL.gbor g) = true ->
  ~ RetireReady gmax g sid rho beta H n R Ho binding slots caps tok.
Proof.
  intros gmax g sid rho beta H n R Ho binding slots caps tok Hb
    [_ [_ [_ [handle [x [c [s [_ [_ [_ [Hb' _]]]]]]]]]]]. congruence.
Qed.

Theorem stale_root_refuses_retirement : forall gmax g sid rho beta H n R Ho handle x slots caps tok,
  SC.HandleStale slots handle ->
  ~ RetireReady gmax g sid rho beta H n R Ho (Some (handle, x)) slots caps tok.
Proof.
  intros gmax g sid rho beta H n R Ho handle x slots caps tok Hst
    [_ [_ [_ [handle' [x' [c [s [E [_ [_ [_ Hr]]]]]]]]]]].
  injection E as E1 E2. subst handle'.
  destruct (SC.stale_handle_admits_nothing slots caps handle tok Hst) as [_ [_ [_ Hno]]]. exact (Hno Hr).
Qed.

Theorem pinned_root_refuses_retirement : forall gmax g sid rho beta H n R Ho handle x slots caps tok slot,
  slots (SC.h_slot handle) = Some slot -> SC.s_pin slot = SC.Pinned ->
  ~ RetireReady gmax g sid rho beta H n R Ho (Some (handle, x)) slots caps tok.
Proof.
  intros gmax g sid rho beta H n R Ho handle x slots caps tok slot Hslot Hpin
    [_ [_ [_ [handle' [x' [c [s [E [_ [_ [_ Hr]]]]]]]]]]].
  injection E as E1 E2. subst handle'.
  exact (SC.pinned_handle_release_impossible _ _ _ _ _ Hslot Hpin Hr).
Qed.

Theorem unissued_token_refuses_retirement : forall gmax g sid rho beta H n R Ho binding slots caps tok,
  caps tok = false -> ~ RetireReady gmax g sid rho beta H n R Ho binding slots caps tok.
Proof.
  intros gmax g sid rho beta H n R Ho binding slots caps tok Htok
    [_ [_ [_ [handle [x [c [s [_ [_ [_ [_ Hr]]]]]]]]]]].
  exact (SC.unissued_token_release_impossible _ _ _ _ Htok Hr).
Qed.

Theorem unissued_token_refuses_access : forall gmax g rho beta H n R binding slots caps tok l nd,
  caps tok = false -> ~ AccessValid gmax g rho beta H n R binding slots caps tok l nd.
Proof.
  intros gmax g rho beta H n R binding slots caps tok l nd Htok
    [_ [_ [[handle [x [c [s [_ [_ [_ Hread]]]]]]] _]]].
  exact (SC.unissued_token_read_impossible _ _ _ _ Htok Hread).
Qed.

(* After the root is released and reclaimed, the previous root handle
   refuses every access, through any number of later Slot steps. *)
Theorem reclaimed_root_refuses_old_handle : forall gmax g rho beta H n R handle x slots slots' caps tok l nd,
  SC.HandleStale slots handle -> SC.Steps caps slots slots' ->
  ~ AccessValid gmax g rho beta H n R (Some (handle, x)) slots' caps tok l nd.
Proof.
  intros gmax g rho beta H n R handle x slots slots' caps tok l nd Hst Hsteps
    [_ [_ [[handle' [x' [c [s [E [_ [_ Hread]]]]]]] _]]].
  injection E as E1 E2. subst handle'.
  destruct (SC.stale_handle_never_admitted caps slots slots' handle tok Hsteps Hst) as [Hno _].
  exact (Hno Hread).
Qed.

(* ------------------------------------------------------------------ *)
(* 6. Rooted links: store identity carries the root's generation        *)
(* ------------------------------------------------------------------ *)

(* A store id that is reissued without a generation resurrects links
   (OwnershipGraphLinks [store_id_reuse_resurrects]). A rooted link also
   remembers the root handle it was issued under, and access checks that
   handle. Reusing the root bumps its generation, so the old link is
   refused even when its store id now names a different store. *)
Definition RootedLink := (SC.Handle * GL.Link)%type.

Definition RootedAccessValid gmax g rho beta H n R slots caps tok (rl : RootedLink) x nd : Prop :=
  AccessValid gmax g rho beta H n R (Some (fst rl, x)) slots caps tok (snd rl) nd.

Theorem rooted_link_refused_after_root_reuse : forall gmax g rho beta H n R slots slots' caps tok rl x nd,
  SC.HandleStale slots (fst rl) -> SC.Steps caps slots slots' ->
  ~ RootedAccessValid gmax g rho beta H n R slots' caps tok rl x nd.
Proof.
  intros gmax g rho beta H n R slots slots' caps tok rl x nd Hst Hsteps.
  unfold RootedAccessValid. eapply reclaimed_root_refuses_old_handle; eassumption.
Qed.

(* The store id 0 is dropped and reissued; the old link to A resolves in
   the new store. Its root slot was released (a tombstone at generation 3)
   and reclaimed at generation 4, so the rooted link issued under
   generation 3 is refused. *)
Definition reused_store_graph : GL.GS :=
  GL.grun 3 (GL.g_new_at (GL.grun 3 GL.gempty (GL.doc_build ++ [GL.ODrop 0])) 0 (8, 0))
    [GL.OInsert 0 0 99 [] [(2, 0)] (9, 0)].

Definition root_slots_reclaimed : SC.Heap :=
  fun id => if Nat.eqb id 1 then Some (SC.mkSlot 0 4 SC.Unpinned true) else None.

Example rooted_link_survives_store_id_reuse : forall rho beta H n R caps tok x nd,
  GL.strip (GL.resolve reused_store_graph GL.linkA) = Some (99, []) /\
  SC.HandleStale root_slots_reclaimed demo_owner /\
  ~ RootedAccessValid 3 reused_store_graph rho beta H n R root_slots_reclaimed caps tok
      (demo_owner, GL.linkA) x nd.
Proof.
  intros rho beta H n R caps tok x nd.
  assert (Hst : SC.HandleStale root_slots_reclaimed demo_owner).
  { exists (SC.mkSlot 0 4 SC.Unpinned true). split; [reflexivity|]. left. simpl. repeat constructor. }
  split; [reflexivity|]. split; [exact Hst|].
  unfold RootedAccessValid. apply node_validity_does_not_admit_stale_root. exact Hst.
Qed.

(* ------------------------------------------------------------------ *)
(* 7. Allocation/growth with another canonical owner live              *)
(* ------------------------------------------------------------------ *)

Lemma graph_before_split : HeapSplit (GL.gheap graph_before ++ [(9, 0)]) graph_before [(9, 0)].
Proof.
  split; [rewrite graph_before_heap; repeat constructor; simpl; intuition discriminate|].
  apply Permutation_refl.
Qed.

(* The original mixed-heap falsifier is retained: graph-fragment GInv is
   not enough to admit storage owned by the rest of the program. *)
Example graph_fragment_can_take_external_storage :
  OC.INV (other_env graph_before) [] (GL.gheap graph_before ++ [(9, 0)]) 10 [] /\
  HeapSplit (GL.gheap graph_before ++ [(9, 0)]) graph_before [(9, 0)] /\
  snd (GL.gexec 3 graph_before (GL.ONew (9, 0))) = GL.GSid 1 /\
  let g' := fst (GL.gexec 3 graph_before (GL.OInsert 0 3 99 [] [(9, 0)] (7, 0))) in
  GL.GInv 3 g' /\ ~ HeapSplit (GL.gheap g' ++ [(9, 0)]) g' [(9, 0)].
Proof.
  split; [exact other_env_invariant|]. split; [exact graph_before_split|].
  split; [reflexivity|]. split; [apply GL.ginv_step; exact graph_before_invariant|].
  intros [HN _]. cbn in HN. inversion HN as [|b rest Hnot _]; subst.
  apply Hnot. simpl. tauto.
Qed.

(* Exact state equality makes these pre-transition refusals, not rollback
   of a partial allocation or free. No accepted token is fabricated. *)
Example framed_allocation_refusals :
  GL.gexec_framed 3 (Some [(9, 0)]) graph_before (GL.ONew (9, 0)) =
    (graph_before, GL.GRefused GL.RNotFree) /\
  GL.gexec_framed 3 (Some [(9, 0)]) graph_before (GL.OInsert 0 3 99 [] [(9, 0)] (7, 0)) =
    (graph_before, GL.GRefused GL.RNotFree) /\
  GL.gexec_framed 3 (Some [(9, 0)]) graph_before (GL.OInsert 0 3 99 [] [(8, 0)] (9, 0)) =
    (graph_before, GL.GRefused GL.RNotFree) /\
  GL.gexec_framed 3 None graph_before (GL.ONew (8, 0)) =
    (graph_before, GL.GRefused GL.RNoFrame) /\
  GL.gexec_framed 3 (Some [(9, 0)]) graph_before (GL.OInsert 0 3 99 [] [(8, 0); (8, 0)] (7, 0)) =
    (graph_before, GL.GRefused GL.RNotFree) /\
  GL.gexec_framed 3 (Some [(9, 0)]) graph_before (GL.OInsert 0 3 99 [] [(6, 0)] (7, 0)) =
    (graph_before, GL.GRefused GL.RNotFree).
Proof. repeat split; reflexivity. Qed.

Example framed_vacant_insert_refuses_external_node :
  let g := fst (GL.gexec_framed 3 (Some [(9, 0)]) graph_before (GL.ODelete GL.linkB)) in
  GL.gexec_framed 3 (Some [(9, 0)]) g (GL.OInsert 0 1 99 [] [(9, 0)] (7, 0)) =
    (g, GL.GRefused GL.RNotFree) /\
  snd (GL.gexec_framed 3 (Some [(9, 0)]) g (GL.OInsert 0 1 99 [] [(4, 0)] (9, 0))) =
    GL.GLink (GL.mkLink 0 1 1).
Proof. split; reflexivity. Qed.

Example framed_vacant_insertion_preserves_split :
  let g := fst (GL.gexec_framed 3 (Some [(9, 0)]) graph_before (GL.ODelete GL.linkB)) in
  let g' := fst (GL.gexec_framed 3 (Some [(9, 0)]) g (GL.OInsert 0 1 99 [] [(4, 0)] (9, 0))) in
  GL.GInv 3 g' /\ HeapSplit ([(4, 0)] ++ GL.gheap g ++ [(9, 0)]) g' [(9, 0)].
Proof.
  destruct (GL.framed_step_preserves_ownership 3 [(9, 0)] graph_before (GL.ODelete GL.linkB)
    graph_before_invariant (proj1 graph_before_split)) as [Hi HN].
  eapply vacant_insert_allocation_agrees_split with (sid := 0) (idx := 1) (d := 99)
    (es := []) (bs := [(4, 0)]) (tb := (9, 0));
    [exact Hi|split; [exact HN|apply Permutation_refl]|reflexivity|reflexivity|reflexivity|reflexivity].
Qed.

Example framed_grow_refuses_another_stores_table :
  let g := fst (GL.gexec_framed 3 (Some [(9, 0)]) graph_before (GL.ONew (8, 0))) in
  GL.gexec_framed 3 (Some [(9, 0)]) g (GL.OInsert 0 3 99 [] [(8, 0)] (7, 0)) =
    (g, GL.GRefused GL.RNotFree).
Proof. reflexivity. Qed.

Example framed_grow_refuses_table_payload_overlap :
  GL.gexec_framed 3 (Some [(9, 0)]) graph_before (GL.OInsert 0 3 99 [] [(7, 0)] (7, 0)) =
    (graph_before, GL.GRefused GL.RNotFree).
Proof. reflexivity. Qed.

Example framed_grow_still_refuses_borrow :
  let g := fst (GL.gexec_framed 3 (Some [(9, 0)]) graph_before (GL.OBegin GL.linkA false)) in
  GL.gexec_framed 3 (Some [(9, 0)]) g (GL.OInsert 0 3 99 [] [(5, 0)] (7, 0)) =
    (g, GL.GRefused GL.RBorrowed).
Proof. reflexivity. Qed.

Example framed_new_store_preserves_split :
  let g' := fst (GL.gexec_framed 3 (Some [(9, 0)]) graph_before (GL.ONew (8, 0))) in
  GL.GInv 3 g' /\ HeapSplit ((8, 0) :: GL.gheap graph_before ++ [(9, 0)]) g' [(9, 0)].
Proof.
  eapply new_store_allocation_agrees_split;
    [exact graph_before_invariant|exact graph_before_split|reflexivity].
Qed.

(* Growth immediately reuses its OWN old table (5,0) for the new node.
   The table moves to (7,0); the other owner's (9,0) remains untouched. *)
Definition framed_grown_graph :=
  fst (GL.gexec_framed 3 (Some [(9, 0)]) graph_before (GL.OInsert 0 3 99 [] [(5, 0)] (7, 0))).
Definition framed_grown_heap :=
  [(5, 0)] ++ (7, 0) :: OC.free [(5, 0)] (GL.gheap graph_before ++ [(9, 0)]).
Definition framed_grown_content := OC.SNode [OC.SLeaf 10; OC.SLeaf 11; OC.SLeaf 12; OC.SLeaf 99].
Definition framed_grown_env : OC.TEnv :=
  [(OC.Src 0, (framed_grown_content, root_fp framed_grown_graph));
   (OC.Src 1, (OC.SLeaf 5, [(9, 0)]))].

(* A concrete expected table, checked by the find_store premise below. *)
Definition framed_growth_old_store := GL.mkStore 0 (5, 0)
  [GL.mkSlot 0 (Some (GL.mkNode 10 [(1, 0); (2, 0)] [(2, 0)]));
   GL.mkSlot 0 (Some (GL.mkNode 11 [(0, 0); (2, 0)] [(4, 0)]));
   GL.mkSlot 0 (Some (GL.mkNode 12 [] [(6, 0)]))].

Lemma framed_growth_split : GL.GInv 3 framed_grown_graph /\
  HeapSplit framed_grown_heap framed_grown_graph [(9, 0)].
Proof.
  apply (growth_allocation_agrees_split 3 graph_before (GL.gheap graph_before ++ [(9, 0)])
    [(9, 0)] 0 3 99 [] [(5, 0)] (7, 0) framed_growth_old_store framed_grown_graph (GL.mkLink 0 3 0));
    [exact graph_before_invariant|exact graph_before_split|reflexivity|reflexivity|reflexivity].
Qed.

Lemma framed_grown_canonical_invariant : OC.INV framed_grown_env [] framed_grown_heap 10 [].
Proof.
  unfold OC.INV. split; [simpl; repeat constructor; simpl; intuition discriminate|].
  split; [cbn; repeat constructor; simpl; intuition discriminate|].
  split.
  - change (Permutation framed_grown_heap (GL.gfp (GL.gstores framed_grown_graph) ++ [(9, 0)] ++ [])).
    rewrite app_nil_r. eapply Permutation_trans; [exact (proj2 (proj2 framed_growth_split))|].
    apply Permutation_app_tail. exact (GL.gi_perm 3 _ (proj1 framed_growth_split)).
  - split; [intros b Hb; cbn in Hb; intuition (subst; simpl; lia)|].
    split; [intros z c bs F; discriminate|].
    split; [|intros z c bs F; discriminate].
    intros z c bs Hl. unfold framed_grown_env in Hl. rewrite OC.tlookup_cons in Hl.
    destruct (OC.tvar_eq_dec (OC.Src 0) z); [injection Hl as Hc Hb; subst; reflexivity|].
    rewrite OC.tlookup_cons in Hl.
    destruct (OC.tvar_eq_dec (OC.Src 1) z); [injection Hl as Hc Hb; subst; reflexivity|simpl in Hl; discriminate].
Qed.

Example framed_growth_then_retirement_keeps_other_owner : forall slots caps tok,
  SC.HandleRelease slots caps demo_owner tok ->
  RetireReady 3 framed_grown_graph 0 framed_grown_env [] framed_grown_heap 10 []
    [(9, 0)] demo_binding slots caps tok /\
  OC.free (root_fp framed_grown_graph) framed_grown_heap = [(9, 0)] /\
  OC.tlookup (OC.Src 1) (OC.tremove (OC.Src 0) framed_grown_env) = Some (OC.SLeaf 5, [(9, 0)]).
Proof.
  intros slots caps tok Hr. split.
  - split; [exact (proj1 framed_growth_split)|]. split; [exact framed_grown_canonical_invariant|].
    split; [exact (proj2 framed_growth_split)|].
    exists demo_owner, (OC.Src 0), framed_grown_content.
    eexists. split; [reflexivity|]. split; [reflexivity|]. split; [reflexivity|].
    split; [reflexivity|exact Hr].
  - split; reflexivity.
Qed.

(* This consumes RetireReady through the existing executable texec / graph /
   Slot theorem. A readiness witness alone is not retirement evidence. *)
Theorem framed_growth_retirement_executes : forall tf tp slots caps tok,
  SC.HandleRelease slots caps demo_owner tok ->
  snd (GL.gexec_framed 3 (Some [(9, 0)]) framed_grown_graph (GL.ODrop 0)) = GL.GUnit /\
  OC.texec tf tp framed_grown_env [] framed_grown_heap 10 (OC.TDrop (OC.Src 0))
    [(OC.Src 1, (OC.SLeaf 5, [(9, 0)]))] [] [(9, 0)] 10 [] /\
  OC.INV [(OC.Src 1, (OC.SLeaf 5, [(9, 0)]))] [] [(9, 0)] 10 [] /\
  exists slot, slots 1 = Some slot /\
    SC.Step slots caps (SC.update_heap slots 1 (Some (SC.mkSlot 0 (SC.s_gen slot) SC.Unpinned false))) /\
    SC.HandleStale (SC.update_heap slots 1 (Some (SC.mkSlot 0 (SC.s_gen slot) SC.Unpinned false))) demo_owner.
Proof.
  intros tf tp slots caps tok Hr.
  destruct (framed_growth_then_retirement_keeps_other_owner slots caps tok Hr) as [Hready [Hfree _]].
  destruct (retirement_agrees_split tf tp 3 framed_grown_graph 0 framed_grown_env []
    framed_grown_heap 10 [] [(9, 0)] demo_binding slots caps tok Hready)
    as [handle [x [s [slot [Hbind [Hs [Hunit [Htex [Hi [_ [_ [_ [_ [Hslot [Hstep Hstale]]]]]]]]]]]]]]].
  unfold demo_binding in Hbind. inversion Hbind; subst handle x.
  assert (Hfp : GL.store_fp s = root_fp framed_grown_graph)
    by (unfold root_fp; rewrite Hs; reflexivity).
  rewrite Hfp, Hfree in Htex, Hi. split; [exact Hunit|].
  split; [exact Htex|]. split; [exact Hi|]. exists slot. split; [exact Hslot|].
  split; [exact Hstep|exact Hstale].
Qed.

Print Assumptions new_store_allocation_agrees_split.
Print Assumptions vacant_insert_allocation_agrees_split.
Print Assumptions growth_allocation_agrees_split.
Print Assumptions framed_growth_then_retirement_keeps_other_owner.
Print Assumptions framed_growth_retirement_executes.
Print Assumptions graph_fragment_can_take_external_storage.

Print Assumptions reuse_never_aliases_names.
Print Assumptions rooted_link_refused_after_root_reuse.
Print Assumptions rooted_link_survives_store_id_reuse.
Print Assumptions address_link_admits_reused_node.
Print Assumptions access_targets_owned_live_storage.
Print Assumptions stale_node_refused_under_live_root.
Print Assumptions retirement_agrees_split.
Print Assumptions retirement_consumes_once.
Print Assumptions retirement_with_other_values_live.
Print Assumptions reclaimed_root_refuses_old_handle.
