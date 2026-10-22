(* Independent typed consumer: a node is the actual cleanup proposition,
   not True, a file name, or a theorem-name occurrence. These contracts retain
   source execution and called-body admission; they do not assert inference
   completeness, native allocator refinement, or whole-language safety. *)
Require Import Stdlib.Lists.List.
Require Import ProofSpine OwnershipCleanCore OwnershipCleanComposition.
Require Import OwnershipCleanReadOnly OwnershipCleanExits.
Import ListNotations.

Definition CoreCleanupContract : Prop := forall M funs procs,
  (forall g d, funs g = Some d -> elab_fun M g d <> None) ->
  (forall g d, procs g = Some d -> elab_proc M g d <> None) ->
  forall s t sg sg' tr,
  elab M s [] [] = Some (t, []) -> sexec funs procs sg s sg' tr ->
  exists n', texec (tfuns_of M funs) (tprocs_of M procs)
    [] [] [] 0 t [] [] [] n' tr.

Definition CompositionCleanupContract : Prop := forall tf tp t,
  cleanup_equiv tf tp t (normalize_cleanup t).

Definition ReadOnlyCleanupContract : Prop := forall M funs procs,
  (forall g d, funs g = Some d -> elab_fun M g d <> None) ->
  (forall g d, procs g = Some d -> elab_proc M g d <> None) ->
  forall prefix a r body t sg sg' tr,
  elab_readonly_program M prefix a r body = Some (t, []) ->
  sexec funs procs sg (SSeq prefix (SSeq (SCopy a r) body)) sg' tr ->
  exists n', texec (tfuns_of M funs) (tprocs_of M procs)
    [] [] [] 0 t [] [] [] n' tr.

Definition ExitCleanupContract : Prop := forall M funs procs,
  (forall g d, funs g = Some d -> elab_fun M g d <> None) ->
  (forall g d, procs g = Some d -> elab_proc M g d <> None) ->
  forall s t sg sg' tr o,
  xelab M s [] no_exits [] = Some (t, []) -> xsexec funs procs sg s sg' tr o ->
  exists n', xtexec (tfuns_of M funs) (tprocs_of M procs)
    [] [] [] 0 t [] [] [] n' tr o.

Definition cleanup_nodes (node : ProofNode) : Prop :=
  match node with
  | NodeOwnershipCleanCore => CoreCleanupContract
  | NodeOwnershipCleanComposition => CompositionCleanupContract
  | NodeOwnershipCleanReadOnly => ReadOnlyCleanupContract
  | NodeOwnershipCleanExits => ExitCleanupContract
  | _ => False
  end.

Theorem actual_cleanup_models_connect :
  PermitsClaim cleanup_nodes OwnershipCleanupConnected.
Proof.
  unfold PermitsClaim, HasNode, cleanup_nodes.
  split; [exact closed_program_frees_everything|].
  split; [exact normalize_cleanup_equiv|].
  split; [exact readonly_elaboration_frees_everything|].
  exact x_closed_frees_everything.
Qed.

(* Dropping just one real contract cannot be hidden by the other nodes. *)
Definition cleanup_without (missing : ProofNode) (node : ProofNode) : Prop :=
  match missing, node with
  | NodeOwnershipCleanCore, NodeOwnershipCleanCore
  | NodeOwnershipCleanComposition, NodeOwnershipCleanComposition
  | NodeOwnershipCleanReadOnly, NodeOwnershipCleanReadOnly
  | NodeOwnershipCleanExits, NodeOwnershipCleanExits => False
  | _, _ => cleanup_nodes node
  end.

Example omitted_cleanup_contract_refuses :
  forall missing,
  In missing [NodeOwnershipCleanCore; NodeOwnershipCleanComposition;
              NodeOwnershipCleanReadOnly; NodeOwnershipCleanExits] ->
  ~ PermitsClaim (cleanup_without missing) OwnershipCleanupConnected.
Proof.
  intros missing H. simpl in H.
  destruct H as [<-|[<-|[<-|[<-|[]]]]];
    unfold PermitsClaim, HasNode, cleanup_without; tauto.
Qed.

Example model_binding_is_not_runtime_refinement :
  ~ PermitsClaim cleanup_nodes WholeLanguageVerified.
Proof. exact (fun impossible => impossible). Qed.

(* Weakening the postcondition to True cannot satisfy the pinned contract. *)
Fail Definition truth_is_cleanup_evidence : CoreCleanupContract := I.
Fail Definition declaration_is_cleanup_evidence : ExitCleanupContract := I.
