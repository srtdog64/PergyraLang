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

### 2026-06-20 -- parser examples baseline + strategic finding

- Ran `parser_scale_probe.sh`: **88/121 byte-equal, 23 byte-drift, 9 parser
  crashes, 1 C-skip**.
- Categorized the 32 failures (oracle-diff):
  - **Crashes (9) = missing parser features**, not small drifts:
    `transaction { ... compensate ... fail }` block (transaction_saga),
    `parallel`/`spawn` (parallel, structured_comments), `$"`/`f"` interpolated
    strings (party_system_demo, world_roster_city), `ability`/`role`/`vessel`
    domain constructs (role_ability_demo, six_item_alignment_demo,
    vessel_action_design), and a type-inference return (infer_return).
  - **Byte-drifts (23) = small AST-emission deltas** (e.g. walrus_test: the
    parser omits a `Returns: Void` line + a blank line).
- **Key finding**: the self-host parser (`src/self_hosted/parser/main.pgy`, 3356
  lines) has its OWN tokenizer and does NOT share the self-host lexer, so the
  lexer's DOC_COMMENT / keyword / interpolated-string fixes do NOT propagate to
  it. Closing the parser crashes means re-adding those + the block constructs in
  the parser's own front matter.
- **Strategic note (read before grinding this)**: per
  `project_self_host_pause_backend_first`, this text-mirror parser is *throwaway*
  in the substitution pivot (it is slated to be rewritten to emit *structured
  AST*, not text). Pushing its byte-exact coverage toward 100% polishes
  rewrite-bound code. It does still extend the live C/LLVM parity surface, so it
  is not worthless -- but it is diminishing-returns feature work, and the higher
  value completion step is the structured-AST rewrite, not more text coverage.
- **Decision deferred to BDFL** (surfaced, not pre-empted): grind parser text
  coverage (cheap parity-surface gains, e.g. the 23 small drifts) vs. begin the
  structured-AST parser rewrite (the real substitution step) vs. a verifier /
  golden-probe track. Lexer (121/121) was load-bearing and done; parser text
  coverage is the explicitly-cautioned area.
- **Empirical confirmation that grinding text coverage is the wrong track**: a
  tiny attempt (default empty return type to `Returns: Void`) did NOT fix the
  target (walrus_test routes through a different emission path) and would have
  broken a committed C-leg fixture (some forms emit no Returns line) -- reverted.
  This is throwaway-bound, fragile feature work; resume the parser only as the
  structured-AST rewrite. (The earlier "LLVM-blocked" note here is now stale --
  see the correction below.)

### 2026-06-20 -- CORRECTION: the parser LLVM-leg blockage was a real bug, now fixed

- The earlier "LLVM leg fails to compile the parser tool
  (`silent i32 fallback is not allowed`, line 14:9)" was NOT just an in-flight
  artifact -- it was a genuine codegen bug, now diagnosed and fixed (by the
  BDFL, in the in-flight LLVM codegen work).
- **Root cause**: a reassignment inside a control-flow block (`if`/`while`/`for`)
  is lowered to an SSA `def` (e.g. `result=x.2 ast=AST_ASSIGNMENT`), unlike a
  flat reassignment which is a plain `assign`. The SSA-DEF LLVM emission derived
  the target type from the nameless AST node instead of the source-local-type
  fact (which already holds `x->Int`), so it hit the fail-closed
  `ast_type_to_llvm`. C silently fell back to i32 (correct only by luck for Int;
  would have miscompiled a String/struct local).
- **Fix verified (no regression)**: reassignment in if/while/for/nested/else and
  the String case all compile on both backends; all four self-host tools compile
  on LLVM; **parser parity is now green on BOTH legs (188 byte-equal, c+llvm)**;
  lexer (6) and codegen (rung 0-15, 48 fixtures) parity unchanged.
- **Consequence**: the parser self-hosts on both backends again -- the LLVM leg
  is no longer blocked. The "don't grind text coverage" conclusion still holds
  (that is BDFL direction, independent of the bug); but parser work *can* now be
  validated on LLVM.
