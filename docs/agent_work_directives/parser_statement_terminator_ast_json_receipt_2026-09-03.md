# Parser Statement-Terminator AST JSON Receipt

Status: IMPLEMENTATION COMPLETE — PUBLISHED, EXACT CI GREEN

Exact base revision: `24397904d4a2f4a9b650205fde0b3020cb251b87`

Implementation revision: `66ffd0e2149b3ca03e2c804b4f7d5c42410e823b`

This directive coordinates one bounded executable diagnostic-receipt repair.
It is not a semantic owner, SoT registry, progress increment, general parser
recovery campaign, or completion claim.

## Shared objective card

- Objective: make installed public `SOURCE --ast --error-format=json` publish
  an admitted parser-owned JSON receipt when the reached top-level expression
  statement is followed by a non-terminator token, instead of rejecting the
  Pergyra child's legacy text as a malformed private receipt.
- Priority order: preserve the parser decision and text-mode bytes; carry the
  already admitted JSON projection to the reached statement-terminator branch;
  publish the existing five-axis public syntax identity; keep C transport
  opaque and fail closed; retain valid AST bytes and existing callable-contract
  receipts.
- Fact owners: `ConsumeStmtTerminatorOpt` retains statement-terminator
  recognition; `ParseOneStmtCore` retains the reached expression-statement
  rejection; `parser/diagnostic_owner.pgy` retains parse reason/fix and public
  identity; `public_diagnostic_receipt_owner.pgy` remains the sole wire
  renderer.
- Production entrypoint: default installed
  `pgy SOURCE --ast --error-format=json`.
- Direct bypass to delete: the expression-statement fallback calls legacy
  `ConsumeStmtTerminator`, whose `Fail` prints text and exits even when the AST
  request already selected `ParseDiagnosticPublicJson`.
- Last legitimate consumer: `ParseOneStmtCore` at the failed
  `ConsumeStmtTerminatorOpt` result, before the DRV-2 AST executor and opaque C
  relay observe the child payload.
- Forbidden fallback: C wording recovery or parser identity, native retry or
  preflight, source/program-root rescanning, a second terminator recognizer,
  changing native parser semantics, parsing the legacy `PARSE ERROR` string,
  partial/private wire output, or admitting the separate silent `=` family.
- Focused gate:
  `tests/self_hosted/parity/public_ast_json_diagnostic_receipt_owner.sh`.
- Falsifying case: exact source `c C` must retain nonzero text-mode legacy
  output, while direct private-JSON and installed public JSON AST requests must
  carry `stage=parse`, `layer=syntax`, `code=PGY_PARSE_SYNTAX`,
  `cause_ir=parse:unexpected_token`, and `fix_source=check-syntax`. Public JSON
  must use stderr only with no private marker or native timing. Explicit native
  AST must retain the same five-axis identity. Valid AST bytes and the existing
  callable-contract receipt remain unchanged.

## Opening evidence

- On the exact base, public and explicit-native AST both reject `c C`. Native
  publishes the five-axis syntax identity at line 1, column 3; public reports
  only `pgy: self-host JSON diagnostic receipt is malformed`.
- Direct `pgy-self-driver --ast-json-diagnostic-verified` prints
  `PARSE ERROR: expected statement terminator ...` on stdout with no private
  marker, proving that the C relay correctly fails closed and that the missing
  admission is inside the Pergyra parser request path.
- The JSON projection already reaches
  `CompileSourceToAstArtifactForPublicDiagnosticRequest` and
  `ParseRootProgramArtifactWithDiagnosticProjection`. Callable-contract parser
  rejections already prove the parser-owned receipt renderer and opaque relay.
- `=` is intentionally excluded: its direct selected request exits without a
  payload, so it belongs to a broader missing-parser-payload family and cannot
  be inferred from this terminator branch.
- SoT remains `88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`, with 9 blockers. This
  is an executable consumer migration inside `diagnostic.catalog`, not whole-
  row closure; project forecast remains 83%.

## Coordination and validation bounds

- The primary task is the sole code, focused-gate, integration, commit/push,
  and exact-CI owner for this rung.
- Edit only the existing Pergyra parser statement/diagnostic owners, the
  existing AST receipt gate and its bounded fixture, required generated or
  aggregate wiring if evidence demands it, and current coordination snapshots.
- Protected unrelated untracked paths remain outside inspection, edit, and
  staging: `docs/compiler_architectures/`, `pgy-80135c2c/`, and
  `pgy-91d769ec/`.
- Run static owner gates within 60 seconds, focused parity within five minutes,
  and the exact remote matrix only after the installed focused slice is green.
  Until then all outputs are implementation candidates, not completion proof.

## Observed local evidence

- A fresh Pergyra-built DRV-2 at SHA-256
  `D199A71301476FFC68916E98B81C9A5D0751A9E06A0DABEA846F5E15EE1F7992`
  emits the private marker plus parser-owned JSON receipt for exact `c C`.
  The installed public JSON AST path relays the byte-exact JSON on stderr only;
  public and direct text modes retain identical legacy text output.
- Public and explicit-native JSON receipts share exact `parse` / `syntax` /
  `PGY_PARSE_SYNTAX` / `parse:unexpected_token` / `check-syntax` identity. The
  native oracle retains line 1, column 3 and `Expected ';' after expression`;
  the Pergyra owner records `expression_statement_terminator`,
  `expression_statement`, and source offset 2 without C wording recovery.
- The focused AST receipt gate, existing callable-contract parser receipt,
  diagnostic registry, parser/lexer and layered diagnostic contracts, hard
  self-host contract, and the complete installed-driver CLI aggregate pass.
  Valid AST bytes, token/LLVM receipts, MIR/C/LLVM semantic receipts, REPL,
  formatter, and machine-manifest paths remain green.
- `stmt_owner.pgy` remains at the existing 600-line shrink-only cap. The broad
  local size gate still reports only the unrelated pre-existing
  `src/parser/ast_expr_control_accessors.c` 725/699 violation.
- Local SoT edge and live adequacy mutations pass. The formal adequacy model is
  a declared local skip because neither `rocq` nor `coqc` is installed; exact
  CI remains the proof owner. The broad component inventory was likewise left
  to exact CI under the repository's 60-second local budget and passed there.
- SoT remains `88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`, with 9 blockers;
  project forecast remains 83%. This closes one reached executable receipt
  seam and does not close the whole `diagnostic.catalog` row.
- Implementation `66ffd0e2149b3ca03e2c804b4f7d5c42410e823b` is published on
  `origin/main`. Exact-head workflow-dispatch run `33656673799` at
  `64ff69ee00df4d6b6d70b6cf4c39868c5e166eea` completed `30/30` success in
  33 minutes 2 seconds. `build-linux` passed in 25 minutes 38 seconds. Full
  self-host passed in 32 minutes 41 seconds with
  `gen2 == gen3 (173470 lines)`, installed a Pergyra-built DRV-2, and passed
  the focused AST marker plus the complete installed CLI aggregate.
