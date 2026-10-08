(*
  One owner, many links: a model of graph stores.

  A store owns its nodes. Links are plain values (store id, slot index,
  generation) that own nothing, so a node can be linked from any number of
  places, cycles included, without duplicating ownership. Edges stored
  inside a node are store-relative (slot index, generation).

  This supplement imports the canonical cleanup machine only for its heap
  vocabulary (blocks, [free]). It adds no instruction to that machine. The
  allocator is adversarial: every operation that needs storage takes the
  blocks to use as an argument, and any choice of blocks that are distinct
  and not live in the graph fragment is admitted, including blocks freed
  by the previous step. Whole-heap composition uses [gexec_framed] and an
  admitted external footprint, never the fragment as a fallback. Its frame
  guard excludes every actually allocated block before the transition;
  [framed_step_preserves_ownership] / [framed_run_preserves_ownership] retain
  the global split through allocation, growth and retirement. None refuses.
  Nothing below depends on storage names being fresh; physical reuse of an
  address is the case each theorem has to survive. It checks the design
  recorded in docs/audits/ownership_graph_links_design_2026-10-08.md
  against its own falsifiers:

  - [ginv_step]: every operation keeps the live heap equal to the stores'
    footprints, duplicate-free, and keeps every active borrow on a live node
    of a live store, with no write borrow overlapping another borrow.
  - [drop_releases_exactly]: dropping a store frees each of its blocks once,
    by walking its slots, whatever its links and cycles;
    [naive_edge_drop_double_frees] is the counterexample for dropping by
    following edges.
  - [reused_blocks_do_not_revive_link], [address_identity_confuses_reuse]:
    when a deleted node's exact blocks are reused for a new node, its old
    link stays refused, while an identity built from the address names the
    new node. A storage address is not a node identity.
  - [store_id_reuse_resurrects]: store ids here are never reissued.
    Reissuing a dropped store's id without a generation resurrects every
    link into it, so an implementation needs ids that are never reissued
    and fail closed on exhaustion, or links rooted in a handle that carries
    a generation.
  - [stale_step], [stale_forever], [delete_makes_stale], [drop_makes_stale]:
    once a link is stale it stays stale, whatever is later inserted in its
    slot or store. [issued_links_unique]: no two inserts ever return the
    same link; [insert_resolves]: the link an insert returns names the node
    it inserted.
  - [gens_bounded]: no generation wraps around; [wraparound_resurrects] is
    the counterexample for modular generations.
  - [drop_refused_while_borrowed], [borrow_exclusive],
    [borrow_table_live]: a borrow keeps its store and its table alive;
    [unguarded_grow_dangles] is the counterexample for growing the table
    under a borrow: with reuse, the borrow's old table address then holds
    another object's storage. Filling a free slot under a borrow is admitted
    ([free_slot_insert_under_borrow]).
  - [append_only_links_resolve]: in a store that never deletes, every link
    resolves, so no generation check is needed there.
  - [unreachable_nodes_retained], [retired_slots_grow_table]: a long-lived
    store keeps every node it has not deleted, reachable or not, and
    retired slots stay in the table. Store-end cleanup is not reclamation
    inside a live store.
  - [snapshot_preserves_relative_edges], [absolute_internal_edge_dangles],
    [relative_edge_in_wrong_store]: store-relative edges survive a
    snapshot; absolute ones do not, and a relative edge read in another
    store names an unrelated node.

  Negative scope: no async, worker or FFI escape, no finalizers with
  observable effects, no node moves between stores, and snapshot is a pure
  function, not a machine operation. Borrows are the ghost state that a
  compiler's static check must refine; no runtime borrow counter is implied.
  Generation checks are runtime checks, except in append-only stores.
*)

Require Import Stdlib.Lists.List.
Require Import Stdlib.Arith.PeanoNat.
Require Import Stdlib.Bool.Bool.
Require Import Stdlib.Sorting.Permutation.
Require Import Stdlib.micromega.Lia.
Require Import OwnershipCleanCore.
Import ListNotations.

(* ------------------------------------------------------------------ *)
(* Stores, links, borrows                                               *)
(* ------------------------------------------------------------------ *)

(* An edge inside a store: (slot index, generation). *)
Definition Edge := (nat * nat)%type.

Record Node := mkNode { ndata : nat; nedges : list Edge; nblocks : list Block }.
Record Slot := mkSlot { sgen : nat; snode : option Node }.
(* [stab] is the block of the slot table; growing the table moves it. *)
Record Store := mkStore { ssid : nat; stab : Block; sslots : list Slot }.
Record Link := mkLink { lsid : nat; lidx : nat; lgen : nat }.
Record Borrow := mkBorrow { bsid : nat; bidx : nat; bwr : bool; btab : Block }.

(* [gissued] is ghost: every link an insert has returned. *)
Record GS := mkGS {
  gstores : list Store; gheap : list Block; gsid : nat;
  gbor : list Borrow; gissued : list Link }.

Inductive Refusal : Type :=
| RNoStore | RStale | RBorrowed | RConflict | RExhausted | RNoBorrow | RBadIndex | RNotFree
| RNoFrame.

Inductive GRes : Type :=
| GLink (l : Link) | GSid (sid : nat) | GUnit | GRefused (r : Refusal).

Inductive Res : Type := Found (nd : Node) | Missing (r : Refusal).

Fixpoint find_store (sid : nat) (ss : list Store) : option Store :=
  match ss with
  | [] => None
  | s :: r => if Nat.eqb (ssid s) sid then Some s else find_store sid r
  end.

Definition set_store (s' : Store) (ss : list Store) : list Store :=
  map (fun s => if Nat.eqb (ssid s) (ssid s') then s' else s) ss.

Definition del_store (sid : nat) (ss : list Store) : list Store :=
  filter (fun s => negb (Nat.eqb (ssid s) sid)) ss.

Fixpoint upd {A : Type} (l : list A) (i : nat) (x : A) : list A :=
  match l, i with
  | [], _ => []
  | _ :: r, 0 => x :: r
  | a :: r, S j => a :: upd r j x
  end.

Fixpoint remove_nth {A : Type} (l : list A) (i : nat) : list A :=
  match l, i with
  | [], _ => []
  | _ :: r, 0 => r
  | a :: r, S j => a :: remove_nth r j
  end.

Definition node_fp (o : option Node) : list Block :=
  match o with Some nd => nblocks nd | None => [] end.
Definition slots_fp (sl : list Slot) : list Block := flat_map (fun s => node_fp (snode s)) sl.
Definition store_fp (s : Store) : list Block := stab s :: slots_fp (sslots s).
Definition gfp (ss : list Store) : list Block := flat_map store_fp ss.

(* Resolve a store-relative edge. *)
Definition follow (s : Store) (e : Edge) : option Node :=
  match nth_error (sslots s) (fst e) with
  | Some sl => if Nat.eqb (sgen sl) (snd e) then snode sl else None
  | None => None
  end.

(* Resolve a link: its store must be live and its slot must hold a node of
   its generation. Any other case is an explicit refusal. *)
Definition resolve (g : GS) (l : Link) : Res :=
  match find_store (lsid l) (gstores g) with
  | None => Missing RNoStore
  | Some s => match follow s (lidx l, lgen l) with
              | Some nd => Found nd
              | None => Missing RStale
              end
  end.

Definition same_place (sid idx : nat) (b : Borrow) : bool :=
  Nat.eqb (bsid b) sid && Nat.eqb (bidx b) idx.
Definition conflicts (wr : bool) (sid idx : nat) (bs : list Borrow) : bool :=
  existsb (fun b => same_place sid idx b && (wr || bwr b)) bs.
Definition store_borrowed (sid : nat) (bs : list Borrow) : bool :=
  existsb (fun b => Nat.eqb (bsid b) sid) bs.

(* An allocator's choice is admitted when its blocks are distinct and none
   of them is live. *)
Fixpoint bnodup (bs : list Block) : bool :=
  match bs with
  | [] => true
  | b :: r => negb (bmem b r) && bnodup r
  end.
Definition bfree (bs H : list Block) : bool := forallb (fun b => negb (bmem b H)) bs.

Inductive GOp : Type :=
| ONew (tb : Block)
| OInsert (sid idx d : nat) (es : list Edge) (bs : list Block) (tb : Block)
| ODelete (l : Link)
| OBegin (l : Link) (wr : bool)
| OEnd (i : nat)
| OWrite (i d : nat) (es : list Edge)
| ODrop (sid : nat).

Section Machine.

(* Generations run from 0 to [gmax]; a slot whose generation reaches
   [gmax] is retired and never holds a node again. *)
Variable gmax : nat.

Definition refuse (g : GS) (r : Refusal) : GS * GRes := (g, GRefused r).

(* A new store's table is the block [tb]. *)
Definition g_new (g : GS) (tb : Block) : GS * GRes :=
  if bmem tb (gheap g) then refuse g RNotFree else
  (mkGS (mkStore (gsid g) tb [] :: gstores g) (tb :: gheap g)
        (S (gsid g)) (gbor g) (gissued g), GSid (gsid g)).

(* Insert a node stored in the blocks [bs] at slot [idx]: a vacant,
   unretired slot is reused at its current generation; [idx = length]
   appends, which grows the table and moves it to the block [tb]. The old
   table is freed first, so [tb] or [bs] may reuse it. Reuse is allowed while other
   nodes are borrowed, since it neither moves the table nor touches a
   borrowed slot; growth is refused while the store is borrowed. *)
Definition g_insert (g : GS) (sid idx d : nat) (es : list Edge) (bs : list Block) (tb : Block)
    : GS * GRes :=
  match find_store sid (gstores g) with
  | None => refuse g RNoStore
  | Some s =>
      match nth_error (sslots s) idx with
      | Some sl =>
          match snode sl with
          | Some _ => refuse g RConflict
          | None =>
              if Nat.ltb (sgen sl) gmax then
                if bnodup bs && bfree bs (gheap g) then
                  let nd := mkNode d es bs in
                  let l := mkLink sid idx (sgen sl) in
                  (mkGS (set_store (mkStore sid (stab s) (upd (sslots s) idx (mkSlot (sgen sl) (Some nd))))
                                   (gstores g))
                        (bs ++ gheap g) (gsid g) (gbor g) (l :: gissued g),
                   GLink l)
                else refuse g RNotFree
              else refuse g RExhausted
          end
      | None =>
          if store_borrowed sid (gbor g) then refuse g RBorrowed else
          if Nat.eqb idx (length (sslots s)) && Nat.ltb 0 gmax then
            if bnodup (tb :: bs) && bfree (tb :: bs) (free [stab s] (gheap g)) then
              let nd := mkNode d es bs in
              let l := mkLink sid idx 0 in
              (mkGS (set_store (mkStore sid tb (sslots s ++ [mkSlot 0 (Some nd)])) (gstores g))
                    (bs ++ tb :: free [stab s] (gheap g)) (gsid g) (gbor g) (l :: gissued g),
               GLink l)
            else refuse g RNotFree
          else refuse g RBadIndex
      end
  end.

(* Delete the node a link names; its slot's generation moves on. *)
Definition g_delete (g : GS) (l : Link) : GS * GRes :=
  match find_store (lsid l) (gstores g) with
  | None => refuse g RNoStore
  | Some s =>
      match nth_error (sslots s) (lidx l) with
      | Some (mkSlot gen (Some nd)) =>
          if Nat.eqb gen (lgen l) then
            if existsb (same_place (lsid l) (lidx l)) (gbor g) then refuse g RBorrowed else
            (mkGS (set_store (mkStore (ssid s) (stab s) (upd (sslots s) (lidx l) (mkSlot (S gen) None)))
                             (gstores g))
                  (free (nblocks nd) (gheap g)) (gsid g) (gbor g) (gissued g), GUnit)
          else refuse g RStale
      | _ => refuse g RStale
      end
  end.

(* Begin a read or write borrow of the node a link names. A write borrow
   excludes every other borrow of that node; a read borrow excludes a
   write borrow. *)
Definition g_begin (g : GS) (l : Link) (wr : bool) : GS * GRes :=
  match find_store (lsid l) (gstores g) with
  | None => refuse g RNoStore
  | Some s =>
      match follow s (lidx l, lgen l) with
      | None => refuse g RStale
      | Some _ =>
          if conflicts wr (lsid l) (lidx l) (gbor g) then refuse g RConflict
          else (mkGS (gstores g) (gheap g) (gsid g)
                     (mkBorrow (lsid l) (lidx l) wr (stab s) :: gbor g) (gissued g), GUnit)
      end
  end.

Definition g_end (g : GS) (i : nat) : GS * GRes :=
  if Nat.ltb i (length (gbor g))
  then (mkGS (gstores g) (gheap g) (gsid g) (remove_nth (gbor g) i) (gissued g), GUnit)
  else refuse g RNoBorrow.

(* Write through a write borrow, in place: same blocks. *)
Definition g_write (g : GS) (i d : nat) (es : list Edge) : GS * GRes :=
  match nth_error (gbor g) i with
  | None => refuse g RNoBorrow
  | Some b =>
      if negb (bwr b) then refuse g RConflict else
      match find_store (bsid b) (gstores g) with
      | None => refuse g RNoStore
      | Some s =>
          match nth_error (sslots s) (bidx b) with
          | Some (mkSlot gen (Some nd)) =>
              (mkGS (set_store (mkStore (ssid s) (stab s)
                                 (upd (sslots s) (bidx b) (mkSlot gen (Some (mkNode d es (nblocks nd))))))
                               (gstores g))
                    (gheap g) (gsid g) (gbor g) (gissued g), GUnit)
          | _ => refuse g RStale
          end
      end
  end.

(* Drop a store: free its table and every node it holds, slot by slot. *)
Definition g_drop (g : GS) (sid : nat) : GS * GRes :=
  match find_store sid (gstores g) with
  | None => refuse g RNoStore
  | Some s =>
      if store_borrowed sid (gbor g) then refuse g RBorrowed
      else (mkGS (del_store sid (gstores g)) (free (store_fp s) (gheap g)) (gsid g)
                 (gbor g) (gissued g), GUnit)
  end.

Definition gexec (g : GS) (o : GOp) : GS * GRes :=
  match o with
  | ONew tb => g_new g tb
  | OInsert sid idx d es bs tb => g_insert g sid idx d es bs tb
  | ODelete l => g_delete g l
  | OBegin l wr => g_begin g l wr
  | OEnd i => g_end g i
  | OWrite i d es => g_write g i d es
  | ODrop sid => g_drop g sid
  end.

Fixpoint grun (g : GS) (os : list GOp) : GS :=
  match os with
  | [] => g
  | o :: r => grun (fst (gexec g o)) r
  end.

Fixpoint gresults (g : GS) (os : list GOp) : list GRes :=
  match os with
  | [] => []
  | o :: r => snd (gexec g o) :: gresults (fst (gexec g o)) r
  end.

End Machine.

Definition gempty : GS := mkGS [] [] 0 [] [].

(* gexec is a graph-fragment transition: it knows only graph-owned storage.
   Whole-heap composition must use gexec_framed with the external footprint
   admitted by the canonical HeapSplit. None is missing evidence, not [].
   Vacant-slot insertion does not allocate tb; only growth checks it. *)
Definition allocation_blocks (g : GS) (o : GOp) : list Block :=
  match o with
  | ONew tb => [tb]
  | OInsert sid idx _ _ bs tb =>
      match find_store sid (gstores g) with
      | None => []
      | Some s => match nth_error (sslots s) idx with
                  | Some _ => bs
                  | None => tb :: bs
                  end
      end
  | _ => []
  end.

Definition gexec_framed (gmax : nat) (frame : option (list Block)) (g : GS) (o : GOp)
    : GS * GRes :=
  match frame with
  | None => refuse g RNoFrame
  | Some Ho => if bfree (allocation_blocks g o) Ho
               then gexec gmax g o else refuse g RNotFree
  end.

Fixpoint grun_framed (gmax : nat) (frame : option (list Block)) (g : GS) (os : list GOp) : GS :=
  match os with
  | [] => g
  | o :: r => grun_framed gmax frame (fst (gexec_framed gmax frame g o)) r
  end.

(* ------------------------------------------------------------------ *)
(* Executable witnesses                                                 *)
(* ------------------------------------------------------------------ *)

Definition strip (r : Res) : option (nat * list Edge) :=
  match r with Found nd => Some (ndata nd, nedges nd) | Missing _ => None end.

(* A document owns A, B, C; links A <-> B, A -> C, B -> C. *)
Definition doc_build : list GOp :=
  [ONew (0, 0);
   OInsert 0 0 10 [(1, 0); (2, 0)] [(2, 0)] (1, 0);
   OInsert 0 1 11 [(0, 0); (2, 0)] [(4, 0)] (3, 0);
   OInsert 0 2 12 [] [(6, 0)] (5, 0)].

Definition linkA := mkLink 0 0 0.
Definition linkB := mkLink 0 1 0.
Definition linkC := mkLink 0 2 0.

Example doc_links_resolve :
  let g := grun 3 gempty doc_build in
  strip (resolve g linkA) = Some (10, [(1, 0); (2, 0)]) /\
  strip (resolve g linkB) = Some (11, [(0, 0); (2, 0)]) /\
  strip (resolve g linkC) = Some (12, []) /\
  length (gheap g) = 4.
Proof. repeat split; reflexivity. Qed.

(* Dropping the document frees its four blocks once each and leaves an
   empty heap, though its links form a cycle. *)
Example cycle_store_drop_frees_each_once :
  let g := grun 3 gempty doc_build in
  match find_store 0 (gstores g) with
  | Some s => store_fp s = [(5, 0); (2, 0); (4, 0); (6, 0)]
  | None => False
  end /\
  gheap (grun 3 g [ODrop 0]) = [] /\
  gresults 3 g [ODrop 0] = [GUnit] /\
  resolve (grun 3 g [ODrop 0]) linkA = Missing RNoStore.
Proof. repeat split; reflexivity. Qed.

(* Dropping by following edges instead frees A twice on the A <-> B cycle. *)
Fixpoint edge_drop (fuel : nat) (s : Store) (e : Edge) : list Block :=
  match fuel with
  | 0 => []
  | S f => match follow s e with
           | None => []
           | Some nd => nblocks nd ++ flat_map (edge_drop f s) (nedges nd)
           end
  end.

Example naive_edge_drop_double_frees :
  match find_store 0 (gstores (grun 3 gempty doc_build)) with
  | Some s => ~ NoDup (edge_drop 3 s (0, 0))
  | None => False
  end.
Proof.
  simpl. intros HN. inversion HN as [|a l Ha _]; subst. apply Ha. simpl. tauto.
Qed.

(* Delete B and store D in B's slot and in B's exact block (4, 0): the
   allocator reuses the address at once. B's old link stays refused and does
   not reach D; D gets the next generation. *)
Definition reuse_ops : list GOp := [ODelete linkB; OInsert 0 1 13 [] [(4, 0)] (9, 0)].

Example reused_blocks_do_not_revive_link :
  let g := grun 3 gempty (doc_build ++ reuse_ops) in
  resolve g linkB = Missing RStale /\
  strip (resolve g (mkLink 0 1 1)) = Some (13, []) /\
  gresults 3 (grun 3 gempty doc_build) reuse_ops = [GUnit; GLink (mkLink 0 1 1)] /\
  match resolve g (mkLink 0 1 1) with Found nd => nblocks nd = [(4, 0)] | Missing _ => False end.
Proof. repeat split; reflexivity. Qed.

(* The same reuse into another slot: B's link stays refused there too. *)
Example reused_blocks_in_another_slot :
  let g := grun 3 gempty (doc_build ++ [ODelete linkB; OInsert 0 3 13 [] [(4, 0)] (9, 0)]) in
  resolve g linkB = Missing RStale /\
  strip (resolve g (mkLink 0 3 0)) = Some (13, []).
Proof. split; reflexivity. Qed.

(* Counterexample: identity by address. The node holding a block is B
   before the reuse and D after it, so a link that remembers the address
   (or the block) names D, a different node, with no failure. *)
Fixpoint holder_in (ss : list Slot) (b : Block) : option Node :=
  match ss with
  | [] => None
  | sl :: r => match snode sl with
               | Some nd => if bmem b (nblocks nd) then Some nd else holder_in r b
               | None => holder_in r b
               end
  end.

Definition address_holder (g : GS) (sid : nat) (b : Block) : option nat :=
  match find_store sid (gstores g) with
  | Some s => option_map ndata (holder_in (sslots s) b)
  | None => None
  end.

Example address_identity_confuses_reuse :
  address_holder (grun 3 gempty doc_build) 0 (4, 0) = Some 11 /\
  address_holder (grun 3 gempty (doc_build ++ reuse_ops)) 0 (4, 0) = Some 13 /\
  resolve (grun 3 gempty (doc_build ++ reuse_ops)) linkB = Missing RStale.
Proof. repeat split; reflexivity. Qed.

(* An allocator choice that overlaps live storage is refused. *)
Example live_block_choice_refused :
  let g := grun 3 gempty doc_build in
  snd (gexec 3 g (OInsert 0 3 13 [] [(4, 0)] (9, 0))) = GRefused RNotFree /\
  snd (gexec 3 g (OInsert 0 3 13 [] [(9, 0); (9, 0)] (8, 0))) = GRefused RNotFree /\
  snd (gexec 3 g (OInsert 0 3 13 [] [(9, 0)] (9, 0))) = GRefused RNotFree /\
  snd (gexec 3 g (ONew (2, 0))) = GRefused RNotFree.
Proof. repeat split; reflexivity. Qed.

(* A's edge to B is store-relative: after the reuse it is stale too. *)
Example edge_to_deleted_node_is_stale :
  let g := grun 3 gempty (doc_build ++ reuse_ops) in
  match find_store 0 (gstores g) with
  | Some s => follow s (1, 0) = None
  | None => False
  end.
Proof. reflexivity. Qed.

(* With gmax = 2 a slot is retired after two deletes: the next insert
   there is refused and has to append, so the table grows. Every node here
   reuses the block (2, 0). *)
Example retired_slots_grow_table :
  let ops := [ONew (0, 0); OInsert 0 0 1 [] [(2, 0)] (1, 0); ODelete (mkLink 0 0 0);
              OInsert 0 0 2 [] [(2, 0)] (1, 0); ODelete (mkLink 0 0 1);
              OInsert 0 0 3 [] [(2, 0)] (1, 0); OInsert 0 1 3 [] [(2, 0)] (3, 0)] in
  gresults 2 gempty ops =
    [GSid 0; GLink (mkLink 0 0 0); GUnit; GLink (mkLink 0 0 1); GUnit;
     GRefused RExhausted; GLink (mkLink 0 1 0)] /\
  match find_store 0 (gstores (grun 2 gempty ops)) with
  | Some s => length (sslots s) = 2
  | None => False
  end.
Proof. split; reflexivity. Qed.

(* Modular generations instead resurrect a stale link onto a new node,
   here stored at the very address of the old one. *)
Definition g_delete_wrap (gmax : nat) (g : GS) (l : Link) : GS * GRes :=
  match find_store (lsid l) (gstores g) with
  | None => refuse g RNoStore
  | Some s =>
      match nth_error (sslots s) (lidx l) with
      | Some (mkSlot gen (Some nd)) =>
          if Nat.eqb gen (lgen l) then
            (mkGS (set_store (mkStore (ssid s) (stab s)
                               (upd (sslots s) (lidx l) (mkSlot (Nat.modulo (S gen) gmax) None)))
                             (gstores g))
                  (free (nblocks nd) (gheap g)) (gsid g) (gbor g) (gissued g), GUnit)
          else refuse g RStale
      | _ => refuse g RStale
      end
  end.

Example wraparound_resurrects :
  let g0 := grun 2 gempty [ONew (0, 0); OInsert 0 0 1 [] [(2, 0)] (1, 0)] in
  let g1 := fst (g_delete_wrap 2 g0 (mkLink 0 0 0)) in
  let g2 := grun 2 g1 [OInsert 0 0 2 [] [(2, 0)] (1, 0)] in
  let g3 := fst (g_delete_wrap 2 g2 (mkLink 0 0 1)) in
  let g4 := grun 2 g3 [OInsert 0 0 3 [] [(2, 0)] (1, 0)] in
  strip (resolve g0 (mkLink 0 0 0)) = Some (1, []) /\
  strip (resolve g4 (mkLink 0 0 0)) = Some (3, []).
Proof. split; reflexivity. Qed.

(* Growing the table under a borrow moves the table the borrow points into
   and frees the old one; with reuse, the old table address then holds the
   new node. The borrow would read another object's storage (ABA). *)
Definition g_insert_unguarded (gmax : nat) (g : GS) (sid idx d : nat) (es : list Edge)
    (bs : list Block) (tb : Block) : GS * GRes :=
  g_insert gmax (mkGS (gstores g) (gheap g) (gsid g) [] (gissued g)) sid idx d es bs tb.

Example unguarded_grow_dangles :
  let g0 := grun 3 gempty [ONew (0, 0); OInsert 0 0 1 [] [(2, 0)] (1, 0); OBegin (mkLink 0 0 0) false] in
  let g1 := fst (g_insert_unguarded 3 g0 0 1 2 [] [(1, 0)] (5, 0)) in
  snd (gexec 3 g0 (OInsert 0 1 2 [] [(1, 0)] (5, 0))) = GRefused RBorrowed /\
  map btab (gbor g0) = [(1, 0)] /\
  In (1, 0) (gheap g1) /\
  match find_store 0 (gstores g1) with
  | Some s => stab s = (5, 0) /\ option_map nblocks (follow s (1, 0)) = Some [(1, 0)]
  | None => False
  end.
Proof. repeat split; try reflexivity. simpl. tauto. Qed.

(* Filling a free slot is admitted while another node is borrowed;
   growing the store is not. *)
Example free_slot_insert_under_borrow :
  let g := grun 3 gempty (doc_build ++ [ODelete linkC; OBegin linkA false]) in
  snd (gexec 3 g (OInsert 0 2 14 [] [(6, 0)] (9, 0))) = GLink (mkLink 0 2 1) /\
  snd (gexec 3 g (OInsert 0 3 15 [] [(8, 0)] (9, 0))) = GRefused RBorrowed.
Proof. split; reflexivity. Qed.

(* A borrow keeps its store: dropping or deleting under it is refused. *)
Example borrowed_store_is_kept :
  let g := grun 3 gempty (doc_build ++ [OBegin linkB false]) in
  snd (gexec 3 g (ODrop 0)) = GRefused RBorrowed /\
  snd (gexec 3 g (ODelete linkB)) = GRefused RBorrowed /\
  snd (gexec 3 g (OBegin linkB true)) = GRefused RConflict /\
  snd (gexec 3 g (OBegin linkB false)) = GUnit.
Proof. repeat split; reflexivity. Qed.

(* Many links, one node: a write through one link is seen through every
   copy of it, and through the edges that name the node. *)
Example shared_write_is_seen :
  let g := grun 3 gempty (doc_build ++ [OBegin linkC true; OWrite 0 42 []; OEnd 0]) in
  strip (resolve g linkC) = Some (42, []) /\
  match find_store 0 (gstores g) with
  | Some s => option_map ndata (follow s (2, 0)) = Some 42
  | None => False
  end.
Proof. split; reflexivity. Qed.

(* A long-lived store keeps nodes no link reaches. After the program
   forgets the links of an isolated cycle, its two nodes are still live. *)
Example unreachable_nodes_retained :
  let g := grun 3 gempty [ONew (0, 0); OInsert 0 0 1 [(1, 0)] [(2, 0)] (1, 0);
                          OInsert 0 1 2 [(0, 0)] [(4, 0)] (3, 0);
                          OInsert 0 2 3 [(3, 0)] [(6, 0)] (5, 0);
                          OInsert 0 3 4 [(2, 0)] [(8, 0)] (7, 0)] in
  length (gheap g) = 5.
Proof. reflexivity. Qed.

(* Snapshot: a copy of a store with fresh blocks and a new id. *)
Definition copy_node (n j : nat) (o : option Node) : option Node :=
  match o with
  | Some nd => Some (mkNode (ndata nd) (nedges nd) (alloc (n + j) (length (nblocks nd))))
  | None => None
  end.

Fixpoint copy_slots (n j : nat) (ss : list Slot) : list Slot :=
  match ss with
  | [] => []
  | sl :: r => mkSlot (sgen sl) (copy_node n j (snode sl)) :: copy_slots n (S j) r
  end.

Definition snapshot (s : Store) (sid' n : nat) : Store :=
  mkStore sid' (n, 0) (copy_slots (S n) 0 (sslots s)).

(* An edge stored as an absolute link names the original store; read in
   the snapshot after the original is dropped, it is dangling, while the
   same edge stored relative still resolves in the snapshot. *)
Example absolute_internal_edge_dangles :
  let g := grun 3 gempty doc_build in
  match find_store 0 (gstores g) with
  | Some s =>
      let snap := snapshot s 1 7 in
      let g' := mkGS [snap] [] 2 [] [] in
      resolve g' (mkLink 0 1 0) = Missing RNoStore /\
      option_map ndata (follow snap (1, 0)) = Some 11
  | None => False
  end.
Proof. split; reflexivity. Qed.

(* A relative edge read in another store names an unrelated node. *)
Example relative_edge_in_wrong_store :
  let g := grun 3 gempty (doc_build ++ [ONew (7, 0); OInsert 1 0 99 [] [(9, 0)] (8, 0);
                                         OInsert 1 1 98 [] [(11, 0)] (10, 0)]) in
  match find_store 0 (gstores g), find_store 1 (gstores g) with
  | Some a, Some b => option_map ndata (follow a (1, 0)) = Some 11 /\
                      option_map ndata (follow b (1, 0)) = Some 98
  | _, _ => False
  end.
Proof. split; reflexivity. Qed.

(* Store identity is reused too in a real system. Here store ids come from
   a counter and are never issued twice, which an implementation can only
   match with ids that are never reissued and fail closed on exhaustion
   (as the Slot runtime does for slot ids). Reissuing a dropped store's id
   without a generation resurrects every link into it: the new store's
   first node answers the old document's link to A. Store identity needs a
   generation as well, which a rooted link gets from its Slot root
   (MemoryBoundaryCompositionAudit, [rooted_link_refused_after_root_reuse]). *)
Definition g_new_at (g : GS) (sid : nat) (tb : Block) : GS :=
  mkGS (mkStore sid tb [] :: gstores g) (tb :: gheap g) (gsid g) (gbor g) (gissued g).

Example store_id_reuse_resurrects :
  let g0 := grun 3 gempty (doc_build ++ [ODrop 0]) in
  let g1 := grun 3 (g_new_at g0 0 (8, 0)) [OInsert 0 0 99 [] [(2, 0)] (9, 0)] in
  resolve g0 linkA = Missing RNoStore /\
  strip (resolve g1 linkA) = Some (99, []).
Proof. split; reflexivity. Qed.

(* ------------------------------------------------------------------ *)
(* List lemmas                                                          *)
(* ------------------------------------------------------------------ *)

Lemma filter_keep : forall (A : Type) (f : A -> bool) l,
  (forall x, In x l -> f x = true) -> filter f l = l.
Proof.
  intros A f l. induction l as [|a r IH]; simpl; intros Hf; [reflexivity|].
  rewrite (Hf a (or_introl eq_refl)). f_equal. apply IH. intros x Hx. apply Hf. right. exact Hx.
Qed.

Lemma perm_swap_app : forall (T : Type) (a b c : list T),
  Permutation (a ++ b ++ c) (b ++ a ++ c).
Proof.
  intros. rewrite !app_assoc. apply Permutation_app_tail. apply Permutation_app_comm.
Qed.

Lemma nth_upd_same : forall (A : Type) (l : list A) i x,
  i < length l -> nth_error (upd l i x) i = Some x.
Proof.
  intros A l. induction l as [|a r IH]; intros [|i] x H; simpl in *; try lia; [reflexivity|].
  apply IH. lia.
Qed.

Lemma nth_upd_other : forall (A : Type) (l : list A) i j x,
  j <> i -> nth_error (upd l i x) j = nth_error l j.
Proof.
  intros A l. induction l as [|a r IH]; intros [|i] [|j] x H; simpl; try reflexivity; try lia.
  apply IH. lia.
Qed.

Lemma in_upd : forall (A : Type) (l : list A) i x y, In y (upd l i x) -> y = x \/ In y l.
Proof.
  intros A l. induction l as [|a r IH]; intros [|i] x y H; simpl in *; try contradiction.
  - destruct H as [H|H]; [left; symmetry; exact H| right; right; exact H].
  - destruct H as [H|H]; [right; left; exact H|].
    destruct (IH i x y H) as [E|E]; [left; exact E| right; right; exact E].
Qed.

Lemma in_remove_nth : forall (A : Type) (l : list A) i x, In x (remove_nth l i) -> In x l.
Proof.
  intros A l. induction l as [|a r IH]; intros [|i] x H; simpl in *; try contradiction.
  - right. exact H.
  - destruct H as [H|H]; [left; exact H| right; exact (IH i x H)].
Qed.

Lemma slots_fp_upd : forall sl i a b, nth_error sl i = Some a ->
  Permutation (slots_fp (upd sl i b) ++ node_fp (snode a)) (slots_fp sl ++ node_fp (snode b)).
Proof.
  unfold slots_fp. intros sl. induction sl as [|c r IH]; intros [|i] a b H; simpl in *; try discriminate.
  - injection H as H. subst c. rewrite <- !app_assoc.
    eapply Permutation_trans; [apply Permutation_app_comm|]. rewrite <- app_assoc.
    apply perm_swap_app.
  - rewrite <- !app_assoc. apply Permutation_app_head. apply IH. exact H.
Qed.

Lemma slots_fp_snoc : forall sl x, slots_fp (sl ++ [x]) = slots_fp sl ++ node_fp (snode x).
Proof. intros. unfold slots_fp. rewrite flat_map_app. simpl. rewrite app_nil_r. reflexivity. Qed.

(* ------------------------------------------------------------------ *)
(* Store-list lemmas                                                    *)
(* ------------------------------------------------------------------ *)

Lemma find_store_some : forall sid ss s, find_store sid ss = Some s -> In s ss /\ ssid s = sid.
Proof.
  intros sid ss s. induction ss as [|a r IH]; simpl; [discriminate|].
  destruct (Nat.eqb (ssid a) sid) eqn:E; intros H.
  - injection H as H. subst a. split; [left; reflexivity| apply Nat.eqb_eq; exact E].
  - destruct (IH H) as [Hi Hs]. split; [right; exact Hi| exact Hs].
Qed.

Lemma find_set_same : forall s' ss s,
  find_store (ssid s') ss = Some s -> find_store (ssid s') (set_store s' ss) = Some s'.
Proof.
  intros s' ss s. unfold set_store. induction ss as [|a r IH]; simpl; [discriminate|].
  destruct (Nat.eqb (ssid a) (ssid s')) eqn:E; intros H; simpl.
  - rewrite Nat.eqb_refl. reflexivity.
  - rewrite E. apply IH. exact H.
Qed.

Lemma find_set_other : forall s' ss sid,
  sid <> ssid s' -> find_store sid (set_store s' ss) = find_store sid ss.
Proof.
  intros s' ss sid Hne. unfold set_store. induction ss as [|a r IH]; simpl; [reflexivity|].
  destruct (Nat.eqb (ssid a) (ssid s')) eqn:E; simpl.
  - apply Nat.eqb_eq in E.
    destruct (Nat.eqb (ssid s') sid) eqn:E1; [apply Nat.eqb_eq in E1; congruence|].
    destruct (Nat.eqb (ssid a) sid) eqn:E2; [apply Nat.eqb_eq in E2; congruence| exact IH].
  - destruct (Nat.eqb (ssid a) sid); [reflexivity| exact IH].
Qed.

Lemma map_ssid_set : forall s' ss, map ssid (set_store s' ss) = map ssid ss.
Proof.
  intros s' ss. unfold set_store. induction ss as [|a r IH]; simpl; [reflexivity|].
  destruct (Nat.eqb (ssid a) (ssid s')) eqn:E; simpl; rewrite IH; [|reflexivity].
  apply Nat.eqb_eq in E. rewrite E. reflexivity.
Qed.

Lemma in_set : forall s' ss x, In x (set_store s' ss) -> x = s' \/ In x ss.
Proof.
  intros s' ss x H. unfold set_store in H. apply in_map_iff in H. destruct H as [y [E Hy]].
  destruct (Nat.eqb (ssid y) (ssid s')); [left; symmetry; exact E| right; subst; exact Hy].
Qed.

Lemma find_del_same : forall sid ss, find_store sid (del_store sid ss) = None.
Proof.
  intros sid ss. unfold del_store. induction ss as [|a r IH]; simpl; [reflexivity|].
  destruct (Nat.eqb (ssid a) sid) eqn:E; simpl; [exact IH|]. rewrite E. exact IH.
Qed.

Lemma find_del_other : forall sid sid' ss,
  sid' <> sid -> find_store sid' (del_store sid ss) = find_store sid' ss.
Proof.
  intros sid sid' ss Hne. unfold del_store. induction ss as [|a r IH]; simpl; [reflexivity|].
  destruct (Nat.eqb (ssid a) sid) eqn:E; simpl.
  - apply Nat.eqb_eq in E.
    destruct (Nat.eqb (ssid a) sid') eqn:E2; [apply Nat.eqb_eq in E2; congruence| exact IH].
  - destruct (Nat.eqb (ssid a) sid'); [reflexivity| exact IH].
Qed.

Lemma in_del : forall sid ss x, In x (del_store sid ss) -> In x ss.
Proof. intros sid ss x H. unfold del_store in H. apply filter_In in H. apply H. Qed.

Lemma nodup_del : forall sid ss, NoDup (map ssid ss) -> NoDup (map ssid (del_store sid ss)).
Proof.
  intros sid ss. unfold del_store. induction ss as [|a r IH]; simpl; intros HN; [constructor|].
  inversion HN as [|x l Hx HN']; subst.
  destruct (negb (Nat.eqb (ssid a) sid)); simpl; [|apply IH; exact HN'].
  constructor; [|apply IH; exact HN']. intros Hin. apply Hx.
  apply in_map_iff in Hin. destruct Hin as [y [Ey Hy]]. apply filter_In in Hy.
  rewrite <- Ey. apply in_map. apply Hy.
Qed.

Lemma del_set_same : forall s' ss, del_store (ssid s') (set_store s' ss) = del_store (ssid s') ss.
Proof.
  intros s' ss. unfold del_store, set_store. induction ss as [|a r IH]; simpl; [reflexivity|].
  destruct (Nat.eqb (ssid a) (ssid s')) eqn:E; simpl; rewrite ?Nat.eqb_refl, ?E; simpl;
    [exact IH| f_equal; exact IH].
Qed.

Lemma gfp_del_perm : forall sid ss s, NoDup (map ssid ss) -> find_store sid ss = Some s ->
  Permutation (gfp ss) (store_fp s ++ gfp (del_store sid ss)).
Proof.
  intros sid ss s. unfold del_store, gfp.
  induction ss as [|a r IH]; [simpl; intros _ H; discriminate H|].
  intros HN Hf. inversion HN as [|x l Hx HN']; subst.
  cbn [find_store] in Hf. cbn [filter flat_map].
  destruct (Nat.eqb (ssid a) sid) eqn:E; cbn [negb flat_map].
  - injection Hf as Hf. subst a. rewrite filter_keep; [apply Permutation_refl|].
    intros y Hy. apply negb_true_iff. apply Nat.eqb_neq. intros Ey. apply Hx.
    apply Nat.eqb_eq in E. rewrite E, <- Ey. apply in_map. exact Hy.
  - eapply Permutation_trans; [apply Permutation_app_head; apply IH; [exact HN'| exact Hf]|].
    apply perm_swap_app.
Qed.

Lemma gfp_set_perm : forall s' ss s, NoDup (map ssid ss) -> find_store (ssid s') ss = Some s ->
  Permutation (gfp (set_store s' ss)) (store_fp s' ++ gfp (del_store (ssid s') ss)).
Proof.
  intros s' ss s HN Ef. rewrite <- del_set_same. apply gfp_del_perm.
  - rewrite map_ssid_set. exact HN.
  - eapply find_set_same. exact Ef.
Qed.

(* ------------------------------------------------------------------ *)
(* Borrow lemmas                                                        *)
(* ------------------------------------------------------------------ *)

Fixpoint NoConflict (bs : list Borrow) : Prop :=
  match bs with
  | [] => True
  | b :: r => (forall b', In b' r -> same_place (bsid b) (bidx b) b' = true ->
                bwr b = false /\ bwr b' = false) /\ NoConflict r
  end.

Lemma noconflict_remove : forall bs i, NoConflict bs -> NoConflict (remove_nth bs i).
Proof.
  induction bs as [|b r IH]; intros [|i] H; simpl in *; try exact I; [exact (proj2 H)|].
  destruct H as [Hb Hr]. split; [|apply IH; exact Hr].
  intros b' Hin. apply Hb. eapply in_remove_nth. exact Hin.
Qed.

Lemma noconflict_pair : forall bs i j b1 b2, NoConflict bs -> i <> j ->
  nth_error bs i = Some b1 -> nth_error bs j = Some b2 ->
  bsid b1 = bsid b2 -> bidx b1 = bidx b2 -> bwr b1 = false /\ bwr b2 = false.
Proof.
  induction bs as [|b r IH]; intros [|i] [|j] b1 b2 H Hij E1 E2 Hs Hx; simpl in *;
    try discriminate; try lia.
  - injection E1 as E1. subst b1. destruct H as [Hb _].
    apply Hb; [eapply nth_error_In; exact E2|].
    unfold same_place. rewrite Hs, Hx, !Nat.eqb_refl. reflexivity.
  - injection E2 as E2. subst b2. destruct H as [Hb _].
    assert (Hp : same_place (bsid b) (bidx b) b1 = true)
      by (unfold same_place; rewrite Hs, Hx, !Nat.eqb_refl; reflexivity).
    destruct (Hb b1 (nth_error_In _ _ E1) Hp) as [X Y]. split; assumption.
  - apply (IH i j b1 b2 (proj2 H)); try assumption. lia.
Qed.

Lemma follow_some : forall s e nd, follow s e = Some nd ->
  exists sl, nth_error (sslots s) (fst e) = Some sl /\ sgen sl = snd e /\ snode sl = Some nd.
Proof.
  intros s e nd. unfold follow. destruct (nth_error (sslots s) (fst e)) as [sl|]; [|discriminate].
  destruct (Nat.eqb (sgen sl) (snd e)) eqn:E; [|discriminate]. intros H.
  exists sl. split; [reflexivity| split; [apply Nat.eqb_eq; exact E| exact H]].
Qed.

Theorem resolve_found_spec : forall g l nd,
  resolve g l = Found nd <->
  exists s sl, find_store (lsid l) (gstores g) = Some s /\ nth_error (sslots s) (lidx l) = Some sl /\
    sgen sl = lgen l /\ snode sl = Some nd.
Proof.
  intros g l nd. unfold resolve. split.
  - destruct (find_store (lsid l) (gstores g)) as [s|]; [|discriminate].
    destruct (follow s (lidx l, lgen l)) as [nd'|] eqn:Ef; [|discriminate]. intros H.
    injection H as H. subst nd'. destruct (follow_some _ _ _ Ef) as [sl [E1 [E2 E3]]].
    exists s, sl. auto.
  - intros [s [sl [Ef [En [Eg Eo]]]]]. rewrite Ef. unfold follow. simpl. rewrite En, Eg, Nat.eqb_refl, Eo.
    reflexivity.
Qed.

(* ------------------------------------------------------------------ *)
(* The machine invariant                                                *)
(* ------------------------------------------------------------------ *)

Section Invariant.
Variable gmax : nat.

(* Every issued link names a slot of its live store whose generation has
   moved past it, or that still holds its node. *)
Definition IssuedAt (ss : list Store) (l : Link) : Prop :=
  forall s, find_store (lsid l) ss = Some s ->
    exists sl, nth_error (sslots s) (lidx l) = Some sl /\
      (lgen l < sgen sl \/ (lgen l = sgen sl /\ snode sl <> None)).

Definition BorrowAt (ss : list Store) (b : Borrow) : Prop :=
  exists s sl, find_store (bsid b) ss = Some s /\ btab b = stab s /\
    nth_error (sslots s) (bidx b) = Some sl /\ snode sl <> None.

Record GInv (g : GS) : Prop := {
  gi_nodup : NoDup (gheap g);
  gi_perm : Permutation (gheap g) (gfp (gstores g));
  gi_sids : NoDup (map ssid (gstores g));
  gi_sid_lt : forall s, In s (gstores g) -> ssid s < gsid g;
  gi_gens : forall s sl, In s (gstores g) -> In sl (sslots s) ->
              sgen sl <= gmax /\ (snode sl <> None -> sgen sl < gmax);
  gi_bor : forall b, In b (gbor g) -> BorrowAt (gstores g) b;
  gi_noconf : NoConflict (gbor g);
  gi_iss_nodup : NoDup (gissued g);
  gi_iss : forall l, In l (gissued g) -> lsid l < gsid g /\ IssuedAt (gstores g) l
}.

Lemma issued_at_set : forall ss s s' l,
  find_store (ssid s') ss = Some s -> IssuedAt ss l ->
  (lsid l = ssid s' -> forall sl, nth_error (sslots s) (lidx l) = Some sl ->
     (lgen l < sgen sl \/ (lgen l = sgen sl /\ snode sl <> None)) ->
     exists sl', nth_error (sslots s') (lidx l) = Some sl' /\
       (lgen l < sgen sl' \/ (lgen l = sgen sl' /\ snode sl' <> None))) ->
  IssuedAt (set_store s' ss) l.
Proof.
  intros ss s s' l Ef Hat Hc s0 E0.
  destruct (Nat.eq_dec (lsid l) (ssid s')) as [Eq|Ne].
  - rewrite Eq, (find_set_same _ _ _ Ef) in E0. injection E0 as E0. subst s0.
    rewrite <- Eq in Ef. destruct (Hat s Ef) as [sl [En Hsl]]. exact (Hc Eq sl En Hsl).
  - rewrite (find_set_other _ _ _ Ne) in E0. exact (Hat s0 E0).
Qed.

Lemma borrow_at_set : forall ss s s' b,
  find_store (ssid s') ss = Some s -> BorrowAt ss b ->
  (bsid b = ssid s' -> stab s' = stab s /\ forall sl, nth_error (sslots s) (bidx b) = Some sl ->
     snode sl <> None -> exists sl', nth_error (sslots s') (bidx b) = Some sl' /\ snode sl' <> None) ->
  BorrowAt (set_store s' ss) b.
Proof.
  intros ss s s' b Ef [s0 [sl [E0 [Et [En Ho]]]]] Hc.
  destruct (Nat.eq_dec (bsid b) (ssid s')) as [Eq|Ne].
  - rewrite Eq in E0. rewrite E0 in Ef. injection Ef as Ef. subst s0.
    destruct (Hc Eq) as [Hst Hsl]. destruct (Hsl sl En Ho) as [sl' [En' Ho']].
    exists s', sl'. rewrite Eq. split; [eapply find_set_same; exact E0|].
    split; [rewrite Hst; exact Et| split; assumption].
  - exists s0, sl. rewrite (find_set_other _ _ _ Ne). auto.
Qed.

Lemma gens_set : forall ss s' (P : Slot -> Prop),
  (forall x sl, In x ss -> In sl (sslots x) -> P sl) -> (forall sl, In sl (sslots s') -> P sl) ->
  forall x sl, In x (set_store s' ss) -> In sl (sslots x) -> P sl.
Proof.
  intros ss s' P Hold Hnew x sl Hx Hsl. destruct (in_set _ _ _ Hx) as [E|E].
  - subst x. exact (Hnew sl Hsl).
  - exact (Hold x sl E Hsl).
Qed.

Lemma ginv_empty : GInv gempty.
Proof.
  constructor; simpl; try (intros; contradiction); try constructor; apply Permutation_refl.
Qed.

Lemma bnodup_spec : forall bs, bnodup bs = true -> NoDup bs.
Proof.
  induction bs as [|b r IH]; simpl; intros H; [constructor|].
  apply andb_true_iff in H. destruct H as [H1 H2]. constructor; [|exact (IH H2)].
  intros Hin. apply negb_true_iff in H1. apply bmem_true in Hin. congruence.
Qed.

Lemma bfree_spec : forall bs H, bfree bs H = true -> forall b, In b bs -> ~ In b H.
Proof.
  intros bs H Hf b Hb Hh. unfold bfree in Hf. rewrite forallb_forall in Hf.
  specialize (Hf b Hb). apply negb_true_iff in Hf. apply bmem_true in Hh. congruence.
Qed.

Lemma ginv_new : forall g tb, GInv g -> GInv (fst (g_new g tb)).
Proof.
  intros g tb Hinv. unfold g_new. destruct (bmem tb (gheap g)) eqn:Hm; [exact Hinv|].
  destruct Hinv as [HN HP HS HL HG HB HC HIN HI]. simpl.
  assert (Hfr : ~ In tb (gheap g)) by (intros Hin; apply bmem_true in Hin; congruence).
  constructor; simpl.
  - constructor; assumption.
  - unfold gfp. simpl. apply perm_skip. exact HP.
  - constructor; [|exact HS]. intros Hin. apply in_map_iff in Hin. destruct Hin as [s [Es Hs]].
    specialize (HL s Hs). lia.
  - intros s [Hs|Hs]; [subst s; simpl; lia| specialize (HL s Hs); lia].
  - intros s sl [Hs|Hs] Hsl; [subst s; simpl in Hsl; contradiction| exact (HG s sl Hs Hsl)].
  - intros b Hb. destruct (HB b Hb) as [s [sl [Ef [Et [En Ho]]]]].
    destruct (find_store_some _ _ _ Ef) as [Hs Hid].
    exists s, sl. split; [|split; [exact Et| split; assumption]].
    simpl. assert (Hne : Nat.eqb (gsid g) (bsid b) = false)
      by (apply Nat.eqb_neq; specialize (HL s Hs); lia).
    rewrite Hne. exact Ef.
  - exact HC.
  - exact HIN.
  - intros l Hl. destruct (HI l Hl) as [Hlt Hat]. split; [lia|].
    intros s Ef. simpl in Ef.
    assert (Hne : Nat.eqb (gsid g) (lsid l) = false) by (apply Nat.eqb_neq; lia).
    rewrite Hne in Ef. exact (Hat s Ef).
Qed.

Lemma not_borrowed : forall sid bs, store_borrowed sid bs = false ->
  forall b, In b bs -> bsid b <> sid.
Proof.
  intros sid bs Hb b Hin E. unfold store_borrowed in Hb.
  assert (existsb (fun b0 => Nat.eqb (bsid b0) sid) bs = true)
    by (apply existsb_exists; exists b; split; [exact Hin| apply Nat.eqb_eq; exact E]).
  congruence.
Qed.

Lemma ginv_insert : forall g sid idx d es bs tb,
  GInv g -> GInv (fst (g_insert gmax g sid idx d es bs tb)).
Proof.
  intros g sid idx d es bs tb Hinv. unfold g_insert.
  destruct (find_store sid (gstores g)) as [s|] eqn:Ef; [|exact Hinv].
  pose proof Hinv as [HN HP HS HL HG HB HC HIN HI].
  destruct (find_store_some _ _ _ Ef) as [Hs Hsid].
  assert (Hdel : Permutation (gfp (gstores g)) (store_fp s ++ gfp (del_store sid (gstores g))))
    by (apply gfp_del_perm; assumption).
  destruct (nth_error (sslots s) idx) as [sl|] eqn:En.
  - destruct (snode sl) as [nd0|] eqn:Eo; [exact Hinv|].
    destruct (Nat.ltb (sgen sl) gmax) eqn:Hlt; [|exact Hinv].
    destruct (bnodup bs && bfree bs (gheap g)) eqn:Hok; [|exact Hinv].
    apply andb_true_iff in Hok. destruct Hok as [Hnd Hfree].
    apply Nat.ltb_lt in Hlt. simpl.
    set (A := bs).
    set (s' := mkStore sid (stab s) (upd (sslots s) idx (mkSlot (sgen sl) (Some (mkNode d es A))))).
    assert (Ef' : find_store (ssid s') (gstores g) = Some s) by exact Ef.
    assert (Hidx : idx < length (sslots s)) by (apply nth_error_Some; congruence).
    assert (Hset : Permutation (gfp (set_store s' (gstores g))) (A ++ gfp (gstores g))).
    { eapply Permutation_trans; [apply (gfp_set_perm _ _ _ HS Ef')|]. simpl ssid.
      eapply Permutation_trans; [|apply Permutation_app_head; apply Permutation_sym; exact Hdel].
      rewrite app_assoc. apply Permutation_app_tail. unfold store_fp. simpl.
      eapply Permutation_trans; [|apply Permutation_middle]. apply perm_skip.
      eapply Permutation_trans; [|apply Permutation_app_comm].
      pose proof (slots_fp_upd (sslots s) idx sl (mkSlot (sgen sl) (Some (mkNode d es A))) En) as Hu.
      rewrite Eo in Hu. simpl in Hu. rewrite app_nil_r in Hu. exact Hu. }
    constructor; simpl.
    + apply NoDup_app_join; [exact (bnodup_spec _ Hnd)| exact HN| exact (bfree_spec _ _ Hfree)].
    + eapply Permutation_trans; [apply Permutation_app_head; exact HP|]. apply Permutation_sym. exact Hset.
    + rewrite map_ssid_set. exact HS.
    + intros x Hx. destruct (in_set _ _ _ Hx) as [E|E]; [subst x; simpl; rewrite <- Hsid; exact (HL s Hs)|].
      exact (HL x E).
    + apply gens_set; [exact HG|]. intros y Hy. simpl in Hy. destruct (in_upd _ _ _ _ _ Hy) as [E|E].
      * subst y. simpl. split; [lia| intros _; exact Hlt].
      * exact (HG s y Hs E).
    + intros b Hin. apply (borrow_at_set _ s); [exact Ef'| exact (HB b Hin)|].
      intros _. split; [reflexivity|]. intros sl0 En0 Ho0. simpl.
      destruct (Nat.eq_dec (bidx b) idx) as [Ei|Ei].
      * exfalso. rewrite Ei, En in En0. injection En0 as En0. subst sl0.
        rewrite Eo in Ho0. apply Ho0. reflexivity.
      * exists sl0. rewrite (nth_upd_other _ _ _ _ _ Ei). split; assumption.
    + exact HC.
    + constructor; [|exact HIN]. intros Hin. destruct (HI _ Hin) as [_ Hat].
      destruct (Hat s Ef) as [sl' [En' Hc]]. simpl in En'. rewrite En in En'.
      injection En' as En'. subst sl'. simpl in Hc. destruct Hc as [Hc|[_ Hc]]; [lia| contradiction].
    + intros l0 [E0|Hl0].
      * subst l0. simpl. split; [rewrite <- Hsid; exact (HL s Hs)|].
        intros s0 E0.
        change (find_store (ssid s') (set_store s' (gstores g)) = Some s0) in E0.
        rewrite (find_set_same _ _ _ Ef') in E0. injection E0 as E0. subst s0.
        exists (mkSlot (sgen sl) (Some (mkNode d es A))). simpl.
        split; [apply nth_upd_same; exact Hidx| right; split; [reflexivity| discriminate]].
      * destruct (HI l0 Hl0) as [Hlt0 Hat]. split; [exact Hlt0|].
        apply (issued_at_set _ s); [exact Ef'| exact Hat|].
        intros _ sl0 En0 Hc0. simpl.
        destruct (Nat.eq_dec (lidx l0) idx) as [Ei|Ei].
        -- rewrite Ei in En0 |- *. rewrite En in En0. injection En0 as En0. subst sl0.
           exists (mkSlot (sgen sl) (Some (mkNode d es A))).
           split; [apply nth_upd_same; exact Hidx|]. simpl.
           destruct Hc0 as [Hc0|[_ Hc0]]; [left; exact Hc0| contradiction].
        -- exists sl0. rewrite (nth_upd_other _ _ _ _ _ Ei). split; [exact En0| exact Hc0].
  - destruct (store_borrowed sid (gbor g)) eqn:Hb; [exact Hinv|].
    pose proof (not_borrowed _ _ Hb) as Hnb.
    destruct (Nat.eqb idx (length (sslots s)) && Nat.ltb 0 gmax) eqn:Hap; [|exact Hinv].
    destruct (bnodup (tb :: bs) && bfree (tb :: bs) (free [stab s] (gheap g))) eqn:Hok; [|exact Hinv].
    apply andb_true_iff in Hap. destruct Hap as [Hidx Hpos].
    apply Nat.eqb_eq in Hidx. apply Nat.ltb_lt in Hpos.
    apply andb_true_iff in Hok. destruct Hok as [Hnd Hfreeb].
    pose proof (bnodup_spec _ Hnd) as Hnd'. apply NoDup_cons_iff in Hnd'. destruct Hnd' as [Htb Hbs].
    pose proof (bfree_spec _ _ Hfreeb) as Hfb. simpl.
    set (A := bs).
    set (s' := mkStore sid tb (sslots s ++ [mkSlot 0 (Some (mkNode d es A))])).
    assert (Ef' : find_store (ssid s') (gstores g) = Some s) by exact Ef.
    set (D := gfp (del_store sid (gstores g))).
    assert (HP2 : Permutation (gheap g) ([stab s] ++ (slots_fp (sslots s) ++ D)))
      by (eapply Permutation_trans; [exact HP| exact Hdel]).
    assert (Hfree : Permutation (free [stab s] (gheap g)) (slots_fp (sslots s) ++ D))
      by (apply free_perm; [eapply Permutation_NoDup; [exact HP2| exact HN]| exact HP2]).
    assert (Hfsub : forall b, In b (free [stab s] (gheap g)) -> In b (gheap g))
      by (intros b Hin; unfold free in Hin; apply filter_In in Hin; apply Hin).
    assert (Hfr : ~ In tb (free [stab s] (gheap g))) by exact (Hfb tb (or_introl eq_refl)).
    constructor; simpl.
    + apply NoDup_app_join; [exact Hbs| |].
      * constructor; [exact Hfr|]. unfold free. apply NoDup_filter'. exact HN.
      * intros b Ha Hh. destruct Hh as [Hh|Hh]; [subst b; contradiction|].
        exact (Hfb b (or_intror Ha) Hh).
    + eapply Permutation_trans; [apply Permutation_app_head; apply perm_skip; exact Hfree|].
      apply Permutation_sym. eapply Permutation_trans; [apply (gfp_set_perm _ _ _ HS Ef')|].
      simpl ssid. fold D. unfold store_fp. simpl. rewrite slots_fp_snoc. simpl.
      eapply Permutation_trans; [|apply Permutation_middle]. apply perm_skip.
      rewrite <- app_assoc. apply perm_swap_app.
    + rewrite map_ssid_set. exact HS.
    + intros x Hx. destruct (in_set _ _ _ Hx) as [E|E]; [subst x; simpl; rewrite <- Hsid; exact (HL s Hs)|].
      exact (HL x E).
    + apply gens_set; [exact HG|]. intros y Hy. simpl in Hy. apply in_app_or in Hy.
      destruct Hy as [Hy|[Hy|[]]]; [exact (HG s y Hs Hy)|].
      subst y. simpl. split; [lia| intros _; exact Hpos].
    + intros b Hin. apply (borrow_at_set _ s); [exact Ef'| exact (HB b Hin)|].
      intros E. simpl in E. exfalso. exact (Hnb b Hin E).
    + exact HC.
    + constructor; [|exact HIN]. intros Hin. destruct (HI _ Hin) as [_ Hat].
      destruct (Hat s Ef) as [sl' [En' _]]. simpl in En'. rewrite En in En'. discriminate.
    + intros l0 [E0|Hl0].
      * subst l0. simpl. split; [rewrite <- Hsid; exact (HL s Hs)|].
        intros s0 E0. change (find_store (ssid s') (set_store s' (gstores g)) = Some s0) in E0.
        rewrite (find_set_same _ _ _ Ef') in E0. injection E0 as E0. subst s0.
        exists (mkSlot 0 (Some (mkNode d es A))). simpl.
        rewrite Hidx, nth_error_app2, Nat.sub_diag by lia. simpl.
        split; [reflexivity| right; split; [reflexivity| discriminate]].
      * destruct (HI l0 Hl0) as [Hlt0 Hat]. split; [exact Hlt0|].
        apply (issued_at_set _ s); [exact Ef'| exact Hat|].
        intros _ sl0 En0 Hc0. exists sl0. simpl.
        rewrite nth_error_app1 by (apply nth_error_Some; congruence). split; [exact En0| exact Hc0].
Qed.

Lemma ginv_delete : forall g l, GInv g -> GInv (fst (g_delete g l)).
Proof.
  intros g l Hinv. unfold g_delete.
  destruct (find_store (lsid l) (gstores g)) as [s|] eqn:Ef; [|exact Hinv].
  destruct (nth_error (sslots s) (lidx l)) as [[gen [nd|]]|] eqn:En; try exact Hinv.
  destruct (Nat.eqb gen (lgen l)) eqn:Eg; [|exact Hinv].
  destruct (existsb (same_place (lsid l) (lidx l)) (gbor g)) eqn:Hbp; [exact Hinv|].
  apply Nat.eqb_eq in Eg.
  pose proof Hinv as [HN HP HS HL HG HB HC HIN HI].
  destruct (find_store_some _ _ _ Ef) as [Hs Hsid]. simpl.
  set (s' := mkStore (ssid s) (stab s) (upd (sslots s) (lidx l) (mkSlot (S gen) None))).
  assert (Ef' : find_store (ssid s') (gstores g) = Some s) by (simpl; rewrite Hsid; exact Ef).
  assert (Hidx : lidx l < length (sslots s)) by (apply nth_error_Some; congruence).
  assert (Hold : In (mkSlot gen (Some nd)) (sslots s)) by (eapply nth_error_In; exact En).
  destruct (HG s _ Hs Hold) as [_ Hglt]. simpl in Hglt. specialize (Hglt ltac:(discriminate)).
  set (D := gfp (del_store (ssid s) (gstores g))).
  assert (Hdel : Permutation (gfp (gstores g)) (store_fp s ++ D))
    by (unfold D; apply gfp_del_perm; [exact HS| rewrite Hsid; exact Ef]).
  assert (Hup : Permutation (slots_fp (sslots s)) (nblocks nd ++ slots_fp (sslots s'))).
  { pose proof (slots_fp_upd (sslots s) (lidx l) _ (mkSlot (S gen) None) En) as Hu.
    simpl in Hu. rewrite app_nil_r in Hu. eapply Permutation_trans; [apply Permutation_sym; exact Hu|].
    apply Permutation_app_comm. }
  assert (Hset : Permutation (gfp (set_store s' (gstores g))) (store_fp s' ++ D))
    by (unfold D; exact (gfp_set_perm _ _ _ HS Ef')).
  assert (HP2 : Permutation (gheap g) (nblocks nd ++ gfp (set_store s' (gstores g)))).
  { eapply Permutation_trans; [exact HP|]. eapply Permutation_trans; [exact Hdel|].
    eapply Permutation_trans; [|apply Permutation_app_head; apply Permutation_sym; exact Hset].
    unfold store_fp. simpl. eapply Permutation_trans; [|apply Permutation_middle]. apply perm_skip.
    rewrite app_assoc. apply Permutation_app_tail. exact Hup. }
  constructor; simpl.
  - unfold free. apply NoDup_filter'. exact HN.
  - apply free_perm; [eapply Permutation_NoDup; [exact HP2| exact HN]| exact HP2].
  - rewrite map_ssid_set. exact HS.
  - intros x Hx. destruct (in_set _ _ _ Hx) as [E|E]; [subst x; simpl; exact (HL s Hs)| exact (HL x E)].
  - apply gens_set; [exact HG|]. intros y Hy. simpl in Hy. destruct (in_upd _ _ _ _ _ Hy) as [E|E].
    + subst y. simpl. split; [lia| intros F; contradiction F; reflexivity].
    + exact (HG s y Hs E).
  - intros b Hin. apply (borrow_at_set _ s); [exact Ef'| exact (HB b Hin)|].
    intros E. simpl in E. split; [reflexivity|]. intros sl Ens Ho. simpl.
    assert (Hi : bidx b <> lidx l).
    { intros Ei. assert (Hx : existsb (same_place (lsid l) (lidx l)) (gbor g) = true).
      { apply existsb_exists. exists b. split; [exact Hin|]. unfold same_place.
        rewrite E, Ei, <- Hsid, !Nat.eqb_refl. reflexivity. }
      congruence. }
    exists sl. rewrite (nth_upd_other _ _ _ _ _ Hi). split; assumption.
  - exact HC.
  - exact HIN.
  - intros l0 Hl0. destruct (HI l0 Hl0) as [Hlt0 Hat]. split; [exact Hlt0|].
    apply (issued_at_set _ s); [exact Ef'| exact Hat|].
    intros _ sl0 En0 Hc0. simpl.
    destruct (Nat.eq_dec (lidx l0) (lidx l)) as [Ei|Ei].
    + rewrite Ei in En0 |- *. rewrite En in En0. injection En0 as En0. subst sl0.
      exists (mkSlot (S gen) None). split; [apply nth_upd_same; exact Hidx|]. simpl in Hc0 |- *.
      left. lia.
    + exists sl0. rewrite (nth_upd_other _ _ _ _ _ Ei). split; [exact En0| exact Hc0].
Qed.

Lemma ginv_begin : forall g l wr, GInv g -> GInv (fst (g_begin g l wr)).
Proof.
  intros g l wr Hinv. unfold g_begin.
  destruct (find_store (lsid l) (gstores g)) as [s|] eqn:Ef; [|exact Hinv].
  destruct (follow s (lidx l, lgen l)) as [nd|] eqn:Efo; [|exact Hinv].
  destruct (conflicts wr (lsid l) (lidx l) (gbor g)) eqn:Hc; [exact Hinv|].
  destruct Hinv as [HN HP HS HL HG HB HC HIN HI].
  constructor; simpl; try assumption.
  - intros b [E|Hin]; [|exact (HB b Hin)]. subst b.
    destruct (follow_some _ _ _ Efo) as [sl [En [_ Eo]]].
    exists s, sl. simpl. split; [exact Ef| split; [reflexivity| split; [exact En| rewrite Eo; discriminate]]].
  - split; [|exact HC]. intros b' Hin Hp. simpl in Hp.
    unfold conflicts in Hc.
    assert (Hf : (same_place (lsid l) (lidx l) b' && (wr || bwr b')) = false).
    { destruct (same_place (lsid l) (lidx l) b' && (wr || bwr b')) eqn:E; [|reflexivity].
      assert (existsb (fun b => same_place (lsid l) (lidx l) b && (wr || bwr b)) (gbor g) = true)
        by (apply existsb_exists; exists b'; split; assumption).
      congruence. }
    rewrite Hp in Hf. simpl in Hf. apply orb_false_iff in Hf. exact Hf.
Qed.

Lemma ginv_end : forall g i, GInv g -> GInv (fst (g_end g i)).
Proof.
  intros g i Hinv. unfold g_end. destruct (Nat.ltb i (length (gbor g))); [|exact Hinv].
  destruct Hinv as [HN HP HS HL HG HB HC HIN HI].
  constructor; simpl; try assumption.
  - intros b Hin. apply HB. eapply in_remove_nth. exact Hin.
  - apply noconflict_remove. exact HC.
Qed.

Lemma ginv_write : forall g i d es, GInv g -> GInv (fst (g_write g i d es)).
Proof.
  intros g i d es Hinv. unfold g_write.
  destruct (nth_error (gbor g) i) as [b|] eqn:Eb; [|exact Hinv].
  destruct (negb (bwr b)); [exact Hinv|].
  destruct (find_store (bsid b) (gstores g)) as [s|] eqn:Ef; [|exact Hinv].
  destruct (nth_error (sslots s) (bidx b)) as [[gen [nd|]]|] eqn:En; try exact Hinv.
  pose proof Hinv as [HN HP HS HL HG HB HC HIN HI].
  destruct (find_store_some _ _ _ Ef) as [Hs Hsid]. simpl.
  set (nd' := mkNode d es (nblocks nd)).
  set (s' := mkStore (ssid s) (stab s) (upd (sslots s) (bidx b) (mkSlot gen (Some nd')))).
  assert (Ef' : find_store (ssid s') (gstores g) = Some s) by (simpl; rewrite Hsid; exact Ef).
  assert (Hidx : bidx b < length (sslots s)) by (apply nth_error_Some; congruence).
  assert (Hold : In (mkSlot gen (Some nd)) (sslots s)) by (eapply nth_error_In; exact En).
  assert (Hup : Permutation (slots_fp (sslots s')) (slots_fp (sslots s))).
  { pose proof (slots_fp_upd (sslots s) (bidx b) _ (mkSlot gen (Some nd')) En) as Hu.
    simpl in Hu. eapply Permutation_app_inv_r. exact Hu. }
  assert (Hset : Permutation (gfp (set_store s' (gstores g))) (gfp (gstores g))).
  { eapply Permutation_trans; [apply (gfp_set_perm _ _ _ HS Ef')|].
    eapply Permutation_trans; [|apply Permutation_sym; apply gfp_del_perm; [exact HS| exact Ef]].
    simpl ssid. rewrite Hsid. apply Permutation_app_tail. unfold store_fp. simpl. apply perm_skip. exact Hup. }
  constructor; simpl; try assumption.
  - eapply Permutation_trans; [exact HP|]. apply Permutation_sym. exact Hset.
  - rewrite map_ssid_set. exact HS.
  - intros x Hx. destruct (in_set _ _ _ Hx) as [E|E]; [subst x; simpl; exact (HL s Hs)| exact (HL x E)].
  - apply gens_set; [exact HG|]. intros y Hy. simpl in Hy. destruct (in_upd _ _ _ _ _ Hy) as [E|E].
    + subst y. simpl. destruct (HG s _ Hs Hold) as [H1 H2]. simpl in H1, H2. split; [exact H1| intros _; apply H2; discriminate].
    + exact (HG s y Hs E).
  - intros b0 Hin. apply (borrow_at_set _ s); [exact Ef'| exact (HB b0 Hin)|].
    intros _. split; [reflexivity|]. intros sl Ens Ho. simpl.
    destruct (Nat.eq_dec (bidx b0) (bidx b)) as [Ei|Ei].
    + rewrite Ei. exists (mkSlot gen (Some nd')). split; [apply nth_upd_same; exact Hidx| discriminate].
    + exists sl. rewrite (nth_upd_other _ _ _ _ _ Ei). split; assumption.
  - intros l0 Hl0. destruct (HI l0 Hl0) as [Hlt0 Hat]. split; [exact Hlt0|].
    apply (issued_at_set _ s); [exact Ef'| exact Hat|].
    intros _ sl0 En0 Hc0. simpl.
    destruct (Nat.eq_dec (lidx l0) (bidx b)) as [Ei|Ei].
    + rewrite Ei in En0 |- *. rewrite En in En0. injection En0 as En0. subst sl0.
      exists (mkSlot gen (Some nd')). split; [apply nth_upd_same; exact Hidx|]. simpl in Hc0 |- *.
      destruct Hc0 as [Hc0|[Hc0 _]]; [left; exact Hc0| right; split; [exact Hc0| discriminate]].
    + exists sl0. rewrite (nth_upd_other _ _ _ _ _ Ei). split; [exact En0| exact Hc0].
Qed.

Lemma ginv_drop : forall g sid, GInv g -> GInv (fst (g_drop g sid)).
Proof.
  intros g sid Hinv. unfold g_drop.
  destruct (find_store sid (gstores g)) as [s|] eqn:Ef; [|exact Hinv].
  destruct (store_borrowed sid (gbor g)) eqn:Hb; [exact Hinv|].
  pose proof Hinv as [HN HP HS HL HG HB HC HIN HI].
  pose proof (not_borrowed _ _ Hb) as Hnb.
  assert (HP2 : Permutation (gheap g) (store_fp s ++ gfp (del_store sid (gstores g))))
    by (eapply Permutation_trans; [exact HP| apply gfp_del_perm; assumption]).
  constructor; simpl.
  - unfold free. apply NoDup_filter'. exact HN.
  - apply free_perm; [eapply Permutation_NoDup; [exact HP2| exact HN]| exact HP2].
  - apply nodup_del. exact HS.
  - intros x Hx. exact (HL x (in_del _ _ _ Hx)).
  - intros x sl Hx Hsl. exact (HG x sl (in_del _ _ _ Hx) Hsl).
  - intros b Hin. destruct (HB b Hin) as [s0 [sl [E0 R]]].
    exists s0, sl. rewrite (find_del_other _ _ _ (Hnb b Hin)). split; [exact E0| exact R].
  - exact HC.
  - exact HIN.
  - intros l0 Hl0. destruct (HI l0 Hl0) as [Hlt0 Hat]. split; [exact Hlt0|].
    intros s0 E0. destruct (Nat.eq_dec (lsid l0) sid) as [Eq|Ne].
    + rewrite Eq, find_del_same in E0. discriminate.
    + rewrite (find_del_other _ _ _ Ne) in E0. exact (Hat s0 E0).
Qed.

(* Every operation keeps the invariant, whether it succeeds or is refused. *)
Theorem ginv_step : forall g o, GInv g -> GInv (fst (gexec gmax g o)).
Proof.
  intros g [tb|sid idx d es bs tb|l|l wr|i|i d es|sid] H; simpl.
  - apply ginv_new. exact H.
  - apply ginv_insert. exact H.
  - apply ginv_delete. exact H.
  - apply ginv_begin. exact H.
  - apply ginv_end. exact H.
  - apply ginv_write. exact H.
  - apply ginv_drop. exact H.
Qed.

Theorem ginv_run : forall os g, GInv g -> GInv (grun gmax g os).
Proof. induction os as [|o r IH]; simpl; intros g H; [exact H| apply IH; apply ginv_step; exact H]. Qed.

(* ------------------------------------------------------------------ *)
(* Consequences                                                         *)
(* ------------------------------------------------------------------ *)

(* The heap is exactly the stores' footprints. Links are not part of it:
   what a long-lived store retains is what its slots hold, reachable from a
   link or not. *)
Theorem heap_is_store_footprint : forall g, GInv g ->
  NoDup (gheap g) /\ Permutation (gheap g) (gfp (gstores g)).
Proof. intros g H. split; [exact (gi_nodup _ H)| exact (gi_perm _ H)]. Qed.

Theorem drop_releases_exactly : forall g sid s, GInv g ->
  find_store sid (gstores g) = Some s -> store_borrowed sid (gbor g) = false ->
  let g' := fst (gexec gmax g (ODrop sid)) in
  Permutation (gheap g) (store_fp s ++ gheap g') /\ NoDup (store_fp s) /\
  (forall b, In b (store_fp s) -> ~ In b (gheap g')).
Proof.
  intros g sid s Hinv Ef Hb. cbv zeta. cbn [gexec]. unfold g_drop. rewrite Ef, Hb. cbn [fst gheap].
  pose proof Hinv as [HN HP HS _ _ _ _ _ _].
  assert (HP2 : Permutation (gheap g) (store_fp s ++ gfp (del_store sid (gstores g))))
    by (eapply Permutation_trans; [exact HP| apply gfp_del_perm; assumption]).
  pose proof (Permutation_NoDup HP2 HN) as HN2.
  pose proof (free_perm _ _ _ HN2 HP2) as Hf.
  split; [eapply Permutation_trans; [exact HP2| apply Permutation_app_head; apply Permutation_sym; exact Hf]|].
  split; [exact (proj1 (NoDup_app_split _ _ _ HN2))|].
  intros b Hin Hh. unfold free in Hh. apply filter_In in Hh. destruct Hh as [_ Hh].
  apply negb_true_iff in Hh. assert (bmem b (store_fp s) = true) by (apply bmem_true; exact Hin). congruence.
Qed.

Theorem drop_refused_while_borrowed : forall g sid s,
  find_store sid (gstores g) = Some s -> store_borrowed sid (gbor g) = true ->
  gexec gmax g (ODrop sid) = (g, GRefused RBorrowed).
Proof. intros g sid s Ef Hb. simpl. unfold g_drop. rewrite Ef, Hb. reflexivity. Qed.

Theorem delete_refused_while_borrowed : forall g l,
  existsb (same_place (lsid l) (lidx l)) (gbor g) = true ->
  fst (gexec gmax g (ODelete l)) = g.
Proof.
  intros g l Hb. simpl. unfold g_delete.
  destruct (find_store (lsid l) (gstores g)); [|reflexivity].
  destruct (nth_error (sslots s) (lidx l)) as [[gen [nd|]]|]; try reflexivity.
  destruct (Nat.eqb gen (lgen l)); [rewrite Hb|]; reflexivity.
Qed.

Theorem borrow_exclusive : forall g i j b1 b2, GInv g -> i <> j ->
  nth_error (gbor g) i = Some b1 -> nth_error (gbor g) j = Some b2 ->
  bsid b1 = bsid b2 -> bidx b1 = bidx b2 -> bwr b1 = false /\ bwr b2 = false.
Proof. intros g i j b1 b2 H. exact (noconflict_pair _ _ _ _ _ (gi_noconf _ H)). Qed.

(* A borrow points into its store's current table, which is live, and at a
   slot that still holds a node. *)
Theorem borrow_table_live : forall g b, GInv g -> In b (gbor g) ->
  In (btab b) (gheap g) /\ exists s sl, find_store (bsid b) (gstores g) = Some s /\
    nth_error (sslots s) (bidx b) = Some sl /\ snode sl <> None.
Proof.
  intros g b H Hin. destruct (gi_bor _ H b Hin) as [s [sl [Ef [Et [En Ho]]]]].
  split; [|exists s, sl; auto].
  destruct (find_store_some _ _ _ Ef) as [Hs _].
  eapply Permutation_in; [apply Permutation_sym; exact (gi_perm _ H)|].
  rewrite Et. unfold gfp. apply in_flat_map. exists s. split; [exact Hs| left; reflexivity].
Qed.

Theorem issued_links_unique : forall g, GInv g -> NoDup (gissued g).
Proof. intros g H. exact (gi_iss_nodup _ H). Qed.

Theorem gens_bounded : forall g s sl, GInv g -> In s (gstores g) -> In sl (sslots s) ->
  sgen sl <= gmax /\ (snode sl <> None -> sgen sl < gmax).
Proof. intros g s sl H. exact (gi_gens _ H s sl). Qed.

(* The link an insert returns resolves to the node it inserted, in the
   blocks the allocator chose. *)
Theorem insert_resolves : forall g sid idx d es bs tb l, GInv g ->
  snd (gexec gmax g (OInsert sid idx d es bs tb)) = GLink l ->
  exists nd, resolve (fst (gexec gmax g (OInsert sid idx d es bs tb))) l = Found nd /\
    ndata nd = d /\ nedges nd = es /\ nblocks nd = bs.
Proof.
  intros g sid idx d es bs tb l Hinv Hr. simpl in Hr |- *. unfold g_insert in Hr |- *.
  destruct (find_store sid (gstores g)) as [s|] eqn:Ef; [|discriminate].
  destruct (nth_error (sslots s) idx) as [sl|] eqn:En.
  - destruct (snode sl); [discriminate|]. destruct (Nat.ltb (sgen sl) gmax); [|discriminate].
    destruct (bnodup bs && bfree bs (gheap g)); [|discriminate].
    injection Hr as Hr. subst l. exists (mkNode d es bs). split; [|split; [reflexivity| split; reflexivity]].
    apply resolve_found_spec. simpl.
    set (s' := mkStore sid (stab s) (upd (sslots s) idx (mkSlot (sgen sl) (Some (mkNode d es bs))))).
    exists s'. eexists. split; [change sid with (ssid s') at 1; eapply find_set_same; exact Ef|].
    split; [apply nth_upd_same; apply nth_error_Some; congruence| split; reflexivity].
  - destruct (store_borrowed sid (gbor g)); [discriminate|].
    destruct (Nat.eqb idx (length (sslots s)) && Nat.ltb 0 gmax) eqn:Hap; [|discriminate].
    destruct (bnodup (tb :: bs) && bfree (tb :: bs) (free [stab s] (gheap g))); [|discriminate].
    apply andb_true_iff in Hap. destruct Hap as [Hidx _]. apply Nat.eqb_eq in Hidx.
    injection Hr as Hr. subst l. exists (mkNode d es bs). split; [|split; [reflexivity| split; reflexivity]].
    apply resolve_found_spec. simpl.
    set (s' := mkStore sid tb (sslots s ++ [mkSlot 0 (Some (mkNode d es bs))])).
    exists s'. eexists. split; [change sid with (ssid s') at 1; eapply find_set_same; exact Ef|].
    unfold s'. simpl. rewrite Hidx, nth_error_app2, Nat.sub_diag by lia. simpl.
    split; [reflexivity| split; reflexivity].
Qed.

(* A resolved link's node lies in its store's footprint, and so in the
   live heap: resolution never reaches storage outside the owner. *)
Theorem resolve_in_store_fp : forall g l nd, GInv g -> resolve g l = Found nd ->
  exists s, find_store (lsid l) (gstores g) = Some s /\
    (forall b, In b (nblocks nd) -> In b (store_fp s)) /\
    (forall b, In b (nblocks nd) -> In b (gheap g)).
Proof.
  intros g l nd Hinv Hr. apply resolve_found_spec in Hr.
  destruct Hr as [s [sl [Ef [En [_ Eo]]]]].
  assert (Hin : forall b, In b (nblocks nd) -> In b (store_fp s)).
  { intros b Hb. right. unfold slots_fp. apply in_flat_map. exists sl.
    split; [eapply nth_error_In; exact En| rewrite Eo; exact Hb]. }
  exists s. split; [exact Ef|]. split; [exact Hin|].
  intros b Hb. eapply Permutation_in; [apply Permutation_sym; exact (gi_perm _ Hinv)|].
  unfold gfp. apply in_flat_map. exists s.
  split; [exact (proj1 (find_store_some _ _ _ Ef))| exact (Hin b Hb)].
Qed.

(* ------------------------------------------------------------------ *)
(* Staleness is permanent                                               *)
(* ------------------------------------------------------------------ *)

Definition StaleAt (ss : list Store) (l : Link) : Prop :=
  forall s, find_store (lsid l) ss = Some s ->
    exists sl, nth_error (sslots s) (lidx l) = Some sl /\ lgen l < sgen sl.

Definition stale (g : GS) (l : Link) : Prop := lsid l < gsid g /\ StaleAt (gstores g) l.

Theorem stale_missing : forall g l, stale g l -> exists r, resolve g l = Missing r.
Proof.
  intros g l [_ Hst]. unfold resolve. destruct (find_store (lsid l) (gstores g)) as [s|] eqn:Ef;
    [|eexists; reflexivity].
  destruct (Hst s Ef) as [sl [En Hlt]]. unfold follow. simpl. rewrite En.
  destruct (Nat.eqb (sgen sl) (lgen l)) eqn:E; [apply Nat.eqb_eq in E; lia| eexists; reflexivity].
Qed.

Lemma stale_at_set : forall ss s s' l,
  find_store (ssid s') ss = Some s -> StaleAt ss l ->
  (lsid l = ssid s' -> forall sl, nth_error (sslots s) (lidx l) = Some sl -> lgen l < sgen sl ->
     exists sl', nth_error (sslots s') (lidx l) = Some sl' /\ lgen l < sgen sl') ->
  StaleAt (set_store s' ss) l.
Proof.
  intros ss s s' l Ef Hat Hc s0 E0.
  destruct (Nat.eq_dec (lsid l) (ssid s')) as [Eq|Ne].
  - rewrite Eq, (find_set_same _ _ _ Ef) in E0. injection E0 as E0. subst s0.
    rewrite <- Eq in Ef. destruct (Hat s Ef) as [sl [En Hsl]]. exact (Hc Eq sl En Hsl).
  - rewrite (find_set_other _ _ _ Ne) in E0. exact (Hat s0 E0).
Qed.

Theorem stale_step : forall g o l, GInv g -> stale g l -> stale (fst (gexec gmax g o)) l.
Proof.
  intros g [tb|sid idx d es bs tb|l0|l0 wr|i|i d es|sid] l Hinv [Hlt Hst]; simpl.
  - unfold g_new. destruct (bmem tb (gheap g)); [split; assumption|].
    split; [simpl; lia|]. intros s E0. simpl in E0.
    assert (Hne : Nat.eqb (gsid g) (lsid l) = false) by (apply Nat.eqb_neq; lia).
    rewrite Hne in E0. exact (Hst s E0).
  - unfold g_insert. destruct (find_store sid (gstores g)) as [s|] eqn:Ef; [|split; assumption].
    destruct (find_store_some _ _ _ Ef) as [_ Hsid].
    destruct (nth_error (sslots s) idx) as [sl|] eqn:En.
    + destruct (snode sl); [split; assumption|]. destruct (Nat.ltb (sgen sl) gmax); [|split; assumption].
      destruct (bnodup bs && bfree bs (gheap g)); [|split; assumption].
      split; [exact Hlt|]. simpl. apply (stale_at_set _ s); [exact Ef| exact Hst|].
      intros _ sl0 En0 Hc0. simpl. destruct (Nat.eq_dec (lidx l) idx) as [Ei|Ei].
      * rewrite Ei in En0 |- *. rewrite En in En0. injection En0 as En0. subst sl0.
        eexists. split; [apply nth_upd_same; apply nth_error_Some; congruence| simpl; exact Hc0].
      * exists sl0. rewrite (nth_upd_other _ _ _ _ _ Ei). split; assumption.
    + destruct (store_borrowed sid (gbor g)); [split; assumption|].
      destruct (Nat.eqb idx (length (sslots s)) && Nat.ltb 0 gmax); [|split; assumption].
      destruct (bnodup (tb :: bs) && bfree (tb :: bs) (free [stab s] (gheap g))); [|split; assumption].
      split; [exact Hlt|]. simpl. apply (stale_at_set _ s); [exact Ef| exact Hst|].
      intros _ sl0 En0 Hc0. exists sl0. simpl.
      rewrite nth_error_app1 by (apply nth_error_Some; congruence). split; assumption.
  - unfold g_delete. destruct (find_store (lsid l0) (gstores g)) as [s|] eqn:Ef; [|split; assumption].
    destruct (nth_error (sslots s) (lidx l0)) as [[gen [nd|]]|] eqn:En; try (split; assumption).
    destruct (Nat.eqb gen (lgen l0)); [|split; assumption].
    destruct (existsb (same_place (lsid l0) (lidx l0)) (gbor g)); [split; assumption|].
    destruct (find_store_some _ _ _ Ef) as [_ Hsid].
    split; [exact Hlt|]. simpl. apply (stale_at_set _ s); [simpl; rewrite Hsid; exact Ef| exact Hst|].
    intros _ sl0 En0 Hc0. simpl. destruct (Nat.eq_dec (lidx l) (lidx l0)) as [Ei|Ei].
    + rewrite Ei in En0 |- *. rewrite En in En0. injection En0 as En0. subst sl0.
      eexists. split; [apply nth_upd_same; apply nth_error_Some; congruence| simpl in Hc0 |- *; lia].
    + exists sl0. rewrite (nth_upd_other _ _ _ _ _ Ei). split; assumption.
  - unfold g_begin. destruct (find_store (lsid l0) (gstores g)); [|split; assumption].
    destruct (follow s (lidx l0, lgen l0)); [|split; assumption].
    destruct (conflicts wr (lsid l0) (lidx l0) (gbor g)); split; assumption.
  - unfold g_end. destruct (Nat.ltb i (length (gbor g))); split; assumption.
  - unfold g_write. destruct (nth_error (gbor g) i) as [b|]; [|split; assumption].
    destruct (negb (bwr b)); [split; assumption|].
    destruct (find_store (bsid b) (gstores g)) as [s|] eqn:Ef; [|split; assumption].
    destruct (nth_error (sslots s) (bidx b)) as [[gen [nd|]]|] eqn:En; try (split; assumption).
    destruct (find_store_some _ _ _ Ef) as [_ Hsid].
    split; [exact Hlt|]. simpl. apply (stale_at_set _ s); [simpl; rewrite Hsid; exact Ef| exact Hst|].
    intros _ sl0 En0 Hc0. simpl. destruct (Nat.eq_dec (lidx l) (bidx b)) as [Ei|Ei].
    + rewrite Ei in En0 |- *. rewrite En in En0. injection En0 as En0. subst sl0.
      eexists. split; [apply nth_upd_same; apply nth_error_Some; congruence| simpl in Hc0 |- *; exact Hc0].
    + exists sl0. rewrite (nth_upd_other _ _ _ _ _ Ei). split; assumption.
  - unfold g_drop. destruct (find_store sid (gstores g)) as [s|]; [|split; assumption].
    destruct (store_borrowed sid (gbor g)); [split; assumption|].
    split; [exact Hlt|]. intros s0 E0. simpl in E0. destruct (Nat.eq_dec (lsid l) sid) as [Eq|Ne].
    + rewrite Eq, find_del_same in E0. discriminate.
    + rewrite (find_del_other _ _ _ Ne) in E0. exact (Hst s0 E0).
Qed.

Theorem stale_forever : forall os g l, GInv g -> stale g l ->
  exists r, resolve (grun gmax g os) l = Missing r.
Proof.
  induction os as [|o r IH]; simpl; intros g l Hinv Hs; [exact (stale_missing g l Hs)|].
  apply IH; [apply ginv_step; exact Hinv| apply stale_step; assumption].
Qed.

(* A successful delete makes its link stale, so it stays refused even
   after its slot is reused. *)
Theorem delete_makes_stale : forall g l, GInv g ->
  snd (gexec gmax g (ODelete l)) = GUnit -> stale (fst (gexec gmax g (ODelete l))) l.
Proof.
  intros g l Hinv Hr. simpl in Hr |- *. unfold g_delete in Hr |- *.
  destruct (find_store (lsid l) (gstores g)) as [s|] eqn:Ef; [|discriminate].
  destruct (nth_error (sslots s) (lidx l)) as [[gen [nd|]]|] eqn:En; try discriminate.
  destruct (Nat.eqb gen (lgen l)) eqn:Eg; [|discriminate].
  destruct (existsb (same_place (lsid l) (lidx l)) (gbor g)); [discriminate|].
  apply Nat.eqb_eq in Eg. destruct (find_store_some _ _ _ Ef) as [Hs Hsid]. simpl.
  split; [rewrite <- Hsid; exact (gi_sid_lt _ Hinv s Hs)|].
  intros s0 E0.
  set (s' := mkStore (ssid s) (stab s) (upd (sslots s) (lidx l) (mkSlot (S gen) None))) in *.
  assert (Ef' : find_store (ssid s') (gstores g) = Some s) by (simpl; rewrite Hsid; exact Ef).
  rewrite <- Hsid in E0. change (find_store (ssid s') (set_store s' (gstores g)) = Some s0) in E0.
  rewrite (find_set_same _ _ _ Ef') in E0. injection E0 as E0. subst s0.
  exists (mkSlot (S gen) None). simpl.
  split; [apply nth_upd_same; apply nth_error_Some; congruence| lia].
Qed.

(* Dropping a store makes every link into it stale; store ids are never
   reused, so no later store resolves them. *)
Theorem drop_makes_stale : forall g sid l, GInv g ->
  snd (gexec gmax g (ODrop sid)) = GUnit -> lsid l = sid -> stale (fst (gexec gmax g (ODrop sid))) l.
Proof.
  intros g sid l Hinv Hr Hl. simpl in Hr |- *. unfold g_drop in Hr |- *.
  destruct (find_store sid (gstores g)) as [s|] eqn:Ef; [|discriminate].
  destruct (store_borrowed sid (gbor g)); [discriminate|]. simpl.
  destruct (find_store_some _ _ _ Ef) as [Hs Hsid].
  split; [rewrite Hl, <- Hsid; exact (gi_sid_lt _ Hinv s Hs)|].
  intros s0 E0. rewrite Hl in E0.
  change (find_store sid (del_store sid (gstores g)) = Some s0) in E0.
  rewrite find_del_same in E0. discriminate.
Qed.

(* ------------------------------------------------------------------ *)
(* Append-only stores need no generation check                          *)
(* ------------------------------------------------------------------ *)

Definition NoHoles (ss : list Store) : Prop :=
  forall s sl, In s ss -> In sl (sslots s) -> sgen sl = 0 /\ snode sl <> None.

Lemma noholes_set : forall ss s', NoHoles ss -> (forall sl, In sl (sslots s') -> sgen sl = 0 /\ snode sl <> None) ->
  NoHoles (set_store s' ss).
Proof. intros ss s' H Hn. exact (gens_set ss s' (fun sl => sgen sl = 0 /\ snode sl <> None) H Hn). Qed.

Theorem noholes_step : forall g o, NoHoles (gstores g) -> (forall l, o <> ODelete l) ->
  NoHoles (gstores (fst (gexec gmax g o))).
Proof.
  intros g [tb|sid idx d es bs tb|l|l wr|i|i d es|sid] Hh Hno; simpl; try (exfalso; eapply Hno; reflexivity).
  - unfold g_new. destruct (bmem tb (gheap g)); [exact Hh|]. simpl.
    intros s sl [E|Hs] Hsl; [subst s; simpl in Hsl; contradiction| exact (Hh s sl Hs Hsl)].
  - unfold g_insert. destruct (find_store sid (gstores g)) as [s|] eqn:Ef; [|exact Hh].
    destruct (find_store_some _ _ _ Ef) as [Hs _].
    destruct (nth_error (sslots s) idx) as [sl|] eqn:En.
    + destruct (snode sl) eqn:Eo; [exact Hh|]. exfalso.
      destruct (Hh s sl Hs (nth_error_In _ _ En)) as [_ Ho]. contradiction.
    + destruct (store_borrowed sid (gbor g)); [exact Hh|].
      destruct (Nat.eqb idx (length (sslots s)) && Nat.ltb 0 gmax); [|exact Hh].
      destruct (bnodup (tb :: bs) && bfree (tb :: bs) (free [stab s] (gheap g))); [|exact Hh]. simpl.
      apply noholes_set; [exact Hh|]. intros y Hy. simpl in Hy. apply in_app_or in Hy.
      destruct Hy as [Hy|[Hy|[]]]; [exact (Hh s y Hs Hy)| subst y; simpl; split; [reflexivity| discriminate]].
  - unfold g_begin. destruct (find_store (lsid l) (gstores g)); [|exact Hh].
    destruct (follow s (lidx l, lgen l)); [|exact Hh].
    destruct (conflicts wr (lsid l) (lidx l) (gbor g)); exact Hh.
  - unfold g_end. destruct (Nat.ltb i (length (gbor g))); exact Hh.
  - unfold g_write. destruct (nth_error (gbor g) i) as [b|]; [|exact Hh].
    destruct (negb (bwr b)); [exact Hh|].
    destruct (find_store (bsid b) (gstores g)) as [s|] eqn:Ef; [|exact Hh].
    destruct (nth_error (sslots s) (bidx b)) as [[gen [nd|]]|] eqn:En; try exact Hh. simpl.
    destruct (find_store_some _ _ _ Ef) as [Hs _].
    apply noholes_set; [exact Hh|]. intros y Hy. simpl in Hy. destruct (in_upd _ _ _ _ _ Hy) as [E|E].
    + subst y. simpl. destruct (Hh s _ Hs (nth_error_In _ _ En)) as [Hg _]. simpl in Hg.
      split; [exact Hg| discriminate].
    + exact (Hh s y Hs E).
  - unfold g_drop. destruct (find_store sid (gstores g)) as [s|]; [|exact Hh].
    destruct (store_borrowed sid (gbor g)); [exact Hh|]. simpl.
    intros x sl Hx Hsl. exact (Hh x sl (in_del _ _ _ Hx) Hsl).
Qed.

(* In a store that never deletes, every link it has issued resolves: the
   generation check can be elided there; only the store's liveness
   remains. *)
Theorem append_only_links_resolve : forall g l, GInv g -> NoHoles (gstores g) ->
  In l (gissued g) -> find_store (lsid l) (gstores g) <> None ->
  exists nd, resolve g l = Found nd.
Proof.
  intros g l Hinv Hh Hin Hs. destruct (gi_iss _ Hinv l Hin) as [_ Hat].
  destruct (find_store (lsid l) (gstores g)) as [s|] eqn:Ef; [|contradiction].
  destruct (Hat s Ef) as [sl [En Hc]]. destruct (find_store_some _ _ _ Ef) as [Hs' _].
  destruct (Hh s sl Hs' (nth_error_In _ _ En)) as [Hg Ho].
  destruct Hc as [Hc|[Hc _]]; [lia|].
  destruct (snode sl) as [nd|] eqn:Eo; [|contradiction].
  exists nd. apply resolve_found_spec. exists s, sl. auto.
Qed.

End Invariant.

(* ------------------------------------------------------------------ *)
(* Snapshot                                                             *)
(* ------------------------------------------------------------------ *)

Lemma nth_copy_slots : forall n j ss i,
  nth_error (copy_slots n j ss) i =
    option_map (fun sl => mkSlot (sgen sl) (copy_node n (j + i) (snode sl))) (nth_error ss i).
Proof.
  intros n j ss. revert j. induction ss as [|sl r IH]; intros j [|i]; simpl; try reflexivity.
  - rewrite Nat.add_0_r. reflexivity.
  - rewrite IH. f_equal. rewrite Nat.add_succ_r. reflexivity.
Qed.

(* Store-relative edges survive a snapshot: each one names the copy of the
   node it named in the original. *)
Theorem snapshot_preserves_relative_edges : forall s sid' n e,
  option_map (fun nd => (ndata nd, nedges nd)) (follow (snapshot s sid' n) e) =
  option_map (fun nd => (ndata nd, nedges nd)) (follow s e).
Proof.
  intros s sid' n [i gen]. unfold follow, snapshot. simpl. rewrite nth_copy_slots.
  destruct (nth_error (sslots s) i) as [sl|]; simpl; [|reflexivity].
  destruct (Nat.eqb (sgen sl) gen); [|reflexivity].
  destruct (snode sl); reflexivity.
Qed.

(* A snapshot copies the whole node footprint. *)
Theorem snapshot_copies_footprint : forall s sid' n,
  length (slots_fp (sslots (snapshot s sid' n))) = length (slots_fp (sslots s)).
Proof.
  intros s sid' n. unfold snapshot, slots_fp. simpl. generalize 0.
  induction (sslots s) as [|sl r IH]; intros j; simpl; [reflexivity|].
  rewrite !length_app, IH. destruct (snode sl); simpl; [rewrite alloc_length|]; reflexivity.
Qed.

(* ------------------------------------------------------------------ *)
(* Whole-heap admission: the external footprint is not graph storage   *)
(* ------------------------------------------------------------------ *)

Lemma free_contains_only_old_blocks : forall b bs H,
  In b (free bs H) -> In b H.
Proof. intros b bs H Hb. unfold free in Hb. apply filter_In in Hb. exact (proj1 Hb). Qed.

(* Even the fragment can only retain old blocks or add its explicit allocation
   candidates. This is derived from all seven operations, not assumed from an
   allocator specification or the desired post-state. *)
Lemma gexec_storage_coverage : forall gmax g o b,
  In b (gheap (fst (gexec gmax g o))) ->
  In b (gheap g) \/ In b (allocation_blocks g o).
Proof.
  intros gmax g [tb|sid idx d es bs tb|l|l wr|i|i d es|sid] b;
    cbn [gexec allocation_blocks].
  - unfold g_new. destruct (bmem tb (gheap g)); cbn [refuse fst gheap]; simpl; tauto.
  - unfold g_insert. destruct (find_store sid (gstores g)) as [s|]; [|simpl; tauto].
    destruct (nth_error (sslots s) idx) as [sl|].
    + destruct (snode sl); [simpl; tauto|].
      destruct (Nat.ltb (sgen sl) gmax); [|simpl; tauto].
      destruct (bnodup bs && bfree bs (gheap g)); [|simpl; tauto].
      cbn [fst gheap]. rewrite in_app_iff. tauto.
    + destruct (store_borrowed sid (gbor g)); [simpl; tauto|].
      destruct (Nat.eqb idx (length (sslots s)) && Nat.ltb 0 gmax); [|simpl; tauto].
      destruct (bnodup (tb :: bs) && bfree (tb :: bs) (free [stab s] (gheap g))); [|simpl; tauto].
      cbn [fst gheap]. rewrite in_app_iff. simpl. intros [Hb|[Hb|Hb]];
        [tauto|tauto|left; eapply free_contains_only_old_blocks; exact Hb].
  - unfold g_delete. destruct (find_store (lsid l) (gstores g)) as [s|]; [|simpl; tauto].
    destruct (nth_error (sslots s) (lidx l)) as [[gen [nd|]]|]; try (simpl; tauto).
    destruct (Nat.eqb gen (lgen l)); [|simpl; tauto].
    destruct (existsb (same_place (lsid l) (lidx l)) (gbor g)); [simpl; tauto|].
    cbn [fst gheap]. intros Hb. left. eapply free_contains_only_old_blocks. exact Hb.
  - unfold g_begin. destruct (find_store (lsid l) (gstores g)) as [s|]; [|simpl; tauto].
    destruct (follow s (lidx l, lgen l)); [|simpl; tauto].
    destruct (conflicts wr (lsid l) (lidx l) (gbor g)); simpl; tauto.
  - unfold g_end. destruct (Nat.ltb i (length (gbor g))); simpl; tauto.
  - unfold g_write. destruct (nth_error (gbor g) i) as [br|]; [|simpl; tauto].
    destruct (negb (bwr br)); [simpl; tauto|].
    destruct (find_store (bsid br) (gstores g)) as [s|]; [|simpl; tauto].
    destruct (nth_error (sslots s) (bidx br)) as [[gen [nd|]]|]; simpl; tauto.
  - unfold g_drop. destruct (find_store sid (gstores g)) as [s|]; [|simpl; tauto].
    destruct (store_borrowed sid (gbor g)); [simpl; tauto|].
    cbn [fst gheap]. intros Hb. left. eapply free_contains_only_old_blocks. exact Hb.
Qed.

Theorem missing_frame_refused : forall gmax g o,
  gexec_framed gmax None g o = (g, GRefused RNoFrame).
Proof. reflexivity. Qed.

Theorem external_allocation_refused : forall gmax Ho g o b,
  In b (allocation_blocks g o) -> In b Ho ->
  gexec_framed gmax (Some Ho) g o = (g, GRefused RNotFree).
Proof.
  intros gmax Ho g o b Ha Ho'. unfold gexec_framed.
  destruct (bfree (allocation_blocks g o) Ho) eqn:Hf; [|reflexivity].
  exfalso. exact (bfree_spec _ _ Hf b Ha Ho').
Qed.

Theorem ginv_framed_step : forall gmax Ho g o,
  GInv gmax g -> GInv gmax (fst (gexec_framed gmax (Some Ho) g o)).
Proof.
  intros gmax Ho g o Hi. unfold gexec_framed.
  destruct (bfree (allocation_blocks g o) Ho); [apply ginv_step; exact Hi| exact Hi].
Qed.

Theorem framed_step_disjoint : forall gmax Ho g o,
  NoDup (gheap g ++ Ho) ->
  forall b, In b (gheap (fst (gexec_framed gmax (Some Ho) g o))) -> ~ In b Ho.
Proof.
  intros gmax Ho g o HN b Hb.
  destruct (NoDup_app_split _ _ _ HN) as [_ [_ Hd]].
  unfold gexec_framed in Hb. destruct (bfree (allocation_blocks g o) Ho) eqn:Hf.
  - destruct (gexec_storage_coverage _ _ _ _ Hb) as [Hold|Hnew].
    + exact (Hd b Hold).
    + exact (bfree_spec _ _ Hf b Hnew).
  - exact (Hd b Hb).
Qed.

Theorem framed_step_preserves_ownership : forall gmax Ho g o,
  GInv gmax g -> NoDup (gheap g ++ Ho) ->
  GInv gmax (fst (gexec_framed gmax (Some Ho) g o)) /\
  NoDup (gheap (fst (gexec_framed gmax (Some Ho) g o)) ++ Ho).
Proof.
  intros gmax Ho g o Hi HN. pose proof (ginv_framed_step gmax Ho g o Hi) as Hi'.
  split; [exact Hi'|]. apply NoDup_app_join.
  - exact (gi_nodup _ _ Hi').
  - exact (proj1 (proj2 (NoDup_app_split _ _ _ HN))).
  - exact (framed_step_disjoint _ _ _ _ HN).
Qed.

Theorem framed_run_preserves_ownership : forall gmax Ho os g,
  GInv gmax g -> NoDup (gheap g ++ Ho) ->
  GInv gmax (grun_framed gmax (Some Ho) g os) /\
  NoDup (gheap (grun_framed gmax (Some Ho) g os) ++ Ho).
Proof.
  intros gmax Ho os. induction os as [|o r IH]; intros g Hi HN; [auto|].
  cbn [grun_framed]. destruct (framed_step_preserves_ownership gmax Ho g o Hi HN) as [Hi' HN'].
  exact (IH _ Hi' HN').
Qed.

Print Assumptions framed_step_preserves_ownership.
Print Assumptions framed_run_preserves_ownership.
Print Assumptions external_allocation_refused.
