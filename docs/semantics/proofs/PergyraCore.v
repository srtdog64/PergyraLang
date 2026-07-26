(*
  Pergyra Formal Semantics -- Shared Core (the vertical spine's root)
  Status: PENDING kernel-check (rocq9 CI). The model and both lemmas are lifted
  verbatim from the already-verified UnifiedCore.v, so they are written to close
  with Qed and add 0 axioms -- but this file has not itself been through coqc on
  the authoring machine (no local prover); coq_kernel_check.sh in CI is the
  authority. Definitions only, plus two foundational lemmas.

  Why this file exists. The proof corpus grew as ~38 INDEPENDENT models: each
  file re-defines its own principal/zone/cap/slot/config/step and Requires only
  the Coq standard library, so no theorem in one file composes with a theorem in
  another. Even the capstone UnifiedCore.v re-states the abstract machine on a
  private copy rather than importing it. Broad in topic, sparse in vertical
  linkage.

  PergyraCore is the first shared foundation the rest of the corpus is meant to
  build ON, via `Require Import PergyraCore` -- turning re-definition into
  composition. It is the unified abstract-machine vocabulary (state + step
  relation) lifted verbatim out of UnifiedCore.v so there is ONE machine, not a
  copy per file. Downstream files derive their local notions from these names
  instead of re-declaring them; UnifiedCore's synthesis theorems and the four
  corner fragments migrate onto this root next.

  Configuration:
    actor    : the principal currently acting
    holdings : authority distribution (principal -> capabilities)
    here     : zone residence
    elog     : effect log; each entry carries the pre-effect store snapshot
    store    : slot typestate map

  Step forms (all capability/typestate gated, fail-closed by construction):
    Cross z' | Emit e | Acquire s | Use s | Release s | Delegate b k | Rollback

  This file introduces NO axioms: the two abstract Parameters the corpus is
  allowed to assume live in SlotCalculus, not here. Adding an Axiom/Admitted to
  this foundation would widen the kernel-checked budget and fail coq_kernel_check.
*)

Require Import Coq.Lists.List.
Require Import Coq.Arith.PeanoNat.
Import ListNotations.

Definition principal := nat.
Definition zone := nat.
Definition cap  := nat.
Definition eff  := nat.
Definition slot := nat.

Inductive lcstate := Empty | Filled | Released.

Definition slot_store := slot -> lcstate.

Definition slot_in (s : slot) (ss : list slot) : bool :=
  existsb (fun x => Nat.eqb s x) ss.

Definition restore_targets
  (current : slot_store) (before : slot_store) (targets : list slot) : slot_store :=
  fun x => if slot_in x targets then before x else current x.

Lemma slot_in_true : forall s targets,
  In s targets -> slot_in s targets = true.
Proof.
  intros s targets Hin. unfold slot_in.
  induction targets as [| x xs IH]; simpl in *.
  - contradiction.
  - destruct Hin as [Heq | Hin].
    + subst x. rewrite Nat.eqb_refl. reflexivity.
    + destruct (Nat.eqb s x) eqn:E.
      * reflexivity.
      * apply IH. exact Hin.
Qed.

Definition zone_graph    := zone -> cap.
Definition effect_graph  := eff  -> cap.
Definition acquire_graph := slot -> cap.
Definition comp_target   := eff -> list slot.

Record effect_log_entry := mkLog {
  logged_eff : eff;
  before_store : slot_store
}.

Inductive action :=
  | ActCross (z' : zone)
  | ActEmit (e : eff)
  | ActAcquire (s : slot)
  | ActUse (s : slot)
  | ActRelease (s : slot)
  | ActDelegate (b : principal) (k : cap)
  | ActRollback.

Definition cmap (h : principal -> list cap) (p : principal) (cs : list cap)
  : principal -> list cap :=
  fun x => if Nat.eqb x p then cs else h x.
Definition smap (s0 : slot_store) (s : slot) (v : lcstate)
  : slot_store :=
  fun x => if Nat.eqb x s then v else s0 x.

Record config := mkConfig {
  actor    : principal;
  holdings : principal -> list cap;
  here     : zone;
  elog     : list effect_log_entry;
  store    : slot_store
}.

Definition has_cap (c : config) (k : cap) : Prop := In k (holdings c (actor c)).
Definition in_circulation (c : config) (k : cap) : Prop :=
  exists p, In k (holdings c p).

Definition with_zone  (c : config) (z : zone) : config :=
  mkConfig (actor c) (holdings c) z (elog c) (store c).
Definition with_emit  (c : config) (e : eff) : config :=
  mkConfig (actor c) (holdings c) (here c)
           (mkLog e (store c) :: elog c) (store c).
Definition with_store (c : config) (s : slot) (v : lcstate) : config :=
  mkConfig (actor c) (holdings c) (here c) (elog c) (smap (store c) s v).
Definition with_deleg (c : config) (b : principal) (k : cap) : config :=
  mkConfig (actor c) (cmap (holdings c) b (k :: holdings c b))
           (here c) (elog c) (store c).
Definition with_target_deleg
  (c : config) (b : principal) (ga : acquire_graph) (targets : list slot)
  : config :=
  mkConfig (actor c)
           (cmap (holdings c) b (map ga targets ++ holdings c b))
           (here c) (elog c) (store c).
Definition with_rollback
  (c : config) (targets : list slot) (before : slot_store)
  (rest : list effect_log_entry) : config :=
  mkConfig (actor c) (holdings c) (here c) rest
           (restore_targets (store c) before targets).

Inductive step (gz : zone_graph) (ge : effect_graph) (ga : acquire_graph) (ct : comp_target)
  : action -> config -> config -> Prop :=
| SCross   : forall c z', has_cap c (gz z') ->
               step gz ge ga ct (ActCross z') c (with_zone c z')
| SEmit    : forall c e,  has_cap c (ge e) ->
               step gz ge ga ct (ActEmit e) c (with_emit c e)
| SAcquire : forall c s,  has_cap c (ga s) -> store c s = Empty ->
               step gz ge ga ct (ActAcquire s) c (with_store c s Filled)
| SUse     : forall c s,  store c s = Filled ->
               step gz ge ga ct (ActUse s) c c
| SRelease : forall c s,  store c s = Filled ->
               step gz ge ga ct (ActRelease s) c (with_store c s Released)
| SDelegate: forall c b k, has_cap c k ->
               step gz ge ga ct (ActDelegate b k) c (with_deleg c b k)
| SRollback: forall c e before rest,
               elog c = mkLog e before :: rest ->
               Forall (fun s => has_cap c (ga s)) (ct e) ->
               step gz ge ga ct ActRollback c
                    (with_rollback c (ct e) before rest).

(* Inductive multi-step relation. *)
Inductive steps (gz : zone_graph) (ge : effect_graph) (ga : acquire_graph) (ct : comp_target)
  : config -> config -> Prop :=
| SRefl : forall c, steps gz ge ga ct c c
| SStep : forall act a b c,
    step gz ge ga ct act a b -> steps gz ge ga ct b c -> steps gz ge ga ct a c.

(* Foundational authority lemma reused by the conservation theorems: a new
   capability in circulation after a cmap update is either the freshly written
   list or was already circulating. *)
Lemma cmap_circulation : forall h b cs k,
  (exists p, In k (cmap h b cs p)) ->
  In k cs \/ (exists p, In k (h p)).
Proof.
  intros h b cs k [p Hp]. unfold cmap in Hp.
  destruct (Nat.eqb p b) eqn:E.
  - left. exact Hp.
  - right. exists p. exact Hp.
Qed.
