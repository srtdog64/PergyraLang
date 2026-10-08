(* OwnershipTeardown.v -- one owner per node, many links, and a teardown
   theorem: release always succeeds and never leaves an orphan or a dangling
   stored link, without computing reachability from roots.

   Design check for docs/audits/ownership_graph_links_design_2026-10-08.md,
   section "해체 정리". It abstracts Qt's QObject tree: a parent owns its
   children, deleting an object deletes its subtree, and connections and
   QPointer guards to a deleted object are removed through per-object lists.

   Model.
   - A heap of slots; each slot has a generation and maybe a node.
   - A node has exactly one owner: a live program root [ORoot r] or a parent
     node [OParent p]. Ownership is the child's owner field, so "one owner"
     is structural, and links never own.
   - Roots are program variables with a lifetime: [roots] lists the live
     ones. A root is created by RootNew and goes out of scope by RootDrop,
     which tears down everything it owns. A node owned by a root nobody
     declared cannot exist (RootsLive).
   - A node has link fields [fields k : option Link]; a link (also a
     handle) is (target slot, generation).
   - [rix t] is the reverse index of stored links into t (Qt's per-object
     connection lists). [kids o] is the children index keyed by owner:
     [kids (OParent p)] is p's children list (Qt's children list) and
     [kids (ORoot r)] lists the nodes root r owns directly.
   - Every step names its subject node, and the new parent node if any, by a
     handle (slot, generation) that must resolve. A stale node handle admits
     no step. Root-directed allocation, attachment and drop likewise name
     a live root by (root id, epoch); drop advances its epoch.

   Steps: RootNew, RootDrop (teardown of what the root owns), Alloc into any
   free slot (reuse), SetField, Attach (reparent, with the ancestor check),
   Release (teardown of the owned subtree).

   Teardown of a unit U: phase 1 clears every stored link from outside U
   into U, found through the index; phase 2 frees U's slots and bumps their
   generations. The index and the children index drop U's entries.

   Main results.
   - [inv_step], [inv_steps]: OrphanFree (every live node has an owner path
     to a root), RootsLive, LinksResolve (every stored link resolves),
     IndexExact, KidsExact and Bounded hold after any number of steps;
     [inv_empty] starts them.
   - [release_always_succeeds], [root_drop_always_succeeds]: an exact unit
     with no duplicate slot always exists, so both always have a step;
     [release_unit_unique], [root_drop_unit_unique] forbid repeated
     retirement entries; [teardown_deterministic]:
     the result does not depend on how the unit is enumerated.
   - [index_set_refines_scan], [kids_move_refines_scan]: a field update
     removes its entry only from the actual old target, and reparenting
     removes its child only from the actual old owner. Both equal the scan
     reference under the exact-index invariant. Unrelated rows are returned
     directly ([index_set_frame], [kids_move_frame]), not copied by app [].
   - [every_node_has_live_root], [no_roots_no_nodes]: every live node
     belongs to a root that is still in scope; with no roots, nothing lives.
   - [unit_from_kids], [root_unit_from_kids]: the release unit and the
     root-drop unit are exactly what the children index reaches, so the
     runtime need not scan the heap for either.
   - [index_finds_every_incoming]: with an exact index, phase 1 finds every
     stored link into U.
   - [release_frees_exactly_subtree], [root_drop_frees_exactly],
     [root_drop_leaves_nothing_owned], [release_field_effect],
     [release_clears_incoming], [release_frame].
   - [step_handles_resolve], [released_handle_never_acts],
     [dropped_root_handles_never_act]: after a node is torn down, no later
     step can act through its old handle, including a second release, even
     after its slot is reused ([released_slot_reusable],
     [temp_link_dead_forever]).
   - [released_unit_local_read_none_forever],
     [dropped_root_unit_local_read_none_forever]: the checked read of a
     saved local link into any member of a released or dropped unit, not
     only its root node, returns None in every later state.
   - [full_run_cycle_released]: from the empty heap, a root owns parent A,
     A owns child B, A and B link to each other; dropping the root leaves
     nothing; the reused slot refuses the old handle.

   Counterexamples, each removing one rule:
   - [no_clear_dangles]: without phase 1 a stored link dangles.
   - [stale_index_dangles]: an index missing one entry leaves it dangling.
   - [checked_attach_refuses_cycle], [unchecked_attach_orphans],
     [ownership_cycle_permanent]: attaching a node under its own descendant
     makes an ownership cycle with no root. Every step that could free or
     move it needs a rooted subject, so no later step does; teardown cannot
     replace the ancestor check.
   - [rc_cycle_permanent] vs [full_run_cycle_released]: in a reference-
     counting machine on the same heap, after the program drops its root a
     link cycle is unreachable and is kept by every later step; under tree
     ownership the same drop frees it.
   - [finalizer_relink_dangles], [relink_into_released_refused]: a raw
     write between the phases that links into U leaves a dangling link;
     after teardown the same write is refused.
   - [inexact_unit_orphans], [orphan_adopted_by_reused_slot],
     [exact_unit_frees_descendant]: a unit that misses a descendant orphans
     it, and reusing the parent's slot then adopts the orphan into an
     unrelated node. This is why the children index must be exact.
   - [release_empties_incoming_field], [owned_but_unlinked_is_kept]: the
     costs. A link field can become empty, and a node that is owned but not
     linked is kept until its owner releases it or its root is dropped (a
     logical leak; a tracing GC also keeps reachable-but-unneeded objects).

   Precision notes.
   - "No reachability" is by construction: teardown never computes paths
     from roots. It edits index and children lists outside U (the entries
     whose source or child is in U). Teardown still filters every projected
     row; an indexed destructive implementation would follow U's own fields
     and parent pointer. The admitted SetField/Attach updates now filter
     only the old target/owner row, but a large row remains expensive.
     The scan transforms are specifications, not compatibility fallbacks.
   - [unit_from_kids] and [root_unit_from_kids] identify units by inductive
     reachability in the exact children index. [enum_below] constructs an
     exact unique witness below the heap bound; these are not a production
     indexed subtree walker or a complexity bound. [unit_unique] is an
     extracted finite certificate observer with quadratic list-membership
     cost on a unique list, not the chosen production enumeration algorithm.
     tests/ownership_teardown_redteam_smoke.sh measures only bounded fresh
     OCaml observations, not native allocator, GC or compiler performance.
   - Release needs a resolving handle to a rooted node; it does not model
     that only the owner may release, or that a loan/pin blocks retirement.
     These broader transitions establish forest/stored-link invariants,
     not caller authority or loan safety. Doc 28 owns the required future
     composition boundary; no compiler-side affine issuer is assumed proven.
   - The handle guarantee covers stale handles, not forged ones: a program
     that computes (slot, generation + 1) by arithmetic can name whatever
     now lives there. Handles are assumed opaque values.
   - RootNew declares a lexical root id without resetting its epoch. A
     dropped root id can be declared again; it adopts nothing, and an old
     root handle cannot allocate, attach or drop through the new identity.
     Epochs are unbounded naturals in this single forest, not minted caller
     permissions or cross-arena identities.
   - Finalizers are not steps; the counterexample covers one raw write.
   - Out of model: concurrency, resource-finalizer ordering, generation
     overflow (generations are unbounded nat; bounded generations with
     retirement are in OwnershipGraphLinks.v), a production bound for
     walking the children index, and the memory cost of the indexes. *)

Require Import Stdlib.Lists.List.
Require Import Stdlib.Arith.PeanoNat.
Require Import Stdlib.Bool.Bool.
Require Import Stdlib.micromega.Lia.
Import ListNotations.

(** * Model *)

Definition Link : Type := (nat * nat)%type.

Inductive Owner : Type :=
| ORoot (r : nat)
| OParent (p : nat).

Record Node : Type := mkNode { owner : Owner; fields : nat -> option Link }.
Record Slot : Type := mkSlot { s_gen : nat; s_node : option Node }.

Definition Heap : Type := nat -> Slot.
Definition Index : Type := nat -> list (nat * nat).
(** The children index is keyed by owner: [kids (OParent p)] lists p's
    children and [kids (ORoot r)] lists the nodes root r owns directly. *)
Definition Kids : Type := Owner -> list nat.

Record St : Type :=
  mkSt { heap : Heap; rix : Index; kids : Kids; roots : list nat; bound : nat;
         root_gen : nat -> nat }.

(** Fresh fixtures start at epoch zero; RootNew must never reset epochs. *)
Definition initial_root_generations : nat -> nat := fun _ => 0.

Definition live (h : Heap) (x : nat) : Prop := exists nd, s_node (h x) = Some nd.

Definition resolves (h : Heap) (l : Link) : Prop :=
  s_gen (h (fst l)) = snd l /\ live h (fst l).

Definition root_resolves (rs : list nat) (rg : nat -> nat) (l : Link) : Prop :=
  In (fst l) rs /\ rg (fst l) = snd l.

(** Checked read for a saved local link. A valid identity is not authority;
    returning Node is an immutable model value, not a stable physical loan. *)
Definition resolve_node (h : Heap) (l : Link) : option Node :=
  let sl := h (fst l) in
  if Nat.eqb (s_gen sl) (snd l) then s_node sl else None.

Theorem resolve_node_some : forall h l nd,
  resolve_node h l = Some nd <->
  s_gen (h (fst l)) = snd l /\ s_node (h (fst l)) = Some nd.
Proof.
  intros h l nd. unfold resolve_node. destruct (Nat.eqb (s_gen (h (fst l))) (snd l)) eqn:E.
  - apply Nat.eqb_eq in E. split; [intro H; split; assumption | tauto].
  - apply Nat.eqb_neq in E. split; [discriminate | intros [H _]; contradiction].
Qed.

Theorem resolve_node_none : forall h l,
  resolve_node h l = None <-> ~ resolves h l.
Proof.
  intros h l. split.
  - intros H [Hg [nd Hnd]]. pose proof (proj2 (resolve_node_some h l nd) (conj Hg Hnd)) as Hsome.
    congruence.
  - intro H. destruct (resolve_node h l) as [nd|] eqn:E; [| reflexivity].
    exfalso. apply H. apply resolve_node_some in E. destruct E as [Hg Hnd].
    split; [exact Hg | exists nd; exact Hnd].
Qed.

Definition field_at (h : Heap) (src k : nat) : option Link :=
  match s_node (h src) with
  | Some nd => fields nd k
  | None => None
  end.

Definition owner_at (h : Heap) (x : nat) : option Owner :=
  match s_node (h x) with
  | Some nd => Some (owner nd)
  | None => None
  end.

(** [rooted h x]: x has an owner path ending at a root. *)
Inductive rooted (h : Heap) : nat -> Prop :=
| RRoot : forall x nd r,
    s_node (h x) = Some nd -> owner nd = ORoot r -> rooted h x
| RPar : forall x nd p,
    s_node (h x) = Some nd -> owner nd = OParent p -> rooted h p -> rooted h x.

(** [in_sub h n x]: x is in the subtree owned by n (n included). *)
Inductive in_sub (h : Heap) (n : nat) : nat -> Prop :=
| SubHere : in_sub h n n
| SubDown : forall x nd p,
    s_node (h x) = Some nd -> owner nd = OParent p -> in_sub h n p -> in_sub h n x.

(** [under_root h r x]: x's owner path ends at root r. *)
Inductive under_root (h : Heap) (r : nat) : nat -> Prop :=
| URHere : forall x nd,
    s_node (h x) = Some nd -> owner nd = ORoot r -> under_root h r x
| URDown : forall x nd p,
    s_node (h x) = Some nd -> owner nd = OParent p -> under_root h r p -> under_root h r x.

Definition OrphanFree (h : Heap) : Prop := forall x, live h x -> rooted h x.
Definition RootsLive (h : Heap) (rs : list nat) : Prop :=
  forall x nd r, s_node (h x) = Some nd -> owner nd = ORoot r -> In r rs.
Definition LinksResolve (h : Heap) : Prop :=
  forall src k l, field_at h src k = Some l -> resolves h l.
Definition IndexExact (h : Heap) (ix : Index) : Prop :=
  forall t src k, In (src, k) (ix t) <-> exists g, field_at h src k = Some (t, g).
Definition KidsExact (h : Heap) (kd : Kids) : Prop :=
  forall o c, In c (kd o) <-> exists nd, s_node (h c) = Some nd /\ owner nd = o.
Definition Bounded (h : Heap) (b : nat) : Prop := forall x, live h x -> x < b.

Definition InvH (h : Heap) (ix : Index) (kd : Kids) (rs : list nat) (b : nat) : Prop :=
  OrphanFree h /\ RootsLive h rs /\ LinksResolve h /\ IndexExact h ix /\
  KidsExact h kd /\ Bounded h b.

Definition Inv (s : St) : Prop := InvH (heap s) (rix s) (kids s) (roots s) (bound s).

(** ** Boolean tests *)

Definition inU (Ul : list nat) (y : nat) : bool := existsb (Nat.eqb y) Ul.

Lemma inU_spec : forall Ul y, inU Ul y = true <-> In y Ul.
Proof.
  intros Ul y. unfold inU. rewrite existsb_exists. split.
  - intros [z [Hin Heq]]. apply Nat.eqb_eq in Heq. subst. exact Hin.
  - intros Hin. exists y. split; [exact Hin | apply Nat.eqb_refl].
Qed.

Lemma inU_false : forall Ul y, inU Ul y = false <-> ~ In y Ul.
Proof.
  intros Ul y. rewrite <- inU_spec. destruct (inU Ul y).
  - split; [discriminate | intro H; exfalso; apply H; reflexivity].
  - split; [intros _ H; discriminate | intros _; reflexivity].
Qed.

Definition check_root (s : St) (l : Link) : bool :=
  inU (roots s) (fst l) && Nat.eqb (root_gen s (fst l)) (snd l).

Theorem check_root_spec : forall s l,
  check_root s l = true <-> root_resolves (roots s) (root_gen s) l.
Proof. intros s l. unfold check_root, root_resolves. rewrite andb_true_iff, inU_spec, Nat.eqb_eq. reflexivity. Qed.

Theorem check_root_false : forall s l,
  check_root s l = false <-> ~ root_resolves (roots s) (root_gen s) l.
Proof.
  intros s l. rewrite <- check_root_spec. destruct (check_root s l).
  - split; [discriminate | intro H; exfalso; apply H; reflexivity].
  - split; [intros _ H; discriminate | intros _; reflexivity].
Qed.

(** A finite observer/admission certificate checker, not the production
    subtree issuer. Its list membership cost can be quadratic. *)
Fixpoint unit_unique (Ul : list nat) : bool :=
  match Ul with
  | [] => true
  | x :: xs => negb (inU xs x) && unit_unique xs
  end.

Theorem unit_unique_spec : forall Ul, unit_unique Ul = true <-> NoDup Ul.
Proof.
  intros Ul. induction Ul as [| x xs IH]; simpl.
  - split; [intros _; constructor | intros _; reflexivity].
  - rewrite andb_true_iff, negb_true_iff, inU_false, IH. split.
    + intros [Hnot Hnd]. constructor; assumption.
    + intros H. inversion H; subst. split; assumption.
Qed.

Lemma inU_ext : forall Ul1 Ul2, (forall y, In y Ul1 <-> In y Ul2) ->
  forall y, inU Ul1 y = inU Ul2 y.
Proof.
  intros Ul1 Ul2 H y. destruct (inU Ul1 y) eqn:E1; destruct (inU Ul2 y) eqn:E2;
    try reflexivity.
  - apply inU_spec, H, inU_spec in E1. congruence.
  - apply inU_spec, H, inU_spec in E2. congruence.
Qed.

Definition ent_eqb (a b : nat * nat) : bool :=
  Nat.eqb (fst a) (fst b) && Nat.eqb (snd a) (snd b).

Lemma ent_eqb_spec : forall a b, ent_eqb a b = true <-> a = b.
Proof.
  intros [a1 a2] [b1 b2]. unfold ent_eqb. simpl.
  rewrite andb_true_iff, !Nat.eqb_eq. split.
  - intros [-> ->]. reflexivity.
  - intros H. injection H as E1 E2. auto.
Qed.

(** [hit Ul ix src k]: the index lists field k of src under some target in U.
    Phase 1 visits only these index entries. *)
Definition hit (Ul : list nat) (ix : Index) (src k : nat) : bool :=
  existsb (fun t => existsb (ent_eqb (src, k)) (ix t)) Ul.

Lemma hit_spec : forall Ul ix src k,
  hit Ul ix src k = true <-> exists t, In t Ul /\ In (src, k) (ix t).
Proof.
  intros Ul ix src k. unfold hit. rewrite existsb_exists. split.
  - intros [t [Ht Hx]]. rewrite existsb_exists in Hx.
    destruct Hx as [e [He Heq]]. apply ent_eqb_spec in Heq. subst e.
    exists t. split; assumption.
  - intros [t [Ht Hx]]. exists t. split; [exact Ht |].
    rewrite existsb_exists. exists (src, k). split; [exact Hx |].
    apply ent_eqb_spec. reflexivity.
Qed.

Lemma filter_ext_local : forall (A : Type) (f g : A -> bool) (l : list A),
  (forall a, f a = g a) -> filter f l = filter g l.
Proof.
  intros A f g l H. induction l as [| a l IH]; simpl; [reflexivity |].
  rewrite H, IH. reflexivity.
Qed.

Lemma in_filter_neq : forall c x l, c <> x ->
  (In c (filter (fun c' => negb (Nat.eqb c' x)) l) <-> In c l).
Proof.
  intros c x l Hne. rewrite filter_In. apply Nat.eqb_neq in Hne. rewrite Hne.
  simpl. tauto.
Qed.

Lemma not_in_filter_self : forall x l, ~ In x (filter (fun c' => negb (Nat.eqb c' x)) l).
Proof.
  intros x l H. apply filter_In in H. rewrite Nat.eqb_refl in H.
  destruct H as [_ H]. discriminate.
Qed.

(** * Teardown *)

Definition clear_node (Ul : list nat) (ix : Index) (src : nat) (nd : Node) : Node :=
  mkNode (owner nd) (fun k => if hit Ul ix src k then None else fields nd k).

(** Phase 1: clear the stored links into U, found through the index. *)
Definition phase1 (h : Heap) (ix : Index) (Ul : list nat) : Heap :=
  fun src =>
    (* One immutable slot snapshot; repeated evaluation of older functional
       heaps here otherwise duplicates work at every retirement. *)
    let sl := h src in
    if inU Ul src then sl
    else match s_node sl with
         | Some nd => mkSlot (s_gen sl) (Some (clear_node Ul ix src nd))
         | None => sl
         end.

(** Phase 2: free U's slots and bump their generations. *)
Definition phase2 (h : Heap) (Ul : list nat) : Heap :=
  fun src => if inU Ul src then mkSlot (S (s_gen (h src))) None else h src.

Definition teardown_heap (h : Heap) (ix : Index) (Ul : list nat) : Heap :=
  phase2 (phase1 h ix Ul) Ul.

Definition teardown_index (ix : Index) (Ul : list nat) : Index :=
  fun t => if inU Ul t then [] else filter (fun e => negb (inU Ul (fst e))) (ix t).

Definition teardown_kids (kd : Kids) (Ul : list nat) : Kids :=
  fun o => filter (fun c => negb (inU Ul c)) (kd o).

Definition teardown (s : St) (Ul : list nat) : St :=
  mkSt (teardown_heap (heap s) (rix s) Ul) (teardown_index (rix s) Ul)
       (teardown_kids (kids s) Ul) (roots s) (bound s) (root_gen s).

Definition root_drop (s : St) (r : nat) (Ul : list nat) : St :=
  mkSt (teardown_heap (heap s) (rix s) Ul) (teardown_index (rix s) Ul)
       (teardown_kids (kids s) Ul) (remove Nat.eq_dec r (roots s)) (bound s)
       (fun q => if Nat.eqb q r then S (root_gen s q) else root_gen s q).

Lemma root_drop_epoch : forall s r Ul,
  root_gen (root_drop s r Ul) r = S (root_gen s r).
Proof. intros s r Ul. cbn [root_drop root_gen]. rewrite Nat.eqb_refl. reflexivity. Qed.

Lemma root_drop_epoch_frame : forall s r Ul q, q <> r ->
  root_gen (root_drop s r Ul) q = root_gen s q.
Proof. intros s r Ul q H. cbn [root_drop root_gen]. apply Nat.eqb_neq in H. rewrite H. reflexivity. Qed.

Lemma heap_teardown : forall s Ul, heap (teardown s Ul) = teardown_heap (heap s) (rix s) Ul.
Proof. reflexivity. Qed.

Lemma heap_root_drop : forall s r Ul,
  heap (root_drop s r Ul) = teardown_heap (heap s) (rix s) Ul.
Proof. reflexivity. Qed.

Lemma roots_root_drop : forall s r Ul, roots (root_drop s r Ul) = remove Nat.eq_dec r (roots s).
Proof. reflexivity. Qed.

Lemma teardown_in : forall h ix Ul x, inU Ul x = true ->
  teardown_heap h ix Ul x = mkSlot (S (s_gen (h x))) None.
Proof.
  intros h ix Ul x H. unfold teardown_heap, phase2, phase1. cbv beta.
  rewrite H. reflexivity.
Qed.

Lemma teardown_out_node : forall h ix Ul x nd, inU Ul x = false -> s_node (h x) = Some nd ->
  teardown_heap h ix Ul x = mkSlot (s_gen (h x)) (Some (clear_node Ul ix x nd)).
Proof.
  intros h ix Ul x nd H Hx. unfold teardown_heap, phase2, phase1. cbv beta.
  rewrite H, Hx. reflexivity.
Qed.

Lemma teardown_out_free : forall h ix Ul x, inU Ul x = false -> s_node (h x) = None ->
  teardown_heap h ix Ul x = h x.
Proof.
  intros h ix Ul x H Hx. unfold teardown_heap, phase2, phase1. cbv beta.
  rewrite H, Hx. reflexivity.
Qed.

Lemma field_at_teardown : forall h ix Ul src k,
  field_at (teardown_heap h ix Ul) src k =
  if inU Ul src then None else if hit Ul ix src k then None else field_at h src k.
Proof.
  intros h ix Ul src k. destruct (inU Ul src) eqn:Hu.
  - unfold field_at. rewrite (teardown_in h ix Ul src Hu). reflexivity.
  - destruct (s_node (h src)) as [nd|] eqn:Hs.
    + unfold field_at. rewrite (teardown_out_node h ix Ul src nd Hu Hs), Hs. reflexivity.
    + unfold field_at. rewrite (teardown_out_free h ix Ul src Hu Hs), Hs.
      destruct (hit Ul ix src k); reflexivity.
Qed.

Lemma teardown_live : forall h ix Ul y,
  live (teardown_heap h ix Ul) y <-> live h y /\ ~ In y Ul.
Proof.
  intros h ix Ul y. unfold live. destruct (inU Ul y) eqn:Hu.
  - rewrite (teardown_in h ix Ul y Hu). split.
    + intros [nd Hnd]. discriminate.
    + intros [_ Hn]. exfalso. apply Hn, inU_spec, Hu.
  - assert (Hn : ~ In y Ul) by (apply inU_false, Hu).
    destruct (s_node (h y)) as [nd|] eqn:Hs.
    + rewrite (teardown_out_node h ix Ul y nd Hu Hs). split.
      * intros _. split; [exists nd; reflexivity | exact Hn].
      * intros _. eexists. reflexivity.
    + rewrite (teardown_out_free h ix Ul y Hu Hs). split.
      * intros [nd Hnd]. congruence.
      * intros [[nd Hnd] _]. congruence.
Qed.

(** * Steps *)

Definition upd (h : Heap) (i : nat) (v : Slot) : Heap :=
  fun j => if Nat.eqb j i then v else h j.

Lemma upd_eq : forall h i v, upd h i v i = v.
Proof. intros h i v. unfold upd. rewrite Nat.eqb_refl. reflexivity. Qed.

Lemma upd_neq : forall h i v j, j <> i -> upd h i v j = h j.
Proof.
  intros h i v j H. unfold upd. apply Nat.eqb_neq in H. rewrite H. reflexivity.
Qed.

Definition empty_fields : nat -> option Link := fun _ => None.

(** A new node's owner: a live root, or a parent named by a resolving handle. *)
Definition owner_ok_new (h : Heap) (rs : list nat) (rg : nat -> nat) (o : Owner) (g : nat) : Prop :=
  match o with
  | ORoot r => root_resolves rs rg (r, g)
  | OParent p => resolves h (p, g)
  end.

(** Reparenting adds the ancestor check: the new parent is not in x's own
    subtree. *)
Definition attach_ok (h : Heap) (rs : list nat) (rg : nat -> nat) (x : nat) (o : Owner) (g : nat) : Prop :=
  match o with
  | ORoot r => root_resolves rs rg (r, g)
  | OParent p => resolves h (p, g) /\ ~ in_sub h x p
  end.

Definition link_ok (h : Heap) (ol : option Link) : Prop :=
  match ol with
  | Some l => resolves h l
  | None => True
  end.

Definition set_field (nd : Node) (k : nat) (ol : option Link) : Node :=
  mkNode (owner nd) (fun j => if Nat.eqb j k then ol else fields nd j).

Definition index_set_scan_spec (ix : Index) (x k : nat) (ol : option Link) : Index :=
  fun t => filter (fun e => negb (ent_eqb e (x, k))) (ix t) ++
           match ol with
           | Some l => if Nat.eqb t (fst l) then [(x, k)] else []
           | None => []
           end.

(** The admitted update reads the old field once and filters only its old
    target row. The scan is a reference specification, never a fallback. *)
Definition index_set (h : Heap) (ix : Index) (x k : nat) (ol : option Link) : Index :=
  let old := field_at h x k in
  fun t =>
    let row := match old with
     | Some l => if Nat.eqb t (fst l)
                 then filter (fun e => negb (ent_eqb e (x, k))) (ix t)
                 else ix t
     | None => ix t
     end in
    match ol with
    | Some l => if Nat.eqb t (fst l) then row ++ [(x, k)] else row
    | None => row
    end.

Lemma filter_entry_absent : forall row x k, ~ In (x, k) row ->
  filter (fun e => negb (ent_eqb e (x, k))) row = row.
Proof.
  intros row x k. induction row as [| e row IH]; intros H; simpl; [reflexivity |].
  assert (E : ent_eqb e (x, k) = false).
  { destruct (ent_eqb e (x, k)) eqn:E; [| reflexivity].
    apply ent_eqb_spec in E. subst e. exfalso. apply H; left; reflexivity. }
  rewrite E. simpl. f_equal. apply IH. intro Hin. apply H; right; exact Hin.
Qed.

Theorem index_set_refines_scan : forall h ix x k ol, IndexExact h ix ->
  forall t, index_set h ix x k ol t = index_set_scan_spec ix x k ol t.
Proof.
  intros h ix x k ol Hix t. unfold index_set, index_set_scan_spec.
  assert (HA : forall row,
    (match ol with Some l => if Nat.eqb t (fst l) then row ++ [(x,k)] else row
     | None => row end) = row ++
    (match ol with Some l => if Nat.eqb t (fst l) then [(x,k)] else [] | None => [] end)).
  { intros row. destruct ol as [l|]; [destruct (Nat.eqb t (fst l)) |];
      try reflexivity; symmetry; apply app_nil_r. }
  rewrite HA.
  destruct (field_at h x k) as [[old g]|] eqn:Hf.
  - simpl. destruct (Nat.eqb t old) eqn:Et; [reflexivity |].
    assert (Hno : ~ In (x, k) (ix t)).
    { intro Hin. apply Hix in Hin. destruct Hin as [g' Hg].
      rewrite Hf in Hg. injection Hg as E _. subst t.
      rewrite Nat.eqb_refl in Et. discriminate. }
    rewrite (filter_entry_absent _ x k Hno). reflexivity.
  - assert (Hno : ~ In (x, k) (ix t)).
    { intro Hin. apply Hix in Hin. destruct Hin as [g Hg]. congruence. }
    rewrite (filter_entry_absent _ x k Hno). reflexivity.
Qed.

Theorem index_set_frame : forall h ix x k ol t,
  (forall l, field_at h x k = Some l -> t <> fst l) ->
  (forall l, ol = Some l -> t <> fst l) ->
  index_set h ix x k ol t = ix t.
Proof.
  intros h ix x k ol t HOld HNew. unfold index_set.
  destruct (field_at h x k) as [l|] eqn:HO.
  - rewrite (proj2 (Nat.eqb_neq t (fst l)) (HOld l eq_refl)).
    destruct ol as [l'|]; [rewrite (proj2 (Nat.eqb_neq t (fst l')) (HNew l' eq_refl)) |];
      reflexivity.
  - destruct ol as [l'|]; [rewrite (proj2 (Nat.eqb_neq t (fst l')) (HNew l' eq_refl)) |];
      reflexivity.
Qed.

Definition owner_eqb (a b : Owner) : bool :=
  match a, b with
  | ORoot x, ORoot y => Nat.eqb x y
  | OParent x, OParent y => Nat.eqb x y
  | _, _ => false
  end.

Lemma owner_eqb_spec : forall a b, owner_eqb a b = true <-> a = b.
Proof.
  intros [x | x] [y | y]; simpl; rewrite ?Nat.eqb_eq;
    split; intro H; try discriminate; try congruence.
Qed.

Definition kids_add (kd : Kids) (o : Owner) (c : nat) : Kids :=
  fun o' => if owner_eqb o' o then c :: kd o' else kd o'.

Definition kids_move_scan_spec (kd : Kids) (x : nat) (o : Owner) : Kids :=
  kids_add (fun o' => filter (fun c => negb (Nat.eqb c x)) (kd o')) o x.

Definition kids_move (kd : Kids) (x : nat) (old o : Owner) : Kids :=
  kids_add (fun q => if owner_eqb q old
                    then filter (fun c => negb (Nat.eqb c x)) (kd q)
                    else kd q) o x.

Lemma filter_child_absent : forall row x, ~ In x row ->
  filter (fun c => negb (Nat.eqb c x)) row = row.
Proof.
  intros row x. induction row as [| c row IH]; intros H; simpl; [reflexivity |].
  assert (E : Nat.eqb c x = false).
  { apply Nat.eqb_neq. intro E. subst c. apply H; left; reflexivity. }
  rewrite E. simpl. f_equal. apply IH. intro Hin. apply H; right; exact Hin.
Qed.

Theorem kids_move_refines_scan : forall h kd x nd o,
  KidsExact h kd -> s_node (h x) = Some nd -> forall q,
  kids_move kd x (owner nd) o q = kids_move_scan_spec kd x o q.
Proof.
  intros h kd x nd o Hkd Hx q.
  unfold kids_move, kids_move_scan_spec, kids_add.
  assert (E : (if owner_eqb q (owner nd)
               then filter (fun c => negb (Nat.eqb c x)) (kd q) else kd q) =
              filter (fun c => negb (Nat.eqb c x)) (kd q)).
  { destruct (owner_eqb q (owner nd)) eqn:Eq; [reflexivity |].
    assert (Hno : ~ In x (kd q)).
    { intro Hin. apply Hkd in Hin. destruct Hin as [nd' [Hx' Ho]].
      rewrite Hx in Hx'. injection Hx' as En. subst nd'. subst q.
      rewrite (proj2 (owner_eqb_spec (owner nd) (owner nd)) eq_refl) in Eq.
      discriminate. }
    rewrite (filter_child_absent _ x Hno). reflexivity. }
  rewrite E. reflexivity.
Qed.

Theorem kids_move_frame : forall kd x old o q,
  q <> old -> q <> o -> kids_move kd x old o q = kd q.
Proof.
  intros kd x old o q HOld HNew. unfold kids_move, kids_add.
  assert (EOld : owner_eqb q old = false).
  { destruct (owner_eqb q old) eqn:E; [apply owner_eqb_spec in E; contradiction | reflexivity]. }
  assert (ENew : owner_eqb q o = false).
  { destruct (owner_eqb q o) eqn:E; [apply owner_eqb_spec in E; contradiction | reflexivity]. }
  rewrite EOld, ENew. reflexivity.
Qed.

(** Filter visits for one finite row projection. Common materialization,
    lookup representation, allocator and GC costs are not counted here. *)
Definition index_set_filter_visits (h : Heap) (ix : Index) (x k : nat) : nat :=
  match field_at h x k with Some l => length (ix (fst l)) | None => 0 end.
Definition kids_move_filter_visits (kd : Kids) (old : Owner) : nat := length (kd old).

Inductive Op : Type :=
| OpAlloc (i : nat) (o : Owner) (g : nat)
| OpSetField (x g k : nat) (ol : option Link)
| OpAttach (x g : nat) (o : Owner) (gp : nat)
| OpRelease (n g : nat) (Ul : list nat)
| OpRootNew (r : nat)
| OpRootDrop (r g : nat) (Ul : list nat).

Inductive Step : St -> Op -> St -> Prop :=
| StAlloc : forall s i o g,
    s_node (heap s i) = None ->
    owner_ok_new (heap s) (roots s) (root_gen s) o g ->
    Step s (OpAlloc i o g)
      (mkSt (upd (heap s) i (mkSlot (s_gen (heap s i)) (Some (mkNode o empty_fields))))
            (rix s) (kids_add (kids s) o i) (roots s) (Nat.max (bound s) (S i)) (root_gen s))
| StSetField : forall s x g nd k ol,
    resolves (heap s) (x, g) ->
    s_node (heap s x) = Some nd ->
    link_ok (heap s) ol ->
    Step s (OpSetField x g k ol)
      (mkSt (upd (heap s) x (mkSlot (s_gen (heap s x)) (Some (set_field nd k ol))))
            (index_set (heap s) (rix s) x k ol) (kids s) (roots s) (bound s) (root_gen s))
| StAttach : forall s x g nd o gp,
    resolves (heap s) (x, g) ->
    s_node (heap s x) = Some nd ->
    rooted (heap s) x ->
    attach_ok (heap s) (roots s) (root_gen s) x o gp ->
    Step s (OpAttach x g o gp)
      (mkSt (upd (heap s) x (mkSlot (s_gen (heap s x)) (Some (mkNode o (fields nd)))))
            (rix s) (kids_move (kids s) x (owner nd) o) (roots s) (bound s) (root_gen s))
| StRelease : forall s n g Ul,
    resolves (heap s) (n, g) ->
    rooted (heap s) n ->
    (forall y, In y Ul <-> in_sub (heap s) n y) ->
    NoDup Ul ->
    Step s (OpRelease n g Ul) (teardown s Ul)
| StRootNew : forall s r,
    ~ In r (roots s) ->
    Step s (OpRootNew r) (mkSt (heap s) (rix s) (kids s) (r :: roots s) (bound s) (root_gen s))
| StRootDrop : forall s r g Ul,
    root_resolves (roots s) (root_gen s) (r, g) ->
    (forall y, In y Ul <-> under_root (heap s) r y) ->
    NoDup Ul ->
    Step s (OpRootDrop r g Ul) (root_drop s r Ul).

Inductive Steps : St -> St -> Prop :=
| StepsRefl : forall s, Steps s s
| StepsCons : forall s1 op s2 s3, Step s1 op s2 -> Steps s2 s3 -> Steps s1 s3.

(** * Ownership lemmas *)

Lemma rooted_live : forall h x, rooted h x -> live h x.
Proof. intros h x H. destruct H as [x nd r Hx _ | x nd p Hx _ _]; exists nd; exact Hx. Qed.

Lemma in_sub_inv : forall h n y, in_sub h n y ->
  y = n \/ exists nd p, s_node (h y) = Some nd /\ owner nd = OParent p /\ in_sub h n p.
Proof.
  intros h n y H. destruct H as [| y nd p Hy Ho Hp];
    [left; reflexivity | right; exists nd, p; auto].
Qed.

Lemma in_sub_live : forall h n y, live h n -> in_sub h n y -> live h y.
Proof. intros h n y Hn H. destruct H as [| y nd p Hy _ _]; [exact Hn | exists nd; exact Hy]. Qed.

Lemma sub_of_rooted : forall h n y, rooted h n -> in_sub h n y -> rooted h y.
Proof.
  intros h n y Hn H. induction H as [| y nd p Hy Ho Hp IH]; [exact Hn |].
  eapply RPar; eauto.
Qed.

Lemma sub_dec_rooted : forall h n y, rooted h y -> in_sub h n y \/ ~ in_sub h n y.
Proof.
  intros h n y Hr. induction Hr as [y nd r Hy Ho | y nd p Hy Ho Hp IH].
  - destruct (Nat.eq_dec y n) as [-> | Hne]; [left; constructor |].
    right. intro Hs. destruct (in_sub_inv _ _ _ Hs) as [E | [nd' [p [Hy' [Ho' _]]]]].
    + contradiction.
    + rewrite Hy in Hy'. injection Hy' as E. subst nd'. rewrite Ho in Ho'. discriminate.
  - destruct (Nat.eq_dec y n) as [-> | Hne]; [left; constructor |].
    destruct IH as [IH | IH].
    + left. eapply SubDown; eauto.
    + right. intro Hs. destruct (in_sub_inv _ _ _ Hs) as [E | [nd' [p' [Hy' [Ho' Hp']]]]].
      * contradiction.
      * rewrite Hy in Hy'. injection Hy' as E. subst nd'. rewrite Ho in Ho'.
        injection Ho' as E'. subst p'. contradiction.
Qed.

Lemma sub_dec : forall h n y, OrphanFree h -> in_sub h n y \/ ~ in_sub h n y.
Proof.
  intros h n y Hof. destruct (s_node (h y)) as [nd|] eqn:Hy.
  - apply sub_dec_rooted, Hof. exists nd. exact Hy.
  - destruct (Nat.eq_dec y n) as [-> | Hne]; [left; constructor |].
    right. intro Hs. destruct (in_sub_inv _ _ _ Hs) as [E | [nd [p [Hy' _]]]].
    + contradiction.
    + congruence.
Qed.

Lemma under_root_inv : forall h r y, under_root h r y ->
  exists nd, s_node (h y) = Some nd /\
    (owner nd = ORoot r \/ exists p, owner nd = OParent p /\ under_root h r p).
Proof.
  intros h r y H. destruct H as [y nd Hy Ho | y nd p Hy Ho Hp].
  - exists nd. split; [exact Hy | left; exact Ho].
  - exists nd. split; [exact Hy | right; exists p; split; assumption].
Qed.

Lemma under_root_live : forall h r y, under_root h r y -> live h y.
Proof. intros h r y H. destruct H as [y nd Hy _ | y nd p Hy _ _]; exists nd; exact Hy. Qed.

Lemma under_root_rooted : forall h r y, under_root h r y -> rooted h y.
Proof.
  intros h r y H. induction H as [y nd Hy Ho | y nd p Hy Ho Hp IH];
    [eapply RRoot; eauto | eapply RPar; eauto].
Qed.

Lemma under_root_dec : forall h r y, OrphanFree h -> under_root h r y \/ ~ under_root h r y.
Proof.
  intros h r y Hof. destruct (s_node (h y)) as [nd0|] eqn:Hy0.
  - assert (Hr : rooted h y) by (apply Hof; exists nd0; exact Hy0).
    clear Hy0 nd0. induction Hr as [y nd r' Hy Ho | y nd p Hy Ho Hp IH].
    + destruct (Nat.eq_dec r' r) as [-> | Hne]; [left; eapply URHere; eauto |].
      right. intro H. destruct (under_root_inv _ _ _ H) as [nd' [Hy' [Ho' | [p [Ho' _]]]]];
        rewrite Hy in Hy'; injection Hy' as E; subst nd'; congruence.
    + destruct IH as [IH | IH]; [left; eapply URDown; eauto |].
      right. intro H. destruct (under_root_inv _ _ _ H) as [nd' [Hy' [Ho' | [p' [Ho' Hp']]]]];
        rewrite Hy in Hy'; injection Hy' as E; subst nd'.
      * congruence.
      * rewrite Ho in Ho'. injection Ho' as E. subst p'. contradiction.
  - right. intro H. destruct (under_root_live _ _ _ H) as [nd Hnd]. congruence.
Qed.

Lemma enum_below : forall (P : nat -> Prop) N, (forall x, P x \/ ~ P x) ->
  exists l, NoDup l /\ forall x, In x l <-> x < N /\ P x.
Proof.
  intros P N Hd. induction N as [| N [l [Hnd Hl]]].
  - exists []. split; [constructor |].
    intros x. simpl. split; [intros [] | intros [H _]; lia].
  - destruct (Hd N) as [HP | HnP].
    + exists (N :: l). split.
      * constructor; [intro Hin; apply Hl in Hin; lia | exact Hnd].
      * intros x. simpl. rewrite Hl. split.
        -- intros [<- | [Hlt HPx]]; [split; [lia | exact HP] | split; [lia | exact HPx]].
        -- intros [Hlt HPx]. destruct (Nat.eq_dec x N) as [-> | Hne];
          [left; reflexivity | right; split; [lia | exact HPx]].
    + exists l. split; [exact Hnd |]. intros x. rewrite Hl. split.
      * intros [Hlt HPx]. split; [lia | exact HPx].
      * intros [Hlt HPx]. destruct (Nat.eq_dec x N) as [-> | Hne];
          [contradiction | split; [lia | exact HPx]].
Qed.

Definition owners_kept (h h' : Heap) : Prop :=
  forall y nd, s_node (h y) = Some nd ->
    exists nd', s_node (h' y) = Some nd' /\ owner nd' = owner nd.

Lemma rooted_frame : forall h h' x, owners_kept h h' -> rooted h x -> rooted h' x.
Proof.
  intros h h' x Hk Hr. induction Hr as [x nd r Hx Ho | x nd p Hx Ho Hp IH].
  - destruct (Hk _ _ Hx) as [nd' [Hx' Ho']]. eapply RRoot; [exact Hx' | rewrite Ho'; exact Ho].
  - destruct (Hk _ _ Hx) as [nd' [Hx' Ho']].
    eapply RPar; [exact Hx' | rewrite Ho'; exact Ho | exact IH].
Qed.

Lemma attach_rooted_out : forall h x sl y,
  rooted h y -> ~ in_sub h x y -> rooted (upd h x sl) y.
Proof.
  intros h x sl y Hr. induction Hr as [y nd r Hy Ho | y nd p Hy Ho Hp IH]; intros Hn.
  - assert (y <> x) by (intro E; subst; apply Hn; constructor).
    eapply RRoot; [rewrite upd_neq by assumption; exact Hy | exact Ho].
  - assert (y <> x) by (intro E; subst; apply Hn; constructor).
    eapply RPar; [rewrite upd_neq by assumption; exact Hy | exact Ho |].
    apply IH. intro Hp'. apply Hn. eapply SubDown; eauto.
Qed.

Lemma attach_rooted_in : forall h x sl y,
  rooted (upd h x sl) x -> in_sub h x y -> rooted (upd h x sl) y.
Proof.
  intros h x sl y Hx Hs. induction Hs as [| y nd p Hy Ho Hp IH]; [exact Hx |].
  destruct (Nat.eq_dec y x) as [-> | Hne]; [exact Hx |].
  eapply RPar; [rewrite upd_neq by exact Hne; exact Hy | exact Ho | exact IH].
Qed.

(** A unit is down-closed if it holds every child of its members. Both
    release units (subtrees) and root-drop units are. *)
Definition DownClosed (h : Heap) (Ul : list nat) : Prop :=
  forall y nd p, s_node (h y) = Some nd -> owner nd = OParent p -> In p Ul -> In y Ul.

Lemma sub_unit_closed : forall h n Ul, (forall y, In y Ul <-> in_sub h n y) -> DownClosed h Ul.
Proof. intros h n Ul HU y nd p Hy Ho Hp. apply HU. apply HU in Hp. eapply SubDown; eauto. Qed.

Lemma root_unit_closed : forall h r Ul,
  (forall y, In y Ul <-> under_root h r y) -> DownClosed h Ul.
Proof. intros h r Ul HU y nd p Hy Ho Hp. apply HU. apply HU in Hp. eapply URDown; eauto. Qed.

Lemma teardown_rooted : forall h ix Ul y, DownClosed h Ul -> rooted h y -> ~ In y Ul ->
  rooted (teardown_heap h ix Ul) y.
Proof.
  intros h ix Ul y Hc Hr. induction Hr as [y nd r Hy Ho | y nd p Hy Ho Hp IH]; intros Hn.
  - assert (Hu : inU Ul y = false) by (apply inU_false, Hn).
    eapply RRoot; [rewrite (teardown_out_node h ix Ul y nd Hu Hy); reflexivity | exact Ho].
  - assert (Hu : inU Ul y = false) by (apply inU_false, Hn).
    eapply RPar; [rewrite (teardown_out_node h ix Ul y nd Hu Hy); reflexivity | exact Ho |].
    apply IH. intro Hpin. apply Hn. exact (Hc y nd p Hy Ho Hpin).
Qed.

(** The release unit is what the children index reaches from the node. *)
Inductive kreach (kd : Kids) (n : nat) : nat -> Prop :=
| KHere : kreach kd n n
| KDown : forall y p, In y (kd (OParent p)) -> kreach kd n p -> kreach kd n y.

Theorem unit_from_kids : forall h kd n y, KidsExact h kd -> (in_sub h n y <-> kreach kd n y).
Proof.
  intros h kd n y Hk. split.
  - intros H. induction H as [| y nd p Hy Ho Hp IH]; [constructor |].
    apply (KDown kd n y p); [apply Hk; exists nd; split; assumption | exact IH].
  - intros H. induction H as [| y p Hin Hp IH]; [constructor |].
    apply Hk in Hin. destruct Hin as [nd [Hy Ho]]. eapply SubDown; eauto.
Qed.

(** The root-drop unit is what the index reaches from the root's own list. *)
Inductive rreach (kd : Kids) (r : nat) : nat -> Prop :=
| RKHere : forall y, In y (kd (ORoot r)) -> rreach kd r y
| RKDown : forall y p, In y (kd (OParent p)) -> rreach kd r p -> rreach kd r y.

Theorem root_unit_from_kids : forall h kd r y, KidsExact h kd ->
  (under_root h r y <-> rreach kd r y).
Proof.
  intros h kd r y Hk. split.
  - intros H. induction H as [y nd Hy Ho | y nd p Hy Ho Hp IH].
    + apply RKHere. apply Hk. exists nd. split; assumption.
    + apply (RKDown kd r y p); [apply Hk; exists nd; split; assumption | exact IH].
  - intros H. induction H as [y Hin | y p Hin Hp IH].
    + apply Hk in Hin. destruct Hin as [nd [Hy Ho]]. eapply URHere; eauto.
    + apply Hk in Hin. destruct Hin as [nd [Hy Ho]]. eapply URDown; eauto.
Qed.

(** No orphan, stated directly: every live node belongs to a live root. *)
Lemma rooted_under_root : forall h x, rooted h x -> exists r, under_root h r x.
Proof.
  intros h x H. induction H as [x nd r Hx Ho | x nd p Hx Ho Hp [r IH]].
  - exists r. eapply URHere; eauto.
  - exists r. eapply URDown; eauto.
Qed.

Lemma under_root_root_live : forall h rs r x, RootsLive h rs -> under_root h r x -> In r rs.
Proof.
  intros h rs r x Hrl H. induction H as [x nd Hx Ho | x nd p Hx Ho Hp IH];
    [exact (Hrl x nd r Hx Ho) | exact IH].
Qed.

(** * The teardown theorem *)

Theorem index_finds_every_incoming : forall h ix Ul src k, IndexExact h ix ->
  hit Ul ix src k = true <-> exists t g, field_at h src k = Some (t, g) /\ In t Ul.
Proof.
  intros h ix Ul src k Hix. rewrite hit_spec. split.
  - intros [t [Ht Hin]]. apply Hix in Hin. destruct Hin as [g Hg]. exists t, g. auto.
  - intros [t [g [Hg Ht]]]. exists t. split; [exact Ht | apply Hix; exists g; exact Hg].
Qed.

(** The index-driven teardown equals the scan specification. *)
Theorem release_field_effect : forall h ix Ul src k, IndexExact h ix ->
  field_at (teardown_heap h ix Ul) src k =
  if inU Ul src then None
  else match field_at h src k with
       | Some (t, g) => if inU Ul t then None else Some (t, g)
       | None => None
       end.
Proof.
  intros h ix Ul src k Hix. rewrite field_at_teardown.
  destruct (inU Ul src); [reflexivity |].
  destruct (field_at h src k) as [[t g]|] eqn:Hf.
  - destruct (hit Ul ix src k) eqn:Hh.
    + apply (index_finds_every_incoming h ix Ul src k Hix) in Hh.
      destruct Hh as [t' [g' [Hf' Ht']]]. rewrite Hf in Hf'.
      injection Hf' as E1 E2. subst t' g'. apply inU_spec in Ht'. rewrite Ht'. reflexivity.
    + destruct (inU Ul t) eqn:Ht; [| reflexivity]. exfalso.
      assert (Hh' : hit Ul ix src k = true).
      { apply (index_finds_every_incoming h ix Ul src k Hix).
        exists t, g. split; [exact Hf | apply inU_spec; exact Ht]. }
      congruence.
  - destruct (hit Ul ix src k); reflexivity.
Qed.

Theorem release_clears_incoming : forall h ix Ul t, IndexExact h ix -> In t Ul ->
  forall src k g, field_at (teardown_heap h ix Ul) src k <> Some (t, g).
Proof.
  intros h ix Ul t Hix Ht src k g. rewrite (release_field_effect h ix Ul src k Hix).
  apply inU_spec in Ht. destruct (inU Ul src); [discriminate |].
  destruct (field_at h src k) as [[t0 g0]|]; [| discriminate].
  destruct (inU Ul t0) eqn:E; [discriminate |].
  intro H. injection H as E1 E2. subst. congruence.
Qed.

Theorem release_frees_exactly_subtree : forall h ix Ul n,
  (forall y, In y Ul <-> in_sub h n y) ->
  forall y, live (teardown_heap h ix Ul) y <-> live h y /\ ~ in_sub h n y.
Proof.
  intros h ix Ul n HU y. rewrite teardown_live. split.
  - intros [Hl Hn]. split; [exact Hl | intro Hs; apply Hn, HU, Hs].
  - intros [Hl Hn]. split; [exact Hl | intro Hin; apply Hn, HU, Hin].
Qed.

Theorem root_drop_frees_exactly : forall s r Ul,
  (forall y, In y Ul <-> under_root (heap s) r y) ->
  forall y, live (heap (root_drop s r Ul)) y <-> live (heap s) y /\ ~ under_root (heap s) r y.
Proof.
  intros s r Ul HU y. rewrite heap_root_drop, teardown_live. split.
  - intros [Hl Hn]. split; [exact Hl | intro Hs; apply Hn, HU, Hs].
  - intros [Hl Hn]. split; [exact Hl | intro Hin; apply Hn, HU, Hin].
Qed.

(** No double free in one step: the unit holds only live nodes. *)
Theorem release_frees_only_live : forall h n Ul, live h n ->
  (forall y, In y Ul <-> in_sub h n y) -> forall y, In y Ul -> live h y.
Proof. intros h n Ul Hn HU y Hy. apply HU in Hy. exact (in_sub_live h n y Hn Hy). Qed.

Theorem release_frame : forall h ix Ul y nd, inU Ul y = false -> s_node (h y) = Some nd ->
  s_gen (teardown_heap h ix Ul y) = s_gen (h y) /\
  owner_at (teardown_heap h ix Ul) y = Some (owner nd).
Proof.
  intros h ix Ul y nd Hu Hs. unfold owner_at.
  rewrite (teardown_out_node h ix Ul y nd Hu Hs). split; reflexivity.
Qed.

(** ** Invariant preservation *)

Lemma inv_alloc_h : forall h ix kd rs rg b i o g,
  InvH h ix kd rs b -> s_node (h i) = None -> owner_ok_new h rs rg o g ->
  InvH (upd h i (mkSlot (s_gen (h i)) (Some (mkNode o empty_fields))))
       ix (kids_add kd o i) rs (Nat.max b (S i)).
Proof.
  intros h ix kd rs rg b i o g [Hof [Hrl [Hlr [Hix [Hkd Hb]]]]] Hi Ho.
  set (h' := upd h i (mkSlot (s_gen (h i)) (Some (mkNode o empty_fields)))).
  assert (Hfa : forall src k, field_at h' src k = field_at h src k).
  { intros src k. unfold field_at, h'. destruct (Nat.eq_dec src i) as [-> | Hne].
    - rewrite upd_eq, Hi. reflexivity.
    - rewrite upd_neq by exact Hne. reflexivity. }
  assert (Hkeep : owners_kept h h').
  { intros y nd Hy. assert (y <> i) by (intro E; subst; congruence).
    exists nd. unfold h'. rewrite upd_neq by assumption. auto. }
  assert (Hold : forall t, live h t -> s_gen (h' t) = s_gen (h t) /\ live h' t).
  { intros t [nd Ht]. assert (t <> i) by (intro E; subst; congruence).
    unfold h', live. rewrite upd_neq by assumption. split; [reflexivity | exists nd; exact Ht]. }
  split; [| split; [| split; [| split; [| split]]]].
  - intros y [nd Hy]. destruct (Nat.eq_dec y i) as [-> | Hne].
    + destruct o as [r | p].
      * eapply RRoot; [unfold h'; rewrite upd_eq; reflexivity | reflexivity].
      * eapply RPar; [unfold h'; rewrite upd_eq; reflexivity | reflexivity |].
        apply (rooted_frame h h' p Hkeep). apply Hof. exact (proj2 Ho).
    + apply (rooted_frame h h' y Hkeep). apply Hof. exists nd.
      unfold h' in Hy. rewrite upd_neq in Hy by exact Hne. exact Hy.
  - intros x nd r Hx Hor. destruct (Nat.eq_dec x i) as [-> | Hne].
    + unfold h' in Hx. rewrite upd_eq in Hx. simpl in Hx. injection Hx as E. subst nd.
      simpl in Hor. subst o. exact (proj1 Ho).
    + unfold h' in Hx. rewrite upd_neq in Hx by exact Hne. exact (Hrl x nd r Hx Hor).
  - intros src k l Hl. rewrite Hfa in Hl. destruct (Hlr src k l Hl) as [Hg Hlv].
    destruct (Hold (fst l) Hlv) as [Hg' Hlv']. split; [rewrite Hg'; exact Hg | exact Hlv'].
  - intros t src k. rewrite (Hix t src k).
    split; intros [g' Hg]; exists g'; [rewrite Hfa | rewrite <- Hfa]; exact Hg.
  - intros q c. unfold kids_add. destruct (Nat.eq_dec c i) as [-> | Hne].
    + unfold h'. rewrite upd_eq. simpl. destruct (owner_eqb q o) eqn:Eq.
      * apply owner_eqb_spec in Eq. subst q. split.
        -- intros _. eexists. split; reflexivity.
        -- intros _. left. reflexivity.
      * split.
        -- intros Hin. apply Hkd in Hin. destruct Hin as [nd [Hnd _]]. congruence.
        -- intros [nd [E Hon]]. injection E as E'. subst nd. simpl in Hon. subst q.
           rewrite (proj2 (owner_eqb_spec o o) eq_refl) in Eq. discriminate.
    + unfold h'. rewrite upd_neq by exact Hne. rewrite <- (Hkd q c).
      destruct (owner_eqb q o); [| reflexivity].
      simpl. split; [intros [E | H]; [congruence | exact H] | intros H; right; exact H].
  - intros y [nd Hy]. destruct (Nat.eq_dec y i) as [-> | Hne]; [lia |].
    unfold h' in Hy. rewrite upd_neq in Hy by exact Hne.
    pose proof (Hb y (ex_intro _ nd Hy)). lia.
Qed.

Lemma inv_setfield_h : forall h ix kd rs b x nd k ol,
  InvH h ix kd rs b -> s_node (h x) = Some nd -> link_ok h ol ->
  InvH (upd h x (mkSlot (s_gen (h x)) (Some (set_field nd k ol))))
       (index_set h ix x k ol) kd rs b.
Proof.
  intros h ix kd rs b x nd k ol [Hof [Hrl [Hlr [Hix [Hkd Hb]]]]] Hx Hl.
  set (h' := upd h x (mkSlot (s_gen (h x)) (Some (set_field nd k ol)))).
  assert (Hgen : forall t, s_gen (h' t) = s_gen (h t)).
  { intros t. unfold h'. destruct (Nat.eq_dec t x) as [-> | Hne];
      [rewrite upd_eq; reflexivity | rewrite upd_neq by exact Hne; reflexivity]. }
  assert (Hlive : forall t, live h' t <-> live h t).
  { intros t. unfold h', live. destruct (Nat.eq_dec t x) as [-> | Hne].
    - rewrite upd_eq. simpl. split; intros _; [exists nd; exact Hx | eexists; reflexivity].
    - rewrite upd_neq by exact Hne. reflexivity. }
  assert (Hres : forall l, resolves h l -> resolves h' l).
  { intros [t g] [Hg Hlv]. split; [rewrite Hgen; exact Hg | apply Hlive; exact Hlv]. }
  assert (Hfx : forall j, field_at h' x j = if Nat.eqb j k then ol else fields nd j).
  { intros j. unfold field_at, h'. rewrite upd_eq. reflexivity. }
  assert (Hfo : forall src j, src <> x -> field_at h' src j = field_at h src j).
  { intros src j Hne. unfold field_at, h'. rewrite upd_neq by exact Hne. reflexivity. }
  assert (Hhx : forall j, field_at h x j = fields nd j).
  { intros j. unfold field_at. rewrite Hx. reflexivity. }
  assert (Hown : owners_kept h h').
  { intros z ndz Hz. unfold h'. destruct (Nat.eq_dec z x) as [-> | Hne].
    - rewrite upd_eq. exists (set_field nd k ol). split; [reflexivity |].
      rewrite Hx in Hz. injection Hz as E. subst ndz. reflexivity.
    - rewrite upd_neq by exact Hne. exists ndz. split; [exact Hz | reflexivity]. }
  assert (Happ : forall t src j, (src, j) <> (x, k) ->
    ~ In (src, j) (match ol with
                   | Some l => if Nat.eqb t (fst l) then [(x, k)] else []
                   | None => []
                   end)).
  { intros t src j Hne. destruct ol as [l|]; [destruct (Nat.eqb t (fst l)) |];
      simpl; [intros [E | []]; congruence | intros [] | intros []]. }
  assert (Hneg : forall src j, (src, j) <> (x, k) -> negb (ent_eqb (src, j) (x, k)) = true).
  { intros src j Hne. destruct (ent_eqb (src, j) (x, k)) eqn:E; [| reflexivity].
    apply ent_eqb_spec in E. contradiction. }
  split; [| split; [| split; [| split; [| split]]]].
  - intros y Hy. apply (rooted_frame h h' y Hown). apply Hof, Hlive, Hy.
  - intros y ndy r Hy Hor. destruct (Nat.eq_dec y x) as [-> | Hne].
    + unfold h' in Hy. rewrite upd_eq in Hy. simpl in Hy. injection Hy as E. subst ndy.
      exact (Hrl x nd r Hx Hor).
    + unfold h' in Hy. rewrite upd_neq in Hy by exact Hne. exact (Hrl y ndy r Hy Hor).
  - intros src j l Hfl. apply Hres. destruct (Nat.eq_dec src x) as [-> | Hne].
    + rewrite Hfx in Hfl. destruct (Nat.eqb j k).
      * subst ol. exact Hl.
      * apply (Hlr x j l). rewrite Hhx. exact Hfl.
    + rewrite (Hfo src j Hne) in Hfl. exact (Hlr src j l Hfl).
  - intros t src j. rewrite (index_set_refines_scan h ix x k ol Hix t).
    unfold index_set_scan_spec. rewrite in_app_iff, filter_In.
    destruct (Nat.eq_dec src x) as [-> | Hne].
    + rewrite Hfx. destruct (Nat.eq_dec j k) as [-> | Hjk].
      * rewrite Nat.eqb_refl.
        assert (Hself : negb (ent_eqb (x, k) (x, k)) = false).
        { rewrite (proj2 (ent_eqb_spec (x, k) (x, k)) eq_refl). reflexivity. }
        rewrite Hself. destruct ol as [[t' g']|]; simpl.
        -- destruct (Nat.eq_dec t t') as [-> | Htt].
           ++ rewrite Nat.eqb_refl. simpl. split.
              ** intros [[_ F] | [E | []]]; [discriminate | exists g'; reflexivity].
              ** intros _. right. left. reflexivity.
           ++ rewrite (proj2 (Nat.eqb_neq t t') Htt). simpl. split.
              ** intros [[_ F] | []]. discriminate.
              ** intros [g E]. injection E as E1 E2. congruence.
        -- split; [intros [[_ F] | []]; discriminate | intros [g E]; discriminate].
      * rewrite (proj2 (Nat.eqb_neq j k) Hjk).
        assert (Hjne : (x, j) <> (x, k)) by (intro E; injection E; auto).
        rewrite (Hneg x j Hjne). rewrite (Hix t x j). rewrite <- (Hhx j). split.
        -- intros [[H _] | H]; [exact H | exfalso; exact (Happ t x j Hjne H)].
        -- intros H. left. split; [exact H | reflexivity].
    + rewrite (Hfo src j Hne).
      assert (Hsne : (src, j) <> (x, k)) by (intro E; injection E; auto).
      rewrite (Hneg src j Hsne). rewrite (Hix t src j). split.
      * intros [[H _] | H]; [exact H | exfalso; exact (Happ t src j Hsne H)].
      * intros H. left. split; [exact H | reflexivity].
  - intros q c. rewrite (Hkd q c). destruct (Nat.eq_dec c x) as [-> | Hne].
    + unfold h'. rewrite upd_eq. simpl. split.
      * intros [nd' [E Ho]]. rewrite Hx in E. injection E as E'. subst nd'.
        exists (set_field nd k ol). split; [reflexivity | exact Ho].
      * intros [nd' [E Ho]]. injection E as E'. subst nd'. exists nd. split; [exact Hx | exact Ho].
    + unfold h'. rewrite upd_neq by exact Hne. reflexivity.
  - intros y Hy. apply Hb, Hlive, Hy.
Qed.

Lemma inv_attach_h : forall h ix kd rs rg b x nd o gp,
  InvH h ix kd rs b -> s_node (h x) = Some nd -> attach_ok h rs rg x o gp ->
  InvH (upd h x (mkSlot (s_gen (h x)) (Some (mkNode o (fields nd)))))
       ix (kids_move kd x (owner nd) o) rs b.
Proof.
  intros h ix kd rs rg b x nd o gp [Hof [Hrl [Hlr [Hix [Hkd Hb]]]]] Hx Ha.
  set (sl := mkSlot (s_gen (h x)) (Some (mkNode o (fields nd)))).
  set (h' := upd h x sl).
  assert (Hgen : forall t, s_gen (h' t) = s_gen (h t)).
  { intros t. unfold h'. destruct (Nat.eq_dec t x) as [-> | Hne];
      [rewrite upd_eq; reflexivity | rewrite upd_neq by exact Hne; reflexivity]. }
  assert (Hlive : forall t, live h' t <-> live h t).
  { intros t. unfold h', live. destruct (Nat.eq_dec t x) as [-> | Hne].
    - rewrite upd_eq. simpl. split; intros _; [exists nd; exact Hx | eexists; reflexivity].
    - rewrite upd_neq by exact Hne. reflexivity. }
  assert (Hres : forall l, resolves h l -> resolves h' l).
  { intros [t g] [Hg Hlv]. split; [rewrite Hgen; exact Hg | apply Hlive; exact Hlv]. }
  assert (Hfa : forall src j, field_at h' src j = field_at h src j).
  { intros src j. unfold field_at, h'. destruct (Nat.eq_dec src x) as [-> | Hne];
      [rewrite upd_eq, Hx; reflexivity | rewrite upd_neq by exact Hne; reflexivity]. }
  assert (Hxr : rooted h' x).
  { destruct o as [r | p].
    - eapply RRoot; [unfold h'; rewrite upd_eq; reflexivity | reflexivity].
    - destruct Ha as [[_ Hp] Hnp].
      eapply RPar; [unfold h'; rewrite upd_eq; reflexivity | reflexivity |].
      apply attach_rooted_out; [apply Hof, Hp | exact Hnp]. }
  split; [| split; [| split; [| split; [| split]]]].
  - intros y Hy. assert (Hy0 : live h y) by (apply Hlive, Hy).
    destruct (sub_dec h x y Hof) as [Hs | Hs].
    + apply attach_rooted_in; assumption.
    + apply attach_rooted_out; [apply Hof, Hy0 | exact Hs].
  - intros y ndy r Hy Hor. destruct (Nat.eq_dec y x) as [-> | Hne].
    + unfold h', sl in Hy. rewrite upd_eq in Hy. simpl in Hy. injection Hy as E. subst ndy.
      simpl in Hor. subst o. exact (proj1 Ha).
    + unfold h' in Hy. rewrite upd_neq in Hy by exact Hne. exact (Hrl y ndy r Hy Hor).
  - intros src j l Hfl. rewrite Hfa in Hfl. apply Hres, (Hlr src j l Hfl).
  - intros t src j. rewrite (Hix t src j).
    split; intros [g Hg]; exists g; [rewrite Hfa | rewrite <- Hfa]; exact Hg.
  - intros q c. rewrite (kids_move_refines_scan h kd x nd o Hkd Hx q).
    unfold kids_move_scan_spec, kids_add. destruct (Nat.eq_dec c x) as [-> | Hne].
    + unfold h', sl. rewrite upd_eq. simpl. destruct (owner_eqb q o) eqn:Eq.
      * apply owner_eqb_spec in Eq. subst q. split.
        -- intros _. eexists. split; reflexivity.
        -- intros _. left. reflexivity.
      * split.
        -- intros H. exfalso. exact (not_in_filter_self x (kd q) H).
        -- intros [nd' [E Ho]]. injection E as E'. subst nd'. simpl in Ho. subst q.
           rewrite (proj2 (owner_eqb_spec o o) eq_refl) in Eq. discriminate.
    + unfold h'. rewrite upd_neq by exact Hne. rewrite <- (Hkd q c).
      destruct (owner_eqb q o).
      * simpl. rewrite (in_filter_neq c x (kd q) Hne).
        split; [intros [E | H]; [congruence | exact H] | intros H; right; exact H].
      * apply in_filter_neq, Hne.
  - intros y Hy. apply Hb, Hlive, Hy.
Qed.

Lemma inv_teardown_h : forall h ix kd rs rs' b Ul,
  InvH h ix kd rs b -> DownClosed h Ul ->
  (forall x nd r, s_node (h x) = Some nd -> owner nd = ORoot r -> ~ In x Ul -> In r rs') ->
  InvH (teardown_heap h ix Ul) (teardown_index ix Ul) (teardown_kids kd Ul) rs' b.
Proof.
  intros h ix kd rs rs' b Ul [Hof [Hrl [Hlr [Hix [Hkd Hb]]]]] Hc Hroot.
  assert (Hlive_out : forall y, live (teardown_heap h ix Ul) y -> ~ In y Ul /\ live h y).
  { intros y Hy. apply teardown_live in Hy. tauto. }
  assert (Hsurv : forall y ndy, s_node (teardown_heap h ix Ul y) = Some ndy ->
    exists nd, s_node (h y) = Some nd /\ ~ In y Ul /\ ndy = clear_node Ul ix y nd).
  { intros y ndy Hy. destruct (inU Ul y) eqn:Hu.
    - rewrite (teardown_in h ix Ul y Hu) in Hy. discriminate.
    - destruct (s_node (h y)) as [nd|] eqn:Hs.
      + rewrite (teardown_out_node h ix Ul y nd Hu Hs) in Hy. simpl in Hy. injection Hy as E.
        exists nd. split; [reflexivity | split; [apply inU_false, Hu | symmetry; exact E]].
      + rewrite (teardown_out_free h ix Ul y Hu Hs) in Hy. congruence. }
  split; [| split; [| split; [| split; [| split]]]].
  - intros y Hy. destruct (Hlive_out y Hy) as [Hn Hy0].
    apply teardown_rooted; [exact Hc | apply Hof, Hy0 | exact Hn].
  - intros y ndy r Hy Hor. destruct (Hsurv y ndy Hy) as [nd [Hs [Hn E]]]. subst ndy.
    exact (Hroot y nd r Hs Hor Hn).
  - intros src k [t g] Hfl. rewrite (release_field_effect h ix Ul src k Hix) in Hfl.
    destruct (inU Ul src) eqn:Hus; [discriminate |].
    destruct (field_at h src k) as [[t0 g0]|] eqn:Hf; [| discriminate].
    destruct (inU Ul t0) eqn:Hut; [discriminate |].
    injection Hfl as E1 E2. subst t0 g0.
    destruct (Hlr src k (t, g) Hf) as [Hg [nd Hnd]]. simpl in Hg, Hnd.
    split; simpl.
    + rewrite (teardown_out_node h ix Ul t nd Hut Hnd). exact Hg.
    + exists (clear_node Ul ix t nd). rewrite (teardown_out_node h ix Ul t nd Hut Hnd).
      reflexivity.
  - intros t src k. rewrite (release_field_effect h ix Ul src k Hix). unfold teardown_index.
    destruct (inU Ul t) eqn:Hut.
    + split; [intros [] |]. intros [g Hg]. destruct (inU Ul src); [discriminate |].
      destruct (field_at h src k) as [[t0 g0]|] eqn:Hf; [| discriminate].
      destruct (inU Ul t0) eqn:Hut0; [discriminate |].
      injection Hg as E1 E2. subst t0 g0. congruence.
    + rewrite filter_In. simpl. rewrite (Hix t src k).
      destruct (inU Ul src) eqn:Hus; simpl.
      * split; [intros [_ F]; discriminate | intros [g Hg]; discriminate].
      * split.
        -- intros [[g Hg] _]. exists g. rewrite Hg, Hut. reflexivity.
        -- intros [g Hg]. destruct (field_at h src k) as [[t0 g0]|] eqn:Hf; [| discriminate].
           destruct (inU Ul t0) eqn:Hut0; [discriminate |].
           injection Hg as E1 E2. subst t0 g0.
           split; [exists g; reflexivity | reflexivity].
  - intros q c. unfold teardown_kids.
    rewrite filter_In. rewrite (Hkd q c). destruct (inU Ul c) eqn:Hu; simpl.
    + split; [intros [_ F]; discriminate |]. intros [nd' [Hy _]].
      rewrite (teardown_in h ix Ul c Hu) in Hy. discriminate.
    + split.
      * intros [[nd [Hs Ho]] _]. exists (clear_node Ul ix c nd).
        rewrite (teardown_out_node h ix Ul c nd Hu Hs). split; [reflexivity | exact Ho].
      * intros [nd' [Hy Ho]]. destruct (Hsurv c nd' Hy) as [nd [Hs [Hn E]]]. subst nd'.
        split; [exists nd; split; [exact Hs | exact Ho] | reflexivity].
  - intros y Hy. apply Hb, (proj2 (Hlive_out y Hy)).
Qed.

Theorem inv_empty : Inv (mkSt (fun _ => mkSlot 0 None) (fun _ => []) (fun _ => []) [] 0 initial_root_generations).
Proof.
  unfold Inv, InvH; cbn [heap rix kids roots bound].
  split; [| split; [| split; [| split; [| split]]]].
  - intros x [nd H]. discriminate.
  - intros x nd r H. discriminate.
  - intros src k l H. discriminate.
  - intros t src k. split; [intros [] | intros [g H]; discriminate].
  - intros p c. split; [intros [] | intros [nd [H _]]; discriminate].
  - intros x [nd H]. discriminate.
Qed.

Theorem inv_step : forall s op s', Inv s -> Step s op s' -> Inv s'.
Proof.
  intros s op s' HI HS. unfold Inv in *.
  destruct HS as [s i o g Hi Ho | s x g nd k ol Hr Hx Hl | s x g nd o gp Hr Hx Hro Ha
                 | s n g Ul Hr Hro HU Hnd | s r Hr | s r g Ul Hr HU Hnd];
    cbn [heap rix kids roots bound].
  - apply inv_alloc_h with (rg := root_gen s) (g := g); assumption.
  - apply inv_setfield_h; assumption.
  - apply inv_attach_h with (rg := root_gen s) (gp := gp); assumption.
  - unfold teardown; cbn [heap rix kids roots bound].
    apply (inv_teardown_h _ _ _ (roots s) _ _ _ HI (sub_unit_closed _ n Ul HU)).
    destruct HI as [_ [Hrl _]]. intros x nd r Hx Hor _. exact (Hrl x nd r Hx Hor).
  - destruct HI as [Hof [Hrl [Hlr [Hix [Hkd Hb]]]]].
    split; [exact Hof | split; [| split; [exact Hlr | split; [exact Hix | split; [exact Hkd | exact Hb]]]]].
    intros x nd r' Hx Hor. right. exact (Hrl x nd r' Hx Hor).
  - unfold root_drop; cbn [heap rix kids roots bound].
    apply (inv_teardown_h _ _ _ (roots s) _ _ _ HI (root_unit_closed _ r Ul HU)).
    destruct HI as [_ [Hrl _]]. intros x nd r' Hx Hor Hn.
    destruct (Nat.eq_dec r' r) as [-> | Hne].
    + exfalso. apply Hn, HU. eapply URHere; eauto.
    + apply in_in_remove; [exact Hne | exact (Hrl x nd r' Hx Hor)].
Qed.

Theorem inv_steps : forall s s', Inv s -> Steps s s' -> Inv s'.
Proof.
  intros s s' HI HS. induction HS as [s | s1 op s2 s3 H12 H23 IH]; [exact HI |].
  apply IH. exact (inv_step s1 op s2 HI H12).
Qed.

Corollary orphan_free_forever : forall s s', Inv s -> Steps s s' -> OrphanFree (heap s').
Proof. intros s s' HI HS. exact (proj1 (inv_steps s s' HI HS)). Qed.

Corollary links_never_dangle : forall s s', Inv s -> Steps s s' -> LinksResolve (heap s').
Proof. intros s s' HI HS. exact (proj1 (proj2 (proj2 (inv_steps s s' HI HS)))). Qed.

(** ** Release and root drop always succeed *)

Theorem release_always_succeeds : forall s n g, Inv s -> resolves (heap s) (n, g) ->
  exists Ul, (forall y, In y Ul <-> in_sub (heap s) n y) /\
             Step s (OpRelease n g Ul) (teardown s Ul) /\ Inv (teardown s Ul).
Proof.
  intros s n g HI Hng. pose proof HI as [Hof [_ [_ [_ [_ Hb]]]]].
  assert (Hn : live (heap s) n) by exact (proj2 Hng).
  destruct (enum_below (in_sub (heap s) n) (bound s) (fun y => sub_dec _ n y Hof))
    as [Ul [Hnd HUl]].
  assert (HU : forall y, In y Ul <-> in_sub (heap s) n y).
  { intros y. rewrite HUl. split; [intros [_ H]; exact H |].
    intros H. split; [apply Hb, (in_sub_live _ _ _ Hn H) | exact H]. }
  exists Ul. split; [exact HU |].
  assert (HS : Step s (OpRelease n g Ul) (teardown s Ul)).
  { apply StRelease; [exact Hng | apply Hof, Hn | exact HU | exact Hnd]. }
  split; [exact HS | exact (inv_step _ _ _ HI HS)].
Qed.

Theorem root_drop_always_succeeds : forall s r, Inv s -> In r (roots s) ->
  exists Ul, (forall y, In y Ul <-> under_root (heap s) r y) /\
             Step s (OpRootDrop r (root_gen s r) Ul) (root_drop s r Ul) /\ Inv (root_drop s r Ul).
Proof.
  intros s r HI Hr. pose proof HI as [Hof [_ [_ [_ [_ Hb]]]]].
  destruct (enum_below (under_root (heap s) r) (bound s) (fun y => under_root_dec _ r y Hof))
    as [Ul [Hnd HUl]].
  assert (HU : forall y, In y Ul <-> under_root (heap s) r y).
  { intros y. rewrite HUl. split; [intros [_ H]; exact H |].
    intros H. split; [apply Hb, (under_root_live _ _ _ H) | exact H]. }
  exists Ul. split; [exact HU |].
  assert (Hroot : root_resolves (roots s) (root_gen s) (r, root_gen s r)) by (split; [exact Hr | reflexivity]).
  assert (HS : Step s (OpRootDrop r (root_gen s r) Ul) (root_drop s r Ul)) by (apply StRootDrop; assumption).
  split; [exact HS | exact (inv_step _ _ _ HI HS)].
Qed.

(** Once a root goes out of scope, no surviving node is owned by it. *)
Theorem root_drop_leaves_nothing_owned : forall s r g Ul s',
  Inv s -> Step s (OpRootDrop r g Ul) s' ->
  forall y nd, s_node (heap s' y) = Some nd -> owner nd <> ORoot r.
Proof.
  intros s r g Ul s' HI HS y nd Hy Ho.
  pose proof (inv_step _ _ _ HI HS) as [_ [Hrl _]].
  pose proof (Hrl y nd r Hy Ho) as Hin.
  inversion HS; subst. rewrite roots_root_drop in Hin.
  exact (remove_In Nat.eq_dec (roots s) r Hin).
Qed.

(** No orphan, in the program's terms: every live node belongs to a root
    variable that is still in scope. *)
Theorem every_node_has_live_root : forall s x, Inv s -> live (heap s) x ->
  exists r, In r (roots s) /\ under_root (heap s) r x.
Proof.
  intros s x [Hof [Hrl _]] Hx. destruct (rooted_under_root _ _ (Hof x Hx)) as [r Hu].
  exists r. split; [exact (under_root_root_live _ _ _ _ Hrl Hu) | exact Hu].
Qed.

Corollary no_roots_no_nodes : forall s, Inv s -> roots s = [] -> forall x, ~ live (heap s) x.
Proof.
  intros s HI E x Hx. destruct (every_node_has_live_root s x HI Hx) as [r [Hr _]].
  rewrite E in Hr. destruct Hr.
Qed.

(** RootNew retains the retired epoch. The reused name starts empty: no
    node of the old root is adopted. *)
Theorem root_reuse_adopts_nothing : forall s r g Ul s1 s2,
  Inv s -> Step s (OpRootDrop r g Ul) s1 -> Step s1 (OpRootNew r) s2 ->
  forall y nd, s_node (heap s2 y) = Some nd -> owner nd <> ORoot r.
Proof.
  intros s r g Ul s1 s2 HI H1 H2 y nd Hy.
  inversion H2; subst. cbn [heap] in Hy.
  exact (root_drop_leaves_nothing_owned s r g Ul s1 HI H1 y nd Hy).
Qed.

Theorem teardown_deterministic : forall h ix kd Ul1 Ul2, IndexExact h ix ->
  (forall y, In y Ul1 <-> In y Ul2) ->
  forall x, s_gen (teardown_heap h ix Ul1 x) = s_gen (teardown_heap h ix Ul2 x) /\
            owner_at (teardown_heap h ix Ul1) x = owner_at (teardown_heap h ix Ul2) x /\
            (forall k, field_at (teardown_heap h ix Ul1) x k =
                       field_at (teardown_heap h ix Ul2) x k) /\
            teardown_index ix Ul1 x = teardown_index ix Ul2 x /\
            (forall o, teardown_kids kd Ul1 o = teardown_kids kd Ul2 o).
Proof.
  intros h ix kd Ul1 Ul2 Hix Heq x. pose proof (inU_ext Ul1 Ul2 Heq) as E.
  split; [| split; [| split; [| split]]].
  - destruct (inU Ul1 x) eqn:H1.
    + assert (H2 : inU Ul2 x = true) by (rewrite <- E; exact H1).
      rewrite (teardown_in h ix Ul1 x H1), (teardown_in h ix Ul2 x H2). reflexivity.
    + assert (H2 : inU Ul2 x = false) by (rewrite <- E; exact H1).
      destruct (s_node (h x)) as [nd|] eqn:Hs.
      * rewrite (teardown_out_node h ix Ul1 x nd H1 Hs), (teardown_out_node h ix Ul2 x nd H2 Hs).
        reflexivity.
      * rewrite (teardown_out_free h ix Ul1 x H1 Hs), (teardown_out_free h ix Ul2 x H2 Hs).
        reflexivity.
  - unfold owner_at. destruct (inU Ul1 x) eqn:H1.
    + assert (H2 : inU Ul2 x = true) by (rewrite <- E; exact H1).
      rewrite (teardown_in h ix Ul1 x H1), (teardown_in h ix Ul2 x H2). reflexivity.
    + assert (H2 : inU Ul2 x = false) by (rewrite <- E; exact H1).
      destruct (s_node (h x)) as [nd|] eqn:Hs.
      * rewrite (teardown_out_node h ix Ul1 x nd H1 Hs), (teardown_out_node h ix Ul2 x nd H2 Hs).
        reflexivity.
      * rewrite (teardown_out_free h ix Ul1 x H1 Hs), (teardown_out_free h ix Ul2 x H2 Hs).
        reflexivity.
  - intros k. rewrite (release_field_effect h ix Ul1 x k Hix), (release_field_effect h ix Ul2 x k Hix).
    rewrite E. destruct (inU Ul2 x); [reflexivity |].
    destruct (field_at h x k) as [[t g]|]; [rewrite E; reflexivity | reflexivity].
  - unfold teardown_index. rewrite E. destruct (inU Ul2 x); [reflexivity |].
    apply filter_ext_local. intros a. rewrite E. reflexivity.
  - intros o. unfold teardown_kids.
    apply filter_ext_local. intros a. rewrite E. reflexivity.
Qed.

(** ** Handles: reuse without resurrection *)

Definition subject (op : Op) : option Link :=
  match op with
  | OpSetField x g _ _ => Some (x, g)
  | OpAttach x g _ _ => Some (x, g)
  | OpRelease n g _ => Some (n, g)
  | _ => None
  end.

Definition parent_handle (op : Op) : option Link :=
  match op with
  | OpAlloc _ (OParent p) g => Some (p, g)
  | OpAttach _ _ (OParent p) gp => Some (p, gp)
  | _ => None
  end.

(** Every operation naming an existing root shares the same epoch owner.
    RootNew is a declaration, not an operation through an old handle. *)
Definition root_handle (op : Op) : option Link :=
  match op with
  | OpAlloc _ (ORoot r) g => Some (r, g)
  | OpAttach _ _ (ORoot r) g => Some (r, g)
  | OpRootDrop r g _ => Some (r, g)
  | _ => None
  end.

Theorem step_root_handles_resolve : forall s op s' l, Step s op s' ->
  root_handle op = Some l -> root_resolves (roots s) (root_gen s) l.
Proof.
  intros s op s' l HS Hl.
  destruct HS as [s i o g Hi Ho | s x g nd k ol Hr Hx Hlk | s x g nd o gp Hr Hx Hro Ha
                 | s n g Ul Hr Hro HU Hnd | s r Hr | s r g Ul Hr HU Hnd]; simpl in Hl;
    try discriminate.
  - destruct o as [r | p]; simpl in Hl; [| discriminate].
    injection Hl as E. subst l. exact Ho.
  - destruct o as [r | p]; simpl in Hl; [| discriminate].
    injection Hl as E. subst l. exact Ha.
  - injection Hl as E. subst l. exact Hr.
Qed.

(** Every step acts only through handles that resolve. *)
Theorem step_handles_resolve : forall s op s' l, Step s op s' ->
  (subject op = Some l \/ parent_handle op = Some l) -> resolves (heap s) l.
Proof.
  intros s op s' l HS Hl.
  destruct HS as [s i o g Hi Ho | s x g nd k ol Hr Hx Hlk | s x g nd o gp Hr Hx Hro Ha
                 | s n g Ul Hr Hro HU Hnd | s r Hr | s r g Ul Hr HU Hnd]; simpl in Hl.
  - destruct Hl as [Hl | Hl]; [discriminate |].
    destruct o as [r | p]; simpl in Hl; [discriminate |].
    injection Hl as E. subst l. exact Ho.
  - destruct Hl as [Hl | Hl]; [| discriminate]. injection Hl as E. subst l. exact Hr.
  - destruct Hl as [Hl | Hl].
    + injection Hl as E. subst l. exact Hr.
    + destruct o as [r | p]; simpl in Hl; [discriminate |].
      injection Hl as E. subst l. exact (proj1 Ha).
  - destruct Hl as [Hl | Hl]; [| discriminate]. injection Hl as E. subst l. exact Hr.
  - destruct Hl as [Hl | Hl]; discriminate.
  - destruct Hl as [Hl | Hl]; discriminate.
Qed.

Lemma gen_mono_step : forall s op s', Step s op s' ->
  forall i, s_gen (heap s i) <= s_gen (heap s' i).
Proof.
  intros s op s' H i.
  destruct H as [s j o g Hj Ho | s x g nd k ol Hr Hx Hl | s x g nd o gp Hr Hx Hro Ha
                | s n g Ul Hr Hro HU Hnd | s r Hr | s r g Ul Hr HU Hnd].
  - cbn [heap]. destruct (Nat.eq_dec i j) as [-> | Hne];
      [rewrite upd_eq; simpl; lia | rewrite upd_neq by exact Hne; lia].
  - cbn [heap]. destruct (Nat.eq_dec i x) as [-> | Hne];
      [rewrite upd_eq; simpl; lia | rewrite upd_neq by exact Hne; lia].
  - cbn [heap]. destruct (Nat.eq_dec i x) as [-> | Hne];
      [rewrite upd_eq; simpl; lia | rewrite upd_neq by exact Hne; lia].
  - rewrite heap_teardown. destruct (inU Ul i) eqn:E.
    + rewrite (teardown_in _ _ _ _ E). simpl. lia.
    + destruct (s_node (heap s i)) as [nd|] eqn:Hs.
      * rewrite (teardown_out_node _ _ _ _ nd E Hs). simpl. lia.
      * rewrite (teardown_out_free _ _ _ _ E Hs). lia.
  - cbn [heap]. lia.
  - rewrite heap_root_drop. destruct (inU Ul i) eqn:E.
    + rewrite (teardown_in _ _ _ _ E). simpl. lia.
    + destruct (s_node (heap s i)) as [nd|] eqn:Hs.
      * rewrite (teardown_out_node _ _ _ _ nd E Hs). simpl. lia.
      * rewrite (teardown_out_free _ _ _ _ E Hs). lia.
Qed.

Lemma gen_mono_steps : forall s s', Steps s s' -> forall i, s_gen (heap s i) <= s_gen (heap s' i).
Proof.
  intros s s' H. induction H as [s | s1 op s2 s3 H12 H23 IH]; intros i; [lia |].
  pose proof (gen_mono_step s1 op s2 H12 i). specialize (IH i). lia.
Qed.

Lemma root_gen_mono_step : forall s op s', Step s op s' ->
  forall r, root_gen s r <= root_gen s' r.
Proof.
  intros s op s' HS q. destruct HS;
    cbn [root_gen teardown root_drop]; try lia.
  destruct (Nat.eqb q r); lia.
Qed.

Lemma root_gen_mono_steps : forall s s', Steps s s' ->
  forall r, root_gen s r <= root_gen s' r.
Proof.
  intros s s' HS. induction HS as [s | s1 op s2 s3 H12 H23 IH]; intros r; [lia |].
  pose proof (root_gen_mono_step _ _ _ H12 r). specialize (IH r). lia.
Qed.

Theorem root_new_preserves_epoch : forall s r s', Step s (OpRootNew r) s' ->
  forall q, root_gen s' q = root_gen s q.
Proof. intros s r s' HS q. inversion HS; reflexivity. Qed.

(** A link or handle into a torn-down unit never resolves again, whatever
    is later allocated into the same slot. *)
Theorem temp_link_dead_forever : forall h ix Ul t g s1 s2,
  resolves h (t, g) -> In t Ul -> heap s1 = teardown_heap h ix Ul -> Steps s1 s2 ->
  ~ resolves (heap s2) (t, g).
Proof.
  intros h ix Ul t g s1 s2 [Hg _] Ht E1 Hs [Hg2 _]. simpl in Hg, Hg2.
  assert (E : s_gen (heap s1 t) = S g).
  { rewrite E1, (teardown_in _ _ _ _ (proj2 (inU_spec Ul t) Ht)). simpl.
    rewrite Hg. reflexivity. }
  pose proof (gen_mono_steps _ _ Hs t) as M. rewrite E in M. lia.
Qed.

Lemma release_step_inv : forall s n g Ul s1, Step s (OpRelease n g Ul) s1 ->
  resolves (heap s) (n, g) /\ (forall y, In y Ul <-> in_sub (heap s) n y) /\ s1 = teardown s Ul.
Proof. intros s n g Ul s1 H. inversion H; subst. split; [assumption | split; [assumption | reflexivity]]. Qed.

Theorem release_unit_unique : forall s n g Ul s1,
  Step s (OpRelease n g Ul) s1 -> NoDup Ul.
Proof. intros s n g Ul s1 H. inversion H; assumption. Qed.

Lemma root_drop_step_inv : forall s r g Ul s1, Step s (OpRootDrop r g Ul) s1 ->
  (forall y, In y Ul <-> under_root (heap s) r y) /\ s1 = root_drop s r Ul.
Proof. intros s r g Ul s1 H. inversion H; subst. split; [assumption | reflexivity]. Qed.

Theorem root_drop_unit_unique : forall s r g Ul s1,
  Step s (OpRootDrop r g Ul) s1 -> NoDup Ul.
Proof. intros s r g Ul s1 H. inversion H; assumption. Qed.

Theorem dropped_root_identity_dead_forever : forall s r g Ul s1 s2,
  Step s (OpRootDrop r g Ul) s1 -> Steps s1 s2 ->
  ~ root_resolves (roots s2) (root_gen s2) (r, g).
Proof.
  intros s r g Ul s1 s2 HD H12 [_ Hg2].
  pose proof (step_root_handles_resolve s _ s1 (r, g) HD eq_refl) as [_ Hg].
  destruct (root_drop_step_inv _ _ _ _ _ HD) as [_ E]. subst s1.
  pose proof (root_gen_mono_steps _ _ H12 r) as M.
  rewrite root_drop_epoch in M. simpl in Hg, Hg2. rewrite Hg in M. lia.
Qed.

Theorem dropped_root_identity_never_acts : forall s r g Ul s1 s2 op s3,
  Step s (OpRootDrop r g Ul) s1 -> Steps s1 s2 -> Step s2 op s3 ->
  root_handle op <> Some (r, g).
Proof.
  intros s r g Ul s1 s2 op s3 HD H12 H23 E.
  apply (dropped_root_identity_dead_forever _ _ _ _ _ _ HD H12).
  exact (step_root_handles_resolve _ _ _ _ H23 E).
Qed.

Corollary dropped_root_check_false_forever : forall s r g Ul s1 s2,
  Step s (OpRootDrop r g Ul) s1 -> Steps s1 s2 -> check_root s2 (r, g) = false.
Proof.
  intros s r g Ul s1 s2 HD H12. apply check_root_false.
  exact (dropped_root_identity_dead_forever _ _ _ _ _ _ HD H12).
Qed.

Theorem released_local_read_none_forever : forall s n g Ul s1 s2,
  Step s (OpRelease n g Ul) s1 -> Steps s1 s2 -> resolve_node (heap s2) (n, g) = None.
Proof.
  intros s n g Ul s1 s2 HR H12. apply resolve_node_none.
  destruct (release_step_inv _ _ _ _ _ HR) as [Hr [HU E]]. subst s1.
  apply (temp_link_dead_forever (heap s) (rix s) Ul n g (teardown s Ul) s2 Hr);
    [apply HU; constructor | apply heap_teardown | exact H12].
Qed.

(** The checked read refuses every saved local link into the released unit,
    not only the released node: descendants are retired with it. *)
Theorem released_unit_local_read_none_forever : forall s n g Ul s1 s2 t gt,
  Step s (OpRelease n g Ul) s1 -> resolves (heap s) (t, gt) -> In t Ul -> Steps s1 s2 ->
  resolve_node (heap s2) (t, gt) = None.
Proof.
  intros s n g Ul s1 s2 t gt HR Ht Hin H12. apply resolve_node_none.
  destruct (release_step_inv _ _ _ _ _ HR) as [_ [_ E]]. subst s1.
  exact (temp_link_dead_forever (heap s) (rix s) Ul t gt (teardown s Ul) s2 Ht Hin
           (heap_teardown s Ul) H12).
Qed.

(** The same for a root going out of scope: every saved local link into
    anything the root owned reads None forever. *)
Theorem dropped_root_unit_local_read_none_forever : forall s r g Ul s1 s2 t gt,
  Step s (OpRootDrop r g Ul) s1 -> resolves (heap s) (t, gt) -> In t Ul -> Steps s1 s2 ->
  resolve_node (heap s2) (t, gt) = None.
Proof.
  intros s r g Ul s1 s2 t gt HR Ht Hin H12. apply resolve_node_none.
  destruct (root_drop_step_inv _ _ _ _ _ HR) as [_ E]. subst s1.
  exact (temp_link_dead_forever (heap s) (rix s) Ul t gt (root_drop s r Ul) s2 Ht Hin
           (heap_root_drop s r Ul) H12).
Qed.

(** After a release, no later step acts through the released handle: no
    second release, no write, no attach under it. *)
Theorem released_handle_never_acts : forall s n g Ul s1 s2 op s3,
  Step s (OpRelease n g Ul) s1 -> Steps s1 s2 -> Step s2 op s3 ->
  subject op <> Some (n, g) /\ parent_handle op <> Some (n, g).
Proof.
  intros s n g Ul s1 s2 op s3 HR H12 H23.
  destruct (release_step_inv _ _ _ _ _ HR) as [Hr [HU E]]. subst s1.
  assert (Hdead : ~ resolves (heap s2) (n, g)).
  { apply (temp_link_dead_forever (heap s) (rix s) Ul n g (teardown s Ul) s2 Hr);
      [apply HU; constructor | apply heap_teardown | exact H12]. }
  split; intro Eop; apply Hdead; apply (step_handles_resolve s2 op s3 (n, g) H23); tauto.
Qed.

(** After a root is dropped, no later step acts through a handle to
    anything it owned. *)
Theorem dropped_root_handles_never_act : forall s r rg Ul s1 s2 op s3 t g,
  Step s (OpRootDrop r rg Ul) s1 -> resolves (heap s) (t, g) -> under_root (heap s) r t ->
  Steps s1 s2 -> Step s2 op s3 ->
  subject op <> Some (t, g) /\ parent_handle op <> Some (t, g).
Proof.
  intros s r rg Ul s1 s2 op s3 t g HR Ht Hu H12 H23.
  destruct (root_drop_step_inv _ _ _ _ _ HR) as [HU E]. subst s1.
  assert (Hdead : ~ resolves (heap s2) (t, g)).
  { apply (temp_link_dead_forever (heap s) (rix s) Ul t g (root_drop s r Ul) s2 Ht);
      [apply HU, Hu | apply heap_root_drop | exact H12]. }
  split; intro Eop; apply Hdead; apply (step_handles_resolve s2 op s3 (t, g) H23); tauto.
Qed.

Theorem released_slot_reusable : forall s Ul t r, In t Ul -> In r (roots s) ->
  exists s2, Step (teardown s Ul) (OpAlloc t (ORoot r) (root_gen s r)) s2 /\
             owner_at (heap s2) t = Some (ORoot r) /\
             s_gen (heap s2 t) = S (s_gen (heap s t)).
Proof.
  intros s Ul t r Ht Hr.
  assert (E : heap (teardown s Ul) t = mkSlot (S (s_gen (heap s t))) None).
  { rewrite heap_teardown. apply teardown_in, inU_spec, Ht. }
  eexists. split.
  - apply StAlloc; [rewrite E; reflexivity | split; [exact Hr | reflexivity]].
  - cbn [heap]. unfold owner_at. rewrite upd_eq. split; [reflexivity |].
    rewrite E. reflexivity.
Qed.

Theorem relink_into_released_refused : forall h ix Ul t, In t Ul ->
  forall g, ~ resolves (teardown_heap h ix Ul) (t, g).
Proof.
  intros h ix Ul t Ht g [_ [nd Hnd]]. simpl in Hnd.
  rewrite (teardown_in h ix Ul t (proj2 (inU_spec Ul t) Ht)) in Hnd. discriminate.
Qed.

(** * Witnesses and counterexamples *)

Definition f0 (l : Link) : nat -> option Link := fun k => if Nat.eqb k 0 then Some l else None.
Definition ix0 : Index := fun _ => [].
Definition kids0 : Kids := fun _ => [].

(** A = 0 and B = 1 are owned by roots 0 and 1; B's field 0 links to A. *)
Definition h_ab : Heap := fun x =>
  match x with
  | 0 => mkSlot 0 (Some (mkNode (ORoot 0) empty_fields))
  | 1 => mkSlot 0 (Some (mkNode (ORoot 1) (f0 (0, 0))))
  | _ => mkSlot 0 None
  end.
Definition ix_ab : Index := fun t => match t with 0 => [(1, 0)] | _ => [] end.
Definition kids_ab : Kids :=
  fun o => match o with ORoot 0 => [0] | ORoot 1 => [1] | _ => [] end.
Definition st_ab : St := mkSt h_ab ix_ab kids_ab [0; 1] 2 initial_root_generations.

Lemma inv_st_ab : Inv st_ab.
Proof.
  unfold Inv, InvH, st_ab; cbn [heap rix kids roots bound].
  split; [| split; [| split; [| split; [| split]]]].
  - intros x [nd Hx]. destruct x as [| [| x]]; simpl in Hx.
    + injection Hx as E. subst nd. eapply RRoot; [reflexivity | reflexivity].
    + injection Hx as E. subst nd. eapply RRoot; [reflexivity | reflexivity].
    + discriminate.
  - intros x nd r Hx Ho. destruct x as [| [| x]]; simpl in Hx.
    + injection Hx as E. subst nd. simpl in Ho. injection Ho as E. subst r. left. reflexivity.
    + injection Hx as E. subst nd. simpl in Ho. injection Ho as E. subst r. right. left. reflexivity.
    + discriminate.
  - intros src k l H. destruct src as [| [| src]]; unfold field_at in H; simpl in H.
    + discriminate.
    + unfold f0 in H. destruct (Nat.eqb k 0); [| discriminate].
      injection H as E. subst l. split; [reflexivity | eexists; reflexivity].
    + discriminate.
  - intros t src k. split.
    + destruct t as [| t]; simpl; [| intros []].
      intros [E | []]. injection E as E1 E2. subst. exists 0. reflexivity.
    + intros [g H]. destruct src as [| [| src]]; unfold field_at in H; simpl in H.
      * discriminate.
      * unfold f0 in H. destruct (Nat.eqb k 0) eqn:Ek; [| discriminate].
        apply Nat.eqb_eq in Ek. subst k. injection H as E1 E2. subst t g.
        simpl. left. reflexivity.
      * discriminate.
  - intros o c. split.
    + destruct o as [[| [| r]] | p]; simpl;
        [intros [<- | []] | intros [<- | []] | intros [] | intros []];
        eexists; split; reflexivity.
    + intros [nd [Hc Ho]]. destruct c as [| [| c]]; simpl in Hc;
        [injection Hc as E; subst nd; simpl in Ho; subst o; simpl; left; reflexivity
        |injection Hc as E; subst nd; simpl in Ho; subst o; simpl; left; reflexivity
        |discriminate].
  - intros x [nd Hx]. destruct x as [| [| x]]; simpl in Hx; [lia | lia | discriminate].
Qed.

(** The cost of "links never own": releasing A empties B's field. *)
Example release_empties_incoming_field :
  field_at h_ab 1 0 = Some (0, 0) /\ field_at (teardown_heap h_ab ix_ab [0]) 1 0 = None.
Proof. split; reflexivity. Qed.

(** Counterexample: phase 2 without phase 1 leaves B's link dangling. *)
Example no_clear_dangles : ~ LinksResolve (phase2 h_ab [0]).
Proof.
  intro H. assert (E : field_at (phase2 h_ab [0]) 1 0 = Some (0, 0)) by reflexivity.
  destruct (H 1 0 (0, 0) E) as [_ [nd Hnd]]. simpl in Hnd. discriminate.
Qed.

(** Counterexample: an index missing B's entry makes phase 1 miss it. *)
Example stale_index_dangles :
  ~ IndexExact h_ab ix0 /\ ~ LinksResolve (teardown_heap h_ab ix0 [0]).
Proof.
  split.
  - intro H. destruct (proj2 (H 0 1 0) (ex_intro _ 0 eq_refl)).
  - intro H. assert (E : field_at (teardown_heap h_ab ix0 [0]) 1 0 = Some (0, 0)) by reflexivity.
    destruct (H 1 0 (0, 0) E) as [_ [nd Hnd]]. simpl in Hnd. discriminate.
Qed.

(** Scope limit: B is owned by root 1 and nothing links to it. It stays
    until root 1 releases it or goes out of scope: a logical leak, not an
    orphan. *)
Example owned_but_unlinked_is_kept :
  ix_ab 1 = [] /\ rooted h_ab 1 /\ live (teardown_heap h_ab ix_ab [0]) 1.
Proof.
  split; [reflexivity |]. split.
  - eapply RRoot; reflexivity.
  - eexists. reflexivity.
Qed.

(** ** Ownership cycles: the ancestor check cannot be replaced by teardown *)

(** A = 0 is owned by root 0, C = 1 is A's child. *)
Definition h_c : Heap := fun x =>
  match x with
  | 0 => mkSlot 0 (Some (mkNode (ORoot 0) empty_fields))
  | 1 => mkSlot 0 (Some (mkNode (OParent 0) empty_fields))
  | _ => mkSlot 0 None
  end.
Definition kids_c : Kids :=
  fun o => match o with ORoot 0 => [0] | OParent 0 => [1] | _ => [] end.

Lemma inv_st_c : Inv (mkSt h_c ix0 kids_c [0] 2 initial_root_generations).
Proof.
  unfold Inv, InvH; cbn [heap rix kids roots bound].
  split; [| split; [| split; [| split; [| split]]]].
  - intros x [nd Hx]. destruct x as [| [| x]]; simpl in Hx.
    + injection Hx as E. subst nd. eapply RRoot; reflexivity.
    + injection Hx as E. subst nd. eapply RPar; [reflexivity | reflexivity |].
      eapply RRoot; reflexivity.
    + discriminate.
  - intros x nd r Hx Ho. destruct x as [| [| x]]; simpl in Hx.
    + injection Hx as E. subst nd. simpl in Ho. injection Ho as E. subst r. left. reflexivity.
    + injection Hx as E. subst nd. simpl in Ho. discriminate.
    + discriminate.
  - intros src k l H. destruct src as [| [| src]]; unfold field_at in H; simpl in H; discriminate.
  - intros t src k. split; [intros [] |].
    intros [g H]. destruct src as [| [| src]]; unfold field_at in H; simpl in H; discriminate.
  - intros o c. split.
    + destruct o as [[| r] | [| p]]; simpl;
        [intros [<- | []] | intros [] | intros [<- | []] | intros []];
        eexists; split; reflexivity.
    + intros [nd [Hc Ho]]. destruct c as [| [| c]]; simpl in Hc;
        [injection Hc as E; subst nd; simpl in Ho; subst o; simpl; left; reflexivity
        |injection Hc as E; subst nd; simpl in Ho; subst o; simpl; left; reflexivity
        |discriminate].
  - intros x [nd Hx]. destruct x as [| [| x]]; simpl in Hx; [lia | lia | discriminate].
Qed.

Lemma c_in_sub_a : in_sub h_c 0 1.
Proof. eapply SubDown; [reflexivity | reflexivity | constructor]. Qed.

Example checked_attach_refuses_cycle : forall rs rg g, ~ attach_ok h_c rs rg 0 (OParent 1) g.
Proof. intros rs rg g [_ H]. exact (H c_in_sub_a). Qed.

(** The same reparent with the check dropped. *)
Definition h_cyc : Heap := upd h_c 0 (mkSlot 0 (Some (mkNode (OParent 1) empty_fields))).

Lemma h_cyc_is_unchecked_attach :
  h_cyc = upd h_c 0 (mkSlot (s_gen (h_c 0))
                     (Some (mkNode (OParent 1) (fields (mkNode (ORoot 0) empty_fields))))).
Proof. reflexivity. Qed.

Definition CycleAB (h : Heap) (a b : nat) : Prop :=
  exists nda ndb, s_node (h a) = Some nda /\ owner nda = OParent b /\
                  s_node (h b) = Some ndb /\ owner ndb = OParent a.

Lemma cycle_not_rooted : forall h a b, CycleAB h a b -> forall y, rooted h y -> y <> a /\ y <> b.
Proof.
  intros h a b [nda [ndb [Ha [Hoa [Hb Hob]]]]] y Hr.
  induction Hr as [y nd r Hy Ho | y nd p Hy Ho Hp IH].
  - split; intro E; subst y.
    + rewrite Ha in Hy. injection Hy as E. subst nd. congruence.
    + rewrite Hb in Hy. injection Hy as E. subst nd. congruence.
  - split; intro E; subst y.
    + rewrite Ha in Hy. injection Hy as E. subst nd. rewrite Hoa in Ho.
      injection Ho as E. subst p. destruct IH as [_ F]. apply F. reflexivity.
    + rewrite Hb in Hy. injection Hy as E. subst nd. rewrite Hob in Ho.
      injection Ho as E. subst p. destruct IH as [F _]. apply F. reflexivity.
Qed.

Lemma setfield_keeps_owner : forall h x nd k ol y ndy,
  s_node (h x) = Some nd -> s_node (h y) = Some ndy ->
  exists ndy', s_node (upd h x (mkSlot (s_gen (h x)) (Some (set_field nd k ol))) y) = Some ndy' /\
               owner ndy' = owner ndy.
Proof.
  intros h x nd k ol y ndy Hx Hy. destruct (Nat.eq_dec y x) as [-> | Hne].
  - rewrite upd_eq. exists (set_field nd k ol). split; [reflexivity |].
    rewrite Hx in Hy. injection Hy as E. subst ndy. reflexivity.
  - rewrite upd_neq by exact Hne. exists ndy. split; [exact Hy | reflexivity].
Qed.

Lemma cycle_step : forall s op s' a b, CycleAB (heap s) a b -> Step s op s' -> CycleAB (heap s') a b.
Proof.
  intros s op s' a b Hc HS. pose proof (cycle_not_rooted _ _ _ Hc) as Hnr.
  destruct Hc as [nda [ndb [Ha [Hoa [Hb Hob]]]]].
  destruct HS as [s i o g Hi Ho | s x g nd k ol Hr Hx Hl | s x g nd o gp Hr Hx Hro Hat
                 | s n g Ul Hr Hro HU Hnd | s r Hr | s r g Ul Hr HU Hnd].
  - cbn [heap]. assert (Hai : a <> i) by (intro E; subst; congruence).
    assert (Hbi : b <> i) by (intro E; subst; congruence).
    exists nda, ndb. rewrite (upd_neq _ _ _ a Hai), (upd_neq _ _ _ b Hbi). auto.
  - cbn [heap].
    destruct (setfield_keeps_owner (heap s) x nd k ol a nda Hx Ha) as [nda' [Ha' Hoa']].
    destruct (setfield_keeps_owner (heap s) x nd k ol b ndb Hx Hb) as [ndb' [Hb' Hob']].
    exists nda', ndb'. rewrite Hoa', Hob'. auto.
  - cbn [heap]. destruct (Hnr x Hro) as [Hxa Hxb].
    assert (Hax : a <> x) by (intro E; apply Hxa; symmetry; exact E).
    assert (Hbx : b <> x) by (intro E; apply Hxb; symmetry; exact E).
    exists nda, ndb. rewrite (upd_neq _ _ _ a Hax), (upd_neq _ _ _ b Hbx). auto.
  - rewrite heap_teardown.
    assert (Hua : inU Ul a = false).
    { apply inU_false. intro Hin. apply HU in Hin.
      destruct (Hnr a (sub_of_rooted _ _ _ Hro Hin)) as [F _]. apply F. reflexivity. }
    assert (Hub : inU Ul b = false).
    { apply inU_false. intro Hin. apply HU in Hin.
      destruct (Hnr b (sub_of_rooted _ _ _ Hro Hin)) as [_ F]. apply F. reflexivity. }
    exists (clear_node Ul (rix s) a nda), (clear_node Ul (rix s) b ndb).
    rewrite (teardown_out_node _ _ _ a nda Hua Ha), (teardown_out_node _ _ _ b ndb Hub Hb).
    simpl. auto.
  - cbn [heap]. exists nda, ndb. auto.
  - rewrite heap_root_drop.
    assert (Hua : inU Ul a = false).
    { apply inU_false. intro Hin. apply HU in Hin.
      destruct (Hnr a (under_root_rooted _ _ _ Hin)) as [F _]. apply F. reflexivity. }
    assert (Hub : inU Ul b = false).
    { apply inU_false. intro Hin. apply HU in Hin.
      destruct (Hnr b (under_root_rooted _ _ _ Hin)) as [_ F]. apply F. reflexivity. }
    exists (clear_node Ul (rix s) a nda), (clear_node Ul (rix s) b ndb).
    rewrite (teardown_out_node _ _ _ a nda Hua Ha), (teardown_out_node _ _ _ b ndb Hub Hb).
    simpl. auto.
Qed.

Lemma cycle_steps : forall s s' a b, CycleAB (heap s) a b -> Steps s s' -> CycleAB (heap s') a b.
Proof.
  intros s s' a b Hc HS. induction HS as [s | s1 op s2 s3 H12 H23 IH]; [exact Hc |].
  apply IH. exact (cycle_step s1 op s2 a b Hc H12).
Qed.

Lemma cycle_h_cyc : CycleAB h_cyc 0 1.
Proof. exists (mkNode (OParent 1) empty_fields), (mkNode (OParent 0) empty_fields). auto. Qed.

Example unchecked_attach_orphans :
  live h_cyc 0 /\ live h_cyc 1 /\ ~ rooted h_cyc 0 /\ ~ rooted h_cyc 1.
Proof.
  split; [eexists; reflexivity |]. split; [eexists; reflexivity |].
  split; intro H; destruct (cycle_not_rooted _ _ _ cycle_h_cyc _ H) as [F1 F2];
    [apply F1 | apply F2]; reflexivity.
Qed.

(** No later step frees, moves or roots the cycle, including dropping its
    former root: every step that could needs a rooted subject. *)
Theorem ownership_cycle_permanent : forall s',
  Steps (mkSt h_cyc ix0 kids0 [0] 2 initial_root_generations) s' ->
  live (heap s') 0 /\ live (heap s') 1 /\ ~ rooted (heap s') 0 /\ ~ rooted (heap s') 1.
Proof.
  intros s' HS. pose proof (cycle_steps (mkSt h_cyc ix0 kids0 [0] 2 initial_root_generations) s' 0 1 cycle_h_cyc HS) as Hc.
  pose proof (cycle_not_rooted _ _ _ Hc) as Hnr.
  destruct Hc as [nda [ndb [Ha [_ [Hb _]]]]].
  split; [exists nda; exact Ha |]. split; [exists ndb; exact Hb |].
  split; intro H; destruct (Hnr _ H) as [F1 F2]; [apply F1 | apply F2]; reflexivity.
Qed.

(** ** A reference-counting machine on the same heap

    A node lives while a root or a stored link points at it; [RcFree] frees a
    node whose count is zero. The program reaches nodes only from its roots. *)

Record RcSt : Type := mkRc { rc_heap : Heap; rc_rix : Index; rc_roots : list nat }.

Inductive rc_reach (h : Heap) (rs : list nat) : nat -> Prop :=
| RcRoot : forall x, In x rs -> rc_reach h rs x
| RcLink : forall src k x g,
    rc_reach h rs src -> field_at h src k = Some (x, g) -> rc_reach h rs x.

Inductive RcStep : RcSt -> RcSt -> Prop :=
| RcDropRoot : forall s x,
    RcStep s (mkRc (rc_heap s) (rc_rix s) (remove Nat.eq_dec x (rc_roots s)))
| RcAlloc : forall s i,
    s_node (rc_heap s i) = None ->
    RcStep s (mkRc (upd (rc_heap s) i (mkSlot (s_gen (rc_heap s i))
                                         (Some (mkNode (ORoot i) empty_fields))))
                   (rc_rix s) (i :: rc_roots s))
| RcSetField : forall s x nd k ol,
    rc_reach (rc_heap s) (rc_roots s) x ->
    s_node (rc_heap s x) = Some nd ->
    (forall t g, ol = Some (t, g) -> rc_reach (rc_heap s) (rc_roots s) t) ->
    RcStep s (mkRc (upd (rc_heap s) x (mkSlot (s_gen (rc_heap s x)) (Some (set_field nd k ol))))
                   (index_set (rc_heap s) (rc_rix s) x k ol) (rc_roots s))
| RcFree : forall s x,
    rc_rix s x = [] -> ~ In x (rc_roots s) ->
    RcStep s (mkRc (phase2 (rc_heap s) [x]) (teardown_index (rc_rix s) [x]) (rc_roots s)).

Inductive RcSteps : RcSt -> RcSt -> Prop :=
| RcRefl : forall s, RcSteps s s
| RcCons : forall s1 s2 s3, RcStep s1 s2 -> RcSteps s2 s3 -> RcSteps s1 s3.

(** A and B link to each other; the program holds A as root 0. *)
Definition h_rc : Heap := fun x =>
  match x with
  | 0 => mkSlot 0 (Some (mkNode (ORoot 0) (f0 (1, 0))))
  | 1 => mkSlot 0 (Some (mkNode (ORoot 1) (f0 (0, 0))))
  | _ => mkSlot 0 None
  end.
Definition ix_rc : Index := fun t => match t with 0 => [(1, 0)] | 1 => [(0, 0)] | _ => [] end.

Lemma rc_index_exact : IndexExact h_rc ix_rc.
Proof.
  intros t src k. split.
  - destruct t as [| [| t]]; simpl; [| | intros []];
      intros [E | []]; injection E as E1 E2; subst; exists 0; reflexivity.
  - intros [g H]. destruct src as [| [| src]]; unfold field_at in H; simpl in H.
    + unfold f0 in H. destruct (Nat.eqb k 0) eqn:Ek; [| discriminate].
      apply Nat.eqb_eq in Ek. subst k. injection H as E1 E2. subst t g. simpl. left. reflexivity.
    + unfold f0 in H. destruct (Nat.eqb k 0) eqn:Ek; [| discriminate].
      apply Nat.eqb_eq in Ek. subst k. injection H as E1 E2. subst t g. simpl. left. reflexivity.
    + discriminate.
Qed.

Lemma rc_reach_roots_mono : forall h rs rs' y, (forall z, In z rs -> In z rs') ->
  rc_reach h rs y -> rc_reach h rs' y.
Proof.
  intros h rs rs' y Hin H. induction H as [x Hx | src k x g Hs IH Hf];
    [apply RcRoot, Hin, Hx | eapply RcLink; eauto].
Qed.

Definition RcCycle (s : RcSt) : Prop :=
  (exists nd0, s_node (rc_heap s 0) = Some nd0 /\ fields nd0 0 = Some (1, 0)) /\
  (exists nd1, s_node (rc_heap s 1) = Some nd1 /\ fields nd1 0 = Some (0, 0)) /\
  In (1, 0) (rc_rix s 0) /\ In (0, 0) (rc_rix s 1) /\
  (forall y, rc_reach (rc_heap s) (rc_roots s) y -> y <> 0 /\ y <> 1).

Lemma rc_cycle_step : forall s s', RcCycle s -> RcStep s s' -> RcCycle s'.
Proof.
  intros s s' [[nd0 [H0 F0]] [[nd1 [H1 F1]] [I0 [I1 Hun]]]] HS.
  destruct HS as [s x | s i Hi | s x nd k ol Hx Hnd Hol | s x Hx Hnr];
    unfold RcCycle; cbn [rc_heap rc_rix rc_roots].
  - split; [exists nd0; auto | split; [exists nd1; auto | split; [exact I0 | split; [exact I1 |]]]].
    intros y Hy. cbn [rc_heap rc_roots] in Hy. apply Hun.
    apply (rc_reach_roots_mono (rc_heap s) (remove Nat.eq_dec x (rc_roots s)) (rc_roots s) y);
      [| exact Hy].
    intros z Hz. apply in_remove in Hz. exact (proj1 Hz).
  - assert (Hi0 : 0 <> i) by (intro E; subst; congruence).
    assert (Hi1 : 1 <> i) by (intro E; subst; congruence).
    set (h' := upd (rc_heap s) i (mkSlot (s_gen (rc_heap s i)) (Some (mkNode (ORoot i) empty_fields)))).
    assert (Hreach : forall y, rc_reach h' (i :: rc_roots s) y -> y = i \/ rc_reach (rc_heap s) (rc_roots s) y).
    { intros y Hy. induction Hy as [y Hy | src k y g Hs IH Hf].
      - destruct Hy as [E | Hy]; [left; symmetry; exact E | right; apply RcRoot, Hy].
      - destruct (Nat.eq_dec src i) as [-> | Hne].
        + unfold field_at, h' in Hf. rewrite upd_eq in Hf. simpl in Hf. discriminate.
        + destruct IH as [E | IH]; [contradiction |]. right.
          unfold field_at, h' in Hf. rewrite upd_neq in Hf by exact Hne.
          eapply RcLink; [exact IH | exact Hf]. }
    split; [exists nd0; unfold h'; rewrite upd_neq by exact Hi0; auto |].
    split; [exists nd1; unfold h'; rewrite upd_neq by exact Hi1; auto |].
    split; [exact I0 | split; [exact I1 |]].
    intros y Hy. destruct (Hreach y Hy) as [-> | Hy']; [split; intro E; subst; auto | exact (Hun y Hy')].
  - destruct (Hun x Hx) as [Hx0 Hx1].
    set (h' := upd (rc_heap s) x (mkSlot (s_gen (rc_heap s x)) (Some (set_field nd k ol)))).
    assert (Hreach : forall y, rc_reach h' (rc_roots s) y -> rc_reach (rc_heap s) (rc_roots s) y).
    { intros y Hy. induction Hy as [y Hy | src k' y g Hs IH Hf]; [apply RcRoot, Hy |].
      destruct (Nat.eq_dec src x) as [-> | Hne].
      - unfold field_at, h' in Hf. rewrite upd_eq in Hf. simpl in Hf.
        destruct (Nat.eqb k' k).
        + exact (Hol y g Hf).
        + eapply RcLink; [exact IH |]. unfold field_at. rewrite Hnd. exact Hf.
      - unfold field_at, h' in Hf. rewrite upd_neq in Hf by exact Hne.
        eapply RcLink; [exact IH | exact Hf]. }
    assert (Hkeep : forall e t, fst e <> x -> In e (rc_rix s t) ->
      In e (index_set (rc_heap s) (rc_rix s) x k ol t)).
    { intros [src j] t Hne Hin. unfold index_set.
      assert (HR : In (src, j)
        (match field_at (rc_heap s) x k with
         | Some l => if Nat.eqb t (fst l)
                     then filter (fun e => negb (ent_eqb e (x,k))) (rc_rix s t)
                     else rc_rix s t
         | None => rc_rix s t end)).
      { destruct (field_at (rc_heap s) x k) as [l|]; [destruct (Nat.eqb t (fst l)) |];
          try exact Hin. apply filter_In. split; [exact Hin |].
        destruct (ent_eqb (src, j) (x, k)) eqn:E; [| reflexivity].
        apply ent_eqb_spec in E. injection E as E1 E2. simpl in Hne. contradiction. }
      destruct ol as [l|]; [destruct (Nat.eqb t (fst l)) |]; try exact HR.
      apply in_app_iff. left. exact HR. }
    split; [exists nd0; unfold h'; rewrite upd_neq by (intro E; apply Hx0; symmetry; exact E); auto |].
    split; [exists nd1; unfold h'; rewrite upd_neq by (intro E; apply Hx1; symmetry; exact E); auto |].
    split; [apply Hkeep; [simpl; intro E; apply Hx1; symmetry; exact E | exact I0] |].
    split; [apply Hkeep; [simpl; intro E; apply Hx0; symmetry; exact E | exact I1] |].
    intros y Hy. exact (Hun y (Hreach y Hy)).
  - assert (Hx0 : x <> 0) by (intro E; subst; rewrite Hx in I0; destruct I0).
    assert (Hx1 : x <> 1) by (intro E; subst; rewrite Hx in I1; destruct I1).
    assert (Hu0 : inU [x] 0 = false) by (apply inU_false; intros [E | []]; apply Hx0; exact E).
    assert (Hu1 : inU [x] 1 = false) by (apply inU_false; intros [E | []]; apply Hx1; exact E).
    assert (Hfa : forall src j, field_at (phase2 (rc_heap s) [x]) src j = if inU [x] src then None else field_at (rc_heap s) src j).
    { intros src j. unfold field_at, phase2. destruct (inU [x] src); reflexivity. }
    assert (Hreach : forall y, rc_reach (phase2 (rc_heap s) [x]) (rc_roots s) y -> rc_reach (rc_heap s) (rc_roots s) y).
    { intros y Hy. induction Hy as [y Hy | src k y g Hs IH Hf]; [apply RcRoot, Hy |].
      rewrite Hfa in Hf. destruct (inU [x] src); [discriminate |]. eapply RcLink; eauto. }
    split; [exists nd0; unfold phase2; rewrite Hu0; auto |].
    split; [exists nd1; unfold phase2; rewrite Hu1; auto |].
    split.
    + unfold teardown_index. rewrite Hu0. apply filter_In. split; [exact I0 | cbn [fst]; rewrite Hu1; reflexivity].
    + split.
      * unfold teardown_index. rewrite Hu1. apply filter_In. split; [exact I1 | cbn [fst]; rewrite Hu0; reflexivity].
      * intros y Hy. exact (Hun y (Hreach y Hy)).
Qed.

Lemma rc_cycle_steps : forall s s', RcCycle s -> RcSteps s s' -> RcCycle s'.
Proof.
  intros s s' Hc HS. induction HS as [s | s1 s2 s3 H12 H23 IH]; [exact Hc |].
  apply IH. exact (rc_cycle_step s1 s2 Hc H12).
Qed.

Lemma remove_0_singleton : remove Nat.eq_dec 0 [0] = [].
Proof. simpl. destruct (Nat.eq_dec 0 0) as [_ | F]; [reflexivity | contradiction F; reflexivity]. Qed.

(** Before the drop, the program reaches both through root 0. After it
    drops root 0 the cycle is unreachable, and every later RC step keeps it:
    both counts stay 1 because each link lives in the other node. *)
Theorem rc_cycle_permanent :
  rc_reach h_rc [0] 0 /\ rc_reach h_rc [0] 1 /\
  RcStep (mkRc h_rc ix_rc [0]) (mkRc h_rc ix_rc []) /\
  forall s', RcSteps (mkRc h_rc ix_rc []) s' ->
    live (rc_heap s') 0 /\ live (rc_heap s') 1 /\
    ~ rc_reach (rc_heap s') (rc_roots s') 0 /\ ~ rc_reach (rc_heap s') (rc_roots s') 1.
Proof.
  split; [apply RcRoot; left; reflexivity |].
  split; [apply (RcLink h_rc [0] 0 0 1 0); [apply RcRoot; left; reflexivity | reflexivity] |].
  split; [rewrite <- remove_0_singleton; exact (RcDropRoot (mkRc h_rc ix_rc [0]) 0) |].
  intros s' HS.
  assert (H0 : RcCycle (mkRc h_rc ix_rc [])).
  { split; [eexists; split; reflexivity |]. split; [eexists; split; reflexivity |].
    split; [left; reflexivity |]. split; [left; reflexivity |].
    intros y Hy. exfalso. cbn [rc_heap rc_roots] in Hy.
    induction Hy as [y Hy | src k y g Hs IH Hf]; [destruct Hy | exact IH]. }
  destruct (rc_cycle_steps _ _ H0 HS) as [[nd0 [Hn0 _]] [[nd1 [Hn1 _]] [_ [_ Hun]]]].
  split; [exists nd0; exact Hn0 |]. split; [exists nd1; exact Hn1 |].
  split; intro H; destruct (Hun _ H) as [F0 F1]; [apply F0 | apply F1]; reflexivity.
Qed.

(** ** Finalizers must not relink into the dying unit *)

Definition write_field (h : Heap) (x k : nat) (ol : option Link) : Heap :=
  match s_node (h x) with
  | Some nd => upd h x (mkSlot (s_gen (h x)) (Some (set_field nd k ol)))
  | None => h
  end.

(** A raw write between the phases, while U is still allocated. *)
Definition teardown_fin_during (h : Heap) (ix : Index) (Ul : list nat) (x k : nat) (l : Link) : Heap :=
  phase2 (write_field (phase1 h ix Ul) x k (Some l)) Ul.

Definition h_ab0 : Heap := fun x =>
  match x with
  | 0 => mkSlot 0 (Some (mkNode (ORoot 0) empty_fields))
  | 1 => mkSlot 0 (Some (mkNode (ORoot 1) empty_fields))
  | _ => mkSlot 0 None
  end.

(** Releasing A, a finalizer links B to A between the phases. The write was
    valid when it ran (A still resolved), yet the result dangles. *)
Example finalizer_relink_dangles :
  resolves (phase1 h_ab0 ix0 [0]) (0, 0) /\
  ~ LinksResolve (teardown_fin_during h_ab0 ix0 [0] 1 0 (0, 0)).
Proof.
  split.
  - split; [reflexivity | eexists; reflexivity].
  - intro H.
    assert (E : field_at (teardown_fin_during h_ab0 ix0 [0] 1 0 (0, 0)) 1 0 = Some (0, 0))
      by reflexivity.
    destruct (H 1 0 (0, 0) E) as [_ [nd Hnd]]. simpl in Hnd. discriminate.
Qed.

(** ** The unit must be exact *)

(** Releasing A with a unit that omits its child C orphans C. *)
Example inexact_unit_orphans :
  ~ (forall y, In y [0] <-> in_sub h_c 0 y) /\
  live (teardown_heap h_c ix0 [0]) 1 /\ ~ rooted (teardown_heap h_c ix0 [0]) 1.
Proof.
  split; [| split].
  - intro H. destruct (proj2 (H 1) c_in_sub_a) as [E | []]. discriminate.
  - eexists. reflexivity.
  - intro H. inversion H as [x nd r Hx Ho | x nd p Hx Ho Hp]; subst.
    + assert (E : s_node (teardown_heap h_c ix0 [0] 1) =
                  Some (clear_node [0] ix0 1 (mkNode (OParent 0) empty_fields))) by reflexivity.
      rewrite E in Hx. injection Hx as E'. subst nd. discriminate.
    + assert (E : s_node (teardown_heap h_c ix0 [0] 1) =
                  Some (clear_node [0] ix0 1 (mkNode (OParent 0) empty_fields))) by reflexivity.
      rewrite E in Hx. injection Hx as E'. subst nd. simpl in Ho. injection Ho as E''. subst p.
      destruct (rooted_live _ _ Hp) as [nd' Hnd']. discriminate.
Qed.

(** Reusing A's slot for an unrelated root-7 node then adopts the orphan:
    C silently becomes the stranger's child (an ownership ABA). *)
Definition h_adopt : Heap :=
  upd (teardown_heap h_c ix0 [0]) 0
      (mkSlot (s_gen (teardown_heap h_c ix0 [0] 0)) (Some (mkNode (ORoot 7) empty_fields))).

Example orphan_adopted_by_reused_slot :
  s_node (teardown_heap h_c ix0 [0] 0) = None /\
  rooted h_adopt 1 /\ in_sub h_adopt 0 1 /\ owner_at h_adopt 0 = Some (ORoot 7).
Proof.
  split; [reflexivity |]. split; [| split].
  - eapply RPar; [reflexivity | reflexivity |]. eapply RRoot; reflexivity.
  - eapply SubDown; [reflexivity | reflexivity | constructor].
  - reflexivity.
Qed.

(** The exact unit, which the children index gives, frees C too. *)
Example exact_unit_frees_descendant :
  (forall y, In y [0; 1] <-> in_sub h_c 0 y) /\
  (forall y, in_sub h_c 0 y <-> kreach kids_c 0 y) /\
  ~ live (teardown_heap h_c ix0 [0; 1]) 1.
Proof.
  split; [| split].
  - intros y. split.
    + intros [<- | [<- | []]]; [constructor | exact c_in_sub_a].
    + intros Hs. destruct (in_sub_inv _ _ _ Hs) as [-> | [nd [p [Hy _]]]]; [left; reflexivity |].
      destruct y as [| [| y]]; [left; reflexivity | right; left; reflexivity | discriminate].
  - intros y. apply unit_from_kids. pose proof inv_st_c as [_ [_ [_ [_ [Hk _]]]]]. exact Hk.
  - intros [nd H]. discriminate.
Qed.

(** ** A full run from the empty heap

    Root 0 owns A, A owns B, and A and B link to each other. Dropping root 0
    leaves nothing. A new root reuses A's slot, and the old handle to A is
    refused by every later step. *)

Definition st_empty : St := mkSt (fun _ => mkSlot 0 None) (fun _ => []) (fun _ => []) [] 0 initial_root_generations.
Definition ndA : Node := mkNode (ORoot 0) empty_fields.
Definition ndB : Node := mkNode (OParent 0) empty_fields.

Definition run0 : St := mkSt (heap st_empty) (rix st_empty) (kids st_empty) (0 :: roots st_empty) (bound st_empty) (root_gen st_empty).
Definition run1 : St :=
  mkSt (upd (heap run0) 0 (mkSlot (s_gen (heap run0 0)) (Some (mkNode (ORoot 0) empty_fields))))
       (rix run0) (kids_add (kids run0) (ORoot 0) 0) (roots run0) (Nat.max (bound run0) 1) (root_gen run0).
Definition run2 : St :=
  mkSt (upd (heap run1) 1 (mkSlot (s_gen (heap run1 1)) (Some (mkNode (OParent 0) empty_fields))))
       (rix run1) (kids_add (kids run1) (OParent 0) 1) (roots run1) (Nat.max (bound run1) 2) (root_gen run1).
Definition run3 : St :=
  mkSt (upd (heap run2) 0 (mkSlot (s_gen (heap run2 0)) (Some (set_field ndA 0 (Some (1, 0))))))
       (index_set (heap run2) (rix run2) 0 0 (Some (1, 0))) (kids run2) (roots run2) (bound run2) (root_gen run2).
Definition run4 : St :=
  mkSt (upd (heap run3) 1 (mkSlot (s_gen (heap run3 1)) (Some (set_field ndB 0 (Some (0, 0))))))
       (index_set (heap run3) (rix run3) 1 0 (Some (0, 0))) (kids run3) (roots run3) (bound run3) (root_gen run3).
Definition run5 : St := root_drop run4 0 [0; 1].
Definition run6 : St := mkSt (heap run5) (rix run5) (kids run5) (9 :: roots run5) (bound run5) (root_gen run5).
Definition run7 : St :=
  mkSt (upd (heap run6) 0 (mkSlot (s_gen (heap run6 0)) (Some (mkNode (ORoot 9) empty_fields))))
       (rix run6) (kids_add (kids run6) (ORoot 9) 0) (roots run6) (Nat.max (bound run6) 1) (root_gen run6).

Lemma run4_unit : forall y, In y [0; 1] <-> under_root (heap run4) 0 y.
Proof.
  intros y. split.
  - intros [<- | [<- | []]].
    + eapply URHere; reflexivity.
    + eapply URDown; [reflexivity | reflexivity |]. eapply URHere; reflexivity.
  - intros Hu. destruct (under_root_live _ _ _ Hu) as [nd Hy].
    destruct y as [| [| y]]; [left; reflexivity | right; left; reflexivity |].
    simpl in Hy. discriminate.
Qed.

Theorem full_run_cycle_released :
  Steps st_empty run7 /\ Inv run5 /\ Inv run7 /\
  field_at (heap run4) 0 0 = Some (1, 0) /\ field_at (heap run4) 1 0 = Some (0, 0) /\
  (forall x, ~ live (heap run5) x) /\
  live (heap run7) 0 /\ ~ resolves (heap run7) (0, 0) /\
  (forall op s', Step run7 op s' -> subject op <> Some (0, 0) /\ parent_handle op <> Some (0, 0)).
Proof.
  assert (S0 : Step st_empty (OpRootNew 0) run0) by (apply StRootNew; intros []).
  assert (S1 : Step run0 (OpAlloc 0 (ORoot 0) 0) run1)
    by (apply StAlloc; [reflexivity | split; [left; reflexivity | reflexivity]]).
  assert (S2 : Step run1 (OpAlloc 1 (OParent 0) 0) run2)
    by (apply StAlloc; [reflexivity | split; [reflexivity | eexists; reflexivity]]).
  assert (S3 : Step run2 (OpSetField 0 0 0 (Some (1, 0))) run3).
  { apply StSetField; [split; [reflexivity | eexists; reflexivity] | reflexivity |].
    split; [reflexivity | exists ndB; reflexivity]. }
  assert (S4 : Step run3 (OpSetField 1 0 0 (Some (0, 0))) run4).
  { apply StSetField; [split; [reflexivity | eexists; reflexivity] | reflexivity |].
    split; [reflexivity | eexists; reflexivity]. }
  assert (S5 : Step run4 (OpRootDrop 0 0 [0; 1]) run5).
  { apply StRootDrop; [split; [left; reflexivity | reflexivity] | exact run4_unit |].
    repeat constructor; simpl; intuition discriminate. }
  assert (S6 : Step run5 (OpRootNew 9) run6).
  { apply StRootNew. unfold run5. rewrite roots_root_drop. intro H.
    apply in_remove in H. destruct H as [H _]. destruct H as [E | []]. discriminate. }
  assert (S7 : Step run6 (OpAlloc 0 (ORoot 9) 0) run7)
    by (apply StAlloc; [reflexivity | split; [left; reflexivity | reflexivity]]).
  assert (I5 : Inv run5).
  { apply (inv_steps st_empty); [exact inv_empty |].
    apply (StepsCons _ _ _ _ S0), (StepsCons _ _ _ _ S1), (StepsCons _ _ _ _ S2),
          (StepsCons _ _ _ _ S3), (StepsCons _ _ _ _ S4), (StepsCons _ _ _ _ S5), StepsRefl. }
  assert (I7 : Inv run7) by exact (inv_step _ _ _ (inv_step _ _ _ I5 S6) S7).
  assert (Hdead : ~ resolves (heap run7) (0, 0)) by (intros [Hg _]; simpl in Hg; discriminate).
  split.
  { apply (StepsCons _ _ _ _ S0), (StepsCons _ _ _ _ S1), (StepsCons _ _ _ _ S2),
          (StepsCons _ _ _ _ S3), (StepsCons _ _ _ _ S4), (StepsCons _ _ _ _ S5),
          (StepsCons _ _ _ _ S6), (StepsCons _ _ _ _ S7), StepsRefl. }
  split; [exact I5 |]. split; [exact I7 |].
  split; [reflexivity |]. split; [reflexivity |]. split.
  - intros x [nd H]. unfold run5 in H. rewrite heap_root_drop in H.
    destruct x as [| [| x]]; simpl in H; discriminate.
  - split; [eexists; reflexivity |]. split; [exact Hdead |].
    intros op s' HS. split; intro E; apply Hdead;
      apply (step_handles_resolve run7 op s' (0, 0) HS); tauto.
Qed.
