# Generic Return Probe -- Intent / Contract

**Status:** soft self-host parity candidate. The executable semantic probe checks
that generic function return types substitute correctly -- both the exact case
(`T -> T`) and the composite case (`T -> Option<T>`, `Array<T> -> T`) -- and that generic
argument/return mismatches are rejected. The C checker remains the oracle; this
Pergyra origin is the parity candidate.

## Intent

When a generic function is called, its declared return type must be substituted
with the call's actual type arguments before the result feeds an initializer. If
that substitution is skipped or reconstructed from source text, a call like
`Wrap(2): Option<T>` loses its `Option<Int>` result type and a mismatched
argument goes unnoticed. This probe checks substitution reaches the initializer
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
- The exact/composite and `--target-mismatch` cases use an in-memory
  `ParserAstTreeArtifactFromText` program. The target case changes the graph
  arena's carried target; the nested case instead calls the signature-owned
  parameter binder with an `Int` actual for an `Array<T>` parameter.
- `--callable-resolution` checks exact, empty-owner, missing, colliding, and
  invalid-node callable identity rows against `callable_resolution_expected.txt`.

Paths are fixed relative to repository root; the CLI surface is the mode
selector (`--explicit-mismatch`, `--target-mismatch`, `--nested-mismatch`,
`--callable-resolution`;
default is the clean-proof run). `main.pgy` constructs and checks probe inputs;
it consumes the semantic owners `ast_body_type_bundle_owner` (typed body bundle) and
`program_parse_owner` (parse); generic facts must come from the signature owner,
not from source-text type reconstruction.

`SemanticAstSignatureParameterTypesBind` consumes an `own Array<Int>` parameter
index row and reads three call-bounded `Slice<String>` views: generic names,
actual types, and prior bindings. It does not consume those String backing
arrays. It materializes an independent `Array<String>` binding row; the caller
retires its temporary actual/prior arrays after return and later retires the
result. An empty result means binding failure (`generic_count > 0` is required).
Passing this probe is not a general non-retention guarantee for Slice or `inout`.

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

`--callable-resolution` exits `0` with five lines matching
`callable_resolution_expected.txt`; a violated comparison contract logs
`callable canonical comparison contract failed` and exits `1`.

## Oracle

The C checker is the semantic oracle. The parity script compiles and runs this
probe for the selected backend legs (default: C and LLVM) and compares each
mode's stdout with its expected artifact through the shared backend-output
comparator. The probe analyzes its in-memory programs and source fixtures;
those programs/fixtures are not emitted or run. Probe execution and generic
program runtime execution are different evidence scopes.

An LLVM-only result proves only that selected leg; it does not establish C/LLVM
parity, installed-driver admission, or self-host bootstrap. The last observed
native LLVM-only receipt was PASS (`exec-563b114f`); installed C failed with
`compiler_internal_builtin` against its stale admitted caller registry.
New source/gate revisions require their own receipts.

The script also pins the ownership boundary: generic parameter/return facts come
from the signature owner (`SemanticAstSignatureReturnTypeResolveAt`,
`SemanticAstSignatureParameterTypesBind`, `generic_actual_type_names`,
`SemanticExpressionGraphGenericCallFactFromGraph`) and the generic-call owner
must not reopen source-text typing (`ExprType(` is forbidden).

## Not In Scope

- Generic constraint/where-clause solving beyond direct argument-to-parameter
  substitution.
- Higher-kinded or nested-generic inference beyond the `Option<T>` / `Array<T>`
  composites proven here.
- Runtime dispatch of generic calls; this is a static, fact-level proof.
