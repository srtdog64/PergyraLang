# Read payload and foreign String retention: bounded observation

Status: SCOPED NATIVE REPAIR VERIFIED; broader FFI/View safety and canonical
acceptance OPEN. Original gap predates the candidate.
Date: 2026-10-16 KST. Main recovery checkpoint: c64719068995a7ccf59504d56da87ed05facf80d;
published main: 39a8fc1db0dc7162ba75ac7f9584f4cf096d8efe. Observed source
candidates are identified by the binary and immutable receipts below, not by
an assertion that the dirty tree is a published revision.

## Fixed falsifier

```pergyra
subject TextLedger { let text: String; }
extern "C" { func Keep(text: String) -> Void; }
func Snapshot(ref ledger: Slot<TextLedger>) -> Void {
    let cur: TextLedger = Read(ledger);
    Keep(cur.text);
}
```

Source: `.tmp/p0-destination-slot-payload-retention-20261016.pgy`.
SHA256: 121e6ba00c82c6874614abca1bd3f5038a3ca1937e5bd3beb795eac466150f78.
No foreign implementation, linkage or execution was performed. A retaining
foreign implementation is a possible boundary, not an observed runtime UAF.

## Root-observed evidence

All three exact binaries admit the same source: exit0, MIR schema1, no error.

| Binary | SHA256 | Source provenance |
| --- | --- | --- |
| d61 | d61d3695e471e67be549fb2ec0c38fb95889a981b7ac4338d6d7b945fe8abaaa | frozen TUl3PL |
| f835 | f835b13e1bf9ccc80c4872d2eba7147c73db1e05a0672343099409c4fc9e3120 | frozen QUBf9g |
| e8a | e8a21cd12f520dfe2e0d03ea4d092abb80f67976dd06b37dd5565674653d79a1 | current destination/Read candidate |

Comparison receipt:
`/root/pgy-main-20261016/.tmp/p0-read-retention-compare.lGQ3WZ/`.
Its runner and input remain in the ignored scratch area; existing immutable
review snapshots were not modified.

Emit-only C and LLVM on e8a also succeed. In
`/root/pgy-main-20261016/.tmp/p0-read-retention-emit.WpzSaI/probe.c`:
lines62-63 assign `pgy_read_TextLedger(ledger)` to the local aggregate and pass
its `.text` directly to `Keep`. The corresponding LLVM function loads the
payload and calls `Keep` with the extracted pointer (probe.ll1253-1264).
There is no deep copy on that path. Runtime `plain_slot_inline.h` returns
`s->value`; a String-containing payload is a shallow value copy. This does
not by itself establish the lifetime of the external use or an actual crash.

## Owner and scope boundary

The existing successful Read owner validates Slot/view, lifecycle, secure
token and parallel access. Publication owns the checked result relation;
backing and container-identity queries are two projections of that receipt.
Read can preserve payload backing without returning its container identity.

The existing summary skips optional backing analysis for an anchored input
with a Void result, even if a later call may retain a payload field. Source
inspection found no automatic Clone/retain on the inspected Read, local
binding or default foreign String argument path. This is not proof that every
other foreign/lifetime route is unprotected.

The contract in `docs/semantics/27_ownership_clean.md` section3 requires a
lend/consume ABI row for a foreign heap-typed parameter; section5.6 refuses a
missing callee summary rather than assuming all-borrowed. The same document
marks production implementation and physical refinement OPEN. A model theorem
is not evidence that this native path implements the contract.

Independent frozen review reproduced the original case, a smaller
Slot<String> Read-to-Keep, direct ref TextLedger-to-Keep refusal, and a
Read-to-Log positive. It classifies this as an existing admission soundness
gap and an explicitly unfinished owner connection, not permission to assume
an independent copy or that the opaque foreign function does not retain.
The missing consumer is the optional backing-frame selector: anchored inputs
with Void/scalar results skip the already issued Read payload relation.
Read's synchronous input borrowing does not prove result independence.
Review receipt: `/root/pgy-review-destination-39a8fc1db.QUBf9g/.tmp/redteam/read-slot-retention-contract-review.md`.
Do not run this unsafe falsifier; no runtime lifetime trace was observed.

The new phase candidate's two failing assertions belong to this new negative;
no pre-existing negative oracle was changed. Keep the counterexample and its
rejection expectation. The bounded repair must also preserve current
Read-to-Log and exact same-owner Write-back without declaring Write generally
nonretaining or treating every anchored input as ordinary borrowed storage.

V1 remains unchanged canonical admission, actual bootstrap/fixed point,
installed-driver parity and required same-commit CI. No new all-anchored
analysis track, FFI language policy or closure claim is authorized by this
observation. Existing positive/negative, work and identity gates remain in force.

## Scoped implementation and current evidence

The successful typed Read receipt now retains two separate facts: container
identity does not flow out of Read, but the returned payload can retain backing
from receiver0. The backing owner selects that current payload dependency;
it does not infer a deep copy from synchronous borrowing. Successful typed
Write can restore that backing only to the exact original input binding,
not another Slot or a guessed shared root. Complete outer-subject construction
uses current actual field sources, not subject freshness. Phase fault controls
invalidate cached permissions when those typed receipts or identities change.

The safe same-owner subject update emits and runs on C and LLVM with output
`2\nkept\n`. Its first LLVM failure exposed a separate last consumer: active-MIR
CALL receiver lookup now consumes the existing admitted MIR expression-type
owner and registered nominal identity. Missing/unregistered facts do not retry
the old callee-name inference. It is not a Read-name codegen exception.

Independent1283 review additionally found scoped REF ReadView<String> -> Read
-> opaque Keep emits MIR with existing summary READ|CALL_ESCAPE=17. This was
not missing effect production: final parameter and generic boundary consumers
discarded COPY_ONLY non-String summaries. The descriptor's copyability is not
permission to retain its payload. The four-file repair connects exact current
REF ReadView contracts and sealed escape masks to declaration and deferred-call
validation, with current actual argument/callee joins. It neither reclassifies
View globally nor adds an alias/Read-presence analysis. Missing evidence and
ALL grant no permission. Ordinary local copies and nonretaining/physical reads
retain their existing behavior.

Root-observed binaryfd17c96ab954311f85a924f314b866f088dc1331584cd985829909f9282bf8c2
passes phase6779/0, LLVM receiver metadata140/0, strict summary/work controls,
whole native MIR327/semantic3228/transpile1009, Map24 C+LLVM positives/58
negatives, all reached IR/caps/component gates, Slot goldens and three pin/view
C+LLVM runtime pairs. Unsafe retention negatives were only analyzed/emitted,
not linked or executed. Receipts: clone `.tmp/p0-destination-candidate.3yBcBq/`
and `p0-read-view-{native,chain,slot,pin-parity}.log` beside that clone.
Immutable follow-up reviewkGgLNi completed; report `.tmp/redteam/review.md`
SHA256 is8f12e3e803e76999e4ed7da0f2d5a3341b3b74157ecc8c96f1555d1f7211d574.
It confirms the scoped REF refusal but finds a separate P1: a local Main
ViewRead result can be stored through bodyful inout Save, followed by Release
and use of the output. Local return and opaque Keep/wrapper/pin variants also
emit MIR/C/LLVM. The inout/return aliases do not depend on an external retention
assumption. None was linked or run. Current runtime Release marks occupancy;
physical payload cleanup/lifetime refinement is not established by that alone,
so the evidence is admission and alias flow, not an observed UAF.

The current Read row already owns exact caller, receiver and result facts, but
no final local Read-payload loan registration consumes it. Existing borrowed
root lookup follows REF formal provenance, not arbitrary local initializers.
The next scoped plan is to reuse the existing sparse loan evaluator with the
actual checked Read call as seed, not add another syntax/alias scan. Main with
zero formals needs no invented parameter ordinal. Before implementation,
resolve the legitimate caller-NULL/global execution owner and preserve exact
owned-Slot Write-back, physical reads and nonretaining consumption. This would
check eager RETURN/CALL/CHANNEL/output escape, not establish all source lifetime
or Release-after-last-use properties. Zero-input result implementation is held.
Root observed top-level String/Int Read positives both emit MIR/C/LLVM onfd17,
receipt `.tmp/p0-top-level-read.C3zEQw/`. Their backend-synthesised Main does
not exist during semantic publication. A callerNULL failure or validation skip
would respectively regress that supported path or leave the admission gap.

Reviewer-created REF/default View helper positives can pass semantic admission
yet fail C/LLVM emission on existing local-view/ABI facts. Direct Main
Log/Length/physical/pin positives emit on both backends. These are separate
backend capability limits; no failed helper emission is a memory-safety pass.

Unchanged full bootstrap is still RED before gen0, first required-backing
failure at418:6,22.87s/1025260KiB. No gen2==gen3, installed-driver, broad FFI
soundness, whole-language memory safety, SoT closure or green CI follows from
these scoped native tests. No runtime UAF was executed or claimed.
