(* Approved abstract API contracts, not implementations or cryptographic laws.
   Type ascriptions are kernel-checked against the imported declarations, using
   nat rather than aliases whose definitions could change with the parameter. *)
Require Import SlotCalculus.

Definition approved_max_slot_id : nat := SlotCalculus.MaxSlotId.

Definition approved_verify_token :
  nat -> nat -> nat -> SlotCalculus.AccessMode -> bool :=
  SlotCalculus.verify_token.

(* Exhaustiveness pins the approved access-mode domain. Adding a mode requires
   an explicit contract review instead of retaining an unnoticed old budget. *)
Definition approved_access_mode (mode : SlotCalculus.AccessMode) : nat :=
  match mode with
  | SlotCalculus.ModeRead => 0
  | SlotCalculus.ModeWrite => 1
  | SlotCalculus.ModeRelease => 2
  | SlotCalculus.ModePin => 3
  | SlotCalculus.ModeClaim => 4
  end.
