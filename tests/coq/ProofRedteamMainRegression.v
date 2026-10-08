(* Independent typed consumers of repaired edges and honest claim boundaries. *)
Require Import Stdlib.Lists.List.
Require Import Stdlib.micromega.Lia.
Require Import GuardWitnessBinding IRMinimality IntentObligations.
Require Import WholeProgramCore AIRBinding BinaryAdequacy.
Import ListNotations.

Example division_overflow_class_not_unique :
  In PcArithmeticOverflow (witnesses OpDiv) /\
  In PcArithmeticOverflow (witnesses OpAddMul) /\ OpDiv <> OpAddMul :=
  panic_class_does_not_identify_operation.

Definition all_real_reads :
  Reads NRIR NHIR /\ Reads NMIR NHIR /\ Reads NMIR NRIR /\
  Reads NDIR NHIR /\ Reads NMIR NDIR.
Proof. repeat split; constructor. Qed.

Definition audit_domain_chain : forall L, Valid L ->
  L NHIR < L NDIR /\ L NDIR < L NMIR := domain_chain.
Definition audit_rir_deferral_is_insufficient : ~ ValidD L2 :=
  two_layers_refused_when_only_rir_deferred.

Definition audit_checked_emission : forall emitted,
  emitted_allb emitted = true <-> all_verifier_families_emitted emitted :=
  intent_binder_emits_all_verifier_families.

Example missing_compensation_is_not_a_binder_certificate :
  binder_strength [VFParticipant; VFCoordination; VFBoundary; VFAuthority; VFEffect] = None :=
  compensation_missing_is_refused.
Example empty_binder_certificate_is_refused : binder_strength [] = None :=
  absent_emission_is_refused.
Example complete_vocabulary_is_admitted :
  binder_strength required_verifier_families = Some ClaimNonLibraryExpressible :=
  complete_emission_is_admitted.

Definition fixed_air : AIRFacts :=
  mkAIR (fun _ => 0) (fun _ => 0) (fun _ => 0) (fun _ => []) (fun _ => []).
Definition config_with_cap : WholeProgramCore.config :=
  mkConfig 0 (fun _ => [0]) 0 [] (fun _ => Empty) [].
Definition config_without_cap : WholeProgramCore.config :=
  mkConfig 0 (fun _ => []) 0 [] (fun _ => Empty) [].

(* Identical AIR is NOT sufficient: admitted current state is also required. *)
Example same_air_different_config_changes_admission :
  guard_air fixed_air (ActCross 0) config_with_cap /\
  ~ guard_air fixed_air (ActCross 0) config_without_cap.
Proof. cbn. split; [left; reflexivity|intros H; exact H]. Qed.

Example same_air_different_store_changes_admission :
  guard_air fixed_air (ActUse 0) (with_store config_with_cap 0 Filled) /\
  ~ guard_air fixed_air (ActUse 0) (with_store config_with_cap 0 Released).
Proof. cbn. split; [reflexivity|discriminate]. Qed.

Example completed_task_refused_by_integrated_boolean :
  accept fixed_air (ActRun 0) (with_run config_with_cap 0) = false.
Proof. reflexivity. Qed.

Print Assumptions audit_checked_emission.
Print Assumptions audit_domain_chain.
Print Assumptions same_air_different_config_changes_admission.
