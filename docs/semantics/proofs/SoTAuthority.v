(*
  Pergyra single-source-of-truth authority model.

  Scope: one finite hard-substitution rung. This model proves that a closed
  rung has one authority producer for every required semantic fact, that every
  semantic consumer reads that authority, and that a compatibility bridge with
  a fallback read is not closed.

  It does not prove that the whole compiler has no future SoT seams. The live
  adequacy gate binds the concrete array-literal instance below to source.
*)

From Stdlib Require Import Logic.Classical_Prop.

Inductive Fact : Type :=
  | FInitializerArrayBody
  | FInitializerTextProvenance.

Inductive FactClass : Type :=
  | SemanticFact
  | ProvenanceFact.

Inductive Owner : Type :=
  | OSemanticLocalBindingFacts
  | OAstArenaProvenance
  | OCodegenTextRecovery.

Inductive Consumer : Type :=
  | CArrayLiteralEmitter.

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
  | FInitializerTextProvenance => ProvenanceFact
  end.

Definition current_authority (f : Fact) : Owner :=
  match f with
  | FInitializerArrayBody => OSemanticLocalBindingFacts
  | FInitializerTextProvenance => OAstArenaProvenance
  end.

Inductive current_produces : Owner -> Fact -> Prop :=
  | CurrentArrayBodyProducer :
      current_produces OSemanticLocalBindingFacts FInitializerArrayBody
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

(* Claim limit: future facts and consumers require new concrete bindings. *)
Theorem current_model_does_not_claim_future_consumer_coverage :
  forall c, c = CArrayLiteralEmitter.
Proof.
  intros c. destruct c. reflexivity.
Qed.
