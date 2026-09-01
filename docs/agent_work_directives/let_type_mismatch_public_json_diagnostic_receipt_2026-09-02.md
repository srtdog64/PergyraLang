# Let-Type-Mismatch Public JSON Diagnostic Receipt

Status: DONE — IMPLEMENTATION AND EXACT CI GREEN

Exact base revision: `a7e99d5c2eced2a16b5d2cd3095296ae87401781`

This directive coordinates one bounded executable replacement. It is not a
semantic owner, SoT registry, progress counter, or completion claim.

## Shared objective card

- Objective: make the existing Pergyra-owned `let_type_mismatch` verdict
  publish its exact assignability public JSON identity from all thirteen
  observed semantic fixture contexts and through installed MIR, C, and LLVM
  requests instead of producing an empty private receipt and a generic C
  transport failure.
- Priority order: preserve each Pergyra text verdict and expected/actual facts;
  carry the native-agreed identity for this exact code; prove all thirteen
  contexts; reuse the shared wire and process owners; keep other unadmitted
  type mismatch codes fail-closed.
- Fact owners: `src/self_hosted/semantic/diagnostic_code_owner.pgy` continues to
  own the `PGY_SEM_TYPE_MISMATCH` projection. The semantic public-receipt owner
  will own cause `semantic:assignability_check` and fix `annotate-or-convert`
  for exactly `let_type_mismatch`. The shared public wire renderer remains the
  sole serialization owner.
- Production entrypoints: public `pgy --mir --error-format=json SOURCE`, public
  C compile/artifact JSON requests, and public LLVM compile/artifact JSON
  requests.
- Direct bypass to delete: the empty private JSON receipt for this owned code
  and the resulting generic C-side malformed/missing diagnostic. Native
  compilation is not an acceptable replacement.
- Last legitimate consumers: the installed Pergyra semantic verdict projection,
  followed by the existing MIR/artifact diagnostic process owner and public
  stderr boundary.
- Forbidden fallback: C semantic mapping, native retry or preflight, message
  parsing, grouping by the shared `PGY_SEM_TYPE_MISMATCH` identity, co-admitting
  `assign_type_mismatch`, changing Pergyra text codes or facts, a second
  semantic pass, invented source location, partial wire output, or another
  serializer.
- Focused gate:
  `tests/self_hosted/parity/public_let_type_mismatch_json_diagnostic_receipt_owner.sh`.
- Falsifying cases: the thirteen `bad_*` fixtures currently emitting
  `let_type_mismatch` must retain their normalized text diagnostics and produce
  the same owned private identity; installed MIR/C/LLVM requests must relay
  exact Pergyra receipts on stderr without stdout, artifact, or native timing;
  wording drift must not change identity; the already admitted return verdict
  must remain distinct by Pergyra code and `assign_type_mismatch` must remain
  unadmitted.

## Opening evidence

- The installed Pergyra semantic owner emits `let_type_mismatch` for thirteen
  fixtures spanning arithmetic/call/builtin-result assignment, nested and loop
  bodies, option/result payloads, and explicit let mismatch. All thirteen
  private JSON requests exit nonzero with no non-whitespace receipt, and all
  thirteen public MIR JSON requests stop at
  `pgy: self-host JSON diagnostic receipt is malformed`.
- Explicit native JSON agrees across all thirteen contexts on code
  `PGY_SEM_TYPE_MISMATCH`, stage `semantic`, layer `type`, cause
  `semantic:assignability_check`, and fix `annotate-or-convert`.
- `return_type_mismatch` legitimately carries the same public identity but
  retains a different Pergyra code. That fact does not authorize grouping by
  the public code or silently widening this rung to `assign_type_mismatch`.
- SoT opens unchanged at `88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`, with 9
  blockers. `diagnostic.catalog` remains `BRIDGE`; project forecast remains
  83%.

## Coordination bounds

- Independent edit scope: the semantic public-receipt and contract owners, one
  focused parity gate, its Make/installed-aggregate/component wiring, affected
  admitted-family negative fixtures, the `diagnostic.catalog` evidence row,
  and current coordination snapshots.
- Forbidden overlap: no other task may edit or publish this executable rung.
  C diagnostic transports, parser/lexer owners, semantic verdict generation,
  stable public wire schema, fuzz findings, and protected untracked paths are
  read-only.
- Allowed budgets: focused source/static checks within 60 seconds, focused
  parity within 5 minutes after one required DRV-2 rebuild, and one installed
  CLI integration shard within 30 minutes. Full matrices run only after
  publication.
- Integration owner and gate: the primary task owns integration; the focused
  gate above is the falsifier and
  `tests/self_hosted/parity/installed_driver_cli_mode_owner.sh` is the single
  local integration boundary.

## Local implementation evidence

- The semantic public-receipt owner admits only `let_type_mismatch` with the
  native-agreed assignability cause and fix. All thirteen fixtures preserve
  their exact normalized text diagnostics and expected/actual facts and now
  produce owned private receipts.
- Public MIR uses the explicit let fixture, public C uses a call-result
  assignment, and public LLVM uses a while-body mismatch. Each relays the exact
  corresponding Pergyra receipt on stderr without stdout, artifact, wire
  marker, or native timing. The admitted return receipt retains the shared
  public identity while `assign_type_mismatch` remains unadmitted.
- A fresh Pergyra-built DRV-2 passes the focused thirteen-context gate, all five
  existing semantic receipt-family gates, and the complete installed CLI
  aggregate including artifact, parser, token, AST, LLVM IR, native opt-in,
  REPL, formatter, and DeviceSlot boundaries.
- The first aggregate invocation used Windows absolute driver paths and was
  rejected by the repository-relative machine-manifest guard. The
  Make/CI-equivalent POSIX-path rerun passed without a source change.
- Diagnostic registry, SoT edge, Gate single-owner, protocol registry,
  build-source inventory, substitution velocity, hard self-host contract, and
  progress metric gates pass. The broad component inventory remains for exact
  CI because its repository-defined local budget is 60 seconds.
- The observed census remains `88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`, with 9
  blockers. Implementation volume is 21.20%; neither measure changes the 83%
  project forecast or closes `diagnostic.catalog`.

## Publication evidence

- Implementation `6c13bf35513fcdb6d5ee51317a2610054146fb7c` is on
  `origin/main`. Exact-head CI run `33545829167` is green 30/30, including
  backend comparison shards 20/20.
- `build-linux` passed in 25m55s and actually invoked
  `tests/self_hosted_component_contract_smoke.sh`; the structural source
  inventory and removed-path ratchets reported PASS.
- `self-host-bootstrap-linux` passed in 34m36s and records exactly one
  173259-line `gen2 == gen3` fixed point, exactly one receipt-bound driver
  adoption, exactly one Pergyra-built DRV-2 installation, and exactly one
  focused let-receipt PASS marker.
- This directive is complete and authorizes no successor rung. A new edit lease
  requires a fresh production executable falsifier. The malformed-enum fuzz
  finding remains separate waiting work.
