# Self-compile memory A1 execution

- Status: LOCAL INTEGRATION VERIFIED; publication and complete memory target
  remain open in [current state](../truth/state/blocker.md).
- Objective: remove nested instruction/block/routine/program JSON String
  materializations from source-to-C, preserving the admitted schema and
  atomic artifact transaction. This is step 2 of the user's memory plan.
- Priority: ownership safety, byte-equal output, fail-closed projection, peak
  RSS, time. No larger budget, smaller compiler input or admission bypass.
- Fact owner: admitted `SelfMirProgramFacts`. Wire schema has one source:
  the six existing sequential file writers. The memory projections are
  generated consumers of those bodies; they own no schema decision.
- Full chain: source analysis -> admitted facts -> authoritative row writers
  and generated pure specializations -> artifact stream or one flat owned
  fragment buffer -> StringJoin -> machine admission/MIR reader -> C emitter.
  Last consumers: `DriverRung2MirProjectionJson`, canonical MIR execution,
  and the two projection probes. Publication remains with the transaction.
- Complete change set: separate program row framing from its transaction;
  generate six memory specializations from the sequential row functions;
  replace only output handle/write operations with owned-fragment appends;
  delete the nested String projection and migrate imports/registry consumers;
  keep public `SelfMirProgramJson` identity and required row/refusal logic.
  The instruction writer imports its digest owner directly.
- Output lifetime: borrowed tokens/fact strings are copied. A renderer-owned
  fragment transfers first into its cleanup owner, then is synchronously
  written or independently copied. StringJoin copies before the flat buffer
  is retired at one epilogue on both success and rejection.
- Effect boundary: an optional file/memory sink made readonly source APIs
  require io_write even with None. That implementation was removed. Generated
  memory functions are pure of IO; file functions retain their existing
  authority. No additional IO permission, temporary path, runtime builtin or
  public syntax is introduced. TextBuilder's local-owner restriction stays.
- Literal dependency: scratch instrumentation observed initial literal source
  -1 and StringJoin result 0. Their conditional join stayed unknown. Repair
  only the existing indexed String source lattice's literal base case. This
  grants neither ownership nor release permission; retain actual conditional
  element-loan and static-free negatives. No StringJoin spelling exception,
  assignment kill change or weaker deep-drop guard. The unrelated investigated
  retention-selection extension was removed.
- Ceremony boundary: 16 internal inout fragment formals and one own String
  handoff express the mutable output/release boundary. No caller Clone, field
  restoration or user-facing annotation. Explicit scratch drops are removed
  when the later MIR-owned String/array retirement replaces this substrate.
- Gates: generator drift plus compilation; C/LLVM String/file byte parity
  including escaping/domain rows and preserved outputs on invalid facts;
  nine independent pre-change JSON goldens; source-to-C equals MIR-to-C;
  -O0 C/LLVM ASan+LSan sink probe and a borrowed-pool mutation; installed/native
  join/loan/static-free checks; complete MIR-lower source-to-C below 8 GiB;
  full DRV-2 measurement; bootstrap/fixed point and exact published-SHA CI.
- Limits: the existing escaper's substring/result temporaries still leak on
  nonempty escaped input; native may also insert extra String Clones. The
  isolated sink probe does not cover those old allocations. Do not label A1
  compiler-wide leak freedom. Compiler Clone carrier coverage and
  `alias_read_after` guards remain for the owned-value plan.
- The wire probe previously reused facts after a consuming mutation. Each
  invalid case now constructs independent facts; safety rules stay unchanged.
- Validation boundary discovered: native LLVM 18 release optimization aborted
  its writer-probe build with an InstCombine fixpoint assertion; no LLVM byte
  result is inferred from the C result. The installed lifetime/join LLVM legs
  passed. Check the writer's LLVM execution separately and retain this failed
  default-profile result; do not change a backend policy in the A1 schema edit.

## Reached allocation failure boundary

- The complete DRV-2 source-to-C run failed inside the existing 14 GiB address
  space limit, at 14,107,288 KiB RSS. GDB locates a backing write in
  `SemanticAstCollectionExclusiveAddDep`, during consumer ownership checking.
  The AST-consuming C emitter's ordinary array pushes overwrite their only
  backing pointer with unchecked realloc. This is an unplanned reached
  consumer dependency, not a reason to enlarge the memory allowance.
- Existing owner: `codegen/runtime_abi/collection_runtime_owner.pgy`; emit one
  checked push body for Int, Long, String, owned String, Bool, Float, Double,
  AST-node and nominal-record arrays. Preserve each ABI and copy policy.
  Validate capacity and byte multiplication before allocation; publish a new
  pointer/capacity only on success. Use the existing OOM panic contract.
- Last consumer: the installed compiler's array writes, including AddDep.
  Delete the unchecked push bodies. No manual cleanup at the failing source
  site, retry, empty-array fallback or native/LLVM policy change.
- Falsifier: emitted C with forced realloc failure must report class=oom
  instead of storing through NULL. Exercise each array family and preserve
  ordinary output; prove the new codegen seed's gen2 == gen3 and run the same
  complete DRV-2 input again. An explicit OOM still leaves the memory goal open.
