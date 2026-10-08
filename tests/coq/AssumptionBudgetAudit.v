(* The kernel gate's last consumer must reach the approved API bindings, not
   merely compile an empty file named AssumptionBudget. The approval module
   owns their types; these equalities pin their connection to the actual API. *)
Require Import SlotCalculus AssumptionBudget.

Definition max_slot_contract_is_actual :
  AssumptionBudget.approved_max_slot_id = SlotCalculus.MaxSlotId := eq_refl.
Definition token_contract_is_actual :
  AssumptionBudget.approved_verify_token = SlotCalculus.verify_token := eq_refl.
Definition access_mode_contract_is_reached := AssumptionBudget.approved_access_mode.
