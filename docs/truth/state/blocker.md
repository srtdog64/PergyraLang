# Current blocker

Owner: Claude root lane. Checked 2026-11-15 on `fix/native-unsafe-admissions`
(on main `b06f07c1a`, CI green at `65724e27d`).

## Objective card

- Objective: native admits no lifetime negative that reads or frees freed
  memory, with required CI green on one published commit (user goal
  2026-11-14: all memory errors, SoT rows they pass through).
- Priority: memory safety of what either front end admits; one fact owner;
  fail-closed admission; C/LLVM parity; fixed point; exact-SHA CI.
- Forbidden: a larger job or gate timeout, a reduced canonical input, a
  skipped backend, a weakened negative, per-program-shape route admission.

## Open, in dependency order

1. Required CI on the commit carrying the native checker fixes
   ([measurement](../evidence/2026-11-15_native_unsafe_negative_admissions_closed.md)):
   36 memory errors to 1; the one left (`inout_cycle_drop`) is endless
   recursion that its admitted positive twin shares.
2. A user-call String producer the owned-result examination neither
   proves nor disproves stays at the native boundary (the self-host
   analyzer proves the canonical ones, e.g. `EmitFunctionSet`).
3. Locally, core step 5 fails only on WSL (`wslpath -w` gives a UNC repo
   root); a pass-through `wslpath` shim makes it pass.
4. The self-host MIR route carries no compiler `Clone`; native lands one for
   `collection_field_use_after_move`, the self-host refuses it.

## Done since `cdb65d798`

- Folder reorganization (user decision 2026-11-13) with gate coverage kept
  across the moves; the seed prebuild key refuses a missing input.
- Nested Array element store on both legs; three drifted projections
  regenerated and checked in the core shard.
- Native checker (2026-11-15): later-argument moves and changes, defer
  reads after an own hand-off, owned String transfers, field write versus
  escape (a written field moves), break/continue move states, String reads
  beside a releasing argument, releases of shared or borrowed parts.
  `tests/ownership/native_unsafe_negative_refusal_smoke.sh` (core shard)
  holds the refusals, their safe neighbours and the copy landings.

## Falsifiers kept

- A drop on one branch still refuses the read after the join; own
  transfers of storage a constructor kept stay refused natively.
- A field restore whose MIR row names another field, and a binding move
  whose destination row names itself, are refused with no artifact.
- A 600-line unregistered owner in `src/self_hosted/semantic/ast_collection/`
  is refused; a missing shared leg refuses the seed prebuild key.
- `owned_string_member_actual_positive`, `exclusive_focus_update_positive`
  and `empty_capture_two_fields_positive` stay admitted beside the
  refused negatives they mirror.
