# Exact inout member release/restore receipt

Observed 2026-10-23 KST. Production slice commit:
`f90dd715736068312b74f35fa499a81a78f35802`. The commit is local and was not
pushed because the unchanged canonical program is still rejected before MIR.

## Objective card

- Objective: admit the canonical straight-line operation that moves an
  `Array<String>` field from the current `inout` aggregate, deep-releases it,
  and restores the resulting empty descriptor to that exact field.
- Priority: exact identity and all-path restoration, then admission. Do not
  infer owned elements or relax the escaped-storage refusal.
- Fact owner: native collection ownership facts, keyed by function, local
  binding, source parameter birth and field syntax identity.
- Last consumers: assignment restoration and the function-exit guard, before
  MIR publication.
- Forbidden fallback: borrowed deep drop, broad `ref`, whole-analysis Clone,
  arbitrary-call or branch admission, or a per-program `direct_mir_*` route.
- Falsifiers: missing restoration, restoration to another field, drop in a
  branch, and an arbitrary call between release and restoration.

## Implemented contract

The native checker now has a transient `RELEASED_MEMBER_MOVE` disposition.
It is issued only for an exact member move from the current `inout` parameter
when the function body contains a direct, straight-line release followed by
an exact restoration. Other exact member releases/restorations may intervene;
a branch, loop, return, nested drop, arbitrary call or unrelated statement
fails closed. The released local remains unreadable except as the exact
restoration value. Function exit rejects any unclosed transient fact.

This is a native bootstrap bridge, not String-element provenance authority.
The self-host aggregate analyzer remains the call-site semantic owner.

## Exact candidate and focused evidence

The six changed semantic source files in the build snapshot
`/root/cc-member-census-20261023-v1` had byte-identical SHA-256 values to the
files committed in `f90dd7157`. The exact product binary is
`/root/cc-member-census-evidence-v2/pgy.product`, SHA-256
`8ed9f3af3b182abcaf3ab3e954110ad1c662435b0cff005e20a0dcc5a5966e7e`.

| Check | Observed result |
|---|---|
| New focused gate | PASS: two positive programs on native C and LLVM, exact output; four negatives refused before MIR |
| Owned-local transition unit | PASS: 15 checks, 0 failures |
| Component contract | PASS: 14 unit checks, source caps and generator drift |
| Minimal release/restore C under ASan+UBSan | PASS with leak detection enabled; exit 0, no report |

The output-bearing positive also ran correctly on C and LLVM. Its sanitizer
run exposed three pre-existing temporary-String leaks (7 bytes total): the
input duplicated by `ArrayPushOwnedString` and two `ToString` values used only
for logging. A no-temporary program exercising the same release/restore path
passed ASan, UBSan and leak detection. This receipt therefore does not claim
general temporary-String leak freedom.

## Unchanged canonical comparison

Both the exact baseline and candidate product runs ended with `33647 error(s),
21 warning(s)` and no MIR. Their 322,404-line stderr files differ only in the
first diagnostic and the non-comparable timing line:

```text
-[ERROR] 96:27 - ArrayDropOwnedStrings cannot release escaped array storage in 'names'
+[ERROR] 236:18 - A String element cannot cross an opaque boundary without a current non-retention proof
```

Exact reporting instrumentation, binary SHA-256
`4f7edc3ee5486c01c6508801801cdda17cd0ea0f4fb76df82af5452fcf8bc350`,
found none of the eight targeted release rows. It then reached deeper hard
errors (`56 error(s), 33614 warning(s)`), so this is not evidence for a
comparable `309 -> 301` final-owner count. The next existing barrier is the
opaque String crossing in `AstTreeArtifactBindDestructurePayloads` at source
line 236. Its producer, proof owner and last consumer must be mapped before
editing.

Bootstrap, gen2 == gen3, the installed C/LLVM driver and required exact-SHA CI
were not reached. Concurrent unrelated compilation was present during the
canonical runs, so their wall time and peak memory are observations only, not
a performance comparison.
