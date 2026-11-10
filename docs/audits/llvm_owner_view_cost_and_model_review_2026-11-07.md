# LLVM owner/View cost and design review

Observed 2026-11-07 KST. The user requested the LLVM comparison, a <=1.3x
direct-C target and a review of the owner/View approach. These are scratch
native-emission experiments, not a production compiler patch.

## Verdict

The owner plus borrowed View model is viable for the measured workloads.
Ordinary Slice is a pointer and length, not a copy of the elements or a
per-access ownership lookup. Six algorithms reach 0.76-1.06x the faster
direct-C comparator under common O3 release settings; the Slice buffer is
1.15x. The extra direct-owner buffer is **1.300606x**, so the strict aggregate
1.3x gate remains OPEN. It must not be rounded into a pass.

This supports keeping the design, with lifecycle facts visible through
access and final retirement. It does not establish whole-language memory
safety or close growing/returned-storage cleanup gaps. Production optimization
exclusions are unchanged. The [frozen receipt](../truth/evidence/2026-11-07_llvm_owner_view_cost_trials.md)
records exact identities, measurements and controls.

## Reached chain and findings

1. **CONFIRMED — literal construction.** Typed constant Array/count ->
   `llvm_expr_aggregate.c` -> zero descriptor/repeated pushes -> runtime
   allocation/growth. Bulk scalar initialization removes those calls and
   reallocations without changing later accesses or synthesized drops.
2. **CONFIRMED — target compatibility.** Emitted functions lack the target
   attributes on the Clang-built runtime. LLVM 18 reports conflicting
   attributes and refuses inlining. Deriving matching CPU/features from the
   actual configuration removes that refusal. No alias, nonnull, overflow
   or floating-point promises were invented.
3. **CONFIRMED — checked access remains a call.** After target alignment,
   LLVM reports cost 260 versus threshold 225 for ArraySet and 265 for
   SliceGet. Whole-module visibility alone fails the speed target. Inlining
   preferences expose the original runtime checks and accesses.
4. **CONFIRMED — final retirement matters.** Exposing unchanged storage
   retirement bodies, together with cold failure entrypoints, removes opaque
   drop calls and improves graph/LRU workloads. This intervention combines
   both preferences; their individual timing contributions were not isolated.
5. **CONFIRMED — prototype boundary correction.** The first constructor shim
   included the C-inline runtime and introduced its separate context/budget
   path. The final shim calls the existing LLVM typed allocator through
   Clang's aggregate-return ABI. C's `pgy_alloc` path and LLVM's direct
   allocation are existing implementation differences, not permission to
   bypass a production budget contract for speed.

Final chain: admitted source/MIR -> scalar literal IR -> existing LLVM
allocation ABI -> checked accesses -> existing cleanup body -> physical free.
Matched target, runtime visibility and executable symbol internalization let
LLVM consume those facts. Direct C also receives whole-module optimization.

LLVM's [inline cost rules](https://github.com/llvm/llvm-project/blob/llvmorg-18.1.3/llvm/lib/Analysis/InlineCost.cpp)
and [X86 compatibility rules](https://github.com/llvm/llvm-project/blob/llvmorg-18.1.3/llvm/lib/Target/X86/X86TargetTransformInfo.cpp)
agree with the missed-inline diagnostics. Local artifacts establish the cost.

## Design and adoption boundary

- Keep one backing owner and automatic retirement; View borrows storage and
  extent without acquiring a second cleanup responsibility.
- Keep necessary checks. Remove a check only when existing range, backing
  and lifetime facts prove it redundant for that execution.
- Distinguish static Slice from a reusable dynamic handle. The latter needs
  owner/generation/lease validation. Earlier C-manager evidence supports a
  checked pin boundary and access within that lease, not access after unpin.
- Region/bump allocation is a storage strategy inside a lifetime boundary;
  it does not replace ownership, View validity or retirement obligations.
- Judge each fixed workload. A low average cannot hide a failing case.

`llvm_runtime_attrs.c` and `llvm_api.c` deliberately retain/externalize checked
arithmetic and accessors after historical panic/overflow miscompiles. This
experiment compiles emitted IR with Clang and leaves that policy intact. Eight
original panic fixtures pass here; the actual native optimizer and installed
driver must also pass before their exclusions can change.

Production work must bind target facts to `llvm_target_machine.c`, carry the
literal/ABI decision through native and self-host general emitters, preserve
runtime failure/allocator owners, and verify actual C/LLVM drivers. Constant
Int initialization does not establish effectful-element ordering, owning
aggregate transfer, custom allocator or partial-initialization behavior.
The owner-buffer ratio and growing/returned-storage cleanup remain explicit;
Main's V.001 integration and published-commit gates remain separately owned.
