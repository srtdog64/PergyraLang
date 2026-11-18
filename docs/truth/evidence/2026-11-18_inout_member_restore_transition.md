# Inout member restoration: bounded execution record

Observed 2026-11-18 KST. Edit checkout `D:\PergyraLang` on published
`main @ d21aa036dc7599085c61325c45e8954f7113357f` plus this candidate.
The changed `.pgy` source in WSL `/root/mg-main` matched the D checkout in a
recursive checksum comparison. The Pergyra-seeded v11 installed driver is
SHA-256 `e32b0d5c9f0f3013e4834a342881407e7e51b4bb5b044e250dd6a82cb22a8a53`.
Its build receipt `member-restore-v11-build.receipt.json` records a completed
14 GiB `RLIMIT_AS` / 1,800 s process and a drained group.

The member-restore query now admits an exact value-result parameter identity
as well as an exact declared-local identity, never both. The CFG consumer
retire-transfers a live member-moved local when that field is rebound. If the
local was deep-dropped first, a same-block restore carries only the zeroed
descriptor when the drop is its last use or definition before the restore.
A generic retired state alone is not a release or restoration proof.

## Executed gates

- New installed-driver source-to-MIR and C/LLVM direct-MIR gate passed. Its
  live inout restore prints `2` on both backends under ASan. The three-field
  drop-then-restore prints `0` on both with ASan and LSan enabled and clean.
  A source that hands the field to an `own` callee then reuses it is refused
  as `owned_argument_storage_not_live`, with no MIR artifact. Two forged MIR
  facts are refused without C/LLVM artifacts at the earlier MIR integrity
  boundary; that refusal is not transition-specific evidence. Scratch output:
  `.tmp/self_hosted/inout_member_restore.R6TfTo/`.
- Existing Array<String> cleanup gate passed: refused `18+4+6+2`, admitted
  `8x2+native-mir-2x2+binding-6+nested-2`. Scratch log
  `member-restore-v11-cleanup.log`.
- Component contract passed, including 3,097 line-cap requests and generator
  drift. Keyword registry check and docs truth size check passed locally.
- Fixed complete MIR input: 418,865,243 bytes, SHA-256
  `e7767d3059972797baf41654ececd912bfb91a0755547afea6336e6af8bbdf19`.
  The v11 driver stopped at `collection_ownership_transition ordinal=1438`,
  the same first refusal as published v8, with no C artifact. Its unchanged
  14 GiB / 1,800 s boundary receipt has exit 1, `child_failed` and a drained
  process group; GNU time reports peak RSS 3,928,628 KiB. Wrapper/GNU clocks
  differ, so elapsed time is not compared. Logs: `member-restore-v11-fullmir.*`.

## Open memory boundary

The nonempty `Bag` fixture still leaks its record field at routine end.
Running the v11 C and LLVM artifacts with LSan enabled reports 32 bytes in
one allocation on each; the native C artifact reports 16 bytes. The focused
gate disables leak detection only for this nonempty record and checks invalid
access/double free. The empty three-field case runs with leak detection on.
This is an S2 record-field retirement obligation, not leak-freedom evidence.
The last full one-process source-to-C remains the v5 OOM under the same 14 GiB
bound. Full MIR admission, DRV-2 self-compilation, S1/S2 and CI for this
candidate remain open.
