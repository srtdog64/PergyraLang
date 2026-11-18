# Bare Array<String> return and copied String operand: bounded evidence

Observed 2026-11-18 KST from local `main` parent
`8343ded4c4961dd8e169b400f463f3c3b0afba71`. The edit checkout was
`D:\PergyraLang`; the seven changed compiler `.pgy` files, fixture and gate
matched the WSL `/root/mg-main` validation copy byte for byte. This is a
reached S1 prerequisite in the [owned-value lifetime plan](../../agent_work_directives/selfhost_owned_value_lifetime_execution_2026-11-18.md),
not completion of S1/S2 or full DRV-2 self-compilation.

The existing return-transfer owner now admits an exact direct local
`Array<String>` return under its definition, unique use, eager evaluation and
all-exit checks. It still checks returned record fields. The existing
expression-edge owner marks only a uniquely used fresh String right operand
of copying `ArrayPushOwnedString` for retirement. C materializes the operand
once, pushes a copy, then frees that source. LLVM consumes the same edge after
its push call. Literal/local borrowed inputs do not receive a free.

## Executed local checks

- Pergyra-seeded installed v8 driver build PASS under the unchanged 14 GiB
  `RLIMIT_AS` and 1,800 s owner; receipt
  `.tmp/selfhost-memory-20261117/bare-return-v8-build.receipt.json` exits 0
  and records a drained process group. Driver SHA-256:
  `9a06da62ed82fa3d4dbd21699931b70f168ebf5f996ad820395b3d295e92b174`.
- New actual-driver source-to-MIR, direct C/LLVM and `-O0` ASan/LSan gate PASS:
  returned array elements and the fresh copied source are cleaned, output is
  `2\nab\nfixed\n`, and a forged MIR origin is refused without artifacts.
  The forged origin is rejected at the earlier array-value ABI boundary, so
  this gate does not prove a transition-specific diagnostic. Omitting the C
  operand free causes an observed 3-byte LSan leak. The v8 C and LLVM emitted
  artifacts are each byte-equal to the previously checked v7 output.
  Receipts: `.tmp/selfhost-memory-20261117/bare-return-v8-focused.log` and
  `.tmp/self_hosted/owned_bare_retired_return.LlpTUD/`.
- Existing Array<String> cleanup gate PASS: refusals 18+4+6+2, admitted
  8x2+native-MIR-2x2+binding-6+nested-2. Component contract gate PASS,
  including 3,097 line-cap requests and generator drift. Local logs are
  `bare-return-v8-cleanup.log` and `bare-return-v8-component.log` in the
  same memory directory. These local gates are not remote CI evidence.

## Full fixed input

Frozen complete MIR SHA-256
`e7767d3059972797baf41654ececd912bfb91a0755547afea6336e6af8bbdf19`.
The v8 driver under the same 14 GiB / 1,800 s boundary stops at
`stage=collection_ownership_transition ordinal=1438`; the previous v5
driver stopped at 1437. Receipt `bare-return-v8-fullmir.receipt.json` records
exit 1, `child_failed`, and a drained process group. GNU time reports peak
RSS 3,928,948 KiB; GNU and wrapper elapsed clocks disagree, so no wall-time
comparison is made. No C artifact was emitted. The previous complete
one-process source-to-C v5 run remains OOM at 14,077,584 KiB, with no output.
The independent transition failure census is still incomplete. No full
direct-MIR admission, source self-compilation, value-retire S1/S2 or new
exact-SHA CI result follows from this slice.
