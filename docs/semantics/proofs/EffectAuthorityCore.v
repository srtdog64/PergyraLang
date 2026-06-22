(*
  Pergyra Formal Semantics -- Mechanized Fragment (second corner)
  Target: docs/semantics/19 "Pergyra Abstract Machine Obligation"
  Status: proof-sketch; not beta-closure evidence unless checked by CI (coqc).

  Scope: extends the boundary-transfer corner (ZoneCrossingCore.v) to TWO Step
  forms over ONE shared state, showing the composition the docs/19 synthesis
  flags as the hard part. The machine state carries AuthorityEvidence (held
  capabilities), a zone residence (ZoneGraph), and an EffectLog. The two steps
  are a capability-gated zone crossing and a capability-gated effect emission.

  Mechanized obligations (docs/19):
    - Effect isolation: an effect cannot appear in the log without an authority
      evidence path -- every newly emitted effect was authorized
      (`step_effect_authorized`).
    - Capability soundness (crossing): any zone change witnesses the entry
      capability (`crossing_capability_sound`).
    - Progress / fail-closed: an emission whose capability is not held is not a
      derivable step (`fail_closed_emit`); likewise no ambient authority
      (`step_preserves_authority`).

  Composition claim: both steps act on the SAME state and the SAME authority
  evidence, and authority is invariant under either -- so the two capability
  disciplines (movement, effect) compose without one weakening the other.

  Negative scope: still no slot/typestate (resource op) or authority delegation
  (says-modality) step, and the graphs are model parameters, not yet bound to the
  live AIR/MIR owner facts (that binding is task #45 / docs/18 machine-neutral).
*)

Require Import Coq.Lists.List.
Require Import Coq.micromega.Lia.
Import ListNotations.

Section EffectAuthorityCore.

Definition zone := nat.
Definition cap  := nat.
Definition eff  := nat.

Definition zone_graph   := zone -> cap.   (* cap required to ENTER each zone *)
Definition effect_graph := eff  -> cap.   (* cap required to EMIT each effect *)

Definition authority := list cap.

(* One shared state: held authority + residence + the effect log. *)
Record config := mkConfig {
  held : authority;
  here : zone;
  elog : list eff
}.

Definition has_cap (c : config) (k : cap) : Prop := In k (held c).

(* Two capability-gated Step forms on the one state. Fail-closed by
   construction: neither constructor admits an ungated transition. *)
Inductive step (gz : zone_graph) (ge : effect_graph) : config -> config -> Prop :=
| SCross : forall c z',
    has_cap c (gz z') ->
    step gz ge c (mkConfig (held c) z' (elog c))
| SEmit : forall c e,
    has_cap c (ge e) ->
    step gz ge c (mkConfig (held c) (here c) (e :: elog c)).

Inductive steps (gz : zone_graph) (ge : effect_graph) : config -> config -> Prop :=
| SRefl  : forall c, steps gz ge c c
| STrans : forall a b d, step gz ge a b -> steps gz ge b d -> steps gz ge a d.

(* ---- authority is invariant under either step (no ambient authority) ---- *)

Lemma step_preserves_authority : forall gz ge a b,
  step gz ge a b -> held b = held a.
Proof. intros gz ge a b H. inversion H; subst; simpl; reflexivity. Qed.

Lemma steps_preserves_authority : forall gz ge a b,
  steps gz ge a b -> held b = held a.
Proof.
  intros gz ge a b H. induction H.
  - reflexivity.
  - rewrite IHsteps. apply (step_preserves_authority gz ge a b H).
Qed.

(* ---- crossing capability soundness (the reused corner) ---- *)
(* Any step that changes the zone witnesses the entry capability. *)

Theorem crossing_capability_sound : forall gz ge a b,
  step gz ge a b -> here b <> here a -> has_cap a (gz (here b)).
Proof.
  intros gz ge a b H Hne. inversion H; subst; simpl in *.
  - unfold has_cap. simpl. assumption.
  - exfalso. apply Hne. reflexivity.
Qed.

(* ---- effect isolation: a newly logged effect was authorized ---- *)

Theorem step_effect_authorized : forall gz ge a b e,
  step gz ge a b -> In e (elog b) -> ~ In e (elog a) -> has_cap a (ge e).
Proof.
  intros gz ge a b e H Hin Hnin.
  inversion H; subst; simpl in *.
  - (* SCross: elog unchanged -> Hin contradicts Hnin *)
    contradiction.
  - (* SEmit: elog b = e0 :: elog a *)
    destruct Hin as [Heq | Hin'].
    + subst. assumption.
    + contradiction.
Qed.

(* ---- progress / fail-closed for emission ---- *)

Lemma no_self_cons : forall (A : Type) (x : A) (l : list A), x :: l <> l.
Proof.
  intros A x l H. apply (f_equal (@length A)) in H. simpl in H. lia.
Qed.

Theorem fail_closed_emit : forall gz ge c e,
  ~ has_cap c (ge e) ->
  ~ (exists c', step gz ge c c' /\ elog c' = e :: elog c).
Proof.
  intros gz ge c e Hno [c' [Hstep Hlog]].
  inversion Hstep; subst; simpl in *.
  - (* SCross: elog c' = elog c, so Hlog says elog c = e :: elog c -- impossible *)
    apply (no_self_cons eff e (elog c)). symmetry. exact Hlog.
  - (* SEmit: e0 :: elog c = e :: elog c -> e0 = e, premise gives has_cap c (ge e0) *)
    injection Hlog as He. subst. contradiction.
Qed.

End EffectAuthorityCore.
