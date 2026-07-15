# Aggregate Field Policy Probe -- Intent / Contract

**Status:** soft self-host parity candidate. An executable proof that
graph-owned aggregate (struct) field type validation rejects type drift. It
parses a program, runs typed semantic analysis, builds a
`SemanticAstBodyTypeBundle`, and proves that struct-literal field types are
judged through the expression graph's field-type facts rather than
reconstructed from source text. The C checker remains the oracle; this Pergyra
origin is the parity candidate.

## Intent

A struct literal binds each field to an initializer expression whose type must
match the declared field type. If that check is driven from re-parsed source
text instead of the graph-owned field-type facts, a leaf whose type silently
drifts -- a wrong-typed scalar, a dropped child node, or a generic call whose
argument type changed -- is accepted and the aggregate is mistyped. This probe
proves the graph-owned path rejects all three drifts with a stable diagnostic,
dogfooding the `SemanticExpressionGraphStructValue*` field-type substrate that
the self-hosted codegen consumes.

Concretely it establishes, over the clean fixture:

- The valid program's initializer types verify (`bundle.verdict.ok`).
- **leaf-type-drift** -- mutating a scalar field value to a wrong-typed literal
  is rejected with `call_arg_type_mismatch`.
- **child-fact-missing** -- removing a value node's child fact
  (`left_children[node] = -1`) is rejected with `call_arg_type_mismatch`.
- **generic-leaf-type-drift** -- mutating the argument of a generic call in a
  field (`Identity<Int>(7)` -> `Identity<Int>("bad")`) is rejected with
  `call_arg_type_mismatch`.

## Input Contract

- **valid fixture**: `valid.pgy` -- a program with a `Pair` struct
  (`left: Int; right: Long;`), a generic `Identity<T>`, and struct literals,
  one of which places a generic call in a field. The clean fixture that all
  three in-memory mutations are applied to.
- **bad_field_type.pgy** -- a struct literal with a wrong-typed scalar field,
  for the `--bad-field` mode.
- **bad_generic_field_type.pgy** -- a struct literal whose generic-call field
  is wrong-typed, for the `--bad-generic-field` mode.

Paths are fixed relative to repository root; the only CLI surface is the mode
selector (`--bad-field`, `--bad-generic-field`; default is the clean-proof
run). `main.pgy` is entrypoint-only and consumes the semantic owners
`ast_body_type_bundle_owner.pgy` (typed body bundle),
`ast_expression_graph_struct_type_verdict_owner.pgy` (graph struct/field
verdict), and `program_parse_owner.pgy` (parse). It must not reconstruct field
types from source text.

## Output Contract

Default (clean) run -- four lines on stdout, byte-matching `expected.txt`:

```
aggregate-field=graph
leaf-type-drift=reject
child-fact-missing=reject
generic-leaf-type-drift=reject
```

- Exit `0` when all three drifts are rejected. If any drift is accepted the
  probe logs `<check>=accepted` (e.g. `leaf-type-drift=accepted`) and exits `1`;
  a failing clean bundle logs its diagnostic code/message and exits `1`.

`--bad-field` / `--bad-generic-field` -- one line on stdout, matching
`bad_expected.txt`:

```
call_arg_type_mismatch
```

- Exit `1` when the bad fixture is correctly rejected with that code; exit `2`
  (with `bad ... was accepted`) if the bad aggregate field was accepted.

## Oracle

The C backend is the oracle. `tests/self_hosted/parity/aggregate_field_policy_probe_parity.sh`
compiles the fixtures with `--backend=c` and asserts `valid.pgy` compiles clean
while `bad_field_type.pgy` and `bad_generic_field_type.pgy` are rejected with
`PGY_SEM_CLASS_CONTRACT_INVALID`. It then compiles the probe on both the C and
LLVM legs, runs each, and byte-compares stdout against `expected.txt`
(clean run) and `bad_expected.txt` (`--bad-field` / `--bad-generic-field`)
through the shared backend-output comparator, so the two backends must produce
identical output.

The parity script also pins the ownership boundary structurally: the struct
verdict owner must read graph field-type facts
(`SemanticExpressionGraphFieldValueTypeName`,
`SemanticExpressionGraphFieldValueAssignableTo`,
`SemanticExpressionGraphGenericCallFactFromGraph`) and neither the struct
verdict owner nor the field-type owner may reopen source-text type policy
(`ExprType(` / `ExpressionAssignableTo(` are forbidden).

## Not In Scope

- General struct/DOM validation beyond declared-field-type policy.
- Runtime typing or value checking; this is a static, graph-fact proof.
- Non-aggregate expression typing, and struct *shape* validation (missing or
  extra fields) beyond the field-type and child-fact drifts proven here.
