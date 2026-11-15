# 2026-11-15: why the weekly driver shards and Linux job stop

Run by the Claude root lane on WSL `Ubuntu-C-WSL`, with `pgy` and
`pgy-self-driver` built at `d630c8c16` (the content of main `24fe553f4`).
Weekly runs: 43663675229 (`f00fad7b8`) and 43669288274 (`e1a9cba9e`).

## Windows driver shard

The log ends with `CODEGEN ERROR: driver rung-2 artifact request requires
installed composition root`.

- `driver_rung2_body_parity.sh` compiles the test-only DRV-2 entry
  (`driver_rung2_main.pgy`).
- Since 2026-10-22 (`57baf8426`) it hands that binary to the launcher as
  `PGY_SELF_DRIVER_BIN` for `mir_program_error_storage_owner.sh`.
- The test-only entry dies on every artifact request
  (`driver_rung2_cli_read_execution_owner.pgy`). So that gate could pass only
  when a prebuilt driver was given.
- The gate passes on WSL when it is given the installed driver.

## Linux driver shard

Both runs end about four minutes in with "The runner has received a
shutdown signal".

- The shard's first step compiles `driver_rung2_main.pgy` from source with
  the installed self-host driver (`--emit-c-artifact-verified`).
- On WSL that process was at 7.4 GB RSS after 158 s and at 16.6 GB after
  212 s, still growing. It was stopped there.
- A hosted runner has 16 GB and no swap.
- The platform shard script promises to use only the same-run installed
  toolchain, so the driver shard now passes it as the prebuilt driver.

The llvm lane still compiles the same entry with the native pipeline.

- Native C and LLVM both refuse it after about 500 s, with 20 errors.
- The first error is `Function parameter flow summary failed closed:
  required return backing relation is incomplete or stale` at
  `PgyCompilerWorld.CompileSourceToC` (`world.pgy:333`).
  `PGY_DEBUG_FUNCTION_PARAM_FLOW` reports
  `output-rows-stale function=CompileSourceToC ordinal=3`.
- With the store failed, every later default aggregate argument reads as
  escaping. The other 18 refusals ("cannot be used after ... let its storage
  escape") are that cascade. For example, `DriverRung2TryRunFixtureManifest`
  only reads `args`.
- The same callee shape is admitted on its own: a 20-line program passes on
  both native legs.
- `driver_bootstrap_main.pgy` calls the same method through
  `DriverRung2InstalledPublishSourceC`. The same binaries admit it on native
  LLVM with no error.

## `platform-full-linux`

- Both runs stop logging at line 6394, during the `test-all` compile
  (`transpiler_thread_pool.o`).
- The step never records a completion time, and there are no post-job
  lines. The job is cancelled at its 70-minute limit.
- push CI's `build-linux` runs the same `test-all` with no problem.
