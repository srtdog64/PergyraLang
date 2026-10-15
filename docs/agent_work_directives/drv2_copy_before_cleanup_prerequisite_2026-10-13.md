# DRV-2 copy-before-cleanup prerequisite

Status: SCOPED IMPLEMENTATION COMPLETE; main3a2eefa827 committed/pushed;
whole DRV-2/ownership cleanup and exact-SHA CI remain OPEN.
Base: shared main e2240cb2abf741f30629d72c811a460894a28f91 plus preserved WIP.
Root integrates; exactly the three existing implementers share this checkout.

## Objective card and whole-chain boundary

- Objective: make the existing explicit nested-array Clone boundary actually
  produce independent backing before extending ownership cleanup. This is a
  prerequisite of 27 section 5.4, not permission to accept ordinary String
  returns, introduce automatic drops or claim DRV-2 substitution.
- Priority: existing type/operation identity, actual independent storage,
  matching C/LLVM consumption, fail-closed unsupported types, executable
  falsifiers, then patch size. No annotation, lifetime syntax or budget change.
- Fact owner: existing resolved Clone operation and concrete Array element
  specialization. Its existing inner clone operation owns the recursive copy;
  consumers must not substitute identity copies for known nested arrays.
- Last consumers: generated C PGY_ARRAY_IMPLEMENT clone/SliceCopy bodies and
  LLVM registered pgy_array_clone_Array_*_into pointer-out twins. Source spelling alone is
  not authority to select a callable; preserve the original Clone dispatch.
- Forbidden fallback: nested COPY_VALUE(value)=(value); unsupported element
  type silently treated as deep-copyable; copy success on payload allocation
  failure; copies acquiring ownership of the source's backing.
- Verification: same declared nested Int/String input on C and LLVM, actual
  pointer-independent copies at runtime, source mutation/retirement leaving
  clone unchanged, empty arrays, and allocation-failure refusal. Validate
  unsupported types without executing corrupt IR or unsafe source programs.

The reached chain is checked Clone expression -> concrete element type -> C
specialization or LLVM registered runtime operation -> existing per-element
clone -> new outer and inner backing -> subsequent reads/explicit retirement.
Array/String ABI layouts and original operation names stay stable. The existing
scalar/String element clones are the only admitted leaves here. General
struct/Option glue, ordinary-result allocator/origin/lifetime carrier, caller
automatic retirement, nested literal ownership selection and self-host-only
unsupported nested types remain OPEN; this unit must not invent their facts.
The fixed canonical input still has 6599 refusals; this unit is supporting
copy-safety evidence, not a new independent implementation rung or a reason
to lower that input or weaken its refusal. There is no new top-level fact.

## Independent implementation scopes

- A, drv2_exclusive_flow_impl: runtime nested-array exported Clone/SliceCopy
  copy policy, reached String-copy allocation failure, existing runtime headers
  only. Every inner value is cloned through its already concrete clone routine;
  keep storing/pushing a descriptor as a move distinct from explicit copying.
  Do not add a new drop operation or change borrowed String getters. Add the
  six nested Clone pointer-out twins below at this same physical ABI boundary.
- B, drv2_descriptor_effect_impl: C specialization and LLVM Clone selection/
  registration in existing codegen owners only. Remove known nested-array
  identity-copy fallback, preserve unsupported-type refusal and callable
  identity. Match A's actual pointer-out runtime signatures; existing Clone
  consumer/header edits belong to this reached scope. No runtime/test edits.
- C, drv2_map_payload_impl: permanent focused runtime/compiled C+LLVM fixtures
  and one focused gate under tests. Observe each original failure first if
  safe. Preserve existing test expectations, negatives and validation budgets.
  No runtime/codegen edits; root runs builds and integration.
- Root: navigation, integration, gate recipes, independent validation and
  publication. No overlapping source edits while lanes are active.

## Execution and integration

Use apply_patch. No shared index/ref/commit/push, worktrees, broad process
termination, heavy builds or extra agents. Root alone builds at -j2. Static
budget is 60 seconds, focused parity 5 minutes, integration shard 30 minutes.
Report exact changed paths, actual observed tests, and missing evidence.

Freeze all lanes before one identified current native executable is built.
Root runs the focused nested-copy gate, original nested-array smoke, existing
readonly-map/provenance negatives, then the same native matrix and canonical
input at the integration boundary. Compare source/binary hashes. Send the
scoped candidate to the designated existing red-team chat; scoped approval
does not mean general glue, canonical, installed driver or CI approval.
Preserve a recoverable checkpoint; land only an intentional reviewed unit.

## Reached ABI dependency, decided before LLVM integration

The existing leaf Clone emitter uses a raw LLVM struct return, whereas the
aggregate-return owner admits only StringSplit/args/dirwalk. That is not
evidence for a new nested C ABI: platforms may lower a C struct return through
a hidden output pointer. Do not generalize that owner, reevaluate the Clone
operand or guess a platform hidden-sret convention.

The nested export owner supplies only the six existing concrete nested leaves
with an explicit physical twin:

`void pgy_array_clone_Array_<leaf>_into(PgyArray_Array_<leaf> *source,
PgyArray_Array_<leaf> *out)`

The source comes first. NULL source/output and identical pointers refuse before
writing; otherwise one original deep Clone fills the fresh output. LLVM uses
this ordinary pointer ABI with one lowered input and a fresh entry-block output
slot, then loads the result. Original exported Clone names remain available;
there is no public language out-parameter or new ownership permission. This
ABI twin does not claim a new CLOSED runtime-call registry row. C's known
nested leaf policy must also refuse an unsupported nested Clone at that
explicit consumer, without rejecting legal literal/move declarations.

Compiled LLVM nested SliceCopy and the pre-existing scalar raw-return ABI are
OPEN outside this Clone-only emitter change. Runtime Clone/SliceCopy and C
generated copy bodies share the concrete recursive element rule. The gate
tests the twin's actual success and refusal, not a mocked transport.

## Root acceptance and landing

Three lanes completed; root actual mutation control exposed old C aliasing
and old LLVM refusal. Repaired C/LLVM preserve the copy. Full shared-native
L2fJ64, focused runtime/transport gate and designated external scoped review
PASS. The transport tests moved unchanged out of the unlanded Map unit into
one standalone program: original98 plus Clone286, not duplicate progress.

Exactly base main plus this unit was cold-built before landing, with focused
gate/semantic3191/HIR26/MIR224/transpile1006 and source/binary guards PASS.
The fresh checkout exposed missing .tmp setup, now fixed explicitly. Read
the committed [acceptance audit](../audits/nested_array_copy_prerequisite_2026-10-13.md).
Main3a2eefa827b7b7bd15132a8d5585b07752ccada4 is remote-verified; no new branch,
worktree, source annotation or automatic drop. The full canonical remains6599;
CI38319266060 is in progress, not green at this observation. Whole result/
allocator/glue/caller retirement join remains the same next OPEN obligation.
