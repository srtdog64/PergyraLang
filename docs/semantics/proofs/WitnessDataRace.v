(*
  WitnessDataRace.v  --  data-race-freedom invariant (Witness model).

  Companion to docs/semantics/10_ability_witness_evidence.md.

  Thesis (the construction we trade for Rust's borrow checker): access to a slot
  across concurrent contexts obeys aliasing-xor-mutability -- a slot is EITHER
  written by a single context (with no other-context access) OR read by many. A
  Witness is the compiler-internal proof a context's access conforms. "Every
  concurrency boundary carries a Witness" denotes the invariant [xor_mut] below;
  this file proves that invariant rules out data races -- BOTH write-write and
  read-write -- by construction, and that the permitted boundary steps preserve
  it. Preservation is the obligation Pergyra's boundary typing discharges
  (docs/semantics/10 §7 maps move/consume + pin/view exclusivity + atomic-shared
  + cannot-cross to these steps).

  A data race (the real UB class) = two DISTINCT contexts concurrently access the
  same slot with AT LEAST ONE write. This file models read AND write capabilities
  so both write-write and read-write races are ruled out.
*)

Require Import Coq.Lists.List.
Require Import Coq.Arith.PeanoNat.
Import ListNotations.

Definition Slot    := nat.
Definition Context := nat.

Inductive Mode := Rd | Wr.

(* A configuration = the capabilities held: (context, slot, mode). *)
Definition Cap    := (Context * Slot * Mode)%type.
Definition Config := list Cap.

Definition holds (g : Config) (c : Context) (s : Slot) (m : Mode) : Prop :=
  In (c, s, m) g.

Definition writes   (g : Config) (c : Context) (s : Slot) : Prop := holds g c s Wr.
Definition accesses (g : Config) (c : Context) (s : Slot) : Prop :=
  exists m, holds g c s m.

(* Aliasing-xor-mutability invariant: a write-cap on a slot excludes any
   other-context access to it. (Many readers coexist; a writer is alone.) This
   is the Witness invariant for concurrent access. *)
Definition xor_mut (g : Config) : Prop :=
  forall c1 c2 s, writes g c1 s -> accesses g c2 s -> c1 = c2.

(* A data race: distinct contexts access the same slot, at least one writing.
   (Existential is symmetric in read/write: relabel so the writer is c1.) *)
Definition data_race (g : Config) : Prop :=
  exists c1 c2 s, c1 <> c2 /\ writes g c1 s /\ accesses g c2 s.

(* --------------------------------------------------------------------- *)
(* Theorem 1: the invariant rules out data races by construction          *)
(* (write-write AND read-write).                                          *)
(* --------------------------------------------------------------------- *)
Theorem xor_mut_no_data_race :
  forall g, xor_mut g -> ~ data_race g.
Proof.
  intros g Hxm [c1 [c2 [s [Hne [Hw Ha]]]]].
  apply Hne. exact (Hxm c1 c2 s Hw Ha).
Qed.

(* --------------------------------------------------------------------- *)
(* Permitted boundary steps. None creates a conflicting concurrent access. *)
(* --------------------------------------------------------------------- *)

(* Clear every capability for slot [s] (release the slot). *)
Definition clear_slot (g : Config) (s : Slot) : Config :=
  filter (fun p => negb (Nat.eqb (snd (fst p)) s)) g.

Lemma holds_clear_slot : forall g s c s' m,
  holds (clear_slot g s) c s' m <-> (holds g c s' m /\ s' <> s).
Proof.
  intros g s c s' m. unfold holds, clear_slot. split.
  - intro Hin. apply filter_In in Hin. destruct Hin as [Hin Hneq].
    simpl in Hneq. split. exact Hin.
    intro Heq. subst s'. rewrite Nat.eqb_refl in Hneq. discriminate.
  - intros [Hin Hneq]. apply filter_In. split. exact Hin.
    simpl. apply Bool.negb_true_iff. apply Nat.eqb_neq. exact Hneq.
Qed.

Lemma accesses_clear_slot : forall g s c s',
  accesses (clear_slot g s) c s' -> (accesses g c s' /\ s' <> s).
Proof.
  intros g s c s' [m Hm]. apply holds_clear_slot in Hm.
  destruct Hm as [Hm Hneq]. split. exists m; exact Hm. exact Hneq.
Qed.

Inductive step : Config -> Config -> Prop :=
  (* ACQUIRE-WRITE: only on a slot with no current access (exclusive). *)
  | step_acq_write : forall g c s,
      (forall c' m, ~ holds g c' s m) ->
      step g ((c, s, Wr) :: g)
  (* ACQUIRE-READ: only on a slot with no current writer (readers coexist). *)
  | step_acq_read : forall g c s,
      (forall c', ~ writes g c' s) ->
      step g ((c, s, Rd) :: g)
  (* RELEASE: clear a slot's capabilities. *)
  | step_release : forall g s,
      step g (clear_slot g s).

(* --------------------------------------------------------------------- *)
(* Theorem 2 (Preservation): every permitted step preserves xor_mut.      *)
(* --------------------------------------------------------------------- *)
Theorem xor_mut_preserved :
  forall g g', xor_mut g -> step g g' -> xor_mut g'.
Proof.
  intros g g' Hxm Hstep.
  destruct Hstep as [g c s Hfree | g c s Hnowriter | g s].
  - (* acquire-write: precond -- s had no access at all *)
    intros c1 c2 s0 Hw Ha.
    unfold writes, holds in Hw. simpl in Hw.
    destruct Ha as [m2 Ha]. unfold holds in Ha. simpl in Ha.
    destruct Hw as [E1 | Hw]; destruct Ha as [E2 | Ha].
    + inversion E1; inversion E2; subst; reflexivity.
    + inversion E1; subst c s0. exfalso. apply (Hfree c2 m2). exact Ha.
    + inversion E2; subst c s0. exfalso. apply (Hfree c1 Wr). exact Hw.
    + exact (Hxm c1 c2 s0 Hw (ex_intro _ m2 Ha)).
  - (* acquire-read: precond -- s had no writer; we add only a reader *)
    intros c1 c2 s0 Hw Ha.
    unfold writes, holds in Hw. simpl in Hw.
    destruct Ha as [m2 Ha]. unfold holds in Ha. simpl in Ha.
    destruct Hw as [E1 | Hw].
    + inversion E1. (* (c,s0,Wr) = (c,s,Rd) is impossible: Wr <> Rd *)
    + destruct Ha as [E2 | Ha].
      * inversion E2; subst c s0.
        exfalso. apply (Hnowriter c1). exact Hw.
      * exact (Hxm c1 c2 s0 Hw (ex_intro _ m2 Ha)).
  - (* release: caps only shrink *)
    intros c1 c2 s0 Hw Ha.
    unfold writes in Hw. apply holds_clear_slot in Hw. destruct Hw as [Hw _].
    apply accesses_clear_slot in Ha. destruct Ha as [Ha _].
    exact (Hxm c1 c2 s0 Hw Ha).
Qed.

(* --------------------------------------------------------------------- *)
(* Corollary: a well-typed run starting xor_mut is data-race-free at every *)
(* reachable state.                                                        *)
(* --------------------------------------------------------------------- *)
Inductive steps : Config -> Config -> Prop :=
  | steps_refl  : forall g, steps g g
  | steps_trans : forall g g' g'', step g g' -> steps g' g'' -> steps g g''.

Theorem run_data_race_free :
  forall g g', xor_mut g -> steps g g' -> ~ data_race g'.
Proof.
  intros g g' Hxm Hrun.
  apply xor_mut_no_data_race.
  induction Hrun as [g | g g' g'' Hstep Hrun IH].
  - exact Hxm.
  - apply IH. exact (xor_mut_preserved g g' Hxm Hstep).
Qed.

(* Sanity: many readers coexist (no over-restriction). With two readers of the
   same slot from distinct contexts, there is no data race. *)
Example readers_share_ok :
  forall c1 c2 s, ~ data_race [(c1, s, Rd); (c2, s, Rd)].
Proof.
  intros c1 c2 s [d1 [d2 [s0 [Hne [Hw _]]]]].
  unfold writes, holds in Hw. simpl in Hw.
  destruct Hw as [E1 | [E2 | F]].
  - inversion E1.
  - inversion E2.
  - exact F.
Qed.

(* ===================================================================== *)
(* (a) Bridge: pin/view exclusivity (SlotCalculus ModePin) => xor_mut.    *)
(*                                                                        *)
(* SlotCalculus.v models a single context's token validity: a ModePin /  *)
(* Pinned slot, kept stable by the Pin Non-Eviction Lemma, gives a        *)
(* context a stable EXCLUSIVE capability. In the concurrent setting that  *)
(* exclusive capability is a write-cap held ALONE on its slot. The §7     *)
(* refinement audit found exactly this guard in the implementation        *)
(* ("Cannot write slot while a view/pin is live", PIN_PARALLEL_CONFLICT). *)
(* Here we discharge that audit as a theorem: the pin-exclusivity         *)
(* discipline entails the data-race invariant.                            *)
(* ===================================================================== *)
Definition pin_exclusive (g : Config) : Prop :=
  forall c s, writes g c s -> forall c' m, holds g c' s m -> c' = c.

Theorem pin_exclusive_xor_mut :
  forall g, pin_exclusive g -> xor_mut g.
Proof.
  intros g Hpe c1 c2 s Hw [m2 Ha].
  symmetry. exact (Hpe c1 s Hw c2 m2 Ha).
Qed.

Corollary pin_exclusive_no_data_race :
  forall g, pin_exclusive g -> ~ data_race g.
Proof.
  intros g Hpe. apply xor_mut_no_data_race. apply pin_exclusive_xor_mut. exact Hpe.
Qed.

(* ===================================================================== *)
(* (b) Typed boundary calculus: well-typed => data-race-free.             *)
(*                                                                        *)
(* A boundary program is a sequence of boundary ops (the model of         *)
(* spawn/channel-send/borrow/release). Each op has a GUARD -- the         *)
(* precondition the boundary type-checker must enforce. We prove every    *)
(* guarded op steps in the WitnessDataRace model, so a well-typed program *)
(* preserves xor_mut and is data-race-free. This is the soundness of the  *)
(* boundary TYPING DISCIPLINE; that the C checker actually enforces these *)
(* guards is the remaining refinement obligation (docs/semantics/10 §7,   *)
(* the RustBelt-vs-rustc gap), not provable here.                         *)
(* ===================================================================== *)
Inductive Op :=
  | OpAcqW : Context -> Slot -> Op   (* exclusive acquire: spawn-into / claim  *)
  | OpAcqR : Context -> Slot -> Op   (* shared read acquire                    *)
  | OpRel  : Slot -> Op.             (* release / move-out / drop              *)

Definition op_guard (g : Config) (o : Op) : Prop :=
  match o with
  | OpAcqW _ s => forall c' m, ~ holds g c' s m   (* no current access at all *)
  | OpAcqR _ s => forall c', ~ writes g c' s        (* no current writer       *)
  | OpRel  _   => True
  end.

Definition op_apply (g : Config) (o : Op) : Config :=
  match o with
  | OpAcqW c s => (c, s, Wr) :: g
  | OpAcqR c s => (c, s, Rd) :: g
  | OpRel  s   => clear_slot g s
  end.

Lemma op_step : forall g o, op_guard g o -> step g (op_apply g o).
Proof.
  intros g [c s | c s | s] Hg; simpl in *.
  - apply step_acq_write. exact Hg.
  - apply step_acq_read. exact Hg.
  - apply step_release.
Qed.

Fixpoint run_prog (g : Config) (p : list Op) : Config :=
  match p with
  | []      => g
  | o :: p' => run_prog (op_apply g o) p'
  end.

Inductive well_typed : Config -> list Op -> Prop :=
  | wt_nil  : forall g, well_typed g []
  | wt_cons : forall g o p,
      op_guard g o -> well_typed (op_apply g o) p -> well_typed g (o :: p).

Theorem well_typed_data_race_free :
  forall g p, xor_mut g -> well_typed g p -> ~ data_race (run_prog g p).
Proof.
  intros g p Hxm Hwt. apply xor_mut_no_data_race.
  revert g Hxm Hwt. induction p as [| o p IH]; intros g Hxm Hwt.
  - simpl. exact Hxm.
  - simpl. inversion Hwt; subst. apply IH.
    + apply (xor_mut_preserved g (op_apply g o) Hxm).
      apply op_step. assumption.
    + assumption.
Qed.
