(*
  Pergyra Formal Semantics - Mechanized Sketch
  Target: minimality of the codegen IR-layer decomposition and the AIR
  verification witness surface.
  Status: proof-sketch; not beta-closure evidence unless checked by CI (coqc).

  Question (architectural): is the IR decomposition HIR/DIR/RIR/MIR over-
  decomposed, or is it the minimum number of layers the dependencies allow?
  "Minimal" is made precise here as: with respect to the real reads-from
  dependency among the IRs, no *valid* layering (one where an IR is in a strictly
  earlier layer than anything that reads its completed output) uses fewer layers.
  By Mirsky's theorem the minimum equals the longest reads-from chain.

  Grounded edges (driver_app.c order; verified by grep):
    - hir_lower(ast), rir_lower(ast)                    -- AST-derived
    - dir_lower_with_hir_facts(ast, hir)                -- DIR reads HIR
    - rir_enrich_scope_with_hir_flow(scope, hir)        -- RIR reads HIR flow
    - mir_lower_request_bind_dir(request, dir)          -- MIR reads DIR
    - mir_lower(request)                               -- MIR fuses HIR/RIR/DIR
    - Backend codegen consumes MIR, not DIR/AIR directly. This does NOT erase
      the driver's DIR -> MIR dependency.

  Negative scope: this proves minimality W.R.T. THIS fact/dependency model. It
  does NOT prove that some entirely different IR design could not be simpler --
  you cannot quantify over all possible designs in this model. It proves the
  current fact-sets cannot be carried in fewer layers given their real reads-from
  edges. There are two independent length-three chains, not a single pivot.

  Second question: for intent composition, is a Functor/HKT abstraction the
  smaller core, or is AIR the smaller proof surface? The model below treats
  "smaller" as a witness-set question: any adequate verifier for Pergyra intent
  composition must carry the domain facts that make side effects auditable.
  Functor/HKT laws describe shape-preserving maps over type constructors; they
  do not witness authority, effect, boundary, coordination, or provenance facts.
  This witness vocabulary is an interface declaration, not a proof of AIR
  architecture minimality or of the expressiveness limits of HKT/Functor.
*)

Require Import Stdlib.Init.Nat.
Require Import Stdlib.Arith.PeanoNat.
Require Import Stdlib.micromega.Lia.

(* The codegen-relevant IRs. (AST and the backend are endpoints, not layers.) *)
Inductive Node : Type := NHIR | NDIR | NRIR | NMIR.

(* The real construction reads-from edges. *)
Inductive Reads : Node -> Node -> Prop :=
  | ReadsRIR_HIR : Reads NRIR NHIR    (* RIR enriched with HIR flow *)
  | ReadsMIR_HIR : Reads NMIR NHIR    (* MIR fuses HIR *)
  | ReadsMIR_RIR : Reads NMIR NRIR    (* MIR fuses RIR *)
  | ReadsDIR_HIR : Reads NDIR NHIR    (* DIR uses admitted HIR facts *)
  | ReadsMIR_DIR : Reads NMIR NDIR.   (* MIR request binds DIR *)

(* A layering assigns each IR a layer index. It is VALID when, whenever A reads
   B's completed output, B sits in a strictly earlier layer. *)
Definition Layering := Node -> nat.
Definition Valid (L : Layering) : Prop :=
  forall a b, Reads a b -> L b < L a.

(* ========================================== *)
(* 1. Lower bound: the codegen chain forces 3 *)
(* ========================================== *)

(* In any valid layering the codegen IRs form a strictly increasing chain
   HIR < RIR < MIR -- the longest reads-from chain. *)
Theorem codegen_chain :
  forall L, Valid L -> L NHIR < L NRIR /\ L NRIR < L NMIR.
Proof.
  intros L HV. split.
  - apply HV. apply ReadsRIR_HIR.
  - apply HV. apply ReadsMIR_RIR.
Qed.

(* Hence the three codegen IRs occupy three distinct layers: you cannot carry
   HIR, RIR and MIR in fewer than 3 layers. *)
Theorem codegen_needs_three :
  forall L, Valid L ->
    L NHIR <> L NRIR /\ L NRIR <> L NMIR /\ L NHIR <> L NMIR.
Proof.
  intros L HV. destruct (codegen_chain L HV) as [H1 H2]. repeat split; lia.
Qed.

(* ========================================== *)
(* 2. Upper bound: 3 layers suffice; DIR/RIR   *)
(* share the middle layer, not HIR's layer.   *)
(* ========================================== *)

Definition L3 (n : Node) : nat :=
  match n with NHIR => 0 | NDIR => 1 | NRIR => 1 | NMIR => 2 end.

Theorem three_layers_suffice : Valid L3.
Proof. intros a b HR. destruct HR; simpl; lia. Qed.

(* DIR and RIR both consume HIR before MIR consumes their completed facts. *)
Theorem dir_colocates_with_rir : L3 NDIR = L3 NRIR.
Proof. reflexivity. Qed.

Theorem domain_chain : forall L, Valid L ->
  L NHIR < L NDIR /\ L NDIR < L NMIR.
Proof. intros L H; split; apply H; constructor. Qed.

(* Three dependency levels for this fixed graph, not a theorem that three IR
   representations are the smallest conceivable compiler architecture. *)
Theorem codegen_minimum_is_three :
  Valid L3 /\ (forall L, Valid L ->
    L NHIR <> L NRIR /\ L NRIR <> L NMIR /\ L NHIR <> L NMIR).
Proof. split. apply three_layers_suffice. apply codegen_needs_three. Qed.

(* ========================================== *)
(* 3. Deferring RIR flow alone does not       *)
(* remove the independent HIR -> DIR -> MIR. *)
(* ========================================== *)

(* Keep every real edge except the proposed RIR<-HIR deferral. *)
Inductive ReadsDeferred : Node -> Node -> Prop :=
  | DReadsMIR_HIR : ReadsDeferred NMIR NHIR
  | DReadsMIR_RIR : ReadsDeferred NMIR NRIR
  | DReadsDIR_HIR : ReadsDeferred NDIR NHIR
  | DReadsMIR_DIR : ReadsDeferred NMIR NDIR.

Definition ValidD (L : Layering) : Prop :=
  forall a b, ReadsDeferred a b -> L b < L a.

Definition L2 (n : Node) : nat :=
  match n with NHIR => 0 | NDIR => 0 | NRIR => 0 | NMIR => 1 end.

Theorem deferred_still_needs_three : forall L,
  ValidD L -> L NHIR < L NDIR /\ L NDIR < L NMIR.
Proof. intros L H; split; apply H; constructor. Qed.

Theorem two_layers_refused_when_only_rir_deferred : ~ ValidD L2.
Proof. intros H. pose proof (proj1 (deferred_still_needs_three L2 H)); simpl in *; lia. Qed.

Theorem three_layers_suffice_when_deferred : ValidD L3.
Proof. intros a b H; destruct H; simpl; lia. Qed.

(* ========================================== *)
(* 4. AIR witness minimality                   *)
(* ========================================== *)

(* Intent composition in Pergyra is not just map/identity/composition over a
   type constructor. It must verify the domain facts that make the composition
   auditable. These are the required observations for the beta AIR contract. *)
Inductive VerificationRequirement : Type :=
  | ReqIntentOrder
  | ReqBoundary
  | ReqAuthority
  | ReqEffect
  | ReqCoordination
  | ReqProvenance.

Definition EvidenceSurface := VerificationRequirement -> Prop.
Definition AdequateEvidence (S : EvidenceSurface) : Prop :=
  forall r, S r.

(* AIR has exactly one witness class for each required verification axis. *)
Inductive AIRWitness : VerificationRequirement -> Prop :=
  | AIRIntentOrder : AIRWitness ReqIntentOrder
  | AIRBoundary : AIRWitness ReqBoundary
  | AIRAuthority : AIRWitness ReqAuthority
  | AIREffect : AIRWitness ReqEffect
  | AIRCoordination : AIRWitness ReqCoordination
  | AIRProvenance : AIRWitness ReqProvenance.

Theorem air_witness_adequate : AdequateEvidence AIRWitness.
Proof.
  intros r. destruct r; constructor.
Qed.

(* An intentionally restricted interface with only an ordering constructor.
   Its missing fields do not establish limits of every HKT/Functor encoding. *)
Inductive FunctorHKTWitness : VerificationRequirement -> Prop :=
  | FunctorIntentOrder : FunctorHKTWitness ReqIntentOrder.

Theorem functor_hkt_not_adequate : ~ AdequateEvidence FunctorHKTWitness.
Proof.
  unfold AdequateEvidence. intro H.
  specialize (H ReqAuthority). inversion H.
Qed.

(* Extensional vocabulary contract: AdequateEvidence means every field holds,
   and AIRWitness has a constructor for every field. This is definition-level
   coverage, not implementation adequacy or architectural minimality. *)
Theorem air_is_minimal_witness_set :
  AdequateEvidence AIRWitness /\
  forall S : EvidenceSurface,
    (forall r, S r -> AIRWitness r) ->
    AdequateEvidence S ->
    forall r, S r <-> AIRWitness r.
Proof.
  split.
  - apply air_witness_adequate.
  - intros S Hsubset Hadequate r. split.
    + apply Hsubset.
    + intro Hair. apply Hadequate.
Qed.

(* ========================================== *)
(* 5. Out of scope (honest)                    *)
(* ------------------------------------------ *)
(* - AIR witness rows fix an interface only, not a minimal implementation or
      non-library-expressibility result.
   - This does not rule out a different fact factoring with a shorter codegen
      chain; it proves minimality for the current fact-sets and their real
      dependencies.                                                              *)
(* ========================================== *)
