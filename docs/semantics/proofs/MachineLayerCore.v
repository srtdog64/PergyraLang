(*
  Pergyra Formal Semantics -- Mechanized Fragment (machine-contact corner)
  Target: docs/semantics/19 "Pergyra Abstract Machine Obligation" -- the layer
          BELOW the slot: the raw span that faces the machine.
  Status: proof-sketch; not beta-closure evidence unless checked by CI
          (rocq compile / coqc + rocqchk).
  Budget: 0 admits / 0 axioms (adds nothing to the corpus axiom budget).

  Scope: Slot (SlotCalculus.v / SlotLifecycleCore.v) is a HIGH abstraction --
  typed, owned, lifecycle-tracked. Under it a systems language needs the point
  that actually touches the machine. In C/Rust/Zig that point is a raw pointer /
  `unsafe` / `[*]u8` -- the object that has FORGOTTEN the four facts a systems
  language must keep: how far it extends (extent), who backs it (provenance),
  how the machine must treat it (access mode: plain / volatile-MMIO / atomic),
  and where it came from. This file models the Pergyra answer: NOT a raw pointer,
  but a `Region` that carries those four facts as evidence and is grounded in a
  declared machine `Grant`. malloc is then just one Grant backend; freestanding
  swaps the root Grant. The keystone is that `place : Region -> Slot` preserves
  the safety chain -- every slot handed up traces to a real declared grant.

  Mechanized obligations:
    - Grant soundness: the full region of a declared grant is valid
      (`grant_yields_valid_region`).
    - Allocator safety: carving a valid region yields a valid sub-region
      (`carve_preserves_validity`) and non-overlapping carves are disjoint
      (`carve_disjoint`).
    - KEYSTONE -- safety chain: placing a typed slot on a valid region yields a
      slot GROUNDED in a real grant: provenance, mode, and bounds preserved
      (`place_grounds_slot`); end-to-end grant->carve->place stays grounded
      (`chain_grant_carve_place_grounded`); no slot escapes the declared machine
      (`no_wild_slot`).
    - Access-mode discipline / fail-closed: a data slot may NOT sit on a
      volatile (MMIO) or atomic region (`place_rejects_volatile`,
      `place_rejects_atomic`); placement fails closed on overflow or misalignment
      (`place_oversize_fail_closed`, `place_misaligned_fail_closed`).
    - No aliasing up the chain: slots placed on disjoint regions are disjoint
      (`placed_slots_disjoint`).
    - Capability gate (the metal is an effect): without the metal capability no
      region operation produces a slot (`cap_gate_fail_closed`), and the gate
      does not weaken grounding (`guarded_place_grounds_slot`).

  Decide-vs-declare at the metal (docs/19 Rice corner): a `Grant` is DATA here,
  not a Coq Axiom. The *language* treats the first page / MMIO window / linker
  section as DECLARED (you cannot analyze the memory map into existence -- you
  declare it, the deed), and everything above is CHECKED against that declaration
  and fails closed otherwise. The proof takes the grant table as a parameter and
  discharges the chain over it, adding no axiom to the corpus budget.

  Lineage (docs/19 lineage map): region-based memory management
  (Tofte-Talpin region calculus) for the scoped extent, and pointer-provenance
  discipline for the grant-rooted `r_prov` tag.

  Negative scope: this models the STATIC grounding chain (extent + mode +
  provenance) and its fail-closed gates. It does NOT model full pointer-
  provenance soundness under arbitrary aliasing/type-punning (the Stacked/Tree
  Borrows-grade obligation), concurrent atomics' memory ordering, nor the
  binding of `Grant` onto a live boot memory map. Those are the named open
  research corners; this file is the fail-closed skeleton they must refine.
*)

Require Import Coq.Init.Nat.
Require Import Coq.Arith.PeanoNat.
Require Import Coq.Bool.Bool.
Require Import Coq.Lists.List.
Require Import Coq.micromega.Lia.
Import ListNotations.

Section MachineContactCore.

(* ================================================================= *)
(* 1. Domains                                                        *)
(* ================================================================= *)

Definition Addr    := nat.   (* flat machine address space (offset model)     *)
Definition GrantId := nat.   (* provenance root: which grant a region descends *)

Inductive AccessMode : Type :=
  | Plain    : AccessMode    (* ordinary RAM: a typed data slot may be placed  *)
  | Volatile : AccessMode    (* MMIO: accesses must not be elided/reordered    *)
  | Atomic   : AccessMode.   (* lock-free cell                                 *)

Definition mode_eqb (a b : AccessMode) : bool :=
  match a, b with
  | Plain, Plain | Volatile, Volatile | Atomic, Atomic => true
  | _, _ => false
  end.

(* ================================================================= *)
(* 2. The machine's declared ground truth (the "deed")               *)
(* ================================================================= *)

Record Grant := mkGrant {
  g_id   : GrantId;
  g_base : Addr;
  g_size : nat;
  g_mode : AccessMode
}.

Definition Machine := list Grant.   (* declared memory map / MMIO windows *)

(* ================================================================= *)
(* 3. The missing primitive: a raw typed span facing the machine     *)
(* ================================================================= *)

Record Region := mkRegion {
  r_base : Addr;
  r_size : nat;
  r_mode : AccessMode;
  r_prov : GrantId            (* the grant this region descends from *)
}.

(* interval [lo, lo+n) is within [b, b+m) *)
Definition range_within (lo n b m : nat) : Prop := b <= lo /\ lo + n <= b + m.

(* intervals [a1,a1+n1) and [a2,a2+n2) do not overlap *)
Definition range_disjoint (a1 n1 a2 n2 : nat) : Prop :=
  a1 + n1 <= a2 \/ a2 + n2 <= a1.

(* A region is VALID w.r.t a machine iff it traces to a real grant: same
   provenance id, same access mode, and its bytes lie within the grant. *)
Definition region_valid (m : Machine) (r : Region) : Prop :=
  exists g, In g m /\
            g_id g = r_prov r /\
            g_mode g = r_mode r /\
            range_within (r_base r) (r_size r) (g_base g) (g_size g).

(* ================================================================= *)
(* 4. grant: obtain the full region of a declared grant              *)
(* ================================================================= *)

Definition region_of_grant (g : Grant) : Region :=
  mkRegion (g_base g) (g_size g) (g_mode g) (g_id g).

Theorem grant_yields_valid_region :
  forall m g, In g m -> region_valid m (region_of_grant g).
Proof.
  intros m g Hin. exists g. unfold region_of_grant, range_within. simpl.
  repeat split; try assumption; try reflexivity; try lia.
Qed.

(* ================================================================= *)
(* 5. carve: split a region for an allocator (a Zone carves Regions) *)
(* ================================================================= *)

Definition carve (r : Region) (off len : nat) : option Region :=
  if Nat.leb (off + len) (r_size r)
  then Some (mkRegion (r_base r + off) len (r_mode r) (r_prov r))
  else None.

Theorem carve_preserves_validity :
  forall m r off len r',
    region_valid m r ->
    carve r off len = Some r' ->
    region_valid m r'.
Proof.
  intros m r off len r' [g [Hin [Hid [Hmode Hwithin]]]] Hc.
  unfold carve in Hc.
  destruct (Nat.leb (off + len) (r_size r)) eqn:Hle; [| discriminate].
  apply Nat.leb_le in Hle.
  inversion Hc; subst; clear Hc.
  destruct Hwithin as [Hw1 Hw2].
  exists g. unfold range_within. simpl.
  repeat split; try assumption; try reflexivity; try lia.
Qed.

Theorem carve_disjoint :
  forall r off1 len1 off2 len2 r1 r2,
    off1 + len1 <= off2 \/ off2 + len2 <= off1 ->
    carve r off1 len1 = Some r1 ->
    carve r off2 len2 = Some r2 ->
    range_disjoint (r_base r1) (r_size r1) (r_base r2) (r_size r2).
Proof.
  intros r off1 len1 off2 len2 r1 r2 Hsep Hc1 Hc2.
  unfold carve in Hc1, Hc2.
  destruct (Nat.leb (off1 + len1) (r_size r)); [| discriminate].
  destruct (Nat.leb (off2 + len2) (r_size r)); [| discriminate].
  inversion Hc1; inversion Hc2; subst; simpl.
  unfold range_disjoint. lia.
Qed.

(* ================================================================= *)
(* 6. Slot: a typed cell placed on a Plain region (the bridge UP)    *)
(* ================================================================= *)

Record Slot := mkSlot {
  sl_base : Addr;
  sl_size : nat;              (* = sizeof(T) *)
  sl_mode : AccessMode;
  sl_prov : GrantId
}.

Definition aligned (base align : nat) : bool :=
  match align with 0 => false | _ => Nat.eqb (base mod align) 0 end.

(* place r tsize talign: reinterpret the region's bytes as a typed data slot.
   Fail-closed unless: the region is Plain (a data slot may NOT sit on MMIO /
   atomic memory), the type fits, and the base is aligned. *)
Definition place (r : Region) (tsize talign : nat) : option Slot :=
  if andb (mode_eqb (r_mode r) Plain)
          (andb (Nat.leb tsize (r_size r)) (aligned (r_base r) talign))
  then Some (mkSlot (r_base r) tsize Plain (r_prov r))
  else None.

(* A slot is GROUNDED in a machine iff it traces to a real grant. This is the
   "no wild slot" property: no Slot exists without a declared machine root. *)
Definition slot_grounded (m : Machine) (s : Slot) : Prop :=
  exists g, In g m /\
            g_id g = sl_prov s /\
            g_mode g = sl_mode s /\
            range_within (sl_base s) (sl_size s) (g_base g) (g_size g).

(* ================================================================= *)
(* 7. KEYSTONE: place preserves the safety chain                     *)
(* ================================================================= *)

Theorem place_grounds_slot :
  forall m r tsize talign s,
    region_valid m r ->
    place r tsize talign = Some s ->
    slot_grounded m s.
Proof.
  intros m r tsize talign s [g [Hin [Hid [Hmode Hwithin]]]] Hp.
  unfold place in Hp.
  destruct (mode_eqb (r_mode r) Plain) eqn:Hpl; simpl in Hp; [| discriminate].
  destruct (Nat.leb tsize (r_size r)) eqn:Hsz; simpl in Hp; [| discriminate].
  destruct (aligned (r_base r) talign) eqn:Hal; simpl in Hp; [| discriminate].
  inversion Hp; subst; clear Hp.
  assert (r_mode r = Plain) as HrPlain.
  { destruct (r_mode r); simpl in Hpl; try discriminate; reflexivity. }
  apply Nat.leb_le in Hsz.
  destruct Hwithin as [Hw1 Hw2].
  exists g. unfold range_within. simpl.
  repeat split; try assumption; try lia.
  rewrite Hmode. exact HrPlain.
Qed.

(* End-to-end: an allocator grants, carves, and places; the slot stays grounded
   in the same machine -- nothing it hands out escapes the declared memory. *)
Theorem chain_grant_carve_place_grounded :
  forall m g off len tsize talign r' s,
    In g m ->
    carve (region_of_grant g) off len = Some r' ->
    place r' tsize talign = Some s ->
    slot_grounded m s.
Proof.
  intros m g off len tsize talign r' s Hin Hc Hp.
  eapply place_grounds_slot; [| exact Hp].
  eapply carve_preserves_validity; [| exact Hc].
  apply grant_yields_valid_region; exact Hin.
Qed.

(* No wild slot: a grounded slot descends from a declared grant of the machine. *)
Theorem no_wild_slot :
  forall m s, slot_grounded m s -> exists g, In g m /\ g_id g = sl_prov s.
Proof. intros m s [g [Hin [Hid _]]]. exists g. split; assumption. Qed.

(* ================================================================= *)
(* 8. Access-mode discipline + fail-closed placement                 *)
(* ================================================================= *)

Theorem place_rejects_volatile :
  forall r tsize talign, r_mode r = Volatile -> place r tsize talign = None.
Proof. intros r tsize talign Hv. unfold place. rewrite Hv. simpl. reflexivity. Qed.

Theorem place_rejects_atomic :
  forall r tsize talign, r_mode r = Atomic -> place r tsize talign = None.
Proof. intros r tsize talign Ha. unfold place. rewrite Ha. simpl. reflexivity. Qed.

Theorem place_oversize_fail_closed :
  forall r tsize talign, r_size r < tsize -> place r tsize talign = None.
Proof.
  intros r tsize talign Hlt. unfold place.
  assert (Nat.leb tsize (r_size r) = false) as Hf by (apply Nat.leb_gt; lia).
  rewrite Hf, andb_false_l, andb_false_r. reflexivity.
Qed.

Theorem place_misaligned_fail_closed :
  forall r tsize talign,
    aligned (r_base r) talign = false -> place r tsize talign = None.
Proof.
  intros r tsize talign Hal. unfold place.
  rewrite Hal, andb_false_r, andb_false_r. reflexivity.
Qed.

(* ================================================================= *)
(* 9. No aliasing up the chain                                       *)
(* ================================================================= *)

Theorem placed_slots_disjoint :
  forall r1 r2 t1 a1 t2 a2 s1 s2,
    range_disjoint (r_base r1) (r_size r1) (r_base r2) (r_size r2) ->
    place r1 t1 a1 = Some s1 ->
    place r2 t2 a2 = Some s2 ->
    range_disjoint (sl_base s1) (sl_size s1) (sl_base s2) (sl_size s2).
Proof.
  intros r1 r2 t1 a1 t2 a2 s1 s2 Hdis Hp1 Hp2.
  unfold place in Hp1, Hp2.
  destruct (andb (mode_eqb (r_mode r1) Plain)
                 (andb (Nat.leb t1 (r_size r1)) (aligned (r_base r1) a1)))
    eqn:E1; [| discriminate].
  destruct (andb (mode_eqb (r_mode r2) Plain)
                 (andb (Nat.leb t2 (r_size r2)) (aligned (r_base r2) a2)))
    eqn:E2; [| discriminate].
  inversion Hp1; inversion Hp2; subst; simpl.
  apply andb_true_iff in E1 as [_ E1b]. apply andb_true_iff in E1b as [E1sz _].
  apply andb_true_iff in E2 as [_ E2b]. apply andb_true_iff in E2b as [E2sz _].
  apply Nat.leb_le in E1sz. apply Nat.leb_le in E2sz.
  unfold range_disjoint in *. lia.
Qed.

(* ================================================================= *)
(* 10. Capability gate: the metal is an effect                       *)
(* ================================================================= *)

(* The metal capability is required to touch a region. `place_guarded` models
   the language-level gate: without the capability the operation is not even
   attempted -- it fails closed, so ordinary code provably produces no slot from
   a region (no hidden metal access). *)
Definition place_guarded (has_cap : bool) (r : Region) (tsize talign : nat)
  : option Slot :=
  if has_cap then place r tsize talign else None.

Theorem cap_gate_fail_closed :
  forall r tsize talign, place_guarded false r tsize talign = None.
Proof. intros. reflexivity. Qed.

(* The gate restricts WHO may cross it; it does not weaken grounding. *)
Theorem guarded_place_grounds_slot :
  forall m has_cap r tsize talign s,
    region_valid m r ->
    place_guarded has_cap r tsize talign = Some s ->
    slot_grounded m s.
Proof.
  intros m has_cap r tsize talign s Hv Hg.
  unfold place_guarded in Hg. destruct has_cap; [| discriminate].
  eapply place_grounds_slot; eauto.
Qed.

End MachineContactCore.
