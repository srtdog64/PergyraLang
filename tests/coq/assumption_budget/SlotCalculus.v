(* Benign abstract API fixture; no native/runtime or security implementation. *)
Definition SlotId := nat.
Definition Generation := nat.
Definition Token := nat.
Inductive AccessMode := ModeRead | ModeWrite | ModeRelease | ModePin | ModeClaim.
Parameter MaxSlotId : nat.
Parameter verify_token : nat -> nat -> nat -> AccessMode -> bool.
