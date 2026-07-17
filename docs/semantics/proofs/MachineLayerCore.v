(*
  Pergyra Formal Semantics -- Mechanized Fragment (machine-layer core)
  Target: docs/semantics/19 "Pergyra Abstract Machine Obligation" -- the layer
          BELOW the slot: the raw span that faces the machine.
  Status: proof-sketch; not beta-closure evidence unless checked by CI
          (rocq compile / coqc + rocqchk).
  Budget: 0 admits / 0 axioms (adds nothing to the corpus axiom budget).

  Scope: Slot (SlotCalculus.v / SlotLifecycleCore.v) is a HIGH abstraction --
  typed, owned, lifecycle-tracked. Under it a systems language needs two
  separate responsibilities: an address-evidence layer and the operation/state
  transition that actually contacts the machine. A raw pointer conflates both and forgets the
  extent, provenance, access mode, and lifetime evidence that must be retained.
  This file keeps `Region` as the address-evidence layer and adds an explicit
  `contact_step` transition for plain, volatile, atomic, and fence operations.
  `place : Region -> Slot` remains only the plain-data bridge UP; it is not the
  machine-layer operation. Every contact now requires a declared hardware
  adequacy witness, authority evidence, a live lease, and a mode-compatible
  operation, updates abstract machine state, and emits a machine-layer event.

  Mechanized obligations:
    - Grant soundness: the full region of a declared grant is valid
      (`grant_yields_valid_region`).
    - Allocator safety: carving a valid region yields a valid sub-region
      (`carve_preserves_validity`) and non-overlapping carves are disjoint
      (`carve_disjoint`).
    - Address bridge: placing a typed slot on a valid region yields a
      slot GROUNDED in a real grant: provenance, mode, and bounds preserved
      (`place_grounds_slot`); end-to-end grant->carve->place stays grounded
      (`chain_grant_carve_place_grounded`).
    - Contact soundness: a contact transition requires authority, a live lease,
      a valid declared region, explicit hardware adequacy, and a compatible
      operation mode (`contact_step_requires_*`).
    - Contact observability: every contact appends a typed event to the machine
      trace (`contact_step_emits_event`); no unauthorized or revoked contact is
      derivable (`cap_gate_fail_closed`, `revoked_lease_fail_closed`).
    - Access-mode discipline / fail-closed: a plain data slot may NOT sit on a
      volatile (MMIO) or atomic region (`place_rejects_volatile`,
      `place_rejects_atomic`); actual volatile/atomic operations are modeled by
      `contact_step`, not by `place`.
    - No aliasing up the chain: slots placed on disjoint regions are disjoint
      (`placed_slots_disjoint`).
    - Capability and lifetime gate: contact is impossible without the grant
      authority or after lease revocation; authority and lease state are
      preserved by contact transitions.

  Decide-vs-declare at the metal (docs/19 Rice corner): a `Grant` is DATA here,
  not a Coq Axiom. The *language* treats the first page / MMIO window / linker
  section as DECLARED (you cannot analyze the memory map into existence -- you
  declare it, the deed), and everything above is CHECKED against that declaration
  and fails closed otherwise. The proof takes the grant table as a parameter and
  discharges the chain over it, adding no axiom to the corpus budget.

  Lineage (docs/19 lineage map): region-based memory management
  (Tofte-Talpin region calculus) for the scoped extent, pointer-provenance
  discipline for the grant-rooted `r_prov` tag, and effect/authority transition
  systems for the observable contact step.

  Negative scope: this models the static grounding chain plus an abstract
  contact-event transition. It does NOT claim that a declared hardware map is
  the live boot map, that an event is a concrete instruction, or that volatile
  and atomic ordering/device semantics are fully refined. Full pointer-
  provenance under arbitrary aliasing/type-punning (the Stacked/Tree-Borrows
  obligation), concrete memory values, cache/TLB/DMA behavior, and backend
  adequacy remain explicit refinement obligations.
*)

Require Import Stdlib.Init.Nat.
Require Import Stdlib.Arith.PeanoNat.
Require Import Stdlib.Bool.Bool.
Require Import Stdlib.Lists.List.
Require Import Stdlib.micromega.Lia.
Import ListNotations.

Section MachineLayerCore.

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

(* A machine declaration makes the external boundary explicit.  The proof does
   not pretend that a list of Grant records is hardware: the caller supplies a
   predicate saying which declarations are adequate for the target and must
   provide uniqueness, non-overlap, and adequacy evidence for the declaration
   being used. *)
Definition machine_unique_ids (m : Machine) : Prop :=
  forall g1 g2, In g1 m -> In g2 m -> g_id g1 = g_id g2 -> g1 = g2.

Definition machine_nonoverlap (m : Machine) : Prop :=
  forall g1 g2, In g1 m -> In g2 m -> g1 <> g2 ->
    range_disjoint (g_base g1) (g_size g1) (g_base g2) (g_size g2).

Record MachineDeclaration := mkMachineDeclaration {
  md_grants : Machine;
  md_hardware_adequate : Grant -> Prop;
  md_addr_limit : nat;
  md_unique_ids : machine_unique_ids md_grants;
  md_nonoverlap : machine_nonoverlap md_grants;
  md_adequacy : forall g, In g md_grants -> md_hardware_adequate g;
  md_address_space : forall g, In g md_grants ->
    g_base g + g_size g <= md_addr_limit
}.

Theorem declared_grant_id_unique :
  forall d g1 g2,
    In g1 (md_grants d) -> In g2 (md_grants d) ->
    g_id g1 = g_id g2 -> g1 = g2.
Proof.
  intros d g1 g2 H1 H2 Hid. eapply md_unique_ids; eauto.
Qed.

Theorem declared_grant_hardware_adequate :
  forall d g, In g (md_grants d) -> md_hardware_adequate d g.
Proof. intros d g Hin. eapply md_adequacy; exact Hin. Qed.

Theorem declared_grants_nonoverlap :
  forall d g1 g2,
    In g1 (md_grants d) -> In g2 (md_grants d) -> g1 <> g2 ->
    range_disjoint (g_base g1) (g_size g1) (g_base g2) (g_size g2).
Proof.
  intros d g1 g2 H1 H2 Hneq. eapply md_nonoverlap; eauto.
Qed.

Theorem declared_grant_address_bounded :
  forall d g, In g (md_grants d) ->
    g_base g + g_size g <= md_addr_limit d.
Proof. intros d g Hin. eapply md_address_space; exact Hin. Qed.

(* A region is VALID w.r.t a machine iff it traces to a real grant: same
   provenance id, same access mode, and its bytes lie within the grant. *)
Definition region_valid (m : Machine) (r : Region) : Prop :=
  exists g, In g m /\
            g_id g = r_prov r /\
            g_mode g = r_mode r /\
            range_within (r_base r) (r_size r) (g_base g) (g_size g).

Theorem valid_region_address_bounded :
  forall d r,
    region_valid (md_grants d) r ->
    r_base r + r_size r <= md_addr_limit d.
Proof.
  intros d r [g [Hin [_ [_ Hwithin]]]].
  destruct Hwithin as [Hlo Hhi].
  pose proof (declared_grant_address_bounded d g Hin) as Hlimit.
  lia.
Qed.

(* A valid region is also tied to the external hardware-adequacy predicate of
   the declaration.  Keeping this witness separate prevents "0 Coq axioms"
   from being confused with a proof that the declaration matches live silicon. *)
Definition region_hardware_adequate (d : MachineDeclaration) (r : Region) : Prop :=
  exists g, In g (md_grants d) /\
            g_id g = r_prov r /\
            g_mode g = r_mode r /\
            range_within (r_base r) (r_size r) (g_base g) (g_size g) /\
            md_hardware_adequate d g.

Theorem valid_region_has_declared_hardware_adequacy :
  forall d r,
    region_valid (md_grants d) r -> region_hardware_adequate d r.
Proof.
  intros d r [g [Hin [Hid [Hmode Hwithin]]]].
  exists g. split; [exact Hin|].
  split; [exact Hid|].
  split; [exact Hmode|].
  split; [exact Hwithin|].
  apply declared_grant_hardware_adequate; exact Hin.
Qed.

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
(* 6. Slot: a TypeLayout-identified cell placed on a Plain region (bridge UP) *)
(* ================================================================= *)

Record TypeLayout := mkTypeLayout {
  tl_type_id : nat;             (* nominal type identity                  *)
  tl_size : nat;                (* ABI sizeof(T)                          *)
  tl_align : nat                (* ABI alignment(T), zero is invalid       *)
}.

Record Slot := mkSlot {
  sl_type_id : nat;
  sl_base : Addr;
  sl_size : nat;                (* = tl_size(T) *)
  sl_mode : AccessMode;
  sl_prov : GrantId
}.

Definition aligned (base align : nat) : bool :=
  match align with 0 => false | _ => Nat.eqb (base mod align) 0 end.

(* place r layout: reinterpret the region's bytes as a typed data slot.
   Fail-closed unless: the region is Plain (a data slot may NOT sit on MMIO /
   atomic memory), the type fits, and the base is aligned. *)
Definition place (r : Region) (layout : TypeLayout) : option Slot :=
  if andb (mode_eqb (r_mode r) Plain)
          (andb (Nat.leb (tl_size layout) (r_size r))
                (aligned (r_base r) (tl_align layout)))
  then Some (mkSlot (tl_type_id layout) (r_base r) (tl_size layout)
                     Plain (r_prov r))
  else None.

(* A slot is GROUNDED in a machine iff it traces to a real grant.  The
   `no_wild_slot` lemma below is only a conditional projection from this
   predicate; constructors remain public in this proof fragment, so API-level
   slot unforgeability is a separate compiler/runtime obligation. *)
Definition slot_grounded (m : Machine) (s : Slot) : Prop :=
  exists g, In g m /\
            g_id g = sl_prov s /\
            g_mode g = sl_mode s /\
            range_within (sl_base s) (sl_size s) (g_base g) (g_size g).

(* ================================================================= *)
(* 7. KEYSTONE: place preserves the safety chain                     *)
(* ================================================================= *)

Theorem place_grounds_slot :
  forall m r layout s,
    region_valid m r ->
    place r layout = Some s ->
    slot_grounded m s.
Proof.
  intros m r layout s [g [Hin [Hid [Hmode Hwithin]]]] Hp.
  unfold place in Hp.
  destruct (mode_eqb (r_mode r) Plain) eqn:Hpl; simpl in Hp; [| discriminate].
  destruct (Nat.leb (tl_size layout) (r_size r)) eqn:Hsz; simpl in Hp; [| discriminate].
  destruct (aligned (r_base r) (tl_align layout)) eqn:Hal; simpl in Hp; [| discriminate].
  inversion Hp; subst; clear Hp.
  assert (r_mode r = Plain) as HrPlain.
  { destruct (r_mode r); simpl in Hpl; try discriminate; reflexivity. }
  apply Nat.leb_le in Hsz.
  destruct Hwithin as [Hw1 Hw2].
  exists g. unfold range_within. simpl.
  repeat split; try assumption; try lia.
  rewrite Hmode. exact HrPlain.
Qed.

Theorem place_preserves_layout_identity :
  forall r layout s,
    place r layout = Some s -> sl_type_id s = tl_type_id layout.
Proof.
  intros r layout s Hp. unfold place in Hp.
  destruct (andb (mode_eqb (r_mode r) Plain)
                 (andb (Nat.leb (tl_size layout) (r_size r))
                       (aligned (r_base r) (tl_align layout))))
    eqn:E; [| discriminate].
  inversion Hp. reflexivity.
Qed.

(* End-to-end: an allocator grants, carves, and places; the slot stays grounded
   in the same machine -- nothing it hands out escapes the declared memory. *)
Theorem chain_grant_carve_place_grounded :
  forall m g off len layout r' s,
    In g m ->
    carve (region_of_grant g) off len = Some r' ->
    place r' layout = Some s ->
    slot_grounded m s.
Proof.
  intros m g off len layout r' s Hin Hc Hp.
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
  forall r layout, r_mode r = Volatile -> place r layout = None.
Proof. intros r layout Hv. unfold place. rewrite Hv. simpl. reflexivity. Qed.

Theorem place_rejects_atomic :
  forall r layout, r_mode r = Atomic -> place r layout = None.
Proof. intros r layout Ha. unfold place. rewrite Ha. simpl. reflexivity. Qed.

Theorem place_oversize_fail_closed :
  forall r layout, r_size r < tl_size layout -> place r layout = None.
Proof.
  intros r layout Hlt. unfold place.
  assert (Nat.leb (tl_size layout) (r_size r) = false) as Hf by (apply Nat.leb_gt; lia).
  rewrite Hf, andb_false_l, andb_false_r. reflexivity.
Qed.

Theorem place_misaligned_fail_closed :
  forall r layout,
    aligned (r_base r) (tl_align layout) = false -> place r layout = None.
Proof.
  intros r layout Hal. unfold place.
  rewrite Hal, andb_false_r, andb_false_r. reflexivity.
Qed.

(* ================================================================= *)
(* 9. No aliasing up the chain                                       *)
(* ================================================================= *)

Theorem placed_slots_disjoint :
  forall r1 r2 layout1 layout2 s1 s2,
    range_disjoint (r_base r1) (r_size r1) (r_base r2) (r_size r2) ->
    place r1 layout1 = Some s1 ->
    place r2 layout2 = Some s2 ->
    range_disjoint (sl_base s1) (sl_size s1) (sl_base s2) (sl_size s2).
Proof.
  intros r1 r2 layout1 layout2 s1 s2 Hdis Hp1 Hp2.
  unfold place in Hp1, Hp2.
  destruct (andb (mode_eqb (r_mode r1) Plain)
                 (andb (Nat.leb (tl_size layout1) (r_size r1))
                       (aligned (r_base r1) (tl_align layout1))))
    eqn:E1; [| discriminate].
  destruct (andb (mode_eqb (r_mode r2) Plain)
                 (andb (Nat.leb (tl_size layout2) (r_size r2))
                       (aligned (r_base r2) (tl_align layout2))))
    eqn:E2; [| discriminate].
  inversion Hp1; inversion Hp2; subst; simpl.
  apply andb_true_iff in E1 as [_ E1b]. apply andb_true_iff in E1b as [E1sz _].
  apply andb_true_iff in E2 as [_ E2b]. apply andb_true_iff in E2b as [E2sz _].
  apply Nat.leb_le in E1sz. apply Nat.leb_le in E2sz.
  unfold range_disjoint in *. lia.
Qed.

(* ================================================================= *)
(* 10. Machine contact: authority + lifetime + operation + event     *)
(* ================================================================= *)

(* `place` above is deliberately only the plain-data bridge.  The actual
   machine-facing operation is an explicit effectful transition.  A Region is
   evidence about where the operation may happen; ContactOp says what the
   machine must observe. *)
Inductive ContactOp : Type :=
  | ContactRead
  | ContactWrite (value : nat)
  | ContactVolatileRead
  | ContactVolatileWrite (value : nat)
  | ContactAtomicRmw (value : nat)
  | ContactFence.

Definition contact_mode_allowed (op : ContactOp) (mode : AccessMode) : Prop :=
  match op, mode with
  | ContactRead, Plain => True
  | ContactWrite _, Plain => True
  | ContactVolatileRead, Volatile => True
  | ContactVolatileWrite _, Volatile => True
  | ContactAtomicRmw _, Atomic => True
  | ContactFence, Atomic => True
  | _, _ => False
  end.

Inductive LeaseState : Type :=
  | LeaseLive
  | LeaseRevoked.

Record ContactEvent := mkContactEvent {
  ce_op : ContactOp;
  ce_base : Addr;
  ce_size : nat;
  ce_mode : AccessMode;
  ce_prov : GrantId;
  ce_value : option nat
}.

Record ContactConfig := mkContactConfig {
  cc_memory : Addr -> nat;                (* abstract machine memory cells  *)
  cc_held : list GrantId;                 (* authority evidence, not a bool *)
  cc_lease : GrantId -> LeaseState;       (* explicit lifetime state          *)
  cc_reads : list nat;                    (* read observations in trace order *)
  cc_events : list ContactEvent          (* observable machine-layer trace *)
}.

Definition contact_has_cap (c : ContactConfig) (gid : GrantId) : Prop :=
  In gid (cc_held c).

Definition contact_lease_live (c : ContactConfig) (gid : GrantId) : Prop :=
  cc_lease c gid = LeaseLive.

Definition revoke_contact_lease (c : ContactConfig) (gid : GrantId)
  : ContactConfig :=
  mkContactConfig (cc_memory c) (cc_held c)
                  (fun x => if Nat.eqb x gid then LeaseRevoked else cc_lease c x)
                  (cc_reads c)
                  (cc_events c).

Theorem revoke_contact_lease_revokes :
  forall c gid, contact_lease_live (revoke_contact_lease c gid) gid -> False.
Proof.
  intros c gid H. unfold contact_lease_live, revoke_contact_lease in H.
  change ((if Nat.eqb gid gid then LeaseRevoked else cc_lease c gid) = LeaseLive) in H.
  destruct (Nat.eqb gid gid) eqn:Heq.
  - simpl in H. discriminate.
  - apply Nat.eqb_neq in Heq. exfalso. apply Heq. reflexivity.
Qed.

(* The abstract machine owns a cell-valued memory state.  A concrete backend
   may refine a cell to bytes/registers, but a write must still change the
   machine state and a read must expose the pre-step value. *)
Definition memory_write (m : Addr -> nat) (base value : nat) : Addr -> nat :=
  fun a => if Nat.eqb a base then value else m a.

Definition contact_event_for (c : ContactConfig) (op : ContactOp) (r : Region)
  : ContactEvent :=
  match op with
  | ContactRead => mkContactEvent op (r_base r) (r_size r)
                                  (r_mode r) (r_prov r)
                                  (Some (cc_memory c (r_base r)))
  | ContactWrite value => mkContactEvent op (r_base r) (r_size r)
                                  (r_mode r) (r_prov r) (Some value)
  | ContactVolatileRead => mkContactEvent op (r_base r) (r_size r)
                                  (r_mode r) (r_prov r)
                                  (Some (cc_memory c (r_base r)))
  | ContactVolatileWrite value => mkContactEvent op (r_base r) (r_size r)
                                  (r_mode r) (r_prov r) (Some value)
  | ContactAtomicRmw value => mkContactEvent op (r_base r) (r_size r)
                                  (r_mode r) (r_prov r) (Some value)
  | ContactFence => mkContactEvent op (r_base r) (r_size r)
                                  (r_mode r) (r_prov r) None
  end.

Definition contact_apply (c : ContactConfig) (op : ContactOp) (r : Region)
  : ContactConfig :=
  match op with
  | ContactRead => mkContactConfig (cc_memory c) (cc_held c) (cc_lease c)
                       (cc_memory c (r_base r) :: cc_reads c)
                       (contact_event_for c op r :: cc_events c)
  | ContactWrite value => mkContactConfig
                       (memory_write (cc_memory c) (r_base r) value)
                       (cc_held c) (cc_lease c) (cc_reads c)
                       (contact_event_for c op r :: cc_events c)
  | ContactVolatileRead => mkContactConfig (cc_memory c) (cc_held c) (cc_lease c)
                       (cc_memory c (r_base r) :: cc_reads c)
                       (contact_event_for c op r :: cc_events c)
  | ContactVolatileWrite value => mkContactConfig
                       (memory_write (cc_memory c) (r_base r) value)
                       (cc_held c) (cc_lease c) (cc_reads c)
                       (contact_event_for c op r :: cc_events c)
  | ContactAtomicRmw value => mkContactConfig
                       (memory_write (cc_memory c) (r_base r) value)
                       (cc_held c) (cc_lease c)
                       (cc_memory c (r_base r) :: cc_reads c)
                       (contact_event_for c op r :: cc_events c)
  | ContactFence => mkContactConfig (cc_memory c) (cc_held c) (cc_lease c)
                       (cc_reads c)
                       (contact_event_for c op r :: cc_events c)
  end.

Inductive contact_step (d : MachineDeclaration) :
    ContactOp -> ContactConfig -> Region -> ContactConfig -> Prop :=
| ContactStep : forall op c r,
    region_valid (md_grants d) r ->
    region_hardware_adequate d r ->
    contact_has_cap c (r_prov r) ->
    contact_lease_live c (r_prov r) ->
    contact_mode_allowed op (r_mode r) ->
    contact_step d op c r (contact_apply c op r).

Theorem contact_step_constructible :
  forall d op c r,
    region_valid (md_grants d) r ->
    region_hardware_adequate d r ->
    contact_has_cap c (r_prov r) ->
    contact_lease_live c (r_prov r) ->
    contact_mode_allowed op (r_mode r) ->
    contact_step d op c r (contact_apply c op r).
Proof. intros. constructor; assumption. Qed.

(* A concrete one-grant witness keeps the core from being vacuously safe: the
   positive plain-read path is actually constructible under the declaration,
   authority, and live-lease evidence. *)
Definition sample_grant : Grant := mkGrant 1 0 16 Plain.
Definition sample_machine : Machine := [sample_grant].

Lemma sample_machine_address_space :
  forall g, In g sample_machine -> g_base g + g_size g <= 32.
Proof.
  intros g Hin. simpl in Hin. destruct Hin as [Hin | Hin].
  - subst g. simpl. lia.
  - contradiction.
Qed.

Lemma sample_machine_unique_ids : machine_unique_ids sample_machine.
Proof.
  intros g1 g2 H1 H2 _. simpl in H1, H2.
  destruct H1 as [H1 | H1]; [| contradiction].
  destruct H2 as [H2 | H2]; [| contradiction].
  subst g1. subst g2. reflexivity.
Qed.

Lemma sample_machine_nonoverlap : machine_nonoverlap sample_machine.
Proof.
  intros g1 g2 H1 H2 Hneq. simpl in H1, H2.
  destruct H1 as [H1 | H1]; [| contradiction].
  destruct H2 as [H2 | H2]; [| contradiction].
  subst g1. subst g2. exfalso. apply Hneq. reflexivity.
Qed.

Definition sample_declaration : MachineDeclaration :=
  mkMachineDeclaration sample_machine
                       (fun _ => True)
                       32
                       sample_machine_unique_ids
                       sample_machine_nonoverlap
                       (fun _ _ => I)
                       sample_machine_address_space.

Definition sample_config : ContactConfig :=
  mkContactConfig (fun _ => 0) [1] (fun _ => LeaseLive) [] [].

Theorem sample_plain_read_contact :
  contact_step sample_declaration ContactRead sample_config
               (region_of_grant sample_grant)
               (contact_apply sample_config ContactRead
                              (region_of_grant sample_grant)).
Proof.
  apply ContactStep.
  - apply grant_yields_valid_region. simpl. auto.
  - apply valid_region_has_declared_hardware_adequacy.
    apply grant_yields_valid_region. simpl. auto.
  - simpl. left. reflexivity.
  - reflexivity.
  - exact I.
Qed.

Theorem contact_step_requires_valid_region :
  forall d op c r c',
    contact_step d op c r c' -> region_valid (md_grants d) r.
Proof. intros d op c r c' H. inversion H; assumption. Qed.

Theorem contact_step_requires_hardware_adequacy :
  forall d op c r c',
    contact_step d op c r c' -> region_hardware_adequate d r.
Proof. intros d op c r c' H. inversion H; assumption. Qed.

Theorem contact_step_requires_capability :
  forall d op c r c',
    contact_step d op c r c' -> contact_has_cap c (r_prov r).
Proof. intros d op c r c' H. inversion H; assumption. Qed.

Theorem contact_step_requires_live_lease :
  forall d op c r c',
    contact_step d op c r c' -> contact_lease_live c (r_prov r).
Proof. intros d op c r c' H. inversion H; assumption. Qed.

Theorem contact_step_requires_mode :
  forall d op c r c',
    contact_step d op c r c' -> contact_mode_allowed op (r_mode r).
Proof. intros d op c r c' H. inversion H; assumption. Qed.

Theorem volatile_contact_requires_volatile :
  forall d c r c',
    contact_step d ContactVolatileRead c r c' ->
    r_mode r = Volatile.
Proof.
  intros d c r c' H.
  pose proof (contact_step_requires_mode d ContactVolatileRead c r c' H) as Hmode.
  destruct (r_mode r) eqn:Hr.
  - exfalso. exact Hmode.
  - reflexivity.
  - exfalso. exact Hmode.
Qed.

Theorem atomic_contact_requires_atomic :
  forall d c r c' value,
    contact_step d (ContactAtomicRmw value) c r c' ->
      r_mode r = Atomic.
Proof.
  intros d c r c' value H.
  pose proof (contact_step_requires_mode d (ContactAtomicRmw value) c r c' H) as Hmode.
  destruct (r_mode r) eqn:Hr.
  - exfalso. exact Hmode.
  - exfalso. exact Hmode.
  - reflexivity.
Qed.

Theorem contact_step_preserves_authority :
  forall d op c r c',
    contact_step d op c r c' -> cc_held c' = cc_held c.
Proof.
  intros d op c r c' H. inversion H; subst.
  destruct op; reflexivity.
Qed.

Theorem contact_step_preserves_lease :
  forall d op c r c',
    contact_step d op c r c' -> cc_lease c' = cc_lease c.
Proof.
  intros d op c r c' H. inversion H; subst.
  destruct op; reflexivity.
Qed.

Theorem contact_step_emits_event :
  forall d op c r c',
    contact_step d op c r c' ->
    cc_events c' = contact_event_for c op r :: cc_events c.
Proof.
  intros d op c r c' H. inversion H; subst.
  destruct op; reflexivity.
Qed.

Theorem contact_step_reads_current_value :
  forall d c r c',
    contact_step d ContactRead c r c' ->
    cc_reads c' = cc_memory c (r_base r) :: cc_reads c.
Proof.
  intros d c r c' H. inversion H; reflexivity.
Qed.

Theorem contact_step_volatile_reads_current_value :
  forall d c r c',
    contact_step d ContactVolatileRead c r c' ->
    cc_reads c' = cc_memory c (r_base r) :: cc_reads c.
Proof.
  intros d c r c' H. inversion H; reflexivity.
Qed.

Theorem contact_step_writes_value :
  forall d c r c' value,
    contact_step d (ContactWrite value) c r c' ->
    cc_memory c' (r_base r) = value.
Proof.
  intros d c r c' value H. inversion H; subst.
  simpl. unfold memory_write.
  rewrite Nat.eqb_refl.
  reflexivity.
Qed.

Theorem contact_step_volatile_writes_value :
  forall d c r c' value,
    contact_step d (ContactVolatileWrite value) c r c' ->
    cc_memory c' (r_base r) = value.
Proof.
  intros d c r c' value H. inversion H; subst.
  simpl. unfold memory_write.
  rewrite Nat.eqb_refl.
  reflexivity.
Qed.

Theorem contact_step_atomic_rmw_reads_before_write :
  forall d c r c' value,
    contact_step d (ContactAtomicRmw value) c r c' ->
    cc_reads c' = cc_memory c (r_base r) :: cc_reads c.
Proof.
  intros d c r c' value H. inversion H; reflexivity.
Qed.

Theorem contact_step_atomic_rmw_writes_value :
  forall d c r c' value,
    contact_step d (ContactAtomicRmw value) c r c' ->
    cc_memory c' (r_base r) = value.
Proof.
  intros d c r c' value H. inversion H; subst.
  simpl. unfold memory_write.
  rewrite Nat.eqb_refl.
  reflexivity.
Qed.

Theorem contact_step_fence_preserves_memory :
  forall d c r c',
    contact_step d ContactFence c r c' ->
    cc_memory c' = cc_memory c.
Proof.
  intros d c r c' H. inversion H; reflexivity.
Qed.

(* Negative gates: there is no contact path without authority or after lease
   revocation.  The constructor itself requires evidence held in the current
   state; a boolean mode bit is not an authority witness. *)
Theorem cap_gate_fail_closed :
  forall d op c r c',
    ~ contact_has_cap c (r_prov r) ->
    ~ contact_step d op c r c'.
Proof.
  intros d op c r c' Hno Hstep.
  apply Hno. eapply contact_step_requires_capability; exact Hstep.
Qed.

Theorem revoked_lease_fail_closed :
  forall d op c r c',
    cc_lease c (r_prov r) = LeaseRevoked ->
    ~ contact_step d op c r c'.
Proof.
  intros d op c r c' Hrev Hstep.
  pose proof (contact_step_requires_live_lease d op c r c' Hstep) as Hlive.
  unfold contact_lease_live in Hlive.
  rewrite Hrev in Hlive. discriminate.
Qed.

Theorem contact_mode_fail_closed :
  forall d op c r c',
    ~ contact_mode_allowed op (r_mode r) ->
    ~ contact_step d op c r c'.
Proof.
  intros d op c r c' Hno Hstep.
  apply Hno. eapply contact_step_requires_mode; exact Hstep.
Qed.

Theorem sample_plain_region_rejects_volatile_read :
  forall c',
    ~ contact_step sample_declaration ContactVolatileRead sample_config
        (region_of_grant sample_grant) c'.
Proof.
  intros c'. apply contact_mode_fail_closed. simpl. intro H. exact H.
Qed.

Theorem sample_revoked_region_rejects_read :
  forall c',
    ~ contact_step sample_declaration ContactRead
        (revoke_contact_lease sample_config 1)
        (region_of_grant sample_grant) c'.
Proof.
  intros c'. apply revoked_lease_fail_closed. simpl. reflexivity.
Qed.

(* The old `no_wild_slot` name was only a projection from the grounded-slot
   predicate.  The real no-ambient-contact theorem is explicit below. *)
Theorem no_ambient_machine_contact :
  forall d op c r c',
    ~ contact_has_cap c (r_prov r) ->
    ~ contact_step d op c r c'.
Proof. intros. eapply cap_gate_fail_closed; eauto. Qed.

End MachineLayerCore.
