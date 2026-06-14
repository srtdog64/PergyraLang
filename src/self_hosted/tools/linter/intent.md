# Linter -- Intent / Contract

**Status:** *rung-1 tool* (2026-06-15). This Pergyra-written linter reads a
committed source fixture and emits LSP-style diagnostics as JSON. It is a
self-hosted tool surface, not a compiler-core replacement.

## Intent

Exercise source text scanning, string slicing, arrays, JSON emission, and
filesystem reads through Pergyra itself. The tool is deliberately small: it
proves a deterministic diagnostic pipeline without becoming a second parser.

## Input Contract

- The tool reads `src/self_hosted/tools/linter/fixture.pgy` through
  capability-bound `ReadFile`.
- Paths are project-root relative.
- The checked rules are trailing whitespace, tab indentation, lines longer than
  100 columns, `TODO`, and `FIXME`.

## Output Contract

The tool writes one JSON array of LSP Diagnostic objects to stdout. Each
diagnostic contains:

- `range.start.line` / `range.start.character`
- `range.end.line` / `range.end.character`
- `severity`
- `source: "pgy-lint"`
- `message`

Severity mapping follows LSP values: `1` Error, `2` Warning, `3` Information,
and `4` Hint.

## Oracle

`src/self_hosted/parity/linter_parity.sh` compiles and runs this tool through
the C backend, compares the emitted JSON to
`src/self_hosted/tools/linter/expected/diagnostics.json`, and checks LLVM output
byte-for-byte when the active compiler supports the LLVM backend. LLVM-disabled
builds skip only the LLVM half; C parity remains mandatory.

## Run

```sh
pgy src/self_hosted/tools/linter/main.pgy --backend=c --run
```
