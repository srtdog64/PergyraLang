# PergyraLang Daily Technical Review - Checked Claims

This note keeps only the parts of the attached daily technical review that are
supported by the current repository state. Removed material includes stale date
claims, unverified repository-hosting claims, broad external-reference framing,
and proposed make targets that do not exist in the current Makefile.

## Verdict

PergyraLang is in beta-closure work, not beta-complete. The strongest current
direction is source-of-truth closure: CFG/body dataflow, AIR/MIR evidence,
diagnostics, ABI/runtime contracts, and C/LLVM parity must agree before a
surface is called stable.

The review is correct that the core risk is no longer "add more features".
The risk is claiming beta safety before the existing surface is backed by
named facts, smoke gates, backend parity, and proof obligations.

## Confirmed Claims

### Multi-IR Pipeline Is Real

The compiler is not a single AST-walk pipeline. Current docs describe a staged
pipeline with AST/HIR/DIR/RIR/MIR plus AIR, and the beta checklist treats this
as part of production-readiness evidence.

Correct remaining framing:

- Keep AST as parse/provenance input, not backend semantic source of truth.
- Keep MIR/AIR facts as the backend and verification owners.
- Gate semantic fallback reintroduction with loss-contract and declaration
  inventory smokes.

### CFG/Body Dataflow Is A Beta Blocker

The review is correct that CFG-backed body analysis is central. The current
docs say semantic body safety is not fully sourced from CFG/dataflow facts yet,
and `cfg-body-dataflow-test-smoke` is the required ratchet.

Correct remaining blockers:

- broader reachability provenance across nested/exceptional CFG edges;
- general branch/join assignment lattice beyond the current sealed local-let
  surface;
- full drop/cleanup insertion and validation beyond current isolated slices;
- zone/effect transition and projection freshness at joins;
- broader channel receive/backpressure and cancellation task-boundary checks;
- further migration of zone/effect/runtime/codegen consumers onto checked
  body-summary readers.

Do not treat already-closed baseline evidence as still open. The docs already
record multiple closed slices for pin view boundaries, copy-only cancellation
payload rejection, copy-only channel close, and several parallel/channel
transport rejections.

### Slot Is Not A Borrow Checker

This claim is correct and should stay. Slot is a runtime capability/resource
boundary with generation/token/pin-state validation. Borrow-checker-equivalent
claims require the separate static bridge: CFG body dataflow, ABI ownership
parity, task/channel boundary rules, and diagnostics.

Allowed wording:

- "Slot is a runtime capability/resource boundary."
- "Borrow-checker-equivalent safety for the stable subset requires CFG/body
  dataflow and ABI/runtime evidence."

Forbidden wording:

- "Slot is a borrow checker."
- "Slot alone proves Rust-style lifetime safety."

### Stable Surface Must Be Gate-Enforced

The stable subset contract is real and should be kept as a review point. A
feature is beta-stable only when syntax, semantic analysis, runtime/ABI,
C backend, LLVM backend where supported, diagnostics, regressions, and docs
agree.

The platform/backend wording must be precise:

- Linux: C backend and LLVM backend regression coverage.
- Windows: C backend regression coverage always; LLVM only when executable
  LLVM evidence is present.
- macOS: C-only CI preflight; macOS LLVM/backend parity remains out-of-beta.

### Raw Escape And Runtime-None Are Not Stable

The review is correct that raw escape and no-runtime claims must stay narrow.
Current docs say `unsafe { ... }` is not itself a raw-pointer escape contract,
`SlotRawPointer(...)` rejects as unstable, and `--runtime=none` is still a
beta-gated unsupported target for runtime-dependent surfaces.

### Performance Evidence Exists But Is Narrow

The review is correct that performance must be measured rather than assumed.
The current repo has `perf-contract-test-smoke`, `perf-c-baseline-test-smoke`,
`test-abi-perf`, and `perf-summary`.

The review's proposed targets such as `make perf-parser`,
`make perf-semantic`, `make perf-ir-memory`, and per-runtime perf targets are
not current Makefile facts. They should be treated as future design ideas, not
current evidence.

### Diagnostics Are A Real Contract, But Not Fully Mature

The review is correct that diagnostics are a core beta contract. Current docs
record stable diagnostic codes, `Reason:` / `Fix:` style guidance, and
diagnostic JSON regression gates.

The mature-diagnostics claim must be limited:

- single-span diagnostics are still the current API shape;
- multi-span diagnostics and rich related-site payloads are not done;
- structured JSON regression exists, but richer schema validation is still a
  growth target.

## Removed From The Review

The following claims were removed because they are stale, unverified, or too
broad for the current evidence:

- repository hosting/admin/open-issue statements;
- "latest docs are 2026-06-20~21" as a current status claim;
- external paper/reference sections as evidence for current implementation
  status;
- generic "today P0" lists that treat already-closed baseline slices as still
  open;
- proposed make targets that do not exist in the current Makefile;
- claims implying complete cross-platform LLVM support;
- claims implying whole-language mechanized proof is already complete.

## Current Actionable Review

1. Keep `cfg-body-dataflow-test-smoke`, `formal-semantics-test-smoke`,
   backend-compare gates, and stable-subset gates as the real beta readiness
   evidence.
2. Continue closing semantic fallback paths by moving decisions behind MIR/AIR
   facts or checked body-summary readers.
3. Keep Slot public wording precise: resource/capability boundary first,
   borrow-checker-equivalent only for the proven stable slice.
4. Treat performance as measured evidence. Extend existing perf gates instead
   of inventing status claims ahead of measurement.
5. Do not claim beta-complete until docs, tests, diagnostics, proof rows, and
   C/LLVM behavior agree on the same stable surface.
