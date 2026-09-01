# Builtin-Argument-Type-Mismatch Public JSON Diagnostic Receipt

Status: DONE — PUBLISHED, EXACT CI GREEN

Exact base revision: `1946f4ea4daf55443eb7c8737d2be1a25d302626`

This directive coordinates one bounded executable replacement. It is not a
semantic owner, SoT registry, progress counter, or completion claim.

## Shared objective card

- Objective: make the existing Pergyra-owned `builtin_arg_type_mismatch`
  verdict publish its exact builtin-signature public JSON identity from the
  observed `IsSome` and `UnwrapOption` semantic contexts and through installed
  MIR, C, and LLVM requests instead of producing an empty private receipt and
  generic C transport failure.
- Priority order: preserve both Pergyra text codes and fact sets; carry the
  native-agreed identity for this exact code; prove both production contexts;
  reuse the shared wire and process owners; keep every other code fail-closed,
  especially a code that shares the same public identity.
- Fact owners: `src/self_hosted/semantic/diagnostic_code_owner.pgy` continues
  to own the `PGY_SEM_BUILTIN_ARGS_INVALID` projection. The semantic public-
  receipt owner will own cause `semantic:builtin:signature_mismatch` and fix
  `match-builtin-signature` for exactly `builtin_arg_type_mismatch`. The shared
  public wire renderer remains the sole serialization owner.
- Production entrypoints: public `pgy --mir --error-format=json SOURCE`, public
  C compile/artifact JSON requests, and public LLVM compile/artifact JSON
  requests.
- Direct bypass to delete: the empty private JSON receipt for this owned code
  and the resulting generic C-side malformed/missing diagnostic. Native
  compilation is not an acceptable replacement.
- Last legitimate consumers: the installed Pergyra semantic verdict
  projection, followed by the existing MIR/artifact diagnostic process owner
  and public stderr boundary.
- Forbidden fallback: C semantic mapping, native retry or preflight, message
  parsing, grouping by `PGY_SEM_BUILTIN_ARGS_INVALID`, co-admitting
  `value_param_collection_mutation`, co-admitting
  `option_concrete_type_required`, changing Pergyra text codes or facts, a
  second semantic pass, invented source location, partial wire output, or
  another serializer.
- Focused gate:
  `tests/self_hosted/parity/public_builtin_arg_type_mismatch_json_diagnostic_receipt_owner.sh`.
- Falsifying cases: `bad_issome_non_option.pgy` and
  `bad_unwrap_non_option.pgy` must retain their normalized text diagnostics
  and produce the same owned private identity; installed MIR/C/LLVM requests
  must relay exact Pergyra receipts on stderr without stdout, artifact, or
  native timing; wording drift must not change identity;
  `bad_value_param_arraypush.pgy` must remain unadmitted despite sharing the
  same native public identity; `bad_issome_none_call.pgy` must remain
  unadmitted because native success provides no public error identity.

## Opening evidence

- Both option-builtin fixtures exit nonzero with exact Pergyra text code
  `builtin_arg_type_mismatch`, their distinct function facts, and an empty
  two-byte private JSON response. Public MIR JSON stops at
  `pgy: self-host JSON diagnostic receipt is malformed`.
- Explicit native JSON agrees for both contexts on code
  `PGY_SEM_BUILTIN_ARGS_INVALID`, stage `semantic`, layer `type`, cause
  `semantic:builtin:signature_mismatch`, and fix `match-builtin-signature`.
- `value_param_collection_mutation` has a different Pergyra code but the same
  native public identity and is therefore the exact-code negative ratchet.
  `option_concrete_type_required` remains a separate Pergyra failure for which
  the explicit native pipeline currently succeeds without a receipt.
- SoT opens unchanged at `88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`, with 9
  blockers. `diagnostic.catalog` remains `BRIDGE`; project forecast remains
  83%.

## Coordination bounds

- Independent edit scope: the semantic public-receipt and contract owners, one
  focused parity gate, its Make/installed-aggregate/component wiring, affected
  admitted-family negative fixtures, the `diagnostic.catalog` evidence row,
  and current coordination snapshots.
- Forbidden overlap: no other task may edit or publish this executable rung.
  C diagnostic transports, semantic verdict generation, native parser fuzz
  repair, stable public wire schema, and protected untracked paths are
  read-only.
- Allowed budgets: focused source/static checks within 60 seconds, focused
  parity within 5 minutes after one required DRV-2 rebuild, and one installed
  CLI integration shard within 30 minutes. Full matrices run only after
  publication.
- Integration owner and gate: the primary task owns integration; the focused
  gate above is the falsifier and
  `tests/self_hosted/parity/installed_driver_cli_mode_owner.sh` is the single
  local integration boundary.
- Outputs are implementation candidates until the primary task observes the
  focused and integration gates. They do not change SoT or progress status by
  themselves.

## Local implementation evidence

- The semantic public-receipt owner admits only
  `builtin_arg_type_mismatch` with the native-agreed builtin-signature cause
  and fix. Both option-builtin fixtures preserve their exact normalized text
  diagnostics and distinct function facts and now produce owned private
  receipts.
- Public MIR uses `IsSome`, public C uses `UnwrapOption`, and public LLVM uses
  `IsSome`. Each relays the exact corresponding Pergyra receipt on stderr
  without stdout, artifact, private wire marker, or native timing.
- `value_param_collection_mutation` remains unadmitted despite carrying the
  same native public identity, and `option_concrete_type_required` remains
  unadmitted without importing native success. Wording independence is
  executable in the existing semantic diagnostic contract.
- A fresh Pergyra-built DRV-2 passes the focused gate, rebaselined
  return/logical/let/assignment/call-argument negatives, and the complete
  installed CLI aggregate including artifact, parser, token, AST, LLVM IR,
  native opt-in, REPL, formatter, and DeviceSlot boundaries. The first
  aggregate run correctly exposed the logical gate's stale option-builtin
  negative; its exact-code replacement and the complete rerun pass.
- Diagnostic registry, SoT edge, Gate single-owner, protocol registry,
  build-source inventory, substitution velocity, hard self-host contract, and
  progress metric gates pass. The broad component inventory remains for exact
  CI because its repository-defined local budget is 60 seconds.
- The observed census remains `88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`, with 9
  blockers. Implementation volume is 21.22%; neither measure changes the 83%
  project forecast or closes `diagnostic.catalog`.

## Publication evidence

- Implementation `36bd2d41ada2a307fddc302706614837ae175f28` is published on
  `origin/main`. Exact-head CI run `33565119186`, attempt 1, is green 30/30;
  all twenty backend comparison shards passed.
- `build-linux` passed in 25m28s and records one execution of the structural
  component contract plus its removed-path ratchet PASS marker. Full self-host
  passed in 34m37s and records exactly one 173274-line `gen2 == gen3` fixed
  point, receipt-bound driver adoption, Pergyra-built DRV-2 installation, and
  the focused two-context builtin-argument receipt PASS marker.
- This directive is DONE and authorizes no successor rung. A new lease requires
  a fresh production executable falsifier; the native malformed-enum parser
  finding remains separate waiting work.
