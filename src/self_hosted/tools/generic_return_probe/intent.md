# Generic Return Probe -- Intent / Contract

**Status:** soft self-host parity candidate. An executable proof that generic
function return types substitute correctly -- both the exact case (`T -> T`) and
the composite case (`T -> Option<T>`, `Array<T> -> T`) -- and that generic
argument/return mismatches are rejected. The C checker remains the oracle; this
Pergyra origin is the parity candidate.

## Intent

When a generic function is called, its declared return type must be substituted
with the call's actual type arguments before the result feeds an initializer. If
that substitution is skipped or reconstructed from source text, a call like
`Wrap(2): Option<T>` loses its `Option<Int>` result type and a mismatched
argument goes unnoticed. This probe proves substitution reaches the initializer
type facts for four shapes and that three distinct mismatches are rejected:

- **exact / composite substitution** -- over an in-memory program
  (`Identity`, `Wrap`, `First`, `ToTextValue`, `Main`) the initializer types
  resolve to `Int`, `Option<Int>`, `Array<Int>`, `Int`, and an explicit
  generic call resolves to `String`.
- **explicit-mismatch** -- an explicit generic call with a wrong argument is
  rejected with `call_arg_type_mismatch`.
- **target-mismatch** -- rewriting the call target (`Identity` -> `ToTextValue`)
  is rejected by the body call-target fixpoint with
  `call_target_unresolved`; generic return projection must not trust the
  damaged carried target.
- **nested-mismatch** -- binding `First`'s `Array<T>` parameter against a plain
  `Int` fails the signature-owned parameter bind, rejected with
  `call_arg_type_mismatch`.

## Input Contract

- **explicit_ok.pgy** -- an explicit generic call whose result is `String`,
  proven accepted by the clean run.
- **explicit_mismatch.pgy** -- an explicit generic call with a mismatched
  argument, for the `--explicit-mismatch` mode.
- The exact/composite and `--target-mismatch` / `--nested-mismatch` cases run
  against an in-memory `AstTreeArtifactFromText` program mutated through the
  graph arena, not a fixture file.

Paths are fixed relative to repository root; the CLI surface is the mode
selector (`--explicit-mismatch`, `--target-mismatch`, `--nested-mismatch`;
default is the clean-proof run). `main.pgy` is entrypoint-only and consumes the
semantic owners `ast_body_type_bundle_owner` (typed body bundle) and
`program_parse_owner` (parse); generic facts must come from the signature owner,
not from source-text type reconstruction.

## Output Contract

Default (clean) run -- four lines on stdout, byte-matching `expected.txt`:

```
generic-call=x type=Int
generic-call=wrapped type=Option<Int>
generic-call=first type=Int
generic-call=explicit type=String
```

- Exit `0` when all four initializer types resolve and `explicit_ok.pgy` is
  accepted; a substitution that does not reach the initializer facts logs
  `generic return substitution did not reach initializer facts` and exits `1`.

Mismatch modes -- one line on stdout, matching the paired expected file:

- `--explicit-mismatch` -> `call_arg_type_mismatch` (`explicit_mismatch_expected.txt`)
- `--target-mismatch` -> `call_target_unresolved` (`mismatch_expected.txt`)
- `--nested-mismatch` -> `call_arg_type_mismatch` (`nested_mismatch_expected.txt`)

Each exits `1` on correct rejection and `2` (with a `... was not rejected`
message) if the mismatch was accepted.

## Oracle

The C backend is the oracle. `tests/self_hosted/parity/generic_return_probe_parity.sh`
runs the fixtures through `--backend=c`, then compiles the probe on both the C
and LLVM legs, runs each mode, and byte-compares stdout against `expected.txt`
and the three mismatch expected files through the shared backend-output
comparator, so the two backends must produce identical output. The parity
script also pins the ownership boundary: generic parameter/return facts come
from the signature owner (`SemanticAstSignatureReturnTypeResolveAt`,
`SemanticAstSignatureParameterTypeBindAt`, `generic_actual_type_names`,
`SemanticExpressionGraphGenericCallFactFromGraph`) and the generic-call owner
must not reopen source-text typing (`ExprType(` is forbidden).

## Not In Scope

- Generic constraint/where-clause solving beyond direct argument-to-parameter
  substitution.
- Higher-kinded or nested-generic inference beyond the `Option<T>` / `Array<T>`
  composites proven here.
- Runtime dispatch of generic calls; this is a static, fact-level proof.
