# Raw View load/store and physical cleanup bridge

Status: bounded implementation verified; main integration OPEN, 2026-11-05.
Run and remaining obligations: [bridge state](../truth/state/raw_view_bridge.md).
Base: `60fd1e26d8ea3987efbcbfc65f59b892b234701a` plus the preserved dirty
main tree; the executable run uses its own source/binary snapshot.

## Objective card

- Objective: connect the supported `Array<Int>` owner / `Slice<Int>` view to
  actual raw data reads and writes, compiler-produced provenance, canonical
  cleanup, and physical release on C and LLVM.
- Priority: existing fact owner and stable binding identity; one cleanup
  obligation; fail-closed missing evidence; kernel proof; physical execution.
- Owners: semantic array/backing and binding facts, admitted MIR expression
  graph and cleanup facts, the existing Array/Slice runtime ABI.
- Last consumers: C/LLVM indexed access and automatic Array cleanup, then
  the runtime allocator. The Rocq importer is a checked projection consumer.
- Forbidden fallback: infer ownership from a raw address or source spelling;
  add an independent backend ownership rule; manually drop the fixture;
  accept public `SlotRawPointer` without its separate scoped ABI contract.
- Gate: one fixed source produces admitted MIR, a checked Rocq memory witness,
  C/LLVM equal output and an observed allocation/free pair with sanitizer
  controls. Mutated/missing origin, owner, bounds, or cleanup evidence refuses.

## Whole chain and dependency order

1. Freeze the native compiler source, runtime and executable independently of
   the main V1 build. Record their hashes and the fixed input.
2. Trace source binding and Slice origin through semantic admission, MIR
   expression graph, and cleanup generation. Reuse those facts; a missing
   required fact is an implementation obligation, not a guessed importer row.
3. Add load/store contents and footprint refinement to the existing Rocq
   ownership model; generate the concrete witness from admitted compiler data.
4. Execute the same source on native C and LLVM. Observe write-through values
   and physical backing release; demonstrate that suppressed/double cleanup
   or corrupted provenance cannot pass the gate.
5. Run existing positive/refusal gates for the reached ownership surface and
   the raw-escape refusal. Run the pinned existing Rocq focused and corpus gates.

This first executable bridge uses the language's View abstraction. The system
raw-pointer escape remains a separately gated future consumer of the same
ownership facts; internal raw execution is not public raw-pointer admission.

## Edit and integration boundary

The fixed baseline exposed two reached obligations: the graph builder already
records native `source_binding_syntax_id`, but the wire emitter omits it; and
the existing cleanup synthesizer has no scalar-Array literal candidate. The
plan therefore adds the existing source-ID projection, a scalar backing
retirement disposition in the existing cleanup owner, and a `ReleaseStorage`
runtime ABI row consumed by both backends. The shared dominance/exit analysis
and its validation remain the retirement authority. A local Slice may only
participate through checked source IDs and indexed accesses; escaping,
reassigned, deferred or otherwise unproved aliases do not gain cleanup.
The source read value must be evaluated before retirement on every return.

Reached edit files: `mir_json_expression_graph.c`,
`mir_json_dump.c` (existing instruction binding-ID projection),
`mir_loop_collection_cleanup.c/.h`, `mir_abi_resource_runtime.c`, the C/LLVM
MIR retirement emitters, LLVM runtime registration and typed runtime array
exports. Existing dirty changes in these files are preserved. Numeric Array
storage is not classified as owned String elements. The new ABI disposition
must not change existing String cleanup rows or formal-parameter graph kinds.

This lane owns new bridge proof, fixture, importer, runtime observation and
gate files plus this plan. The main V1 flow-summary/declaration-projection
owners are excluded. No parallel implementation track or new backend route
is opened. An additional compiler fact change requires an updated chain map
and an exact edit boundary before implementation.

Projection completeness also requires each DEF's existing binding ID on the
wire; `source_locals` names are not an admissible ownership join. The importer
joins DEF, view leaves and cleanup exclusively by these IDs. The MIR cleanup
instruction carries the existing `resource_runtime_fact` row, rather than a
backend lookup or new parallel ABI field.

Reached wire consumers also include the Pergyra MIR cleanup reader and the
native-resource ABI projection. Their existing String-only validation must
accept a scalar retirement only with the typed fresh DEF, exact binding ID,
matching carried ReleaseStorage ABI and the same exit checks. The old
String-kind-3 mutation remains refused. No new admission/emission route is
added. A native-compiled current Pergyra MIR reader will test reconstruction
and corrupted-row refusals before this candidate is handed to main; replacing
the installed driver and bootstrap remain main integration obligations.

The persisted expression-node reader is another reached strict-schema
consumer: it validates the added native source ID while keeping it distinct
from formal binding rows; reconstruction retires this generation's identity.
The cleanup reader's duplicate key is `(block, binding, target)`, matching MIR:
two alternative return blocks may each carry one retirement; two copies on
one exit must still refuse.

The current chat integrates this candidate; the Pergyra main chat receives
the patch and receipts. Budgets: static proof 60 s, focused gate 5 min,
integration shard 30 min. Source/binary publication and V1 closure are not
established by this bridge.
