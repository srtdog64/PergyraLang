(*
  Pergyra Formal Semantics - Mechanized Sketch
  Target: Slot Capability Calculus & Pin Non-Eviction Lemma
  Status: proof-sketch; not beta-closure evidence unless checked by CI
  Scope: this file models one small-step invariant, not the full language.
  Negative scope: this file does not prove Rust-style borrow checking,
  aliasing-XOR-mutability, lexical no-escape, async/task boundary safety,
  or CFG cleanup insertion. Those are separate CFG/body-dataflow proof
  obligations in the beta checklist and ownership proof pack.

  Reuse (2026-10-08). A released slot stays in the heap as a tombstone
  that keeps its generation, and claiming it again advances the
  generation, as the runtime does (slot_manager_core_ops.c:
  recycledGeneration + 1). The earlier rule claimed every free id at
  generation 1, so a handle issued before a release read the next
  occupant ([gen_one_reclaim_resurrects]). [stale_step] and
  [stale_handle_never_admitted] show that a handle, once stale, stays
  refused through every later step, reuse included. Generations are
  unbounded naturals here; the runtime's 32-bit counter refines them by
  refusing a recycle at UINT32_MAX (SLOT_ERROR_ID_EXHAUSTED).

  Unpin (2026-10-08). Unpin needs a capability that verifies for pinning
  the slot's generation, as docs/semantics/08 states and as the runtime
  enforces through the pinned view (slot_manager_pin.c). Without that
  guard any context could unpin and then release a pinned slot in two
  steps. [pin_holds_without_pin_capability] states non-eviction over any
  number of steps.
*)

Require Import Stdlib.Init.Nat.
Require Import Stdlib.Arith.PeanoNat.
Require Import Stdlib.Bool.Bool.
Require Import Stdlib.micromega.Lia.

(* ========================================== *)
(* 1. Domains & State Definition              *)
(* ========================================== *)

Definition SlotId := nat.
Definition Generation := nat.
Definition Token := nat. (* Abstract Cryptographic Token *)
Definition Value := nat.
Parameter MaxSlotId : SlotId.

Inductive AccessMode : Type :=
  | ModeRead : AccessMode
  | ModeWrite : AccessMode
  | ModeRelease : AccessMode
  | ModePin : AccessMode
  | ModeClaim : AccessMode.

Inductive PinState : Type :=
  | Unpinned : PinState
  | Pinned : PinState.

(* [s_live] is false for a released slot: a tombstone that keeps the
   generation of its last occupant. *)
Record Slot : Type := mkSlot {
  s_val : Value;
  s_gen : Generation;
  s_pin : PinState;
  s_live : bool
}.

Record Handle : Type := mkHandle {
  h_slot : SlotId;
  h_gen : Generation
}.

Record PinnedView : Type := mkPinnedView {
  v_slot : SlotId;
  v_gen : Generation
}.

(*
  Heap represents the global memory state (\Sigma).
  Maps a SlotId to an optional Slot. None means never claimed; a released
  slot is a tombstone (s_live = false).
*)
Definition Heap := SlotId -> option Slot.

(*
  Capability Environment (\Delta).
  Represents the set of valid tokens the current execution context holds.
*)
Definition CapEnv := Token -> bool.

(* Abstract verification function for Intent Security *)
Parameter verify_token : Token -> SlotId -> Generation -> AccessMode -> bool.

(* Utility function to update the Heap *)
Definition update_heap (h: Heap) (id: SlotId) (s: option Slot) : Heap :=
  fun x => if Nat.eqb x id then s else h x.

Definition ValidSlotId (id: SlotId) : Prop :=
  id <> 0 /\ id <> MaxSlotId.

Definition FreshSlotId (h: Heap) (id: SlotId) : Prop :=
  h id = None /\ ValidSlotId id.

(* ========================================== *)
(* 2. Operational Semantics (State Transitions)*)
(* ========================================== *)

(*
  The 'Step' inductive relation defines the legal state transitions
  in the Pergyra Memory Model.
*)
Inductive Step (h: Heap) (caps: CapEnv) : Heap -> Prop :=

  (* Rule 0: Claim (fresh non-sentinel id; tombstone at zero/wrap boundary) *)
  | Step_Claim : forall id tok h',
      FreshSlotId h id ->
      caps tok = true ->
      verify_token tok id 1 ModeClaim = true ->
      h' = update_heap h id (Some (mkSlot 0 1 Unpinned true)) ->
      Step h caps h'

  (* Rule 0b: Reclaim (a released slot is reused at the next generation) *)
  | Step_Reclaim : forall id s tok h',
      h id = Some s ->
      s_live s = false ->
      caps tok = true ->
      verify_token tok id (S (s_gen s)) ModeClaim = true ->
      h' = update_heap h id (Some (mkSlot 0 (S (s_gen s)) Unpinned true)) ->
      Step h caps h'

  (* Rule 1: Read (Zero-state-change, requires Token verification) *)
  | Step_Read : forall id s tok,
      h id = Some s ->
      s_live s = true ->
      caps tok = true ->
      verify_token tok id (s_gen s) ModeRead = true ->
      Step h caps h

  (* Rule 2: Write (state change, requires write capability) *)
  | Step_Write : forall id s tok value h',
      h id = Some s ->
      s_live s = true ->
      caps tok = true ->
      verify_token tok id (s_gen s) ModeWrite = true ->
      h' = update_heap h id (Some (mkSlot value (s_gen s) (s_pin s) true)) ->
      Step h caps h'

  (* Rule 3: Pin (Lease capability, locks the slot in memory) *)
  | Step_Pin : forall id s tok h',
      h id = Some s ->
      s_live s = true ->
      caps tok = true ->
      verify_token tok id (s_gen s) ModePin = true ->
      s_pin s = Unpinned ->
      h' = update_heap h id (Some (mkSlot (s_val s) (s_gen s) Pinned true)) ->
      Step h caps h'

  (* Rule 4: Unpin (Release lease) *)
  | Step_Unpin : forall id s tok h',
      h id = Some s ->
      s_live s = true ->
      s_pin s = Pinned ->
      caps tok = true ->
      verify_token tok id (s_gen s) ModePin = true ->
      h' = update_heap h id (Some (mkSlot (s_val s) (s_gen s) Unpinned true)) ->
      Step h caps h'

  (* Rule 5: Release (Evict from heap. STRICTLY requires Unpinned state).
     The slot becomes a tombstone that keeps its generation. *)
  | Step_Release : forall id s tok h',
      h id = Some s ->
      s_live s = true ->
      caps tok = true ->
      verify_token tok id (s_gen s) ModeRelease = true ->
      s_pin s = Unpinned ->
      h' = update_heap h id (Some (mkSlot 0 (s_gen s) Unpinned false)) ->
      Step h caps h'.

Definition HandleRead (h: Heap) (caps: CapEnv) (handle: Handle) (tok: Token) : Prop :=
  exists s,
    h (h_slot handle) = Some s /\
    s_live s = true /\
    h_gen handle = s_gen s /\
    caps tok = true /\
    verify_token tok (h_slot handle) (h_gen handle) ModeRead = true.

Definition HandleWrite (h: Heap) (caps: CapEnv) (handle: Handle) (tok: Token) : Prop :=
  exists s,
    h (h_slot handle) = Some s /\
    s_live s = true /\
    h_gen handle = s_gen s /\
    caps tok = true /\
    verify_token tok (h_slot handle) (h_gen handle) ModeWrite = true.

Definition HandlePin (h: Heap) (caps: CapEnv) (handle: Handle) (tok: Token) : Prop :=
  exists s,
    h (h_slot handle) = Some s /\
    s_live s = true /\
    h_gen handle = s_gen s /\
    s_pin s = Unpinned /\
    caps tok = true /\
    verify_token tok (h_slot handle) (h_gen handle) ModePin = true.

Definition HandleRelease (h: Heap) (caps: CapEnv) (handle: Handle) (tok: Token) : Prop :=
  exists s,
    h (h_slot handle) = Some s /\
    s_live s = true /\
    h_gen handle = s_gen s /\
    s_pin s = Unpinned /\
    caps tok = true /\
    verify_token tok (h_slot handle) (h_gen handle) ModeRelease = true.

Definition ViewUnpin (h: Heap) (view: PinnedView) : Prop :=
  exists s,
    h (v_slot view) = Some s /\
    s_live s = true /\
    v_gen view = s_gen s /\
    s_pin s = Pinned.

(* ========================================== *)
(* 3. Core Theorems & Lemmas                  *)
(* ========================================== *)

(*
  Lemma: Stale Handle Read Impossible
  Proof Obligation: a handle whose generation differs from the current slot
  generation cannot satisfy the read rule.
*)
Lemma stale_handle_read_impossible : forall h caps handle s tok,
  h (h_slot handle) = Some s ->
  h_gen handle <> s_gen s ->
  ~ HandleRead h caps handle tok.
Proof.
  intros h caps handle s tok H_slot H_stale H_read.
  unfold HandleRead in H_read.
  destruct H_read as [read_slot [H_read_slot [_ [H_read_gen [_ H_verify]]]]].
  rewrite H_slot in H_read_slot.
  inversion H_read_slot; subst.
  apply H_stale.
  exact H_read_gen.
Qed.

Lemma stale_handle_write_impossible : forall h caps handle s tok,
  h (h_slot handle) = Some s ->
  h_gen handle <> s_gen s ->
  ~ HandleWrite h caps handle tok.
Proof.
  intros h caps handle s tok H_slot H_stale H_write.
  unfold HandleWrite in H_write.
  destruct H_write as [write_slot [H_write_slot [_ [H_write_gen [_ H_verify]]]]].
  rewrite H_slot in H_write_slot.
  inversion H_write_slot; subst.
  apply H_stale.
  exact H_write_gen.
Qed.

Lemma stale_handle_release_impossible : forall h caps handle s tok,
  h (h_slot handle) = Some s ->
  h_gen handle <> s_gen s ->
  ~ HandleRelease h caps handle tok.
Proof.
  intros h caps handle s tok H_slot H_stale H_release.
  unfold HandleRelease in H_release.
  destruct H_release as [release_slot [H_release_slot [_ [H_release_gen [_ [_ H_verify]]]]]].
  rewrite H_slot in H_release_slot.
  inversion H_release_slot; subst.
  apply H_stale.
  exact H_release_gen.
Qed.

(*
  Lemma: Handle Read Requires Issued Token
  Proof Obligation: every successful handle read must use a capability that is
  present in the current capability environment.
*)
Lemma handle_read_requires_issued_token : forall h caps handle tok,
  HandleRead h caps handle tok ->
  caps tok = true.
Proof.
  intros h caps handle tok H_read.
  unfold HandleRead in H_read.
  destruct H_read as [_ [_ [_ [_ [H_cap _]]]]].
  exact H_cap.
Qed.

(*
  Lemma: Unissued Token Read Impossible
  Proof Obligation: source code cannot satisfy the stable read rule with a token
  absent from the runtime-issued capability environment.
*)
Lemma unissued_token_read_impossible : forall h caps handle tok,
  caps tok = false ->
  ~ HandleRead h caps handle tok.
Proof.
  intros h caps handle tok H_unissued H_read.
  apply handle_read_requires_issued_token in H_read.
  rewrite H_unissued in H_read.
  discriminate.
Qed.

Lemma handle_write_requires_issued_token : forall h caps handle tok,
  HandleWrite h caps handle tok ->
  caps tok = true.
Proof.
  intros h caps handle tok H_write.
  unfold HandleWrite in H_write.
  destruct H_write as [_ [_ [_ [_ [H_cap _]]]]].
  exact H_cap.
Qed.

Lemma unissued_token_write_impossible : forall h caps handle tok,
  caps tok = false ->
  ~ HandleWrite h caps handle tok.
Proof.
  intros h caps handle tok H_unissued H_write.
  apply handle_write_requires_issued_token in H_write.
  rewrite H_unissued in H_write.
  discriminate.
Qed.

Lemma handle_pin_requires_issued_token : forall h caps handle tok,
  HandlePin h caps handle tok ->
  caps tok = true.
Proof.
  intros h caps handle tok H_pin.
  unfold HandlePin in H_pin.
  destruct H_pin as [_ [_ [_ [_ [_ [H_cap _]]]]]].
  exact H_cap.
Qed.

Lemma unissued_token_pin_impossible : forall h caps handle tok,
  caps tok = false ->
  ~ HandlePin h caps handle tok.
Proof.
  intros h caps handle tok H_unissued H_pin.
  apply handle_pin_requires_issued_token in H_pin.
  rewrite H_unissued in H_pin.
  discriminate.
Qed.

Lemma handle_release_requires_issued_token : forall h caps handle tok,
  HandleRelease h caps handle tok ->
  caps tok = true.
Proof.
  intros h caps handle tok H_release.
  unfold HandleRelease in H_release.
  destruct H_release as [_ [_ [_ [_ [_ [H_cap _]]]]]].
  exact H_cap.
Qed.

Lemma unissued_token_release_impossible : forall h caps handle tok,
  caps tok = false ->
  ~ HandleRelease h caps handle tok.
Proof.
  intros h caps handle tok H_unissued H_release.
  apply handle_release_requires_issued_token in H_release.
  rewrite H_unissued in H_release.
  discriminate.
Qed.

Lemma claim_requires_valid_slot_id : forall h id,
  FreshSlotId h id ->
  ValidSlotId id.
Proof.
  intros h id H_fresh.
  unfold FreshSlotId in H_fresh.
  destruct H_fresh as [_ H_valid].
  exact H_valid.
Qed.

Lemma zero_slot_id_claim_impossible : forall h,
  ~ FreshSlotId h 0.
Proof.
  intros h H_fresh.
  apply claim_requires_valid_slot_id in H_fresh.
  unfold ValidSlotId in H_fresh.
  destruct H_fresh as [H_nonzero _].
  apply H_nonzero.
  reflexivity.
Qed.

Lemma max_slot_id_claim_impossible : forall h,
  ~ FreshSlotId h MaxSlotId.
Proof.
  intros h H_fresh.
  apply claim_requires_valid_slot_id in H_fresh.
  unfold ValidSlotId in H_fresh.
  destruct H_fresh as [_ H_not_max].
  apply H_not_max.
  reflexivity.
Qed.

Lemma tampered_view_unpin_impossible : forall h view s,
  h (v_slot view) = Some s ->
  v_gen view <> s_gen s ->
  ~ ViewUnpin h view.
Proof.
  intros h view s H_slot H_stale H_unpin.
  unfold ViewUnpin in H_unpin.
  destruct H_unpin as [unpin_slot [H_unpin_slot [_ [H_unpin_gen _]]]].
  rewrite H_slot in H_unpin_slot.
  inversion H_unpin_slot; subst.
  apply H_stale.
  exact H_unpin_gen.
Qed.

Lemma double_unpin_impossible : forall h view s,
  h (v_slot view) = Some s ->
  s_pin s = Unpinned ->
  ~ ViewUnpin h view.
Proof.
  intros h view s H_slot H_unpinned H_unpin.
  unfold ViewUnpin in H_unpin.
  destruct H_unpin as [unpin_slot [H_unpin_slot [_ [_ H_pinned]]]].
  rewrite H_slot in H_unpin_slot.
  inversion H_unpin_slot; subst.
  rewrite H_unpinned in H_pinned.
  discriminate.
Qed.

Lemma pinned_handle_release_impossible : forall h caps handle s tok,
  h (h_slot handle) = Some s ->
  s_pin s = Pinned ->
  ~ HandleRelease h caps handle tok.
Proof.
  intros h caps handle s tok H_slot H_pinned H_release.
  unfold HandleRelease in H_release.
  destruct H_release as [release_slot [H_release_slot [_ [_ [H_unpinned [_ _]]]]]].
  rewrite H_slot in H_release_slot.
  inversion H_release_slot; subst.
  rewrite H_pinned in H_unpinned.
  discriminate.
Qed.

(*
  Lemma: Released Slot Read/Write/Pin/Release Impossible
  Proof Obligation: a handle pointing to a never-claimed slot (None) or to a
  released tombstone cannot be read, written, pinned, or released.
*)
Lemma released_slot_read_impossible : forall h caps handle tok,
  h (h_slot handle) = None ->
  ~ HandleRead h caps handle tok.
Proof.
  intros h caps handle tok H_none H_read.
  unfold HandleRead in H_read.
  destruct H_read as [s [H_some _]].
  rewrite H_none in H_some.
  discriminate.
Qed.

Lemma released_slot_write_impossible : forall h caps handle tok,
  h (h_slot handle) = None ->
  ~ HandleWrite h caps handle tok.
Proof.
  intros h caps handle tok H_none H_write.
  unfold HandleWrite in H_write.
  destruct H_write as [s [H_some _]].
  rewrite H_none in H_some.
  discriminate.
Qed.

Lemma released_slot_pin_impossible : forall h caps handle tok,
  h (h_slot handle) = None ->
  ~ HandlePin h caps handle tok.
Proof.
  intros h caps handle tok H_none H_pin.
  unfold HandlePin in H_pin.
  destruct H_pin as [s [H_some _]].
  rewrite H_none in H_some.
  discriminate.
Qed.

Lemma released_slot_release_impossible : forall h caps handle tok,
  h (h_slot handle) = None ->
  ~ HandleRelease h caps handle tok.
Proof.
  intros h caps handle tok H_none H_release.
  unfold HandleRelease in H_release.
  destruct H_release as [s [H_some _]].
  rewrite H_none in H_some.
  discriminate.
Qed.

Lemma tombstone_read_impossible : forall h caps handle s tok,
  h (h_slot handle) = Some s -> s_live s = false -> ~ HandleRead h caps handle tok.
Proof.
  intros h caps handle s tok H_slot H_dead [s' [H_s' [H_live _]]].
  rewrite H_slot in H_s'. inversion H_s'; subst. rewrite H_dead in H_live. discriminate.
Qed.

Lemma tombstone_write_impossible : forall h caps handle s tok,
  h (h_slot handle) = Some s -> s_live s = false -> ~ HandleWrite h caps handle tok.
Proof.
  intros h caps handle s tok H_slot H_dead [s' [H_s' [H_live _]]].
  rewrite H_slot in H_s'. inversion H_s'; subst. rewrite H_dead in H_live. discriminate.
Qed.

Lemma tombstone_pin_impossible : forall h caps handle s tok,
  h (h_slot handle) = Some s -> s_live s = false -> ~ HandlePin h caps handle tok.
Proof.
  intros h caps handle s tok H_slot H_dead [s' [H_s' [H_live _]]].
  rewrite H_slot in H_s'. inversion H_s'; subst. rewrite H_dead in H_live. discriminate.
Qed.

Lemma tombstone_release_impossible : forall h caps handle s tok,
  h (h_slot handle) = Some s -> s_live s = false -> ~ HandleRelease h caps handle tok.
Proof.
  intros h caps handle s tok H_slot H_dead [s' [H_s' [H_live _]]].
  rewrite H_slot in H_s'. inversion H_s'; subst. rewrite H_dead in H_live. discriminate.
Qed.

(*
  Lemma: Pin Non-Eviction
  Proof Obligation: "A Pinned slot cannot be released or evicted by any Step rule."
  A live pinned slot stays live through every step.
*)
Lemma pin_non_eviction : forall h caps id s h',
  h id = Some s ->
  s_live s = true ->
  s_pin s = Pinned ->
  Step h caps h' ->
  exists s', h' id = Some s' /\ s_live s' = true.
Proof.
  intros h caps pinned_id pinned_slot next_heap H_some H_live H_pinned H_step.
  destruct H_step as
    [ claim_id tok claim_heap H_fresh H_cap H_verify H_heap
    | reclaim_id reclaim_slot tok reclaim_heap H_reclaim H_dead H_cap H_verify H_heap
    | read_id read_slot tok H_read H_rlive H_cap H_verify
    | write_id write_slot tok value write_heap H_write H_wlive H_cap H_verify H_heap
    | pin_id pin_slot tok pin_heap H_pin H_plive H_cap H_verify H_unpinned H_heap
    | unpin_id unpin_slot tok unpin_heap H_unpin H_ulive H_was_pinned H_cap H_verify H_heap
    | release_id release_slot tok release_heap H_release H_llive H_cap H_verify H_unpinned H_heap
    ]; subst; unfold update_heap.

  - (* Step_Claim: claim requires a never-claimed id, so it cannot touch an
       already-claimed pinned slot. *)
    destruct (Nat.eqb pinned_id claim_id) eqn:E_eq.
    + apply Nat.eqb_eq in E_eq. subst.
      unfold FreshSlotId in H_fresh.
      destruct H_fresh as [H_empty _].
      rewrite H_some in H_empty. discriminate.
    + exists pinned_slot. split; assumption.

  - (* Step_Reclaim: reclaim requires a tombstone, and this slot is live. *)
    destruct (Nat.eqb pinned_id reclaim_id) eqn:E_eq.
    + apply Nat.eqb_eq in E_eq. subst.
      rewrite H_some in H_reclaim. inversion H_reclaim; subst.
      rewrite H_live in H_dead. discriminate.
    + exists pinned_slot. split; assumption.

  - (* Step_Read: the heap is unchanged. *)
    exists pinned_slot. split; assumption.

  - (* Step_Write: updates either this slot, which stays live, or another
       slot. *)
    destruct (Nat.eqb pinned_id write_id).
    + eexists. split; reflexivity.
    + exists pinned_slot. split; assumption.

  - (* Step_Pin: updates either this slot, which stays live, or another slot. *)
    destruct (Nat.eqb pinned_id pin_id).
    + eexists. split; reflexivity.
    + exists pinned_slot. split; assumption.

  - (* Step_Unpin: updates either this slot, which stays live, or another slot. *)
    destruct (Nat.eqb pinned_id unpin_id).
    + eexists. split; reflexivity.
    + exists pinned_slot. split; assumption.

  - (* Step_Release: a release of this pinned slot is impossible; another slot
       release preserves this slot. *)
    destruct (Nat.eqb pinned_id release_id) eqn:E_eq.
    + apply Nat.eqb_eq in E_eq. subst.
      rewrite H_some in H_release. inversion H_release; subst.
      rewrite H_pinned in H_unpinned. discriminate.
    + exists pinned_slot. split; assumption.
Qed.

(* Without a capability that verifies for pinning this slot's generation,
   no step unpins, releases, or reclaims a live pinned slot, and none
   changes its generation. *)
Lemma pin_step : forall caps h h' id s,
  Step h caps h' -> h id = Some s -> s_live s = true -> s_pin s = Pinned ->
  (forall tok, caps tok = true -> verify_token tok id (s_gen s) ModePin = false) ->
  exists s', h' id = Some s' /\ s_live s' = true /\ s_pin s' = Pinned /\ s_gen s' = s_gen s.
Proof.
  intros caps h h' id s Hstep Hid Hlive Hpin Hno.
  destruct Hstep as
    [ cid tok ch H_fresh H_cap H_verify H_heap
    | rid rs tok rh H_rs H_dead H_cap H_verify H_heap
    | did ds tok H_ds H_dlive H_cap H_verify
    | wid ws tok value wh H_ws H_wlive H_cap H_verify H_heap
    | pid ps tok ph H_ps H_plive H_cap H_verify H_unpinned H_heap
    | uid us tok uh H_us H_ulive H_upinned H_cap H_verify H_heap
    | lid ls tok lh H_ls H_llive H_cap H_verify H_unpinned H_heap ]; subst; unfold update_heap.
  - destruct (Nat.eqb id cid) eqn:E; [|exists s; repeat split; assumption].
    apply Nat.eqb_eq in E. subst. destruct H_fresh as [F _]. rewrite Hid in F. discriminate.
  - destruct (Nat.eqb id rid) eqn:E; [|exists s; repeat split; assumption].
    apply Nat.eqb_eq in E. subst. rewrite Hid in H_rs. injection H_rs as H_rs. subst rs.
    rewrite Hlive in H_dead. discriminate.
  - exists s. repeat split; assumption.
  - destruct (Nat.eqb id wid) eqn:E; [|exists s; repeat split; assumption].
    apply Nat.eqb_eq in E. subst. rewrite Hid in H_ws. injection H_ws as H_ws. subst ws.
    eexists. split; [reflexivity|]. simpl. split; [reflexivity| split; [exact Hpin| reflexivity]].
  - destruct (Nat.eqb id pid) eqn:E; [|exists s; repeat split; assumption].
    apply Nat.eqb_eq in E. subst. rewrite Hid in H_ps. injection H_ps as H_ps. subst ps.
    rewrite Hpin in H_unpinned. discriminate.
  - destruct (Nat.eqb id uid) eqn:E; [|exists s; repeat split; assumption].
    apply Nat.eqb_eq in E. subst. rewrite Hid in H_us. injection H_us as H_us. subst us.
    rewrite (Hno tok H_cap) in H_verify. discriminate.
  - destruct (Nat.eqb id lid) eqn:E; [|exists s; repeat split; assumption].
    apply Nat.eqb_eq in E. subst. rewrite Hid in H_ls. injection H_ls as H_ls. subst ls.
    rewrite Hpin in H_unpinned. discriminate.
Qed.

(* ========================================== *)
(* 4. Reuse: a stale handle stays refused     *)
(* ========================================== *)

(* A handle is stale when its slot has moved past its generation, or is a
   tombstone of its generation. *)
Definition HandleStale (h: Heap) (handle: Handle) : Prop :=
  exists s, h (h_slot handle) = Some s /\
    (h_gen handle < s_gen s \/ (h_gen handle = s_gen s /\ s_live s = false)).

Lemma stale_handle_admits_nothing : forall h caps handle tok,
  HandleStale h handle ->
  ~ HandleRead h caps handle tok /\ ~ HandleWrite h caps handle tok /\
  ~ HandlePin h caps handle tok /\ ~ HandleRelease h caps handle tok.
Proof.
  intros h caps handle tok [s [H_slot [H_lt | [H_eq H_dead]]]].
  - assert (H_ne : h_gen handle <> s_gen s) by lia.
    split; [eapply stale_handle_read_impossible; eassumption|].
    split; [eapply stale_handle_write_impossible; eassumption|].
    split; [|eapply stale_handle_release_impossible; eassumption].
    intros [s' [H_s' [_ [H_g _]]]]. rewrite H_slot in H_s'. inversion H_s'; subst. lia.
  - split; [eapply tombstone_read_impossible; eassumption|].
    split; [eapply tombstone_write_impossible; eassumption|].
    split; [eapply tombstone_pin_impossible; eassumption|].
    eapply tombstone_release_impossible; eassumption.
Qed.

(* Every step keeps a stale handle stale: reclaiming its slot moves the
   generation further on, and no step moves it back. *)
Theorem stale_step : forall h caps h' handle,
  Step h caps h' -> HandleStale h handle -> HandleStale h' handle.
Proof.
  intros h caps h' handle H_step [s [H_slot H_old]].
  destruct H_step as
    [ id tok h1 H_fresh H_cap H_verify H_heap
    | id s0 tok h1 H_s0 H_dead H_cap H_verify H_heap
    | id s0 tok H_s0 H_live H_cap H_verify
    | id s0 tok value h1 H_s0 H_live H_cap H_verify H_heap
    | id s0 tok h1 H_s0 H_live H_cap H_verify H_unpinned H_heap
    | id s0 tok h1 H_s0 H_live H_pinned H_cap H_verify H_heap
    | id s0 tok h1 H_s0 H_live H_cap H_verify H_unpinned H_heap
    ]; subst; unfold HandleStale, update_heap;
    try (exists s; split; [exact H_slot| exact H_old]);
    destruct (Nat.eqb (h_slot handle) id) eqn:E;
    try (exists s; split; [exact H_slot| exact H_old]);
    apply Nat.eqb_eq in E; rewrite E in H_slot.
  - unfold FreshSlotId in H_fresh. rewrite H_slot in H_fresh. destruct H_fresh as [F _]. discriminate.
  - rewrite H_slot in H_s0. inversion H_s0; subst.
    eexists. split; [reflexivity|]. simpl. left. destruct H_old as [H_lt|[H_eq _]]; lia.
  - rewrite H_slot in H_s0. inversion H_s0; subst.
    eexists. split; [reflexivity|]. simpl.
    destruct H_old as [H_lt|[_ H_dead]]; [left; exact H_lt| rewrite H_dead in H_live; discriminate].
  - rewrite H_slot in H_s0. inversion H_s0; subst.
    eexists. split; [reflexivity|]. simpl.
    destruct H_old as [H_lt|[_ H_dead]]; [left; exact H_lt| rewrite H_dead in H_live; discriminate].
  - rewrite H_slot in H_s0. inversion H_s0; subst.
    eexists. split; [reflexivity|]. simpl.
    destruct H_old as [H_lt|[_ H_dead]]; [left; exact H_lt| rewrite H_dead in H_live; discriminate].
  - rewrite H_slot in H_s0. inversion H_s0; subst.
    eexists. split; [reflexivity|]. simpl.
    destruct H_old as [H_lt|[_ H_dead]]; [left; exact H_lt| rewrite H_dead in H_live; discriminate].
Qed.

Inductive Steps (caps: CapEnv) : Heap -> Heap -> Prop :=
  | Steps_refl : forall h, Steps caps h h
  | Steps_step : forall h h1 h2, Step h caps h1 -> Steps caps h1 h2 -> Steps caps h h2.

Theorem stale_handle_never_admitted : forall caps h h' handle tok,
  Steps caps h h' -> HandleStale h handle ->
  ~ HandleRead h' caps handle tok /\ ~ HandleWrite h' caps handle tok /\
  ~ HandlePin h' caps handle tok /\ ~ HandleRelease h' caps handle tok.
Proof.
  intros caps h h' handle tok H_steps.
  induction H_steps as [h0|h0 h1 h2 H_step H_rest IH]; intros H_stale.
  - apply stale_handle_admits_nothing. exact H_stale.
  - apply IH. eapply stale_step; eassumption.
Qed.

(* Pin non-eviction over any number of steps: a live pinned slot stays
   live and pinned, at its generation, unless some step holds a capability
   that verifies for pinning it. Counterexample for the earlier unguarded
   unpin: with no capability at all, unpin then release evicted it. *)
Theorem pin_holds_without_pin_capability : forall caps h h',
  Steps caps h h' -> forall id s,
  h id = Some s -> s_live s = true -> s_pin s = Pinned ->
  (forall tok, caps tok = true -> verify_token tok id (s_gen s) ModePin = false) ->
  exists s', h' id = Some s' /\ s_live s' = true /\ s_pin s' = Pinned /\ s_gen s' = s_gen s.
Proof.
  intros caps h h' Hs. induction Hs as [h0|h0 h1 h2 Hstep Hrest IH]; intros id s Hid Hlive Hpin Hno.
  - exists s. repeat split; assumption.
  - destruct (pin_step caps h0 h1 id s Hstep Hid Hlive Hpin Hno) as [s1 [E1 [L1 [P1 G1]]]].
    rewrite <- G1 in Hno. destruct (IH id s1 E1 L1 P1 Hno) as [s2 [E2 [L2 [P2 G2]]]].
    exists s2. split; [exact E2|]. split; [exact L2|]. split; [exact P2|]. congruence.
Qed.

(* Releasing a slot makes every handle of its generation stale. *)
Theorem release_makes_stale : forall h caps id s tok h' handle,
  h id = Some s -> s_live s = true -> caps tok = true ->
  verify_token tok id (s_gen s) ModeRelease = true -> s_pin s = Unpinned ->
  h' = update_heap h id (Some (mkSlot 0 (s_gen s) Unpinned false)) ->
  h_slot handle = id -> h_gen handle = s_gen s ->
  Step h caps h' /\ HandleStale h' handle.
Proof.
  intros h caps id s tok h' handle H_s H_live H_cap H_verify H_unpinned H_heap H_slot H_gen.
  split; [eapply Step_Release; eassumption|].
  subst h'. exists (mkSlot 0 (s_gen s) Unpinned false). unfold update_heap.
  rewrite H_slot, Nat.eqb_refl. split; [reflexivity|]. right. simpl. split; [exact H_gen| reflexivity].
Qed.

(* Counterexample for the earlier rule, which emptied a released slot and
   claimed every empty id at generation 1: the handle of the previous
   occupant reads the next one, with the very token that admitted it
   before. *)
Theorem gen_one_reclaim_resurrects : forall h caps id tok,
  HandleRead h caps (mkHandle id 1) tok ->
  HandleRead (update_heap (update_heap h id None) id (Some (mkSlot 0 1 Unpinned true)))
             caps (mkHandle id 1) tok.
Proof.
  intros h caps id tok [s [_ [_ [_ [H_cap H_verify]]]]].
  exists (mkSlot 0 1 Unpinned true). unfold update_heap. simpl. rewrite Nat.eqb_refl.
  split; [reflexivity|]. split; [reflexivity|]. split; [reflexivity|]. split; assumption.
Qed.

(* Under the reuse rule the same sequence refuses the old handle. *)
Theorem reclaim_refuses_previous_handle : forall h caps id s tok tok' h1 h2,
  h id = Some s -> s_live s = true -> s_gen s = 1 -> caps tok = true ->
  verify_token tok id 1 ModeRelease = true -> s_pin s = Unpinned ->
  h1 = update_heap h id (Some (mkSlot 0 1 Unpinned false)) ->
  caps tok' = true -> verify_token tok' id 2 ModeClaim = true ->
  h2 = update_heap h1 id (Some (mkSlot 0 2 Unpinned true)) ->
  Steps caps h h2 /\ forall t, ~ HandleRead h2 caps (mkHandle id 1) t.
Proof.
  intros h caps id s tok tok' h1 h2 H_s H_live H_g H_cap H_verify H_unpinned H1 H_cap' H_verify' H2.
  assert (S1 : Step h caps h1) by (subst h1; eapply Step_Release; [exact H_s| exact H_live| exact H_cap|
    rewrite H_g; exact H_verify| exact H_unpinned| rewrite H_g; reflexivity]).
  assert (H1id : h1 id = Some (mkSlot 0 1 Unpinned false))
    by (subst h1; unfold update_heap; rewrite Nat.eqb_refl; reflexivity).
  assert (S2 : Step h1 caps h2) by (subst h2; eapply Step_Reclaim; [exact H1id| reflexivity| exact H_cap'|
    exact H_verify'| reflexivity]).
  split; [eapply Steps_step; [exact S1| eapply Steps_step; [exact S2| apply Steps_refl]]|].
  intros t. eapply stale_handle_read_impossible.
  - simpl. subst h2. unfold update_heap. rewrite Nat.eqb_refl. reflexivity.
  - simpl. discriminate.
Qed.

(* End of Pergyra Slot Capability Calculus sketch. *)
