# Shared physical String copy and retirement leaf

Status: scoped runtime implementation and focused verification PASS.
Base: main `3a2eefa827b7b7bd15132a8d5585b07752ccada4`.
Ordinary String result ownership, automatic cleanup, DRV-2, installation and
whole compiler CI remain OPEN. This document is evidence, not permission.

## Responsibility and boundary

`pgy_runtime_string_value_inline.h` owns one implementation of
`pgy_string_result_copy` and `pgy_string_result_retire`. The inline C runtime,
external C runtime and LLVM-linkable runtime include the same body. The
existing allocator owns allocation charges, failure and physical release.

Copy requires a valid NUL-terminated readable source. It creates independent
system-heap bytes, including for empty, literal, system and region sources.
The returned allocation is the only valid input to its retirement operation,
exactly once. Source storage class does not confer ownership. A raw non-NULL
`char *` is not a cleanup certificate; this leaf does not validate borrowed,
region, stale or already-retired pointers. Such unsafe inputs are not executed
as negative tests. NULL and allocation failure use the existing panic boundary.

No production compiler consumer grants String return, copy or drop permission
from these functions. No automatic drop, Option transport, layout change,
whole-program leak-freedom or universal memory-safety claim is introduced.
The general result/allocator/glue/retirement bridge remains separate and OPEN.

## Actual verification

- Root `.tmp/string-value-copy.siKGjK/`: actual Make entry, the runtime BC
  contract and C-extern contract finish with rc0. The leaf checks three real
  execution routes, independent pointers and bytes, original release before
  copied-value reads, exact allocation size and one retirement per allocation.
- Nine failure controls (NULL copy, NULL retirement and copy OOM on each route)
  check exact panic class/reason and recognized abort status, not arbitrary
  crashes. Runtime source membership, test/helper inputs, objects and binary
  before/after guards match. The original focused budget is 300 seconds.
- The designated independent reviewer reran the final script unchanged in
  `.tmp/string-value-copy.wTP0cn/`: rc0, 12.7 seconds, all three routes, nine
  controls and guards PASS. The review also checked the Make connection but
  did not rerun the two neighboring static contracts.

Final gate SHA-256:
`2619170709A266F569095B41A44BBB1A7CA51402A671E37859E2FD5901FDBD07`.
Earlier gXvyo8/cWKnzR receipts use the preceding script generation. They do not
replace final-generation evidence. Fresh published-main-only packaging is a
separate check; shared-WIP test success does not establish it.

Root also tested an isolated published-main-plus-only-this-unit source tree
`52e3e13a15d67b3bc68d86ebb6460e3048195000` (not a branch or worktree).
Its real Make leaf and neighboring BC/C-extern contracts finish rc0;
`.tmp/string-value-copy.zGDKFe/` records the three routes, nine controls and
all input/runtime/binary guards. The tree excludes unrelated shared WIP.
This additional paragraph records that receipt; it changes no runtime bytes.

## Remaining real compiler blocker

Main CI38319266060 is terminal RED. The actual Pergyra parser maps node107167
to `ArrayPush(call_target_names, call_target_name.value)` in
`MirIntentExecutionCaptureCompensations`. Its extracted field descriptors can
grow and return on error before restoration; the caller adds a nested field
recovery obligation. This leaf does not address that admission or recovery.
Six dependent compiler jobs were skipped. Formal, Windows, macOS C-only and
TSan success is not whole compiler acceptance.
