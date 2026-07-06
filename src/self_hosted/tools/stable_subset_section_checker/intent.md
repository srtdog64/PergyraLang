# Stable Subset Section Checker -- Intent / Contract

**Status:** *rung-2 minimal* (2026-07-06). Reads
[`docs/107_beta_stable_subset.md`](../../../../docs/107_beta_stable_subset.md),
counts `^## ` section headings, and emits the schema document with a real
`counts.sections` value. `findings[]` lists missing canonical anchors.

## Intent

Verify that the beta stable-subset freeze document
([`docs/107_beta_stable_subset.md`](../../../../docs/107_beta_stable_subset.md))
contains every canonical section anchor required by the BDFL freeze decision.
A missing anchor means the contract was silently weakened or restructured
without owner sign-off; the tool surfaces that drift before it ships.

## Input Contract

- **manifest_owner**: `docs/107_beta_stable_subset.md` (text, UTF-8,
  Markdown).
- The tool does not parse Markdown structure; it line-scans for `^## ` headings
  and compares the visible titles against a fixed canonical list.

The path is passed relative to the repository root.

## Output Contract

JSON document on stdout, conforming to schema
`pgy.selfhost.stable-subset-section.v1`:

```json
{
  "schema": "pgy.selfhost.stable-subset-section.v1",
  "ok": true,
  "source": {
    "manifest_owner": "docs/107_beta_stable_subset.md"
  },
  "counts": {
    "sections": 0,
    "expected": 6,
    "missing": 0
  },
  "findings": []
}
```

- `ok = (counts.missing == 0)`.
- `findings[]` carries one entry per missing canonical anchor:
  `{ "kind": "missing_section" | "input_error", "title": "...", "location": "..." }`.
- `findings[]` ordering is the canonical list order (not source order), so the
  emitted JSON is reproducible regardless of how sections were reshuffled.

Exit code: `0` on `ok:true`, `1` on `ok:false`. Missing manifest reports an
`input_error` finding and exits `1`.

## Oracle

The clean oracle is the committed expected JSON artifact
`expected/clean.json`, compared through the shared ArtifactZone/TestHarness
path. There is no existing C-side smoke that gates this contract today, so the
Pergyra checker owns the section count, canonical anchor list, and missing
anchor verdict.

The parity rung (`tests/self_hosted/parity/`) asserts:

- The Pergyra origin exits `0` on the clean repo.
- The emitted JSON byte-matches `expected/clean.json` on the clean repo.
- A synthetic missing-section fixture exits `1` and reports the stripped
  canonical anchor.
- Both C and LLVM legs compile the same self-hosted checker.

Do not add shell `grep -c '^## '` reconstruction as a second clean oracle; fix
the self-hosted owner or add a negative fixture instead.

## Negative Fixture

`expected/missing_section.json` will hold the verdict when a synthetic copy
of the manifest has one canonical anchor stripped. The parity script builds
this fixture in a temporary directory so the live `docs/` remains untouched.

## Why Now

This is the *second* soft self-host tool. It reuses the diagnostic catalog
checker's read-and-scan pattern with a different input and a different
canonical-anchor invariant, proving that the
`ReadFile` + `Split` + `StringContains` + `StringJoin` surface scales to a
second tool without further LLVM/runtime gaps.

## Not In Scope

- Markdown structure / link / heading-level validation (out of scope for
  rung-2; would need a small Markdown reader).
- Cross-doc consistency between `docs/107` and `docs/19_design_philosophy.md`
  (different tool).
- Auto-fix / docs rewrite.
