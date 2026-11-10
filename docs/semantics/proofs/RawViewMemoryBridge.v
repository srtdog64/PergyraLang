(* Checked scalar memory execution over RawPointerOwnershipTrace.  Addresses,
   allocation epochs, owner issuance, and release authority remain separate.
   The compiler importer supplies MIR projections; it is not assumed correct.
   Concrete projected programs are reduced in the kernel and tested against
   emitted C/LLVM. Foreign writes and arbitrary public pointers are excluded. *)
Require Import Stdlib.Arith.PeanoNat Stdlib.Bool.Bool Stdlib.Lists.List.
Require Import Stdlib.ZArith.ZArith Stdlib.micromega.Lia.
Require Import RawPointerOwnershipClean RawPointerOwnershipTrace OwnershipCleanCore.
Import ListNotations.

Fixpoint cell_put (xs : list Z) (index : nat) (value : Z) : list Z :=
  match xs, index with
  | [], _ => []
  | _ :: rest, 0 => value :: rest
  | x :: rest, S i => x :: cell_put rest i value
  end.

Lemma cell_put_length : forall xs i v,
  length (cell_put xs i v) = length xs.
Proof. induction xs; intros [|i] v; simpl; auto. Qed.

Lemma cell_put_read_same : forall xs i v,
  i < length xs -> nth_error (cell_put xs i v) i = Some v.
Proof.
  induction xs; intros [|i] v H; simpl in *; try lia; auto.
  apply IHxs; lia.
Qed.

Lemma cell_put_read_other : forall xs i j v,
  i <> j -> nth_error (cell_put xs i v) j = nth_error xs j.
Proof.
  induction xs; intros [|i] [|j] v H; simpl; auto; try congruence.
Qed.

Record MemoryRange := mkMemoryRange { range_start : nat; range_length : nat }.
Definition RangeStore := nat -> option MemoryRange.
Definition range_put (rs : RangeStore) x span : RangeStore :=
  fun y => if Nat.eqb x y then Some span else rs y.

Record MemoryMachine := mkMemoryMachine {
  memory_raw : RawMachine;
  memory_cells : list Z;
  memory_ranges : RangeStore
}.

Definition initial_memory a e owner cells : MemoryMachine :=
  mkMemoryMachine (initial_raw_machine a e owner) cells
    (range_put (fun _ => None) owner (mkMemoryRange 0 (length cells))).

Definition locate_cell (m : MemoryMachine) binding index : option nat :=
  match rm_store (memory_raw m) binding, memory_ranges m binding with
  | Some p, Some span =>
      if managed_raw_access (rm_region (memory_raw m)) p &&
         Nat.ltb index (range_length span) &&
         Nat.leb (range_start span + range_length span) (length (memory_cells m))
      then Some (range_start span + index) else None
  | _, _ => None
  end.

Theorem located_cell_is_in_bounds : forall m binding index physical,
  locate_cell m binding index = Some physical ->
  physical < length (memory_cells m).
Proof.
  intros m b i p H. unfold locate_cell in H.
  destruct (rm_store (memory_raw m) b); try discriminate.
  destruct (memory_ranges m b) as [[start count]|]; try discriminate.
  destruct (_ && _ && _) eqn:E; try discriminate. inversion H; subst.
  apply andb_true_iff in E as [E Hb]. apply andb_true_iff in E as [_ Hi].
  apply Nat.ltb_lt in Hi. apply Nat.leb_le in Hb. simpl in *; lia.
Qed.

Inductive MemoryExpr :=
| MConst (value : Z)
| MLoad (binding index : nat)
| MAdd (left right : MemoryExpr)
| MSub (left right : MemoryExpr).

Fixpoint memory_eval (m : MemoryMachine) (expr : MemoryExpr) : option Z :=
  match expr with
  | MConst v => Some v
  | MLoad binding index =>
      match locate_cell m binding index with
      | Some physical => nth_error (memory_cells m) physical | None => None end
  | MAdd l r => match memory_eval m l, memory_eval m r with
                | Some x, Some y => Some (x + y)%Z | _, _ => None end
  | MSub l r => match memory_eval m l, memory_eval m r with
                | Some x, Some y => Some (x - y)%Z | _, _ => None end
  end.

Inductive MemoryInstruction :=
| MControl (instruction : RawInstruction)
| MSlice (source destination start count : nat)
| MStore (binding index : nat) (value : MemoryExpr).

Definition memory_step (m : MemoryMachine) i : option MemoryMachine :=
  match i with
  | MControl control =>
      match raw_step (memory_raw m) control with
      | Some after => Some (mkMemoryMachine after (memory_cells m) (memory_ranges m))
      | None => None end
  | MSlice source destination start count =>
      match memory_ranges m source with
      | Some span =>
          if Nat.leb (start + count) (range_length span) then
            match raw_step (memory_raw m) (RIBorrow source destination) with
            | Some after => Some (mkMemoryMachine after (memory_cells m)
                (range_put (memory_ranges m) destination
                  (mkMemoryRange (range_start span + start) count)))
            | None => None end
          else None
      | None => None end
  | MStore binding index expr =>
      match locate_cell m binding index, memory_eval m expr with
      | Some physical, Some value => Some (mkMemoryMachine (memory_raw m)
          (cell_put (memory_cells m) physical value) (memory_ranges m))
      | _, _ => None end
  end.

Fixpoint run_memory code m : option MemoryMachine :=
  match code with
  | [] => Some m
  | i :: rest => match memory_step m i with
                | Some after => run_memory rest after | None => None end
  end.

Theorem memory_step_preserves_owner_and_extent : forall m i after,
  RawLive (memory_raw m) -> memory_step m i = Some after ->
  RawLive (memory_raw after) /\ length (memory_cells after) = length (memory_cells m).
Proof.
  intros m [control|src dst start count|binding index expr] after Hl E;
    cbn [memory_step] in E.
  - destruct (raw_step _ _) as [raw|] eqn:Hr; try discriminate.
    inversion E; subst. split; [eapply raw_step_preserves_affine_owner; eauto|reflexivity].
  - destruct (memory_ranges m src); try discriminate.
    destruct (Nat.leb _ _); try discriminate.
    destruct (raw_step _ _) as [raw|] eqn:Hr; try discriminate.
    inversion E; subst. split; [eapply raw_step_preserves_affine_owner; eauto|reflexivity].
  - destruct (locate_cell _ _ _); try discriminate.
    destruct (memory_eval _ _); try discriminate. inversion E; subst.
    split; [exact Hl|apply cell_put_length].
Qed.

Theorem memory_trace_preserves_owner_and_extent : forall code m after,
  RawLive (memory_raw m) -> run_memory code m = Some after ->
  RawLive (memory_raw after) /\ length (memory_cells after) = length (memory_cells m).
Proof.
  induction code; intros m after Hl E; simpl in E.
  - inversion E; subst; auto.
  - destruct (memory_step m a) as [mid|] eqn:Hs; try discriminate.
    destruct (memory_step_preserves_owner_and_extent _ _ _ Hl Hs) as [Hlive Hlen].
    destruct (IHcode _ _ Hlive E) as [Hlive' Hlen']. split; [exact Hlive'|congruence].
Qed.

(* A store changes one physical cell. Every other alias reads the same
   backing; neither the borrow nor a write can create a release authority. *)
Theorem store_updates_one_cell : forall m binding index expr after physical value,
  locate_cell m binding index = Some physical -> memory_eval m expr = Some value ->
  memory_step m (MStore binding index expr) = Some after ->
  nth_error (memory_cells after) physical = Some value /\
  (forall other, other <> physical ->
    nth_error (memory_cells after) other = nth_error (memory_cells m) other) /\
  memory_raw after = memory_raw m.
Proof.
  intros m b i expr after p v Hp Hv E. cbn [memory_step] in E.
  rewrite Hp, Hv in E. inversion E; subst; simpl.
  split; [apply cell_put_read_same; eapply located_cell_is_in_bounds; eauto|].
  split; [intros; apply cell_put_read_other; congruence|reflexivity].
Qed.

Theorem store_preserves_all_locations : forall m binding index expr after,
  memory_step m (MStore binding index expr) = Some after ->
  forall alias offset, locate_cell after alias offset = locate_cell m alias offset.
Proof.
  intros m b i expr after E alias offset. cbn [memory_step] in E.
  destruct (locate_cell m b i); try discriminate.
  destruct (memory_eval m expr); try discriminate. inversion E; subst.
  unfold locate_cell; simpl. rewrite cell_put_length. reflexivity.
Qed.

Theorem store_is_visible_through_every_same_cell_alias :
  forall m binding index expr after physical value alias offset,
  locate_cell m binding index = Some physical -> memory_eval m expr = Some value ->
  memory_step m (MStore binding index expr) = Some after ->
  locate_cell m alias offset = Some physical ->
  memory_eval after (MLoad alias offset) = Some value.
Proof.
  intros m b i expr after p v alias offset Hp Hv E Ha.
  cbn [memory_eval]. rewrite (store_preserves_all_locations _ _ _ _ _ E), Ha.
  exact (proj1 (store_updates_one_cell _ _ _ _ _ _ _ Hp Hv E)).
Qed.

Definition memory_erase_instruction i : RawInstruction :=
  match i with
  | MControl control => control
  | MSlice src dst _ _ => RIBorrow src dst
  | MStore binding _ _ => RIRawAccess binding
  end.

Lemma located_cell_has_raw_authority : forall m binding index physical,
  locate_cell m binding index = Some physical ->
  raw_step (memory_raw m) (RIRawAccess binding) = Some (memory_raw m).
Proof.
  intros m b i p H. unfold locate_cell in H.
  destruct (rm_store (memory_raw m) b) as [pointer|] eqn:Ep; try discriminate.
  destruct (memory_ranges m b); try discriminate.
  destruct (_ && _ && _) eqn:E; try discriminate.
  apply andb_true_iff in E as [E _]. apply andb_true_iff in E as [Ha _].
  cbn [raw_step]. rewrite Ep, Ha. reflexivity.
Qed.

Theorem memory_step_erases_to_checked_raw_step : forall m i after,
  memory_step m i = Some after ->
  raw_step (memory_raw m) (memory_erase_instruction i) = Some (memory_raw after).
Proof.
  intros m [control|src dst start count|binding index expr] after E;
    cbn [memory_step memory_erase_instruction] in *.
  - destruct (raw_step _ _) eqn:Hr; try discriminate. inversion E; subst; reflexivity.
  - destruct (memory_ranges m src); try discriminate.
    destruct (Nat.leb _ _); try discriminate.
    destruct (raw_step _ _) eqn:Hr; try discriminate. inversion E; subst; reflexivity.
  - destruct (locate_cell m binding index) eqn:Hp; try discriminate.
    destruct (memory_eval _ _) eqn:Hv; try discriminate. inversion E; subst; simpl.
    eapply located_cell_has_raw_authority; eauto.
Qed.

Theorem memory_trace_erases_to_checked_raw_trace : forall code m after,
  run_memory code m = Some after ->
  run_raw_body (map memory_erase_instruction code) (memory_raw m) = Some (memory_raw after).
Proof.
  induction code; intros m after E; simpl in *.
  - inversion E; subst; reflexivity.
  - destruct (memory_step m a) as [mid|] eqn:Hs; try discriminate.
    rewrite (memory_step_erases_to_checked_raw_step _ _ _ Hs).
    apply IHcode; exact E.
Qed.

Theorem checked_memory_program_frees_canonical_heap :
  forall a e owner cells code after kind tf tp c,
  run_memory code (initial_memory a e owner cells) = Some after ->
  exists retired final_holder,
    automatic_raw_exit kind (memory_raw after) = Some retired /\ RawRetired retired /\
    texec tf tp [(Tmp owner, (c, alloc e (vsize c)))] [] (alloc e (vsize c)) (S e)
      (TSeq (raw_core_body (map memory_erase_instruction code))
        (TDrop (Tmp final_holder))) [] [] [] (S e) [].
Proof.
  intros a e owner cells code after kind tf tp c E.
  apply memory_trace_erases_to_checked_raw_trace in E.
  exact (closed_raw_program_frees_canonical_heap a e owner c
    (map memory_erase_instruction code) (memory_raw after) kind tf tp E).
Qed.

Theorem checked_memory_trace_has_one_release : forall a e owner cells code after kind,
  run_memory code (initial_memory a e owner cells) = Some after ->
  exists retired,
    automatic_raw_exit kind (memory_raw after) = Some retired /\
    RawRetired retired /\
    (forall p, managed_raw_access (rm_region retired) p = false) /\
    automatic_raw_exit kind retired = None.
Proof.
  intros a e owner cells code after kind E.
  destruct (memory_trace_preserves_owner_and_extent code (initial_memory a e owner cells) after
    (allocation_issues_one_owner a e owner) E) as [Hl _].
  exists (mkRawMachine (released_region (rm_region (memory_raw after))) pointer_empty).
  split; [apply automatic_exit_is_total; exact Hl|].
  pose proof (released_machine_is_retired (rm_region (memory_raw after))) as Hr.
  split; [exact Hr|]. split.
  - intros; apply retired_machine_refuses_access; exact Hr.
  - apply retired_machine_refuses_second_exit; exact Hr.
Qed.
