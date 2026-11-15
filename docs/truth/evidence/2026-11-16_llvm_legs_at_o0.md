# 2026-11-16: LLVM-leg sweeps at -O0

Run by the Claude root lane on WSL `Ubuntu-C-WSL`. The binaries are from
the Track G tree `/root/tg-own`, which `0809be9c7` squashes onto main.

## Why -O0

The LLVM-leg harness compiles each emitted `.ll` with clang, with every
`define` marked `sanitize_address`, and links it against the gcc ASan
runtime.

We checked it with a witness: an IR `main` that frees the same pointer
twice.
- Built at `-O0`, ASan reports the double free.
- Built at `-O1`, it does not, because LLVM deletes the second free.

So every native-LLVM and self-host-LLVM sweep before this date, which all
used `-O1`, could hide a double free. `/root/legs_lib.sh` now defaults to
`-O0`.

## Results at -O0

| Corpus | Leg | Admitted | Sanitizer reports |
|---|---|---|---|
| 868 lifetime fixtures | native LLVM | 568 | 2 stack overflows (endless recursion) |
| 868 lifetime fixtures | self-host LLVM | 167 | 1 stack overflow; `post_field_return_negative`: double free |
| 2602 programs (tests/cases, examples, three fixture dirs) | native LLVM | 1995 | 1 stack overflow (`function_param_flow_summary/main.pgy`); `intent_aggregate_readonly_boundary_valid`: unconfirmed |
| 2602 programs | self-host LLVM | 670 | none |

- `post_field_return_negative` is an `Array<Int>` record on self-host
  LLVM. It is a real double free. Track G also found it with glibc heap
  checks.
- `intent_aggregate_readonly_boundary_valid` on native LLVM fails under
  clang-instrumented IR. ASan reports a stack access at offset 31 of the
  `InspectIntentRows` frame, but the error-kind string in the report is
  garbage. We could not confirm it:
  - the same IR built at `-O0` without ASan runs clean under
    `MALLOC_CHECK_=3`;
  - the native C leg built with gcc ASan at `-O0` runs clean;
  - the function's IR has no out-of-range access.

  The instrumentation (clang 18) and the runtime (gcc 13) do not match.
  Settling this needs a clang ASan runtime or valgrind, and neither is
  installed on this host.
- Track G fixed `bundle_field_nested_read_positive` (self-host LLVM),
  which the `-O1` sweeps had missed.
