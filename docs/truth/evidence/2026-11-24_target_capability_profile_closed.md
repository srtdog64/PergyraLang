# `target.capability_profile` closed (2026-11-24)

Source: main `6a6f612b5` plus this change. Trees: WSL `Ubuntu-C-WSL`
`/root/ts2` and `/root/ts3`. Scope:
[decision](../decisions/2026-11-24_target_capability_profile_closure_scope.md).

## Before

- The "target capability envelope" held vocabulary only (3 projection names,
  8 fact names, 5 fallback reasons) and a hash of it.
- The native LLVM backend took its triple from `LLVMGetDefaultTargetTriple()`,
  overridden to `x86_64-w64-windows-gnu` under MinGW.
- The C toolchain adopted whatever `--target` a `PGY_CC` or `CC` command
  named, and on Windows probed `clang --target=x86_64-w64-mingw32`.
- The clang that compiles self-host LLVM IR got no target, so clang's own
  default decided.
- The self-host Array layout contract spelled `pointer64-size_t64`, size 32,
  alignment 8 and offsets 0/8/16/24 as literals.
- The row's smoke (`target-capability-test-smoke`) ran in no CI job.

## Change

- `src/common/target_profile.def`: five rows (x86_64 and aarch64 Linux,
  x86_64 Windows GNU, x86_64 and arm64 macOS), each with LLVM triple, C
  driver target, object format, pointer width, pointer alignment and byte
  order.
- `target_capability_contract.c`: the native compiler selects the host's row
  at build time. `pgy_target_profile_matches_host` refuses a missing row and
  any row with an empty target, an unknown object format, or a pointer width,
  alignment or byte order unlike the host's.
- `llvm_target_machine.c`: the triple comes from the row; the default triple
  and the MinGW override are deleted.
- `compiler_toolchain.c`: a `PGY_CC`, `CC` or `PGY_LLVM_CLANG` command that
  names a target other than the row's is refused with the row it differs
  from; the Windows clang probe and the LLVM IR clang take their `--target`
  from the row.
- `scripts/render_target_profile_rows.py` projects the rows into
  `src/self_hosted/compiler/target_profile_row_projection_owner.pgy`;
  `target_capability_owner.pgy` reads the layout values every row shares;
  `direct_mir_array_storage_layout_contract_owner.pgy` computes the Array
  layout (and its layout id and LLVM aggregate type) from them and is invalid
  unless the rows share a 64-bit little-endian layout.
- `tests/ir/target_capability_projection_smoke.sh` and its probe check the
  projection's drift, the host row, five forged rows, a missing row, the
  triple of the module the native LLVM backend emits its object from
  (`PGY_LLVM_DUMP_OPT_IR`), and the refusal of a foreign `PGY_CC` and a
  foreign `PGY_LLVM_CLANG`. `target-capability-test-smoke` now runs as a
  push core step.
- Registry: the row is `CLOSED` with the new consumers and gate; counts
  `CLOSED=63 BRIDGE=16 ACTIVE=2`.

## Results

Run in WSL on main `6a6f612b5` + this change; release driver `driver_t1`
and launcher `pgy_t1` built from it.

- `target_capability_projection_smoke.sh` PASS: projection in sync, host row
  `X86_64_LINUX` (`x86_64-pc-linux-gnu`, `elf`, 64-bit), five forged rows and
  a missing row refused with their diagnostics, the emitted module carries
  `target triple = "x86_64-pc-linux-gnu"`, `PGY_CC="cc --target=riscv64-unknown-elf"`
  and `PGY_LLVM_CLANG="clang --target=riscv64-unknown-elf"` refused with the
  profile they differ from.
- `target_capability_manifest_parity.sh`, `sot_registry_gate.py` and
  `sot_authority_edge_smoke.sh` PASS (`CLOSED=63 BRIDGE=16 ACTIVE=2`);
  component contract PASS.
- Sweep of 2,746 programs on both default routes with `pgy_t1` (clang now
  gets `--target` from the profile on the LLVM route): equal to the
  previous driver's on every program.
- Direct MIR gates that read the Array layout contract
  (`direct_mir_array_string_cleanup_fact_owner.sh`,
  `selfhost_compiler_clone_carriage_owner.sh`,
  `direct_mir_general_route_collection_programs_owner.sh`,
  `direct_mir_general_route_collection_iteration_owner.sh`,
  `default_route_native_runtime_parity_owner.sh`,
  `one_mir_string_case_math_projection.sh`) PASS, with
  `copy_report_owner.sh` and `element_release_loan_refusal_owner.sh`.
- Canonical DRV-2 producer: exit 0, 160 s, peak 4,692,728 KiB.
- Codegen bootstrap: gen2 == gen3 (125,935 lines).
- The Windows and macOS legs and the installed-driver jobs run in CI.

## CI: a third Linux shard

In CI run 45413127760 (`6a6f612b5`) `build-linux-self-host-contracts` was
cancelled at 30.3 minutes. Its shard step took 1,794 s against 998 s in run
45107146975, every self-host step 1.5-1.8x slower, while its dependency
install took 10 s against 24 s. The drivers of `72c2d2747`, `0a9053745` and
`6a6f612b5` take the same time on three large inputs locally (4.0-4.1 s,
22.4-23.0 s, 28.9-29.4 s, equal peak RSS), so the runner was slow, not the
compiler. The two Linux push shards hold about 40 minutes of work against
two 30-minute limits; at 1.8x neither fits. A third job,
`build-linux-self-host-gates`, now runs nine installed-pair gates that build
nothing native (six from the core shard, the zone-sync and zone-spawn
targets and the installed-driver memory bound): about 12, 15 and 11 minutes
of usual work per job. No gate is dropped and no limit is raised.

## Not covered

- Cross compilation: the active row is always the host's.
- `src/compiler/mir/mir_abi_layout.c` still sizes native ABI rows with host
  `sizeof` (`abi.layout_rows`).
- CPU name and features still follow the host (`LLVMGetHostCPUName`); they
  tune code for the build host and are not part of the target identity.
