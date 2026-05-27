# Module Manifest Resolver -- Intent / Contract

**Status:** *rung-2 minimal* (2026-05-27). Reads
[`docs/language_module_manifest.json`](../../../docs/language_module_manifest.json)
from a fixed path, counts modules, beta_blocker entries, and stable-subset
modules, validates that every module has a complete required-field set
via parallel substring counts (no JSON parser), and emits the validator
schema.

## Intent

The language module manifest is the freeze surface for the soft self-host
module-resolver gate. Tools downstream from beta require a stable count of
modules, a known set of beta-blocker modules, and the assurance that every
module entry carries all canonical fields. This tool is the *first*
Pergyra-origin gate on that JSON shape; it ships before any of the planned
`pgy.foundation` / `pgy.core` / `pgy.host` surface lifts so drift is
caught at the manifest level, not at consumer level.

## Input Contract

- **manifest_owner**: `docs/language_module_manifest.json` (text, UTF-8,
  pretty-printed JSON with one field per line). The manifest is
  source-of-truth for the module roadmap and is hand-edited by the BDFL
  on roadmap changes.

The path is fixed relative to repository root; no CLI argument surface yet.

## Output Contract

JSON document on stdout, conforming to schema
`pgy.selfhost.module-manifest-resolver.v1`:

```json
{
  "schema": "pgy.selfhost.module-manifest-resolver.v1",
  "ok": true,
  "source": {
    "manifest_owner": "docs/language_module_manifest.json"
  },
  "counts": {
    "modules": 17,
    "beta_blockers": 3,
    "stable_subset": 3,
    "missing_fields": 0
  },
  "findings": []
}
```

- `ok = (counts.missing_fields == 0)`.
- `findings[]` carries one entry per anomaly:
  `{ "kind": "field_count_mismatch" | "missing_modules_key" |
     "input_error",
     "key": "...", "location": "..." }`.

Exit code: `0` on `ok:true`, `1` on `ok:false`. Missing manifest reports
`input_error` and exits `1`.

## Oracle

The shell drift detector is `grep -c` against the manifest for `"name":`,
`"beta_blocker": true`, `"status": "stable-subset"`. There is no existing
C-side smoke today that emits this structured JSON; the Pergyra origin is
the primary implementation and the shell grep is the auxiliary parity
backend.

The parity rung (`self_hosted/parity/`) asserts:

- The Pergyra origin exits `0` on the live manifest.
- Emitted JSON byte-matches `expected/clean.json` on the live manifest.
- Counts match shell grep ground truth on the live manifest.
- A synthetic missing-modules-key fixture (strip the `"modules":` key) is
  detected via `missing_modules_key` finding and `rc=1`.

## Why Now

This is the *fifth* soft self-host tool. The first four shared a single
JSON or Markdown input shape; this one operates on the *module-manifest
JSON* that downstream tools will consume to bootstrap their own
language-surface contracts. Validating the manifest first removes the
"resolver finds stale fields" failure mode before any resolver-style
tooling is written.

## Not In Scope

- Full JSON parsing (nested objects walked individually).
- Validating module name patterns / dependency graph cycles.
- `surfaces[]` inner element schema checks (string array contents).
- Manifest schema migration (`schema: 2` etc.).
