# Initializer Projection Probe - Intent / Contract

**Status:** focused executable substitution proof.

## Intent

Prove that MIR local-declaration lowering consumes the semantic initializer
type row. An unannotated `let x = seed + 2` must become an `Int` MIR local
without source-text type reconstruction or a backend-local default. Scalar
operator result typing is owned by the parser expression graph for this lane.
Concrete scalar return types for direct named calls are likewise projected from
the graph callee and canonical callable return table. Direct calls whose return
and parameter rows are concrete scalars also validate graph-owned scalar
argument trees, arity, and argument types from graph handles. Arguments
may also be concrete nested direct calls, scalar operators containing concrete
direct calls, or namespace-qualified static calls. Generic/aggregate
signatures, collection or Option/Result policy, and receiver-bound member calls
remain bridges.

The probe builds its closed fixture through the approved compact artifact
bridge. That bridge is test provenance only; the MIR consumer is ratcheted
against rebuilding the initializer verdict.

## Input Contract

The probe owns one closed artifact fixture. Optional CLI mode may remove the
initializer row or its inferred type; no external source file is read.

## Negative Contract

- `--missing-initializer-row` removes the owner row.
- `--missing-inferred-type` preserves the row identity but removes its type.
- `--unknown-scalar-operand` preserves the source expression and root text but
  damages one graph leaf. Graph identifier evidence must reject the row as
  `undefined_symbol`; it must not recover the type from source text.
- `--scalar-type-mismatch` changes the same graph leaf to a String while
  preserving source/root text. Graph operand validation must reject the row as
  `binop_type_mismatch`.
- `--direct-call-callee-mismatch` preserves the source expression and root text
  but changes the graph callee from an `Int` function to a `String` function.
  The initializer owner must reject the declared `Int` row as
  `let_type_mismatch`; source-text return reconstruction must not win.
- `--direct-call-positive` proves the graph-owned direct-call return reaches the
  MIR local row under the same compiled probe.
- `--direct-call-nested-positive` proves a scalar operator argument tree reaches
  the same MIR local row under C/LLVM parity.
- `--direct-call-argument-mismatch` preserves source/root text but changes only
  the graph argument leaf from `Int` to `String`. The graph call verdict must
  reject it as `call_arg_type_mismatch`.
- `--direct-call-nested-mismatch` preserves
  `ToIntValue(1 + (2 * 3))` while changing only the graph's `2` leaf to a
  String. Graph operand validation must reject it as `binop_type_mismatch`.
- `--direct-call-nested-call-positive` proves a concrete nested direct call
  reaches the same MIR local row under C/LLVM parity.
- `--direct-call-nested-call-mismatch` preserves
  `ToIntValue(ToIntValue(2))` while changing only the inner graph callee to a
  String-returning function. The outer call must reject it as
  `call_arg_type_mismatch`.
- `--scalar-call-positive` proves `1 + ToIntValue(2)` reaches the same MIR local
  row under C/LLVM parity.
- `--scalar-call-mismatch` preserves that source while changing only the graph
  callee to a String-returning function. The operator must reject it as
  `binop_type_mismatch`.
- `--namespace-call-positive` proves `Math.Add(2)` consumes the carried
  `Math_Add` call target and reaches the same MIR local under C/LLVM parity.
- `--namespace-call-target-mismatch` preserves the source/member graph while
  changing only the carried target to the String-returning `ToTextValue`.
  Initializer typing must reject it as `let_type_mismatch`.

The first two modes must fail at the MIR projection boundary. The graph-damage
modes must fail in semantic initializer typing. The former requirement that
every bounded MIR local have an explicit source annotation is retired.

## Output Contract

Normal execution emits the single MIR local/type/source row in `expected.txt`.
Negative modes exit nonzero with their stable semantic or MIR diagnostic.

## Oracle

`tests/self_hosted/parity/initializer_projection_probe_parity.sh` runs the
positive and negative modes under C and LLVM when available. It also ratchets
the old consumer-side initializer rebuild and declared-type-only path out of
the executable rung.
