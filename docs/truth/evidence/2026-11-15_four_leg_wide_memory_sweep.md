# 2026-11-15: four-leg memory sweep beyond the lifetime fixtures

Run by the Claude root lane. Earlier ASan measurements covered only the C
leg of the 868 lifetime fixtures. This run widened them in two steps.

## LLVM legs of the lifetime fixtures

`/root/fx_llvm.sh` builds each admitted program's IR with every `define`
marked `sanitize_address`, because clang does not instrument `.ll` input
otherwise. It instruments the IR with clang and links it by gcc against
`pgy_runtime_lib.c`, which is built with gcc ASan/UBSan. A use-after-free
IR witness must be caught first.

| | native LLVM | self-host LLVM |
|---|---|---|
| Admitted (of 868) | 568 | 174 |
| Memory errors before | 1 (`numeric_string_bool_negative`) + recursion | 6 + recursion |

- Native: `ToString(Bool)` was a select of two static strings, while C
  returns a heap copy. Fixed in `dab1fc0f7`.
- Self-host: six positives freed one `Array<String>` buffer twice or read
  it after free. The C leg never showed them, because struct programs take
  the round-trip C route. Refused since `f6ab5f0ce` and `afbd9ab7f`.
- The direct MIR LLVM leg now admits 136 programs. It refuses 38 more:
  six that crashed, and others that only ran clean because a callee's
  growth fit the literal's capacity of 4. The gate's `grown_record` input
  pushes past that capacity and crashed on the old driver.

## Every corpus on four legs

`/root/fx_wide.sh` covers 2597 programs: tests/cases (2248, including
backend_compare), examples (267), and the collection_member_place,
collection_bootstrap_closure and owned_string_argument_sources fixtures.
They run on native C (nc), native LLVM (nl), self-host C (sc) and
self-host LLVM (sl). The measured driver and compiler were built at
`e304e1cae`.

| leg | admitted | memory errors |
|---|---|---|
| nc | 2005 | 10 |
| nl | 2005 | 10 |
| sc | 1501 | 40 |
| sl | 676 | 10 |

- **nc/nl:**
  - Five `defer_*_invalid` cases were a regression from `a7d7655e1`.
    The weekly flow-summary gate expects them refused, but push CI does
    not run it.
  - Three collection_member_place negatives and `reassigned_member`.
  - Endless recursion in `function_param_flow_summary/main.pgy`.
  - Fixed in `226613c3c`. Only the recursion remains.
- **sc:** almost all are `*_landed_valid` cases. Native lands a compiler
  `Clone` at the sharing site; the self-host round-trip route admits the
  same program without it. The rest are `*_invalid` cases the self-host
  admits.
- **sl:** `by_value_*_landed_valid`, allocator pools and String returns.
- The four UBSan reports are an artifact of the harness, which compiles
  the emitted C without `-fwrapv`.

## Weekly full pipeline

`platform_full` had failed for five weeks without running on main. Run
43663675229 was dispatched on `f00fad7b8` and run 43669288274 on
`e1a9cba9e`. Failures and their state:

**Fixed:**
- macOS C-only build: the LLVM Clone owner had no guard (`e1a9cba9e`).
- LLVM InstCombine fixpoint abort: `instcombine-no-verify-fixpoint` on
  every function (`e1a9cba9e`). Codegen parity passes on CI's LLVM. The
  local LLVM 18.1.3 ignores the attribute.
- Stale probes:
  - the domain-topology digest probe wrote a `let` field;
  - the hashmap probe changed arrays after copying them;
  - the one-MIR index gate linked IR without the runtime.

  All fixed in `e1a9cba9e`. Harness-evidence passes.
- Native over-refusal of `valid_array_builtins` (`226613c3c`).
- Operator transpile tests broken by the entrypoint rule (`e7bb64e06`).

**Open:**
- The Windows driver parity shard stops on "driver rung-2 artifact request
  requires installed composition root".
- The Linux driver parity runner received a shutdown signal after four
  minutes.
- `platform-full-linux` was cancelled after 75 minutes.
