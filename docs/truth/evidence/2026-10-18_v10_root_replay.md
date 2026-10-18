# V10 capture allocation bounds: root replay

Observed 2026-10-18 KST on uncommitted compiler source atop local main
`4a5372b0c8dfbf30ed6891b03c4bf6c7fb37a613`. The production native in
`/root/pgy-nonowner-20261018.5t1W96` was
`6b5b00baf5f21a28b351ce51273e1cea1f1be1fa9c56e34064465cb251c356d0`.
The final narrow receipt is `.tmp/p0-all-def0-candidate.hq0oq8`; its 224
listed source inputs still matched both the Linux copy and D checkout by
`sha256sum -c` after all runs. This supplements the earlier
[post-gap receipt](2026-10-18_v10_capture_allocation_bounds_post_gap.md).

| Root execution | Observed result | Linux receipt |
|---|---|---|
| size/build, phase, MIR | PASS; phase 13,422/0, MIR 683/0 | `/root/pgy-readfile-v10-root-final-narrow.log` |
| focused allocation rollback and capacity-tail release, ASan+UBSan leaks ON | PASS | `/root/pgy-readfile-v10-root-allocation.log` |
| exact C/LLVM function flow | PASS; rejected slot-wrapper secondary diagnostics retained | `/root/pgy-readfile-v10-root-strict.log` |
| AIR/RIR/DIR | 161/0, 44/0, 15/0 | `/root/pgy-readfile-v10-root-air-rir.log` |
| full transpile unit | 1,014/0 | `/root/pgy-readfile-v10-root-transpile.log` |
| semantic, map, flow and sanitized phase/MIR | semantic 3,230/0, transpile 1,014/0, AIR 161/0, four flow gates PASS; ASan+UBSan leaks OFF | `/root/pgy-readfile-v10-root-broad.log` |
| sanitized AIR/RIR/DIR | PASS, leaks OFF | `/root/pgy-readfile-v10-root-san-origins.log` |
| fresh relative-root replay | five units PASS on 20 inputs; two withheld required inputs each rc1; exact restoration PASS | `/root/pgy-readfile-v10-root-portable.log` |
| original and two upward-count native probes, query first | original passes; both reject binding, validator, C, LLVM and parallel plans before output mutation; source/certificate unchanged; exact restore PASS under ASan+UBSan, leaks OFF | `/root/pgy-readfile-v10-capture-current.log` |

The first direct invocation of the allocation smoke script from a copied
runner failed because its repository-root discovery resolved `/root` instead
of the validation copy. Re-running the unchanged script from its actual
`tests/` directory passed with leak detection enabled. The failed invocation
is retained in `/root/pgy-readfile-v10-root-allocation.log` only as the later
successful run; its earlier runner record is
`/root/pgy-readfile-v10-root-allocation.runner.sh` and was a harness error.

The unchanged complete fixed and current canonical programs still both exit
1 with MIR0, 18,435 errors and 16 warnings. The first error in each is
`PGY_SEM_BORROW_ESCAPE` at `diagnostic_owner.pgy:573:39`,
`ArrayPush(codes, found[j])`: `A String element cannot cross an opaque
boundary without a current non-retention proof`. Fixed/current took
33.67/33.92 seconds and peaked at 1,730,056/1,728,048 KiB under concurrent
local validation; these are observations, not a comparative benchmark.
Receipt: `/root/pgy-readfile-v10-root-canonical.log` and
`.tmp/p0-paired-full-canonical.zEil5A/`.

V10 restores the reached native storage safety boundary. It has not admitted
the canonical input, bootstrapped the self-hosted compiler, executed the
installed C/LLVM driver, or produced green published-SHA CI. The next
semantic seam is the owned String element from a returned `Array<String>`
flowing into `ArrayPush`; the complete producer/consumer chain must be mapped
before editing.
