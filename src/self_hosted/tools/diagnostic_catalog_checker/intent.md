# Diagnostic Catalog Checker -- Intent / Contract

**Status:** *rung-2 complete* (2026-05-26). All five counters
(`codes`, `documented`, `missing`, `duplicates`, `orphans`) are **live**: the
Pergyra candidate reads `src/semantic/diag_codes.h` and
`docs/72_diagnostic_codes.md`, scans `#define PGY_CODE_` macros, counts
documented `PGY_*` catalog headings, checks every code literal for a docs
entry, detects duplicate header literal strings via a delimited seen-set, and
checks docs headings for orphan entries.

Rung-2 parity asserts agreement on all five counters against shell-grep
drift detectors. The C-side reference logic still lives in
[`tests/diagnostic_registry_smoke.sh`](../../../../tests/diagnostic_registry_smoke.sh)
and remains the C parity backend; the Pergyra side under
[`main.pgy`](main.pgy) is the candidate implementation.

Follow-up rungs: rung-3 three-way agreement (C / LLVM / Pergyra origin).

## Intent

Verify that every `PGY_CODE_*` literal defined in
[`src/semantic/diag_codes.h`](../../../../src/semantic/diag_codes.h) is documented
in [`docs/72_diagnostic_codes.md`](../../../../docs/72_diagnostic_codes.md), and
that the docs contain no orphan codes.

## Input Contract

- **code_owner**: `src/semantic/diag_codes.h` (text, UTF-8, `#define` macros
  for `PGY_CODE_*` / `PGY_CAUSE_*` / `PGY_FIX_*` literals).
- **docs_owner**: `docs/72_diagnostic_codes.md` (text, UTF-8, Markdown).

Both paths are passed relative to the repository root. The checker does not
load compiler internals -- it reads these two files as text.

## Output Contract

JSON document on stdout, conforming to schema `pgy.selfhost.diagnostic-catalog.v1`:

```json
{
  "schema": "pgy.selfhost.diagnostic-catalog.v1",
  "ok": true,
  "source": {
    "code_owner": "src/semantic/diag_codes.h",
    "docs_owner": "docs/72_diagnostic_codes.md"
  },
  "counts": {
    "codes": 0,
    "documented": 0,
    "missing": 0,
    "duplicates": 0,
    "orphans": 0
  },
  "findings": []
}
```

- `ok = (counts.missing == 0 && counts.duplicates == 0 && counts.orphans == 0)`.
- `findings[]` carries one entry per problem: `{ "kind": "missing" | "duplicate" | "orphan" | "input_error", "code": "PGY_CODE_...", "location": "..." }`.
- `findings[]` ordering is deterministic source order: header missing entries,
  header duplicate entries, then docs orphan entries.
- Counters remain the primary drift signal; `findings[]` gives the first
  machine-readable location surface for remediation.

Exit code: `0` on `ok:true`, `1` on `ok:false`. Missing owner files are reported
as `input_error` findings via the stable `FileExists(String) -> Bool` preflight
surface before `ReadFile(...)` is called.

## Oracle

`tests/diagnostic_registry_smoke.sh` is the C oracle for this rung-2 candidate.
The Pergyra implementation in `main.pgy` is authoritative only for its emitted
JSON/counter contract; it does not replace the C oracle until rung-3 three-way
agreement is shipped.

The parity rung (`src/self_hosted/parity/`) re-runs the catalog walk against the
same clean-repo inputs from both implementations and asserts:

- The C oracle exits `0` and the Pergyra tool exits `0` on the clean repo.
- `counts.missing`, `counts.duplicates`, and `counts.orphans` agree with the
  shell drift detectors.

The C oracle does not emit JSON today. The active parity rung compares exit
class on the clean repo, validates the Pergyra JSON shape against
`expected/clean.json`, checks live counters against shell drift detectors, and
runs a synthetic missing-code fixture that must emit `ok:false` and exit `1`.

## Negative Fixture

`expected/` will hold paired fixtures:

- `expected/clean.json` -- what the checker emits against the live repo with no
  drift. This is currently the rung-2 baseline.
- `expected/missing_code.json` -- what the checker emits when one
  `PGY_FAKE_DRIFT_FOR_SELFHOST` literal is added to a synthetic header without
  a docs entry.
- `expected/missing_input.json` -- what the checker emits when the code owner
  file is missing before `ReadFile(...)`.

## Why First

This tool is pure analysis: text in, JSON out, no backend complexity, no
runtime dependency surface beyond file I/O. It exercises the *required surface
before soft self-host* checklist
([docs/self_hosted/02_required_language_surface.md](../../../../docs/self_hosted/02_required_language_surface.md))
without forcing any new language feature.

## Not In Scope

- Parsing `cause_ir` / `fix_source` *bodies* (only the macro identifiers).
- Validating that diagnostic call sites use macros instead of raw strings (the
  existing `diagnostic-registry` smoke covers that and remains the owner).
- Producing fix suggestions.
