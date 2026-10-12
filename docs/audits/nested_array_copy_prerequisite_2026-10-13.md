# Explicit nested-array copy prerequisite

Status: scoped implementation and native verification PASS; general ownership
cleanup, DRV-2 admission, installed driver and compiler CI remain OPEN.
Base: main `e2240cb2abf741f30629d72c811a460894a28f91`.

## Contract and actual defect

The existing explicit Clone boundary for the six scalar/String nested Array
types must allocate independent outer and inner backing. String leaves also
need independent bytes. Ordinary push/literal descriptor transfer is not
this copy operation. No automatic drop or borrowed-return permission changed.

The old native F15paD C compiler ran the safe mutation control with
`99/99/8/changed/changed/beta/0/0`, exposing shared inner backing. Its LLVM
route refused nested Clone (exit 1, no artifact). The repaired C and LLVM
routes both return `99/7/8/changed/alpha/beta/0/0` on the same input.

Runtime nested Clone/SliceCopy delegate to the existing concrete inner clone.
String-copy allocation failure panics rather than publishing a NULL payload.
Array Clone rejects inconsistent length/capacity/backing before copying.
The C specialization emits the same concrete recursive copy; unsupported
nested explicit copies fail closed, not at legal literal/move declaration.

LLVM consumes the six explicit pointer-out twins, source first/output second.
The original clone names and semantic operation identity remain stable.
NULL source/output and identical pointers refuse before writing. The LLVM
consumer requires the actual registered function, runtime mark and exact
void/two-pointer signature, lowers the source once and uses a distinct output
slot. Opaque pointers do not certify semantic element layout. This is not a
new CLOSED runtime-call fact row or a general type/glue descriptor.

## Verification, with generation boundaries

- Current shared-source L2fJ64 generation: phase 3679/0, semantic 3222/0,
  HIR 58/0, MIR 327/0, transpile 1008/0, transition 15/0, LLVM metadata
  384/0, 24 C/LLVM positives and 58 exact analyze-only negatives, provenance,
  projection, registry and source/binary guards PASS. The 384 unit was later
  split without changing its assertions into original Map 98 and Clone 286;
  these are not additional checks or self-host substitution progress.
- Focused production-runtime gate: independent outer/inner/String pointers
  checked before mutation/free, source/copy retirement, empty arrays, 12 exact
  allocation-failure panics and 6 pointer-out refusals; both compiled backends
  return the fixed output. No corrupted MIR/LLVM module is executed.
- Independent designated red-team review: scoped PASS; reran the then-384
  unit and both safe backends, inspected generated C/LLVM and declaration/
  local-callable Clone shadow controls. Intentional abort tests were read in
  root logs, not independently rerun. Existing alias-index/typedef-ordering
  failures reproduce on old and new compilers and are outside this verdict.
- Isolated main plus exactly this unit: source tree
  `0e69eaccf3a8f65a6eb85a3d95acd0cd08cea059`, native binary SHA-256
  `E1E56AB3F2EA076641C545F8D6C6E461752DE69145C37B535C5670A8A09F1A12`.
  Built from scratch, focused gate including Clone transport 286/0,
  semantic 3191/0, HIR 26/0, MIR 224/0 and transpile 1006/0 PASS;
  source/binary guards PASS. Lower suite counts are the published base,
  not deletion of shared-WIP tests. An initial fresh-checkout gate failure
  exposed a missing .tmp parent; the gate now creates it explicitly.

## Remaining obligations and limits

The unchanged full canonical input still reports 6599 errors/9 warnings:
1830 direct helper, 4214 callsite helper and 555 RETURN. Existing String/plain
aggregate implicit copy, ordinary result origin/allocator/glue and caller
retirement must be connected before those loans can be accepted. The SoT
function-parameter row remains ACTIVE. No new source own/ref ritual was added.

Default-CLI nested-array smoke is not green: a temporary native executable
without its self-host sibling refuses that route. An explicit native-only
diagnostic replay passes the original declaration/index/push/depth refusal
inputs; it is not installation or bootstrap proof. Compiled LLVM nested
SliceCopy, original scalar aggregate-return ABI, general nested literal
ownership, shared graphs and general cleanup remain OPEN.

The unchanged 600-line header gates also fail on four untouched base headers
(642, 656, 772 and 880 source lines). No cap, timeout, canonical input or safety
condition was loosened to hide these failures. No universal memory-safety,
leak-freedom, performance or GC-superiority claim follows from this unit.
