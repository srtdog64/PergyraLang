# Array provenance and Slice type consumer verification

Observed on 2026-11-07 at HEAD
`60fd1e26d8ea3987efbcbfc65f59b892b234701a` plus preserved dirty work.
This record establishes source-owner decisions and native controls, not an
installed-driver or V.001 completion claim.

## Repair and fixed identity

Main preserves the normalized statement position in
`ast_collection_ownership_member_transition_owner.pgy`. The completed-read
owner can then distinguish a read before a later whole-root return. Raw syntax
positions in repeated/deferred contexts remain opaque; direct member moves and
transitive forwarding retain the existing guards. The unrelated-loop positive
falsifies the raw-position variant, which invalidated the whole permission table.

This lane moved two Slice type consumers to the existing
`SemanticExpressionGraphIndexValueTypeName` owner and preserved default Slice
parameter mutation rejection in the collection mutation policy. No new copy,
manual release, user annotation, unsafe admission or benchmark-specific route
was introduced.

Fixed source: `/root/pgy-owner-production-20261107/candidate-r7`.
All 12,667 source input hashes are in `candidate-r7-source.json`.
Native compiler SHA-256:
`06b6c1981a80ad51dbae43e3d2729d5de25884206ca13a1a22addc0a9913b9c5`.
C source observer SHA-256:
`d463195ec212b0f453b276ed434cda7f398d177fe0addc5962f455353eca6646`.
LLVM observer and each exercised input are hashed in the receipts below.

## Observed gates

- C and LLVM source observers: 64/64 existing/new member-capture,
  indexed-read and completed-read decisions match expected behavior and each
  other. Main separately observed the actual canonical receiver-root input
  admitted; that is main's integration evidence, not a substitute for installation.
- C and LLVM source observers: 17/17 decisions match, including all eight
  unchanged workloads, Array index negatives, Slice writes and live-view
  storage invalidation refusals.
- Four permanent Slice controls: each observer accepts/rejects as intended;
  native C and LLVM each reject the three negatives. The positive executes
  to `7\n10\n`, verifying mutation and a nested Slice read in arithmetic.
- Native build passed. Observer compilation took 156.588 s for C and
  181.627 s for LLVM. The original indexed-read script's 120 s compile limit
  remains RED; the separate integration-prerequisite build is not that gate.
- The existing installed array-index gate now includes the Slice controls.
  Its shell syntax passes; its new installed execution checks remain pending.

## Reproducible patch and remaining boundary

Artifacts: `.tmp/owner-production-20261107/array-slice-handoff/` contains
source and binary manifests, the four observer matrices, native control
receipt, exact pre-promotion source hashes, and the unchanged 120 s failure.
`array-slice-consumers.patch` changes eight files against the preserved r6
snapshot. SHA-256:
`98f8fe3b294e004ae7d504f01ec7f40cd66462321b926871916194293911604f`.
Replay onto that baseline, normalized according to repository LF policy,
passed and reproduced every destination hash. It is not a clean-HEAD patch.

The older installed-pair census accepted five C artifacts and refused eleven
of sixteen C/LLVM builds. The five C artifacts computed correct answers but
retained live allocations. Their remaining reached seams are external scalar
call ABI, Slice consumers, and generic Array storage retirement. These results
are diagnostic evidence from the old pair. Final closure requires the new
admitted installed pair, eight identical inputs on both backends, actual
retirement and the existing 1.3x comparison gate. No such closure is recorded.
