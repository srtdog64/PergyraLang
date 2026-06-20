# Self-Host Completion Log

A session-by-session record of the process of completing self-hosting. The
verified *snapshot* lives in [`09_selfhost_status.md`](09_selfhost_status.md);
this file is the *journal* -- what was attempted each session, what landed, and
what the next session should pick up. Append a new entry per session; do not
rewrite history.

## Ground rules (BDFL)

- **Verified rungs only.** No unverified fork of compiler code into Pergyra.
  Every increment must be byte/behaviour-checked against the C (and, where the
  build has LLVM, the LLVM) oracle. (`project_self_host_hard_migration_open`,
  2026-06-17.)
- **Narrow verified core, not more rewriting.** Hard self-hosting is measured by
  ratcheting compatibility fallback to zero, giving each IR layer a verifier
  that owns its contract, and golden-testing ABI/diagnostics/JSON/ordering --
  not by porting more compiler code while escape paths remain. (Scorecard
  `07_hard_self_host_scorecard.md`.)
- **Partial is acceptable; no time-forcing.** Completion is a direction, not a
  deadline. (`project_no_self_host_decision`.)
- **Parallel-work hygiene.** The BDFL owns the live capability-5 frontier
  (MIR source-payload retirement: `mir_branch_source_facts`, match/select/branch
  facts). Assisting sessions stay out of those files and work non-colliding
  areas (front-end, measurement, verifiers for untouched layers), committing
  only their own files.

## Verified state (rolling)

- **Lexer**: self-hosts on C+LLVM. Byte-identical to `pgy --tokens` across the 6
  committed parity fixtures (gated) and **121/121 of the examples corpus**
  (scale probe, as of session 2026-06-20). Zero self-host lexer crashes.
- **Parser**: self-hosts on C+LLVM. Byte-identical against `pgy --ast` on 188
  committed fixtures (gated); examples scale probe last recorded 107/119 with
  4 byte-drifts, 7 self-host exits, 1 C-oracle skip.
- **Backend parity**: parser compiled by C and by LLVM produce byte-identical
  output -- the core self-host correctness signal.
- **Compiler core**: capability-5 single-source-of-truth body tail in progress
  by the BDFL (source-payload reads → dedicated MIR facts).

## Roadmap to completion

1. **Front-end coverage to 100%** (assist-safe): lexer corpus is at 121/121;
   close the remaining parser examples drifts/exits (107/119 → higher) the same
   way -- diagnose each against the oracle, fix the self-host front-end.
2. **Measurement/golden coverage** (assist-safe): committed scale probes per
   tool (lexer done); add golden probes for the other oracle dimensions the
   scorecard names (diagnostics, MIR/AIR JSON, deterministic ordering).
3. **Capability-5 fallback → 0** (BDFL-owned): finish retiring source-payload
   reads in MIR/codegen; assisting sessions only on explicitly non-overlapping
   files.
4. **IR-layer verifiers**: each layer (AIR evidence, HIR/DAG type resolution,
   MIR CFG/body/ownership, ABI layout, backend fact consumption) gets a verifier
   that owns its contract.

## Session log

### 2026-06-20 -- lexer corpus coverage to 121/121

- Committed `src/self_hosted/parity/lexer_scale_probe.sh` (`c7adbb1a`) -- fills
  the noted "no committed lexer-scale probe" gap; mirrors the parser probe.
  Initial measurement: 115/121.
- `a856d3d9`: emit `DOC_COMMENT` for `///` (was skipped), matching the oracle's
  text + text-start column. 115 → 116.
- `62d71ffa`: add missing keywords (transaction, compensate, fail, extends) and
  `$"`/`f"` `INTERPOLATED_STRING` prefixes. 116 → **121/121, zero drift**.
- Lexer parity gate stayed green throughout (no regression on the 6 fixtures).
- Stayed out of the BDFL's capability-5 MIR files; all changes were in the
  self-host lexer + a new probe.
- **Next session**: apply the same oracle-diff method to the parser examples
  drifts (107/119), or add a golden probe for a new oracle dimension.
