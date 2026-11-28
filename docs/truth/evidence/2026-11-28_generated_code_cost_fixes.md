# Generated-code cost fixes: literal divisors, array literals, LLVM region

Observed 2026-11-28 KST on Ubuntu-C-WSL (AMD Ryzen 7 5700X, 16 threads,
Ubuntu clang 18.1.3). Baseline is main `8ba5fef25`; the branch adds three
commits on it. Other lanes were compiling on the same host during every
run (load average 5 to 13), so timings below are CPU time, not wall time.

The scratch hand edits in `.tmp/ownership-cost-isolation-7Lth32/report.json`
named three costs that are not ownership. This record measures the
compiler's own output after the three changes.

## Changes

- Self-host direct-MIR C and LLVM emit Int `/` and `%` with a literal
  divisor other than 0 and -1, inside i32, as plain operations, as native
  does. `DirectMirScalarProgramSafeLiteralDivisor` decides every site from
  the MIR `integer_literal` operand. `-7` and `-1` are negate nodes in MIR
  and in the native AST, so they keep the checked helper on both compilers.
- Native C stores non-constant array literal elements into the pre-sized
  array and publishes the length once, instead of `pgy_array_push_T` per
  element. The self-host direct-MIR C route already stored directly. The
  bootstrap codegen that emits the installed driver uses an unsized
  `X_new()` plus `X_push` and was not changed. Native LLVM still pushes.
- Self-host direct-MIR LLVM regions call `pgy_region_create_export`,
  `pgy_region_string_concat_export` and `pgy_region_destroy_export`, as
  native LLVM does. The head is the new `<region>` row of the self-host ABI
  layout table (C type `PgyRegion`; LLVM `%pgy.runtime.region = type
  { ptr, i64, i64 }`, 24 bytes, align 8). The symbols are three new
  `region` runtime-call rows. The inline `@pgy.region.concat` and
  `@pgy.region.free` definitions were deleted. Direct-MIR C reads the same
  row for its `PgyRegion` type.

## Toolchain

| Artifact | SHA-256 |
|---|---|
| main native `bin/pgy` | `5c3aa3985fa0e82e9e2a21b14fc963306214d46ac8cea2a94e880e4c5112b6d1` |
| branch native `bin/pgy` | `918e6b1248d130a19a7ffb90739ac70857e2ea12789adfb2788647c052c2fcf2` |
| main test-profile driver (`driver_lf2`) | `bc6c3e8b1b4ef3b30c8e6b62069a6d80b3932afb50f1fba7e9fe72afa3a3796f` |
| branch test-profile driver, from the branch bootstrap's gen2 | `3b11728b389cf8c827543d7c43cdddbc8e0b0daff18ead573370039b036d3aa0` |
| main release driver | `1cd59df094fb1c15e0779dd98d49a22cdb6e69f65fa73e323c45f0d067eb6ed6` |
| branch release driver | `858a281f604d91c5a319b5845a9af6b8d882f5632249bd954f0ac7c035aad461` |
| codegen seed used for both release drivers | `bdfb6f3ea91f70f108ac7d62d271c151828e434a2048c8de8b8b4a719266c884` |

## Closure checks on the branch tree

- Codegen bootstrap: fixpoint gen2 == gen3 (126,135 lines). The
  self-hosting legs passed.
- The native emission of the driver source (the CI oracle step) exited 0.
- Default-route sweep of all 2,746 programs with the branch test-profile
  driver. C: 1,419 equal, 601 self refusals, 8 other outputs, 85 run a
  native refusal. LLVM: 983, 1,045, 0 and 25. Both are identical to the
  main rows. No row changed on either backend.
- New or retargeted gates pass on the branch and fail on main:
  `direct_mir_scalar_safe_literal_divisor_owner.sh` (main: no plain
  `sdiv`), `c_array_literal_element_store_smoke.sh` (main: literal
  elements pushed), `selfhost_region_allocation_owner.sh` (main: no runtime
  region call; it now also requires at most 8 mallocs for 512 region
  concatenations on direct C and LLVM), and
  `region_user_callee_installed_self_host_owner.sh`.
- The region make targets, the ABI layout and runtime-call row manifest
  parity targets and the Int divide target pass. The Long remainder and
  Long division targets fail the same way on main's driver. Their artifact
  rewrite matches no call ("expected one target call, replaced 0").

## Workloads

The sources come from `.tmp/ownership-versus-c-DVLJxu/report.json`. Before
is main's native compiler and `driver_lf2`. After is the branch native
compiler and the branch test-profile driver. Every artifact was built with
`clang -O2`; LLVM linked a `-O2` runtime object. Numbers are medians of
child user+system CPU time over 15 paired, alternating runs pinned with
`taskset -c 4`, two batches. Wall time through the process wait came back
quantized to about 32 ms steps on this host and is not used. Output was
equal before and after for every row. Allocation counts came from
`--wrap=malloc,calloc,realloc,free` and were equal for every row except
region-literal on self-host LLVM.

| Workload | Mode | Before ms | After ms | Ratio (batch 1, batch 2) |
|---|---|---:|---:|---|
| single | native C | 62.31 | 30.43 | 0.498, 0.498 |
| single | self LLVM | 43.42 | 26.66 | 0.644, 0.754 |
| single | self C | 27.40 | 29.68 | 1.082, 0.968 |
| branch | native C | 26.61 | 18.02 | 0.603, 0.634 |
| branch | self LLVM | 27.23 | 16.42 | 0.593, 0.605 |
| branch-join | native C | 46.65 | 37.42 | 0.766, 0.869 |
| branch-join | self LLVM | 47.58 | 33.00 | 0.721, 0.719 |
| return-move | native C | 121.91 | 90.92 | 0.757, 0.786 |
| return-move | self LLVM | 38.11 | 30.91 | 0.786, 0.758 |
| projection-large | self LLVM | 35.24 | 23.58 | 0.696, 0.617 |
| clone-large | self LLVM | 180.02 | 143.90 | 0.812, 0.895 |
| region | self LLVM | 56.13 | 58.21 | 1.011, 1.011 |
| region-literal | self LLVM | 62.30 | 43.98 | 0.730, 0.719 |

Before and after columns are batch 1 medians. Self-host C does not move:
its checked helpers are `static inline` and fold at `-O2`. The region
workload has no region site and does not move. Rows where no change was
expected after `-O2` (native C projection-large, clone-large, region and
region-literal; self C on every workload) gave ratios from 0.90 to 1.08 with
no consistent
direction across the two batches, which is the noise on this shared host.

Region-literal on self-host LLVM went from 2,097,152 mallocs and 2,097,152
frees to 28,672 and 28,672, the same count as native C and direct-MIR C.
Region, region-literal and single on self-host LLVM ran clean under
ASan/LSan before and after, with the native output.

## DRV-2

Producer `--emit-mir-json-verified` on `driver_rung2_main.pgy` from main's
tree, run with the release drivers in the order main, branch, main, branch:

| Run | Wall s | User s | Sys s | Peak KiB |
|---|---:|---:|---:|---:|
| main 1 | 200.85 | 195.85 | 4.97 | 4,557,804 |
| branch 1 | 182.29 | 177.99 | 4.27 | 4,557,912 |
| main 2 | 191.80 | 187.51 | 4.28 | 4,557,964 |
| branch 2 | 201.79 | 197.56 | 4.21 | 4,557,556 |

The MIR outputs were byte-equal (`b9e6cfbec9cfa053`). The spread within a
driver is as large as the difference between drivers, so no change is
claimed. None of the three changes reaches this run. The installed driver's
C comes from the bootstrap codegen, which none of them touches. The native
C change only affects the gen0 oracle binary. The direct-MIR emitters that
changed do not run in the MIR producer.
