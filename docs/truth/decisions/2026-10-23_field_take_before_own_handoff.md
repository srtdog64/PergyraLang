# 2026-10-23: an exactly replaced inout field may be handed off to an exact releasing `own` formal

Decided by: the GPT root lane, closing the OPEN question from 2026-10-19.

## Rule

The compiler admits this sequence without a source annotation:

```pergyra
let detached: Array<T> = state.field;
state.field = [];
Release(own detached);
```

only when all of these are current:

1. `state` is the same `inout` root and `field` is the exact extracted path;
2. every live path performs the fresh empty replacement before the handoff;
3. the selected call identity maps this actual to one exact `own` formal;
4. that formal's body summary records storage release.

During Pass 2 the mapping owner is
`semantic_param_publication_selected_formal_for_actual`; after publication it
is `semantic_param_publication_formal_for_actual`. The implementation must not
guess that actual ordinal equals formal ordinal.

The default-aggregate move owner remains responsible for aliases of the
containing value. A caller-side shallow alias that is used after the `inout`
call receives one whole-value `Clone`; a local live alias, duplicate field
extraction, wrong-field replacement, partial replacement, or opaque retention
stays refused when no independent copy proves safety.

## Ownership chain

- fact producer: selected call/formal identity, exact member-move identity,
  fresh replacement, and the callee's storage-release summary;
- last admission consumer:
  `semantic_collection_owned_element_requirement_record_argument`;
- alias consumer: `semantic_default_aggregate_move_finalize`;
- forbidden fallback: ordinal guessing, local replacement alone, or treating
  an arbitrary `own` callee as a releasing callee.

## Falsifiers

`array_field_detach_two_own_valid` and
`array_field_detach_caller_alias_landed_valid` are C/LLVM runtime positives.
The latter must contain one landed `Clone` in `Main`.

The wrong-field, duplicate-alias, partial-branch, opaque-hold, and live-root-
alias fixtures remain ownership refusals with no MIR artifact.
