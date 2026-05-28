# Doc Link Checker -- Intent / Contract

**Status:** *rung-2 minimal* (2026-05-27). Reads
[`docs/INDEX.md`](../../../../docs/INDEX.md), extracts every Markdown link
target via substring scan (`](...)` pattern), and verifies each `.md`
target exists relative to the `docs/` directory.

## Intent

`docs/INDEX.md` is the human entry-point to the documentation set. When
a doc is renamed or removed, the index entry silently becomes a dead link
unless someone notices. This tool catches dead doc-internal links before
they ship.

## Input Contract

- **index_owner**: `docs/INDEX.md` (text, UTF-8, Markdown index with
  bracket-label plus parenthesized path links).
- Targets are resolved relative to `docs/` (since `INDEX.md` itself lives
  in `docs/`).
- Non-`.md` targets (URLs, anchors) are intentionally ignored: this tool
  only gates *internal documentation cohesion*, not external link
  liveness.

The path is fixed relative to repository root.

## Output Contract

JSON document on stdout, conforming to schema
`pgy.selfhost.doc-link-checker.v1`:

```json
{
  "schema": "pgy.selfhost.doc-link-checker.v1",
  "ok": true,
  "source": {
    "index_owner": "docs/INDEX.md"
  },
  "counts": {
    "total_links": 0,
    "md_links": 0,
    "missing_links": 0
  },
  "findings": []
}
```

- `ok = (counts.missing_links == 0)`.
- `findings[]` carries one entry per dead link, capped at 8:
  `{ "kind": "missing_link" | "input_error", "path": "docs/X",
     "location": "docs/INDEX.md" }`.

Exit code: `0` on `ok:true`, `1` on `ok:false`. Missing input reports
`input_error` and exits `1`.

## Oracle

The shell drift detector is `grep -c '](' docs/INDEX.md` for total link
count and a per-target `[[ -f docs/$path ]]` for liveness. There is no
existing C-side doc-link smoke today; the Pergyra origin is the primary
implementation and the shell loop is the auxiliary parity backend.

The parity rung (`src/self_hosted/parity/`) asserts:

- The Pergyra origin exits `0` on the clean repo (no dead links).
- Emitted JSON byte-matches `expected/clean.json`.
- The `md_links` count matches a shell-side `.md` link count.
- A synthetic dead-link fixture (rewrite one link target to a path that
  does not exist) yields `rc=1` with a `"kind":"missing_link"` finding.

## Why Now

This is the *seventh* soft self-host tool. It introduces the
*path-extraction* pattern (substring slice between `](` and `)`)
which is the same shape future tools will need for parsing
`#include "path/to/header.h"` or `import "module/path.pgy"` statements.
Shipping this pattern early surfaces the slicing-precision gaps before
they bite a tool that consumes compiler source.

## Not In Scope

- Anchor (`#fragment`) link validation.
- Outbound URL liveness checks.
- Recursive crawl of links inside *other* docs files (only
  `docs/INDEX.md` is scanned).
- Backtick-quoted target text vs unquoted (treated identically; the path
  inside `(...)` is the only thing checked).
