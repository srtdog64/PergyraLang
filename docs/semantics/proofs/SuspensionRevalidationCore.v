(*
  SuspensionRevalidationCore.v  --  slot temporal safety across a suspension:
  a reference that crosses `await` must be re-resolved, and a stale one never
  resolves to the slot's new occupant.

  Companion to docs/204 §3.3 (the checked suspension contract docs/107 left a
  place for), §2.6 and §5 theorem 3 (Slot Temporal Safety), and the runtime
  shapes it models:

    SlotHandle   { slotId; typeTag; generation }   src/runtime/slot_manager.h
    PgyPinnedView{ slotId; generation; mode; valid }

  SlotCalculus.v proves stale-handle access impossible for ONE context. This
  file is the concurrent half: while a task is suspended, other tasks may
  despawn the entity and reuse its slot. What may cross the suspension is a
  SlotRef -- slot id plus the generation the task observed -- and the task
  must RESOLVE it on resume. A resolved reference names the same incarnation
  the task saw; a reference whose incarnation is gone resolves to nothing.

  The model. A world is a generation counter and a liveness bit per slot.
  Despawning a live slot bumps its generation and clears liveness; respawning
  a dead slot makes it live again at the bumped generation. Any interleaving
  of those is what an awaiting task cannot see.

  What is proved.
    [gen_monotone]           generations never go backwards.
    [resolve_sound]          a resolved reference names the current incarnation.
    [stale_never_resolves]   after ANY run containing a despawn of the slot,
                             the old reference resolves to nothing -- even
                             when the slot is live again with a new occupant.
    [resolved_means_same_incarnation]  a reference that still resolves after
                             the run saw no despawn of its slot: the entity it
                             names is the one it was taken from.
    [revalidation_not_vacuous]  a run without a despawn keeps the ref alive.

  Refutation.
    [unchecked_deref_hits_new_occupant]  dereferencing by slot id alone --
                             what carrying a LiveRef across await amounts to --
                             touches a live slot whose occupant is not the one
                             the reference was taken from. docs/204 appendix
                             A.14's "stale async problem", machine-checked.

  Negative scope. The generation is modelled as an unbounded nat; the runtime's
  is uint32_t, so wrap-around after 2^32 reuses of one slot is outside this
  file. No claim about who bumps the counter or that the compiler inserts the
  resume-time resolve: that is the rung docs/204 §4 item 5 asks for.
*)

Require Import Coq.Arith.PeanoNat.
Require Import Coq.Bool.Bool.
Require Import Coq.micromega.Lia.

Definition SlotId := nat.
Definition Gen    := nat.

Record World := mkWorld {
  gen  : SlotId -> Gen;
  live : SlotId -> bool
}.

Definition upd {A : Type} (f : SlotId -> A) (s : SlotId) (v : A) : SlotId -> A :=
  fun s' => if Nat.eqb s' s then v else f s'.

(* What crosses a suspension: the slot and the generation the task saw. *)
Definition SlotRef := (SlotId * Gen)%type.

(* A reference is valid in a world when the slot is live at that generation. *)
Definition valid (w : World) (r : SlotRef) : Prop :=
  live w (fst r) = true /\ gen w (fst r) = snd r.

(* Resume-time resolution: the liveness check of a generational reference. *)
Definition resolve (w : World) (r : SlotRef) : option SlotRef :=
  if live w (fst r) && Nat.eqb (gen w (fst r)) (snd r) then Some r else None.

(* Dereferencing by slot id alone, ignoring the generation: what a LiveRef
   carried across await would do. *)
Definition deref_unchecked (w : World) (r : SlotRef) : bool := live w (fst r).

(* ===================================================================== *)
(* 1. What other tasks may do while one is suspended                      *)
(* ===================================================================== *)

Definition despawn (w : World) (s : SlotId) : World :=
  mkWorld (upd (gen w) s (S (gen w s))) (upd (live w) s false).

Definition respawn (w : World) (s : SlotId) : World :=
  mkWorld (gen w) (upd (live w) s true).

Inductive wstep : World -> World -> Prop :=
  | ws_despawn : forall w s, live w s = true  -> wstep w (despawn w s)
  | ws_respawn : forall w s, live w s = false -> wstep w (respawn w s).

Inductive wsteps : World -> World -> Prop :=
  | wsteps_refl  : forall w, wsteps w w
  | wsteps_trans : forall w w' w'', wstep w w' -> wsteps w' w'' -> wsteps w w''.

(* A run that contains a despawn of slot s somewhere along the way. *)
Inductive has_despawn (s : SlotId) : World -> World -> Prop :=
  | hd_here  : forall w w', live w s = true -> wsteps (despawn w s) w' ->
      has_despawn s w w'
  | hd_later : forall w w' w'', wstep w w' -> has_despawn s w' w'' ->
      has_despawn s w w''.

(* ===================================================================== *)
(* 2. Generations only move forward                                       *)
(* ===================================================================== *)

Lemma upd_same : forall A (f : SlotId -> A) s v, upd f s v s = v.
Proof. intros. unfold upd. rewrite Nat.eqb_refl. reflexivity. Qed.

Lemma upd_other : forall A (f : SlotId -> A) s v s', s' <> s -> upd f s v s' = f s'.
Proof.
  intros A f s v s' Hne. unfold upd.
  destruct (Nat.eqb s' s) eqn:E.
  - apply Nat.eqb_eq in E. contradiction.
  - reflexivity.
Qed.

Lemma step_gen_mono : forall w w' s, wstep w w' -> gen w s <= gen w' s.
Proof.
  intros w w' s Hs. destruct Hs as [w s0 Hl | w s0 Hl]; simpl.
  - destruct (Nat.eq_dec s s0) as [E | E].
    + subst. rewrite upd_same. lia.
    + rewrite upd_other by exact E. lia.
  - lia.
Qed.

Theorem gen_monotone : forall w w' s, wsteps w w' -> gen w s <= gen w' s.
Proof.
  intros w w' s Hrun. induction Hrun as [w | w w' w'' Hs Hrun IH].
  - lia.
  - pose proof (step_gen_mono w w' s Hs). lia.
Qed.

(* A despawn strictly advances the generation of its slot, and nothing after
   it can bring the counter back down. *)
Theorem despawn_advances : forall s w w',
  has_despawn s w w' -> gen w s < gen w' s.
Proof.
  intros s w w' Hd. induction Hd as [w w' Hl Hrest | w w' w'' Hs Hd IH].
  - pose proof (gen_monotone (despawn w s) w' s Hrest) as Hm.
    unfold despawn in Hm. simpl in Hm. rewrite upd_same in Hm. lia.
  - pose proof (step_gen_mono w w' s Hs). lia.
Qed.

(* ===================================================================== *)
(* 3. Resolution                                                          *)
(* ===================================================================== *)

Theorem resolve_sound : forall w r r',
  resolve w r = Some r' -> r' = r /\ valid w r.
Proof.
  intros w [s g] r' Hres. unfold resolve in Hres. simpl in Hres.
  destruct (live w s) eqn:Hl; simpl in Hres; [| discriminate].
  destruct (Nat.eqb (gen w s) g) eqn:Hg; [| discriminate].
  inversion Hres; subst. split. reflexivity.
  unfold valid. simpl. split. exact Hl. apply Nat.eqb_eq. exact Hg.
Qed.

(* MAIN: once the slot has been despawned, the old reference never resolves
   again -- whatever happened afterwards, including a respawn. *)
Theorem stale_never_resolves : forall w w' s g,
  valid w (s, g) -> has_despawn s w w' -> resolve w' (s, g) = None.
Proof.
  intros w w' s g [_ Hg] Hd. simpl in Hg.
  pose proof (despawn_advances s w w' Hd) as Hlt.
  unfold resolve. simpl.
  destruct (live w' s); simpl; [| reflexivity].
  destruct (Nat.eqb (gen w' s) g) eqn:E; [| reflexivity].
  apply Nat.eqb_eq in E. lia.
Qed.

(* MAIN: a reference that still resolves after the run names the incarnation
   it was taken from -- no despawn of its slot happened in between. *)
Theorem resolved_means_same_incarnation : forall w w' s g,
  valid w (s, g) -> wsteps w w' -> resolve w' (s, g) <> None ->
  gen w' s = gen w s /\ ~ has_despawn s w w'.
Proof.
  intros w w' s g Hv Hrun Hres.
  assert (Hg : gen w' s = g).
  { unfold resolve in Hres. simpl in Hres.
    destruct (live w' s); simpl in Hres; [| exfalso; apply Hres; reflexivity].
    destruct (Nat.eqb (gen w' s) g) eqn:E; [apply Nat.eqb_eq; exact E |].
    exfalso. apply Hres. reflexivity. }
  destruct Hv as [_ Hg0]. simpl in Hg0.
  split.
  - lia.
  - intro Hd. pose proof (despawn_advances s w w' Hd). lia.
Qed.

(* ===================================================================== *)
(* 4. Refutation: an unchecked dereference hits the new occupant          *)
(*                                                                        *)
(*   slot 0 @ gen 0, live  --despawn-->  gen 1, dead  --respawn-->  gen 1, live *)
(*                                                                        *)
(* The task took (0, 0) before suspending. After the run the slot is live, *)
(* so a dereference by id succeeds -- on an entity the task never saw.    *)
(* The generational resolve says no.                                      *)
(* ===================================================================== *)

Definition w0 : World := mkWorld (fun _ => 0) (fun _ => true).

Theorem unchecked_deref_hits_new_occupant :
  valid w0 (0, 0) /\
  exists w', wsteps w0 w' /\ has_despawn 0 w0 w' /\
             deref_unchecked w' (0, 0) = true /\ resolve w' (0, 0) = None.
Proof.
  split.
  - unfold valid, w0. simpl. split; reflexivity.
  - exists (respawn (despawn w0 0) 0). split; [| split; [| split]].
    + eapply wsteps_trans. apply (ws_despawn w0 0). reflexivity.
      eapply wsteps_trans. apply (ws_respawn (despawn w0 0) 0).
      unfold despawn. simpl. reflexivity.
      apply wsteps_refl.
    + apply (hd_here 0 w0). reflexivity.
      eapply wsteps_trans. apply (ws_respawn (despawn w0 0) 0).
      unfold despawn. simpl. reflexivity.
      apply wsteps_refl.
    + reflexivity.
    + reflexivity.
Qed.

(* ===================================================================== *)
(* 5. Non-vacuity: with no despawn of the slot the reference stays good   *)
(* ===================================================================== *)

Theorem revalidation_not_vacuous :
  exists w', wsteps w0 w' /\ w' <> w0 /\ resolve w' (0, 0) = Some (0, 0).
Proof.
  (* despawn and respawn a DIFFERENT slot; slot 0 keeps its incarnation *)
  exists (respawn (despawn w0 1) 1). split; [| split].
  - eapply wsteps_trans. apply (ws_despawn w0 1). reflexivity.
    eapply wsteps_trans. apply (ws_respawn (despawn w0 1) 1).
    unfold despawn. simpl. reflexivity.
    apply wsteps_refl.
  - intro Heq.
    assert (Hg : gen (respawn (despawn w0 1) 1) 1 = gen w0 1) by (rewrite Heq; reflexivity).
    unfold respawn, despawn, w0 in Hg. simpl in Hg. rewrite upd_same in Hg. discriminate.
  - reflexivity.
Qed.
