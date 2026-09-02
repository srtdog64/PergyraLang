# Call-Arity-Mismatch Public JSON Diagnostic Receipt

Status: PUBLISHED — EXACT CI GREEN

Exact base revision: `ff8aa5d09985634d03861fdb0bcb8f6f1df6c386`

This directive coordinates one bounded executable replacement. It is not a
semantic owner, SoT registry, progress counter, fuzz priority, or completion
claim.

## Shared objective card

- Objective: publish the existing Pergyra-owned `call_arity_mismatch` verdict
  as an exact public JSON identity through installed MIR, C, and LLVM requests
  instead of emitting an empty private receipt and collapsing at the C
  malformed-receipt boundary.
- Priority order: preserve the Pergyra code and exact function/expected/actual
  facts across builtin, user-call-too-many, and user-call-too-few contexts;
  admit only this exact code; reuse the shared wire/process owners; retain
  fail-closed evidence for unadmitted codes and missing native identity.
- Fact owners: the existing semantic call/signature owners retain the arity
  verdict and facts; `diagnostic_code_owner.pgy` owns the public diagnostic
  projection. The semantic public-receipt owner will own cause
  `semantic:builtin:signature_mismatch` and fix `match-builtin-signature` for
  exactly `call_arity_mismatch`. The shared public wire renderer remains the
  sole serialization owner.
- Production entrypoints: public `pgy --mir --error-format=json SOURCE`, public
  C compile/artifact JSON requests, and public LLVM compile/artifact JSON
  requests.
- Direct bypass to delete: all three installed Pergyra arity verdicts emit an
  empty private JSON receipt, after which the public C boundary reports only
  `self-host JSON diagnostic receipt is malformed`. Native compilation is not
  an acceptable replacement.
- Last legitimate consumers: the installed Pergyra semantic verdict is
  consumed by the existing MIR/artifact diagnostic process owner and public
  stderr boundary.
- Forbidden fallback: C semantic mapping, native retry or preflight, message
  parsing, admission by `PGY_SEM_BUILTIN_ARGS_INVALID`, co-changing
  `builtin_arg_type_mismatch` or `value_param_collection_mutation`, inventing a
  new arity policy, changing text codes or facts, a second semantic pass,
  invented source location, partial wire output, or another serializer.
- Focused gate:
  `tests/self_hosted/parity/public_call_arity_mismatch_json_diagnostic_receipt_owner.sh`.
- Falsifying cases: `bad_arity_builtin.pgy`, `bad_arity_too_many.pgy`, and
  `bad_arity_too_few.pgy` must retain exact Pergyra facts for `StringLength` or
  `Twice` with expected 1 and actual 0/2/0; installed MIR/C/LLVM requests must
  relay exact receipts on stderr without stdout, artifact, or native timing;
  wording drift must not change identity. `bad_binop_assign` must remain
  unadmitted, `bad_issome_none_call` must remain fail-closed without a native
  receipt, and an unknown Pergyra code must not become ready.

## Opening evidence

- All three arity fixtures exit nonzero with exact Pergyra code
  `call_arity_mismatch`; their private JSON output is whitespace-only and
  public MIR JSON stops at the generic malformed-receipt boundary.
- Explicit native JSON fixes code `PGY_SEM_BUILTIN_ARGS_INVALID`, stage
  `semantic`, layer `type`, cause `semantic:builtin:signature_mismatch`, and
  fix `match-builtin-signature` across builtin and user-call arity contexts.
- Existing `builtin_arg_type_mismatch` and
  `value_param_collection_mutation` legitimately carry the same native public
  identity under distinct Pergyra codes. Their public receipts remain existing
  admitted behavior, not evidence for grouping this new code by identity.
- `option_concrete_type_required` has no explicit-native diagnostic receipt on
  its current fixture and therefore remains a missing-oracle negative rather
  than a guessed co-admission. `bad_binop_assign` remains an unrelated
  unadmitted semantic code.
- SoT opens unchanged at `88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`, with 9
  blockers. `diagnostic.catalog` remains `BRIDGE`; implementation volume is
  21.23% and project forecast remains 83%.

## Local integration evidence

- A fresh Pergyra-built DRV-2 was installed from the changed Pergyra receipt
  owners with SHA-256
  `19BA2237BBE6059839FC06511D1F70A364FB82DE24E310920C2B05E1F96F620A`.
- The focused gate passed all three arity contexts through installed MIR, C,
  and LLVM requests and retained arithmetic, missing-oracle, and unknown-code
  exclusion.
- The rebaselined return, logical, let, assign, call-argument,
  builtin-argument, and value-parameter-mutation receipt gates all passed.
- The installed CLI aggregate passed through parser, tokens, AST, LLVM IR,
  native opt-in, REPL, formatter, device manifest, typed argv, and general
  MIR-C transaction boundaries.
- Static diagnostic registry, SoT edge, single gate owner, protocol registry,
  build-source inventory, substitution velocity, hard contract, and progress
  gates passed. The observed implementation volume is now 21.24%; SoT counts,
  blocker count, and project forecast remain unchanged.
- Local green evidence preceded publication and did not by itself close the
  active lease.

## Publication evidence

- Exact implementation revision
  `fed3efdbae7b167dfd2a4375e08cfd14130f600c` is published on `origin/main`.
- Exact CI run `33582637404` completed `30/30` success in 34.8 minutes.
  `build-linux` completed in 25.5 minutes, `build-windows` in 10.0,
  sanitizer in 12.8, and self-host codegen bootstrap in 8.7.
- The full self-host job completed in 34.6 minutes with exactly one
  `gen2 == gen3 (173292 lines)`, installed a Pergyra-built DRV-2, observed the
  focused call-arity receipt marker, and completed the installed CLI aggregate.
- The Linux component gate recorded that structural source inventory and
  removed-path ratchets are green while executable parity owns behavior.
- This rung is published. `diagnostic.catalog` remains `BRIDGE`; the exact
  admission closes one reached public receipt seam rather than the remaining
  diagnostic catalog family.

## Coordination bounds

- Independent edit scope: the exact semantic public-receipt and contract
  owners, one focused three-context parity gate, Make/installed-aggregate/
  component wiring, affected admitted-family negative fixtures, the
  `diagnostic.catalog` evidence row, and current coordination snapshots.
- Forbidden overlap: no other task may edit or publish this executable rung.
  C diagnostic transports, semantic arity policy, native parser fuzz repair,
  stable public wire schema, and protected untracked paths are read-only.
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
