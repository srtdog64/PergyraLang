(* 
  Pergyra Formal Semantics - Mechanized Sketch
  Target: Slot Capability Calculus & Pin Non-Eviction Lemma
  Status: proof-sketch; not beta-closure evidence unless checked by CI
  Scope: this file models one small-step invariant, not the full language.
*)

Require Import Coq.Init.Nat.
Require Import Coq.Arith.PeanoNat.
Require Import Coq.Bool.Bool.

(* ========================================== *)
(* 1. Domains & State Definition              *)
(* ========================================== *)

Definition SlotId := nat.
Definition Generation := nat.
Definition Token := nat. (* Abstract Cryptographic Token *)
Definition Value := nat.

Inductive AccessMode : Type :=
  | ModeRead : AccessMode
  | ModeWrite : AccessMode
  | ModeRelease : AccessMode
  | ModePin : AccessMode.

Inductive PinState : Type :=
  | Unpinned : PinState
  | Pinned : PinState.

Record Slot : Type := mkSlot {
  s_val : Value;
  s_gen : Generation;
  s_pin : PinState
}.

Record Handle : Type := mkHandle {
  h_slot : SlotId;
  h_gen : Generation
}.

(* 
  Heap represents the global memory state (\Sigma).
  Maps a SlotId to an optional Slot. None means unallocated or released.
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

(* ========================================== *)
(* 2. Operational Semantics (State Transitions)*)
(* ========================================== *)

(* 
  The 'Step' inductive relation defines the legal state transitions 
  in the Pergyra Memory Model.
*)
Inductive Step (h: Heap) (caps: CapEnv) : Heap -> Prop :=
  
  (* Rule 1: Read (Zero-state-change, requires Token verification) *)
  | Step_Read : forall id s tok,
      h id = Some s ->
      caps tok = true ->
      verify_token tok id (s_gen s) ModeRead = true ->
      Step h caps h

  (* Rule 2: Write (state change, requires write capability) *)
  | Step_Write : forall id s tok value h',
      h id = Some s ->
      caps tok = true ->
      verify_token tok id (s_gen s) ModeWrite = true ->
      h' = update_heap h id (Some (mkSlot value (s_gen s) (s_pin s))) ->
      Step h caps h'

  (* Rule 3: Pin (Lease capability, locks the slot in memory) *)
  | Step_Pin : forall id s tok h',
      h id = Some s ->
      caps tok = true ->
      verify_token tok id (s_gen s) ModePin = true ->
      s_pin s = Unpinned ->
      h' = update_heap h id (Some (mkSlot (s_val s) (s_gen s) Pinned)) ->
      Step h caps h'

  (* Rule 4: Unpin (Release lease) *)
  | Step_Unpin : forall id s h',
      h id = Some s ->
      s_pin s = Pinned ->
      h' = update_heap h id (Some (mkSlot (s_val s) (s_gen s) Unpinned)) ->
      Step h caps h'

  (* Rule 5: Release (Evict from heap. STRICTLY requires Unpinned state) *)
  | Step_Release : forall id s tok h',
      h id = Some s ->
      caps tok = true ->
      verify_token tok id (s_gen s) ModeRelease = true ->
      s_pin s = Unpinned ->
      h' = update_heap h id None ->
      Step h caps h'.

Definition HandleRead (h: Heap) (caps: CapEnv) (handle: Handle) (tok: Token) : Prop :=
  exists s,
    h (h_slot handle) = Some s /\
    h_gen handle = s_gen s /\
    caps tok = true /\
    verify_token tok (h_slot handle) (h_gen handle) ModeRead = true.

Definition HandleWrite (h: Heap) (caps: CapEnv) (handle: Handle) (tok: Token) : Prop :=
  exists s,
    h (h_slot handle) = Some s /\
    h_gen handle = s_gen s /\
    caps tok = true /\
    verify_token tok (h_slot handle) (h_gen handle) ModeWrite = true.

Definition HandlePin (h: Heap) (caps: CapEnv) (handle: Handle) (tok: Token) : Prop :=
  exists s,
    h (h_slot handle) = Some s /\
    h_gen handle = s_gen s /\
    s_pin s = Unpinned /\
    caps tok = true /\
    verify_token tok (h_slot handle) (h_gen handle) ModePin = true.

Definition HandleRelease (h: Heap) (caps: CapEnv) (handle: Handle) (tok: Token) : Prop :=
  exists s,
    h (h_slot handle) = Some s /\
    h_gen handle = s_gen s /\
    s_pin s = Unpinned /\
    caps tok = true /\
    verify_token tok (h_slot handle) (h_gen handle) ModeRelease = true.

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
  destruct H_read as [read_slot [H_read_slot [H_read_gen [_ H_verify]]]].
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
  destruct H_write as [write_slot [H_write_slot [H_write_gen [_ H_verify]]]].
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
  destruct H_release as [release_slot [H_release_slot [H_release_gen [_ [_ H_verify]]]]].
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
  destruct H_read as [_ [_ [_ [H_cap _]]]].
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
  destruct H_write as [_ [_ [_ [H_cap _]]]].
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
  destruct H_pin as [_ [_ [_ [_ [H_cap _]]]]].
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
  destruct H_release as [_ [_ [_ [_ [H_cap _]]]]].
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

Lemma pinned_handle_release_impossible : forall h caps handle s tok,
  h (h_slot handle) = Some s ->
  s_pin s = Pinned ->
  ~ HandleRelease h caps handle tok.
Proof.
  intros h caps handle s tok H_slot H_pinned H_release.
  unfold HandleRelease in H_release.
  destruct H_release as [release_slot [H_release_slot [_ [H_unpinned [_ _]]]]].
  rewrite H_slot in H_release_slot.
  inversion H_release_slot; subst.
  rewrite H_pinned in H_unpinned.
  discriminate.
Qed.

(*
  Lemma: Pin Non-Eviction
  Proof Obligation: "A Pinned slot cannot be released or evicted by any Step rule."
*)
Lemma pin_non_eviction : forall h caps id s h',
  h id = Some s ->
  s_pin s = Pinned ->
  Step h caps h' ->
  h' id <> None.
Proof.
  intros h caps pinned_id pinned_slot next_heap H_some H_pinned H_step H_none.
  destruct H_step as
    [ read_id read_slot tok H_read H_cap H_verify
    | write_id write_slot tok value write_heap H_write H_cap H_verify H_heap
    | pin_id pin_slot tok pin_heap H_pin H_cap H_verify H_unpinned H_heap
    | unpin_id unpin_slot unpin_heap H_unpin H_was_pinned H_heap
    | release_id release_slot tok release_heap H_release H_cap H_verify H_unpinned H_heap
    ]; subst.

  - (* Step_Read: the heap is unchanged. *)
    rewrite H_some in H_none. discriminate.

  - (* Step_Write: updates either this slot to Some with a new value or another
       slot. *)
    unfold update_heap in H_none.
    destruct (Nat.eqb pinned_id write_id) eqn:E_eq.
    + discriminate.
    + rewrite H_some in H_none. discriminate.

  - (* Step_Pin: updates either this slot to Some Pinned or another slot. *)
    unfold update_heap in H_none.
    destruct (Nat.eqb pinned_id pin_id) eqn:E_eq.
    + discriminate.
    + rewrite H_some in H_none. discriminate.

  - (* Step_Unpin: updates either this slot to Some Unpinned or another slot. *)
    unfold update_heap in H_none.
    destruct (Nat.eqb pinned_id unpin_id) eqn:E_eq.
    + discriminate.
    + rewrite H_some in H_none. discriminate.

  - (* Step_Release: a release of this pinned slot is impossible; another slot
       release preserves this slot. *)
    unfold update_heap in H_none.
    destruct (Nat.eqb pinned_id release_id) eqn:E_eq.
    + apply Nat.eqb_eq in E_eq. subst.
      rewrite H_some in H_release. inversion H_release; subst.
      rewrite H_pinned in H_unpinned. discriminate.
    + rewrite H_some in H_none. discriminate.
Qed.

(* End of Pergyra Slot Capability Calculus sketch. *)
