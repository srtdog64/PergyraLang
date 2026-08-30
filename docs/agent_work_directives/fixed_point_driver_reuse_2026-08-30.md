# Fixed-Point Driver Reuse — 2026-08-30

Status: `COMPLETE — EXACT CI GREEN`

Exact base: `3a0c78943f4fa4804731df304f416de8092e7624` on
`origin/main`.

This directive coordinates one compiler-scale build feedback reduction. It
does not change compiler semantics, relax the gen2/gen3 fixed point, add a
cache authority, or claim self-host substitution or SoT progress.

## Objective card

- Objective: let the installed-driver builder consume the exact `driver_gen2`
  binary and C artifact already proved by the full `gen2 == gen3` fixed point,
  instead of emitting and compiling the whole driver a second time in the same
  CI job. Repeated ordinary installer calls in one unchanged workspace must
  also reject the four-minute emit before it starts.
- Priority order: current source-graph identity; exact fixed-point artifact
  identity; fail-closed receipt admission; installed source/manifest smoke;
  old repeated-operation ratchet; then wall time.
- Production execution boundary: the `self-host-bootstrap-linux` job runs
  `self-host-driver-bootstrap-full-test-smoke` and then five installed-driver
  gates. `self_host_compiler_build.sh` is the last installer for the driver
  those gates consume.
- Repeated operation to delete: exact-head run `33310231316` proved
  `driver_gen2 == driver_gen3`, then `self-host-compiler` emitted the complete
  typed-source driver again for 4m43s before the follow-up gates. The parallel
  `build-linux` job independently emitted the same installed driver twice,
  about four minutes per emission.
- Fact owner: a responsibility-named shell owner computes a deterministic,
  content-addressed fingerprint over the conservative self-host `.pgy` source
  graph and writes/admit one fixed-point receipt binding that fingerprint,
  codegen seed, gen2 C, gen3 C, and gen2 binary. The receipt is build evidence,
  never compiler semantic authority.
- Last legitimate consumers: `driver_bootstrap.sh` writes the receipt only
  after generated bounded preflight and `gen2 == gen3`; then
  `self_host_compiler_build.sh` admits it before atomically installing the
  candidate and recording its ordinary installer key.
- Forbidden fallback: timestamps, path existence, or runnable status as the
  cache identity; a receipt not bound to the current source graph and codegen
  seed; accepting unequal gen2/gen3 C; accepting a changed candidate binary;
  copying without the existing source and machine-manifest smokes; silently
  regenerating when an explicitly supplied receipt is invalid; weakening the
  fixed point; or adding parallel compiler-scale emission.
- Verification gate: a focused receipt-owner gate admits an exact synthetic
  graph and rejects source, codegen, gen2 C, gen3 C, binary, missing-field, and
  schema mutations. Structural ratchets require the full bootstrap to write
  the receipt, the installer to admit it, and CI to supply all candidate paths.
  The bounded installer smoke and exact-head full bootstrap remain the
  integration falsifiers.

## Fresh falsifier

- Run `33310231316` spent 32m34s from integrated seed emission through the
  fixed-point result. It then ran `self_host_compiler_build.sh`, which emitted
  and compiled the complete driver again from 12:31:04 to 12:35:47 before
  roughly fourteen seconds of follow-up gates.
- In the same run, `build-linux` emitted the installed driver from 12:03:37 to
  12:07:41 and repeated the complete emit from 12:11:15 to 12:15:18.
- The full bootstrap already compiles `driver_gen2`, executes its bounded MIR
  preflight, emits `driver_gen3`, and requires byte equality. The missing fact
  is a reusable receipt binding that evidence to the current source graph and
  installed artifact identity.

## Scope and budget

- Allowed edits: the new fixed-point receipt owner, the two existing driver
  bootstrap/install scripts, one focused negative gate, its Make/CI and
  component/source-inventory wiring, and current coordination/progress docs.
- Integration owner: the primary task owns all implementation, publication,
  and exact-head CI interpretation. No parallel implementation edit scope is
  open on this single installer boundary.
- Allowed commands: read-only source/CI census; Bash syntax and focused receipt
  gates; component, hard, and build-source inventory contracts; one isolated
  two-call installed-driver measurement; exact-revision GitHub CI inspection.
  Do not run parallel compiler-scale emission or a redundant local full matrix.
- Out of scope: compiler semantics, source/MIR/ABI facts, subprocess language
  surface, a general cache/query engine, parallel full-driver emission,
  timeout or memory increases, and ArrayString consumer migration.
- Budget: static receipt tests under 60 seconds, installer/focused integration
  under five minutes when a valid current receipt exists, and one exact-head
  full CI run for wall-time publication.

## Output classification

Success is one removed repeated compiler-scale operation with a stronger
negative receipt gate. It changes neither 88 authorities / 183 carriers /
`CLOSED=55 BRIDGE=32 ACTIVE=1` nor the 83% project forecast.

## Local implementation receipt

- `self_host_driver_fixed_point_receipt_owner.sh` owns deterministic source-
  graph, fixed-point, prebuild, and installed-artifact fingerprints. GNU/MSYS
  hosts batch 2,201 `.pgy` inputs through NUL-safe `sort`/`xargs`; the local
  full graph fingerprint fell from an interrupted per-file process storm to
  about two seconds. A Git batch path remains the non-GNU fallback.
- `driver_bootstrap.sh` writes the fixed-point receipt only after the generated
  bounded preflight and exact gen2/gen3 C comparison. The installed builder
  admits all four explicit candidate inputs together, reruns its existing
  source and machine-manifest smokes, and installs the exact candidate binary.
  A partial or stale explicit request fails without ordinary emission fallback.
- Ordinary installs write the prebuild key and an output-content receipt only
  after compilation and both smokes. The former post-emission stamp cache read
  was deleted because it bound the output path but not the binary content; it
  cannot bootstrap the stronger receipt.
- The full Linux job passes the fixed-point gen2 C, gen3 C, binary, and receipt
  to the later installed-driver goals in the same sequential `make` call. The
  fast receipt mutation gate is also wired into `ci_linux_steps.sh`.

## Local evidence

- Current-source focused receipt gate: GREEN. It rejects source graph, codegen
  seed, gen2 C, gen3 C, binary, schema, missing-field, duplicate-field, and
  installed-artifact mutations, and proves a source mutation changes the
  prebuild key.
- Current-source component contract: GREEN, including source-MIR execution-
  action commit order, line caps, CI wiring, and the deleted old stamp read.
  Current-source hard contract and build-source inventory are also GREEN.
- Development integration measurement in an isolated clean build directory:
  the first ordinary installed-driver build took 374 seconds; the next
  identical invocation returned before typed-source emission in 15 seconds.
  A receipt-bound candidate install passed the source/manifest smokes in 17
  seconds, installed the byte-exact candidate, and left ordinary cache stamps
  absent. A partial explicit request returned status 1 and did not emit.
- Those local timings are performance evidence, not an exact-head publication
  receipt. The next integration falsifier is the exact pushed CI job: it must
  remain GREEN and show no second installed-driver emission after the full
  fixed point. No SoT row or project percentage changes in this rung.

## First exact-head CI and reached residual

- Implementation commit `5ddecfc6b0a267ad70334f3ea1f705198e5fb6ec`
  is on `origin/main`. Exact run `33314947343` is GREEN 30/30. The full
  self-host job fell from 37m50s to 32m39s. Its log proves one 172,273-line
  `gen2 == gen3` result at `14:16:12.279Z`, one receipt-bound candidate
  adoption at `14:16:12.749Z`, one installed driver, and zero ordinary
  typed-source driver emissions. The fixed-point-to-adoption gap was 0.47s.
- That run falsified the broader ordinary-reuse claim in `build-linux`: the
  installed driver still emitted twice. The first dependency invoked the same
  Ubuntu GCC through `CC=/usr/bin/cc`; the second invoked it through `CC=gcc`.
  The v1 prebuild key hashed the alias-dependent `--version` first line, so a
  spelling change impersonated a toolchain change.
- The reached repair replaces that string with
  `pgy.selfhost.c-compiler-fingerprint.v1`: resolved executable content,
  `-dumpfullversion/-dumpversion`, and target triple. Prebuild schema v2 carries
  that fingerprint. Local `cc` and `gcc` now produce the same compiler
  fingerprint and the same full prebuild key
  `813e43825fb7a4d6af146d34db1df54d9cb5540f00bf8c51eaa69b09e70eb795`.
  The focused alias gate plus current component, hard, and build-source
  inventory contracts are GREEN. A second exact CI run is the next falsifier;
  `build-linux` must show one ordinary emit and a later pre-emission reuse.

## Second exact-head CI and final wiring residual

- Alias-normalization commit `d3cf4e9e22e22d480c763d2e8760f56a7209b5e5`
  is on `origin/main`. Exact run `33317553455` is GREEN 30/30. In
  `build-linux`, the log now has exactly one ordinary typed-source emit at
  `14:48:15.222Z`, one installed driver at `14:53:02.176Z`, and one
  pre-emission reuse at `14:57:35.231Z`; the previous second 3-5 minute emit is
  absent. The job took 23m33s, so the semantic operation count—not one noisy
  wall-time sample—is the acceptance evidence.
- Full self-host remained correct: one 172,273-line fixed point at
  `15:16:29.289Z`, one receipt adoption 0.55s later, one installation, and zero
  ordinary emits. It completed in 35m13s, still 2m37s below the original
  37m50s baseline while showing ordinary runner variance from the first
  receipt run's 32m39s.
- A log census found the focused mutation gate absent from the actual push
  step list. It had been added to full-platform `ci_linux_steps.sh`, while
  `ci-push-linux` owns `ci_push_linux_steps.sh`. The final wiring added the gate
  to the push list and made build-source inventory require both lists. Focused
  receipt mutation and build-source inventory gates were GREEN before
  publication. This wiring correction changed no compiler artifact or SoT
  status.

## Final exact-head CI receipt

- Wiring commit `8011114738bd82eb3f680cfc74149a99c8ddac4e` is on
  `origin/main`. Exact run `33319423595` is GREEN 30/30 at that revision.
- `build-linux` completed in 24m20s. Its log contains exactly one
  `[self-host-driver-fixed-point-receipt-smoke] PASS`, one ordinary typed-source
  driver emission, one installed driver, and one later
  `reusing source-graph fingerprinted driver before emission` line. The focused
  negative gate therefore runs on the actual push path, and the second ordinary
  compiler-scale emission remains absent.
- Full self-host completed in 28m34s. It has exactly one 172,273-line
  `gen2 == gen3` fixed point at `15:50:27.723Z`, one receipt-bound adoption at
  `15:50:28.209Z`, one installed driver, and zero ordinary typed-source driver
  emissions. The fixed-point-to-adoption gap is 0.49s.
- The acceptance evidence is the preserved fixed point, fail-closed receipt
  gate, and removed repeated operation. Runner wall time is supporting evidence,
  not a new semantic or cache authority. This rung changes neither 88
  authorities / 183 carriers / `CLOSED=55 BRIDGE=32 ACTIVE=1` nor the 83%
  project forecast.
