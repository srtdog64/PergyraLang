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

    1. Interface identity (guard_air_faithful): guard_air is defined to call
       the imported guard. This is definitional equality, not an independently
       verified producer, necessary/minimal representation, or runtime binding.
       The current config (holdings, store, log and done-set) also determines
       admission; AIRFacts alone is not sufficient.

    2. Per-gate single-owner locality (each *_reads_only lemma + the umbrella
       gate_locality): the lemmas identify the fields each action reads. Rollback
       reads two; several operations read no AIR field. Changing
       any other field cannot change that action's gate. This is the operational
       form of the docs/42 axis single-owner discipline, at the AIR-fact level:
       the boundary cap is owned by the zone field alone, the effect cap by the
       effect field alone, and so on -- no silent cross-ownership.

  Scope: the record and config are the abstract machine's chosen gate inputs.
  No compiler AIRFacts issuer or backend runtime refinement is proved here.

  WholeProgramCore.v owns the coordination-expanded machine. This file imports
  its actual configuration, actions and guard; there is no private machine
  copy whose agreement would need to be maintained by text convention.

  Negative scope: this proves the gate READS ONLY these facts; it does NOT prove
  the C AIR emitter populates them correctly (that is the air-json-schema smoke +
  the machine-neutral RED->GREEN gate), nor that the fact values are the intended
  ones. It fixes the INTERFACE, not the producer.
*)

Require Import Stdlib.Lists.List.
Require Import Stdlib.Arith.PeanoNat.
Require Import WholeProgramCore.
Import ListNotations.

Section AIRBinding.

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

(* The record supplies facts; the imported machine owns every gate decision. *)
Definition guard_air (F : AIRFacts) (act : action) (c : config) : Prop :=
  WholeProgramCore.guard (air_zone_gate F) (air_effect_gate F)
    (air_acquire_gate F) (air_comp_targets F) (air_dep_graph F) act c.

(* ================================================================ *)
(* 1. Faithfulness: AIR record reconstructs the machine gate exactly.*)
(* ================================================================ *)

Theorem guard_air_faithful : forall F act c,
  guard_air F act c <->
  WholeProgramCore.guard (air_zone_gate F) (air_effect_gate F) (air_acquire_gate F)
                (air_comp_targets F) (air_dep_graph F) act c.
Proof.
  intros F act c. reflexivity.
Qed.

(* Contrapositive corollary: a fail-closed refusal is likewise an AIR-fact
   decision -- if the AIR gate does not hold, the machine does not step. *)
Corollary refusal_is_air_decision : forall F act c,
  ~ guard_air F act c ->
  ~ WholeProgramCore.guard (air_zone_gate F) (air_effect_gate F) (air_acquire_gate F)
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
  intros F F' t c Heq; simpl; unfold WholeProgramCore.ready;
    rewrite Heq; reflexivity.
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
  - (* Run *) unfold WholeProgramCore.ready; rewrite Hd; reflexivity.
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
