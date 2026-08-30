# LLVM self-MIR block stack frame — 2026-08-30

Status: `PUBLISHED — CI GREEN`

Exact base: `f2aff7aba86ce2594e62eef251c089b0b38ca1ad` on
`origin/main`.

This directive coordinates one reached executable falsifier. It does not
reclassify an SoT registry row, change the project percentage, or make compiler
stack size a language semantic fact.

## Shared objective card

- Objective: make the current-source LLVM-built DRV-2 canonicalize
  `dish_result_collect` and `class_method_result_loop` without a Windows stack
  overflow while preserving byte-identical admitted MIR behavior.
- Priority: semantic identity and canonical byte parity; eliminate recursive
  frame growth at its owner; fail closed on missing or invalid facts; preserve
  the C-built control; then patch size.
- Production entrypoint: the current-source LLVM-built
  `driver_rung2_main.pgy --canonicalize-oracle-mir-json` path reused through the
  parity harness's prebuilt-driver slot.
- Direct bypass to delete: none is admitted. The C-built driver is a control,
  not a substitute for the failing LLVM-built production candidate.
- Fact owner: `SelfMirRoutineInput` is the read-only semantic artifact view and
  `SelfMirRoutineBuild` owns the mutable routine construction state. LLVM target
  layout owns the ABI size used to preserve multi-kilobyte value-return
  boundaries during optimization.
- Last legitimate consumer: recursive block dispatch rooted at
  `SelfMirLowerBlockFromArtifact` and reached by the routine entry, if, for,
  match, and while lowering owners.
- Forbidden fallback: increasing the PE stack reserve, selecting the C-built
  driver, skipping or allowlisting fixtures, reducing the admitted MIR,
  special-casing names or types, or routing canonicalization through a native
  non-self-host implementation.
- Verification gate: run the exact fresh LLVM-built DRV-2 against both reached
  failing oracle MIRs and small controls; then resume fixture rows 243-284,
  which cover the reached failures and the unexecuted tail. Preserve the
  installed C-built driver as a control.

## Edit scope and overlap boundary

- Allowed implementation scope: the target-layout LLVM optimization boundary
  proven to multiply the exact self-host MIR block/routine frame, its focused
  parity/negative gates, this directive, collaboration notes, and the current
  handoff.
- Linker stack flags, unrelated SoT cleanup, query or cache architecture, ABI
  redesign, source-level self-host workarounds, and fixture expansion are
  outside this rung.
- The primary task is the sole integration owner. No parallel implementation
  track is open.

## Commands and budgets

- IR inspection and static owner gates: 60 seconds per focused command.
- Two-fixture execution and small controls reuse one built driver; the one-time
  LLVM driver build remains the measured compiler-scale cost.
- Rebuild one current-source LLVM DRV-2 after implementation and reuse it for
  the bounded corpus continuation. One integration shard may use up to 30
  minutes; the full matrix remains a publication boundary.

## Integration ownership and output classification

- The primary task owns integration.
- The integration gate is the two reached fixtures plus the resumed manifest
  interval under one freshly rebuilt LLVM DRV-2.
- IR and debugger results are observations. A source patch is an implementation
  candidate until the focused execution gate passes. This directive itself is
  coordination evidence and does not count as substitution progress.

## Reached falsifier

- Current-source LLVM driver SHA-256:
  `CC7597083BA8C7B246C90E9F4C35556B90363B6D781DCB81C579FFDEAB45B7E7`.
- Manifest source lines 9-248 and 259-263 passed with that driver.
- `dish_result_collect` and `class_method_result_loop` both exit with Windows
  `0xC00000FD` while canonicalizing their admitted oracle MIR JSON.
- GDB reaches `___chkstk_ms` from `SelfMirLowerBlockFromArtifact`. The original
  native frame is `0x362d8` = 221,912 bytes; `SelfMirLowerIfFromArtifact` is
  `0x1cbf8` = 117,752 bytes. Eleven nested block entries consume about
  1,359,232 bytes before Windows exits with `0xC00000FD`. Both C-built and
  LLVM-built executables reserve 2 MiB, so a linker stack-size mismatch is
  falsified.
- The raw IR contains 13 `%SelfMirRoutineBuild` allocas in the block lowerer.
  LLVM target-layout measurement reports the aggregate return ABI size as
  exactly 3,000 bytes. A first 4,096-byte policy therefore did not apply; its
  binary retained the original-sized frames and reproduced the overflow.

## Implemented owner policy

- `src/codegen/llvm_api.c` reads the module target layout and classifies every
  defined struct/array return by `LLVMABISizeOfType`.
- A return ABI of at least 2,048 bytes retains the function boundary with
  LLVM's `noinline` attribute. Ordinary returns keep the normal O2/O3 pipeline.
- The rule is independent of Pergyra function names, fixture names, MIR type
  names, environment toggles, and the PE stack reserve. The failed global
  inliner-threshold route is absent.
- The focused static gate is
  `tests/llvm_large_aggregate_return_stack_smoke.sh`; it is a prerequisite of
  `llvm-test-smoke` and runs in the fast Linux push lane.

## Observed completion evidence

- Fresh current-source LLVM-built DRV-2 SHA-256:
  `3F3EDF13D6E240E2BC80572492DC6D5132B8CB5B1ABEBC1D6004F06B87F71120`.
- Final native frames are `SelfMirLowerBlockFromArtifact = 0x8e18` = 36,376
  bytes, `SelfMirLowerIfFromArtifact = 0x104d8` = 66,776 bytes, and
  `SelfMirLowerTrackedStatementFromArtifact = 0x16358` = 90,968 bytes.
- The same binary passes `dish_result_collect` and
  `class_method_result_loop` together through producer-first source/MIR parity.
- The same binary passes corpus rows 243-284 (42 fixtures), including the
  previously failing rows and every formerly unexecuted tail row. It also
  passes representative controls at rows 1, 100, and 200.
- Rows 1-242 were already green on the preceding current-source LLVM driver.
  A second exact-binary 284-row run was not repeated because its projected
  wall time exceeds the 30-minute integration-shard budget; no one-binary full
  matrix claim is made.
- `llvm_large_aggregate_return_stack_smoke.sh`, its Make target,
  `ci_step_runner_smoke.sh`, `build_source_inventory_smoke.sh`, the incremental
  LLVM compiler build, and `git diff --check` are green.
- The broad `perf_contract_smoke.sh` was attempted but stops at its pre-existing
  line-4060 `could not lower print argument` literal contract before reaching
  this rung. This policy therefore has a separate focused owner gate rather
  than weakening or reviving that unrelated diagnostic spelling.

This is executable LLVM self-host prerequisite closure, not a new hard
`SUBSTITUTING` numerator. The SoT census remains `CLOSED=55 / BRIDGE=32 /
ACTIVE=1`, the project forecast remains 83%, and the C++-class reconstruction
target remains open.

Implementation checkpoint:
`5e1881cc537c837d122d1cc9a36bea659aafe379`.

Publication checkpoint:
`10ce32d8a1e4dc77f4f787cd8250c0fa5f3d7114`. Exact-head CI run
`33297665731` completed GREEN 30/30; the publication lease is retired.
