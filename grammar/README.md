# Pergyra Grammar Examples

This folder is a compact syntax map, not a full language specification.
The examples are gated by `make grammar-examples-compile-test-smoke`, which
requires every `.pgy` file here to parse and emit C through the live compiler.

Pergyra is not hard because the core control syntax is exotic. `func`, `let`,
`if`, `while`, arrays, structs, and `match` are deliberately conventional. The
hard part is that domain syntax carries ownership and evidence meaning:

- `subject` is an actor/state-bearing domain value.
- `object` is an internal projection/value shape.
- `tobject` is a transfer/protocol-facing projection shape.
- `ability` names a behavioral contract.
- `role` implements an ability for a concrete type.
- `effect` records a domain fact caused by an action.
- `zone` owns resource slots and authority boundaries.
- `world` owns larger composed topology.
- `intent` is the orchestration surface that consumes actions, effects,
  authority, and deterministic coordination evidence.

There are two reading paths:

1. `syntax_units/` starts from the smallest grammar forms: `func`, `->`,
   `let`, `:`, blocks, statements, arrays, `match`, then domain declarations.
2. The files in this folder combine those forms by semantic layer.

Read the layer examples in order:

1. `01_values_and_control.pgy` - ordinary values, functions, loops, arrays,
   structs, and `match`.
2. `02_projection_shapes.pgy` - `object` and `tobject` as separate projection
   surfaces.
3. `03_ability_role_subject.pgy` - behavioral contract plus implementation.
4. `04_zone_intent_authority.pgy` - zone-owned slots, authority, effect, and
   compressed intent steps.
5. `05_world_zone_topology.pgy` - a small world containing zones.

Rule of thumb: if a construct owns a resource or boundary, it should be visible
as a `zone`/`world`/`intent` fact. If it is only a helper computation, keep it as
ordinary `func` code.
