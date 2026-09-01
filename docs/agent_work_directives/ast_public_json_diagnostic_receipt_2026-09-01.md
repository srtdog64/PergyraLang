# AST Public JSON Diagnostic Receipt

Status: DONE — EXACT CI GREEN

Exact base revision: `615529ffecc042216fc545699f5a3e87b1ba9020`

Implementation revision: `9ce212627268a26fe20f660be7eda5b66a1f4d19`

Exact-CI gate repair revision: `85b6ed3a750cca205bb92007a74cf622d92ec7cc`

This directive coordinates one bounded executable replacement. It is not a
semantic owner, SoT registry, progress counter, or completion claim.

## Shared objective card

- Objective: make public `SOURCE --ast --error-format=json` enter the existing
  Pergyra parser diagnostic projection while preserving byte-exact successful
  AST stdout, instead of stopping at the C adapter's text-only source-stdout
  predicate.
- Priority order: preserve AST bytes; reuse parser-owned diagnostic identity;
  carry one admitted JSON Bool with the existing AST request identity; keep C
  transport opaque and fail closed; leave every other source-stdout mode
  unchanged.
- Fact owner: existing `DriverCliSourceAstStdout(String)` owns AST request
  identity and migrates in place to carry the admitted Bool.
  `CompileSourceToAstArtifactForPublicDiagnosticRequest` and parser diagnostic
  projection own AST/parser facts and public diagnostic meaning. The shared
  public wire renderer remains the sole serialization owner.
- Production entrypoint: default installed
  `pgy SOURCE --ast --error-format=json`.
- Direct C-owned bypass to delete: the unconditional non-text rejection in
  `driver_self_host_source_stdout_mode` for the AST case and direct child
  passthrough that cannot distinguish an AST success payload from a private
  diagnostic wire.
- Last legitimate consumers: the installed DRV-2 AST read executor, followed by
  one opaque public-diagnostic stdout process owner and the public stdout/stderr
  boundary.
- Forbidden fallback: C message parsing or diagnostic identity, native retry or
  semantic preflight, a second parse, AST text re-rendering, dual text/JSON
  emission, partial/private wire output, environment inference, or admission of
  JSON for tokens, capability manifest, DIR, MIR JSON, or machine manifest.
- Focused gate:
  `tests/self_hosted/parity/public_ast_json_diagnostic_receipt_owner.sh`.
- Falsifying cases: valid text and JSON requests must have byte-identical AST
  stdout; the reached duplicate callable-contract rejection must publish the
  exact parser-owned JSON receipt on stderr only; missing/malformed/absent/
  crosswired child receipts must fail without stdout or native timing; all
  excluded source-stdout modes must retain their current text-only selection.

## Edit scopes and overlap

- Pergyra request scope: retain the AST request variant name and add one Bool;
  add one exact private argv spelling and consume the existing parser projection.
- C selection scope: admit JSON only when `dump_ast` owns the one selected
  source-stdout mode.
- C process scope: own generic opaque public-diagnostic stdout capture once and
  migrate the existing MIR diagnostic caller to that owner; do not duplicate
  receipt validation or give C semantic meaning.
- Gate scope: one AST success/parity/negative gate plus exact installed-CLI CI
  aggregation. Do not add behavioral claims to the component inventory.
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

- With the installed child bound to a missing path, public text `--ast` reaches
  the installed-driver availability failure. The otherwise-identical JSON
  request stops earlier at `--ast options are outside the installed self-host
  driver contract`.
- A fresh current-source DRV-2 already publishes the exact parser-owned public
  receipt for the reached duplicate callable-contract fixture through
  `CompileSourceToAstArtifactForPublicDiagnosticRequest`; current MIR/C/LLVM
  diagnostic requests prove that projection and shared wire. AST has not yet
  admitted or consumed the Bool.
- Raw MIR JSON is not folded into this rung: its existing JSON-diagnostic request
  emits a human-readable MIR diagnostic on success rather than raw MIR JSON, so
  a C-only route change would violate its success contract.

## Observed local evidence

- The native launcher is warning-clean and a current-source Pergyra-built DRV-2
  installs successfully. The launcher, DRV-2, and machine manifest are tested
  from one isolated sibling directory.
- Direct text/private-JSON, public text/JSON, and native AST success stdout are
  byte-identical. The reached callable-contract failure preserves existing text
  output and relays the exact parser-owned JSON receipt on public stderr only.
- Missing, silent-success, malformed, absent, and crosswired child paths fail
  without public stdout, private marker leakage, or native timing. Tokens,
  capability manifest, DIR, MIR JSON, machine manifest, and verbose AST remain
  outside this bounded JSON contract.
- The opaque diagnostic stdout process owner is shared by AST and MIR. An
  initial regression that attempted JSON wire validation for text-mode MIR was
  caught by the existing MIR gate; explicit `emit_json_diagnostic` carriage
  restores the text contract. Focused AST, existing AST/MIR, and the complete
  installed-driver CLI aggregate all pass.
- Component/hard, source inventory, SoT edge, Gate single-owner, protocol, and
  substitution velocity contracts pass. Shrink-only caps were not raised:
  `pgy_driver.c` is 340/340, `self_host_driver.c` is 254/270, and the Pergyra
  request owner is 319/320.
- SoT remains `88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`, with 9 blockers. This is
  an executable consumer migration inside `diagnostic.catalog`, not whole-row
  closure; project forecast remains 83%.

## Observed publication evidence

- The first implementation run `33493015435` reached fixed point, installed the
  receipt-bound DRV-2, and finished 29/30. It correctly exposed one stale
  structural assertion in the source-inspection optimization-profile gate:
  that gate still required the pre-rung one-argument AST request shape.
- Repair `85b6ed3a750cca205bb92007a74cf622d92ec7cc` keeps the other inspection
  requests path-only while requiring AST's exact `(String, Bool)` path plus
  diagnostic-format shape and rejecting any third policy input. The complete
  optimization-profile behavior gate and the focused AST receipt gate pass
  locally.
- Exact repair run `33496180181` is green 30/30. Its full self-host log records
  exactly one each of `gen2 == gen3 (173198 lines)`, receipt-bound fixed-point
  adoption, Pergyra-built DRV-2 installation, the repaired optimization-profile
  marker, and the focused AST JSON diagnostic marker. `build-linux` took
  25m55s and full self-host took 24m42s.
- The bounded publication falsifier is closed. This directive authorizes no
  successor rung; a fresh production executable falsifier is required.
