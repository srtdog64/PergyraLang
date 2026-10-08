(* Permanent consumers for B-F1..B-F10 and C5/C6. These use actual transitions,
   not post-state assumptions. Product refinement is deliberately separate. *)
Require Import Stdlib.Lists.List Stdlib.Arith.PeanoNat Stdlib.micromega.Lia.
Require Import PergyraCore UnifiedCore WholeProgramCore MachineLayerCore.
Require Import CapabilityFlowCore ModuleAuthority BindingIdentityScope.
Require Import PartySlotBinding EvidenceLifecycleCore AuthorityIrreducibility.
Require Import AxisOwnership ReadingConfluence.
Require Import AIRBinding BinaryAdequacy.
Import ListNotations.
Module PC := PergyraCore.
Module UC := UnifiedCore.
Module WC := WholeProgramCore.
Module ML := MachineLayerCore.
Module CF := CapabilityFlowCore.
Module MA := ModuleAuthority.
Module BI := BindingIdentityScope.
Module PB := PartySlotBinding.
Module EL := EvidenceLifecycleCore.
Module AI := AuthorityIrreducibility.
Module AO := AxisOwnership.
Module RC := ReadingConfluence.
Module AB := AIRBinding.
Module BA := BinaryAdequacy.

Definition pc_seed := PC.mkConfig 0 (fun _ => [7]) 0 [] (fun _ => PC.Filled).
Definition pc_emit := PC.with_emit pc_seed 0.
Definition pc_release := PC.with_store pc_emit 0 PC.Released.
Definition pc_rollback := PC.with_rollback pc_release [0] (PC.store pc_seed) [].

Lemma pc_release_rollback_trace :
  PC.steps (fun _ => 7) (fun _ => 7) (fun _ => 7) (fun _ => [0])
    pc_seed pc_rollback.
Proof.
  eapply PC.SStep.
  - apply (PC.SEmit _ _ _ _ pc_seed 0). left. reflexivity.
  - eapply PC.SStep.
    + apply (PC.SRelease _ _ _ _ pc_emit 0). reflexivity.
    + eapply PC.SStep.
      * apply (PC.SRollback _ _ _ _ pc_release 0 (PC.store pc_seed) []).
        -- reflexivity.
        -- constructor; [left; reflexivity | constructor].
      * apply PC.SRefl.
Qed.

Example rollback_cannot_resurrect_or_double_release :
  PC.store pc_rollback 0 = PC.Released /\
  (forall next, ~ PC.step (fun _ => 7) (fun _ => 7) (fun _ => 7)
      (fun _ => [0]) (PC.ActUse 0) pc_rollback next) /\
  (forall next, ~ PC.step (fun _ => 7) (fun _ => 7) (fun _ => 7)
      (fun _ => [0]) (PC.ActRelease 0) pc_rollback next).
Proof.
  split; [reflexivity |]. split; intros next Hstep; inversion Hstep;
    subst; cbn in *; discriminate.
Qed.

Definition pc_empty := PC.mkConfig 0 (fun _ => [7]) 0 [] (fun _ => PC.Empty).
Definition pc_empty_emit := PC.with_emit pc_empty 0.
Definition pc_acquire := PC.with_store pc_empty_emit 0 PC.Filled.
Definition pc_acquire_rollback :=
  PC.with_rollback pc_acquire [0] (PC.store pc_empty) [].

Lemma pc_acquire_rollback_trace :
  PC.steps (fun _ => 7) (fun _ => 7) (fun _ => 7) (fun _ => [0])
    pc_empty pc_acquire_rollback.
Proof.
  eapply PC.SStep.
  - apply (PC.SEmit _ _ _ _ pc_empty 0). left. reflexivity.
  - eapply PC.SStep.
    + apply (PC.SAcquire _ _ _ _ pc_empty_emit 0).
      * left. reflexivity.
      * reflexivity.
    + eapply PC.SStep.
      * apply (PC.SRollback _ _ _ _ pc_acquire 0 (PC.store pc_empty) []).
        -- reflexivity.
        -- constructor; [left; reflexivity | constructor].
      * apply PC.SRefl.
Qed.

Example rollback_cannot_reacquire_allocation : forall next,
  ~ PC.step (fun _ => 7) (fun _ => 7) (fun _ => 7) (fun _ => [0])
      (PC.ActAcquire 0) pc_acquire_rollback next.
Proof. intros next Hstep. inversion Hstep; subst; cbn in *; discriminate. Qed.

(* B-F6: no bulk constructor invents capabilities. The synthesis consumer
   supplies an actual SDelegate edge, then builds the gated rollback edge. *)
Example rollback_synthesis_has_real_delegation_source :
  PC.step (fun _ => 7) (fun _ => 7) (fun _ => 7) (fun _ => [0])
    PC.ActRollback (PC.with_deleg pc_emit 1 7)
    (PC.with_rollback (PC.with_deleg pc_emit 1 7) [0] (PC.store pc_seed) []).
Proof.
  apply (proj1 (UC.delegation_furnishes_gated_rollback
    (fun _ => 7) (fun _ => 7) (fun _ => 7) (fun _ => [0])
    pc_emit (PC.with_deleg pc_emit 1 7) 1 7 0 (PC.store pc_seed) []
    ltac:(apply PC.SDelegate; left; reflexivity)
    eq_refl ltac:(constructor; [left; reflexivity | constructor]))).
Qed.

Example unheld_authority_cannot_delegate : forall next,
  ~ PC.step (fun _ => 7) (fun _ => 7) (fun _ => 7) (fun _ => [0])
    (PC.ActDelegate 1 7)
    (PC.mkConfig 0 (fun _ => []) 0 [] (fun _ => PC.Empty)) next.
Proof. intros next Hstep. inversion Hstep; subst. contradiction. Qed.

Definition wc_seed := WC.mkConfig 0 (fun _ => [7]) 0 [] (fun _ => WC.Filled) [].
Definition wc_emit := WC.with_emit wc_seed 0.
Definition wc_release := WC.with_store wc_emit 0 WC.Released.
Definition wc_rollback := WC.with_rollback wc_release [0] (WC.store wc_seed) [].

Lemma wc_release_rollback_trace :
  WC.steps (fun _ => 7) (fun _ => 7) (fun _ => 7) (fun _ => [0]) (fun _ => [])
    wc_seed wc_rollback.
Proof.
  eapply WC.SStep.
  - apply (WC.SEmit _ _ _ _ _ wc_seed 0). left. reflexivity.
  - eapply WC.SStep.
    + apply (WC.SRelease _ _ _ _ _ wc_emit 0). reflexivity.
    + eapply WC.SStep.
      * apply (WC.SRollback _ _ _ _ _ wc_release 0 (WC.store wc_seed) []).
        -- reflexivity.
        -- constructor; [left; reflexivity | constructor].
      * apply WC.SRefl.
Qed.

Example whole_program_released_run_stays_released :
  WC.store wc_rollback 0 = WC.Released /\
  WC.WF (fun _ => []) wc_rollback /\
  (forall next, ~ WC.step (fun _ => 7) (fun _ => 7) (fun _ => 7)
    (fun _ => [0]) (fun _ => []) (WC.ActUse 0) wc_rollback next).
Proof.
  split; [reflexivity |]. split.
  - eapply WC.steps_preserve_wf; [| exact wc_release_rollback_trace].
    intros t Hin. contradiction.
  - intros next Hstep. inversion Hstep; subst; cbn in *; discriminate.
Qed.

(* A-F10 reached consumer: the integrated machine and its actual imported
   AIR boolean verdict both reject replay after a real first Run transition. *)
Definition wc_run_once := WC.with_run wc_seed 0.
Definition wc_run_air :=
  AB.mkAIR (fun _ => 7) (fun _ => 7) (fun _ => 7) (fun _ => [0]) (fun _ => []).

Lemma wc_run_edge :
  WC.step (AB.air_zone_gate wc_run_air) (AB.air_effect_gate wc_run_air)
    (AB.air_acquire_gate wc_run_air) (AB.air_comp_targets wc_run_air)
    (AB.air_dep_graph wc_run_air) (WC.ActRun 0) wc_seed wc_run_once.
Proof.
  apply WC.SRun. split.
  - simpl. tauto.
  - intros x Hx. contradiction.
Qed.

Example integrated_run_then_run_rejected :
  BA.accept wc_run_air (WC.ActRun 0) wc_seed = true /\
  BA.accept wc_run_air (WC.ActRun 0) wc_run_once = false /\
  ~ AB.guard_air wc_run_air (WC.ActRun 0) wc_run_once /\
  (forall next, ~ WC.step (AB.air_zone_gate wc_run_air)
    (AB.air_effect_gate wc_run_air) (AB.air_acquire_gate wc_run_air)
    (AB.air_comp_targets wc_run_air) (AB.air_dep_graph wc_run_air)
    (WC.ActRun 0) wc_run_once next) /\
  NoDup (WC.done wc_run_once).
Proof.
  split; [reflexivity |]. split; [reflexivity |]. split.
  - apply BA.reject_adequate. reflexivity.
  - split.
    + intros next. eapply WC.run_then_run_refused;
        [exact wc_run_edge | apply WC.SRefl].
    + eapply WC.step_preserves_done_nodup; [| exact wc_run_edge]. constructor.
Qed.

(* An empty region can be well-grounded at an endpoint, yet cannot contact
   even one cell. The contact guard, not region_valid alone, owns this fact. *)
Definition endpoint_region := ML.mkRegion 16 0 ML.Plain 1.
Example zero_endpoint_region_is_valid :
  ML.region_valid (ML.md_grants ML.sample_declaration) endpoint_region.
Proof.
  exists ML.sample_grant. split; [left; reflexivity |].
  split; [reflexivity |]. split; [reflexivity |]. split; simpl; lia.
Qed.

Example zero_endpoint_cannot_write_neighbour : forall next,
  ~ ML.contact_step ML.sample_declaration (ML.ContactWrite 99)
      ML.sample_config endpoint_region next.
Proof.
  intros next. apply ML.zero_extent_contact_fail_closed;
    [discriminate | reflexivity].
Qed.

Example zero_extent_all_addressed_ops_refused : forall d op c r next,
  op <> ML.ContactFence -> ML.r_size r = 0 ->
  ~ ML.contact_step d op c r next.
Proof. exact ML.zero_extent_contact_fail_closed. Qed.

(* A valid nonempty write remains possible and cannot touch its neighbouring
   address at 16. Both premises are tied to the actual admitted contact. *)
Definition last_cell := ML.mkRegion 15 1 ML.Plain 1.
Lemma last_cell_contact :
  ML.contact_step ML.sample_declaration (ML.ContactWrite 99) ML.sample_config
    last_cell (ML.contact_apply ML.sample_config (ML.ContactWrite 99) last_cell).
Proof.
  apply ML.ContactStep.
  - exists ML.sample_grant. split; [left; reflexivity |].
    split; [reflexivity |]. split; [reflexivity |]. split; simpl; lia.
  - simpl. lia.
  - apply ML.valid_region_has_declared_hardware_adequacy.
    exists ML.sample_grant. split; [left; reflexivity |].
    split; [reflexivity |]. split; [reflexivity |]. split; simpl; lia.
  - left. reflexivity.
  - reflexivity.
  - exact I.
Qed.

Example admitted_write_preserves_neighbour :
  ML.cc_memory (ML.contact_apply ML.sample_config (ML.ContactWrite 99) last_cell) 16
    = ML.cc_memory ML.sample_config 16.
Proof.
  eapply ML.contact_write_preserves_outside_grant;
    [exact last_cell_contact | left; reflexivity | reflexivity | right; simpl; lia].
Qed.

Definition cf_lent := CF.mkCfg (CF.manifest CF.seed)
  (CF.upd (CF.upd (CF.hold CF.seed) 1
    (CF.mask_and_not (CF.hold CF.seed 1) (CF.only 0))) 2 (CF.only 0))
  (CF.upd (CF.owner_of CF.seed) 2 (Some 1))
  (CF.upd (CF.borrowed CF.seed) 2 (CF.only 0)).
Definition cf_shared := CF.mkCfg (CF.manifest cf_lent)
  (CF.upd (CF.hold cf_lent) 3
    (CF.mask_and_not (CF.hold cf_lent 2) (CF.borrowed cf_lent 2)))
  (CF.upd (CF.owner_of cf_lent) 3 (Some 2)) (CF.borrowed cf_lent).
Definition cf_returned := CF.mkCfg (CF.manifest cf_shared)
  (CF.upd (CF.upd (CF.hold cf_shared) 1
     (CF.mask_or (CF.hold cf_shared 1) (CF.borrowed cf_shared 2)))
     2 (CF.mask_and_not (CF.hold cf_shared 2) (CF.borrowed cf_shared 2)))
  (CF.owner_of cf_shared) (CF.upd (CF.borrowed cf_shared) 2 CF.mask_none).

Lemma cf_lend_edge : CF.step CF.seed cf_lent.
Proof.
  apply CF.step_lend.
  - unfold CF.fresh, CF.seed; simpl. repeat split; intros; try reflexivity;
      discriminate.
  - discriminate.
  - intros c Hc. unfold CF.only in Hc. apply Nat.eqb_eq in Hc. subst.
    reflexivity.
  - intros c Hc. reflexivity.
Qed.

Lemma cf_share_edge : CF.step cf_lent cf_shared.
Proof.
  apply CF.step_share.
  - unfold CF.fresh, cf_lent, CF.seed, CF.upd; simpl.
    split; [intros; reflexivity |]. split; [reflexivity |].
    split; [intros; reflexivity |]. intros r Howner.
    destruct (Nat.eqb r 2); discriminate.
  - discriminate.
Qed.

Lemma cf_return_edge : CF.step cf_shared cf_returned.
Proof. apply CF.step_return; [reflexivity | discriminate]. Qed.

Example borrowed_share_return_does_not_leave_grandchild_capability :
  CF.steps CF.seed cf_returned /\
  CF.hold cf_lent 2 0 = true /\ CF.hold cf_shared 3 0 = false /\
  CF.hold cf_returned 1 0 = true /\ CF.hold cf_returned 2 0 = false /\
  CF.hold cf_returned 3 0 = false /\ CF.wf cf_returned.
Proof.
  assert (Hrun : CF.steps CF.seed cf_returned).
  { eapply CF.steps_trans; [exact cf_lend_edge |].
    eapply CF.steps_trans; [exact cf_share_edge |].
    eapply CF.steps_trans; [exact cf_return_edge | apply CF.steps_refl]. }
  split; [exact Hrun |]. repeat split; try reflexivity.
  all: try (exact (proj1 (CF.run_wf CF.seed cf_returned CF.seed_wf Hrun))).
  all: try (exact (proj1 (proj2 (CF.run_wf CF.seed cf_returned CF.seed_wf Hrun)))).
  all: try (exact (proj1 (proj2 (proj2 (CF.run_wf CF.seed cf_returned CF.seed_wf Hrun))))).
  all: try (exact (proj2 (proj2 (proj2 (CF.run_wf CF.seed cf_returned CF.seed_wf Hrun))))).
Qed.

Example module_requester_and_registry_are_load_bearing :
  MA.resolve MA.no_authority_registry
    [MA.mkModule [] [] [] []; MA.mkModule [] [5] [] []] 0 5 = None /\
  MA.resolve MA.no_authority_registry
    [MA.mkModule [] [5] [] []; MA.mkModule [0] [] [] []] 1 5 = Some 0 /\
  MA.link_wf MA.no_authority_registry [MA.mkModule [] [] [7] [7]] = false /\
  MA.link_wf (fun _ => Some 0)
    [MA.mkModule [] [] [7] [7]; MA.mkModule [] [] [7] [7]] = false.
Proof. vm_compute. repeat split; reflexivity. Qed.

Example same_depth_sibling_party_cannot_borrow :
  PB.live BI.tree 2 2 /\ ~ PB.live BI.tree 1 2.
Proof.
  split; [constructor |]. intros Hlive.
  exact (BI.sibling_scopes_apart 2 Hlive (BI.enc_refl BI.tree 2)).
Qed.

Example visibility_follows_actual_parent_not_example_ids :
  BI.visible_ids BI.tree 3
    [{| BI.d_name := 9; BI.d_id := 99; BI.d_scope := 0 |};
     {| BI.d_name := 9; BI.d_id := 88; BI.d_scope := 2 |}] 9 3 = [99] /\
  BI.visible_ids (fun s => if Nat.eqb s 40 then Some 11 else None) 1
    [{| BI.d_name := 9; BI.d_id := 77; BI.d_scope := 11 |}] 9 40 = [77].
Proof. vm_compute. split; reflexivity. Qed.

Definition receipt_ceremony :=
  {| EL.admitted := true; EL.before_last_consumer := false;
     EL.authority_required := true; EL.runtime_required := false;
     EL.diagnostic_required := false; EL.would_redecide_without_carrier := false |}.
Definition receipt_needed :=
  {| EL.admitted := true; EL.before_last_consumer := false;
     EL.authority_required := true; EL.runtime_required := false;
     EL.diagnostic_required := false; EL.would_redecide_without_carrier := true |}.

Example receipt_generation_consumes_justification_verdict :
  EL.disposition (EL.project_evidence EL.ValidityEvidence receipt_ceremony) = EL.Reference /\
  EL.carries_established_authority
    (EL.project_evidence EL.ValidityEvidence receipt_ceremony) = true /\
  EL.disposition (EL.project_evidence EL.ValidityEvidence receipt_needed) = EL.Summarize.
Proof. repeat split; reflexivity. Qed.

Example unrestricted_authority_pair_is_not_language_evidence :
  ~ AI.grant_consistent AI.c_ungranted 0.
Proof. exact AI.old_ungranted_pair_is_inconsistent. Qed.

Example duplicate_reading_producers_are_detectable :
  RC.read_producers (fun _ _ => true)
    (fun a _ => match a with AO.AxDomain => 1 | _ => 2 end)
    [AO.AxDomain; AO.AxResource; AO.AxExecution; AO.AxTypeContract] AO.FWho <>
  RC.read_producers (fun _ _ => true)
    (fun a _ => match a with AO.AxDomain => 1 | _ => 2 end)
    [AO.AxResource; AO.AxDomain; AO.AxExecution; AO.AxTypeContract] AO.FWho.
Proof. simpl. discriminate. Qed.

Print Assumptions pc_release_rollback_trace.
Print Assumptions rollback_cannot_resurrect_or_double_release.
Print Assumptions borrowed_share_return_does_not_leave_grandchild_capability.
Print Assumptions module_requester_and_registry_are_load_bearing.
Print Assumptions integrated_run_then_run_rejected.
