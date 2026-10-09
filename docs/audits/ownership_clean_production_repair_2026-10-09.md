# Ownership-clean: reached production repair and next admission failures

Date: 2026-10-09 KST. Observations, not semantic authority or full cutover
completion. Local frozen parent:
`5651c916c87e030cc3ef579180abdfbf1814d1cc`; the production changes below are
subsequent shared-tree edits. Other writers are stopped by user confirmation.
No official executable, CI workflow or SoT status was changed.
The same-folder local checkpoint branch is
`codex/ownership-clean-cutover-2026-10-09`; main remains at the frozen parent.
Candidate checkpointing does not activate cleanup or satisfy P10 landing.

## Delivered production change

The existing `ResourceFlowUniverse` now admits value declarations, not
`SYMBOL_CLASS`/`SYMBOL_TYPE_PARAM` declarations. One owner predicate is used
by bind, nested-declaration capture, function sealing and snapshot
classification. No HIR/RIR/MIR consumer discards rows to hide the producer.
Every other symbol kind retains the previous classification. Required missing
owner facts still refuse; nested Slot/Future and typed/generic value facts
remain present.

The same owner validates an epoch/index memo against its existing declaration
entry matcher. Epoch/index equality alone is not declaration identity. A
retained Symbol from a fresh context or controlled boundary epoch reuse no
longer selects another value's row. No second identity table, source copy or
annotation was introduced. Arbitrary stale snapshot-frame lifetime and
concurrent identity safety are not established by these tests.

Production sources:

- `src/semantic/type_checker_flow_universe.{h,c}`;
- `src/semantic/type_checker_flow_resources.c`;
- existing semantic test owner and `src/test_semantic.c` registration.

The importing old-owner run failed 16 assertions (3135 pass/16 fail).
Production semantic battery after all new cases: **3191 pass/0 fail**.
Actual parsed-source evidence, all symbol-kind controls, missing owner,
nested/seal-only Future, typed/generic and three generation-reentry cases
are in the permanent tests, not just scratch probes. Shared lexical LOC
measurement: edited test include 247; universe source 292. No cap was raised.

## Actual compiler and full-input result

LLVM-enabled isolated compiler built with the `compiler` Make target and
actual production objects:

```text
SHA-256 262768f62f5c8bb3cbf2443996b65f9ace59f7aa4218d50327dc3cc7018919b0
input   src/self_hosted/compiler/driver_bootstrap_main.pgy
cap     3072 MiB, unchanged
exit    0
time    91047 ms
private 1319.5 MiB (1.289 GiB), sampled OS metric
MIR     378143208 bytes
SHA-256 52290d05634c1a95e81d10c3804f7745cd6d05f3fb6b18edbe7252a92985b402
```

The receipt binds 10,561 declared inputs without endpoint drift and reports
complete output capture. All 2531 self-host source files are unchanged from
the baseline. The output is byte-identical to the independent scratch
candidate, with 8637 routines, 25,434 VARIABLE rows and 52,498 loop states;
CLASS/TYPE_PARAM rows are absent. The baseline reference refused the cap at
3114.3 MiB. This is no claim of universal speedup, continuous input
immutability, heap-live/cumulative counters or whole-compiler substitution.
The last parsed-source regression and later proof/docs edits were added
after this receipt; do not bind it to a future commit's complete input set.

Observed unit gates: HIR 26, DIR 15, RIR 26, AIR 147, MIR 217, all pass.
DIR/RIR resource identity and loop-flow summaries pass. Native-only ArrayDrop:
18 positives per C/LLVM lane, 32 negatives per lane and two native declaration
order negatives pass. Native clock/scalar C/LLVM gates pass. This does not
cover the public installed/self-host/JSON ArrayDrop routes.

Static doc quality, evidence lifetime, source UTF-8 and changed semantic-size
gates pass. Full include-size gate remains RED: unchanged AIR/MIR/parser
includes measure 702/785/706 against 699. Component inventory exceeded its
60-second budget after its 14 checker tests passed; the whole gate is not
PASS. Existing likeness sentinel 73 > 20 and the missing same-source isolated
self-host driver remain explicit baseline omissions. No current remote CI
or full P0/fixed-point/installed-pair result is claimed.

One initial `dev-compiler` invocation timed out: that target internally
overrides the supplied build/bin directories and starts `build-dev` with
LLVM disabled. It is not the successful compiler build. Ignored partial
build artifacts were preserved; no official binaries were overwritten.

## Automatic source admission is still incomplete

Four probes ran through the actual production native pipeline. No executable
or C artifact was published by a refusal:

| Probe | Current result | Required transition behavior |
|---|---|---|
| `Bump(counter.value)` with scalar field inout | `PGY_SEM_BORROW_ESCAPE` | admit after single-evaluation rooted-place normalization and disjointness |
| `Count(MakeItems())` for readonly Array formal | `PGY_SEM_TYPE_MISMATCH`, `semantic:move:source_not_named` | compiler-issued ordinary-value temporary, lifetime and origin |
| write/read Slice, then grow backing after last view use | `PGY_SEM_BORROW_ESCAPE` | derive final path-sensitive scope and admit growth afterwards |
| grow backing before last Slice use | refusal | MUST remain refused; no lexical guard deletion without successor evidence |

These are concrete remaining producer failures, not user discipline problems.
Do not insert manual carriers, named locals or copies into their source to
report them green. The existing named boundary for affine/authority values
is not removed along with ordinary-value ceremony.

### Actual generated-code cleanup falsifier

The current production compiler also emits C for a two-element `Array<Int>`
program with **no manual release**, and a separate historical manual-release
reference. GCC in WSL compiled both generated C artifacts together with the
actual `src/runtime/pgy_runtime_lib.c`, using `-fwrapv`,
`-fno-strict-aliasing`, ASan/UBSan and leak detection enabled. These flags
preserve the generated C build contract; no mock allocator was substituted.

- No-manual-release program: runtime refusal exit 1; LeakSanitizer reports
  **8 bytes leaked in one allocation**, from the Array allocator. Its Main
  returns without a drop. This confirms production cleanup is not automatic
  yet, rather than treating a model-only PASS as implementation.
- Manual-release reference: exit 0, exact stdout `2`, no sanitizer report.
  This is a control, not a proposal to retain manual cleanup in the cutover.

Sources/artifacts/logs: `p1_automatic_cleanup_missing.{pgy,c}`,
`p1_manual_cleanup_reference.{pgy,c}`, their `.san` executables and
`*.lsan.log` receipts under the same log root. The first Linux link attempt
missed the `src` include root and did not execute; the corrected run includes
both source include roots and the runtime source. Scope is this emitted-C
fixture only, not LLVM, nested payload glue or the full sanitizer matrix.

## Evidence and boundaries

All run files are below
`.tmp/ownership-cutover-2026-10-09/p0-5651c916-7aeb6d7b3bc44252ba1e80bf2027449b/`:
`semantic-old-owner-falsifier.log`, `production-semantic-final.log`,
`native-production-compiler.log`, `production-air-unit.log`,
`production-mir-unit.log`, `production-memory/production-full-driver-mir.summary.json`,
`production-results.tsv`, `production-array-drop.log`, `p1_*.pgy` and their
stderr receipts. Source Slice census is recorded separately in
`ownership_slice_dx_census_2026-10-09.md`.

The importing preflight, including the new direct-control construction and
its consumer, passes with **10 kernel-verified modules and no assumptions**.
The full formal integration passes with **78 modules plus the approval
binding consumer** (70 proof sources; only the existing two approved Slot
abstractions). Doc quality and evidence lifetime pass after these edits.
The direct-control result is forward trace/value/exit preservation; reverse
graph adequacy, physical recovery and MIR/emitter binding remain OPEN. Logs:
`direct-control-preflight-v2.log`, `direct-control-formal-integration.log`,
`direct-control-documentation-quality.log`,
`direct-control-evidence-lifetime.log`. The initial model compile failed
because dependencies were not built in that directory; the first focused
compile exposed a proof-composition error, fixed before the fresh PASS.
CL6/CL7 remain bounded importing evidence. Ordered source expressions,
physical inout recovery, call-result ABI issuance and static Slice lifetime
production are still P1 obligations. P2-P7 automatic glue/pass/backend/self-host
activation has not happened; the old manual paths have not been retired.

Official binaries remain:

```text
bin/pgy.exe
f6559da94876c93a2e7303866429ef31ef284b1f29ca3d68f67ff5e710cbc1c9
bin/pgy-self-driver.exe
707dcd40049a1697a5827b2a7c8d3cf509573aa3c0031f2eee338c9fa0d78ec7
```

No push, official install, GUI-ready handoff or CLOSED row follows from this
repair. `gmon.out` and unrelated scratch evidence remain untouched.
