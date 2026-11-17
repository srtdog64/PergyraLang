# Nested returned storage keeps one retirement responsibility

Observed on 2026-11-18 KST, parent `568bf7433`, with the source changes
carried by this commit. This repairs a prerequisite of the
[memory plan](../../agent_work_directives/selfhost_compile_memory_plan_2026-11-17.md),
not String/Array last-use synthesis or the one-process DRV-2 memory goal.

## Reached chain

Semantic aggregate named-input reservation consumes the local responsibility
when its storage travels through a constructor into a result whose caller
will retire that field. The existing wire consequently carries
`empty-literal / retired / retired`; this is not itself a physical free.
The constructor hand-off ledger retains the named
`SELF_RETIRED_CONSTRUCTOR_INPUT_KEPT` divergence. Neither producer nor table
was weakened or changed.

The direct-MIR transition consumer already checks a returned aggregate
transfer, but inspected only the outer constructor's immediate fields. Its
existing endpoint owner now follows exact typed constructor-field edges in
one DAG pass. Calls and member reads grant no transfer. Duplicate carried
uses, mismatched field types, non-last uses, repeated definitions and
uncovered exits retain their refusal checks. Both backends consume the same
checked terminal; no new local or element release right is issued.

The coverage input fingerprint moved unchanged from the flow owner to the
existing coverage issuer. Its final-plan caller imports that issuer. No
production owner or route was added and no cap increased: flow 390/421,
coverage issuer 136/170 code lines.

## Frozen generation

Editing checkout: `D:\PergyraLang`; validation: WSL `Ubuntu-C-WSL`,
`/root/mg-main`. All 2,840 production `.pgy` files match the normalized map
SHA-256 `b1a60b19b82dc52e790ece8e749b67625f8c587e92aa50aeba97f679607eef0e`.

| Artifact | SHA-256 |
|---|---|
| Native seed | `97f33eab89e7e264ea37c6f9239b886e4129e639e3dc337c556e205216443c48` |
| Existing Pergyra-built codegen gen2 | `ea6c8e08b8bf8d3ff143768be2ac269b339847d6345c27d464a989fd8b2fc1c8` |
| Newly Pergyra-built installed driver | `808a2233a3875a45664bb478232e8d8c8bcb538e683384eac73dfa9c71071980` |
| Its emitted C | `ac3676e4458eb18ca60dd19d494b494b05690b7277afc82241b8e2ed9e612619` |
| Complete fixed DRV-2 MIR input | `e7767d3059972797baf41654ececd912bfb91a0755547afea6336e6af8bbdf19` |

## Executed checks

- Actual installed-driver build and its smokes PASS; no native replacement
  driver was substituted. `nested-transfer-installed-build.{log,receipt.json}`.
- The small nested constructor plus caller retirement refused on both old
  backends; the exact same MIR now emits C and LLVM.
- `direct_mir_array_string_cleanup_fact_owner.sh` PASS, evidence
  `.tmp/self_hosted/direct_mir_array_string_cleanup.z6wvMh`: all prior
  18+4+6 refusals preserved; 2 invented-end refusals added; prior positives
  and two nested-return executions pass. Empty and owned element factories
  return through three record levels; the caller observes and retires them.
- C and LLVM `-O0` ASan+LSan PASS, zero reported leaks. A test-only LLVM
  copy marks all 22 definitions `sanitize_address`; a raw LLVM store after
  free is detected, so libc interception is not the sole instrumentation
  evidence. Premature free, duplicate free and omitted free C mutations
  respectively trigger UAF, double-free and LSan (all exit 86).
- LLVM initially could not link because its Clang 18 ASan archives were
  absent. Installed matching `libclang-rt-18-dev` version
  `1:18.1.3-1ubuntu1` and its three package dependencies in WSL. The successful
  logs/JSON are `nested-transfer-asan-runtime-fixed*`; compiler IR is unchanged
  by that test-only sanitizer preparation.
- Component owner caps/generator drift, regenerated language registry and
  documentation gates PASS. The failed published Linux job was precisely
  implementation-inventory drift; its owner renderer regenerated the file.

## Full-input result and limits

The unchanged complete MIR now passes transition 1126 and stops at
`builtin-call`, global instruction 19788, node 3: `ArrayReverse` of
`reversed_actuals: Array<String>` in `SemanticCallSpineViewFromGraph`.
`nested-transfer-full-direct.{log,time,receipt.json}` records exit 1, peak
3,597,284 KiB, no C artifact, unchanged 14 GiB address-space/1,800 s bounds
and drained process group. The parent and child clocks disagree, so no
wall-time comparison or memory-improvement claim is made for this prefix.

A separate non-optimized diagnostic census was interrupted within its
owned boundary (exit 143, drained); it did not enumerate every failure.
A native admission-observer build timed out at its 180 s pilot bound and
is not claimed as a native validation PASS. The actual driver above was
instead built through the required Pergyra codegen seed.

The first proposed test logged owned nested String elements before a later
retirement; semantic refused `aggregate_release_element_borrow`. This
unfixed frontend limit remains in the failed scratch producer log. The
admitted test observes their lengths, and both memory and output checks run.

Full one-process source-to-C is still OPEN; this endpoint-only change has
not established S1/S2 or a lower bound for that path. The last measured OOM
and next executable rung are in [measurements](../state/measurements.md) and
[blocker](../state/blocker.md).

At publication preflight, CI run 44140377041 on parent `568bf7433` has 32
successful executed jobs, including both bootstrap sides, fixed-point join
and installed-driver contracts. Its remaining failure is the generated
inventory above; macOS passed its one bounded rerun after an initial runner
capacity cancellation. The commit carrying this repair still needs its own
required CI; parent jobs are not evidence for this candidate.
