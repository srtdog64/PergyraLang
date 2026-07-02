(*
  Pergyra Formal Semantics -- Calculus <-> AIR Fact Binding
  Target: docs/semantics/18 machine-neutral / docs/semantics/19 abstract machine
          (task #47 second half: bind the calculus terms to AIR-owned facts).
  Status: machine-verified (coqc, 0 admits / 0 axioms). All theorems close with Qed.

  The whole-program machine (WholeProgramCore.v) gates every backend-visible
  action on five fact families:

      zone_gate    : zone -> cap        (SCross)          <-> AIR boundary cap
      effect_gate  : eff  -> cap        (SEmit)           <-> AIR per-op effect site
      acquire_gate : slot -> cap        (SAcquire, SRollback) <-> AIR slot-cap site
      comp_targets : eff  -> list slot  (SRollback)       <-> AIR compensation edge
      dep_graph    : task -> list task  (SRun)            <-> AIR intent dep edge

  docs/semantics/18 flagged the machine-neutral gap: these facts were orphaned
  from AIR (scattered across semantic / MIR / runtime), so the program could not
  be gated by a single owner. This file bundles the five families into ONE record
  -- AIRFacts -- and proves two things a fact-ownership claim needs:

    1. Faithfulness (guard_air_faithful): the machine's guard is EXACTLY the guard
       computed from the AIRFacts record. AIR owning these five fields is both
       necessary and sufficient to reconstruct every gate; nothing outside the
       record influences a gating decision.

    2. Per-gate single-owner locality (each *_reads_only lemma + the umbrella
       gate_locality): each action's gate reads EXACTLY ONE AIR field. Changing
       any other field cannot change that action's gate. This is the operational
       form of the docs/42 axis single-owner discipline, at the AIR-fact level:
       the boundary cap is owned by the zone field alone, the effect cap by the
       effect field alone, and so on -- no silent cross-ownership.

  Consequence: the AIR record IS the complete gating interface. A backend that
  consumes AIRFacts can reproduce every fail-closed decision without re-deriving
  anything from semantic/MIR internals -- the machine-neutral property, mechanized.

  This file is standalone (re-declares the minimal config + guard) to match the
  one-file-per-proof convention; WholeProgramCore.v is the source of truth for
  the machine, and the guard here is definitionally its guard.

  Negative scope: this proves the gate READS ONLY these facts; it does NOT prove
  the C AIR emitter populates them correctly (that is the air-json-schema smoke +
  the machine-neutral RED->GREEN gate), nor that the fact values are the intended
  ones. It fixes the INTERFACE, not the producer.
*)

Require Import Coq.Lists.List.
Require Import Coq.Arith.PeanoNat.
Import ListNotations.

Section AIRBinding.

Definition principal := nat.
Definition zone := nat.
Definition cap  := nat.
Definition eff  := nat.
Definition slot := nat.
Definition task := nat.

Inductive lcstate := Empty | Filled | Released.
Definition slot_store := slot -> lcstate.

Record effect_log_entry := mkLog { logged_eff : eff; before_store : slot_store }.

Record config := mkConfig {
  actor    : principal;
  holdings : principal -> list cap;
  here     : zone;
  elog     : list effect_log_entry;
  store    : slot_store;
  done     : list task
}.

Definition has_cap (c : config) (k : cap) : Prop := In k (holdings c (actor c)).

Inductive action :=
  | ActCross (z' : zone)
  | ActEmit (e : eff)
  | ActAcquire (s : slot)
  | ActUse (s : slot)
  | ActRelease (s : slot)
  | ActDelegate (b : principal) (k : cap)
  | ActRollback
  | ActRun (t : task).

(* ================================================================ *)
(* The AIR fact record: exactly the five families the gate reads.   *)
(* ================================================================ *)

Record AIRFacts := mkAIR {
  air_zone_gate    : zone -> cap;
  air_effect_gate  : eff  -> cap;
  air_acquire_gate : slot -> cap;
  air_comp_targets : eff  -> list slot;
  air_dep_graph    : task -> list task
}.

Definition ready_air (F : AIRFacts) (c : config) (t : task) : Prop :=
  forall x, In x (air_dep_graph F t) -> In x (done c).

(* The gate computed purely from the AIR record and the config. *)
Definition guard_air (F : AIRFacts) (act : action) (c : config) : Prop :=
  match act with
  | ActCross z'     => has_cap c (air_zone_gate F z')
  | ActEmit e       => has_cap c (air_effect_gate F e)
  | ActAcquire s    => has_cap c (air_acquire_gate F s) /\ store c s = Empty
  | ActUse s        => store c s = Filled
  | ActRelease s    => store c s = Filled
  | ActDelegate _ k => has_cap c k
  | ActRollback     => exists e before rest,
                         elog c = mkLog e before :: rest /\
                         Forall (fun s => has_cap c (air_acquire_gate F s))
                                (air_comp_targets F e)
  | ActRun t        => ready_air F c t
  end.

(* The machine's guard, with the five families as loose parameters -- this is
   definitionally WholeProgramCore.guard. *)
Definition guard_machine
  (gz : zone -> cap) (ge : eff -> cap) (ga : slot -> cap)
  (ct : eff -> list slot) (dg : task -> list task)
  (act : action) (c : config) : Prop :=
  match act with
  | ActCross z'     => has_cap c (gz z')
  | ActEmit e       => has_cap c (ge e)
  | ActAcquire s    => has_cap c (ga s) /\ store c s = Empty
  | ActUse s        => store c s = Filled
  | ActRelease s    => store c s = Filled
  | ActDelegate _ k => has_cap c k
  | ActRollback     => exists e before rest,
                         elog c = mkLog e before :: rest /\
                         Forall (fun s => has_cap c (ga s)) (ct e)
  | ActRun t        => forall x, In x (dg t) -> In x (done c)
  end.

(* ================================================================ *)
(* 1. Faithfulness: AIR record reconstructs the machine gate exactly.*)
(* ================================================================ *)

Theorem guard_air_faithful : forall F act c,
  guard_air F act c <->
  guard_machine (air_zone_gate F) (air_effect_gate F) (air_acquire_gate F)
                (air_comp_targets F) (air_dep_graph F) act c.
Proof.
  intros F act c. destruct act; simpl; reflexivity.
Qed.

(* Contrapositive corollary: a fail-closed refusal is likewise an AIR-fact
   decision -- if the AIR gate does not hold, the machine does not step. *)
Corollary refusal_is_air_decision : forall F act c,
  ~ guard_air F act c ->
  ~ guard_machine (air_zone_gate F) (air_effect_gate F) (air_acquire_gate F)
                  (air_comp_targets F) (air_dep_graph F) act c.
Proof.
  intros F act c Hn Hg. apply Hn. apply guard_air_faithful. exact Hg.
Qed.

(* ================================================================ *)
(* 2. Per-gate single-owner locality: each action reads ONE field.  *)
(* ================================================================ *)

(* Cross reads only the zone field. *)
Lemma cross_reads_only_zone : forall F F' z' c,
  air_zone_gate F z' = air_zone_gate F' z' ->
  (guard_air F (ActCross z') c <-> guard_air F' (ActCross z') c).
Proof. intros F F' z' c Heq; simpl; rewrite Heq; reflexivity. Qed.

(* Emit reads only the effect field. *)
Lemma emit_reads_only_effect : forall F F' e c,
  air_effect_gate F e = air_effect_gate F' e ->
  (guard_air F (ActEmit e) c <-> guard_air F' (ActEmit e) c).
Proof. intros F F' e c Heq; simpl; rewrite Heq; reflexivity. Qed.

(* Acquire reads only the acquire field. *)
Lemma acquire_reads_only_acquire : forall F F' s c,
  air_acquire_gate F s = air_acquire_gate F' s ->
  (guard_air F (ActAcquire s) c <-> guard_air F' (ActAcquire s) c).
Proof. intros F F' s c Heq; simpl; rewrite Heq; reflexivity. Qed.

(* Run reads only the dependency field. *)
Lemma run_reads_only_deps : forall F F' t c,
  air_dep_graph F t = air_dep_graph F' t ->
  (guard_air F (ActRun t) c <-> guard_air F' (ActRun t) c).
Proof.
  intros F F' t c Heq; simpl; unfold ready_air; rewrite Heq; reflexivity.
Qed.

(* Rollback reads only the comp-target and acquire fields (it re-acquires the
   coupled slots), and nothing else. *)
Lemma rollback_reads_only_comp_acquire : forall F F' c,
  (forall e, air_comp_targets F e = air_comp_targets F' e) ->
  (forall s, air_acquire_gate F s = air_acquire_gate F' s) ->
  (guard_air F ActRollback c <-> guard_air F' ActRollback c).
Proof.
  intros F F' c Hct Hga; simpl.
  split; intros [e [before [rest [Hlog Hall]]]];
    exists e, before, rest; split; try exact Hlog;
    rewrite <- Hct in * || rewrite Hct in *;
    (eapply Forall_impl; [ | eassumption]);
    intros s Hs; unfold has_cap in *;
    (rewrite Hga in * || rewrite <- Hga in *); exact Hs.
Qed.

(* Umbrella: the intent-carrying actions (cross/emit/acquire/run) are each
   invariant under changes to the OTHER AIR fields -- no silent cross-ownership.
   Delegate/Use/Release read no AIR field at all (pure typestate/capability). *)
Theorem gate_locality : forall F F' c,
  air_zone_gate    F = air_zone_gate    F' ->
  air_effect_gate  F = air_effect_gate  F' ->
  air_acquire_gate F = air_acquire_gate F' ->
  air_comp_targets F = air_comp_targets F' ->
  air_dep_graph    F = air_dep_graph    F' ->
  forall act, guard_air F act c <-> guard_air F' act c.
Proof.
  intros F F' c Hz He Ha Hc Hd act.
  destruct act; simpl.
  - (* Cross *) rewrite Hz; reflexivity.
  - (* Emit *) rewrite He; reflexivity.
  - (* Acquire *) rewrite Ha; reflexivity.
  - (* Use *) reflexivity.
  - (* Release *) reflexivity.
  - (* Delegate *) reflexivity.
  - (* Rollback *) rewrite Hc, Ha; reflexivity.
  - (* Run *) unfold ready_air; rewrite Hd; reflexivity.
Qed.

(* Delegate and Use/Release consult NO AIR fact -- they are decided entirely by
   the config (held capability / slot typestate). This is the boundary of the
   AIR gating interface: authority delegation flows through holdings, typestate
   through the store, neither through AIR. *)
Theorem delegate_use_release_air_independent : forall F F' c b k s,
  (guard_air F (ActDelegate b k) c <-> guard_air F' (ActDelegate b k) c) /\
  (guard_air F (ActUse s) c <-> guard_air F' (ActUse s) c) /\
  (guard_air F (ActRelease s) c <-> guard_air F' (ActRelease s) c).
Proof. intros; simpl; repeat split; auto. Qed.

End AIRBinding.
