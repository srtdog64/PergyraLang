# Collection Policy Probe -- Intent / Contract

**Status:** soft self-host parity candidate. An executable proof that
collection-mutation policy is owner-directed and graph-owned: a mutating
collection call (e.g. `ArrayPush(xs, ...)`) is rejected when its target
collection arrives through a by-value parameter and admitted when it arrives
through an `inout` parameter. The C checker remains the oracle; this Pergyra
origin is the parity candidate.

## Intent

Mutating a collection that was passed by value silently discards the mutation
at the call site's expectation -- a real defect. The disposition of the mutated
binding (`default_param` vs `inout_param`) decides whether the mutation is
legal, and that decision must be driven from the graph-owned call-target facts,
not re-derived from source text. This probe proves:

- **statement-policy=owner** -- the clean fixture's statement types verify
  through `ast_body_type_bundle_owner`.
- **call-policy=graph** -- for a graph-owned `ArrayPush(xs, 1)` call,
  `SemanticExpressionGraphCollectionMutationFactFromGraph` rejects the
  `default_param` (value) disposition with `value_param_collection_mutation`
  and admits the `inout_param` disposition.
- **target-drift=reject** -- rewriting the call target from `ArrayPush` to `Log`
  makes the mutation fact reject with `ast_artifact_invalid`, so the policy is
  bound to the actual call target rather than assumed.

## Input Contract

- **valid fixture**: `valid.pgy` -- a program whose statement types verify and
  that exercises the owner-directed collection path.
- **bad_value_param.pgy** -- a program that mutates a by-value collection
  parameter, for the `--bad-value-param` mode.

Paths are fixed relative to repository root; the only CLI surface is the mode
selector (`--bad-value-param`; default is the clean-proof run). `main.pgy` is
entrypoint-only and consumes the semantic owners `ast_body_type_bundle_owner`
(typed body bundle), `ast_expression_graph_collection_mutation_owner` (graph
collection-mutation facts), and `program_parse_owner` (parse). It must reach
the typed artifact analysis rather than reconstructing policy from text.

## Output Contract

Default (clean) run -- three lines on stdout, byte-matching `expected.txt`:

```
statement-policy=owner
call-policy=graph
target-drift=reject
```

- Exit `0` when the owner path verifies, the graph fact owns the call policy,
  and target drift is rejected. A failing clean bundle logs its diagnostic and
  exits `1`; an unowned call policy logs `call-policy=unowned` and a missed
  drift logs `target-drift=accepted`, both exit `1`.

`--bad-value-param` -- one line on stdout, matching `bad_value_param_expected.txt`:

```
value_param_collection_mutation
```

- Exit `1` when the by-value mutation is correctly rejected with that code;
  exit `2` (with `value parameter collection mutation was accepted`) if it was
  accepted.

## Oracle

The C backend is the oracle. `tests/self_hosted/parity/collection_policy_probe_parity.sh`
compiles the fixtures with `--backend=c` and asserts `valid.pgy` compiles clean
while `bad_value_param.pgy` is rejected with `PGY_SEM_BUILTIN_ARGS_INVALID`. It
then compiles the probe on both the C and LLVM legs, runs each, and byte-compares
stdout against `expected.txt` (clean run) and `bad_value_param_expected.txt`
(`--bad-value-param`) through the shared backend-output comparator, so the two
backends must produce identical output. The parity script also pins the
ownership boundary: the statement owner reports `SemanticCollectionMutationError`
and the graph owner exposes `SemanticExpressionGraphCollectionMutationFactFromGraph`
keyed on `collection_call_target`.

## Not In Scope

- General collection typing or element-type checking beyond mutation
  disposition (value vs inout) policy.
- Runtime aliasing or borrow analysis; this is a static, graph-fact proof.
- Non-collection call policy.
