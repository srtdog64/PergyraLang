(*
  Pergyra Formal Semantics -- Resource/Machine Bridge

  Status: proof-sketch; not whole-language verification.
  Budget: 0 admits / 0 axioms.

  Resource/Slot facts and Machine Layer facts are complementary, not aliases.
  The resource side owns who may use which logical resource. The machine side
  owns where and how physical contact occurs. A grounded contact requires an
  explicit projection binding and evidence from both sides.

  MachineLayerCore.v remains the owner of the detailed Region/contact_step
  transition model. This file only proves the cross-layer non-inference and
  complete-binding contract; it does not duplicate the machine transition.
*)

Section ResourceMachineBridge.

Definition Principal := nat.
Definition Capability := nat.
Definition ResourceId := nat.
Definition Address := nat.
Definition AccessModeId := nat.

Record ResourceGrant := mkResourceGrant {
  resource_id : ResourceId;
  resource_principal : Principal;
  resource_capability : Capability
}.

Record MachinePlacement := mkMachinePlacement {
  machine_address : Address;
  machine_extent : nat;
  machine_mode : AccessModeId
}.

Record ProjectionBinding := mkProjectionBinding {
  binding_resource : ResourceId;
  binding_address : Address;
  binding_extent : nat;
  binding_mode : AccessModeId
}.

Definition ResourceAuthorized
  (grant : ResourceGrant) (principal : Principal)
  (capability : Capability) : Prop :=
  resource_principal grant = principal /\
  resource_capability grant = capability.

Definition MachineWitness (placement : MachinePlacement) : Prop :=
  machine_extent placement > 0.

Definition ProjectionBinds
  (grant : ResourceGrant) (placement : MachinePlacement)
  (binding : ProjectionBinding) : Prop :=
  binding_resource binding = resource_id grant /\
  binding_address binding = machine_address placement /\
  binding_extent binding = machine_extent placement /\
  binding_mode binding = machine_mode placement.

Inductive GroundedContact
  (grant : ResourceGrant) (placement : MachinePlacement)
  (binding : ProjectionBinding) (principal : Principal)
  (capability : Capability) : Prop :=
  | grounded_contact :
      ResourceAuthorized grant principal capability ->
      MachineWitness placement ->
      ProjectionBinds grant placement binding ->
      GroundedContact grant placement binding principal capability.

Theorem grounded_contact_requires_resource_authority :
  forall grant placement binding principal capability,
    GroundedContact grant placement binding principal capability ->
    ResourceAuthorized grant principal capability.
Proof.
  intros grant placement binding principal capability Hcontact.
  inversion Hcontact; assumption.
Qed.

Theorem grounded_contact_requires_machine_evidence :
  forall grant placement binding principal capability,
    GroundedContact grant placement binding principal capability ->
    MachineWitness placement.
Proof.
  intros grant placement binding principal capability Hcontact.
  inversion Hcontact; assumption.
Qed.

Theorem grounded_contact_requires_explicit_projection :
  forall grant placement binding principal capability,
    GroundedContact grant placement binding principal capability ->
    ProjectionBinds grant placement binding.
Proof.
  intros grant placement binding principal capability Hcontact.
  inversion Hcontact; assumption.
Qed.

Definition resource_example : ResourceGrant := mkResourceGrant 5 1 7.
Definition placement_left : MachinePlacement := mkMachinePlacement 100 8 0.
Definition placement_right : MachinePlacement := mkMachinePlacement 200 8 0.

Theorem resource_identity_does_not_determine_machine_address :
  resource_id resource_example = resource_id resource_example /\
  machine_address placement_left <> machine_address placement_right.
Proof.
  simpl.
  split; discriminate || reflexivity.
Qed.

Definition resource_alice : ResourceGrant := mkResourceGrant 5 1 7.
Definition resource_bob : ResourceGrant := mkResourceGrant 5 2 7.
Definition shared_placement : MachinePlacement := mkMachinePlacement 100 8 0.

Theorem machine_address_does_not_determine_resource_authority :
  machine_address shared_placement = machine_address shared_placement /\
  resource_principal resource_alice <> resource_principal resource_bob.
Proof.
  simpl.
  split; discriminate || reflexivity.
Qed.

End ResourceMachineBridge.
