# Public Artifact JSON Diagnostic Receipt — 2026-09-01

Status: IMPLEMENTATION COMPLETE — PUBLICATION PENDING
Exact base: `09491bf820e28c0a75a219a741cc22267a7d6fc7`

This directive coordinates one production self-host replacement. It is not a
semantic owner, progress owner, registry, or completion claim.

## Shared objective card

- Goal: make default `pgy --error-format=json SOURCE` and the explicit C/LLVM
  backend variants execute the installed Pergyra artifact producer, preserving
  normal successful compilation and relaying one owner-issued JSON diagnostic
  on a reached semantic failure.
- Priority order: preserve Pergyra diagnostic identity; carry the admitted
  request through the existing source-C/source-MIR owners; delete the launcher
  rejection; fail closed at the opaque process boundary; minimize patch size.
- Semantic facts that must remain first-class: semantic diagnostic code,
  public code, stage, layer, cause, fix, message payload, requested diagnostic
  format, source-C request identity, and source-MIR request identity.
- Owner that must make the decision:
  `public_diagnostic_receipt_owner.pgy` issues the public receipt;
  `driver_source_c_request_owner.pgy` and
  `driver_source_mir_protocol_owner.pgy` own the exact artifact request mode.
- Last legitimate consumer: the installed artifact executor selects the owned
  source request; the C process owner may only admit and relay the opaque wire
  envelope before the backend runner continues or returns failure.
- Allowed bridge: C owns child execution, bounded stdout capture, envelope
  framing, artifact existence, native toolchain invocation, and stderr relay.
- Forbidden fallback or convergence: native pipeline retry, a second semantic
  compile after failure, diagnostic message parsing, C reconstruction of code/
  stage/cause/fix, text-mode substitution, partial child payload publication,
  or accepting a missing/malformed/crosswired receipt.
- Runtime/materialization budget: one installed Pergyra producer execution per
  artifact request; no diagnostic preflight; focused gate under five minutes.
- Positive and negative gates: valid C and LLVM JSON-selected builds execute;
  undefined-function failures publish the exact Pergyra JSON identity; text
  behavior stays unchanged; missing, malformed, and crosswired receipts fail
  without native timing or partial output.
- Non-goals: RIR/AIR/HIR installation, new diagnostic vocabulary, backend
  failure taxonomy, runtime-none support, package command redesign, or SoT row
  closure beyond this reached public boundary.
- Stop/reject condition: if exact format identity cannot travel in the one
  existing artifact request, or if implementation requires semantic retry,
  retain the fail-closed launcher rejection and record the missing owner fact.

## Edit scope and integration

- Primary task is the sole editor, integrator, commit/push owner, and exact-CI
  observer. No parallel implementation scope is open.
- Allowed implementation scope is the installed artifact CLI request, source-C
  and source-MIR request carriage, C/LLVM child transport, launcher selection,
  the focused parity/negative gate, generated language-word inventory, and
  current handoff/collaboration snapshots.
- Narrow validation starts with the new public artifact JSON receipt gate and
  the existing public MIR receipt gate. Component, hard-self-host, SoT edge,
  single-owner, protocol, fresh DRV-2, and exact CI follow only after the slice
  is stable.
- Local implementation evidence is observed on a fresh current-source
  Pergyra-built DRV-2. The focused C/LLVM public gate, existing MIR JSON receipt
  gate, installed CLI aggregate, source-C action gate, compiler-root gate,
  native build, hard contract, component contract, language-word registry,
  SoT edge, Gate single-owner, and protocol registry are green. Publication and
  exact CI remain pending.
