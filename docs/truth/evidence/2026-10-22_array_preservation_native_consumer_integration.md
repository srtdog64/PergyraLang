# 2026-10-22: native Array-preservation consumer and loan/copy integration

Recorded by GPT root. Source checkpoint:
`f25b7bb6e286944c99c89c0dd8a53527f9c2564a`. Scoped native checks passed;
canonical admission, installed-driver integration, CI and V1 remain OPEN.
Nothing was pushed. This is not whole-language memory-safety evidence.

## Source and whole-chain scope

- Main fast-forwarded from `7e7b63673` through Claude `3cdb56256` (seven
  commits, including semantic publication `a35ecdd60` and the 24 landings).
- GPT committed the consumer as `cc560937ba8ae126f2bdd4b9e97aaeaca8a02899`.
- Once handed over, Claude's **whole** loan/copy fix `c877a46e4` was merged
  with that consumer into `f25b7bb6e`. Its two rules were not split. None
  of the other worktree's uncommitted implementation was copied.
- [Closure plan](../../agent_work_directives/mir_array_preservation_consumer_integration_2026-10-22.md).
  The authoring skill kept the positive loop programs free of manual drops;
  no canonical-source edits, extra own/ref annotations or safety exemptions.

The semantic owner alone publishes `array_preserved`. HIR/MIR carry it;
MIR entry checks a one-to-one join against the supplied semantic snapshot,
and MIR rows are checked against HIR. AIR's copied-row certificate and
source-MIR binding include the field. MIR/AIR JSON and dumps expose it.

Cleanup runs at the complete routine-inventory boundary, before method
views/specializations are sealed. Direct function/formal identity, arity,
ABI, both positive bits, read-only flow and scalar/Void result must agree.
Unknown/partial/invalid evidence grants no cleanup. No callee-body query,
name route, Slot-mask-only substitute or weaker birth check was introduced.
Own/inout/async and method/generic grants remain outside this slice. Native
C/LLVM consume the existing checked edge; the legacy self-host reader
still refuses it. No Task B route deletion or Pergyra substitution claimed.

## Frozen input and executable receipts

WSL `Ubuntu-E-WSL -u root`, `/root/gpt-mir-preservation-20261022`, was
overlaid from `git archive f25b7bb6e`; only the two retired fixture names
were removed from this private copy. Main's source/tests were clean at
that checkpoint; the unrelated dirty design document was excluded.
Archive: `C:\Users\user\.codex\tmp\pergyra-preservation-20261022\merged-f25b7bb6e.tar`,
SHA256 `c45be6411fab8f388b583e1ba17e1bf1f7944ee12c63fd9679c91780be1c3aaf`.

| Executable | SHA256 |
|---|---|
| `bin/pgy` | `71f72fe6f99f1cbc3ccec676dba1eb203c907a595e763a96360c63f2fa0298fe` |
| `bin/test_mir` | `63247178cd776eeeb1d952dc12a1170f97906c99e7242f3625d92e410e3f131d` |
| `bin/test_air` | `484727e5eaf47fa338cd63b6d2dcda685446482b0ced50dca41539eb59d515f0` |
| `bin-consumer-san/test_mir` | `2b5f1d73f0b90ab5fc9822ce0fb18ea48de6271ee58b6d25d2fa50459026ff47` |
| `bin-consumer-san/test_air` | `c76e976a8aad5744d55a7778adcabe73f15fafd675c53bb7f48cb5a2172fec6d` |

Log prefix below: `/root/gpt-mir-preservation-20261022`. The release
hashes were unchanged at the end (`.merged-status.log`). Build succeeded
with 13 warning diagnostics in `.merged-build.log`, not a warning-free build.

## Observed gates

| Gate | Result / log suffix |
|---|---|
| MIR / AIR / HIR | 702/0, 163/0, 58/0; `.merged-{mir,air,hir}.log` |
| Semantic / transpile / DIR / RIR | 3237/0, 1014/0, 15/0, 44/0; `.merged-{semantic,transpile,dir,rir}.log` |
| Native loop gate | PASS C/LLVM, including later-declared default/ref readers and Void observer; `.merged-loop.log` |
| Loop controls | builtin reads, owned/borrowed elements, continue/break/return, nesting and explicit drop pass; escape/defer refuse without an artifact |
| Evidence faults | absent0, descriptor-only1, element-only2, unknown4, crossed ordinal, WRITE and missing row refuse; exact restoration passes |
| Projection/certificate faults | changed HIR vs semantic, missing snapshot, duplicate projection replacing another formal, changed AIR row and source MIR preservation refuse |
| Full function-flow target | PASS ordinary and currency/worklist verification modes; phase13429/0, memo48/0, worklist31/0; `.merged-flow.log` |
| MIR/AIR ASan+UBSan | 702/0, 163/0, leaks OFF; `.merged-san-{mir,air}.log` |
| 28 emitted-C runtime controls | ASan+UBSan, exact outputs, leaks OFF; original24 plus four loan/alias controls; `.merged-landing-sanitizers.log` |
| Header size / documentation | PASS; `.final-integration.integration-{headers,docs}.log` |

Generated `.tmp/merged-declared.json` carries `(formal0, mask1,
array_preserved3)` for Count, ReadCount and Observe. The loop prints `6`
on C/LLVM. The isolated declared-call cleanup fixture prints `ok` with
empty stderr and leaks ON on both backends: C ASan+UBSan; LLVM body ASan
(via `opt-18 -passes=forceattrs -force-attribute=sanitize_address`) and C
runtime ASan+UBSan. See `.merged-emitted-sanitizers.log` and
`.tmp/preservation-sanitizers/`. Neither whole-program leak freedom nor
LLVM-body UBSan is claimed.

The flow gate retains its known slot-wrapper secondary undefined-symbol,
type and borrow diagnostics. A preliminary duplicate-row test wrongly
assumed an Int formal had a demanded row; the final test uses two actually
demanded Array callees. Only the final passing test is evidence.

## The reproduced UAF and its integrated repair

Before `c877a46e4`, pinned `3cdb56256` and the consumer-only executable
both admitted `lent_path_copy_grown_landed_valid` and ASan caught a
heap-use-after-free: a getter result holds `graph.rows`, a local path-copy
reallocates it, then the result reads the old backing. Logs:
`.followup-loan-base.err`, `.followup-loan-probe.err`.
The input SHA256 is
`b1ee5bb976d36e7a86e0bb64ad4ad63148c2615d2c45f8c27d3f5d23f82c40f7`.

The merged executable passes that same input with output `75` under
ASan+UBSan. The repository flow gate requires exactly one landing and
C/LLVM parity. `returned_alias_lends_formal_valid` prints `22` with zero
landings. The non-deep-copy counterpart
`returned_alias_lends_formal_grown_invalid` is refused on both backends
with `PGY_SEM_MOVE_FROM_RELEASED` at29:15; neither artifact is published.
This closes the named reproducer, not every possible loan/alias pattern.

## Required integration still red

`make test-mir` exits2 after native units/loop/topology/destructure pass:
match-binding requires an unavailable installed self-host driver.
String-result control exits1 at22:27: shallow mutation withdrew `fields`'
owned String-element premise; the pinned baseline has the same refusal.
Receipts: `.merged-integration-status.log` and
`.final-integration.integration-{test-mir,string-result}.log`.

Unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy` still aborts
before MIR: rc1, 33,644 cascading errors, 21 warnings, MIR0.
First error61:55 is a borrowed String retained beyond its Array owner.
The ERROR/WARNING/error-total stream matches the pinned baseline, SHA256
`3c88d28d14277c169e29c423bf9a46df476f03bd997a60c582c7be8e8965a926`
(`.base-canonical.err`, `.merged-canonical.err`). Current run:93.70 s,
3,439,836 KiB peak RSS with concurrent work; no speedup comparison.

The independent final-owner census is **Claude's measurement**, not a
fresh GPT census:956 -> 904, moves217 -> 166;289 in `direct_mir_*`,615
outside. Its input/rules/results are in the
[producer record](2026-10-22_native_census_returned_alias_lends.md).
Ordinary cascades above are not that census.

Current bootstrap, installed C/LLVM and gen2==gen3 remain unestablished;
no bootstrap replay was attempted after unchanged canonical admission failed.
Published main remains `39a8fc1db0dc7162ba75ac7f9584f4cf096d8efe`, verified
remotely2026-10-22. [CI38860052641](https://github.com/srtdog64/PergyraLang/actions/runs/38860052641)
and [Platform39150389845](https://github.com/srtdog64/PergyraLang/actions/runs/39150389845)
are failures on that SHA. No candidate CI, known-red push, SoT closure or
independent intermediate review. Resume the named blocker; do not open
another precision/owner track from these local passes.
