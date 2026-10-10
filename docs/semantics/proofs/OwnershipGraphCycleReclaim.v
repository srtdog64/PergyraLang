(* Local reclamation of unreachable cycles inside a live graph store.

   OwnershipGraphLinks keeps every node a long-lived store has not deleted,
   reachable or not ([unreachable_nodes_retained]). This supplement adds a
   reference reclaimer using an exact summary of the part of the store
   outside a candidate set C: one number per node, the count of edges that name the node's current identity
   (slot index and generation). For C the reclaimer counts the edges among
   the nodes of C and subtracts: the remainder is the number of edges that
   arrive from outside C. Unlike a centre-of-mass approximation the summary
   is exact, and the check errs only one way: a node it cannot prove
   unreachable is kept.

   The reclaimer adds no heap operation. It only chooses which existing
   [ODelete]s to issue, so the store invariant, exact block release, the
   borrow refusal and stale-link refusal are inherited from
   OwnershipGraphLinks. Who may issue them is a separate admission, checked
   by the importing OwnershipGraphRootCompleteness.

   Proved here:
   - [indeg_split], [external_is_indeg_minus_internal]: incoming = inside +
     outside, so the outside part is the stored count minus a count of C's
     edges. The reference [internal] still traverses the full slot-list spine;
     only C contributes edge payloads. This is not a physical locality theorem.
   - [trial_garbage_with_unreachable]: for every candidate set, root list,
     traversal budget and every counter that never under-counts, a node the
     check returns is unreachable from the roots. Over-counting only retains.
     The closure is a certificate the check verifies; the candidate choice
     affects only how much is found.
   - [reclaimed_forward], [reclaimed_backward], [reclaim_preserves_root_view]:
     deleting nodes unreachable from the roots keeps the store invariant,
     other stores, the reachable set, every reachable slot and every root
     link's resolution.
   - Counter maintenance on every change path. On a slot list:
     [count_after_write] (including clearing a field), [count_after_delete_other],
     [count_after_delete_self] (the identity reset), [count_after_fill]
     (reuse of a vacant slot), [count_after_append], [count_after_snapshot]
     (copy). On the graph machine: [counts_track_every_operation] keeps a
     maintained counter equal to the recount across every [GOp] (new store,
     insert into a vacant or a new slot, write, delete, borrow begin and end,
     drop), under [EdgesIssued] and [op_admitted]. Edges may name past
     generations, but not future or unissued vacant-current identities. Moving a
     node between stores is not an operation of OwnershipGraphLinks; a
     future move must be delete plus insert or bring its own count lemma.
   - Witnesses: the isolated cycle of [unreachable_nodes_retained] is
     reclaimed (heap 5 -> 3 blocks); a pure in-count test does not free it;
     a node held from outside C is kept; dropping the closure step deletes a
     reachable node; a too small budget defers; a forgotten root becomes a
     stale refusal; an insert naming a reclaimed raw index is visible; and a
     counter that decrements by slot index after reuse under-counts and
     deletes a reachable node ([index_decrement_after_reuse_deletes_reachable]).

   - [counted_gstep], [counted_grun]: package sequential program transitions
     with their count deltas. [counted_delete_batch], [counted_reclaim] are
     checked atomic model boundaries: a refusal returns the exact original
     whole counted state and its reason; an accepted batch projects to the
     sequential runner and preserves exact counts. This is not native rollback.

   Boundary: [roots] is a premise here. OwnershipGraphRootCompleteness binds
   it to a checked program's live values and frames. Counter storage,
   overflow and subtraction in the implementation, cross-store cycles,
   finalizers with observable effects, concurrency and the cost of
   maintaining counts are outside this model. This is a store-local,
   explicitly invoked model policy for graph stores, not an adopted collector
   for ordinary values. [fuel] bounds closure rounds, not measured work. *)
Require Import Stdlib.Lists.List Stdlib.Arith.PeanoNat Stdlib.Bool.Bool.
Require Import Stdlib.micromega.Lia.
Require Import OwnershipCleanCore OwnershipGraphLinks.
Import ListNotations.

(* ------------------------------------------------------------------ *)
(* Membership                                                           *)
(* ------------------------------------------------------------------ *)

Definition inb (x : nat) (l : list nat) : bool := existsb (Nat.eqb x) l.

Lemma inb_spec : forall x l, inb x l = true <-> In x l.
Proof.
  intros x l. unfold inb. rewrite existsb_exists. split.
  - intros [y [Hy E]]. apply Nat.eqb_eq in E. subst y. exact Hy.
  - intros H. exists x. split; [exact H| apply Nat.eqb_refl].
Qed.

Lemma inb_false : forall x l, inb x l = false <-> ~ In x l.
Proof.
  intros x l. split.
  - intros E H. apply inb_spec in H. congruence.
  - intros H. destruct (inb x l) eqn:E; [apply inb_spec in E; contradiction| reflexivity].
Qed.

(* ------------------------------------------------------------------ *)
(* Counts of edges that name a current identity                         *)
(* ------------------------------------------------------------------ *)

Definition slot_edges (sl : Slot) : list Edge :=
  match snode sl with Some nd => nedges nd | None => [] end.

Definition gen_at (sl : list Slot) (k : nat) : nat :=
  match nth_error sl k with Some x => sgen x | None => 0 end.

(* Edges that name identity (k, gk). *)
Definition edge_count (k gk : nat) (es : list Edge) : nat :=
  length (filter (fun e => Nat.eqb (fst e) k && Nat.eqb (snd e) gk) es).

(* Such edges from the slots j (numbered from j0) with P j. *)
Fixpoint incoming_from (P : nat -> bool) (k gk j0 : nat) (sl : list Slot) : nat :=
  match sl with
  | [] => 0
  | x :: r => (if P j0 then edge_count k gk (slot_edges x) else 0) + incoming_from P k gk (S j0) r
  end.

Definition indeg (sl : list Slot) (k : nat) : nat := incoming_from (fun _ => true) k (gen_at sl k) 0 sl.
Definition internal (C : list nat) (sl : list Slot) (k : nat) : nat :=
  incoming_from (fun j => inb j C) k (gen_at sl k) 0 sl.
Definition external (C : list nat) (sl : list Slot) (k : nat) : nat :=
  incoming_from (fun j => negb (inb j C)) k (gen_at sl k) 0 sl.

Lemma incoming_split : forall P k gk sl j0,
  incoming_from (fun _ => true) k gk j0 sl =
  incoming_from P k gk j0 sl + incoming_from (fun j => negb (P j)) k gk j0 sl.
Proof.
  intros P k gk sl. induction sl as [|x r IH]; intros j0; simpl; [reflexivity|].
  rewrite (IH (S j0)). destruct (P j0); simpl; lia.
Qed.

Theorem indeg_split : forall C sl k, indeg sl k = internal C sl k + external C sl k.
Proof. intros C sl k. apply (incoming_split (fun j => inb j C)). Qed.

(* The outside contribution is the stored total minus a count that only
   visits the candidate nodes. *)
Theorem external_is_indeg_minus_internal : forall C sl k,
  external C sl k = indeg sl k - internal C sl k.
Proof. intros C sl k. rewrite (indeg_split C sl k). lia. Qed.

Lemma edge_count_pos : forall k gk es e, In e es -> fst e = k -> snd e = gk -> edge_count k gk es <> 0.
Proof.
  intros k gk es e He Ek Eg H. unfold edge_count in H.
  assert (Hin : In e (filter (fun e => Nat.eqb (fst e) k && Nat.eqb (snd e) gk) es)).
  { apply filter_In. split; [exact He|]. rewrite Ek, Eg, !Nat.eqb_refl. reflexivity. }
  destruct (filter (fun e => Nat.eqb (fst e) k && Nat.eqb (snd e) gk) es); [contradiction| discriminate].
Qed.

Lemma edge_count_zero_of : forall k gk es,
  (forall e, In e es -> fst e = k -> snd e <> gk) -> edge_count k gk es = 0.
Proof.
  intros k gk es. induction es as [|e r IH]; intros H; [reflexivity|]. unfold edge_count in *. simpl.
  destruct (Nat.eqb (fst e) k) eqn:E1; destruct (Nat.eqb (snd e) gk) eqn:E2; simpl;
    try (apply IH; intros e' He'; apply H; right; exact He').
  exfalso. apply Nat.eqb_eq in E1, E2. exact (H e (or_introl eq_refl) E1 E2).
Qed.

Lemma incoming_zero_no_edge : forall P k gk sl j0, incoming_from P k gk j0 sl = 0 ->
  forall i x e, nth_error sl i = Some x -> P (j0 + i) = true -> In e (slot_edges x) ->
  fst e = k -> snd e <> gk.
Proof.
  intros P k gk sl. induction sl as [|y r IH]; intros j0 H i x e Hx HP He Ek Eg; [destruct i; discriminate|].
  simpl in H. destruct i as [|i]; simpl in Hx.
  - injection Hx as Hx. subst y. replace (j0 + 0) with j0 in HP by lia. rewrite HP in H.
    apply (edge_count_pos k gk (slot_edges x) e He Ek Eg). lia.
  - apply (IH (S j0)) with (i := i) (x := x) (e := e); try assumption; [lia|].
    replace (S j0 + i) with (j0 + S i) by lia. exact HP.
Qed.

Lemma incoming_zero : forall P k gk sl j0,
  (forall i x e, nth_error sl i = Some x -> In e (slot_edges x) -> fst e = k -> snd e <> gk) ->
  incoming_from P k gk j0 sl = 0.
Proof.
  intros P k gk sl. induction sl as [|y r IH]; intros j0 H; simpl; [reflexivity|].
  rewrite (IH (S j0)); [|intros i x e Hx; exact (H (S i) x e Hx)].
  rewrite (edge_count_zero_of k gk (slot_edges y)); [destruct (P j0); reflexivity|].
  intros e He. exact (H 0 y e eq_refl He).
Qed.

(* No outside edge: no live node outside C names the identity of k. *)
Lemma external_zero_no_edge : forall C sl k, external C sl k = 0 ->
  forall j x e, nth_error sl j = Some x -> ~ In j C -> In e (slot_edges x) ->
  fst e = k -> snd e <> gen_at sl k.
Proof.
  intros C sl k H j x e Hx Hj He. apply (incoming_zero_no_edge _ k _ sl 0 H j x e Hx); [|exact He].
  simpl. apply negb_true_iff. apply inb_false. exact Hj.
Qed.

(* ------------------------------------------------------------------ *)
(* Reachability from the roots                                          *)
(* ------------------------------------------------------------------ *)

(* A slot is reachable when a root link resolves to it, or when an edge of a
   reachable live node resolves to it. *)
Inductive reach (s : Store) (roots : list Link) : nat -> Prop :=
| RRoot : forall l nd, In l roots -> lsid l = ssid s -> follow s (lidx l, lgen l) = Some nd ->
    reach s roots (lidx l)
| REdge : forall j gen nd e nd', reach s roots j ->
    nth_error (sslots s) j = Some (mkSlot gen (Some nd)) -> In e (nedges nd) ->
    follow s e = Some nd' -> reach s roots (fst e).

Lemma follow_gen : forall s e nd, follow s e = Some nd ->
  exists t, nth_error (sslots s) (fst e) = Some t /\ sgen t = snd e /\ snode t = Some nd.
Proof.
  intros s e nd H. unfold follow in H. destruct (nth_error (sslots s) (fst e)) as [t|] eqn:E; [|discriminate].
  destruct (Nat.eqb (sgen t) (snd e)) eqn:Eg; [|discriminate].
  exists t. split; [reflexivity|]. split; [apply Nat.eqb_eq; exact Eg| exact H].
Qed.

(* Adding roots never removes reachability. *)
Lemma reach_mono : forall s R R', (forall l, In l R -> In l R') ->
  forall j, reach s R j -> reach s R' j.
Proof.
  intros s R R' Hi j Hr. induction Hr as [l nd Hl Hs Hf| j gen nd e nd' Hr IH Hj He Hf].
  - exact (RRoot s R' l nd (Hi l Hl) Hs Hf).
  - exact (REdge s R' j gen nd e nd' IH Hj He Hf).
Qed.

(* A root that only names something already reachable adds nothing. *)
Lemma reach_cover : forall s R R',
  (forall l nd, In l R' -> lsid l = ssid s -> follow s (lidx l, lgen l) = Some nd -> reach s R (lidx l)) ->
  forall j, reach s R' j -> reach s R j.
Proof.
  intros s R R' Hc j Hr. induction Hr as [l nd Hl Hs Hf| j gen nd e nd' Hr IH Hj He Hf].
  - exact (Hc l nd Hl Hs Hf).
  - exact (REdge s R j gen nd e nd' IH Hj He Hf).
Qed.

(* ------------------------------------------------------------------ *)
(* The local check                                                      *)
(* ------------------------------------------------------------------ *)

(* A root names slot i of the store (generation ignored: conservative). *)
Definition rooted (sid : nat) (roots : list Link) (i : nat) : bool :=
  existsb (fun l => Nat.eqb (lsid l) sid && Nat.eqb (lidx l) i) roots.

(* Candidates held from outside: by a root, or because the visited inside
   count is below the counter. *)
Definition held_with (cnt : nat -> nat) (s : Store) (roots : list Link) (C : list nat) : list nat :=
  filter (fun i => rooted (ssid s) roots i || Nat.ltb (internal C (sslots s) i) (cnt i)) C.

Definition succs (s : Store) (j : nat) : list nat :=
  match nth_error (sslots s) j with Some sl => map fst (slot_edges sl) | None => [] end.

Definition close_step (s : Store) (C L : list nat) : list nat :=
  L ++ filter (fun k => inb k C && negb (inb k L)) (flat_map (succs s) L).

Fixpoint close (fuel : nat) (s : Store) (C L : list nat) : list nat :=
  match fuel with 0 => L | S f => close f s C (close_step s C L) end.

(* Certificate checks: L contains the held candidates and is closed under the
   edges that stay inside C. *)
Definition covers (H L : list nat) : bool := forallb (fun i => inb i L) H.
Definition closed_in (s : Store) (C L : list nat) : bool :=
  forallb (fun j => forallb (fun k => negb (inb k C) || inb k L) (succs s j)) L.

(* None means "not decided within the budget": nothing is reclaimed. *)
Definition trial_garbage_with (cnt : nat -> nat) (fuel : nat) (s : Store) (roots : list Link)
    (C : list nat) : option (list nat) :=
  let H := held_with cnt s roots C in
  let L := close fuel s C H in
  if covers H L && closed_in s C L then Some (filter (fun i => negb (inb i L)) C) else None.

(* The check with the exact recount. *)
Definition trial_garbage (fuel : nat) (s : Store) (roots : list Link) (C : list nat) : option (list nat) :=
  trial_garbage_with (indeg (sslots s)) fuel s roots C.

Lemma trial_garbage_with_spec : forall cnt fuel s roots C G,
  trial_garbage_with cnt fuel s roots C = Some G ->
  exists L, (forall i, In i (held_with cnt s roots C) -> In i L) /\
    (forall j k, In j L -> In k (succs s j) -> In k C -> In k L) /\
    (forall i, In i G <-> In i C /\ ~ In i L).
Proof.
  intros cnt fuel s roots C G E. unfold trial_garbage_with in E. cbv zeta in E.
  set (L := close fuel s C (held_with cnt s roots C)) in E.
  destruct (covers (held_with cnt s roots C) L && closed_in s C L) eqn:Ec; [|discriminate].
  injection E as E. subst G. apply andb_true_iff in Ec. destruct Ec as [Hcov Hcl].
  exists L. split; [|split].
  - intros i Hi. unfold covers in Hcov. rewrite forallb_forall in Hcov.
    apply inb_spec. exact (Hcov i Hi).
  - intros j k Hj Hk HkC. unfold closed_in in Hcl. rewrite forallb_forall in Hcl.
    pose proof (Hcl j Hj) as Hj'. rewrite forallb_forall in Hj'.
    pose proof (Hj' k Hk) as Hk'. apply orb_true_iff in Hk'. destruct Hk' as [Hn|Hl].
    + apply negb_true_iff, inb_false in Hn. contradiction.
    + apply inb_spec. exact Hl.
  - intros i. rewrite filter_In. split.
    + intros [Hi Hn]. split; [exact Hi|]. apply negb_true_iff, inb_false in Hn. exact Hn.
    + intros [Hi Hn]. split; [exact Hi|]. apply negb_true_iff, inb_false. exact Hn.
Qed.

Lemma rooted_true : forall sid roots l, In l roots -> lsid l = sid -> rooted sid roots (lidx l) = true.
Proof.
  intros sid roots l Hl Hs. unfold rooted. apply existsb_exists. exists l. split; [exact Hl|].
  rewrite Hs, !Nat.eqb_refl. reflexivity.
Qed.

(* Soundness: whatever the candidate set and budget, and with any counter
   that never under-counts, a returned node is not reachable from the
   roots. *)
Theorem trial_garbage_with_unreachable : forall cnt fuel s roots C G,
  (forall k, indeg (sslots s) k <= cnt k) ->
  trial_garbage_with cnt fuel s roots C = Some G -> forall j, reach s roots j -> ~ In j G.
Proof.
  intros cnt fuel s roots C G Hc E. destruct (trial_garbage_with_spec _ _ _ _ _ _ E) as [L [Hcov [Hcl HG]]].
  assert (Key : forall j, reach s roots j -> In j C -> In j L).
  { intros j Hr. induction Hr as [l nd Hl Hs Hf| j gen nd e nd' Hr IH Hj He Hf]; intros HC.
    - apply Hcov. apply filter_In. split; [exact HC|].
      rewrite (rooted_true _ _ _ Hl Hs). reflexivity.
    - destruct (in_dec Nat.eq_dec j C) as [HjC|HjC].
      + apply (Hcl j); [exact (IH HjC)| |exact HC].
        unfold succs. rewrite Hj. simpl. apply in_map. exact He.
      + apply Hcov. apply filter_In. split; [exact HC|]. apply orb_true_iff. right.
        apply Nat.ltb_lt. pose proof (Hc (fst e)) as Hce.
        rewrite (indeg_split C (sslots s) (fst e)) in Hce.
        destruct (follow_gen s e nd' Hf) as [t [Ht [Hg _]]].
        assert (Hx : external C (sslots s) (fst e) <> 0).
        { intro Z. apply (external_zero_no_edge C (sslots s) (fst e) Z j (mkSlot gen (Some nd)) e Hj HjC He eq_refl).
          unfold gen_at. rewrite Ht. symmetry. exact Hg. }
        lia. }
  intros j Hr Hin. apply HG in Hin. destruct Hin as [HC Hn]. exact (Hn (Key j Hr HC)).
Qed.

Theorem trial_garbage_unreachable : forall fuel s roots C G,
  trial_garbage fuel s roots C = Some G -> forall j, reach s roots j -> ~ In j G.
Proof. intros fuel s roots C G. apply trial_garbage_with_unreachable. intros k. lia. Qed.

(* A maintained counter equal to the recount gives the same decision. *)
Theorem trial_with_exact_counter : forall cnt fuel s roots C,
  (forall k, cnt k = indeg (sslots s) k) ->
  trial_garbage_with cnt fuel s roots C = trial_garbage fuel s roots C.
Proof.
  intros cnt fuel s roots C Hc. unfold trial_garbage, trial_garbage_with, held_with.
  assert (E : filter (fun i => rooted (ssid s) roots i || Nat.ltb (internal C (sslots s) i) (cnt i)) C =
              filter (fun i => rooted (ssid s) roots i || Nat.ltb (internal C (sslots s) i) (indeg (sslots s) i)) C).
  { apply filter_ext. intros i. rewrite Hc. reflexivity. }
  rewrite E. reflexivity.
Qed.

(* ------------------------------------------------------------------ *)
(* Deleting unreachable nodes                                           *)
(* ------------------------------------------------------------------ *)

(* s' is s with some slots of G deleted, each at most once. *)
Definition reclaimed (G : list nat) (s s' : Store) : Prop :=
  ssid s' = ssid s /\ stab s' = stab s /\
  forall j, nth_error (sslots s') j = nth_error (sslots s) j \/
    (In j G /\ exists gen nd, nth_error (sslots s) j = Some (mkSlot gen (Some nd)) /\
                         nth_error (sslots s') j = Some (mkSlot (S gen) None)).

Lemma reclaimed_refl : forall G s, reclaimed G s s.
Proof. intros G s. split; [reflexivity| split; [reflexivity| intros j; left; reflexivity]]. Qed.

Lemma follow_kept : forall G s s' e, reclaimed G s s' -> ~ In (fst e) G -> follow s' e = follow s e.
Proof.
  intros G s s' e [_ [_ R]] Hn. unfold follow. destruct (R (fst e)) as [E|[Hin _]]; [rewrite E; reflexivity|].
  contradiction.
Qed.

Lemma follow_vacant : forall s e g, nth_error (sslots s) (fst e) = Some (mkSlot g None) -> follow s e = None.
Proof. intros s e g H. unfold follow. rewrite H. simpl. destruct (Nat.eqb g (snd e)); reflexivity. Qed.

Lemma follow_back : forall G s s' e nd, reclaimed G s s' -> follow s' e = Some nd -> follow s e = Some nd.
Proof.
  intros G s s' e nd [_ [_ R]] Hf. destruct (R (fst e)) as [E|[_ [gen [nd0 [E0 E1]]]]].
  - unfold follow in *. rewrite <- E. exact Hf.
  - rewrite (follow_vacant s' e (S gen) E1) in Hf. discriminate.
Qed.

(* Deleting slots no root reaches keeps everything the roots reach. *)
Theorem reclaimed_forward : forall G s s' roots, reclaimed G s s' ->
  (forall j, reach s roots j -> ~ In j G) ->
  forall j, reach s roots j -> reach s' roots j /\ nth_error (sslots s') j = nth_error (sslots s) j.
Proof.
  intros G s s' roots Rc U j Hr.
  assert (Slot : forall j, reach s roots j -> nth_error (sslots s') j = nth_error (sslots s) j).
  { intros j0 H0. destruct Rc as [_ [_ R]]. destruct (R j0) as [E|[Hin _]]; [exact E|].
    exfalso. exact (U j0 H0 Hin). }
  split; [|exact (Slot j Hr)].
  induction Hr as [l nd Hl Hs Hf| j gen nd e nd' Hr IH Hj He Hf].
  - apply (RRoot s' roots l nd Hl); [destruct Rc as [Es _]; rewrite Es; exact Hs|].
    rewrite (follow_kept G s s' (lidx l, lgen l) Rc); [exact Hf|].
    exact (U _ (RRoot s roots l nd Hl Hs Hf)).
  - apply (REdge s' roots j gen nd e nd' IH); [rewrite (Slot j Hr); exact Hj| exact He|].
    rewrite (follow_kept G s s' e Rc); [exact Hf|].
    exact (U _ (REdge s roots j gen nd e nd' Hr Hj He Hf)).
Qed.

(* Deletion never makes a slot reachable. *)
Theorem reclaimed_backward : forall G s s' roots, reclaimed G s s' ->
  forall j, reach s' roots j -> reach s roots j.
Proof.
  intros G s s' roots Rc j Hr. induction Hr as [l nd Hl Hs Hf| j gen nd e nd' Hr IH Hj He Hf].
  - apply (RRoot s roots l nd Hl); [destruct Rc as [Es _]; rewrite <- Es; exact Hs|].
    exact (follow_back G s s' _ nd Rc Hf).
  - assert (Hj' : nth_error (sslots s) j = Some (mkSlot gen (Some nd))).
    { destruct Rc as [_ [_ R]]. destruct (R j) as [Ej|[_ [g0 [n0 [E0 E1]]]]]; [rewrite <- Ej; exact Hj|].
      rewrite E1 in Hj. discriminate. }
    exact (REdge s roots j gen nd e nd' IH Hj' He (follow_back G s s' e nd' Rc Hf)).
Qed.

Theorem reclaim_preserves_reach : forall fuel s roots C G s',
  trial_garbage fuel s roots C = Some G -> reclaimed G s s' ->
  forall j, reach s roots j <-> reach s' roots j.
Proof.
  intros fuel s roots C G s' E Rc j. split.
  - intros Hr. exact (proj1 (reclaimed_forward G s s' roots Rc (trial_garbage_unreachable _ _ _ _ _ E) j Hr)).
  - exact (reclaimed_backward G s s' roots Rc j).
Qed.

Theorem reclaim_keeps_root_resolution : forall cnt fuel s roots C G s',
  trial_garbage_with cnt fuel s roots C = Some G -> reclaimed G s s' ->
  forall l, In l roots -> lsid l = ssid s -> follow s' (lidx l, lgen l) = follow s (lidx l, lgen l).
Proof.
  intros cnt fuel s roots C G s' E Rc l Hl Hs. apply (follow_kept G s s' _ Rc). simpl.
  destruct (trial_garbage_with_spec _ _ _ _ _ _ E) as [L [Hcov [_ HG]]]. intro Hin.
  apply HG in Hin. destruct Hin as [HC Hn]. apply Hn. apply Hcov. apply filter_In.
  split; [exact HC|]. rewrite (rooted_true _ _ _ Hl Hs). reflexivity.
Qed.

Definition slot_gen (s : Store) (i : nat) : nat := gen_at (sslots s) i.

(* The reclaimer only issues existing deletes. *)
Definition reclaim_ops (s : Store) (G : list nat) : list GOp :=
  map (fun i => ODelete (mkLink (ssid s) i (slot_gen s i))) G.

Lemma delete_step : forall G s0 g s1 i, find_store (ssid s0) (gstores g) = Some s1 ->
  reclaimed G s0 s1 -> In i G ->
  exists s2, find_store (ssid s0) (gstores (fst (g_delete g (mkLink (ssid s0) i (slot_gen s0 i))))) = Some s2 /\
    reclaimed G s0 s2.
Proof.
  intros G s0 g s1 i Ef Rc Hi. unfold g_delete. simpl. rewrite Ef.
  destruct (nth_error (sslots s1) i) as [[gen [nd|]]|] eqn:Ei; try (exists s1; split; assumption).
  destruct (Nat.eqb gen (slot_gen s0 i)); [|exists s1; split; assumption].
  destruct (existsb (same_place (ssid s0) i) (gbor g)); [exists s1; split; assumption|]. simpl.
  destruct (find_store_some _ _ _ Ef) as [_ Hsid].
  set (s2 := mkStore (ssid s1) (stab s1) (upd (sslots s1) i (mkSlot (S gen) None))).
  assert (Ef2 : find_store (ssid s2) (gstores g) = Some s1) by (simpl; rewrite Hsid; exact Ef).
  exists s2. split.
  - rewrite <- Hsid. change (find_store (ssid s2) (set_store s2 (gstores g)) = Some s2).
    exact (find_set_same _ _ _ Ef2).
  - destruct Rc as [Es [Et R]]. split; [simpl; rewrite Hsid; reflexivity|]. split; [simpl; exact Et|]. intros j. simpl.
    destruct (Nat.eq_dec j i) as [->|Hne].
    + right. split; [exact Hi|]. exists gen, nd. split.
      * destruct (R i) as [E|[_ [g0 [n0 [E0 E1]]]]]; [rewrite <- E; exact Ei|].
        rewrite E1 in Ei. discriminate.
      * apply nth_upd_same. apply nth_error_Some. congruence.
    + rewrite (nth_upd_other _ _ _ _ _ Hne). exact (R j).
Qed.

Lemma delete_other_store : forall g sid i gen sid', sid' <> sid ->
  find_store sid' (gstores (fst (g_delete g (mkLink sid i gen)))) = find_store sid' (gstores g).
Proof.
  intros g sid i gen sid' Hne. unfold g_delete. simpl.
  destruct (find_store sid (gstores g)) as [s|] eqn:Ef; [|reflexivity].
  destruct (nth_error (sslots s) i) as [[g0 [nd|]]|]; try reflexivity.
  destruct (Nat.eqb g0 gen); [|reflexivity].
  destruct (existsb (same_place sid i) (gbor g)); [reflexivity|]. simpl.
  destruct (find_store_some _ _ _ Ef) as [_ Hsid].
  apply find_set_other. simpl. rewrite Hsid. exact Hne.
Qed.

Lemma reclaim_run : forall gmax G s0 G0 g s1, (forall i, In i G0 -> In i G) ->
  find_store (ssid s0) (gstores g) = Some s1 -> reclaimed G s0 s1 ->
  exists s2, find_store (ssid s0) (gstores (grun gmax g (reclaim_ops s0 G0))) = Some s2 /\
    reclaimed G s0 s2.
Proof.
  intros gmax G s0 G0. induction G0 as [|i r IH]; intros g s1 Hsub Ef Rc; simpl; [exists s1; split; assumption|].
  destruct (delete_step G s0 g s1 i Ef Rc (Hsub i (or_introl eq_refl))) as [s2 [Ef2 Rc2]].
  apply (IH _ s2); [intros x Hx; apply Hsub; right; exact Hx| exact Ef2| exact Rc2].
Qed.

Lemma reclaim_run_other : forall gmax s0 G0 g sid', sid' <> ssid s0 ->
  find_store sid' (gstores (grun gmax g (reclaim_ops s0 G0))) = find_store sid' (gstores g).
Proof.
  intros gmax s0 G0. induction G0 as [|i r IH]; intros g sid' Hne; simpl; [reflexivity|].
  rewrite (IH _ sid' Hne). apply delete_other_store. exact Hne.
Qed.

(* The whole reclamation step in the graph machine, with any counter that
   never under-counts. *)
Theorem reclaim_preserves_root_view : forall gmax g sid s cnt fuel roots C G,
  GInv gmax g -> find_store sid (gstores g) = Some s ->
  (forall k, indeg (sslots s) k <= cnt k) ->
  trial_garbage_with cnt fuel s roots C = Some G ->
  let g' := grun gmax g (reclaim_ops s G) in
  GInv gmax g' /\
  (forall sid', sid' <> sid -> find_store sid' (gstores g') = find_store sid' (gstores g)) /\
  exists s', find_store sid (gstores g') = Some s' /\ reclaimed G s s' /\
    (forall j, reach s roots j <-> reach s' roots j) /\
    (forall j, reach s roots j -> nth_error (sslots s') j = nth_error (sslots s) j) /\
    (forall l, In l roots -> lsid l = sid -> follow s' (lidx l, lgen l) = follow s (lidx l, lgen l)).
Proof.
  intros gmax g sid s cnt fuel roots C G Hinv Ef Hc E g'.
  destruct (find_store_some _ _ _ Ef) as [_ Hsid]. subst sid.
  pose proof (trial_garbage_with_unreachable _ _ _ _ _ _ Hc E) as U.
  split; [apply ginv_run; exact Hinv|]. split.
  { intros sid' Hne. apply reclaim_run_other. exact Hne. }
  destruct (reclaim_run gmax G s G g s (fun i Hi => Hi) Ef (reclaimed_refl G s)) as [s' [Ef' Rc]].
  exists s'. split; [exact Ef'|]. split; [exact Rc|]. split.
  { intros j. split; [intros Hr; exact (proj1 (reclaimed_forward G s s' roots Rc U j Hr))|].
    exact (reclaimed_backward G s s' roots Rc j). }
  split; [intros j Hr; exact (proj2 (reclaimed_forward G s s' roots Rc U j Hr))|].
  exact (reclaim_keeps_root_resolution _ _ _ _ _ _ _ E Rc).
Qed.

(* ------------------------------------------------------------------ *)
(* Counter maintenance on every change path                             *)
(* ------------------------------------------------------------------ *)

(* An edge names a slot that exists, and neither a generation the slot has
   not reached nor the current generation of a vacant slot. *)
Definition edge_ok (sl : list Slot) (e : Edge) : Prop :=
  exists t, nth_error sl (fst e) = Some t /\ (snd e < sgen t \/ (snd e = sgen t /\ snode t <> None)).

Definition EdgesIssued (sl : list Slot) : Prop :=
  forall i x e, nth_error sl i = Some x -> In e (slot_edges x) -> edge_ok sl e.

Lemma incoming_upd : forall P k gk sl j0 i x y, nth_error sl i = Some x ->
  incoming_from P k gk j0 (upd sl i y) + (if P (j0 + i) then edge_count k gk (slot_edges x) else 0) =
  incoming_from P k gk j0 sl + (if P (j0 + i) then edge_count k gk (slot_edges y) else 0).
Proof.
  intros P k gk sl. induction sl as [|a r IH]; intros j0 i x y Hx; [destruct i; discriminate|].
  destruct i as [|i]; simpl in Hx |- *.
  - injection Hx as Hx. subst a. replace (j0 + 0) with j0 by lia. destruct (P j0); lia.
  - pose proof (IH (S j0) i x y Hx) as E. replace (S j0 + i) with (j0 + S i) in E by lia. lia.
Qed.

Lemma incoming_snoc : forall P k gk sl j0 y,
  incoming_from P k gk j0 (sl ++ [y]) =
  incoming_from P k gk j0 sl + (if P (j0 + length sl) then edge_count k gk (slot_edges y) else 0).
Proof.
  intros P k gk sl. induction sl as [|a r IH]; intros j0 y; simpl.
  - replace (j0 + 0) with j0 by lia. destruct (P j0); lia.
  - rewrite (IH (S j0) y). replace (S j0 + length r) with (j0 + S (length r)) by lia. lia.
Qed.

Lemma gen_at_upd_same_gen : forall sl j x y, nth_error sl j = Some x -> sgen y = sgen x ->
  forall k, gen_at (upd sl j y) k = gen_at sl k.
Proof.
  intros sl j x y Hx Hg k. unfold gen_at. destruct (Nat.eq_dec k j) as [->|Hne].
  - rewrite nth_upd_same by (apply nth_error_Some; congruence). rewrite Hx. exact Hg.
  - rewrite (nth_upd_other _ _ _ _ _ Hne). reflexivity.
Qed.

(* Replacing a node's data and edges, including clearing edge fields. *)
Theorem count_after_write : forall sl j x y, nth_error sl j = Some x -> sgen y = sgen x ->
  forall k, indeg (upd sl j y) k + edge_count k (gen_at sl k) (slot_edges x) =
            indeg sl k + edge_count k (gen_at sl k) (slot_edges y).
Proof.
  intros sl j x y Hx Hg k. unfold indeg. rewrite (gen_at_upd_same_gen sl j x y Hx Hg k).
  exact (incoming_upd (fun _ => true) k _ sl 0 j x y Hx).
Qed.

(* Deleting slot j: every other identity loses the deleted node's edges. *)
Theorem count_after_delete_other : forall sl j x, nth_error sl j = Some x ->
  forall k, k <> j ->
  indeg (upd sl j (mkSlot (S (sgen x)) None)) k + edge_count k (gen_at sl k) (slot_edges x) = indeg sl k.
Proof.
  intros sl j x Hx k Hk. unfold indeg.
  assert (Eg : gen_at (upd sl j (mkSlot (S (sgen x)) None)) k = gen_at sl k)
    by (unfold gen_at; rewrite (nth_upd_other _ _ _ _ _ Hk); reflexivity).
  rewrite Eg. pose proof (incoming_upd (fun _ => true) k (gen_at sl k) sl 0 j x (mkSlot (S (sgen x)) None) Hx) as E.
  cbv beta iota in E.
  assert (Z : edge_count k (gen_at sl k) (slot_edges (mkSlot (S (sgen x)) None)) = 0) by reflexivity.
  rewrite Z in E. lia.
Qed.

(* The identity reset: the deleted slot's new identity has no incoming
   edge, so its counter restarts at zero instead of keeping the old
   generation's count. *)
Theorem count_after_delete_self : forall sl j x, EdgesIssued sl -> nth_error sl j = Some x ->
  indeg (upd sl j (mkSlot (S (sgen x)) None)) j = 0.
Proof.
  intros sl j x HE Hx. unfold indeg.
  assert (Eg : gen_at (upd sl j (mkSlot (S (sgen x)) None)) j = S (sgen x))
    by (unfold gen_at; rewrite nth_upd_same by (apply nth_error_Some; congruence); reflexivity).
  rewrite Eg. apply incoming_zero. intros i y e Hy He Ek.
  destruct (Nat.eq_dec i j) as [->|Hne].
  - rewrite nth_upd_same in Hy by (apply nth_error_Some; congruence). injection Hy as Hy. subst y.
    simpl in He. contradiction.
  - rewrite (nth_upd_other _ _ _ _ _ Hne) in Hy. destruct (HE i y e Hy He) as [t [Ht Hg]].
    rewrite Ek, Hx in Ht. injection Ht as Ht. subst t. lia.
Qed.

(* Filling a vacant slot (reuse) adds the new node's edges. *)
Theorem count_after_fill : forall sl j g nd, nth_error sl j = Some (mkSlot g None) ->
  forall k, indeg (upd sl j (mkSlot g (Some nd))) k = indeg sl k + edge_count k (gen_at sl k) (nedges nd).
Proof.
  intros sl j g nd Hx k. unfold indeg.
  rewrite (gen_at_upd_same_gen sl j (mkSlot g None) (mkSlot g (Some nd)) Hx eq_refl k).
  pose proof (incoming_upd (fun _ => true) k (gen_at sl k) sl 0 j (mkSlot g None) (mkSlot g (Some nd)) Hx) as E.
  cbv beta iota in E.
  assert (Z : edge_count k (gen_at sl k) (slot_edges (mkSlot g None)) = 0) by reflexivity.
  change (slot_edges (mkSlot g (Some nd))) with (nedges nd) in E. rewrite Z in E. lia.
Qed.

Theorem count_vacant_zero : forall sl j g, EdgesIssued sl -> nth_error sl j = Some (mkSlot g None) ->
  indeg sl j = 0.
Proof.
  intros sl j g HE Hx. unfold indeg, gen_at. rewrite Hx. simpl. apply incoming_zero.
  intros i y e Hy He Ek. destruct (HE i y e Hy He) as [t [Ht Hg]]. rewrite Ek, Hx in Ht.
  injection Ht as Ht. subst t. simpl in Hg. destruct Hg as [Hl|[_ Hn]]; [lia| contradiction].
Qed.

(* Appending a node (a growing insert) adds its edges. *)
Theorem count_after_append : forall sl nd k,
  indeg (sl ++ [mkSlot 0 (Some nd)]) k = indeg sl k + edge_count k (gen_at sl k) (nedges nd).
Proof.
  intros sl nd k. unfold indeg.
  assert (Eg : gen_at (sl ++ [mkSlot 0 (Some nd)]) k = gen_at sl k).
  { unfold gen_at. destruct (Nat.lt_ge_cases k (length sl)) as [Hl|Hg].
    - rewrite nth_error_app1 by exact Hl. reflexivity.
    - rewrite nth_error_app2 by exact Hg. rewrite (proj2 (nth_error_None sl k) Hg).
      destruct (k - length sl) as [|m]; [reflexivity|]. simpl. destruct m; reflexivity. }
  rewrite Eg, incoming_snoc. reflexivity.
Qed.

Lemma incoming_copy : forall P k gk n sl j j0,
  incoming_from P k gk j0 (copy_slots n j sl) = incoming_from P k gk j0 sl.
Proof.
  intros P k gk n sl. induction sl as [|x r IH]; intros j j0; simpl; [reflexivity|].
  rewrite IH. destruct x as [g [nd|]]; reflexivity.
Qed.

Lemma gen_at_copy : forall n sl j k, gen_at (copy_slots n j sl) k = gen_at sl k.
Proof.
  intros n sl. unfold gen_at. induction sl as [|x r IH]; intros j [|k]; simpl; try reflexivity. apply IH.
Qed.

(* A copied store has the same counts. *)
Theorem count_after_snapshot : forall s sid' n k,
  indeg (sslots (snapshot s sid' n)) k = indeg (sslots s) k.
Proof. intros s sid' n k. unfold indeg, snapshot. simpl. rewrite gen_at_copy. apply incoming_copy. Qed.

(* ------------------------------------------------------------------ *)
(* Counters across the graph machine                                    *)
(* ------------------------------------------------------------------ *)

Definition CountsExact (cnt : nat -> nat -> nat) (g : GS) : Prop :=
  forall sid s, find_store sid (gstores g) = Some s -> forall k, cnt sid k = indeg (sslots s) k.

Definition AllEdgesIssued (g : GS) : Prop :=
  forall sid s, find_store sid (gstores g) = Some s -> EdgesIssued (sslots s).

Definition slot_edges_at (sl : list Slot) (j : nat) : list Edge :=
  match nth_error sl j with Some x => slot_edges x | None => [] end.

Definition bump (gens : nat -> nat) (cnt : nat -> nat) (added removed : list Edge) : nat -> nat :=
  fun k => cnt k + edge_count k (gens k) added - edge_count k (gens k) removed.

(* The change record a store applies after a successful operation: the
   counts of the touched store move by the old and new edges of the one
   node it touched, and a delete resets the deleted identity. *)
Definition count_update_after (g : GS) (o : GOp) (outcome : GRes)
    (cnt : nat -> nat -> nat) : nat -> nat -> nat :=
  match outcome with
  | GRefused _ => cnt
  | _ =>
    match o with
    | ONew _ => fun sid k => if Nat.eqb sid (gsid g) then 0 else cnt sid k
    | OInsert sid _ _ es _ _ =>
        match find_store sid (gstores g) with
        | Some s => fun sid' => if Nat.eqb sid' sid then bump (gen_at (sslots s)) (cnt sid) es [] else cnt sid'
        | None => cnt
        end
    | OWrite i _ es =>
        match nth_error (gbor g) i with
        | Some b =>
            match find_store (bsid b) (gstores g) with
            | Some s => fun sid' => if Nat.eqb sid' (bsid b)
                then bump (gen_at (sslots s)) (cnt (bsid b)) es (slot_edges_at (sslots s) (bidx b))
                else cnt sid'
            | None => cnt
            end
        | None => cnt
        end
    | ODelete l =>
        match find_store (lsid l) (gstores g) with
        | Some s => fun sid' => if Nat.eqb sid' (lsid l)
            then fun k => if Nat.eqb k (lidx l) then 0
                          else bump (gen_at (sslots s)) (cnt (lsid l)) [] (slot_edges_at (sslots s) (lidx l)) k
            else cnt sid'
        | None => cnt
        end
    | _ => cnt
    end
  end.

(* The actual graph result owns success/refusal; the counted transition below
   consumes that same result instead of executing the graph operation twice. *)
Definition count_update (gmax : nat) (g : GS) (o : GOp)
    (cnt : nat -> nat -> nat) : nat -> nat -> nat :=
  count_update_after g o (snd (gexec gmax g o)) cnt.

(* New edges must name an identity that exists in the target store. The
   language produces them from issued links, which satisfy this. *)
Definition op_admitted (g : GS) (o : GOp) : Prop :=
  match o with
  | OInsert sid _ _ es _ _ => forall s, find_store sid (gstores g) = Some s -> forall e, In e es -> edge_ok (sslots s) e
  | OWrite i _ es => forall b s, nth_error (gbor g) i = Some b -> find_store (bsid b) (gstores g) = Some s ->
      forall e, In e es -> edge_ok (sslots s) e
  | _ => True
  end.

Lemma find_after_set : forall s' ss s sid, find_store (ssid s') ss = Some s ->
  find_store sid (set_store s' ss) = if Nat.eqb sid (ssid s') then Some s' else find_store sid ss.
Proof.
  intros s' ss s sid Ef. destruct (Nat.eqb sid (ssid s')) eqn:E.
  - apply Nat.eqb_eq in E. subst sid. exact (find_set_same _ _ _ Ef).
  - apply Nat.eqb_neq in E. exact (find_set_other _ _ _ E).
Qed.

Lemma edge_ok_upd_same : forall sl j x y e, nth_error sl j = Some x -> sgen y = sgen x ->
  (snode x = None <-> snode y = None) -> edge_ok sl e -> edge_ok (upd sl j y) e.
Proof.
  intros sl j x y e Hx Hg Hn [t [Ht Hc]]. destruct (Nat.eq_dec (fst e) j) as [E|Hne].
  - exists y. rewrite E, nth_upd_same by (apply nth_error_Some; congruence). split; [reflexivity|].
    rewrite E, Hx in Ht. injection Ht as Ht. subst t. rewrite Hg.
    destruct Hc as [Hl|[He Hs]]; [left; exact Hl| right; split; [exact He|]].
    intro Hy. apply Hs. apply Hn. exact Hy.
  - exists t. rewrite (nth_upd_other _ _ _ _ _ Hne). split; [exact Ht| exact Hc].
Qed.

Lemma edges_issued_write : forall sl j x y, EdgesIssued sl -> nth_error sl j = Some x ->
  sgen y = sgen x -> (snode x = None <-> snode y = None) ->
  (forall e, In e (slot_edges y) -> edge_ok sl e) -> EdgesIssued (upd sl j y).
Proof.
  intros sl j x y HE Hx Hg Hn Hy i z e Hz He. apply (edge_ok_upd_same sl j x y e Hx Hg Hn).
  destruct (Nat.eq_dec i j) as [->|Hne].
  - rewrite nth_upd_same in Hz by (apply nth_error_Some; congruence). injection Hz as Hz. subst z.
    exact (Hy e He).
  - rewrite (nth_upd_other _ _ _ _ _ Hne) in Hz. exact (HE i z e Hz He).
Qed.

Lemma edges_issued_delete : forall sl j x, EdgesIssued sl -> nth_error sl j = Some x ->
  EdgesIssued (upd sl j (mkSlot (S (sgen x)) None)).
Proof.
  intros sl j x HE Hx i z e Hz He. destruct (Nat.eq_dec i j) as [->|Hne].
  - rewrite nth_upd_same in Hz by (apply nth_error_Some; congruence). injection Hz as Hz. subst z.
    simpl in He. contradiction.
  - rewrite (nth_upd_other _ _ _ _ _ Hne) in Hz. destruct (HE i z e Hz He) as [t [Ht Hc]].
    destruct (Nat.eq_dec (fst e) j) as [E|Hne2].
    + exists (mkSlot (S (sgen x)) None). rewrite E, nth_upd_same by (apply nth_error_Some; congruence).
      split; [reflexivity|]. left. simpl. rewrite E, Hx in Ht. injection Ht as Ht. subst t.
      destruct Hc as [Hl|[Hq _]]; lia.
    + exists t. rewrite (nth_upd_other _ _ _ _ _ Hne2). split; [exact Ht| exact Hc].
Qed.

Lemma edges_issued_fill : forall sl j g nd, EdgesIssued sl -> nth_error sl j = Some (mkSlot g None) ->
  (forall e, In e (nedges nd) -> edge_ok sl e) -> EdgesIssued (upd sl j (mkSlot g (Some nd))).
Proof.
  intros sl j g nd HE Hx Hn.
  assert (Lift : forall e, edge_ok sl e -> edge_ok (upd sl j (mkSlot g (Some nd))) e).
  { intros e [t [Ht Hc]]. destruct (Nat.eq_dec (fst e) j) as [E|Hne].
    - exists (mkSlot g (Some nd)). rewrite E, nth_upd_same by (apply nth_error_Some; congruence).
      split; [reflexivity|]. rewrite E, Hx in Ht. injection Ht as Ht. subst t. simpl in Hc |- *.
      destruct Hc as [Hl|[_ Hs]]; [left; exact Hl| contradiction].
    - exists t. rewrite (nth_upd_other _ _ _ _ _ Hne). split; [exact Ht| exact Hc]. }
  intros i z e Hz He. apply Lift. destruct (Nat.eq_dec i j) as [->|Hne].
  - rewrite nth_upd_same in Hz by (apply nth_error_Some; congruence). injection Hz as Hz. subst z.
    exact (Hn e He).
  - rewrite (nth_upd_other _ _ _ _ _ Hne) in Hz. exact (HE i z e Hz He).
Qed.

Lemma edges_issued_append : forall sl nd, EdgesIssued sl ->
  (forall e, In e (nedges nd) -> edge_ok sl e) -> EdgesIssued (sl ++ [mkSlot 0 (Some nd)]).
Proof.
  intros sl nd HE Hn.
  assert (Lift : forall e, edge_ok sl e -> edge_ok (sl ++ [mkSlot 0 (Some nd)]) e).
  { intros e [t [Ht Hc]]. exists t. split; [|exact Hc].
    rewrite nth_error_app1; [exact Ht| apply nth_error_Some; congruence]. }
  intros i z e Hz He. apply Lift. destruct (Nat.lt_ge_cases i (length sl)) as [Hl|Hg].
  - rewrite nth_error_app1 in Hz by exact Hl. exact (HE i z e Hz He).
  - rewrite nth_error_app2 in Hz by exact Hg. destruct (i - length sl) as [|m]; simpl in Hz.
    + injection Hz as Hz. subst z. exact (Hn e He).
    + destruct m; discriminate.
Qed.

Lemma slot_edges_at_some : forall sl j x, nth_error sl j = Some x -> slot_edges_at sl j = slot_edges x.
Proof. intros sl j x H. unfold slot_edges_at. rewrite H. reflexivity. Qed.

Ltac solve_refused := try (split; assumption).

(* Every machine operation keeps a maintained counter equal to the recount
   and keeps every edge naming an existing identity. *)
Theorem counts_track_every_operation : forall gmax g o cnt,
  CountsExact cnt g -> AllEdgesIssued g -> op_admitted g o ->
  CountsExact (count_update gmax g o cnt) (fst (gexec gmax g o)) /\
  AllEdgesIssued (fst (gexec gmax g o)).
Proof.
  intros gmax g o cnt HC HE HA. unfold count_update, count_update_after.
  destruct o as [tb|sid idx d es bs tb|l|l wr|i|i d es|sid]; simpl.
  - (* ONew *)
    unfold g_new. destruct (bmem tb (gheap g)); simpl; [solve_refused|]. split.
    + intros sid s Ef k. cbv beta. simpl in Ef. destruct (Nat.eqb (gsid g) sid) eqn:E.
      * apply Nat.eqb_eq in E. subst sid. rewrite Nat.eqb_refl. injection Ef as Ef. subst s. reflexivity.
      * rewrite Nat.eqb_sym, E. exact (HC sid s Ef k).
    + intros sid s Ef. simpl in Ef. destruct (Nat.eqb (gsid g) sid).
      * injection Ef as Ef. subst s. intros i x e Hx. destruct i; discriminate.
      * exact (HE sid s Ef).
  - (* OInsert *)
    unfold g_insert. destruct (find_store sid (gstores g)) as [s|] eqn:Ef; [|simpl; solve_refused].
    destruct (find_store_some _ _ _ Ef) as [_ Hsid].
    destruct (nth_error (sslots s) idx) as [sl|] eqn:Ei.
    + destruct (snode sl) as [nd0|] eqn:En; simpl; [solve_refused|].
      destruct (Nat.ltb (sgen sl) gmax); simpl; [|solve_refused].
      destruct (bnodup bs && bfree bs (gheap g)); simpl; [|solve_refused].
      assert (Hvac : nth_error (sslots s) idx = Some (mkSlot (sgen sl) None))
        by (rewrite Ei; destruct sl; simpl in En |- *; rewrite En; reflexivity).
      split.
      * intros sid' s0 E0 k. cbv beta. simpl in E0.
        rewrite (find_after_set _ _ s sid') in E0 by (simpl; exact Ef). simpl in E0.
        destruct (Nat.eqb sid' sid) eqn:Es.
        -- injection E0 as E0. subst s0. apply Nat.eqb_eq in Es. subst sid'.
           unfold bump. cbv beta. rewrite (HC sid s Ef k). cbn [sslots].
           rewrite (count_after_fill _ _ _ (mkNode d es bs) Hvac k).
           change (nedges (mkNode d es bs)) with es. change (edge_count k (gen_at (sslots s) k) []) with 0. lia.
        -- exact (HC sid' s0 E0 k).
      * intros sid' s0 E0. simpl in E0.
        rewrite (find_after_set _ _ s sid') in E0 by (simpl; exact Ef). simpl in E0.
        destruct (Nat.eqb sid' sid).
        -- injection E0 as E0. subst s0. simpl. apply (edges_issued_fill _ _ _ _ (HE sid s Ef) Hvac).
           intros e He. exact (HA s Ef e He).
        -- exact (HE sid' s0 E0).
    + destruct (store_borrowed sid (gbor g)); [simpl; solve_refused|].
      destruct (Nat.eqb idx (length (sslots s)) && Nat.ltb 0 gmax); [|simpl; solve_refused].
      destruct (bnodup (tb :: bs) && bfree (tb :: bs) (free [stab s] (gheap g))); [|simpl; solve_refused].
      simpl.
      split.
      * intros sid' s0 E0 k. cbv beta. simpl in E0.
        rewrite (find_after_set _ _ s sid') in E0 by (simpl; exact Ef). simpl in E0.
        destruct (Nat.eqb sid' sid) eqn:Es.
        -- injection E0 as E0. subst s0. apply Nat.eqb_eq in Es. subst sid'.
           unfold bump. cbv beta. rewrite (HC sid s Ef k). cbn [sslots]. rewrite count_after_append.
           change (nedges (mkNode d es bs)) with es. change (edge_count k (gen_at (sslots s) k) []) with 0. lia.
        -- exact (HC sid' s0 E0 k).
      * intros sid' s0 E0. simpl in E0.
        rewrite (find_after_set _ _ s sid') in E0 by (simpl; exact Ef). simpl in E0.
        destruct (Nat.eqb sid' sid).
        -- injection E0 as E0. subst s0. simpl. apply (edges_issued_append _ _ (HE sid s Ef)).
           intros e He. exact (HA s Ef e He).
        -- exact (HE sid' s0 E0).
  - (* ODelete *)
    unfold g_delete. destruct (find_store (lsid l) (gstores g)) as [s|] eqn:Ef; simpl; [|solve_refused].
    destruct (find_store_some _ _ _ Ef) as [_ Hsid].
    destruct (nth_error (sslots s) (lidx l)) as [[gen [nd|]]|] eqn:Ei; simpl; try solve_refused.
    destruct (Nat.eqb gen (lgen l)); simpl; [|solve_refused].
    destruct (existsb (same_place (lsid l) (lidx l)) (gbor g)); simpl; [solve_refused|].
    split.
    + intros sid' s0 E0 k. cbv beta. simpl in E0.
      rewrite (find_after_set _ _ s sid') in E0 by (simpl; rewrite Hsid; exact Ef). simpl in E0.
      rewrite Hsid in E0. destruct (Nat.eqb sid' (lsid l)) eqn:Es.
      * injection E0 as E0. subst s0. apply Nat.eqb_eq in Es. subst sid'.
        rewrite (slot_edges_at_some _ _ _ Ei). destruct (Nat.eqb k (lidx l)) eqn:Ek.
        -- apply Nat.eqb_eq in Ek. subst k. symmetry. cbn [sslots].
           exact (count_after_delete_self _ _ (mkSlot gen (Some nd)) (HE (lsid l) s Ef) Ei).
        -- apply Nat.eqb_neq in Ek. unfold bump. cbv beta. rewrite (HC (lsid l) s Ef k).
           pose proof (count_after_delete_other _ _ (mkSlot gen (Some nd)) Ei k Ek) as E.
           cbn [sgen sslots] in E |- *. change (edge_count k (gen_at (sslots s) k) []) with 0. lia.
      * exact (HC sid' s0 E0 k).
    + intros sid' s0 E0. simpl in E0.
      rewrite (find_after_set _ _ s sid') in E0 by (simpl; rewrite Hsid; exact Ef). simpl in E0.
      rewrite Hsid in E0. destruct (Nat.eqb sid' (lsid l)).
      * injection E0 as E0. subst s0. simpl.
        exact (edges_issued_delete _ _ (mkSlot gen (Some nd)) (HE (lsid l) s Ef) Ei).
      * exact (HE sid' s0 E0).
  - (* OBegin *)
    unfold g_begin. destruct (find_store (lsid l) (gstores g)); simpl; [|solve_refused].
    destruct (follow s (lidx l, lgen l)); simpl; [|solve_refused].
    destruct (conflicts wr (lsid l) (lidx l) (gbor g)); simpl; solve_refused.
  - (* OEnd *)
    unfold g_end. destruct (Nat.ltb i (length (gbor g))); simpl; solve_refused.
  - (* OWrite *)
    unfold g_write. destruct (nth_error (gbor g) i) as [b|] eqn:Eb; simpl; [|solve_refused].
    destruct (negb (bwr b)); simpl; [solve_refused|].
    destruct (find_store (bsid b) (gstores g)) as [s|] eqn:Ef; simpl; [|solve_refused].
    destruct (find_store_some _ _ _ Ef) as [_ Hsid].
    destruct (nth_error (sslots s) (bidx b)) as [[gen [nd|]]|] eqn:Ei; simpl; try solve_refused.
    split.
    + intros sid' s0 E0 k. cbv beta. simpl in E0.
      rewrite (find_after_set _ _ s sid') in E0 by (simpl; rewrite Hsid; exact Ef). simpl in E0.
      rewrite Hsid in E0. destruct (Nat.eqb sid' (bsid b)) eqn:Es.
      * injection E0 as E0. subst s0. apply Nat.eqb_eq in Es. subst sid'.
        rewrite (slot_edges_at_some _ _ _ Ei). unfold bump. cbv beta. rewrite (HC (bsid b) s Ef k).
        pose proof (count_after_write _ _ (mkSlot gen (Some nd)) (mkSlot gen (Some (mkNode d es (nblocks nd))))
          Ei eq_refl k) as E.
        change (slot_edges (mkSlot gen (Some (mkNode d es (nblocks nd))))) with es in E.
        cbn [sslots]. lia.
      * exact (HC sid' s0 E0 k).
    + intros sid' s0 E0. simpl in E0.
      rewrite (find_after_set _ _ s sid') in E0 by (simpl; rewrite Hsid; exact Ef). simpl in E0.
      rewrite Hsid in E0. destruct (Nat.eqb sid' (bsid b)).
      * injection E0 as E0. subst s0. simpl.
        apply (edges_issued_write _ _ (mkSlot gen (Some nd)) (mkSlot gen (Some (mkNode d es (nblocks nd))))
          (HE (bsid b) s Ef) Ei eq_refl);
          [simpl; split; discriminate|]. intros e He. exact (HA b s Eb Ef e He).
      * exact (HE sid' s0 E0).
  - (* ODrop *)
    unfold g_drop. destruct (find_store sid (gstores g)); simpl; [|solve_refused].
    destruct (store_borrowed sid (gbor g)); simpl; [solve_refused|]. split.
    + intros sid' s0 E0 k. simpl in E0. destruct (Nat.eq_dec sid' sid) as [->|Hne].
      * rewrite find_del_same in E0. discriminate.
      * rewrite (find_del_other _ _ _ Hne) in E0. exact (HC sid' s0 E0 k).
    + intros sid' s0 E0. simpl in E0. destruct (Nat.eq_dec sid' sid) as [->|Hne].
      * rewrite find_del_same in E0. discriminate.
      * rewrite (find_del_other _ _ _ Hne) in E0. exact (HE sid' s0 E0).
Qed.

(* ------------------------------------------------------------------ *)
(* One graph/count transition and the operational local-reclaim boundary *)
(* ------------------------------------------------------------------ *)

Record CountedGraph := mkCountedGraph {
  counted_graph : GS;
  counted_counts : nat -> nat -> nat
}.

Definition counted_gstep (gmax : nat) (c : CountedGraph) (o : GOp)
    : CountedGraph * GRes :=
  let result := gexec gmax (counted_graph c) o in
  (mkCountedGraph (fst result)
    (count_update_after (counted_graph c) o (snd result) (counted_counts c)),
   snd result).

Fixpoint counted_grun (gmax : nat) (c : CountedGraph) (ops : list GOp) : CountedGraph :=
  match ops with
  | [] => c
  | o :: rest => counted_grun gmax (fst (counted_gstep gmax c o)) rest
  end.

Fixpoint counted_run_admitted (gmax : nat) (c : CountedGraph) (ops : list GOp) : Prop :=
  match ops with
  | [] => True
  | o :: rest => op_admitted (counted_graph c) o /\
                counted_run_admitted gmax (fst (counted_gstep gmax c o)) rest
  end.

Theorem counted_step_preserves_counts : forall gmax c o,
  CountsExact (counted_counts c) (counted_graph c) ->
  AllEdgesIssued (counted_graph c) -> op_admitted (counted_graph c) o ->
  CountsExact (counted_counts (fst (counted_gstep gmax c o)))
              (counted_graph (fst (counted_gstep gmax c o))) /\
  AllEdgesIssued (counted_graph (fst (counted_gstep gmax c o))).
Proof.
  intros gmax [g cnt] o HC HE HA.
  exact (counts_track_every_operation gmax g o cnt HC HE HA).
Qed.

Theorem counted_run_projects : forall gmax ops c,
  counted_graph (counted_grun gmax c ops) = grun gmax (counted_graph c) ops.
Proof.
  intros gmax ops. induction ops as [|o rest IH]; intros c; simpl; [reflexivity|].
  rewrite IH. reflexivity.
Qed.

Theorem counted_run_preserves_counts : forall gmax ops c,
  CountsExact (counted_counts c) (counted_graph c) -> AllEdgesIssued (counted_graph c) ->
  counted_run_admitted gmax c ops ->
  CountsExact (counted_counts (counted_grun gmax c ops))
              (counted_graph (counted_grun gmax c ops)) /\
  AllEdgesIssued (counted_graph (counted_grun gmax c ops)).
Proof.
  intros gmax ops. induction ops as [|o rest IH]; intros c HC HE HA; simpl in *.
  - split; assumption.
  - destruct HA as [HA HR].
    destruct (counted_step_preserves_counts gmax c o HC HE HA) as [HC' HE'].
    exact (IH _ HC' HE' HR).
Qed.

Lemma counted_delete_ops_admitted : forall gmax ops c,
  (forall o, In o ops -> exists l, o = ODelete l) -> counted_run_admitted gmax c ops.
Proof.
  intros gmax ops. induction ops as [|o rest IH]; intros c HD; simpl; [exact I|].
  destruct (HD o (or_introl eq_refl)) as [l ->]. split; [exact I|].
  apply IH. intros o Ho. apply HD. right. exact Ho.
Qed.

Inductive GraphBatchFailure : Type :=
| BatchDuplicate (l : Link)
| BatchDeleteRefused (l : Link) (why : Refusal)
| BatchUnexpectedResult (l : Link)
| BatchMissingStore (sid : nat)
| BatchSelectionDeferred
| BatchAuthorityDenied
| BatchIncompleteRoots.

Inductive CountedBatchResult : Type :=
| CountedBatchAccepted (after : CountedGraph)
| CountedBatchRefused (original : CountedGraph) (why : GraphBatchFailure).

Definition graph_link_eq_dec : forall x y : Link, {x = y} + {x <> y}.
Proof. decide equality; apply Nat.eq_dec. Defined.

Fixpoint first_duplicate (ls : list Link) : option Link :=
  match ls with
  | [] => None
  | l :: rest => if in_dec graph_link_eq_dec l rest then Some l else first_duplicate rest
  end.

Lemma first_duplicate_none : forall ls, first_duplicate ls = None <-> NoDup ls.
Proof.
  induction ls as [|l rest IH]; simpl.
  - split; [constructor|reflexivity].
  - destruct (in_dec graph_link_eq_dec l rest) as [Hin|Hnot].
    + split; [discriminate|intro H; inversion H; contradiction].
    + rewrite IH. split; [intro H; constructor; assumption|intro H; inversion H; assumption].
Qed.

(* Intermediate functional states are private: no native rollback is claimed. *)
Fixpoint counted_delete_stage (gmax : nat) (c : CountedGraph) (ls : list Link)
    : CountedBatchResult :=
  match ls with
  | [] => CountedBatchAccepted c
  | l :: rest =>
      let step := counted_gstep gmax c (ODelete l) in
      match snd step with
      | GUnit => counted_delete_stage gmax (fst step) rest
      | GRefused why => CountedBatchRefused c (BatchDeleteRefused l why)
      | _ => CountedBatchRefused c (BatchUnexpectedResult l)
      end
  end.

Definition counted_delete_batch (gmax : nat) (c : CountedGraph) (ls : list Link)
    : CountedBatchResult :=
  match first_duplicate ls with
  | Some l => CountedBatchRefused c (BatchDuplicate l)
  | None => match counted_delete_stage gmax c ls with
            | CountedBatchAccepted after => CountedBatchAccepted after
            | CountedBatchRefused _ why => CountedBatchRefused c why
            end
  end.

Fixpoint delete_batch_admitted (gmax : nat) (c : CountedGraph) (ls : list Link) : Prop :=
  match ls with
  | [] => True
  | l :: rest => snd (counted_gstep gmax c (ODelete l)) = GUnit /\
                 delete_batch_admitted gmax (fst (counted_gstep gmax c (ODelete l))) rest
  end.

Definition DeleteIdentityReady (g : GS) (l : Link) : Prop :=
  exists s nd, find_store (lsid l) (gstores g) = Some s /\
    nth_error (sslots s) (lidx l) = Some (mkSlot (lgen l) (Some nd)) /\
    existsb (same_place (lsid l) (lidx l)) (gbor g) = false.

Lemma delete_unit_identity_ready : forall g l,
  snd (g_delete g l) = GUnit <-> DeleteIdentityReady g l.
Proof.
  intros g l. split.
  - unfold g_delete. destruct (find_store (lsid l) (gstores g)) as [s|] eqn:HS;
      [|discriminate].
    destruct (nth_error (sslots s) (lidx l)) as [[gen [nd|]]|] eqn:HN; try discriminate.
    destruct (Nat.eqb gen (lgen l)) eqn:HG; [|discriminate].
    destruct (existsb (same_place (lsid l) (lidx l)) (gbor g)) eqn:HB; [discriminate|].
    intros _. apply Nat.eqb_eq in HG. subst gen. exists s, nd. repeat split; assumption.
  - intros [s [nd [HS [HN HB]]]]. unfold g_delete. rewrite HS, HN, Nat.eqb_refl, HB.
    reflexivity.
Qed.

Lemma delete_batch_admitted_first_identity : forall gmax c l rest,
  delete_batch_admitted gmax c (l :: rest) -> DeleteIdentityReady (counted_graph c) l.
Proof.
  intros gmax c l rest [H _]. apply delete_unit_identity_ready. exact H.
Qed.

Lemma counted_delete_stage_accepted : forall gmax ls c after,
  counted_delete_stage gmax c ls = CountedBatchAccepted after ->
  after = counted_grun gmax c (map ODelete ls) /\ delete_batch_admitted gmax c ls.
Proof.
  intros gmax ls. induction ls as [|l rest IH]; intros c after H.
  - cbn [counted_delete_stage] in H. injection H as H. subst. split; [reflexivity|exact I].
  - cbn [counted_delete_stage] in H. cbn [counted_grun map delete_batch_admitted].
    destruct (snd (counted_gstep gmax c (ODelete l))) eqn:HR; try discriminate.
    destruct (IH _ _ H) as [E HA]. split; [exact E|split; [reflexivity|exact HA]].
Qed.

Lemma counted_delete_stage_admitted : forall gmax ls c,
  delete_batch_admitted gmax c ls ->
  counted_delete_stage gmax c ls = CountedBatchAccepted (counted_grun gmax c (map ODelete ls)).
Proof.
  intros gmax ls. induction ls as [|l rest IH]; intros c HA;
    cbn [counted_delete_stage counted_grun map delete_batch_admitted] in *; [reflexivity|].
  destruct HA as [HR HA]. rewrite HR. apply IH. exact HA.
Qed.

Theorem counted_delete_batch_admitted : forall gmax c ls,
  NoDup ls -> delete_batch_admitted gmax c ls ->
  counted_delete_batch gmax c ls = CountedBatchAccepted (counted_grun gmax c (map ODelete ls)).
Proof.
  intros gmax c ls HN HA. unfold counted_delete_batch.
  rewrite (proj2 (first_duplicate_none ls) HN), (counted_delete_stage_admitted _ _ _ HA).
  reflexivity.
Qed.

Theorem counted_delete_batch_refused_unchanged : forall gmax c ls original why,
  counted_delete_batch gmax c ls = CountedBatchRefused original why -> original = c.
Proof.
  intros gmax c ls original why H. unfold counted_delete_batch in H.
  destruct (first_duplicate ls); [injection H; auto|].
  destruct (counted_delete_stage gmax c ls); [discriminate|injection H; auto].
Qed.

Theorem counted_delete_batch_accepted_projects : forall gmax c ls after,
  counted_delete_batch gmax c ls = CountedBatchAccepted after ->
  after = counted_grun gmax c (map ODelete ls) /\
  NoDup ls /\ delete_batch_admitted gmax c ls.
Proof.
  intros gmax c ls after H. unfold counted_delete_batch in H.
  destruct (first_duplicate ls) eqn:HD; [discriminate|].
  destruct (counted_delete_stage gmax c ls) eqn:HS; [|discriminate].
  injection H as H. subst. destruct (counted_delete_stage_accepted _ _ _ _ HS) as [E HA].
  split; [exact E|split; [apply first_duplicate_none; exact HD|exact HA]].
Qed.

Theorem counted_delete_batch_preserves_counts : forall gmax c ls after,
  CountsExact (counted_counts c) (counted_graph c) -> AllEdgesIssued (counted_graph c) ->
  counted_delete_batch gmax c ls = CountedBatchAccepted after ->
  CountsExact (counted_counts after) (counted_graph after) /\ AllEdgesIssued (counted_graph after).
Proof.
  intros gmax c ls after HC HE H.
  destruct (counted_delete_batch_accepted_projects _ _ _ _ H) as [-> _].
  apply counted_run_preserves_counts; [exact HC|exact HE|].
  apply counted_delete_ops_admitted. intros o Ho. apply in_map_iff in Ho.
  destruct Ho as [l [<- _]]. eexists. reflexivity.
Qed.

Definition reclaim_links (s : Store) (C : list nat) : list Link :=
  map (fun i => mkLink (ssid s) i (slot_gen s i)) C.

(* Check the supplied inventory before selection, including retained candidates.
   An index supplies no generation; stale generation checks belong to the Link batch API. *)
Fixpoint reclaim_candidate_error (s : Store) (C : list nat) : option GraphBatchFailure :=
  match C with
  | [] => None
  | i :: rest =>
      let l := mkLink (ssid s) i (slot_gen s i) in
      if inb i rest then Some (BatchDuplicate l) else
      match nth_error (sslots s) i with
      | Some (mkSlot _ (Some _)) => reclaim_candidate_error s rest
      | _ => Some (BatchDeleteRefused l RStale)
      end
  end.

Lemma reclaim_candidates_admitted : forall s C,
  reclaim_candidate_error s C = None ->
  NoDup C /\ forall i, In i C -> exists gen nd, nth_error (sslots s) i = Some (mkSlot gen (Some nd)).
Proof.
  intros s C. induction C as [|i rest IH]; intro H.
  - split; [constructor|intros j HJ; contradiction].
  - cbn [reclaim_candidate_error] in H.
    destruct (inb i rest) eqn:HD; [discriminate|].
    destruct (nth_error (sslots s) i) as [[gen [nd|]]|] eqn:HN; try discriminate.
    destruct (IH H) as [HR HL]. split.
    + constructor; [apply inb_false; exact HD|exact HR].
    + intros j [<-|HJ]; [exists gen, nd; exact HN|apply HL; exact HJ].
Qed.

Definition counted_reclaim (gmax : nat) (c : CountedGraph) (sid : nat)
    (C : list nat) (fuel : nat) (roots : list Link) : CountedBatchResult :=
  match find_store sid (gstores (counted_graph c)) with
  | Some s => match reclaim_candidate_error s C with
              | Some why => CountedBatchRefused c why
              | None => match trial_garbage_with (counted_counts c sid) fuel s roots C with
                        | Some G => counted_delete_batch gmax c (reclaim_links s G)
                        | None => CountedBatchRefused c BatchSelectionDeferred
                        end
              end
  | None => CountedBatchRefused c (BatchMissingStore sid)
  end.

Theorem counted_reclaim_refused_unchanged : forall gmax c sid C fuel roots original why,
  counted_reclaim gmax c sid C fuel roots = CountedBatchRefused original why -> original = c.
Proof.
  intros gmax c sid C fuel roots original why H. unfold counted_reclaim in H.
  destruct (find_store sid (gstores (counted_graph c))) as [s|]; [|injection H; auto].
  destruct (reclaim_candidate_error s C); [injection H; auto|].
  destruct (trial_garbage_with (counted_counts c sid) fuel s roots C);
    [eapply counted_delete_batch_refused_unchanged; exact H|injection H; auto].
Qed.

Theorem counted_reclaim_preserves_counts : forall gmax c sid C fuel roots after,
  CountsExact (counted_counts c) (counted_graph c) -> AllEdgesIssued (counted_graph c) ->
  counted_reclaim gmax c sid C fuel roots = CountedBatchAccepted after ->
  CountsExact (counted_counts after) (counted_graph after) /\ AllEdgesIssued (counted_graph after).
Proof.
  intros gmax c sid C fuel roots after HC HE H. unfold counted_reclaim in H.
  destruct (find_store sid (gstores (counted_graph c))) as [s|]; [|discriminate].
  destruct (reclaim_candidate_error s C); [discriminate|].
  destruct (trial_garbage_with (counted_counts c sid) fuel s roots C); [|discriminate].
  eapply counted_delete_batch_preserves_counts; eassumption.
Qed.

Theorem counted_reclaim_projects : forall gmax c sid C fuel roots after,
  CountsExact (counted_counts c) (counted_graph c) ->
  counted_reclaim gmax c sid C fuel roots = CountedBatchAccepted after ->
  exists s G, find_store sid (gstores (counted_graph c)) = Some s /\
    trial_garbage fuel s roots C = Some G /\
    counted_graph after = grun gmax (counted_graph c) (reclaim_ops s G).
Proof.
  intros gmax c sid C fuel roots after HC H. unfold counted_reclaim in H.
  destruct (find_store sid (gstores (counted_graph c))) as [s|] eqn:HS; [|discriminate].
  destruct (reclaim_candidate_error s C); [discriminate|].
  rewrite (trial_with_exact_counter _ _ _ _ _ (HC sid s HS)) in H.
  destruct (trial_garbage fuel s roots C) as [G|] eqn:HG; [|discriminate].
  destruct (counted_delete_batch_accepted_projects _ _ _ _ H) as [E _].
  exists s, G. split; [reflexivity|split; [exact HG|]]. rewrite E, counted_run_projects.
  unfold reclaim_links, reclaim_ops. rewrite map_map. reflexivity.
Qed.

(* ------------------------------------------------------------------ *)
(* Witnesses                                                            *)
(* ------------------------------------------------------------------ *)

(* The store of [unreachable_nodes_retained]: slots 0 and 1 form a cycle the
   program still reaches through link 0; slots 2 and 3 form a cycle whose
   links the program has forgotten. *)
Definition cyc_ops : list GOp :=
  [ONew (0, 0); OInsert 0 0 1 [(1, 0)] [(2, 0)] (1, 0);
   OInsert 0 1 2 [(0, 0)] [(4, 0)] (3, 0);
   OInsert 0 2 3 [(3, 0)] [(6, 0)] (5, 0);
   OInsert 0 3 4 [(2, 0)] [(8, 0)] (7, 0)].
Definition gcyc : GS := grun 3 gempty cyc_ops.
Definition cyc_store : Store :=
  match find_store 0 (gstores gcyc) with Some s => s | None => mkStore 0 (0, 0) [] end.
Definition link0 : Link := mkLink 0 0 0.
Definition link2 : Link := mkLink 0 2 0.
Definition cyc_roots : list Link := [link0].

Lemma gcyc_inv : GInv 3 gcyc.
Proof. apply ginv_run. apply ginv_empty. Qed.

Lemma gcyc_store : find_store 0 (gstores gcyc) = Some cyc_store.
Proof. reflexivity. Qed.

(* A pure in-count test does not free the cycle: each node keeps one
   incoming edge, from the other. *)
Example count_zero_never_frees_cycle :
  indeg (sslots cyc_store) 2 = 1 /\ indeg (sslots cyc_store) 3 = 1.
Proof. split; reflexivity. Qed.

Example cycle_is_garbage : trial_garbage 2 cyc_store cyc_roots [2; 3] = Some [2; 3].
Proof. reflexivity. Qed.

Definition gcyc_reclaimed : GS := grun 3 gcyc (reclaim_ops cyc_store [2; 3]).

Example cycle_reclaimed_heap :
  length (gheap gcyc) = 5 /\ length (gheap gcyc_reclaimed) = 3 /\
  resolve gcyc_reclaimed link0 = resolve gcyc link0 /\
  resolve gcyc_reclaimed link2 = Missing RStale.
Proof. repeat split; reflexivity. Qed.

Theorem cycle_reclaim_preserves_root_view :
  GInv 3 gcyc_reclaimed /\
  exists s', find_store 0 (gstores gcyc_reclaimed) = Some s' /\
    (forall j, reach cyc_store cyc_roots j <-> reach s' cyc_roots j).
Proof.
  destruct (reclaim_preserves_root_view 3 gcyc 0 cyc_store (indeg (sslots cyc_store)) 2 cyc_roots [2; 3] [2; 3]
    gcyc_inv gcyc_store (fun k => le_n _) cycle_is_garbage) as [Hi [_ [s' [Ef [_ [Hr _]]]]]].
  split; [exact Hi|]. exists s'. split; [exact Ef| exact Hr].
Qed.

(* A candidate pointed to from outside C is kept: slot 1 is named by slot 0.
   The counted check does not inspect slot 0's edge payload to count inside C;
   the reference list representation still traverses its slot-list cell. *)
Example outside_edge_keeps_node :
  external [1; 2; 3] (sslots cyc_store) 1 = 1 /\
  trial_garbage 2 cyc_store cyc_roots [1; 2; 3] = Some [2; 3].
Proof. split; reflexivity. Qed.

(* A chain root -> 0 -> 1 -> 2 <-> 3. *)
Definition chain_store : Store :=
  mkStore 0 (9, 0)
    [mkSlot 0 (Some (mkNode 1 [(1, 0)] [(1, 0)])); mkSlot 0 (Some (mkNode 2 [(2, 0)] [(2, 0)]));
     mkSlot 0 (Some (mkNode 3 [(3, 0)] [(3, 0)])); mkSlot 0 (Some (mkNode 4 [(2, 0)] [(4, 0)]))].

Lemma chain_reaches_2 : reach chain_store cyc_roots 2.
Proof.
  change 2 with (fst ((2, 0) : Edge)).
  eapply REdge with (j := 1) (gen := 0); [|reflexivity|simpl; left; reflexivity|reflexivity].
  change 1 with (fst ((1, 0) : Edge)).
  eapply REdge with (j := 0) (gen := 0); [|reflexivity|simpl; left; reflexivity|reflexivity].
  change 0 with (lidx link0).
  eapply RRoot; [simpl; left; reflexivity|reflexivity|reflexivity].
Qed.

(* Subtracting inside edges without the closure step deletes 2 and 3, which
   the program still reaches through 1. The closure keeps them. *)
Definition naive_trial (s : Store) (roots : list Link) (C : list nat) : list nat :=
  filter (fun i => negb (inb i (held_with (indeg (sslots s)) s roots C))) C.

Example closure_is_required :
  naive_trial chain_store cyc_roots [1; 2; 3] = [2; 3] /\
  trial_garbage 3 chain_store cyc_roots [1; 2; 3] = Some [].
Proof. split; reflexivity. Qed.

Theorem naive_trial_deletes_reachable :
  In 2 (naive_trial chain_store cyc_roots [1; 2; 3]) /\ reach chain_store cyc_roots 2.
Proof. split; [simpl; left; reflexivity| exact chain_reaches_2]. Qed.

Example small_budget_defers : trial_garbage 0 chain_store cyc_roots [1; 2; 3] = None.
Proof. reflexivity. Qed.

(* Here the root list is only a premise: a link the reclaimer was not told
   about becomes stale. OwnershipGraphRootCompleteness shows that a checked
   program's root producer never omits such a link. *)
Example forgotten_root_becomes_stale :
  resolve gcyc link2 <> Missing RStale /\ resolve gcyc_reclaimed link2 = Missing RStale.
Proof. split; [intro H; vm_compute in H; discriminate H| reflexivity]. Qed.

(* The root view is the boundary: an insert naming the raw slot index of a
   reclaimed node was refused before and succeeds after. *)
Example reclaimed_slot_reuse_is_visible :
  snd (gexec 3 gcyc (OInsert 0 2 9 [] [(10, 0)] (11, 0))) = GRefused RConflict /\
  snd (gexec 3 gcyc_reclaimed (OInsert 0 2 9 [] [(10, 0)] (11, 0))) = GLink (mkLink 0 2 1).
Proof. split; reflexivity. Qed.

(* Slot 2 was reused at generation 1. Slot 0 names the new identity (2, 1);
   slot 1 still holds a stale edge to the old identity (2, 0). Clearing that
   stale field must not change the count of (2, 1). *)
Definition reuse_before : list Slot :=
  [mkSlot 0 (Some (mkNode 1 [(2, 1)] [(1, 0)])); mkSlot 0 (Some (mkNode 2 [(2, 0)] [(2, 0)]));
   mkSlot 1 (Some (mkNode 3 [] [(3, 0)]))].
Definition reuse_after : list Slot :=
  upd reuse_before 1 (mkSlot 0 (Some (mkNode 2 [] [(2, 0)]))).
Definition reuse_store : Store := mkStore 0 (9, 0) reuse_after.

(* The identity rule keeps the count; a decrement by slot index, which keeps
   counting the old generation, drops it to zero. *)
Definition index_count (k : nat) (es : list Edge) : nat := length (filter (fun e => Nat.eqb (fst e) k) es).
Definition identity_counter : nat -> nat :=
  bump (gen_at reuse_before) (indeg reuse_before) [] [(2, 0)].
Definition index_counter : nat -> nat :=
  fun k => indeg reuse_before k - index_count k [(2, 0)].

Example identity_counter_tracks :
  identity_counter 2 = 1 /\ indeg reuse_after 2 = 1 /\ index_counter 2 = 0.
Proof. repeat split; reflexivity. Qed.

Lemma reuse_reaches_2 : reach reuse_store cyc_roots 2.
Proof.
  change 2 with (fst ((2, 1) : Edge)).
  eapply REdge with (j := 0) (gen := 0); [|reflexivity|simpl; left; reflexivity|reflexivity].
  change 0 with (lidx link0).
  eapply RRoot; [simpl; left; reflexivity|reflexivity|reflexivity].
Qed.

Theorem index_decrement_after_reuse_deletes_reachable :
  trial_garbage_with index_counter 1 reuse_store cyc_roots [2] = Some [2] /\
  reach reuse_store cyc_roots 2 /\
  trial_garbage_with identity_counter 1 reuse_store cyc_roots [2] = Some [].
Proof. split; [reflexivity| split; [exact reuse_reaches_2| reflexivity]]. Qed.
