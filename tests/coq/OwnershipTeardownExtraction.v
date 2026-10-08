(* Extract the actual model algorithms, not a separately written machine.
   The first output's raw transforms are forest/cost oracles, NOT authority
   APIs. The second output is a trusted-state sequential model observer:
   in_context is scheduler-only, record constructors are fixture tools, and
   machine integers are used only on bounded probes. No native API closure. *)
Require Import OwnershipTeardown.
Require Import OwnershipTeardownAuthority.
Require Import Stdlib.extraction.Extraction Stdlib.extraction.ExtrOcamlBasic.
Require Import Stdlib.extraction.ExtrOcamlNatInt.

Definition extraction_index_contract : forall h ix x k ol, IndexExact h ix ->
  forall t, index_set h ix x k ol t = index_set_scan_spec ix x k ol t := index_set_refines_scan.
Definition extraction_children_contract : forall h kd x nd o,
  KidsExact h kd -> s_node (h x) = Some nd -> forall q,
  kids_move kd x (owner nd) o q = kids_move_scan_spec kd x o q := kids_move_refines_scan.
Definition extraction_unique_contract : forall Ul,
  unit_unique Ul = true <-> Stdlib.Lists.List.NoDup Ul := unit_unique_spec.
Definition extraction_node_read_contract : forall h l,
  resolve_node h l = None <-> ~ resolves h l := resolve_node_none.
Definition extraction_root_read_contract : forall s l,
  check_root s l = true <-> root_resolves (roots s) (root_gen s) l := check_root_spec.
Definition extraction_root_reuse_contract : forall s r g Ul s1 s2,
  Step s (OpRootDrop r g Ul) s1 -> Steps s1 s2 -> check_root s2 (r, g) = false :=
  dropped_root_check_false_forever.

Extraction Language OCaml.
Extraction "ownership_teardown.ml" index_set index_set_scan_spec
  kids_move kids_move_scan_spec index_set_filter_visits kids_move_filter_visits
  unit_unique teardown root_drop resolve_node check_root.

Extraction "ownership_teardown_authority.ml" empty_authority in_context execute
  create_root allocate_owned transfer_cleanup begin_lease end_lease retire
  valid_unit holds_cleanup target_authorized unit_quiet.
