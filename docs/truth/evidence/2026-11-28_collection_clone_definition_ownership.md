# Ownership at a compiler-selected array copy

Observed 2026-11-28 KST on `9e1a54e2355a06d0be7c9f175cfaed63f924a99b`
plus this change. The [previous full-input run](2026-11-28_drv2_fixed_mir_refusal_scan.md)
stopped at `SemanticExpressionGraphScalarTypeContractReady` on both targets.
An instruction in that routine requested a Clone of the initializer, while
the binding's ownership row still recorded a member move. Diagnostic
logging at unchanged guards showed that the typed definition had an array
Clone producer and that the owned definition rejected the mismatch. A small
field-copy and set reproduced the same refusal. The semantic trace of that
program placed copy selection at the later mutation.

The installed test-profile driver was
`0d2225a675374e1bd652bf5643c747ef13a8563f1f8c2a0a5e18e4fc97354d04`.
The release-profile installed driver was
`d94571bf56ed3eef492b82933a23fb4e8be9717a8f63ce6861affd962df0d63c`.
Both used generated C SHA-256
`84ae4971e0b1753e9ef33fbe386268e713dcb5c630e36b0d2442347e3ab3d6d1`.
The release installation finished in 281.74 seconds under the existing
14 GiB and 1,800 s boundary. Its receipt and focused logs are in
`/root/drv2-1128/borrow-verified/.tmp/drv2-review/` on Ubuntu-C-WSL.

## Focused execution

- The test-profile driver published a `clone` origin and the `owned-string-set`
  receipt for the same declaration. C and LLVM ran the nested field copy,
  printed `original` and `changed` like the native oracle, and passed ASan
  without invalid accesses. Both refused the missing initializer copy,
  missing set receipt and wrong set receiver without output.
- Source using the earlier lent element after its replacement was refused at
  `indexed_string_use_after_element_release`, without publishing MIR.
- The complete field-copy program still leaks 32 bytes on each target, from
  the original record's array. The related local-to-local copy and set ran
  with the same output and passed both ASan and LSan on each target.
- The existing `member_alias` case now has an independent copy. The installed
  path ran it on C and LLVM and kept the caller's `seed` field. The direct
  alias guard still refused its uncopied member-move MIR on both backends.
- The owned-return, inout member restoration, collection cleanup and compiler
  Clone carriage gates passed. Ownership carriage passed both producers and
  55 malformed-row checks. Component caps and generator drift passed.
- The release-profile driver passed the installed CLI and source-C action
  gates with its companion launcher and machine manifest.

The release-profile driver published MIR from the same complete source
snapshot in `/root/drv2-1128/final3` used by the return-transfer run. The
publication was 357,039,087 bytes, SHA-256
`3d8f3fbff131f83fd4f9341fe36452628410793280ff096b4234a22941f2ab77`.
It took 2:40.86 with a peak RSS of 4,553,956 KiB (GNU time). The
`clone-fixed-producer.receipt.json` records completion under the same bound.
Whole-input C and LLVM emission and CI evidence remain open.
