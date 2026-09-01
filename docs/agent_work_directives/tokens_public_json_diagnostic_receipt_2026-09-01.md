# Tokens Public JSON Diagnostic Receipt

Status: ACTIVE — LOCAL GATES GREEN, EXACT CI PENDING

Exact base revision: `cb65af9afda1af9533efefab2d1446e60462af41`

This directive coordinates one bounded executable replacement. It is not a
semantic owner, SoT registry, progress counter, or completion claim.

## Shared objective card

- Objective: make public `SOURCE --tokens --error-format=json` enter the
  installed Pergyra lexer while preserving byte-exact successful token stdout,
  and publish an invalid-source-character failure as one lexer-owned public
  JSON receipt instead of stopping at the C adapter's text-only predicate.
- Priority order: preserve successful token bytes and formatter token facts;
  give the Pergyra lexer one stable invalid-token identity; carry one admitted
  JSON Bool with the existing token request; reuse the shared wire and opaque C
  process owner; keep every other source-inspection mode unchanged.
- Fact owners: `src/self_hosted/lexer/scan_owner.pgy` continues to own token
  facts and the exact invalid-character detection point. A responsibility-named
  lexer public-diagnostic owner will own `PGY_LEX_INVALID_TOKEN`, stage/layer,
  cause, fix, and message projection. The shared public wire renderer remains
  the sole serialization owner.
- Production entrypoint: default installed
  `pgy SOURCE --tokens --error-format=json`.
- Direct C-owned bypass to delete: the JSON rejection in
  `driver_self_host_source_stdout_mode` for the tokens case and direct child
  passthrough that cannot separate successful token stdout from a private
  diagnostic wire.
- Last legitimate consumers: the installed DRV-2 token read executor, followed
  by the existing opaque public-diagnostic stdout process owner and the public
  stdout/stderr boundary.
- Forbidden fallback: C lexer diagnostic meaning or message parsing, native
  retry/preflight, a second lex, token-text reconstruction, formatter behavior
  changes, dual text/JSON emission, partial/private wire output, environment
  inference, or JSON admission for capability manifest, DIR, raw MIR JSON, or
  machine manifest.
- Focused gate:
  `tests/self_hosted/parity/public_tokens_json_diagnostic_receipt_owner.sh`.
- Falsifying cases: valid text/JSON/native requests must have byte-identical
  token stdout; the invalid `~` character must preserve current text behavior
  and publish exact lexer-owned JSON on public stderr only; missing, silent,
  malformed, absent, and crosswired child receipts must fail without stdout or
  native timing; formatter callers must retain the existing path-independent
  `LexContentFacts(content)` surface; excluded modes must remain text-only.

## Edit scopes and overlap

- Lexer scope: preserve `LexContentFacts(content)` for formatter consumers and
  add one public-diagnostic projection at the existing invalid-character branch.
- Pergyra request scope: retain the token request variant name, add one Bool,
  and accept one exact private argv spelling.
- C selection/process scope: admit JSON only for `dump_tokens`, select the
  private spelling, and reuse the existing AST/MIR opaque process owner. Do not
  duplicate wire validation or import lexer meaning into C.
- Gate scope: one token success/parity/negative gate plus exact installed-CLI CI
  aggregation. Existing formatter and text-token gates remain authoritative for
  their behavior.
- Documentation scope: refresh collaboration and handoff only from observed
  evidence. SoT status and project percentages do not change from request
  carriage alone.
- Protected unrelated untracked paths remain outside inspection, edit, and
  staging: `docs/compiler_architectures/`, `pgy-80135c2c/`, and
  `pgy-91d769ec/`.

The primary task is the sole edit, integration, commit/push, and exact-CI
observation owner. Outputs before the focused gate passes are implementation
candidates, not completion evidence.

## Opening evidence

- With the installed child bound to a missing path, public text `--tokens`
  reaches the installed-driver availability failure while the otherwise-equal
  JSON request stops at `--tokens options are outside the installed self-host
  driver contract`. Published AST text and JSON both reach the child control.
- On an invalid `~` character, installed public/direct text reaches the Pergyra
  lexer and emits `LEXER ERROR: invalid source character at line 2`. The normal
  native JSON compile already owns `PGY_LEX_INVALID_TOKEN`, stage `lex`, layer
  `syntax`, cause `lex:invalid_token`, and fix
  `remove-or-escape-character`; the Pergyra lexer has not yet carried that
  identity.
- Capability manifest and DIR are not folded into this rung. Their successful
  outputs are Pergyra-owned, but their complete semantic failure domains do not
  yet carry public identities; routing JSON now would create a partial contract.
  Raw MIR JSON and machine manifest remain excluded for their separately owned
  output and evidence contracts.
- SoT opens unchanged at `88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`, with 9
  blockers. This rung is bounded inside `diagnostic.catalog`; project forecast
  remains 83%.

## Local implementation evidence

- The token request now carries one admitted Bool, the lexer assigns the
  existing `PGY_LEX_INVALID_TOKEN` facts at the invalid-character branch, and
  the installed adapter maps public JSON selection to one private argv spelling.
  Successful token rendering and the formatter-facing `LexContentFacts`
  surface remain unchanged.
- The shared public-diagnostic stdout process owner now admits token, AST, and
  MIR kinds. It owns child capture and wire relay; the token C path contains no
  lexer code, stage/layer, cause, fix, or message meaning.
- A warning-clean launcher and fresh isolated Pergyra-built DRV-2 pass the
  focused token JSON gate, existing text-token and formatter gates, existing
  AST/MIR JSON gates, and the complete installed CLI aggregate. The aggregate
  observes the focused token marker exactly once.
- Component, hard self-host, build-source inventory, SoT edge, Gate
  single-owner, protocol registry, and substitution-velocity contracts pass.
  The formatter gate initially exposed its retired exact-string assertion for
  `LexContentFacts`; it now verifies the public renderer's typed scan facts and
  the formatter's preserved wrapper separately.
- Observed census remains `88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`, with 9
  blockers. `diagnostic.catalog` remains `BRIDGE`; this is consumer migration,
  not row closure or a project-percentage increment.
- Exact CI and publication evidence are still pending. No successor executable
  rung is authorized by these local results.
