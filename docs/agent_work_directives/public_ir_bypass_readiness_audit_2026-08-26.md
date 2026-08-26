# Public IR Bypass Readiness Audit Directive

Status: `AUDIT COMPLETE`; this document coordinates agents and does
not own compiler semantics, progress, or a successor implementation rung.

Base revision: `8b8c78f0d6f5efd0eecaeaec7ee2b1796b6723dd`.

Lease F is closed. The public launcher still sends `--rir`, `--rir-json`,
`--air`, `--air-json`, and `--hir*` through the final native
`driver_run_pipeline` dispatch. Their existence is not proof that a Pergyra
producer owns an equivalent payload. This audit identifies whether exactly one
of those live bypasses is ready for the next hard substitution rung.

## Shared objective card

- Objective: find the smallest public IR/debug-mode bypass whose complete
  observable payload is already produced from typed Pergyra-owned facts, so a
  future implementation can delete that default native dispatch without
  inventing or reconstructing semantics.
- Priority: exact semantic identity and existing fact ownership; complete
  payload coverage; fail-closed unsupported shapes; old-path deletion;
  executable parity and negative evidence; then patch size.
- Candidate fact owner: an existing typed RIR, AIR, or HIR producer/admission
  owner. A validator, vocabulary list, parser fixture, readiness probe, JSON
  tool, or native output is evidence only and cannot be promoted into an owner.
- Last legitimate consumer: installed public CLI stdout for the audited mode.
  Native output remains an explicit `--native-pipeline` oracle only after a
  substitution exists.
- Forbidden fallback: native retry, source/AST/root rescan, native JSON parsing
  as production authority, guessed lifecycle/CFG/SSA facts, fixture dispatch,
  partial output presented as the legacy mode, or a new general query/cache
  architecture.
- Integration gate: the primary task must observe the current public native
  bypass, name one exact Pergyra producer and its missing-fact behavior, and
  identify a focused public/internal parity plus missing-driver/unsupported-
  options negative before opening implementation. `NOT READY` is the required
  result when any observable field lacks a typed Pergyra owner.

## Independent audit boundary

- Each auditor edits only its assigned report under `docs/audits/`. Do not edit
  source, tests, Make/workflow files, registries, handoff, progress, dogfood,
  collaboration, or this directive.
- Do not stage, commit, push, build DRV-2, run full suites, or inspect the
  user-owned untracked `pgy-80135c2c/` directory.
- Read-only source searches, bounded file reads, and existing installed/native
  CLI probes are allowed. Keep static commands under 60 seconds and capture
  only schema/field names, byte counts, hashes, or short representative lines.
- Separate observations, inferences, and proposals. `SURFACE` or `REACHABLE`
  evidence is not `SUBSTITUTING`, and an audit report is not a semantic owner.

## RIR readiness track

Assigned report:
`docs/audits/2026-08-26_public_rir_bypass_readiness.md`.

Trace public `--rir` and `--rir-json` from `src/pgy_driver.c` into the native
pipeline. Inventory every observable output fact on one small source and find
the exact Pergyra producer, if any, that owns the same complete fact family.
Distinguish real production-root reachability from parsers, validators, tools,
fixtures, and vocabulary declarations. End with `READY` or `NOT READY`, the
missing fact when not ready, and the smallest exact parity/negative gate that
would falsify a future ownership claim.

## AIR readiness track

Assigned report:
`docs/audits/2026-08-26_public_air_bypass_readiness.md`.

Trace public `--air` and `--air-json`, inventory their observable payload, and
map it to current Pergyra AIR owners. Determine whether existing AIR code owns
a complete general producer or only bounded MIR certificates, validators,
artifact vocabulary, and tools. End with `READY` or `NOT READY`, the first
missing fact, and one exact parity/negative falsifier.

## HIR readiness track

Assigned report:
`docs/audits/2026-08-26_public_hir_bypass_readiness.md`.

Trace `--hir`, `--hir-cfg`, `--hir-dom`, and `--hir-ssa`. Inventory the native
summary/CFG/dominator/SSA fact families separately, then map each to existing
typed Pergyra HIR owners. Reject a partial summary as a replacement for all
four modes. End with the smallest individually `READY` mode, or `NOT READY`
with its first missing fact, plus one exact parity/negative falsifier.

## Primary integration decision

The primary task owns current launcher probes and the sole implementation
choice. It will compare complete-payload ownership, not file counts or keyword
matches. At most one executable rung may open from these reports. If all tracks
are `NOT READY`, the primary task records the precise missing fact and continues
observing another production bypass rather than manufacturing an IR owner.

## Completion receipt

- RIR is `NOT READY`: the installed root has no ordered Pergyra RIR program
  owning scope, fact, operation, state, and flow rows. The existing RIR tool is
  a bounded consumer of native JSON.
- AIR is `NOT READY`: MIR CFG certificates, validators, vocabulary, and tools
  do not own general ordered AIR intent/boundary/evidence issuance.
- HIR is `NOT READY`: typed AST and selected graph facts exist, but no
  post-semantic Pergyra HIR routine identity or CFG carrier owns predecessor,
  exit-summary, phi-candidate, dominance, or SSA-preparation rows.
- All eight bare public modes were observed entering the native pipeline, with
  public and explicit-native output equal. These reports authorize no invented
  producer and count as no substitution progress.
- Primary integration decision: delete the *implicit* final native fallback
  and keep these diagnostics reachable only through explicit
  `--native-pipeline` until a complete Pergyra fact owner exists. This is a
  fail-closed old-path ratchet, not a replacement-progress numerator.
