# P0 v16 lane: handoff from Claude to GPT (2026-10-16)

Status: HANDOFF. The Claude lane stops here at the owner's request; the
GPT lane continues. This note is coordination, not semantic authority:
current source, the V1 directive and the executable gates win.

## Where the work is

- Branch `claude/p0-v16`, worktree `C:\Users\user\pgy-wt\perf` (a worktree
  of `D:\PergyraLang`). Base is `origin/main` `571ae8c3a`. Nothing from this
  branch is pushed.
- Head at handoff: `3cc0ecc47`. Every commit on the branch, the copies and
  manual drops it removed and each contract change are recorded in
  [`v1_scope_ownership_and_selfhost_generality_2026-10-14.md`](v1_scope_ownership_and_selfhost_generality_2026-10-14.md),
  section "P0 takeover". Read that section first.
- The last two commits:
  - `dcf4cac76` fixes two stale probes (see below). Verified to compile
    only.
  - `3cc0ecc47` is `wip(p0)`: the memory change (see below). Not yet
    verified end to end.

## What is proven, at `b8e66a387`, in a fresh Linux clone

The Linux CI chain was run in a fresh WSL clone of `claude/p0-v16`, in CI
dependency order and with CI's commands:

| CI step | Result |
|---|---|
| `self-host-codegen-bootstrap-linux` (`make LLVM_ENABLED=1 self-host-codegen-bootstrap-test-smoke`) | PASS, 905 s, `SELF-HOSTING OK` |
| toolchain: installed driver (`tests/self_hosted/parity/self_host_compiler_build.sh`) | PASS, 203 s, **peak 13.7 GB** |
| `collection_ownership_fact_carrier_owner.sh` | PASS (its earlier probe-self refusal is gone) |
| `mir_round_trip_semantic_owner.sh` (new gate) | PASS |
| driver fixed point (`self-host-driver-bootstrap-full-test-smoke`) | **PASS: `gen2 == gen3 (219995 lines)`** |
| `self-host-direct-mir-legacy-intent-program-llvm-test-smoke` | FAIL: stale probe, fixed in `dcf4cac76` |
| the four later targets and the policy corpus of that job | not reached |

This is the first time the integrated MIR-consumer fixed point has passed
since 2026-10-01. The codegen bootstrap and the toolchain step both compile
self-host source through the self-host checker. Their passing at
`b8e66a387` means the checker admitted the codegen and DRV-2 sources with
these commits in place. `3cc0ecc47` has not been through that yet.

## What the last commits fixed

All of these are found by running the installed driver's `--mir-json`
route (the MIR to AST round trip plus the semantic re-check) on the full
DRV-2 MIR. Each one hid the next.

1. Closed arms (`e68881913`). An `if` arm that always returns had no
   structural merge, so the tree emitter copied the continuation twice
   (`member_use_after_move`).
2. Void ends (`e68881913`). The emitter wrote `Return` at the end of every
   Void body; an aggregate field release needs a straight-line body
   (`aggregate_release_incomplete`).
3. Match payload `let`s (`b8e66a387`). `case V(b):` comes back as
   `let b: T = subject.V._0`; the ownership owners could not type that path
   and treated the binding as a store or an owned value. They now read it
   as part of its subject, as a source match binding is.
4. Stale probes (`dcf4cac76`). `intent_binding_identity_probe.pgy` and
   `intent_step_plan_probe.pgy` still called
   `MirIntentBindingProjectionFromCarriers` with five arrays after
   `18648b8ba` changed it to take the carrier projection.

## Next steps for GPT, in order

1. **Finish the self-host bootstrap job.** At the handoff, a rerun of its
   remaining targets with `dcf4cac76` was in progress in the WSL clone
   `/root/pgy-ci` (it is at `b8e66a387` plus the two probe files). The
   targets:
   - `self-host-direct-mir-legacy-intent-program-llvm-test-smoke`;
   - `self-host-direct-mir-composite-intent-program-llvm-test-smoke`;
   - `self-host-direct-mir-nested-intent-program-llvm-test-smoke`;
   - `self-host-driver-source-c-execution-action-test-smoke`;
   - `self-host-installed-driver-cli-mode-test-smoke`;
   - `tests/selfhost_bootstrap_policy_corpus_smoke.sh`.

   Logs are `/root/pgy-ci.rest-*.log`. Expect more stale pins of this
   kind: the bootstrap was red ahead of these targets since 2026-10-02.
2. **Verify the memory change `3cc0ecc47`.**
   - **The risk.** CI's `ubuntu-latest` has 16 GB. Before the change, a
     native-built driver's DRV-2 consumer run peaked at 16.3 GB, and the
     fixed point runs that route twice (`gen2_emit`, `gen3_emit`). The
     toolchain step peaked at 13.7 GB.
   - **The change.** Profile data says 54.7% of the requested bytes were the
     empty focus recovery facts that every `VerdictOk()` built. The change
     moves those facts from the verdict to the body type bundle
     (`collection_focus_recovery`).
   - **Verified so far.** The driver builds natively. The
     collection-formal-key and collection-field-read-summary gates pass.
   - **To do.** Measure the consumer peak on the full DRV-2 MIR, check that
     the gen0 census stays zero on both inputs, and rerun the whole chain.
3. **Run the rest of Linux CI locally** before pushing:
   `scripts/ci_push_linux_steps.sh` (core and self-host shards) and the
   backend-compare shards. Fix stale pins by their actual cause, not by
   loosening a gate.
4. **Push** with `git push origin HEAD:main` once that is green, then watch
   GitHub CI and fix what it alone finds.
5. **Known red, not in CI.** `self-host-collection-focus-recovery-test-smoke`
   fails at `c-nominal_bodyful_forward-emit` (`PROBE|recovery-missing`). It
   fails the same way at this lane's base, and no CI job runs it. It came
   with the v16 candidate.
6. **Later, not P0.** The self-host source checker refuses an explicit
   trailing `return;` after an aggregate field release, where native
   admits it. It is recorded in the V1 directive as precision the canonical
   program does not need.

## How the failures were found (methods that worked)

- **WSL.** Distro `Ubuntu-E-WSL`. The worktree's `.git` names a Windows
  path, so clone with
  `git clone --depth 1 --branch claude/p0-v16 file:///mnt/d/PergyraLang <dir>`.
  Copy changed files across with `tr -d '\r'`. From Git Bash, call
  `wsl.exe` with `MSYS_NO_PATHCONV=1`, or `/root/...` paths get rewritten.
- **The DRV-2 MIR.** It is 374 MB. A native-built driver's `ReadFile` stops
  at 64 MiB. For scratch diagnosis only, build a driver with
  `PGY_CC=<wrapper adding -DPGY_RUNTIME_MAX_FILE_BYTES=1073741824u>`:
  `./bin/pgy src/self_hosted/compiler/driver_bootstrap_main.pgy --native-pipeline --backend=c -o <exe>`.
  Then run `<exe> --mir-json driver_source.mir.json -o gen2.c`.
- **A 1-second reproduction of a round-trip refusal.** Write
  `import "../../<module>"; func Main() -> Int { return 0; }` into a file,
  then run `driver --emit-mir-json-verified` on it and `driver --mir-json`
  on the result. Splitting a module into one program per function isolates
  the function. Running the installed driver from before the change on the
  same input tells whether it is a regression.
- **The source checker's verdict on one fixture.** Build
  `tests/self_hosted/fixtures/nominal_constructor_source_arity_probe.pgy`
  natively, then run `<probe> <fixture> diagnostic`. It prints `body_ok`,
  `body_diagnostic`, the boundary and the function.
- **First-failure instrumentation.**
  - In a scratch copy, add `Log` lines before an owner's `return false`
    branches, or at the `moved_use` site in the verdict owner.
  - Dump the reconstructed tree by logging `tree_emission.text` in
    `driver_rung2_owner.pgy`.
- **Memory.** Use an `LD_PRELOAD` allocation sampler (one backtrace per
  8 MB requested) on a `-g1` build (drop `-s`), symbolized against the
  binary. The runtime frees little, so the requested bytes track peak RSS.

Claude's scratch scripts for all of the above are in its scratchpad
(`C:\Users\user\AppData\Local\Temp\claude\D--PergyraLang\fa285ed1-...\scratchpad\perf\`).
That directory is temporary, so treat it as a convenience, not a record.
