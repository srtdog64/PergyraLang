# Parser Statement-Head Unexpected-Token AST JSON Receipt

Status: COMPLETE — PUBLISHED, EXACT-CI GREEN

Exact base revision: `7fe0c6d3f08ef5871b27af5869c5a8728b22ee53`

This directive coordinates one bounded executable parser-receipt migration.
It is not a semantic owner, SoT registry, general parser recovery campaign,
progress increment, or completion claim.

## Shared objective card

- Objective: make installed public `SOURCE --ast --error-format=json` publish
  an admitted parser-owned JSON receipt when the statement dispatcher cannot
  read a statement-head identifier, instead of reporting only a generic child
  failure.
- Priority order: preserve the parser decision and silent text-mode bytes;
  publish the already reached empty statement-head fact through the selected
  JSON projection; retain valid AST bytes and all admitted parser receipts;
  keep expression parsing unchanged; keep C transport opaque and fail closed.
- Fact owner: `ReadIdent` plus the initial empty-head branch of
  `ParseOneStmtCore` own whether the statement dispatcher obtained a supported
  statement head. The branch publishes that existing fact only through the
  selected JSON projection. No expression call-graph change, `=` spelling
  comparison, or second start-token recognizer is admitted.
- Production entrypoint: default installed
  `pgy SOURCE --ast --error-format=json`.
- Direct bypass to delete: `ParseOneStmtCore` exits immediately when the
  statement-head identifier is empty, so exact `=` reaches no selected public
  diagnostic renderer even though the dispatcher already owns the rejection.
- Last legitimate consumer: the initial empty-head branch in
  `ParseOneStmtCore`, before expression parsing, statement kind, graph,
  terminator, AST executor, or C relay processing.
- Forbidden fallback: an `=` spelling special case; a second primary-start
  table or token recognizer; reporting from C; native retry or preflight;
  parsing native or legacy error text; source/program-root rescanning;
  partial/private wire output; changing native parser semantics; or widening
  this rung to the other bare parser exits.
- Focused gate:
  `tests/self_hosted/parity/public_ast_json_diagnostic_receipt_owner.sh`.
- Falsifying case: exact one-byte source `=` must keep direct/public text
  rejection byte-identical and silent. Direct JSON must publish the private
  marker plus a parser-owned receipt; public JSON must relay its exact JSON on
  stderr only; explicit native AST must retain line 1, column 1 and the same
  `parse` / `syntax` / `PGY_PARSE_SYNTAX` /
  `parse:unexpected_token` / `check-syntax` identity. Valid AST,
  callable-contract, and statement-terminator controls must remain unchanged.

## Opening evidence

- The read-only differential audit at
  `docs/audits/public_native_parser_receipt_differential_fuzz_audit_2026-09-02.md`
  minimized exact one-byte `=`: public JSON reported
  `self-host driver failed (exit 1) emitting AST diagnostic`, while native
  published the five-axis parse identity at line 1, column 1.
- Fresh measurement at the exact base with `=\n` reproduces the same owner
  boundary. Public JSON exits 1 with generic driver text, direct
  `--ast-json-diagnostic-verified` exits 1 with empty stdout and stderr, and
  explicit native JSON publishes the exact five-axis identity. Direct/public
  text remain silent.
- Exact `=` exits earlier than the final expression fallback: `ReadIdent`
  returns an empty head and `ParseOneStmtCore` exits immediately. Three broader
  designs were rejected as unnecessarily broad: returning
  `ParserExpressionInvalid` from the primary parser, adding an observed primary
  wrapper, and extracting primary/unary `StartKind` functions. Their validation
  exposed the same `parser_parity.sh` failure on `async_demo.pgy`: incomplete
  source-location observations. A one-statement minimization identified
  `ch <- 99;` as the falsifier while receive and ordinary unary expressions
  stayed green. After both expression owners were restored byte-for-byte, the
  same falsifier remained. The current typed-AST text owner has no
  `ChannelSend:` kind, so this is a separate pre-existing fail-closed registry
  gap, not evidence that the bounded statement-head receipt changed channel
  semantics. The accepted candidate publishes only the already owned empty
  statement-head fact. It adds no token table and does not inspect `=`.
- SoT remains `88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`, with 9 blockers. This
  is one executable consumer migration inside `diagnostic.catalog`, not whole-
  row closure; project forecast remains 83%.

## Coordination and validation bounds

- The primary task is the sole code, focused-gate, integration, commit/push,
  and exact-CI owner for this rung.
- Edit only the parser-diagnostic owner, the reached statement consumer, the
  existing AST receipt gate, required static ratchets, and current coordination
  snapshots. Expression owners must remain byte-identical to the exact base.
- Protected unrelated untracked paths remain outside inspection, edit, and
  staging: `docs/compiler_architectures/`, `pgy-80135c2c/`, and
  `pgy-91d769ec/`.
- Run static owner gates within 60 seconds, focused parity within five minutes,
  and exact remote CI only after a fresh installed DRV-2 passes the focused
  slice. Until then all outputs are implementation candidates.

## Local implementation evidence

- Local implementation revision is
  `61189ba7e261f329f5238b4de49958f0cc040c8f`, based on the exact opening
  revision above. Fresh installed DRV-2 SHA-256 is
  `1534140B2754D862473FA8B1F60B687297B6F96568BD9CD62A8A4279AB8B8901`.
- The focused AST receipt gate passes valid AST bytes, exact callable-contract
  receipts, statement-terminator receipts, exact one-byte `=` statement-head
  receipt, native five-axis parity at line 1/column 1, silent text channels,
  malformed/missing/crosswired negatives, and opaque C transport.
- The complete installed-driver CLI aggregate passes public semantic/parser/
  lexer/LLVM receipts, AST/tokens, machine/device manifests, optimization
  profiles, REPL, formatter, MIR-C world/action parity, and transaction
  rejection. Hard-owner, diagnostic registry, parser/lexer diagnostic, layered
  diagnostic, and preparation gates are green.
- `parser_parity.sh` is explicitly not claimed green: it fails at the existing
  `ChannelSend:` source-location kind gap described above. That blocker neither
  owns nor invalidates this receipt, but must be closed before claiming the
  189-source parser parity suite.

## Publication evidence

- Implementation `61189ba7e261f329f5238b4de49958f0cc040c8f` and publication
  checkpoint `288722ee34c2e121dd1471ed9fafb23c086fb615` are on `origin/main`.
- Exact-head CI run `33672957959` completed `30/30` success in 35 minutes 47
  seconds. `build-linux` passed in 18 minutes 6 seconds. Full self-host passed
  in 35 minutes 26 seconds with `gen2 == gen3 (173483 lines)` and installed a
  Pergyra-built DRV-2.
- The same full self-host job emitted the exact focused marker
  `[self-host-public-ast-json-diagnostic] ... statement-head unexpected-token
  parser receipts ...: PASS`, passed both installed-driver CLI markers, and
  completed the three-source policy-corpus census. The publication falsifier
  is closed.
