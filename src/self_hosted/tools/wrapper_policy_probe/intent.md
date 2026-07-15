# Wrapper Policy Probe -- Intent / Contract

**Status:** soft self-host parity candidate. An executable proof that the
`Option` / `Result` wrapper builtins are typed through the graph-owned wrapper
value facts -- their payload types resolve, and malformed wrapper/unwrap uses
are rejected. The C checker remains the oracle; this Pergyra origin is the
parity candidate.

## Intent

`Some`, `Ok`, `UnwrapOption` and the other wrapper builtins carry a payload type
that must be recovered from the graph, not guessed from the call name. If the
wrapper policy is not graph-owned, an `Option<Int>` loses its payload, a
`Result<Int>` unwrap is mistyped, or a call whose target drifted away from the
real builtin is still treated as one. This probe proves:

- **option-wrapper=graph** -- the clean fixture yields at least one verified
  `Option<Int>` initializer and at least one verified `Int` payload.
- **result-wrapper=graph** -- it yields at least one verified `Result<Int>` and
  at least two verified `Int` payloads.
- **target-drift=reject** -- rewriting an `UnwrapOption` call target to `Log`
  makes `SemanticExpressionGraphWrapperValueFactFromGraph` reject with
  `ast_artifact_invalid`, so the policy is bound to the real call target.

## Input Contract

- **valid.pgy** -- a program exercising `Option<Int>` and `Result<Int>`
  wrappers and their unwraps, whose initializer types verify.
- **bad_option.pgy** -- an `Option` builtin without a concrete payload type, for
  the `--bad-option` mode.
- **bad_unwrap.pgy** -- a `Result`/`Option` unwrap with a mismatched argument,
  for the `--bad-unwrap` mode.

Paths are fixed relative to repository root; the CLI surface is the mode
selector (`--bad-option`, `--bad-unwrap`; default is the clean-proof run).
`main.pgy` is entrypoint-only and consumes the semantic owners
`ast_body_type_bundle_owner` (typed body bundle),
`ast_expression_graph_wrapper_value_owner` (graph wrapper value facts), and
`program_parse_owner` (parse), reaching the typed artifact analysis rather than
reconstructing wrapper policy from text.

## Output Contract

Default (clean) run -- three lines on stdout, byte-matching `expected.txt`:

```
option-wrapper=graph
result-wrapper=graph
target-drift=reject
```

- Exit `0` when both wrapper families type through the graph and target drift is
  rejected. A failing clean bundle logs `wrapper-verdict=<code>` plus a per-row
  dump and exits `1`; missing wrapper payload types log
  `option-wrapper-types=missing` / `result-wrapper-types=missing` and a missed
  drift logs `target-drift=accepted`, all exit `1`.

Bad modes -- one line on stdout, matching the paired expected file:

- `--bad-option` -> `option_concrete_type_required` (`bad_option_expected.txt`)
- `--bad-unwrap` -> `builtin_arg_type_mismatch` (`bad_unwrap_expected.txt`)

Each exits `1` on correct rejection and `2` (with a `bad ... was accepted`
message) if it was accepted.

## Oracle

The C backend is the oracle. `tests/self_hosted/parity/wrapper_policy_probe_parity.sh`
compiles the fixtures with `--backend=c` and asserts the valid Option/Result
sources compile clean while `bad_option.pgy` is rejected with
`PGY_C_TYPE_UNSUPPORTED` and `bad_unwrap.pgy` with `PGY_SEM_BUILTIN_ARGS_INVALID`.
It then compiles the probe on both the C and LLVM legs, runs each mode, and
byte-compares stdout against `expected.txt` and the two bad expected files
through the shared backend-output comparator, so the two backends must produce
identical output. The parity script also pins the ownership boundary: the
verdict owner exposes `SemanticExpressionGraphWrapperValueFactFromGraph` keyed
on `wrapper_call_target`, and the payload types come from `OptionPayloadTypeOpt`
/ `ResultPayloadTypeOpt`.

## Not In Scope

- Wrapper types beyond `Option` / `Result` (e.g. user-defined monadic
  wrappers).
- Exhaustiveness or match-arm checking on wrapper values.
- Runtime unwrap behavior; this is a static, graph-fact proof.
