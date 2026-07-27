# Grammar implementation status

This is the executable crosswalk for the grammar reference. A row is called
implemented only when the parser-owned AST, semantic fact owner, and at least
one backend path consume the same syntax. The repository-relative fixtures are
the smoke corpus; they are not a second grammar specification.

## Current gates

| Gate | Scope | Required result |
|---|---|---:|
| `grammar-examples-compile-test-smoke` | native AST and C emission for every `grammar/**/*.pgy` fixture | 17/17 |
| `grammar-self-driver-test-smoke` | DRV-2 self-host source-to-verified-C path for every fixture | 17/17 |
| `backend-compare-inventory-test-smoke` | native C/LLVM backend inventory | pass |
| `grammar-cheatsheet-contract-test-smoke` | authored grammar examples and semicolon policy | pass |

The lexer table currently contains 71 reserved keywords. The four entries
that were missing from the older 66-word reference are `compensate`, `fail`,
`reflect`, and `transaction`.

Run the narrow closure gate with:

```text
mingw32-make grammar-self-driver-test-smoke
```

The self-driver gate intentionally passes repository-relative fixture paths.
The driver does not gain an absolute-path fallback; source IO remains owned by
the existing root-relative policy.

The gate's proof boundary is explicit: `self-host-compiler` builds the DRV-2
driver with the native `pgy` C backend, then the driver executes the Pergyra
source-to-verified-C path. It is not a second-generation bootstrap proof and
does not claim LLVM or run-output parity. Native grammar C coverage is owned by
`grammar-examples-compile-test-smoke`; full MIR-to-C run parity is owned by
`self-host-mir-json-parity-test-smoke`.

## Syntax families now covered by the self-host path

- nominal declarations: `struct`, `class`, `subject`, `object`, `tobject`,
  `vessel`, `zone`, and `world`;
- role/ability method contracts and declaration-only signatures;
- subject-only `action` identity and ordered
  `requires`/`within`/`causes`/`authorized by`/caps/effects declaration facts;
- `if`, `while`, `for`, `match`, `break`, and `continue` CFG lowering;
- field/slot constructor facts, including world-zone and zone projection slots;
- explicit `Clone(...)` calls as a polymorphic builtin whose result preserves
  the argument type;
- loop-carried SSA phi facts patched from the lowered body, not guessed from a
  fixed instruction offset;
- verified C projection with expression graphs and declaration rows carried
  through MIR.

Zone and world slots retain their domain labels in the self-host AST nominal
subkind (`zone`/`world`). Their slot rows are consumed as constructor facts.
Authority and action contracts, relation/effect declarations and the bounded
domain-topology rows stay separate typed fact families owned by the domain
semantic layer rather than being silently converted into executable fields.
This carriage does not by itself materialize layer storage or execute projection
sync.

## Bounded nominal/action claim

The 17/17 self-driver result proves spelling, self-host AST construction,
declaration rows, and the supported verified-C projection. It does not yet prove
full nominal/action semantic parity:

- the self-host parser/semantic path now preserves `func` and `action` as
  distinct callable kinds, rejects non-subject action ownership, and carries one
  typed action declaration contract through native/self `pgy.mir.v1`;
- that declaration carriage does not supply the separate per-call participant/
  zone authority binding or runtime identity/ability evidence;
- `tobject` hosted helpers, nested object/tobject mutation, receiver versus
  parameter carriage and temporary identity receivers still have semantic/ABI
  gaps documented in `docs/200_object_to_action_boundary_patterns.md`;
- domain topology carries exact slot identity, including distinct
  `apply-effect`, but not projection member maps, effect/relation destination
  roles, layer materialization or runtime sync; native `apply <stateName>` is
  normalized to its exact effect/target slots, while the production self source
  parser still rejects that shorthand and supports only the direct form;
- the reached direct-MIR authority hook supports one direct
  world -> zone -> subject / `authorized by self` shape and its runtime evidence
  proves non-null presence, not identity-token or ability authorization.

The action declaration-contract carriage is a closed supporting semantic seam.
The executable compiler dogfood remains `REACHABLE` only for the bounded
direct-MIR world/zone/subject/action slice; the broader call/runtime contract and
domain runtime remain `BRIDGE`. Do not infer completeness from the fixture
count.

## Documentation rule

The implementation matrix above supersedes older "not current surface" notes
when a syntax is present in the fixture corpus and passes both grammar gates.
Reserved syntax is still reserved unless a parser fixture and owner-directed
semantic/backend gate are added.
