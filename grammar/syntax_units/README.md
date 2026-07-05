# Syntax Units

This folder explains Pergyra syntax from the smallest useful forms upward.

Each `.pgy` file is a complete compile-checkable program. The top comments show
the syntax shape first, then the example uses only a small amount of new syntax.

Recommended order:

1. `01_func_arrow.pgy` - `func`, parameter lists, `->`, return type.
2. `02_let_colon_assignment.pgy` - `let`, `: Type`, `=`, reassignment, `;`.
3. `03_blocks_if_while.pgy` - braces, `if`, `while`, Bool conditions.
4. `04_struct_constructor_field.pgy` - `struct`, fields, constructors, `.`.
5. `05_array_generic_index.pgy` - `Array<T>`, literals, indexing.
6. `06_match_cases.pgy` - `match`, `case`, `default`.
7. `07_subject_action_self.pgy` - `subject`, `action`, `self`.
8. `08_ability_role_requires.pgy` - `ability`, `role`, `impl`, `requires`.
9. `09_zone_slots.pgy` - `zone`, typed slots, authority slots.
10. `10_intent_step.pgy` - `intent`, `step`, `using`, `on`, `expect`.
11. `11_world_embedding_clone.pgy` - `world`, zone embedding, `Clone`.
12. `12_combined_flow.pgy` - a small mixed example.

Punctuation rule:

- Function-body statements usually end with `;`.
- Topology declarations inside `zone` and `world` usually do not end with `;`.
- `->` means "returns". It does not create a function value by itself.
- `:` means "has type" in declarations and named step fields.

## What Each Syntax Form Is

| Syntax | What it is | Why it exists |
| --- | --- | --- |
| `func` | Ordinary function declaration | Names reusable computation. It has no domain authority by itself. |
| `->` | Return-type marker | Separates input parameters from output type: `A, B -> C`. |
| `:` | Type/ascription marker | Says a name or step field has a type or named meaning. |
| `let` | Local binding declaration | Introduces a value in a function body. |
| `let mut` | Mutable field/local marker | Makes intended mutation visible at the declaration site. |
| `;` | Statement terminator | Ends ordinary function-body statements. |
| `{ ... }` | Block/body delimiter | Groups function bodies, control bodies, and declaration bodies. |
| `struct` | Passive value record | Groups fields without actor/authority meaning. |
| `Array<T>` | Generic collection type | Says all elements have type `T`. |
| `match` | Value dispatch statement | Chooses a branch by comparing a value to cases. |
| `subject` | State-bearing domain actor | Owns mutable domain state and actions. |
| `action` | Domain operation | Describes behavior that can carry authority/effect contracts. |
| `self` | Action receiver | Names the subject currently being acted on. |
| `ability` | Behavior contract | Names what a subject/type must be able to do. |
| `role` | Contract implementation | Connects an ability to a concrete subject/type. |
| `requires` | Contract requirement | States that an action/zone/step needs an ability. |
| `zone` | Resource/authority boundary | Owns slots and declares local authority facts. |
| `slot` | Zone-owned place | Makes resource ownership explicit inside a zone/world. |
| `authority` | Authority evidence declaration | Marks who or what can authorize domain action. |
| `intent` | Orchestration declaration | Coordinates steps, zones, authority, and expected effects. |
| `step` | Intent operation row | One named unit inside an intent flow. |
| `using:` | Zone instance binding | Tells a step which zone value supplies the boundary. |
| `on:` | Action call binding | Tells a step what action/subintent to execute. |
| `expect:` | Verification expectation | States the condition expected after a step. |
| `world` | Topology owner | Owns larger zone composition. |
| `Clone(...)` | Explicit boundary fork | Makes a copy across containment boundaries visible. |

Short version: Pergyra keeps basic computation familiar, then adds explicit
syntax for domain ownership and proof-bearing flow.
