# Readonly record-array carriage on the general MIR route

Observed 2026-11-28 KST on `4609ab835765ed0130fe71e77d376dd98cd1d088`
plus this change. The release-profile installed driver built in WSL
`/root/drv2-1128/borrow-verified` under the existing 14 GiB / 1,800 s
boundary. Driver SHA-256
`edd5d54181aa2f9796766ebc3796b89758112424cf0203956cbd66c335abb017`.
Generated C SHA-256
`84766ee2346991ee52284d9c5abdd6c5da09c063e28e54d8aaa5d2def85718ed`.

The callable-envelope diagnostic examined all 8,528 instances in the
357,038,615-byte canonical MIR from the
[return run](2026-11-28_direct_mir_owned_result_return.md).
It reported one unsupported formal, `readonly-ref Array<DirectMirScalarProgramRoleMethodDeclaration>`
on `DirectMirScalarProgramRoleImplsJoined`. It exited before GraphPlan
admission and emitted no artifact. Receipt `envelope.receipt.json` is in
`/root/drv2-1128/final3/.tmp/drv2-review/`.

## Executed checks

- The base installed driver refused the extended record-array fixture at
  the same callable-envelope check on both backends. The candidate's
  installed driver emitted both C and LLVM. Each compiled and ran tests of
  local borrowing, forwarding a readonly formal and borrowing a by-value
  formal. Each printed the same three `9` values as the native compiler.
  Changing the readonly array to an indirect pass shape was refused on both.
- The existing record-array gate, Array<String> readonly gate, owned array
  return gate and component contract passed. The Array<Int> readonly gate
  passed its runtime and MIR checks. Four source diagnostic expectations
  failed identically on the base and candidate, and each input was refused.
- ASan on the three new calls found no invalid access. In a separate
  LSan check that excludes stale stack/register pointers, both backends
  report a 64-byte unfreed record-array allocation. The same array and
  reads with by-value formals on the base produce the same 64-byte leak.
  The record-array backing still lacks automatic retirement for this case.

The previous return commit passed the installed CLI and source-C action
gates. Its seed-side full bootstrap produced byte-equal gen2 and gen3 C
(220,855 lines, SHA-256
`9dcf145cf0bf7e6f5e5317245b3dfbcc9c85ddb1a755888832e63a06c468a99f`).
The general-route full-input gates and remote CI remain open.
