# Terminal Output Standard — 3-Tier Architecture

Last updated: 2026-05-04

Related documents:

- `docs/19_design_philosophy.md` §0 — systems-language identity (this
  standard serves the same identity: separation of state from rendering)
- `docs/120_vision_and_capability_audit.md` §4 — vision territory
  (Mode B Grid TUI is post-1.0 aspiration, recorded here for honest
  external description)
- `docs/122_managing_intent_drift.md` §4 — drift management (event
  emission is one of the recovery surfaces for runtime drift)

This document defines Pergyra's terminal-output discipline. The compiler
and runtime emit *structured events* over a single channel; renderers
*subscribe* to those events. Renderers are not part of the compiler.
This separation is the same discipline `docs/19` §0 applies to backends:
the source of truth is the IR, not any one consumer of it.

## 0. Goal

Single rule:

> **Core emits, subscribers render.**

The Pergyra compiler and runtime never assume their output is going to a
human, a CI script, or a TUI. They emit one canonical event stream. Any
of three consumer modes can subscribe to that stream:

| Tier | Role | Render target |
|---|---|---|
| **Core** | Headless event emitter | none — JSON to stdout / socket |
| **Mode A** | Stream CLI subscriber | sequential text lines |
| **Mode B** | Grid TUI subscriber | interactive widgets |

The separation is forced. It is not optional. A single feature inside the
compiler that decides "let me print a spinner here" violates the contract
and breaks every non-TTY consumer (CI, screen reader, IDE host).

## 1. Core — Headless Event Emitter

### 1.1 Event Schema

Every event is one line of JSON, written to stdout (or the configured
sink). Line-delimited JSON (JSONL) is the canonical wire format.

```json
{
  "schema_version": "1.0",
  "timestamp": 1714800000000,
  "agent": "compiler" | "runtime" | "lsp" | "driver",
  "stage": "parse" | "sema" | "hir" | "mir" | "air" | "emit_c" | "emit_llvm" | "run",
  "category": "diagnostic" | "trace" | "phase" | "state",
  "state": "begin" | "thinking" | "ok" | "error" | "done",
  "code": "PGY_CODE_*",
  "layer": "syntax" | "type" | "resource" | "concurrency" | "domain" | "backend" | "driver",
  "payload": { ... }
}
```

Required fields: `schema_version`, `timestamp`, `agent`, `category`,
`state`. Other fields are category-dependent.

`category` values:

- **`diagnostic`** — error / warning / hint. `code` / `layer` /
  `payload.message` required. Existing source: `driver_diag.c` JSON
  emitter
- **`trace`** — runtime intent / step events. Existing source:
  `pgy_runtime_observability_schema.h` event types (`intent.enter` /
  `step.begin` / `bind` / `materialize` / `transfer` / `step.ok` /
  `fail` / `mir.resource`)
- **`phase`** — compiler phase output (token dump, AST dump, AIR dump,
  etc.). Currently only AIR has JSON parity (`--air-json`); others are
  plain text and require migration (Phase 2 below)
- **`state`** — high-level driver state (`begin` / `thinking` / `done`)
  for long-running operations. Used by Mode B subscribers

### 1.2 Output Channel

- **stdout (default)** — line-delimited JSON, one event per line
- **local socket (post-1.0)** — for Mode B subscribers that need
  out-of-band communication

The compiler does not write progress indicators to stderr. stderr is
reserved for unstructured fatal output (panic / abort / assertion).

### 1.3 Existing Infrastructure (reuse)

The Core tier is partially built. New work composes existing helpers:

| Component | Path |
|---|---|
| Diagnostic JSON emitter | `src/compiler/driver_diag.c:11-36, 66-100` |
| LSP diagnostic JSON | `src/lsp/pgy_lsp_diagnostics.c` |
| Layer tag inference | `src/common/diagnostic_layer.c:67-80` |
| AIR JSON dump | `--air-json` flag |
| Intent trace event schema | `src/runtime/pgy_runtime_observability_schema.h:9-46` |
| JSON escape helper | `driver_json_emit_string` in `driver_diag.c` |

### 1.4 Versioning

Every event carries `schema_version`. Current: `"1.0"`. Breaking changes
bump to `"2.0"`. Subscribers reject unknown major versions and surface
`PGY_CODE_DRIVER_SCHEMA_MISMATCH`.

Non-breaking additions (new categories, new optional payload fields)
keep the major version. Subscribers must ignore unknown optional fields.

## 2. Mode A — Stream CLI

### 2.1 Subscriber Pattern

Mode A is the *minimal* subscriber. It reads JSONL from stdin (or a
filtered subset of compiler output when running in `--error-format=text`
mode) and emits one short text line per *completed state transition*.

Filtering rules:

- Drop `state="thinking"` events (no spinners, no progress)
- Drop `category="trace"` events unless `--verbose`
- Emit one line for each `state="ok"` / `state="done"` / `state="error"`
- Emit one line for each `category="diagnostic"` event

Output format example:

```
[ok] sema  examples/dnd_tavern_campaign/main.pgy
[ok] mir   examples/dnd_tavern_campaign/main.pgy
[error] emit_llvm  PGY_CODE_LLVM_TYPE_UNSUPPORTED
        member access did not resolve to a known field
        examples/dnd_tavern_campaign/main.pgy:131
[done] compile  exit=1
```

No color codes by default. `--color=auto|always|never` flag respected.

### 2.2 Use Cases

- **Screen readers** — accessibility. Linear text output works under
  any assistive technology
- **Shell pipelines** — `pgy build | grep ERROR` and similar. Stable
  text format, predictable line structure
- **CI/CD** — exit code is canonical; one-line summary plus diagnostic
  block sufficient for build logs

### 2.3 Existing

`--error-format=text` is the historical origin of Mode A. From now on
this is referred to externally as **"Mode A / Stream CLI"** so the
discipline is named, not just defaulted.

## 3. Mode B — Grid TUI

### 3.1 Subscriber Pattern

Mode B subscribes to the same Core stream and maintains an *internal
state tree*. State transitions update widgets:

- `state="thinking"` → spinner widget on, progress bar updates
- `state="done"` → spinner stops, ✓ check mark
- `state="error"` → red cross, diagnostic panel expands with `code` /
  `layer` / `payload.message`
- `category="trace"` → trace timeline on the side panel

Layout is grid-based: header (current operation), main panel
(file/source view with diagnostics inline), side panel (trace timeline
+ active intents), footer (overall progress + exit summary).

### 3.2 Implementation — Post-1.0 Aspiration

**Mode B is not part of beta. It is not part of pre-1.0.** It is
recorded here so the architecture is consistent (Core + two consumer
modes), but the actual Mode B renderer is *post-1.0 vision territory*
per `docs/120` §4.

Candidate frameworks for an eventual Mode B implementation:

- **Textual** (Python) — fastest to prototype, runs anywhere Python runs
- **BubbleTea** (Go) — single static binary, distribution-friendly
- **Pergyra-native TUI library** — post-1.0, would require its own
  stdlib work and is itself a vision item

Pergyra ships none of these. Mode B is a *consumer* of the Core stream
that an external party (community, separate tool, IDE plugin) can
implement at any time without compiler changes — that is the point of
the separation.

### 3.3 Use Cases (post-1.0)

- General user monitoring of long compilations
- Live debugger integration
- Intuitive visual debugging of intent traces

## 4. Migration Path

### 4.1 Current State (BETA closure)

| Surface | Status |
|---|---|
| Diagnostic JSON | ✅ implemented |
| Intent trace | ⚠️ text-line format; schema defined but not emitted as JSON |
| Phase dumps | ⚠️ AIR only has JSON; `--tokens` / `--ast` / `--mir` / `--hir` / `--dir` / `--rir` are plain text |
| State events | ❌ not emitted |
| Mode B | ❌ not implemented (vision territory) |

### 4.2 Phase 2 — Core Consolidation (post-BETA, sprint candidate)

- Add `--*-json` flag for every phase dump (parity)
- Migrate intent trace from text-line to JSONL events
- Introduce `pgy_event_emit(category, payload)` unified helper that
  wraps existing `driver_json_emit_string` and trace builders
- Add `state` category emission at compiler phase boundaries

### 4.3 Phase 3 — Mode A Naming (post-BETA, sprint candidate)

- Rename `--error-format=text` documentation to **"Mode A / Stream CLI"**
- External docs / marketing use *Mode A* label
- Existing flag continues to work (compatibility); the *name* is what
  changes

### 4.4 Phase 4 — Mode B (post-1.0 vision)

- External Mode B renderer (Textual / BubbleTea / Pergyra-native)
- Coordinated with `docs/120` §4 vision schedule
- Not committed; aspiration only

## 5. Negative Space — Anti-Hype Constraints

Per `docs/120` §6 forbidden-phrase index, the following external
phrasings are **prohibited** until the corresponding capability lands:

| Forbidden phrase | Honest substitution |
|---|---|
| "Pergyra has built-in TUI" | "Pergyra emits structured JSON events; external tools can render TUI views" |
| "Rich terminal UI out of the box" | "Stream CLI mode standardized; Grid TUI is post-1.0 aspiration" |
| "Real-time visualization built in" | "Real-time event stream emitted; visualization is consumer-side" |
| "Beautiful terminal output" | "Structured event stream; text rendering is opt-in via Mode A" |

**Allowed phrasings:**

- "Pergyra emits structured JSON events; external tools can render
  Stream CLI or Grid TUI views"
- "Mode A (Stream CLI) is standardized; Mode B (Grid TUI) is post-1.0
  aspiration"
- "Compiler is headless; rendering is a separate consumer concern"

## 6. Cross-References

- `src/compiler/driver_diag.c` — diagnostic JSON emitter (Core anchor)
- `src/lsp/pgy_lsp_diagnostics.c` — JSON pattern reference
- `src/runtime/pgy_runtime_observability_schema.h` — event-type
  definitions
- `src/common/diagnostic_layer.c` — layer tag inference
- `docs/120_vision_and_capability_audit.md` §4 — Mode B vision alignment
- `docs/120_vision_and_capability_audit.md` §6 — forbidden-phrase index
  (extended by §5 above)
- `docs/19_design_philosophy.md` §0 — systems-language identity (state
  vs rendering separation is the same discipline)
- `TODO.md` §0-meta — review/ folder process (this doc's living-status
  is part of the same monitoring concern)
