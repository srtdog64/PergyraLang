(*
  Pergyra Formal Semantics -- Mechanized Fragment
  Target: docs/semantics/19 "Pergyra Abstract Machine Obligation"
  Status: proof-sketch; not beta-closure evidence unless checked by CI (coqc).

  Scope: the FIRST fragment of the Pergyra core calculus -- the boundary-transfer
  step of the abstract machine. The machine state carries a ZoneGraph (which
  capability the world requires to enter each zone) and AuthorityEvidence (the
  capabilities a configuration holds). The only step is a capability-gated
  boundary transfer (zone crossing). This mechanizes three obligations docs/19
  lists for the abstract machine:

    - Capability soundness: a backend-visible residence is reachable only through
      a capability/evidence path. Here: residing in a crossed-into zone witnesses
      the entry capability the ZoneGraph requires.
    - Progress / fail-closed: an attempted crossing whose entry capability is not
      held is not a derivable step (no ambient-authority crossing rule), so the
      machine fails closed before backend execution.
    - Effect/authority isolation (no ambient authority): crossing never grants a
      capability; authority evidence is invariant under movement.

  Lineage (docs/19 lineage map, world/zone row): the Mobile Ambient Calculus
  (Cardelli-Gordon) -- bounded named places with capability-gated movement.

  Negative scope: this models ONLY the zone-crossing + authority-evidence facet.
  It does NOT model effects, lifecycle/typestate, intent coordination or
  compensation, nor the binding of this model onto the live AIR/MIR ZoneGraph
  facts (a separate adequacy obligation). Composition with the other axes
  (effect, slot lifecycle, authority delegation, intent) is the open synthesis
  named in docs/19; this file is one fail-closed corner of it.
*)

Require Import Coq.Lists.List.
Import ListNotations.

Section ZoneCrossingCore.

(* Abstract names for zones and capabilities. *)
Definition zone := nat.
Definition cap  := nat.

(* The ZoneGraph fact: the capability required to ENTER each zone. In the real
   compiler this is an AIR/MIR owner fact; here it is the model's parameter. *)
Definition zone_graph := zone -> cap.

(* AuthorityEvidence: the capabilities a configuration holds (its granted
   authority). Crossing never adds to it -- there is no ambient authority. *)
Definition authority := list cap.

(* A configuration of the abstract machine: held authority + current residence. *)
Record config := mkConfig {
  held : authority;
  here : zone
}.

(* Capability check against held authority evidence. *)
Definition has_cap (c : config) (k : cap) : Prop := In k (held c).

(* The boundary-transfer step is the ONLY rule, and it is capability-gated:
   a configuration may cross to z' iff it holds the capability the ZoneGraph
   requires to enter z'. Fail-closed by construction -- there is no rule that
   permits crossing without the required capability. *)
Inductive cross (g : zone_graph) : config -> config -> Prop :=
| Cross : forall c z',
    has_cap c (g z') ->
    cross g c (mkConfig (held c) z').

(* Reflexive-transitive closure: reachable configurations. *)
Inductive reaches (g : zone_graph) : config -> config -> Prop :=
| ReachRefl : forall c, reaches g c c
| ReachStep : forall a b d, cross g a b -> reaches g b d -> reaches g a d.

(* ----- Obligation: no ambient authority (authority is invariant) ----- *)

Lemma cross_preserves_authority : forall g a b,
  cross g a b -> held b = held a.
Proof. intros g a b H. inversion H; subst; simpl; reflexivity. Qed.

Lemma reaches_preserves_authority : forall g a b,
  reaches g a b -> held b = held a.
Proof.
  intros g a b H. induction H as [c | a b d Hab Hbd IH].
  - reflexivity.
  - rewrite IH. apply (cross_preserves_authority g a b Hab).
Qed.

Theorem no_ambient_authority : forall g a b k,
  cross g a b -> has_cap b k -> has_cap a k.
Proof.
  intros g a b k H Hk. unfold has_cap in *.
  rewrite (cross_preserves_authority g a b H) in Hk. exact Hk.
Qed.

(* ----- Obligation: capability soundness of a crossing ----- *)
(* Any crossing INTO a zone required, and therefore witnesses, its entry cap:
   the resulting residence is authorized. *)

Theorem crossing_capability_sound : forall g a b,
  cross g a b -> has_cap a (g (here b)).
Proof.
  intros g a b H. inversion H; subst; simpl in *.
  unfold has_cap in *. simpl. exact H0.
Qed.

(* ----- Obligation: progress / fail-closed ----- *)
(* If a configuration does NOT hold the entry capability for z', then no crossing
   into z' is derivable. The step is stuck, so the machine fails closed instead
   of silently entering an unauthorized zone. (Contrapositive of soundness.) *)

Theorem fail_closed_crossing : forall g c z',
  ~ has_cap c (g z') ->
  ~ (exists c', cross g c c' /\ here c' = z').
Proof.
  intros g c z' Hno [c' [Hcross Hhere]].
  apply Hno.
  rewrite <- Hhere.
  apply (crossing_capability_sound g c c' Hcross).
Qed.

(* ----- Lifted to reachability: every entered zone was authorized ----- *)
(* A configuration reachable from [a] holds the same authority as [a] (above),
   and every individual crossing on the path was capability-gated (by the
   inductive rule). Together: you cannot be anywhere you were not authorized to
   enter -- the fail-closed property holds along the whole run, not just one
   step. *)

Theorem reaches_authority_stable : forall g a b k,
  reaches g a b -> has_cap b k -> has_cap a k.
Proof.
  intros g a b k H Hk. unfold has_cap in *.
  rewrite (reaches_preserves_authority g a b H) in Hk. exact Hk.
Qed.

End ZoneCrossingCore.
