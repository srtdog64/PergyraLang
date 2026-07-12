(*
  Pergyra single-source-of-truth authority model.

  Scope: one finite hard-substitution rung. This model proves that a closed
  rung has one authority producer for every required semantic fact, that every
  semantic consumer reads that authority, and that a compatibility bridge with
  a fallback read is not closed.

  It does not prove that the whole compiler has no future SoT seams. The live
  adequacy gate binds the concrete bounded instances below to source.
*)

Inductive Fact : Type :=
  | FInitializerArrayBody
  | FInitializerTryOperand
  | FCollectionMutationParts
  | FEnumDeclarationRows
  | FNominalDeclarationRows
  | FRoleDeclarationRows
  | FExpressionRuntimeUsageSurface
  | FInitializerTextProvenance.

Inductive FactClass : Type :=
  | SemanticFact
  | ProvenanceFact.

Inductive Owner : Type :=
  | OSemanticLocalBindingFacts
  | OSemanticStatementFacts
  | OSemanticEnumFacts
  | OSemanticNominalConstructorFacts
  | OSemanticRoleFacts
  | OSemanticExpressionSurfaceFacts
  | OAstArenaProvenance
  | OCodegenTextRecovery.

Inductive Consumer : Type :=
  | CArrayLiteralEmitter
  | CTryLetEmitter
  | CCollectionMutationEmitter
  | CEnumEmitter
  | CNominalEmitter
  | CRoleOperatorEmitter
  | CRuntimeUsageProjection.

Inductive ReadKind : Type :=
  | OwnedRead
  | ProvenanceRead
  | FallbackRead.

Record AuthorityModel : Type := {
  fact_class : Fact -> FactClass;
  authority : Fact -> Owner;
  produces : Owner -> Fact -> Prop;
  requires_fact : Consumer -> Fact -> Prop;
  reads : Consumer -> Owner -> Fact -> ReadKind -> Prop
}.

Definition AuthorityComplete (m : AuthorityModel) : Prop :=
  forall c f,
    requires_fact m c f -> produces m (authority m f) f.

Definition AuthorityUnique (m : AuthorityModel) : Prop :=
  forall o f,
    produces m o f -> o = authority m f.

Definition RequiredFactsConsumed (m : AuthorityModel) : Prop :=
  forall c f,
    requires_fact m c f -> exists k, reads m c (authority m f) f k.

Definition NoSemanticFallback (m : AuthorityModel) : Prop :=
  forall c o f k,
    reads m c o f k ->
    fact_class m f = SemanticFact ->
    o = authority m f /\ k = OwnedRead.

Definition RungClosed (m : AuthorityModel) : Prop :=
  AuthorityComplete m /\
  AuthorityUnique m /\
  RequiredFactsConsumed m /\
  NoSemanticFallback m.

Theorem closed_required_fact_has_exactly_one_authority :
  forall m c f,
    RungClosed m ->
    requires_fact m c f ->
    produces m (authority m f) f /\
    forall o, produces m o f -> o = authority m f.
Proof.
  intros m c f [Hcomplete [Hunique _]] Hrequired.
  split.
  - apply Hcomplete with (c := c). exact Hrequired.
  - intros o Hproduces. apply Hunique. exact Hproduces.
Qed.

Theorem closed_semantic_read_is_owner_read :
  forall m c o f k,
    RungClosed m ->
    reads m c o f k ->
    fact_class m f = SemanticFact ->
    o = authority m f /\ k = OwnedRead.
Proof.
  intros m c o f k [_ [_ [_ Hno_fallback]]] Hread Hsemantic.
  apply Hno_fallback with (c := c) (f := f).
  - exact Hread.
  - exact Hsemantic.
Qed.

Theorem closed_semantic_read_is_not_fallback :
  forall m c o f k,
    RungClosed m ->
    reads m c o f k ->
    fact_class m f = SemanticFact ->
    k <> FallbackRead.
Proof.
  intros m c o f k Hclosed Hread Hsemantic Hfallback.
  destruct (closed_semantic_read_is_owner_read
    m c o f k Hclosed Hread Hsemantic) as [_ Hkind].
  rewrite Hfallback in Hkind. discriminate.
Qed.

Definition current_fact_class (f : Fact) : FactClass :=
  match f with
  | FInitializerArrayBody => SemanticFact
  | FInitializerTryOperand => SemanticFact
  | FCollectionMutationParts => SemanticFact
  | FEnumDeclarationRows => SemanticFact
  | FNominalDeclarationRows => SemanticFact
  | FRoleDeclarationRows => SemanticFact
  | FExpressionRuntimeUsageSurface => SemanticFact
  | FInitializerTextProvenance => ProvenanceFact
  end.

Definition current_authority (f : Fact) : Owner :=
  match f with
  | FInitializerArrayBody => OSemanticLocalBindingFacts
  | FInitializerTryOperand => OSemanticLocalBindingFacts
  | FCollectionMutationParts => OSemanticStatementFacts
  | FEnumDeclarationRows => OSemanticEnumFacts
  | FNominalDeclarationRows => OSemanticNominalConstructorFacts
  | FRoleDeclarationRows => OSemanticRoleFacts
  | FExpressionRuntimeUsageSurface => OSemanticExpressionSurfaceFacts
  | FInitializerTextProvenance => OAstArenaProvenance
  end.

Inductive current_produces : Owner -> Fact -> Prop :=
  | CurrentArrayBodyProducer :
      current_produces OSemanticLocalBindingFacts FInitializerArrayBody
  | CurrentTryOperandProducer :
      current_produces OSemanticLocalBindingFacts FInitializerTryOperand
  | CurrentCollectionMutationProducer :
      current_produces OSemanticStatementFacts FCollectionMutationParts
  | CurrentEnumDeclarationProducer :
      current_produces OSemanticEnumFacts FEnumDeclarationRows
  | CurrentNominalDeclarationProducer :
      current_produces OSemanticNominalConstructorFacts FNominalDeclarationRows
  | CurrentRoleDeclarationProducer :
      current_produces OSemanticRoleFacts FRoleDeclarationRows
  | CurrentExpressionRuntimeUsageProducer :
      current_produces OSemanticExpressionSurfaceFacts
        FExpressionRuntimeUsageSurface
  | CurrentProvenanceProducer :
      current_produces OAstArenaProvenance FInitializerTextProvenance.

Inductive current_requires : Consumer -> Fact -> Prop :=
  | CurrentEmitterRequiresArrayBody :
      current_requires CArrayLiteralEmitter FInitializerArrayBody.

Inductive current_reads : Consumer -> Owner -> Fact -> ReadKind -> Prop :=
  | CurrentEmitterReadsArrayBody :
      current_reads CArrayLiteralEmitter OSemanticLocalBindingFacts
        FInitializerArrayBody OwnedRead.

Definition current_model : AuthorityModel :=
  {| fact_class := current_fact_class;
     authority := current_authority;
     produces := current_produces;
     requires_fact := current_requires;
     reads := current_reads |}.

Theorem current_array_literal_rung_closed : RungClosed current_model.
Proof.
  unfold RungClosed.
  split.
  - unfold AuthorityComplete. simpl.
    intros consumer fact Hrequired. destruct Hrequired. constructor.
  - split.
    + unfold AuthorityUnique. simpl.
      intros owner fact Hproduces. destruct Hproduces; reflexivity.
    + split.
      * unfold RequiredFactsConsumed. simpl.
        intros consumer fact Hrequired. destruct Hrequired.
        exists OwnedRead. constructor.
      * unfold NoSemanticFallback. simpl.
        intros consumer owner fact kind Hread Hsemantic. destruct Hread.
        split; reflexivity.
Qed.

Inductive try_requires : Consumer -> Fact -> Prop :=
  | CurrentEmitterRequiresTryOperand :
      try_requires CTryLetEmitter FInitializerTryOperand.

Inductive try_reads : Consumer -> Owner -> Fact -> ReadKind -> Prop :=
  | CurrentEmitterReadsTryOperand :
      try_reads CTryLetEmitter OSemanticLocalBindingFacts
        FInitializerTryOperand OwnedRead.

Definition try_model : AuthorityModel :=
  {| fact_class := current_fact_class;
     authority := current_authority;
     produces := current_produces;
     requires_fact := try_requires;
     reads := try_reads |}.

Theorem current_try_let_rung_closed : RungClosed try_model.
Proof.
  unfold RungClosed.
  split.
  - unfold AuthorityComplete. simpl.
    intros consumer fact Hrequired. destruct Hrequired. constructor.
  - split.
    + unfold AuthorityUnique. simpl.
      intros owner fact Hproduces. destruct Hproduces; reflexivity.
    + split.
      * unfold RequiredFactsConsumed. simpl.
        intros consumer fact Hrequired. destruct Hrequired.
        exists OwnedRead. constructor.
      * unfold NoSemanticFallback. simpl.
        intros consumer owner fact kind Hread Hsemantic. destruct Hread.
        split; reflexivity.
Qed.

Inductive try_bridge_reads : Consumer -> Owner -> Fact -> ReadKind -> Prop :=
  | TryBridgeOwnedRead :
      try_bridge_reads CTryLetEmitter OSemanticLocalBindingFacts
        FInitializerTryOperand OwnedRead
  | TryBridgeFallbackRead :
      try_bridge_reads CTryLetEmitter OCodegenTextRecovery
        FInitializerTryOperand FallbackRead.

Definition try_bridge_model : AuthorityModel :=
  {| fact_class := current_fact_class;
     authority := current_authority;
     produces := current_produces;
     requires_fact := try_requires;
     reads := try_bridge_reads |}.

Theorem try_owner_plus_text_fallback_is_not_closed :
  ~ RungClosed try_bridge_model.
Proof.
  intros [_ [_ [_ Hno_fallback]]].
  specialize (Hno_fallback CTryLetEmitter OCodegenTextRecovery
    FInitializerTryOperand FallbackRead TryBridgeFallbackRead eq_refl).
  destruct Hno_fallback as [Howner _]. discriminate.
Qed.

Inductive collection_requires : Consumer -> Fact -> Prop :=
  | CurrentEmitterRequiresCollectionMutationParts :
      collection_requires CCollectionMutationEmitter
        FCollectionMutationParts.

Inductive collection_reads : Consumer -> Owner -> Fact -> ReadKind -> Prop :=
  | CurrentEmitterReadsCollectionMutationParts :
      collection_reads CCollectionMutationEmitter OSemanticStatementFacts
        FCollectionMutationParts OwnedRead.

Definition collection_model : AuthorityModel :=
  {| fact_class := current_fact_class;
     authority := current_authority;
     produces := current_produces;
     requires_fact := collection_requires;
     reads := collection_reads |}.

Theorem current_collection_mutation_rung_closed :
  RungClosed collection_model.
Proof.
  unfold RungClosed.
  split.
  - unfold AuthorityComplete. simpl.
    intros consumer fact Hrequired. destruct Hrequired. constructor.
  - split.
    + unfold AuthorityUnique. simpl.
      intros owner fact Hproduces. destruct Hproduces; reflexivity.
    + split.
      * unfold RequiredFactsConsumed. simpl.
        intros consumer fact Hrequired. destruct Hrequired.
        exists OwnedRead. constructor.
      * unfold NoSemanticFallback. simpl.
        intros consumer owner fact kind Hread Hsemantic. destruct Hread.
        split; reflexivity.
Qed.

Inductive collection_bridge_reads :
  Consumer -> Owner -> Fact -> ReadKind -> Prop :=
  | CollectionBridgeOwnedRead :
      collection_bridge_reads CCollectionMutationEmitter
        OSemanticStatementFacts FCollectionMutationParts OwnedRead
  | CollectionBridgeFallbackRead :
      collection_bridge_reads CCollectionMutationEmitter
        OCodegenTextRecovery FCollectionMutationParts FallbackRead.

Definition collection_bridge_model : AuthorityModel :=
  {| fact_class := current_fact_class;
     authority := current_authority;
     produces := current_produces;
     requires_fact := collection_requires;
     reads := collection_bridge_reads |}.

Theorem collection_owner_plus_text_fallback_is_not_closed :
  ~ RungClosed collection_bridge_model.
Proof.
  intros [_ [_ [_ Hno_fallback]]].
  specialize (Hno_fallback CCollectionMutationEmitter OCodegenTextRecovery
    FCollectionMutationParts FallbackRead CollectionBridgeFallbackRead
    eq_refl).
  destruct Hno_fallback as [Howner _]. discriminate.
Qed.

Inductive enum_requires : Consumer -> Fact -> Prop :=
  | CurrentEmitterRequiresEnumDeclarationRows :
      enum_requires CEnumEmitter FEnumDeclarationRows.

Inductive enum_reads : Consumer -> Owner -> Fact -> ReadKind -> Prop :=
  | CurrentEmitterReadsEnumDeclarationRows :
      enum_reads CEnumEmitter OSemanticEnumFacts
        FEnumDeclarationRows OwnedRead.

Definition enum_model : AuthorityModel :=
  {| fact_class := current_fact_class;
     authority := current_authority;
     produces := current_produces;
     requires_fact := enum_requires;
     reads := enum_reads |}.

Theorem current_enum_declaration_rung_closed : RungClosed enum_model.
Proof.
  unfold RungClosed.
  split.
  - unfold AuthorityComplete. simpl.
    intros consumer fact Hrequired. destruct Hrequired. constructor.
  - split.
    + unfold AuthorityUnique. simpl.
      intros owner fact Hproduces. destruct Hproduces; reflexivity.
    + split.
      * unfold RequiredFactsConsumed. simpl.
        intros consumer fact Hrequired. destruct Hrequired.
        exists OwnedRead. constructor.
      * unfold NoSemanticFallback. simpl.
        intros consumer owner fact kind Hread Hsemantic. destruct Hread.
        split; reflexivity.
Qed.

Inductive enum_bridge_reads : Consumer -> Owner -> Fact -> ReadKind -> Prop :=
  | EnumBridgeOwnedRead :
      enum_bridge_reads CEnumEmitter OSemanticEnumFacts
        FEnumDeclarationRows OwnedRead
  | EnumBridgeFallbackRead :
      enum_bridge_reads CEnumEmitter OCodegenTextRecovery
        FEnumDeclarationRows FallbackRead.

Definition enum_bridge_model : AuthorityModel :=
  {| fact_class := current_fact_class;
     authority := current_authority;
     produces := current_produces;
     requires_fact := enum_requires;
     reads := enum_bridge_reads |}.

Theorem enum_owner_plus_text_fallback_is_not_closed :
  ~ RungClosed enum_bridge_model.
Proof.
  intros [_ [_ [_ Hno_fallback]]].
  specialize (Hno_fallback CEnumEmitter OCodegenTextRecovery
    FEnumDeclarationRows FallbackRead EnumBridgeFallbackRead eq_refl).
  destruct Hno_fallback as [Howner _]. discriminate.
Qed.

Inductive nominal_requires : Consumer -> Fact -> Prop :=
  | CurrentEmitterRequiresNominalDeclarationRows :
      nominal_requires CNominalEmitter FNominalDeclarationRows.

Inductive nominal_reads : Consumer -> Owner -> Fact -> ReadKind -> Prop :=
  | CurrentEmitterReadsNominalDeclarationRows :
      nominal_reads CNominalEmitter OSemanticNominalConstructorFacts
        FNominalDeclarationRows OwnedRead.

Definition nominal_model : AuthorityModel :=
  {| fact_class := current_fact_class;
     authority := current_authority;
     produces := current_produces;
     requires_fact := nominal_requires;
     reads := nominal_reads |}.

Theorem current_nominal_declaration_rung_closed : RungClosed nominal_model.
Proof.
  unfold RungClosed.
  split.
  - unfold AuthorityComplete. simpl.
    intros consumer fact Hrequired. destruct Hrequired. constructor.
  - split.
    + unfold AuthorityUnique. simpl.
      intros owner fact Hproduces. destruct Hproduces; reflexivity.
    + split.
      * unfold RequiredFactsConsumed. simpl.
        intros consumer fact Hrequired. destruct Hrequired.
        exists OwnedRead. constructor.
      * unfold NoSemanticFallback. simpl.
        intros consumer owner fact kind Hread Hsemantic. destruct Hread.
        split; reflexivity.
Qed.

Inductive nominal_bridge_reads : Consumer -> Owner -> Fact -> ReadKind -> Prop :=
  | NominalBridgeOwnedRead :
      nominal_bridge_reads CNominalEmitter OSemanticNominalConstructorFacts
        FNominalDeclarationRows OwnedRead
  | NominalBridgeFallbackRead :
      nominal_bridge_reads CNominalEmitter OCodegenTextRecovery
        FNominalDeclarationRows FallbackRead.

Definition nominal_bridge_model : AuthorityModel :=
  {| fact_class := current_fact_class;
     authority := current_authority;
     produces := current_produces;
     requires_fact := nominal_requires;
     reads := nominal_bridge_reads |}.

Theorem nominal_owner_plus_text_fallback_is_not_closed :
  ~ RungClosed nominal_bridge_model.
Proof.
  intros [_ [_ [_ Hno_fallback]]].
  specialize (Hno_fallback CNominalEmitter OCodegenTextRecovery
    FNominalDeclarationRows FallbackRead NominalBridgeFallbackRead eq_refl).
  destruct Hno_fallback as [Howner _]. discriminate.
Qed.

Inductive role_requires : Consumer -> Fact -> Prop :=
  | CurrentEmitterRequiresRoleDeclarationRows :
      role_requires CRoleOperatorEmitter FRoleDeclarationRows.

Inductive role_reads : Consumer -> Owner -> Fact -> ReadKind -> Prop :=
  | CurrentEmitterReadsRoleDeclarationRows :
      role_reads CRoleOperatorEmitter OSemanticRoleFacts
        FRoleDeclarationRows OwnedRead.

Definition role_model : AuthorityModel :=
  {| fact_class := current_fact_class;
     authority := current_authority;
     produces := current_produces;
     requires_fact := role_requires;
     reads := role_reads |}.

Theorem current_role_declaration_rung_closed : RungClosed role_model.
Proof.
  unfold RungClosed. split.
  - unfold AuthorityComplete. simpl.
    intros consumer fact Hrequired. destruct Hrequired. constructor.
  - split.
    + unfold AuthorityUnique. simpl.
      intros owner fact Hproduces. destruct Hproduces; reflexivity.
    + split.
      * unfold RequiredFactsConsumed. simpl.
        intros consumer fact Hrequired. destruct Hrequired.
        exists OwnedRead. constructor.
      * unfold NoSemanticFallback. simpl.
        intros consumer owner fact kind Hread Hsemantic. destruct Hread.
        split; reflexivity.
Qed.

Inductive role_bridge_reads : Consumer -> Owner -> Fact -> ReadKind -> Prop :=
  | RoleBridgeOwnedRead :
      role_bridge_reads CRoleOperatorEmitter OSemanticRoleFacts
        FRoleDeclarationRows OwnedRead
  | RoleBridgeFallbackRead :
      role_bridge_reads CRoleOperatorEmitter OCodegenTextRecovery
        FRoleDeclarationRows FallbackRead.

Definition role_bridge_model : AuthorityModel :=
  {| fact_class := current_fact_class;
     authority := current_authority;
     produces := current_produces;
     requires_fact := role_requires;
     reads := role_bridge_reads |}.

Theorem role_owner_plus_ast_fallback_is_not_closed :
  ~ RungClosed role_bridge_model.
Proof.
  intros [_ [_ [_ Hno_fallback]]].
  specialize (Hno_fallback CRoleOperatorEmitter OCodegenTextRecovery
    FRoleDeclarationRows FallbackRead RoleBridgeFallbackRead eq_refl).
  destruct Hno_fallback as [Howner _]. discriminate.
Qed.

Inductive expression_usage_requires : Consumer -> Fact -> Prop :=
  | RuntimeProjectionRequiresExpressionSurface :
      expression_usage_requires CRuntimeUsageProjection
        FExpressionRuntimeUsageSurface.

Inductive expression_usage_reads : Consumer -> Owner -> Fact -> ReadKind -> Prop :=
  | RuntimeProjectionReadsExpressionSurface :
      expression_usage_reads CRuntimeUsageProjection
        OSemanticExpressionSurfaceFacts FExpressionRuntimeUsageSurface OwnedRead.

Definition expression_usage_model : AuthorityModel :=
  {| fact_class := current_fact_class;
     authority := current_authority;
     produces := current_produces;
     requires_fact := expression_usage_requires;
     reads := expression_usage_reads |}.

Theorem current_expression_runtime_usage_rung_closed :
  RungClosed expression_usage_model.
Proof.
  unfold RungClosed. split.
  - unfold AuthorityComplete. simpl.
    intros consumer fact Hrequired. destruct Hrequired. constructor.
  - split.
    + unfold AuthorityUnique. simpl.
      intros owner fact Hproduces. destruct Hproduces; reflexivity.
    + split.
      * unfold RequiredFactsConsumed. simpl.
        intros consumer fact Hrequired. destruct Hrequired.
        exists OwnedRead. constructor.
      * unfold NoSemanticFallback. simpl.
        intros consumer owner fact kind Hread Hsemantic. destruct Hread.
        split; reflexivity.
Qed.

Inductive expression_usage_bridge_reads :
  Consumer -> Owner -> Fact -> ReadKind -> Prop :=
  | ExpressionUsageBridgeOwnedRead :
      expression_usage_bridge_reads CRuntimeUsageProjection
        OSemanticExpressionSurfaceFacts FExpressionRuntimeUsageSurface OwnedRead
  | ExpressionUsageBridgeFallbackRead :
      expression_usage_bridge_reads CRuntimeUsageProjection OCodegenTextRecovery
        FExpressionRuntimeUsageSurface FallbackRead.

Definition expression_usage_bridge_model : AuthorityModel :=
  {| fact_class := current_fact_class;
     authority := current_authority;
     produces := current_produces;
     requires_fact := expression_usage_requires;
     reads := expression_usage_bridge_reads |}.

Theorem expression_usage_owner_plus_ast_fallback_is_not_closed :
  ~ RungClosed expression_usage_bridge_model.
Proof.
  intros [_ [_ [_ Hno_fallback]]].
  specialize (Hno_fallback CRuntimeUsageProjection OCodegenTextRecovery
    FExpressionRuntimeUsageSurface FallbackRead
    ExpressionUsageBridgeFallbackRead eq_refl).
  destruct Hno_fallback as [Howner _]. discriminate.
Qed.

Inductive bridge_reads : Consumer -> Owner -> Fact -> ReadKind -> Prop :=
  | BridgeOwnedRead :
      bridge_reads CArrayLiteralEmitter OSemanticLocalBindingFacts
        FInitializerArrayBody OwnedRead
  | BridgeFallbackRead :
      bridge_reads CArrayLiteralEmitter OCodegenTextRecovery
        FInitializerArrayBody FallbackRead.

Definition bridge_model : AuthorityModel :=
  {| fact_class := current_fact_class;
     authority := current_authority;
     produces := current_produces;
     requires_fact := current_requires;
     reads := bridge_reads |}.

Theorem owned_plus_fallback_bridge_is_not_closed : ~ RungClosed bridge_model.
Proof.
  intros [_ [_ [_ Hno_fallback]]].
  specialize (Hno_fallback CArrayLiteralEmitter OCodegenTextRecovery
    FInitializerArrayBody FallbackRead BridgeFallbackRead eq_refl).
  destruct Hno_fallback as [Howner _]. discriminate.
Qed.

Inductive duplicate_produces : Owner -> Fact -> Prop :=
  | DuplicateSemanticProducer :
      duplicate_produces OSemanticLocalBindingFacts FInitializerArrayBody
  | DuplicateCodegenProducer :
      duplicate_produces OCodegenTextRecovery FInitializerArrayBody
  | DuplicateProvenanceProducer :
      duplicate_produces OAstArenaProvenance FInitializerTextProvenance.

Definition duplicate_owner_model : AuthorityModel :=
  {| fact_class := current_fact_class;
     authority := current_authority;
     produces := duplicate_produces;
     requires_fact := current_requires;
     reads := current_reads |}.

Theorem duplicate_semantic_producer_is_not_closed :
  ~ RungClosed duplicate_owner_model.
Proof.
  intros [_ [Hunique _]].
  specialize (Hunique OCodegenTextRecovery FInitializerArrayBody
    DuplicateCodegenProducer).
  discriminate.
Qed.

Inductive no_produces : Owner -> Fact -> Prop := .

Definition missing_fact_model : AuthorityModel :=
  {| fact_class := current_fact_class;
     authority := current_authority;
     produces := no_produces;
     requires_fact := current_requires;
     reads := current_reads |}.

Theorem missing_required_fact_is_not_closed : ~ RungClosed missing_fact_model.
Proof.
  intros [Hcomplete _].
  specialize (Hcomplete CArrayLiteralEmitter FInitializerArrayBody
    CurrentEmitterRequiresArrayBody).
  inversion Hcomplete.
Qed.

(* ================================================================ *)
(* Whole compiler-spine owner declaration.                           *)
(* This fixes owner identity; it does not assert implementation      *)
(* closure for every row. Closure remains a per-rung predicate above. *)
(* ================================================================ *)

Inductive SpineFact : Type :=
  | SFSourceModuleGraph
  | SFTokenStream
  | SFSyntaxProvenanceTree
  | SFSemanticSymbolTypeGraph
  | SFHirTypedControlFlow
  | SFDirDomainGraph
  | SFRirResourceTransitionGraph
  | SFMirExecutionGraph
  | SFAirEvidenceGraph
  | SFAbiLayoutRows
  | SFTargetCapabilityProfile
  | SFProjectionPlan
  | SFDiagnosticCatalog
  | SFBackendArtifact
  | SFCompatibilityEvolution
  | SFInitializerExpressionShape
  | SFCollectionMutationStatement
  | SFEnumDeclarationRows
  | SFNominalDeclarationRows
  | SFRoleDeclarationRows
  | SFExpressionRuntimeUsageSurface.

Inductive SpineOwner : Type :=
  | SOModuleLoader
  | SOLexer
  | SOParserAst
  | SOSemanticAnalyzer
  | SOHir
  | SODir
  | SORir
  | SOMir
  | SOAir
  | SOMirAbi
  | SOTargetCapability
  | SOProjectionPlanner
  | SODiagnosticCatalog
  | SOArtifactZone
  | SOCompatibilityEvolution
  | SOSemanticLocalBinding
  | SOSemanticStatement
  | SOSemanticEnum
  | SOSemanticNominalConstructor
  | SOSemanticRole
  | SOSemanticExpressionSurface.

Definition spine_authority (fact : SpineFact) : SpineOwner :=
  match fact with
  | SFSourceModuleGraph => SOModuleLoader
  | SFTokenStream => SOLexer
  | SFSyntaxProvenanceTree => SOParserAst
  | SFSemanticSymbolTypeGraph => SOSemanticAnalyzer
  | SFHirTypedControlFlow => SOHir
  | SFDirDomainGraph => SODir
  | SFRirResourceTransitionGraph => SORir
  | SFMirExecutionGraph => SOMir
  | SFAirEvidenceGraph => SOAir
  | SFAbiLayoutRows => SOMirAbi
  | SFTargetCapabilityProfile => SOTargetCapability
  | SFProjectionPlan => SOProjectionPlanner
  | SFDiagnosticCatalog => SODiagnosticCatalog
  | SFBackendArtifact => SOArtifactZone
  | SFCompatibilityEvolution => SOCompatibilityEvolution
  | SFInitializerExpressionShape => SOSemanticLocalBinding
  | SFCollectionMutationStatement => SOSemanticStatement
  | SFEnumDeclarationRows => SOSemanticEnum
  | SFNominalDeclarationRows => SOSemanticNominalConstructor
  | SFRoleDeclarationRows => SOSemanticRole
  | SFExpressionRuntimeUsageSurface => SOSemanticExpressionSurface
  end.

Inductive DeclaredSpineAuthority : SpineOwner -> SpineFact -> Prop :=
  | DeclaredAuthority : forall fact,
      DeclaredSpineAuthority (spine_authority fact) fact.

Theorem every_spine_fact_has_declared_authority :
  forall fact, exists owner, DeclaredSpineAuthority owner fact.
Proof.
  intros fact. exists (spine_authority fact). constructor.
Qed.

Theorem declared_spine_authority_unique :
  forall fact left right,
    DeclaredSpineAuthority left fact ->
    DeclaredSpineAuthority right fact ->
    left = right.
Proof.
  intros fact left right Hleft Hright.
  inversion Hleft. inversion Hright. reflexivity.
Qed.

Theorem declared_owner_does_not_imply_rung_closed :
  (exists owner, DeclaredSpineAuthority owner SFMirExecutionGraph) /\
  ~ RungClosed bridge_model.
Proof.
  split.
  - apply every_spine_fact_has_declared_authority.
  - apply owned_plus_fallback_bridge_is_not_closed.
Qed.

(* Claim limit: future facts and consumers require new concrete bindings. *)
Theorem current_model_does_not_claim_future_consumer_coverage :
  forall c, c = CArrayLiteralEmitter \/ c = CTryLetEmitter \/
    c = CCollectionMutationEmitter \/ c = CEnumEmitter \/
    c = CNominalEmitter \/ c = CRoleOperatorEmitter \/
    c = CRuntimeUsageProjection.
Proof.
  intros c. destruct c.
  - left. reflexivity.
  - right. left. reflexivity.
  - right. right. left. reflexivity.
  - right. right. right. left. reflexivity.
  - right. right. right. right. left. reflexivity.
  - right. right. right. right. right. left. reflexivity.
  - right. right. right. right. right. right. reflexivity.
Qed.
