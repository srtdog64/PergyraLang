(* G1 extracts the admitted algorithm; it defines no second ownership model. *)
Require Import OwnershipCleanCore.
Require Import OwnershipCleanComposition.
Require Import OwnershipCleanReadOnly.
Require Import OwnershipCleanExits.
Require Import Stdlib.Lists.List.
Require Import Stdlib.extraction.Extraction Stdlib.extraction.ExtrOcamlBasic.
Require Import Stdlib.extraction.ExtrOcamlNatInt.

(* Consume exact propositions, not just theorem-name presence. Removing or
   weakening the proof cannot leave an extracted observer-only green gate. *)
Definition extraction_normalization_contract : forall tf tp t rho beta H n rho' beta' H' n' tr,
  texec tf tp rho beta H n t rho' beta' H' n' tr <->
  texec tf tp rho beta H n (normalize_cleanup t) rho' beta' H' n' tr :=
  normalize_cleanup_equiv.

Definition extraction_refusal_contract : forall M s L B,
  elab_normalized M s L B = None <-> elab M s L B = None :=
  elab_normalized_refusal.

Definition extraction_copy_sites_contract : forall t,
  count_copies (normalize_cleanup t) = count_copies t := normalize_cleanup_copies.

Definition extraction_readonly_trace_contract : forall funs procs a r body L B rewritten sg sg' tr,
  ro_admit a r body L B = ROAccepted rewritten ->
  sexec funs procs sg (SSeq (SCopy a r) body) sg' tr ->
  exists st', sexec funs procs sg rewritten st' tr /\
    (forall v, v <> a -> sg' v = st' v) := readonly_copy_elision.

Definition extraction_allocation_contract : forall tf tp rho beta H n t rho' beta' H' n' tr,
  texec tf tp rho beta H n t rho' beta' H' n' tr ->
  forall k, fixed_allocations t = Some k -> n' = n + k := fixed_allocations_sound.

(* Consume the execution witness too, rather than a pair of syntax counts. *)
Definition extraction_readonly_execution_contract : forall flag, exists original optimized,
  elab_normalized no_summaries (ro_demo_program flag) nil nil = Some (original, nil) /\
  elab_readonly_program no_summaries (ro_demo_prefix flag) 1 0 ro_demo_body = Some (optimized, nil) /\
  texec (tfuns_of no_summaries no_funs) (tprocs_of no_summaries no_procs)
    nil nil nil 0 original nil nil nil 3 (SLeaf 7 :: SLeaf 7 :: nil) /\
  texec (tfuns_of no_summaries no_funs) (tprocs_of no_summaries no_procs)
    nil nil nil 0 optimized nil nil nil 2 (SLeaf 7 :: SLeaf 7 :: nil) :=
  ro_demo_one_fewer_allocation.

(* A focus moves exactly the part's own blocks; the observer consumes the
   proposition, not just the theorem name. *)
Definition extraction_place_segment_contract : forall (vs : list TValue) (i : nat) (v : TValue) (n off : nat),
  Forall (fun w => length (snd w) = vsize (fst w)) vs -> nth_error vs i = Some v ->
  voff (SNode (map fst vs)) (i :: nil) = Some off ->
  firstn (vsize (fst v)) (skipn off ((n, 0) :: flat_map snd vs)) = snd v := pack_child_segment.

(* Unresolved or mismatched summaries are refused, never read as borrows. *)
Definition extraction_missing_summary_contract : forall M k x g args L B,
  kmodes M k g = None -> elab_call M k x g args L B = None := missing_summary_refuses_call.

Definition extraction_arity_contract : forall M k x g args L B ms,
  kmodes M k g = Some ms -> length ms <> length args -> elab_call M k x g args L B = None :=
  arity_mismatch_refuses_call.

Definition extraction_mode_ascent_contract : forall funs procs n,
  modes_le (iter_modes funs procs n) (iter_modes funs procs (S n)) := infer_ascends.

(* Every exit of an admitted closed program releases everything it owns. *)
Definition extraction_exit_release_contract : forall M funs procs,
  (forall g d, funs g = Some d -> elab_fun M g d <> None) ->
  (forall g d, procs g = Some d -> elab_proc M g d <> None) ->
  forall s t sg sg' tr o,
  xelab M s nil no_exits nil = Some (t, nil) -> xsexec funs procs sg s sg' tr o ->
  exists n', xtexec (tfuns_of M funs) (tprocs_of M procs) nil nil nil 0 t nil nil nil n' tr o :=
  x_closed_frees_everything.

Extraction Language OCaml.
Extraction "ownership_clean.ml" elab elab_fun elab_proc count_copies
  no_summaries gui_modes infer_modes normalize_cleanup elab_normalized cleanup_nodes
  gui_program gui_program_reuse gui_calls gui_calls_reuse gui_funs gui_procs
  ro_admit ro_rename elab_readonly_program fixed_allocations
  ro_demo_prefix ro_demo_body ro_demo_program
  count_field_reads gui_state_inout gui_state_detach gui_state_update
  gui_state_view gui_state_view_copy app_funs app_modes
  borrow_modes gui_borrow iter_modes
  xelab xdrops no_exits partial_build early_return_body break_body.
