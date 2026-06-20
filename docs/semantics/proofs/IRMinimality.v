(*
  Pergyra Formal Semantics - Mechanized Sketch
  Target: minimality of the codegen IR-layer decomposition.
  Status: proof-sketch; not beta-closure evidence unless checked by CI (coqc).

  Question (architectural): is the IR decomposition HIR/DIR/RIR/MIR over-
  decomposed, or is it the minimum number of layers the dependencies allow?
  "Minimal" is made precise here as: with respect to the real reads-from
  dependency among the IRs, no *valid* layering (one where an IR is in a strictly
  earlier layer than anything that reads its completed output) uses fewer layers.
  By Mirsky's theorem the minimum equals the longest reads-from chain.

  Grounded edges (driver_app.c order; verified by grep):
    - hir_lower(ast), dir_lower(ast), rir_lower(ast)   -- AST-derived
    - rir_enrich_scope_with_hir_flow(scope, hir)        -- RIR reads HIR flow
    - mir_lower(hir, rir)                               -- MIR fuses HIR + RIR
    - CODEGEN (the src/codegen tree) references to DIR and AIR: ZERO. DIR and
      AIR are verification IRs OFF the codegen path; they add no codegen layer.

  Negative scope: this proves minimality W.R.T. THIS fact/dependency model. It
  does NOT prove that some entirely different IR design could not be simpler --
  you cannot quantify over all possible designs in this model. It proves the
  current fact-sets cannot be carried in fewer layers given their real reads-from
  edges, and it pinpoints the single collapsible boundary.
*)

Require Import Coq.Init.Nat.
Require Import Coq.Arith.PeanoNat.
Require Import Coq.micromega.Lia.

(* The codegen-relevant IRs. (AST and the backend are endpoints, not layers.) *)
Inductive Node : Type := NHIR | NDIR | NRIR | NMIR.

(* The real construction reads-from edges. *)
Inductive Reads : Node -> Node -> Prop :=
  | ReadsRIR_HIR : Reads NRIR NHIR    (* RIR enriched with HIR flow *)
  | ReadsMIR_HIR : Reads NMIR NHIR    (* MIR fuses HIR *)
  | ReadsMIR_RIR : Reads NMIR NRIR.   (* MIR fuses RIR *)

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
(* 2. Upper bound: 3 layers suffice, and DIR   *)
(* co-locates with HIR (adds no layer).        *)
(* ========================================== *)

Definition L3 (n : Node) : nat :=
  match n with NHIR => 0 | NDIR => 0 | NRIR => 1 | NMIR => 2 end.

Theorem three_layers_suffice : Valid L3.
Proof. intros a b HR. destruct HR; simpl; lia. Qed.

(* DIR shares HIR's layer in a valid layering: nothing on the codegen path reads
   DIR and DIR reads only AST, so it never forces its own layer. *)
Theorem dir_colocates_with_hir : L3 NDIR = L3 NHIR.
Proof. reflexivity. Qed.

(* Combined: the minimum number of codegen IR layers is exactly 3 -- the current
   decomposition (HIR/RIR/MIR; DIR off-path) is minimal, not over-decomposed. *)
Theorem codegen_minimum_is_three :
  Valid L3 /\ (forall L, Valid L ->
    L NHIR <> L NRIR /\ L NRIR <> L NMIR /\ L NHIR <> L NMIR).
Proof. split. apply three_layers_suffice. apply codegen_needs_three. Qed.

(* ========================================== *)
(* 3. The single pivot: the RIR<-HIR edge      *)
(* ========================================== *)

(* The only thing forcing 3 rather than 2 is RIR depending on HIR (its flow
   enrichment). Model RIR's flow-sensitivity deferred to MIR -- drop that edge: *)
Inductive ReadsDeferred : Node -> Node -> Prop :=
  | DReadsMIR_HIR : ReadsDeferred NMIR NHIR
  | DReadsMIR_RIR : ReadsDeferred NMIR NRIR.

Definition ValidD (L : Layering) : Prop :=
  forall a b, ReadsDeferred a b -> L b < L a.

Definition L2 (n : Node) : nat :=
  match n with NHIR => 0 | NDIR => 0 | NRIR => 0 | NMIR => 1 end.

(* Without the RIR<-HIR edge, HIR and RIR are independent and a 2-layering is
   valid: the decomposition would collapse to 2 codegen IRs. *)
Theorem two_layers_suffice_when_deferred : ValidD L2.
Proof. intros a b HR. destruct HR; simpl; lia. Qed.

(* So the codegen layer count is decided by exactly ONE architectural fact: is
   RIR's resource analysis genuinely flow-sensitive (must read HIR's CFG -> 3
   layers, current), or can that flow-sensitivity be deferred into MIR's
   fusion (-> 2 layers)? Every other boundary is forced. That single, precise
   question is where "could it be smaller?" lives -- nowhere else.

   RESOLVED (see IRMinimality.md SS5): the RIR<-HIR edge is NOT a convenience.
   rir_enrich_scope_with_hir_flow runs an RPO-fixpoint dataflow over the HIR CFG
   and rir_validation.c merges resource states across control flow to detect
   conflicts, with rir_validate running BEFORE mir_lower. So the edge encodes a
   named invariant: *flow-sensitive resource checking happens at the resource
   layer*. Hence min=3 is the true minimum for that invariant; collapsing to 2
   relocates resource checking into MIR (a named trade, not a free win). No
   incidental layer remains -- the one removable boundary is the price of
   resource-checking-at-the-resource-layer. *)

(* ========================================== *)
(* 4. Out of scope (honest)                    *)
(* ------------------------------------------ *)
(* - Verification track (DIR, AIR): off the codegen path; their own minimality   *)
(*   (DIR feeds AIR, AIR reads all four) is a separate 2-vs-1 question, not       *)
(*   codegen depth.                                                               *)
(* - This does not rule out a different fact factoring with a shorter chain; it   *)
(*   proves minimality for the current fact-sets and their real dependencies.     *)
(* ========================================== *)
