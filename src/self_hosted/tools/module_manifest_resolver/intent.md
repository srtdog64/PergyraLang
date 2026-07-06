# Module Manifest Resolver -- Intent / Contract

**Status:** *rung-2 fact-table owned* (2026-07-06). Reads
[`docs/language_module_manifest.json`](../../../../docs/language_module_manifest.json)
from the default manifest path or an explicit runner argument, counts modules,
beta_blocker entries, and stable-subset
modules, validates that every module has a complete required-field set
through the shared JSON fact-table owner, and emits the validator schema.

## Intent

The language module manifest is the freeze surface for the soft self-host
module-resolver gate. Tools downstream from beta require a stable count of
modules, a known set of beta-blocker modules, and the assurance that every
module entry carries all canonical fields. This tool is the *first*
Pergyra-origin gate on that JSON shape; it ships before any of the planned
`pgy.foundation` / `pgy.core` / `pgy.host` surface lifts so drift is
caught at the manifest level, not at consumer level.

## Input Contract

- **manifest_owner**: `docs/language_module_manifest.json` by default, or
  `Args()[0]` when supplied by a TestHarness parity runner. The manifest is
  source-of-truth for the module roadmap and is hand-edited by the BDFL
  on roadmap changes.

The path is relative to the runner's cwd. Parity runners pass the manifest path
explicitly so scratch negative fixtures can reuse the same tool without copying
or aliasing the source.

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

The TestHarness-projected `expected/clean.json` artifact is the clean-output
oracle. The missing-modules, nested-modules, and nested-field negative verdicts
also have TestHarness-projected expected JSON artifacts. There is no separate
shell count oracle for `"name":`, `"beta_blocker": true`, or
`"status": "stable-subset"`, and shell does not own the negative finding
kind/key checks; those verdicts are owned by the Pergyra JSON fact-table
implementation and compared through the ArtifactZone comparator.

The parity rung (`tests/self_hosted/parity/`) asserts:

- The Pergyra origin exits `0` on the live manifest.
- Emitted JSON byte-matches `expected/clean.json` on the live manifest.
- Synthetic missing-modules-key, nested-modules, and nested-field fixtures exit
  `1` and byte-match their expected negative JSON artifacts.

## Why Now

This is the *fifth* soft self-host tool. The first four shared a single
JSON or Markdown input shape; this one operates on the *module-manifest
JSON* that downstream tools will consume to bootstrap their own
language-surface contracts. Validating the manifest first removes the
"resolver finds stale fields" failure mode before any resolver-style
tooling is written.

## Not In Scope

- Validating module name patterns / dependency graph cycles.
- `surfaces[]` inner element schema checks (string array contents).
- Manifest schema migration (`schema: 2` etc.).
