# Fresh production bypass entrypoint audit — 2026-09-03

Status: `AUDIT SLICE COMPLETE — NO IMPLEMENTATION RUNG SELECTED`

Exact base: `65807bb35f57cd800b9a9d7a7b32c74c6d60fe82` on
`origin/main`.

This is read-only scheduling evidence. It owns no compiler semantics, SoT
status, progress percentage, or successor implementation.

## Objective card

- Objective: determine whether a current top-level production selector still
  routes compiler semantics through C while an existing complete Pergyra owner
  is available.
- Priority: production entrypoint; direct C bypass; existing Pergyra fact
  owner; last orchestration consumer; no-fallback falsifier; then patch size.
- Fact owner: `Unknown` unless an executable bypass is observed. A selector or
  process adapter is not automatically a semantic owner.
- Last legitimate consumer: `Unknown` unless the same trace reaches both the C
  decision and an existing Pergyra fact.
- Forbidden fallback: treating explicit `--native-pipeline`, host compile/link,
  a C-owned session UI, an unsupported fail-closed mode, a historical MIR
  artifact, or the frozen nine-blocker inventory as a fresh bypass.
- Verification boundary: current source inspection plus the exact-head public
  integration evidence from run `33736375620`. No implementation edit is
  authorized without a concrete positive bypass trace and negative fixture.

## Observations

- `src/pgy_driver.c` checks `--native-pipeline` and the harness-owned
  `PGY_NATIVE_PIPELINE` opt-out before delegation. Both invoke
  `driver_run_pipeline`; neither is an implicit retry from a failed installed
  request.
- Default public MIR diagnostics, machine manifest, tokens, AST, capability
  manifest, DIR, MIR JSON, LLVM IR, C artifact/compile/run, and LLVM
  compile/run route to the installed self-host driver. Unsupported option
  envelopes fail before invoking either pipeline.
- `src/compiler/driver_diag.c` reports RIR, AIR, and HIR requests as having no
  installed Pergyra fact owner and requires explicit `--native-pipeline` for a
  native diagnostic. These modes are product gaps, not hidden C fallbacks and
  not evidence of an already complete Pergyra owner.
- Package check/build/run/test/lint/prove/package compilation uses installed
  self-host MIR/C/LLVM paths unless the caller explicitly selected the native
  harness opt-out. The REPL retains a C session UI but calls
  `c_runner_execute_installed_self_host_c` for each executable submission.
  Default LSP session and diagnostic modes likewise select their installed
  self-host owners; native LSP is an explicit command-line mode.
- Exact-head run `33736375620` completed 30/30 green. Its full self-host job
  installed the Pergyra-built DRV-2, proved
  `gen2 == gen3 (173909 lines)`, and passed the installed CLI aggregate. This
  supports the selector observations but does not turn an unowned public mode
  into a completed Pergyra owner.
- The 35,814,796-byte MIR at
  `.tmp/multi-routine-generalization/routine-index-fixture.mir.json` is stale
  scheduling evidence. Current DRV-2 rejects it in 0.35s with
  `MIR machine-layer facts are missing or invalid`; its historical
  `CompilerSymbolCIdentifier` frontier is not a current production falsifier.

## Verdict and next falsifier

No top-level production semantic bypass was observed in this audit slice.
Therefore there is no fact owner/last-consumer pair on which to open an
implementation directive, and no registry or percentage change is justified.
C-owned process publication, session UI, and host compiler/linker boundaries
remain legitimate last consumers when they do not reinterpret compiler facts.

The next read-only slice must test the frozen nine-blocker categories against
current source rather than assuming their 2026-07 baseline descriptions are
still successor candidates. It may select implementation only if one current
production request reaches a C semantic decision and the same fact already has
a complete Pergyra owner. Otherwise it must record an evidence gap or product
boundary and keep implementation closed.
