Require Import OwnershipRefusalExplanation OwnershipRefusalExplanationAudit.
Require Import Stdlib.extraction.Extraction Stdlib.extraction.ExtrOcamlBasic.
Import RefusalExplanation.

(* Keep inductive naturals. No unchecked machine-int encoding, overflow or
   negative-index interpretation is added to the reference checker here. *)
Definition extraction_sound : forall p s requested w,
  check_witness p s requested w = true -> ValidWitness p s requested w := audit_sound.
Definition extraction_complete : forall p s requested w,
  ValidWitness p s requested w -> check_witness p s requested w = true := audit_complete.
Definition extraction_refusal : forall p s requested,
  refusal p s requested = true -> exists w, check_witness p s requested w = true := audit_refusal_cause.
Extraction Language OCaml.
Extraction "ownership_refusal.ml" check_witness refusal check_tree run rounds
  witness_support.
