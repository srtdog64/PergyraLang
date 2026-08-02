# Beta Readiness Checklist - Active Status

> Split from `docs/100_beta_readiness_checklist.md` on 2026-05-29.
> Keep active blocker edits in the shard that owns the relevant closure track.


WebGL dogfood boundary (2026-05-04): the beta dogfood path is a bridge, not
language surface. `make dogfood-webgl-test-smoke` proves that emitted C can keep
host-import/frame-callback terms and optionally link through Emscripten. It does
not freeze WebGL APIs, renderer syntax, native LLVM wasm, or `pgy.render.webgl`;
those belong to post-beta module ecosystem work. The smoke uses the shared
Windows/MSYS path helper; missing `emcc` may skip only the optional wasm link,
not the emitted-C bridge check.

Self-host boundary (2026-05-24, clarified 2026-05-26, reviewed 2026-06-13):
hard self-host does not start from the compiler core with the current beta
stable subset. Self-hosting is a post-beta consumer of the language spine, not a beta source-of-truth owner. Compiler-adjacent tools may remain as dogfood
evidence, and soft/partial self-host preparation may continue, but beta closure
order is CFG/AIR/DAG/MIR/ABI language trust first. Substrate gaps are tracked in
`docs/self_hosted/05_compiler_core_gap_analysis.md`; they are handoff material,
not permission to rewrite parser, semantic, MIR, codegen, or runtime owners.
Gate: `make self-host-preparation-test-smoke`.

Installed C substitution update (2026-08-01): the admitted public C artifact,
compile/link, and `--run` envelopes now obtain exactly one C artifact from the
sibling fixed-point `pgy-self-driver`; native code owns only host compile/link
and optional execution. Missing driver and unsupported envelopes fail closed,
and an exactly-once shim gate rejects native semantic/codegen fallback. This is
target-specific `SUBSTITUTING` evidence. LLVM-enabled default builds, released
general LLVM, package, dump/check/repl, and production-root intent remain open;
hello-only direct LLVM reachability cannot promote those claims.

Direct LLVM aggregate update (2026-08-01): the bounded Option<Int> match
fixture now uses one routine-local typed match index, one seven-block AIR
certificate, one reconstructible ABI receipt, and one target-capability-bound
plan to emit exact C and textual LLVM executables. Both print `42` twice;
seven MIR mutations plus repaired-digest certificate/ABI/target/plan negatives
fail before artifact creation. AIR cannot reopen match JSON, and a selected ABI
projection carries only its chosen backend mapping. This is `SUBSTITUTING`
inside the production direct-MIR mode.

Direct LLVM Array update (2026-08-01): the bounded runtime-free local
`Array<Int>` literal/reassignment fixture now uses one typed expression-graph
owner, one target-neutral plan, and one selected ABI projection to emit exact C
and textual LLVM executables. Both print `3` and `10`; LLVM has no Pergyra
runtime reference. Element kind, index kind, length target, stale SSA use, ABI
offset, source type, and unsupported static index mutations fail before
artifact publication. The same plan owns both backend projections; scalar or
hello dispatch cannot be retried after Array classification.

Direct LLVM multi-routine update (2026-08-01):
`array_return_literal.pgy` now uses a strict row-order-independent program
identity, one target-neutral call/return/use/ABI/lifetime plan, and one selected
C or LLVM mapping. The producer fills caller-owned fixed storage and the real
producer/caller graph prints `4` then `3`; LLVM has no Pergyra runtime
reference. A routine permutation is artifact-equal. Thirteen mutations cover
entrypoint/callee/signature, caller SSA, canonical ABI including repaired-ID
field drift, straight-line CFG reachability/terminality, and forged Log scalar
facts. Multi-routine rejection cannot retry any single-routine planner.

Direct LLVM parameter update (2026-08-02):
`array_literal_call_argument.pgy` now carries the complete `Array<Int>` ABI row
on its formal parameter. Native and self-host producers agree on all nine
parameter fields. One target-neutral three-routine plan preserves the real
`Double` and `SumPair` calls, gives Main ownership of fixed backing storage,
and passes the aggregate by value. C and LLVM both print `11`; cyclic routine
order is artifact-equal and sixteen parameter/call/use/ABI/CFG mutations fail
before publication. Installed public C and LLVM compile/run use this frontier.

Direct LLVM nominal-struct parameter update (2026-08-02):
`struct_literal_call_argument.pgy` now obtains `Vec2` and nested `Line` physical
receipts from one program-owned topological layout owner. The `Line` formal
parameter cross-seals the exact declaration receipt; native and self MIR agree
on field order, types, layout, and parameter schema without requiring their
producer-local syntax IDs to be numerically equal. One target-neutral
row-order-independent plan preserves real `Twice` and `Width(Line)` calls. C
and LLVM both print `6`; routine/declaration permutations are artifact-equal,
and fifteen declaration/ABI/call/lifetime mutations fail before publication.
Installed public C and LLVM compile/run use this frontier as bounded
target-specific `SUBSTITUTING` evidence.

Direct LLVM nominal-struct value-flow update (2026-08-02):
`struct_literal_value_flow.pgy` now carries the exact program-owned `Pair`
declaration receipt on its aggregate routine return and local definitions. One
target-neutral row-order-independent plan preserves the real
`BuildPair(pair.right)` call, the latest mutable value, and member reads. C and
LLVM return the aggregate by value and both print `11`; thirteen receipt,
declaration, call, latest-use, and member-path mutations fail before artifact
publication. The JSON producer and both backends are negative-gated against
type-spelling or expression-text layout recovery. Installed public C and LLVM
compile/run use this bounded target-specific `SUBSTITUTING` frontier.

Direct LLVM Option-of-nominal value-flow update (2026-08-02):
`option_struct_value_flow.pgy` now composes the static Option tag contract with
the exact program-owned `Pair` receipt. One target-neutral plan preserves a real
`BuildPair(Int) -> Option<Pair>` return, Some/None/Some replacement, explicit
unwraps, and an independent chained unwrap-member read. C and LLVM both print
`7`, `11`, and `5`; routine permutation is artifact-equal and twenty negative
executions reject missing/corrupt outer and inner receipts, repaired geometry,
stale uses, unresolved calls, and flattened unwrap/member paths before output.
The nominal classifier runs once and cannot retry the plain-struct plan after an
Option rejection. Installed public C and LLVM use this bounded target-specific
`SUBSTITUTING` frontier.

The producer distinguishes nominal identity from physical ABI ownership.
Declarations whose layout is not required, including a struct containing
`String`, retain the neutral instruction receipt `(kind=0,row=-1,id=0)`; only
`required==1` declarations may carry nominal or Option-nominal layout IDs. The
current TestHarness manifest and bootstrap seed gate this boundary.

Direct LLVM explicit-generic nominal value-flow update (2026-08-02):
`generic_struct_field_value_flow.pgy` now carries one strict
`Identity<T>(value:T)->T` template, four uniform `T -> Int` specialization
receipts, the exact program-owned `Pair` ABI, and three row-order-independent
routine identities into one target-neutral plan. C and LLVM preserve one real
`Identity_Int` definition, four calls, one real `BuildPair` definition/call,
and aggregate insertion/extraction; both print exact `7`. Routine and
specialization-row permutations are artifact-equal, and twenty-nine mutations
reject generic header/body/ABI, specialization, call, receipt, repaired-layout,
SSA-use, and member-path drift before publication. Native MIR has no
specialization rows and is deliberately not used as a fallback oracle.

The multi-routine root now delegates all three-routine programs to one
declaration-cardinality classifier. A rejected generic nominal plan cannot be
retried as an Array or nested-struct program. Rebuilding also exposed and
closed a prior scalar receipt defect: a typed `Int` return with no physical
layout preserves `abi_type_name=Int` while requiring layout ID zero, required
false, and null layout.

Direct LLVM inferred-generic nominal value-flow update (2026-08-02):
`generic_struct_field_inferred_value_flow.pgy` now carries a distinct two-row
Value-lane specialization class, strict `Identity<T>(value:T)->T`, two exact
inferred calls, one program-owned `Pair` receipt, and the latest SSA use into
one target-neutral plan. C and LLVM preserve one real `Identity_Int`
definition, two calls, aggregate insertion/extraction and exact output `42`.
Routine, specialization, combined-order, and coherent opaque-owner renumber
metamorphics are artifact-equal; thirty-two negative executions reject generic,
specialization, graph, ABI, SSA, and member-path drift before publication. The
two-routine root reads specialization cardinality once, so an inferred-plan
failure cannot retry plain or Option nominal projection. Native MIR's empty
specialization table remains common graph/ABI evidence only.

Installed LLVM substitution update (2026-08-01): plain public LLVM binary
requests use the sibling Pergyra-built driver for exactly one source-to-MIR
production and one direct LLVM projection. `clang -x ir` is the only final host
boundary; native semantic/AIR/libLLVM and implicit runtime-object fallback are
closed. Missing, unsupported, producer/projector, malformed-IR, and unresolved-
runtime cases fail without publishing a new or stale binary. This is executable
`SUBSTITUTING` evidence for the sealed runtime-free Option, local `Array<Int>`,
the bounded two-routine Array return, and three-routine Array parameter
frontiers, including the bounded three-routine nominal-struct parameter slice.
The bounded nominal-struct and Option-of-nominal return/local value-flow slices
are also substituting; this is not evidence for general, heap-backed,
runtime-bearing, or arbitrary multi-routine programs. The bounded explicit and
  inferred `Identity<Int>` nominal value-flow slices are now also substituting.

Direct LLVM inferred-generic scalar assignment update (2026-08-02):
`generic_return_assignment_inferred_flow.pgy` now carries an exact mixed-lane
two-row specialization class, a strict generic identity routine, a real wrapper
call, initial and latest local SSA identities, and exact typed/null ABI receipts
into one target-neutral plan. C and LLVM preserve real `Identity_Int` and
`ReturnIdentity` definitions/calls and execute exact `41`. Five ordering/opaque-
owner metamorphics, two independent value variants, 55 C negatives, and three
LLVM sentinels are green. Three-routine routing is fixed by the exact
`(declaration, specialization, generic-routine)` cardinality tuple, so removing a
specialization cannot reroute the program through Array projection. The next
falsifier is `generic_member_inferred_flow.pgy`: its 6,482-byte self MIR owns one
`Box` declaration, generic value-receiver `Echo<T>`, and two Value-lane member
specializations for the nested call. Both direct targets currently fail closed
in the inferred-generic program envelope; the intended output is exact `41`.

Direct LLVM inferred-generic member update (2026-08-02):
`generic_member_inferred_flow.pgy` now carries an exact class declaration,
generic value-receiver signature, two uniform Value-lane member specialization
receipts, the two nested `Box_Echo_Int` calls, and the inner-result-to-outer-
argument edge into one target-neutral plan. C and LLVM preserve a real
specialized method definition, pass the same value receiver through both calls,
and execute exact `41`; installed public C and LLVM compile/run use this
frontier. Five order/opaque-owner metamorphics, five value/name/collision
variants, 70 C negatives, and four LLVM sentinels are green. Exact two-routine
classification prevents retry as plain/Option/inferred-direct nominal. `Box`
has the explicit internal representation `internal_single_int_value_class`,
not a physical ABI receipt. Raw arrays are exact-tail checked, and generated C
temporaries cannot collide with legal source locals. The next falsifier is
`generic_member_constructed_return_flow.pgy`: its 9,309-byte self MIR carries
`Wrap<Int> -> Option<Int>` into `Echo<Option<Int>>`, followed by unwrap and
exact output `43`. Both targets currently fail closed because no exact
three-routine constructed-member owner exists.

Constructed generic member update (2026-08-02):
`generic_member_constructed_return_flow.pgy` is now target-specific
`SUBSTITUTING` through the installed self-host driver. One 9,309-byte MIR drives
real `Wrapper_Wrap_Int` and `Wrapper_Echo_Option_Int_` calls in C and LLVM,
preserves `Some(Int) -> Echo(Option<Int>) -> checked unwrap`, and executes exact
`43`. An exact three-routine classifier and separate declaration, signature,
heterogeneous-specialization, substitution, graph, instruction/SSA, Option ABI,
internal-representation, plan, and emitter owners prevent fallback to the
uniform two-routine or unrelated planners. The focused gate proves eight order/
identity invariants, five value/name variants, 40 C negatives, and five LLVM
sentinels; public installed C/LLVM and adjacent inferred generic routes are
green. The next falsifier is `generic_member_array_return_flow.pgy`: its
9,225-byte MIR reaches both direct backends but fails before publication at the
Option-specific constructed-specialization owner. The next closure must own
`Array<T>` substitution, fixed `Array<Int>` storage/lifetime, nested member
carriage, and indexing without weakening the completed Option plan.

Constructed Array member update (2026-08-02):
`generic_member_array_return_flow.pgy` is now target-specific `SUBSTITUTING`
through the installed self-host driver. Its final 9,228-byte self MIR drives
real `ArrayWrapper_Wrap_Int` and `ArrayWrapper_Echo_Array_Int_` calls in C and
LLVM and executes exact `44`. `Main` uniquely owns the one-element backing
storage; Wrap fills it through a hidden pointer, Echo carries the admitted
Array shell by value, and Main performs the real index/load. A neutral
specialization-pair owner parses the wire once before an exclusive Option or
Array projection, so neither family can retry or reopen the other. Native/self
return-source identity is also aligned on `AST_ARRAY_LITERAL`. Six ordering and
formal invariants, two value/name variants, 27 C negatives, seven LLVM
sentinels, installed public C/LLVM, hard contract, and component inventory are
green. The next falsifier is `generic_member_record_array_return_flow.pgy`:
its 11,952-byte MIR adds a `Point` declaration and `Array<Point>` carriage, and
both direct targets currently fail closed at the exact three-routine structural
shape boundary. The next closure must own the mixed declaration class, Point
and Array ABI facts, caller storage, nested calls, index and field projection
without broadening the completed single-class path.

Constructed record-Array member update (2026-08-02):
`generic_member_record_array_return_flow.pgy` is now target-specific
`SUBSTITUTING` at `8bd92069`. The same 11,952-byte self MIR drives real
`RecordArrayWrapper_Wrap_Point` and
`RecordArrayWrapper_Echo_Array_Point_` definitions/calls in C and LLVM,
preserves caller-owned fixed storage, by-value `Array<Point>`, index zero,
the loaded `Point` and `.x`, and executes exact `45`. Exact admission owns the
mixed declaration class, three-routine identity, cross-domain unique positive
source IDs, Main instruction envelopes, Point and Array receipts, nested graph,
storage lifetime, representation and Log. Three variants, 35 C negatives, ten
LLVM sentinels, installed C/LLVM, the hard substitution contract and full
component contract are green.

The public direct-MIR Array storage contract is explicitly four-field
`{data,length,capacity,allocator}` under
`pgy.runtime.pointer64-size_t64.v1`. It is not the private three-field growable
container used by the self-host compiler itself. Storage layout is not calling
convention: this slice also carries a separate closed-module call-ABI fact and
makes no external interoperability claim. A fresh full DRV-2 build completed in
104.381 seconds with peak private 1.937 GiB and peak working set 1.836 GiB,
below the 2.4 GiB attention threshold.

Architecture review disposition (2026-08-02): the review's Pair and
`Array<Point>` implementation claims were observed against an older HEAD and
are no longer current. Its structural warning is accepted: the exact fixture
families have reached the point where another topology-named planner/emitter
would be a mini-compiler, not healthy closure. Before adding a third aggregate
shape, the active ratchet is to promote the completed constructed
`Array<Int>`/`Array<Point>` decisions into one representation-parameterized
aggregate value-flow plan and make the old duplicate decision entries
shrink-only. Exact `44` and `45` plus every existing storage/call/ABI/SSA
negative must remain green. This consolidation is architecture work and is not
counted as new self-host substitution progress.

Aggregate storage promotion update (2026-08-02): checkpoint `96b7f88e` moves
the shared target-bound Array storage decision behind
`DirectMirArrayStorageAbiProjection`. Array<Int> and Array<Point> family ABIs
derive the same data-layout ID, 32/8 size/alignment, four field names/offsets,
C `size_t` and LLVM aggregate/indices; direct layout-owner imports are
negative-gated. Both C artifacts now emit six layout assertions. The generated
caller-storage binding is owned by a collision-aware, block/parameter-scope
`_pgy_array_storage_N` fact rather than the C-reserved `__*` namespace, and the
exact source-parameter collision is executable in both lanes. Array<Int> also
now carries the shared closed-module/no-external-interop call receipt. The next
ratchet is the target-neutral aggregate value-flow fact; it must consume sealed
family evidence without reading MIR/JSON or erasing the distinct captured-Int
versus nominal-Point ABI provenance.

Aggregate value-flow promotion update (2026-08-02): checkpoint `e24d5652`
closes that second and final consecutive SoT-only ratchet. Constructed
Array<Int> and Array<Point> plans now share one target-neutral aggregate-flow
fact and one target projection for representation, element/Array identity,
caller storage, index, construction/identity, allocator/lifetime/carriage and
closed-module call ABI. Family-specific admission remains the authority for
different evidence: Array<Int> carries a physical Array receipt, while
Array<Point> carries a dedicated typed-absence fact derived from the admitted
MIR receipt. The latter's exact digest, not the Point declaration digest, is
cross-sealed into `aggregate_flow.abi_evidence_id`. C/LLVM artifacts remain
byte-identical and execute exact 44/45 through installed paths. This promotion
does not add substitution coverage. The active executable rung is the passive
`vessel Cell` inferred generic-member fixture, whose 6,527-byte self MIR is
currently rejected by the two-routine classifier on both direct targets. The
next delta must reuse the existing generic-member plan/emitter and execute
exact 42 without a vessel-specific family or native fallback.

Passive vessel member update (2026-08-02): executable checkpoint `ceb43938`
closes that falsifier. One existing generic-member plan now preserves exact
`class/value` and `vessel/mutable-identity` host/carriage pairs. C uses an
initialized stable `Cell` address for the formal and both nested calls; LLVM
uses one initialized stack `Cell` and the same pointer for both calls. The same
self MIR executes exact 42 in both installed public paths, while class remains
exact 41. Focused host/carriage/ABI/SSA/no-retry negatives, hard/component and
installed C/LLVM gates are green. No vessel-specific plan/emitter or native
fallback was added. The active executable falsifier is now
`nominal_tobject.pgy`: its 2,857-byte self MIR reaches the one-block backend
owner but both targets reject it as unsupported scalar facts. The next delta
must reuse nominal declaration/graph/ABI facts, preserve a real tobject
construction and field read, execute exact 12, and add no tobject-named
mini-compiler.

External review intake (2026-05-08): beta readiness now explicitly tracks
operational and trust risks that are not new language features:
toolchain/preflight clarity, release/debug hygiene, memory/string bounds audit,
MIR-missing diagnostics as hard errors rather than partial generation, security
runtime portability claims, documentation/implementation drift, and anchored
ownership failure coverage. These items must be closed with diagnostics and
smoke gates, not by broad marketing claims.

Current status (2026-06-10): this checklist is the beta execution contract.
The criterion is not feature count; it is **surface trust + structural
sustainability + C/LLVM parity + CFG-backed body safety + AIR-backed
abstraction safety + dogfood-first path**. Feature feel is about 85%, while
strict beta readiness is now about 83%. The older 75% anchor is no longer
accurate for the current implementation surface: current gates cover broad
semantic, HIR, RIR, MIR, AIR, C transpile, LLVM smoke, backend inventory,
ABI/Slot/Pin ownership, worker-boundary, memory-concurrency, diagnostics,
debug-hygiene, and CFG body-dataflow contracts. The low-80% line is still not
beta-complete: current full-suite evidence must be refreshed after each closure
slice, and remaining compatibility fallbacks must not decide beta-stable paths.
The active closure work is now to finish the last source-of-truth consumers
across CFG/AIR, remove guarded backend compatibility scans from MIR-owned
resource/declaration paths where possible, and keep MIR/LLVM declaration
bootstrap plus ABI/Slot/Pin contracts frozen by smoke gates.

C-backend parity tightening (2026-06-10): the C transpiler MIR resource-op
emitter now gates concrete runtime calls on a same-block paired
`MIR_INST_STMT`. Previously the SSA def-block could carry a use-block's
Write resource op (e.g. a while-loop body Write attributed to the slot
def block) and the C path emitted the runtime call in both blocks, while
LLVM emitted it only in the use-block. C/LLVM parity is now block-local
on this dimension. The default `backend_compare` registry grew by 16
fixtures (798 -> 814) covering nested-if / for / while / break / continue
/ return / branch-return pin exits, while-loop Slot reads, conditional
loop Slot writes, `if/else` and `match`-with-default inside Slot-reading
loops, `ref Slot` helper handoffs inside loops, channel sends inside
loops, defer alongside Slot Log, and async/spawn accumulator into a Slot.

ABI/Slot/Pin §4 partial closures (2026-06-10): source-level
`PGY_SEM_PIN_TOKEN_INVALID` now fires when `ViewRead/ViewWrite` is
applied to a `SecureSlot<T>` whose paired capability token symbol is
not reachable in the current scope (runtime ABI hard-fail remains the
deeper backstop); `runtime-abi-lifetime-test-smoke` audits the pool
runtime-owned handle contract
(`pgy_pool_spawn`/`_despawn`/`_get`) alongside the file-descriptor
contract; the intent borrowed-snapshot thread-local store is
explicitly recorded as scratch-teardown-safe; and the exceptional /
cancellation pin-exit blocker is closed by audit (cancellation
rejected at source level, panic = process abort with OS cleanup, C
local-scope exit already cleaned through GCC cleanup attribute).

P0 #1 §0b read-seam closure (2026-06-10): the body-summary prove
helpers `semantic_callable_summary_proves_no_drop_resource`,
`_no_spawn_task`, `_no_send_channel`, and `_no_zone_requirement` join
the existing `_no_ref_escape` helper as the named read seam for
consumers; the first consumer migration applies the channel-send seam
to `semantic_callable_param_escape_summary`;
`semantic-core-shape-test-smoke` gates the helpers + bits + internal
header declarations.

Cross-lane CI (2026-06-10): Windows LLVM-ready CI runs
`air-strict-backend-compare-test-smoke`; macOS C-only CI gracefully
SKIPs `backend_output_tri_compare_parity` when pgy lacks the LLVM
backend, instead of failing the entire `self-host-preparation-test-smoke`
step.

The five closure targets are:

- CFG/body safety source-of-truth: ownership, cleanup, drop, zone/effect body
  facts must be consumed from CFG/MIR facts rather than AST/helper fallbacks.
- AIR abstraction-boundary verification: EvidenceNode and `pgy.air.graph.v1`
  must be the stable verifier surface for boundary drift.
- DAG recursive compatibility seam removal: semantic decisions must use the
  graph/materialized metadata path instead of recursive resolver compatibility.
- MIR/LLVM declaration bootstrap parity: frozen subset declaration inventory
  must be MIR/DIR/RIR-owned rather than AST-carried metadata.
- ABI/Slot/Pin ownership freeze: Slot/Pin/Zone-bound handle, raw escape, and
  runtime-none policy must be documented, smoked, and backend-stable.

MIR declaration-field inventory tightening (2026-05-31): declaration headers
now carry validated `MIRDeclField` rows, and backend consumers are moving from
field compatibility views to that metadata as the first source of truth. Current
closed consumers include C nominal/member lookup, C class constructor
  positional field emission through `TranspilerHostedFieldView`, C
  class/generic-class field emission, C projection
  literal source/target field iteration, C projection invalidation target-field
  matching, C projection field-path relevance/vessel checks, C relation/effect/
  world/zone struct shared-field declaration emission, C relation/effect/zone/
  world constructor shared-field argument/default emission, LLVM constructor
  class-field expected-type/channel checks through `LLVMHostedFieldView`, LLVM
  domain struct shared-field type/layout registration, LLVM domain
  declaration-parts cleanup, LLVM nominal struct field registration, LLVM
projection/domain-projection source-path field iteration through
`LLVMHostedFieldView`, C nominal zone member lookup and zone struct layer-slot
emission through `TranspilerHostedZoneLayerSlotView`, LLVM zone struct layer-slot type
registration, layer-slot field registration, zone bind layer-slot lookup, and
zone-layer query lookup through
`LLVMHostedZoneLayerSlotView`, LLVM zone frontier previous-state/reset/
continue tracking, LLVM zone sync action-cause layer iteration, LLVM zone
action effect-layer emission, LLVM world embedded effect sync layer iteration,
and LLVM intent effect caused-layer emission through
`LLVMHostedZoneLayerSlotView`, C overlay zone-field presence checks through
`TranspilerHostedZoneLayerSlotView`, C overlay zone effect/relation bind lookup
through the same hosted view, C/LLVM constructor
Channel guards, and LLVM current
field-class lookup, C overlay/world shared-field presence checks, and C
projection literal/source-path plus overlay-projection invalidation class-field
iteration through `TranspilerHostedFieldView`. The MIR declaration-inventory
smoke now also globally confines direct class/shared compatibility-view calls
to `host_decl_compat.c`, `transpiler_decl_lookup.c`, and
`llvm_inventory_decl_lookup.c`, and confines direct
`pgy_host_class_field_compat_find(...)` calls to `host_decl_compat.c`, so new
backend consumers must use hosted field views instead of reopening
compatibility views. Remaining declaration-side work is broader declaration/
projection emitter coverage, not metadata creation.

Current DAG intent-zone owner tightening (2026-05-25): intent step `where`,
derived `using`, participant transfer-source checks, and transfer `from`/`to`
zone declaration recovery now consume the intent type owner seam instead of
re-opening `AST_ZONE_DECL` lookup in each consumer. Intent step `causes` effect
declaration recovery uses the same owner seam rather than re-opening
`AST_EFFECT_DECL` in the step validator. `type-resolution-resolver-inventory-test-smoke`
rejects direct `AST_ZONE_DECL` / `AST_EFFECT_DECL` recovery in those consumers,
while
`type-resolution-dag-test-smoke` and `intent-compression-contract-test-smoke`
keep the DAG statistics and compressed intent provenance stable. Remaining
DAG debt is still
semantic-consumer coverage, especially zone authority/generic provenance and
module/visibility fact consumption.

Current domain contract lookup tightening (2026-05-25): action contract
`within` / `causes` validation consumes the semantic domain lookup seam
(`semantic_find_zone_decl_by_name(...)` /
`semantic_find_effect_decl_by_name(...)`) instead of calling
`find_domain_decl_by_name(...)` locally. Zone relation/effect contract
validation now uses the same owner seam through
`semantic_find_relation_decl_by_name(...)` and
`semantic_find_effect_decl_by_name(...)`. World declaration, world helper, and
world embedding consumers also use `semantic_find_zone_decl_by_name(...)` /
`semantic_find_world_decl_by_name(...)` instead of direct domain lookup. Zone
layer-slot relation/effect validation in the authority owner also consumes the
semantic relation/effect seams. The resolver-inventory smoke rejects direct
domain declaration lookup returning to `type_checker_func_action_contract.c`,
`type_checker_domain_contracts.c`, the world semantic consumers, or the zone
authority consumer.

Completed backend/declaration evidence was moved out of this active checklist.
See `docs/133_beta_completed_closure_archive.md` for the 2026-05-29 backend
parity refresh, executable wrapper tightening, Channel constructor/type-family
classifier closure, and hosted declaration compatibility closure. Active
blockers remain unchanged: CFG/AIR consumer completeness, dedicated declaration
IR, and ABI/Slot/Pin freeze.

Current CFG body-flow tightening (2026-05-21): direct parallel slot
`Read` / `Write` / `Release` conflicts now flow through CFG resource
snapshots instead of the AST-only slot analyzer. Slot operations mark
`slot_flow_access_mask`, snapshots preserve `access_masks`, and the parallel
join emits `PGY_SEM_PARALLEL_SLOT_CONFLICT` / `PGY_SEM_PARALLEL_SLOT_RACE_RISK`
from `resource_snapshot_has_parallel_conflict` and
`resource_snapshot_has_parallel_race_risk`. `scope_release_slot(...)` marks
`PGY_SLOT_FLOW_ACCESS_RELEASE` before mutating slot state, so Move /
DeviceSlot-release style helper paths cannot release without a CFG access fact.
`slot_analyzer.c` remains a named
pre-CFG compatibility seam for conservative escape/helper provenance, not the
final body-safety source of truth. Gates: `test-semantic` (`2551/0`),
`test-transpile` (`838/0`), `cfg-body-dataflow-test-smoke`,
`semantic-core-shape-test-smoke`, `parallel-core-contract-test-smoke`,
`perf-contract-test-smoke`, `source-utf8-test-smoke`, and
`build-source-inventory-test-smoke`.

Current AIR evidence provenance tightening (2026-05-25): EvidenceNode creation
now rejects empty provider/subject provenance and zero-fact evidence before an
invalid proof fact can enter the inventory. Validation still rejects malformed
evidence, but the creation owner now enforces the same provenance/fact invariant
earlier. Local verification: `test-air` (`118/0`) and `air-drift-test-smoke`.

Current AIR evidence-kind tightening (2026-05-21): evidence-kind metadata is
now fail-closed. `kEvidenceKindMeta` carries an explicit `present` bit, so a
new `AIR_EVIDENCE_*` enum member is not treated as valid unless its boundary /
global-validator policy is deliberately initialized. This keeps AIR EvidenceNode
inventory from accepting silent default metadata while AIR is being promoted to
the abstraction-boundary verifier. Gate: `air-drift-test-smoke`.

Current smoke portability tightening (2026-05-25): Python is not a beta
runtime or CI dependency. Mandatory smokes must keep a non-Python fallback or
fail when explicitly supplied inputs are unavailable. `diagnostics-json-test-
smoke` and `diagnostic-registry-test-smoke` have shell fallbacks, and
`air-json-schema-test-smoke` now fails instead of skipping when an explicit
`PGY_BIN` is missing. CFG body-dataflow, example, tooling, observability,
memory/concurrency, codegen determinism, runtime panic ABI/codegen, perf
baseline, and LLVM campaign smokes now follow the same rule: if `PGY_BIN` /
`PGY_LSP_BIN` or a required toolchain is explicit, the gate fails when the
binary/toolchain is missing or cannot launch; source-only fallback is allowed
only for default missing binaries. Windows-bash executable smokes must set
runtime DLL paths and pass compiler inputs through
`pgy_path_for_compiler(...)`, and the DnD LLVM campaign smoke writes temporary
stdout files instead of passing large backend output through Python argv.
Runtime-none scanning also now walks every array/tuple literal element instead
of returning success after the first clean element, and source-level
`pin ... as ... { ... }` is rejected as a runtime-dependent surface because it
requires pin/unpin cleanup lowering. Gates:
`runtime-none-contract-test-smoke`, `air-json-schema-test-smoke`,
`cfg-body-dataflow-test-smoke`, `example-test-smoke`,
`tooling-conformance-test-smoke`,
`observability-schema-test-smoke`, `memory-concurrency-model-test-smoke`,
`codegen-determinism-test-smoke`,
`runtime-panic-abi-test-smoke`,
`runtime-panic-codegen-test-smoke`,
`perf-c-baseline-test-smoke`,
`llvm-campaign-projection-test-smoke`, and `llvm-dnd-campaign-test-smoke`.

Current beta surface smoke portability (2026-05-25): formatter, module,
package-module resolver, and stdlib surface smokes now also use the shared
Windows/MSYS path helper. Local gates validate fmt idempotence, module import
behavior, package scaffold/error JSON, and stdlib C/LLVM parity through the same
compiler path contract as the backend smokes.
IR pipeline and Unicode policy smokes now follow the same contract; explicit
compiler binaries fail closed, and the logistics IR probe plus C/LLVM UTF-8
string policy run through converted paths.
`build-source-inventory-test-smoke` now gates the helper requirement for these
beta executable smoke scripts, preventing future Windows path regressions from
landing silently. Tooling conformance also converts the debugger source path;
smokes that only call `compare_backends.sh` are treated as delegated path
conversion rather than raw compiler invocation.

Current raw escape documentation tightening (2026-05-25): security-mode fast
path examples no longer present `unsafe { *slot.get_ptr() = ... }` as a stable
Slot performance model. The stable hot path is typed Pin/Lease
(`pin slot as view: WriteView<T> { ... }`), while system-tier raw escape remains
reserved behind future scoped unsafe capability evidence. Gate:
`raw-escape-contract-test-smoke`.

Current C backend declaration-lookup tightening (2026-05-25): intent step zone
binding, caused-effect zone lookup, world-zone projection resolution, and world
frontier zone lookup now consume active inventory / program-view seams instead
of reopening direct zone declaration lookup at the use site. Gate:
`mir-declaration-inventory-test-smoke`; sanity: `test-transpile` (`838/0`).
The same slice now covers world embedded method-call context, overlay projection
invalidation, zone effect bind, and zone relation bind for zone/effect/relation
declaration recovery.
C backend MIR SSA host recovery and function forward policy now consume active
inventory for zone/world declaration recovery as well, keeping residual SSA
name rendering and prototype policy on the same declaration source-of-truth
seam.

Current DAG tightening (2026-05-15): Generic parameter storage is now closed
behind parser-owned accessors for the main semantic contract path. Generic
support/contracts, default validation, ability ref/match/where diagnostics,
declaration generic-scope setup, constructed metadata materialization, and
function call where-bound validation consume `ast_generic_param_*` accessors.
Module ability-contract validation, intent require-field generic scope
resolution, DAG graph generic-contract collection, DAG stage nominal/signature
replay, and metadata/dead-end/diagnostic owners are included in the same guard.
Late callable generic effective-type derivation now consumes the same
accessors. Role include type-arg precollection and ownership ClaimSlot
generic-arg resolution are also closed. Compiler/codegen consumers for
DIR/HIR/MIR type rendering, module normalization, LLVM type
rendering/registry/forward declaration/pipeline, LLVM Slot/collection/resource
let lowering, LLVM expression type inference, spawn generic substitution, C
generic class specialization, C role ability specialization, and C
forward-declare policy are now on the same read-only seam.
`type-resolution-resolver-inventory-test-smoke` now has broad semantic plus
compiler/codegen guards rejecting direct `GenericParams` storage reopenings
(`->params[...]`, `->default_type`, `->constraint`, and direct
`ast_type_generic_args(...)->count/params`); `type-resolution-dag-test-smoke`
still reports zero metadata dead-ends after the slice, and the retired resolver
surface is now a quarantine sentinel instead of a zero-counter telemetry stream.

Current DAG diagnostic-owner tightening (2026-05-21): expression member
access, hosted-field lookup, and overlay world-zone binding now use
`semantic_type_resolution_lookup_metadata_name_or_alias_or_unknown(...)` for
metadata name/alias lookup plus unknown-type diagnostics. The duplicated local
helpers in expression/host/overlay owners were removed, so unknown named type
resolution remains metadata-owned and cannot drift across owner-local
compatibility seams. Local gate: `type-resolution-resolver-inventory-test-smoke`,
`type-resolution-dag-test-smoke`, `build-source-inventory-test-smoke`, and
`test-semantic` (`2551/0`; DAG stats include `metadata_dead_ends=0`,
`metadata_hits=8771`). The resolver inventory smoke
also rejects reintroducing the previous expression/host/overlay local helper
names.

Current parser owner cleanup (2026-05-15): declaration-name mutation, let,
scalar literal, identifier, extern/use/import/namespace, type-alias, and event
accessors now live in `src/parser/ast_decl_accessors.c`. The domain accessor
owner is reduced to the intent/class/enum slice, and `build-source-inventory`
plus parser smoke confirm the new owner is part of the build inventory rather
than a loose split artifact.

Current semantic owner cleanup (2026-05-15): overlay nominal type creation and
overlay field count/type lookup now live in
`src/semantic/type_checker_host_overlay.c`. `type_checker_host_helpers.c` is
reduced to host-boundary lookup, subject-slot/authority checks, and
movable-resource helper facts; `test_semantic`, `semantic_core_shape_smoke`,
`build-source-inventory`, and `test_inc_size_smoke` gate the split.

Current C backend owner cleanup (2026-05-15): `Option<T>` let constructors and
`HashMap<String,T>` / `List<T>` / `Queue<T>` constructor lowering now live in
`src/codegen/transpiler_let_collection_emit.h`. `transpiler_let_emit.c` is back
to the let orchestration path, while collection constructor dispatch has a
responsibility-named owner. `test_transpile`, source inventory, and owner-size
smokes gate the split.

Current C backend implementation-header cleanup (2026-05-19): generated
Result/collection/tuple specialization registry logic now lives in
`src/codegen/transpiler_specialization_registry.c`. The header is
declaration-only, and AST statement scanning is isolated in
`src/codegen/transpiler_specialization_scan.c`. Result suffix parsing and `Result<T,E>` specialization
discovery now live in `src/codegen/transpiler_type_result_mapping_helpers.c`
instead of an implementation header. HashMap stdlib builtin dispatch and
lowering now live in `src/codegen/transpiler_expr_stdlib_map_builtin.c`, with
HashMap metadata validation owned by the shared collection support owner. The
Queue stdlib builtin follows the same policy in
`src/codegen/transpiler_expr_stdlib_queue_builtin.c`, with unary collection
metadata validation also owned by collection support. Result/Option builtin
dispatch and lowering moved to
`src/codegen/transpiler_call_result_option_builtin_emit.c`, and
`src/codegen/transpiler_option_context.h` now provides the narrow Option context
declarations needed by linked owners. Intent observability builtin lowering now
lives in `src/codegen/transpiler_intent_observability_builtin_emit.c`, while the
header is declaration-only. Projection/world lookup seams also moved into the
compiled projection owner: `src/codegen/transpiler_projection.c` owns overlay
domain-slot lookup, projection-target detection, and world-state lookup, and
`build-source-inventory-test-smoke` rejects the old implementation-header local
helper names. Domain query builtins (`HasProjection`, `HasLayer`, `HasState`,
`HasZone`, `HasZoneProjection`, `HasZoneLayer`, and `HasZoneState`) now lower
through `src/codegen/transpiler_expr_domain_query_builtin.c`, leaving
`transpiler_expr_builtin_dispatch.h` as builtin-family routing rather than a
mixed zone/world/projection lowering body. I/O and time builtins (`FileOpen`,
`FileExists`, `FileRead`, `FileWrite`, `FileClose`, `ReadFile`, `WriteFile`,
`Input`, `Print`, `ReadLine`, `Now`, and `Sleep`) now lower through
`src/codegen/transpiler_expr_io_builtin.c` for the same reason. Domain
constructor bodies now live in
`src/codegen/transpiler_domain_constructor_emit.c`: class compound literals,
party/roster/relation/effect/zone/world designated initializers, projection
dirty defaults, world dirty defaults, and enum variant constructor call strings
are no longer embedded in `transpiler_call_constructor_result_emit.h`; the
remaining dispatch wrapper now lives in `transpiler_call_constructor_result_emit.c`
and the header is declaration-only. Generic class specialization lookup/ensure
now lives in `src/codegen/transpiler_generic_class_specialization_emit.c`;
`transpiler_generic_class_specialization_emit.h` is declaration-only and
`transpiler_func_class_flow_emit.h` no longer injects specialization bodies
through include order. Function declaration fallback emission, with-slot
lowering, and return lowering now live in
`src/codegen/transpiler_func_class_flow_emit.c`, so
`transpiler_func_class_flow_emit.h` is also declaration-only. Expression core,
composite literal, and array access
lowering also moved into linked owners:
`src/codegen/transpiler_expr_core_emit.c` owns binary/operator, coalescing, and
checked div/mod lowering; `src/codegen/transpiler_expr_composite_literal_emit.c`
owns tuple and Array literal lowering; and
`src/codegen/transpiler_expr_array_access_emit.c` owns Array/Slice checked
access lowering. `src/codegen/transpiler_let_channel_emit.c` owns Channel let
lowering and channel metadata registration.
`src/codegen/transpiler_future_type_query.c` owns spawn/Future/RemoteFuture type
queries that were previously static forward-helper bodies, and
`src/codegen/transpiler_let_type_register_emit.c` owns post-let type
registration.
`src/codegen/transpiler_let_box_emit.c` owns `Box<T>`, `Box<Array<T>>`, and
`Rc<T>` let-constructor lowering.
`src/codegen/transpiler_let_collection_emit.c` now owns `Option<T>` `Some`/`None`
let lowering plus stable `HashMap<String,T>`, `List<T>`, and `Queue<T>`
constructor lowering. `src/codegen/transpiler_zone_specialization_emit.c` is
also source-inventory linked for required zone specialization discovery. Their
headers are declaration-only. The redundant `transpiler_mir_emit_predicates.h`
wrapper header is deleted; C function/intent emitters call the canonical
`*_with_reason(...)` MIR contract APIs directly. `pergyra_ast_type_to_c_copy(...)`
now lives in `src/codegen/transpiler_type_render.c`, so shared AST type-to-C
copy ownership matches the public type-render API instead of the forward-helper
include. `src/codegen/transpiler_expr_builtin_dispatch.c` now owns the
`BuiltinKind` routing switch for expression builtins instead of carrying that
body in the expression-emitter include chain.
`src/codegen/transpiler_control_flow_emit.c` now owns C `if`/`for`/
`while` lowering, loop-label lookup, and the condition-head formatter shared
with MIR branch terminator emission; its header is declaration-only. The split
also moved MIR CFG control rendering to
`src/codegen/transpiler_mir_cfg_control_emit.c`, leaving the MIR CFG-control
header declaration-only for loop init, for-in binding, backedge increment,
branch-condition rendering, and select readiness rendering. The split
also removed a hidden
transitive include seam: Result/Option calls, role/ability dispatch, let
lowering, MIR match conditions, MIR preserved lets, domain nominal/role
emitters, and statement dispatch now include the type-mapping,
collection-support, Option-context, intent-observability, or role/ability
declarations they consume directly. Gates: `test-transpile` (`770/0`),
`perf-contract-test-smoke`,
`runtime-panic-contract-test-smoke`, `build-source-inventory-test-smoke`,
`semantic-core-shape-test-smoke`, `test-inc-size-test-smoke`, and
`source-utf8-test-smoke`.

Current C type mapping cleanup (2026-05-15): constructed type argument parsing,
inner-type extraction, suffix sanitization, and capped string copy helpers now
live in `src/codegen/transpiler_type_name_utils.c`. `transpiler_type_mapping.c`
is reduced to mapping policy, which narrows the later ABI/type-layout-first
mapping pass.

Current LLVM task/channel owner cleanup (2026-05-15): task-runtime builtins
(`Cancel`, `IsCancelled`) now live in `src/codegen/llvm_expr_task_calls.c`.
`src/codegen/llvm_expr_task_channel_calls.c` is reduced to `Channel<T>`
metadata, send/receive, timeout, close, and query lowering, keeping task
cancellation separate from channel dispatch before deeper LLVM parity work.

Current LLVM MIR CFG owner cleanup (2026-05-15): match subject discovery and
Option/Result destructor-pattern condition lowering now live in
`src/codegen/llvm_mir_match_condition.c`. `llvm_mir_cfg_control.c` is reduced to
CFG-container classification, select readiness, and channel receive DEF
lowering, so match semantics no longer share the channel/select control owner.
2026-05-16 follow-up: the match-condition owner now includes `llvm_internal.h`
instead of the declaration-only private API, so it compiles as a normal codegen
translation unit with complete `ASTNode`, `LLVMGenCtx`, and LLVM-C types.
Gate: `LLVM_ENABLED=1 pgy`.

Current MIR declaration inventory tightening (2026-05-16): role hosted-method
metadata no longer has a method-count validation exception.
`ast_role_impl_method_total_count(...)` is the shared parser-owned count seam
for role impl-ability methods, and the MIR declaration-header validator plus
C/LLVM hosted-method views consume that same accessor. Role `method_count` and
`method_metadata_count` must match the AST compatibility count, so missing role
declaration metadata fails as a MIR-inventory error instead of silently yielding
an empty role method view. Gates: `test-mir`,
`mir-declaration-inventory-test-smoke`, `perf-contract-test-smoke`, and
`LLVM_ENABLED=1 pgy`.

Current MIR declaration-field inventory tightening (2026-05-31):
`MIRDeclHeader` now records validated `MIRDeclField` metadata for class fields,
domain shared fields, party role slots, roster slots, world roster/zone slots,
domain slots, and zone layer slots. This closes the metadata-creation slice of
the dedicated declaration IR row. Current consumers include C nominal/current
member type lookup, C class constructor positional field emission through
  `TranspilerHostedFieldView`, C class/generic-class field emission, C projection literal source/target field
  iteration, C projection invalidation target-field matching, C projection
  field-path relevance/vessel checks, C relation/effect/world/zone struct
  shared-field declaration emission, C relation/effect/zone/world constructor
  shared-field argument/default emission, LLVM domain struct shared-field
  type/layout registration, LLVM domain declaration-parts cleanup, LLVM
  projection/domain-projection
source-path field iteration, C/LLVM constructor Channel guards, LLVM
constructor class-field expected-type/channel checks through
`LLVMHostedFieldView`, LLVM projection/domain-projection source-path field
iteration through `LLVMHostedFieldView`, LLVM nominal struct field registration
through `LLVMHostedFieldView`, LLVM constructor shared-field defaults, and
LLVM current field-class lookup. The C constructor Channel guard, MIR SSA
implicit zone layer/shared-field recovery, projection zone-layer lookup,
projection sync layer iteration, intent block caused-effect layer marking, and
zone sync/frontier layer iteration now scan layer slots through
`TranspilerHostedZoneLayerSlotView` and shared fields through
`TranspilerHostedSharedFieldView`; C/LLVM zone frontier pass-limit emission uses the
counted frontier policy wrapper instead of recounting zone layer slots. LLVM constructor
Channel/default paths scan shared fields through `LLVMHostedSharedFieldView`
instead of reopening shared-field compatibility directly. C overlay/world
shared-field presence checks use the same `TranspilerHostedSharedFieldView`
path and fail closed when required MIR metadata is missing. C projection
literal/source-path field iteration uses `TranspilerHostedFieldView` instead of
reopening class-field compatibility locally, and overlay-projection
invalidation now uses the same hosted field view. C class constructor
positional field emission in `transpiler_domain_constructor_emit.c` also uses
the hosted field view and fails closed when required MIR metadata is missing.
Annotated let class-constructor lowering now delegates to that same constructor
owner instead of reopening class-field compatibility locally. Broader
declaration/projection emitters still need to migrate from `host_decl_compat.c`
compatibility views before the row can be marked closed.
Gates: `test-mir`, `test-transpile`, `LLVM_ENABLED=1 pgy`, and
`mir-declaration-inventory-test-smoke`.

Current MIR surface-usage tightening (2026-05-16):
`mir_inventory_surface_usage_summary(...)` is now the single inventory summary
seam for thread-pool and intent-observability usage. MIR lowering records both
bits from that summary, and MIR validation recomputes the same summary once
instead of independently walking inventory for each usage bit. Gates:
`test-mir`, `perf-contract-test-smoke`, `parallel-core-contract-test-smoke`,
`build-source-inventory-test-smoke`, and `source-utf8-test-smoke`.

Current LLVM statement owner cleanup (2026-05-15): select statement readiness
and round-robin lowering now lives in `src/codegen/llvm_stmt_select.c`.
`llvm_stmt_parallel_async.c` keeps parallel and async wrapper emission, so
select policy no longer shares the parallel/async owner.

Current LLVM collection owner cleanup (2026-05-15): extended collection call
diagnostic recovery now goes through the collection-require owner instead of
living in the mixed extended-call body. `llvm_expr_call_collections_extended.c`
is reduced to extended collection call lowering and can be split by container
family later without duplicating diagnostic strings.

Current LLVM List/Map collection split (2026-05-15): `ListPush`, `ListGet`,
`ListSet`, `ListSize`, and `ListRemove` lowering now live in
`src/codegen/llvm_expr_call_list_extended.c`. The mixed extended collection
owner delegates queue and List first, then owns HashMap extended calls, keeping
List and Map policy from growing in one owner.

Current C expression owner cleanup (2026-05-15): scalar literal expression
emission now lives in `src/codegen/transpiler_expr_literal_emit.h`.
`transpiler_expr_dispatch_emit.c` dispatches literals through that owner and
keeps the remaining dispatcher focused on expression-form routing.

Current C composite literal owner cleanup (2026-05-15): tuple and Array literal
emission now lives in `src/codegen/transpiler_expr_composite_literal_emit.h`.
The expression dispatcher no longer owns tuple layout name construction or
Array builder emission directly; `test_transpile` gates the split.

Current runtime roster owner cleanup (2026-05-15): roster/world lookup APIs now
live in `src/runtime/world_roster_lookup.c`. `world_roster.c` is reduced to
roster/world lifecycle and mutation, while `RosterFindParty`, `WorldFindRoster`,
and `WorldFindParty` share one lookup owner.

Current runtime collection owner cleanup (2026-05-15): the generic List macro
now lives in `src/runtime/pgy_runtime_list_generic_inline.h`, leaving
`pgy_runtime_list_set_inline.h` focused on concrete List/Set instantiations and
Set macro policy.

Current runtime async owner cleanup (2026-05-15): `AsyncScopeParallelFor` and
`AsyncScopeRace` now live in `src/runtime/async/async_scope_patterns.c`.
`async_scope.c` keeps scope lifecycle, spawn, wait, cancellation, and error
state ownership without carrying higher-level pattern helpers.

Current runtime scheduler owner cleanup (2026-05-15): spawn/enqueue/yield/
block/unblock/steal operations now live in
`src/runtime/async/scheduler_fiber_ops.c`. `scheduler.c` keeps scheduler
lifecycle, worker startup/shutdown, I/O worker bootstrap, and thread-local
current-scheduler ownership.

Current MIR statement population cleanup (2026-05-15): source statement index
tagging and source-inventory count/items accessors now live in
`src/compiler/mir_stmt_source_inventory.c`. `mir_stmt_population.c` keeps the
CFG source-order reconstruction algorithm and local inventory lookup, with
`test_mir` gating the split.

Current module normalizer owner cleanup (2026-05-15): domain/world/zone/
relation/effect reference traversal now lives in
`src/compiler/module_normalizer_domain_refs.c`. `module_normalizer_refs.c` keeps
general declaration/expression reference normalization, while domain graph
normalization has its own owner.

Current LLVM collection owner cleanup (2026-05-15): extended collection
diagnostics now live with collection-require helpers in
`src/codegen/llvm_expr_call_collections_require.c`. The List/HashMap dispatcher
is smaller and has a shared diagnostic seam ready for a later List-vs-Map split.

Current C expression owner cleanup (2026-05-15): number/string/bool literal
rendering now lives in `src/codegen/transpiler_expr_literal_emit.h`.
`transpiler_expr_dispatch_emit.c` is reduced to expression-family dispatch and
non-literal lowering while preserving `test_transpile` behavior.

Current runtime owner cleanup (2026-05-15): world/roster lookup APIs now live in
`src/runtime/world_roster_lookup.c`. `world_roster.c` keeps creation, execution,
async wait, frame loop, and cleanup behavior, while `RosterFindParty`,
`WorldFindRoster`, and `WorldFindParty` have a separate lookup owner.
Party scheduler registry and debug dump behavior now lives in
`src/runtime/party_runtime_scheduler.c`; `party_runtime.c` keeps fiber-map
generation and context role/shared lookup behavior.

Current runtime collection owner cleanup (2026-05-15): generic List macro
generation now lives in `src/runtime/pgy_runtime_list_generic_inline.h`.
`pgy_runtime_list_set_inline.h` keeps concrete List/String and Set bodies, while
`PGY_LIST_DEFINE(...)` has a separate inline owner.

Beta closure asks one practical question: **can the core survive a one-year
freeze while dogfood starts?** If AIR/CFG/runtime invariants are still
incomplete, documentation alone does not count as closure.

마지막 업데이트: 2026-05-25

이 문서는 베타 진입 전 반드시 닫아야 하는 실행 체크리스트다. 기준은 기능 개수가 아니라 **surface trust + 구조 지속 가능성 + C/LLVM parity + CFG-backed body safety + AIR-backed abstraction safety + dogfood-first path**다. 현재 표기는 두 개로 분리한다: 기능 체감 진행도는 약 85%, strict beta readiness는 약 83%다. 75% 표기는 현재 구현 표면보다 낮게 잡힌 과거 anchor로 본다. 다만 beta-complete나 90%대 준비도는 아니다. CFG/AIR의 마지막 consumer-completeness, MIR-owned resource/declaration 경로의 호환 fallback 축소, MIR/LLVM declaration bootstrap, ABI/Slot/Pin freeze를 현재 full-suite evidence로 다시 고정해야 한다.

베타 진입 목표는 1년간 코어 문법과 의미론을 멈추고 생태계(`pgy.compat.*`, `pgy.kit.*`, `pgy.std.*`, `pgy.accel.spray`, `pgy.render.skia` 등)를 분리해도 되는 지점을 만드는 것이다. 따라서 beta closure는 **"이 코어가 1년 동안 자력으로 버틸 수 있는가"**를 기준으로 본다. 새 표면을 늘리는 작업은 AIR/CFG/runtime invariant가 닫힌 뒤로 미루며, 문서 합의만으로 완료된 것으로 보지 않는다.

Operational mode:

- 2026-05-03 priority reset:
  finish beta blockers in this order: (1) CFG/MIR fact contracts, (2) AIR
  evidence consumption for abstraction boundaries, (3) DAG source-of-truth seams,
  (4) LLVM declaration inventory bootstrap, (5) runtime frontier scheduler, and
  (6) ABI ownership/runtime-none/raw-escape contracts. Dogfood/WebGL remains the
  first beta use path after these contracts are stable; self-hosting stays beta+.
  AIR evidence inventory now rejects stale legacy boundary summaries: when
  `evidence_count > 0`, `has_hir_*` / `has_rir_*` boundary summary flags must
  be backed by matching `AIREvidenceNode` entries before drift checking. For
  real HIR/RIR input, boundary evidence nodes must also have matching summary
  flags, keeping inventory and cached summaries bidirectionally consistent.
  MIR cleanup, terminator, select-receive, and pin-cleanup summary counters are
  now treated the same way: strict AIR accepts them only as observability
  summaries, and the proof must be a matching `AIREvidenceNode` inventory entry.
  Cleanup/terminator/select-receive use global evidence nodes; pin cleanup uses
  boundary-scoped evidence nodes. `test_air` locks the counter-only and
  counter-drift negative cases, and `air-drift-test-smoke` source-gates the
  shared diagnostic wording.
  DAG's remaining unresolved metadata path is now explicitly named as a
  metadata dead-end diagnostic instead of a materializer fallback recorder; the
  fallback counters remain as regression evidence, but recursive materialization
  is not presented as a valid owner seam. Developer tracing uses
  `PGY_TYPE_RES_DEAD_END_TRACE` / `[type-res-dead-end]` for the same reason.
  C/LLVM hosted-method declaration views now name their AST-side method arrays
  `ast_compat_methods` / `ast_compat_count`, making the remaining declaration
  bootstrap compatibility seam explicit instead of presenting it as a generic
  fallback path. C backend class/enum/generic method-body emission also consumes
  `MIRDeclMethod` name/routine helper accessors first, so AST compatibility no
  longer owns hosted-method routine identity discovery on that path. C backend
  routine iteration now goes through `TranspilerMIRRoutineInventory`, aligning
  thread-pool, intent-observability, function/intent/method lookup, and
  view-resource scans with the same helper-gated source-of-truth discipline.
  C/LLVM hosted-method views now also reject MIR metadata count drift against
  the AST compatibility count instead of silently truncating or extending hosted
  method iteration. LLVM hosted domain method body emission now consumes only
  linked `MIRDeclMethod.routine_index` metadata; AST/name-based MIR routine
  search is no longer allowed on that path and is smoke-gated by
  `mir-declaration-inventory-test-smoke`. Role implementation methods are now
  materialized into `MIRDeclHeader` metadata as well, so role/ability emission
  and intent role calls no longer need an owner/name routine scan after the
  linked metadata pass. The gate now checks role declaration metadata recording
  and rejects reintroducing AST-identity or owner/name routine fallbacks. C
  hosted method emission no longer calls a generic method lookup after reading a
  hosted-method view, and role method bodies now consume the same
  `TranspilerHostedMethodView -> MIRDeclMethod -> MIRRoutine` path instead of
  the retired owner/name role lookup helper.
  Type-alias and event declaration metadata are now on the same parser-owned
  accessor boundary as nominal/domain declarations: DAG metadata/stage
  resolution, DIR/HIR naming, runtime-none scans, C stdlib/specialization/event
  emission, and LLVM type/event lookup consume `ast_type_alias_*` and
  `ast_event_*` accessors. Namespace prefix rewriting now uses
  `ast_declaration_name(...)` / `ast_replace_declaration_name_copy(...)`, so
  `module_normalizer.c` no longer owns a raw declaration-name slot exception.
  `semantic-core-shape-test-smoke` rejects semantic/compiler/codegen payload
  reads for these declaration names and metadata fields. Extern ABI labels and
  declaration lists are now consumed through `ast_extern_block_abi(...)`,
  `ast_extern_block_declarations(...)`, and
  `ast_extern_block_declaration(...)` as part of the same boundary.
  Function declaration names are also closed across semantic, compiler, C
  backend, and LLVM consumers through `ast_declaration_name(...)`; the only raw
  `data.func_decl.name` access left is parser-owned destruction in
  `src/compiler/hir_destroy.c`. Parameter, return, and body payloads remain
  separate closure work. The parser-owned seam for that next slice now exists:
  `ast_func_param_count(...)`, `ast_func_params(...)`, `ast_func_param(...)`,
  `ast_func_generic_params(...)`, `ast_func_where_clause(...)`,
  `ast_func_return_type(...)`, and `ast_func_body(...)` are built as
  `src/parser/ast_func_accessors.c`, with semantic call-contract,
  async spawn, ability declaration, generic where-call, host/operator method,
  action-contract, compact-intent `on` inference, ownership param-summary,
  effect/host helper, program prepass, module-normalizer/runtime-none scans,
  slot analyzer, HIR/RIR signature/body, DAG precollect/stage signatures,
  MIR non-CFG/type-helper, MIR declaration header/validation, C forward
  declarations, C MIR function/local emission, C specialization/type-inference
  helpers, and LLVM declaration/domain forward, boundary projection, callable
  variable, extern registration, member/spawn calls, return typing,
  let-callable, type inference, and MIR function emission consumers moved onto
  it. The global smoke now rejects raw function signature/body payload reads in
  semantic/codegen, rejects function generic/where payload reads across
  semantic/compiler/codegen, and rejects compiler reads outside parser-owned
  HIR construction/destruction.
  Async function metadata uses the same parser-owned seam via
  `ast_async_func_name(...)`, `ast_async_func_param_count(...)`,
  `ast_async_func_params(...)`, `ast_async_func_param(...)`, and
  `ast_async_func_body(...)` for the existing `AST_FUNC_DECL + is_async_decl`
  shape. Slot analyzer summaries and async channel spawn checks consume those
  accessors, and the semantic core shape smoke rejects raw
  `data.async_func_decl.*` metadata reads in semantic/compiler/codegen.
  Event-handler function-pointer type metadata is also behind parser-owned
  accessors: `ast_event_handler_param_count(...)`,
  `ast_event_handler_param_types(...)`, `ast_event_handler_param_type(...)`,
  and `ast_event_handler_return_type(...)`. DAG type-reference collection,
  constructed-type metadata materialization, C declarator/signature rendering,
  C specialization scans, MIR signature eligibility, and LLVM callable/type
  lowering consume those accessors; the shape smoke rejects raw
  `data.event_handler_type.*` metadata reads in semantic/compiler/codegen.
  Ability contract type-reference helpers now consume `ast_type_name(...)` and
  `ast_type_generic_args(...)` for ability display, matching, and where-clause
  validation, so generic/ability mismatch provenance cannot depend on raw type
  payload reads in those semantic owners.
  Ability require-field metadata is also parser-owned through
  `ast_require_field_name(...)` and `ast_require_field_type(...)`; role-field
  validation, module normalization, DAG graph precollect, and staged nominal
  resolution consume those accessors, and the semantic core shape smoke rejects
  raw `data.require_field.*` reads in semantic/compiler/codegen.
  LLVM/DIR/MIR type rendering and registry helpers consume `ast_type_name(...)`
  and `ast_type_generic_args(...)` for AST type names and generic arguments. The
  closed slice covers LLVM AST type lowering, early forward-declare eligibility,
  variable type registry, type rendering, boundary slot parameter lowering, DIR
  type rendering, and MIR claim type rendering. It also covers LLVM
  function/domain forward signatures, intent participant/value setup, role target
  lookup, DIR ability edges, RIR type facts, async token-boundary checks,
  projection nested-vessel lookup, and intent role-field generic substitution,
  plus generic contract validation and DAG metadata
  named/alias/constructed/dead-end/diagnostic materialization pure-read paths.
  DAG graph collect/core/domain precollect paths also use that seam for
  type-ref dependency recording and zone-slot target lookup. Function call
  generic where-clause validation, late generic argument inference, semantic role
  target lookup, and intent participant type-name helpers also consume the
  accessor seam. Module ability contract arity checks, let-binding ownership
  annotation checks, and party role-slot ability validation now consume that
  seam. LLVM statement type rendering, let helper inference, Slot/View/MoveToken
  resource lets, collection/channel/array let specializations, and generic let
  post-registration now also read annotated type names/generic args only through
  the accessor seam. LLVM ClaimSlot/DeviceSlot let lowering, expression type
  inference, MIR local type recovery, MIR boundary slot parameter helpers,
  with-slot lowering, zone-action subject slot lookup, and Result return-type
  inference are on the same seam. HIR call/type-reference collection, MIR
  intent participant/where facts, and RIR authority/intent ability facts now
  also consume the accessor seam. LLVM function-call dispatch, identifier slot
  source recovery, spawn generic function lowering, and hosted member-call
  argument coercion now also read parameter/binding types through the accessor
  seam. Intent action redundancy diagnostics, contract-source summaries, LLVM
  intent cleanup/context carriers, and C intent zone binding/sync emission now
  read step `where` and participant subject types through the same accessor
  seam. LLVM/C projection sync provenance and C intent zone-slot lookup now
  also read domain slot types through that seam, with a shape-smoke gate against
  raw type payload reads in those owners. Intent participant action matching,
  DAG staged ability evidence/stats, role generic-bound validation,
  type-constraint formatting, and lightweight type inference now consume the
  same accessor seam. C backend declaration host lookup, type-alias target
  resolution, function forward-declaration policy, and generic function forward
  helper binding inference now also consume that seam. C backend type rendering
  and generic/collection/Result specialization discovery now consume the same
  seam instead of reopening `AST_TYPE` storage. C backend Slot, SecureSlot,
  DeviceSlot, View/MoveToken, Box, BoxArray, and Rc let-specialized owners now
  also consume the accessor seam for annotated type names and generic
  arguments. General C let lowering for generic class specialization,
  collection constructors, projection borrows, and constructor matching now also
  consumes the same accessor seam. LLVM pointer-self checks and C domain
  nominal/role ability vtable, spawn role-call, with-slot, generic class
  specialization, MIR SSA local type, intent participant, and projection helper
  owners are now on that seam as well. `ast_replace_type_name_copy(...)` now
  owns module-normalizer type-name rewrites, and short-lived synthetic type refs
  are created through `ast_create_type(...)`; semantic/compiler/codegen now have
  a global shape gate against raw `data.type.name` and
  `data.type.generic_args` access. Call generic-argument reads now go through
  `ast_call_generic_args(...)`, `ast_call_generic_arg_count(...)`, and
  `ast_call_generic_arg(...)`; semantic/compiler/codegen have a global shape
  gate against raw `data.call.generic_args` consumption. AIR call-boundary,
  traversal, and evidence containment now consume `ast_call_callee(...)`,
  `ast_call_arg_count(...)`, `ast_call_arguments(...)`,
  `ast_call_argument(...)`, and `ast_call_argument_name(...)`, with a shape
  gate against reopening call payloads in semantic/compiler/codegen. This keeps
  reserved named-argument diagnostics on the same parser-owned seam before
  default/named argument implementation is widened. Slot analyzer escape/access
  summary owners now use
  the same call accessor seam for callee and argument traversal. HIR control-flow
  and direct-call analysis now use the same seam for callee/argument traversal.
  MIR source preservation, RIR call naming, module-normalizer reference rewrite,
  runtime-none scanning, MIR call facts, MIR source side-effect shape, and MIR
  SSA identifier-use collection now use the same seam. MIR resource write value
  extraction and RIR projection validation now use the same seam. Semantic async
  spawn-boundary, channel-state builtin, and intent-observability builtin owners
  now consume the same call accessor seam for callee/argument reads. Lambda
  capture rejection now traverses call callee/arguments through the same seam.
  Host method call checking, intent target validation, compressed on-clause
  argument inference, intent control-transfer scans, ClaimSlot ownership
  let/destructure handling, world embedding handoff scans, and DAG body
  type-reference precollection now use the same seam. Ownership let binding
  checks, ownership let helper paths, stdlib variant checks, flow match checks,
  and host traversal helpers now use the same seam.
  C user-call emission and LLVM callable let registration now consume that seam
  as well. C constructor/result/option/domain call emission and C
  member/spawn-style call emission are now gated on the same accessor seam.
  LLVM zone-action sync and C projection-sync helpers now use the same seam for
  member-call callee reads. LLVM event, intent-observability, Rc builtin
  lowering and C allocator builtin lowering now use the same seam for arity and
  argument reads. LLVM domain-query utilities, vtable dispatch, C event builtin
  emission, and C channel let lowering now use the same seam.
  LLVM callable-variable lowering, expression result-type probing, spawn target
  lowering, subject projection, ClaimSlot let lowering, C intent observability
  emission, C ToTObject helper emission, C MIR local type lookup, pending-use
  filtering, and spawn wrapper argument emission are also on that seam.
  C parallel capture discovery, C spawn forward generic-return inference, LLVM
  domain-slice methods, LLVM constructor lowering, and LLVM domain query
  dispatch now use the same seam. C Option-return flow emission, C MIR SSA
  local registration, C slot target resolution, LLVM queue/log builtin
  lowering, LLVM slot-source identifier resolution, and LLVM resource let
  lowering now use the same seam. C MIR destructuring, C overlay projection
  invalidation walks, LLVM collection-base/math builtin lowering, LLVM let
  helper type inference, and LLVM nominal type inference now use the same seam.
  MIR claim ABI type helpers, C Queue builtin emission, C Slot/Pin let
  emission, and LLVM MIR local alloca emission now use the same seam. C
  Rc/Box/array core builtin emission, LLVM Array builtin lowering, LLVM
  Slot/Device builtin lowering, and LLVM let metadata registration now use the
  same seam. C MIR local type lookup, C Box/Rc let lowering, C
  Log/LogRaw/LogBanner lowering, and RIR builder call walking now use the same
  seam.
  Semantic projection/query builtin owners now consume the call accessor seam
  for arity and argument reads. Semantic channel query/send/recv/close builtin
  owner now uses the same seam for channel/value/timeout argument reads. Semantic
  world query builtin owner now uses that seam for world zone/detail argument
  reads. Semantic nominal builtin owner now uses the same seam for nominal/box/string scalar
  builtin arity, callee, and argument reads. Semantic Slot/Pin builtin owners
  now use the same seam for slot/value/token/view argument reads. Semantic
  state-tool builtin owner now uses the same seam for prefix argument reads.
  Semantic stdlib scalar/string/math builtin owner now uses the same seam for
  argument arity and type-checking reads. Semantic stdlib HashMap builtin owner
  now uses the same seam for map/key/value argument reads. Semantic stdlib
  collection and body builtin owners now use the same seam for List/Set/Queue,
  Array, Print/Sleep, device-slot, Clone, ToString, cancellation, and qubit
  state/effect argument reads. Core semantic call
  dispatch now uses the same seam for callee, arity, member-call argument,
  Slice argument, Slot method, and synthetic borrowed-call view construction.
  Array access receiver/index facts now have parser-owned accessors, and
  semantic, AIR, HIR, MIR SSA, module normalization, runtime-none, C, and LLVM
  consumers no longer read `data.array_access.*` directly.
  Member access receiver/name facts now have parser-owned accessors. Semantic
  compiler, and codegen consumers are closed on that seam; member-call,
  projection sync/invalidation, type inference, C dispatch, and LLVM assignment
  paths no longer read `data.member.*` directly.
  Assignment target/value facts now have parser-owned accessors. Semantic,
  compiler/AIR, HIR CFG, MIR SSA/source/call-fact/type-helper, RIR walk,
  runtime-none, module-normalizer, C, and LLVM consumers are closed on that
  seam; the shape smoke now rejects non-parser `data.assignment.*` reads.
  Await operand facts now have the same parser-owned seam across semantic,
  AIR evidence/boundary walks, RIR, module normalization, C, and LLVM; the
  shape smoke rejects non-parser `data.await_expr.*` reads.
  Channel send/recv channel/value facts now use parser-owned accessors across
  semantic transport checks, slot escape/access summaries, AIR, RIR, module
  normalization, C select/spawn/MIR SSA, and LLVM channel/select/type-infer
  paths; the shape smoke rejects non-parser `data.channel_send.*` and
  `data.channel_recv.*` reads.
  Unary operand/operator facts now use parser-owned accessors across semantic
  operator/type inference, slot summaries, AIR/HIR/MIR/module/runtime-none,
  C emission/type inference, and LLVM unary lowering; the shape smoke rejects
  non-parser `data.unary.*` reads.
  Binary left/right/operator facts now use parser-owned accessors across
  semantic operator/type inference, slot summaries, AIR/HIR/MIR/module/runtime-
  none, C binary emission/type inference, MIR SSA/local type, parallel capture,
  and LLVM scalar/type-infer lowering; the shape smoke rejects non-parser
  `data.binary.*` reads.
  Array/tuple literal count and element facts now use parser-owned accessors
  across semantic tuple/array checks, ownership exceptions, AIR/HIR/module/
  runtime-none, C/LLVM literal emission, collection let lowering, MIR SSA, and
  parallel capture; the shape smoke rejects non-parser literal payload reads.
  Defer statement body facts now use a parser-owned accessor across semantic
  flow, DAG body precollect, lambda/intent checks, AIR, MIR call facts,
  module normalization, runtime-none, and C/LLVM defer registration; the shape
  smoke rejects non-parser `data.defer_stmt.*` reads.
  Return statement value facts now use a parser-owned accessor across semantic
  ownership/escape summaries, CFG/HIR/AIR/RIR/module/runtime-none scans, lambda
  inference, parallel capture, and C/LLVM return lowering; the shape smoke
  rejects non-parser `data.return_stmt.*` reads.
  Unsafe block body facts now use a parser-owned accessor across semantic flow,
  DAG body precollect, lambda checks, AIR/CFG/module/runtime-none scans, and
  C/LLVM unsafe lowering; the shape smoke rejects non-parser
  `data.unsafe_block.*` reads.
  Break/continue loop labels now use parser-owned accessors across semantic
  loop-label validation, CFG lowering, flow snapshots, and C/LLVM loop-control
  emission; the shape smoke rejects non-parser `data.break_stmt.*` and
  `data.continue_stmt.*` reads.
  While-loop label/condition/body facts now use parser-owned accessors across
  semantic flow/slot/lambda/DAG checks, AIR/HIR/RIR/module/runtime-none scans,
  CFG lowering, and C/LLVM loop lowering/local-binding helpers; the shape smoke
  rejects non-parser `data.while_loop.*` reads.
  For-loop label/variable/range/iterable/body facts now use parser-owned
  accessors across semantic flow/slot/lambda/DAG checks, AIR/HIR/RIR/module/
  runtime-none scans, CFG/MIR population, and C/LLVM loop lowering/local-
  binding helpers; the shape smoke rejects non-parser `data.for_loop.*` reads.
  Task-group task-list/wait policy facts now use parser-owned accessors across
  semantic lambda/DAG checks and AIR/HIR/RIR scans; the shape smoke rejects
  non-parser `data.task_group.*` reads.
  Spawn function/argument/blocking facts now use parser-owned accessors across
  semantic async boundary checks, lambda/DAG/type inference, AIR/HIR/RIR scans,
  parallel capture, and C/LLVM spawn lowering; the shape smoke rejects non-
  parser `data.spawn_expr.*` reads.
  Async-block statement-list facts now use parser-owned accessors across
  semantic async/lambda/DAG/slot checks, AIR/HIR/RIR/module scans, C/LLVM async
  lowering, parallel capture, projection invalidation, and specialization
  discovery; the shape smoke rejects non-parser `data.async_block.*` reads.
  Select case/default facts now use parser-owned accessors across semantic
  async/lambda/slot checks, AIR/HIR/RIR/module/CFG scans, C/LLVM select
  lowering, local-binding/type lookup, and projection invalidation; the shape
  smoke rejects non-parser `data.select_stmt.*` reads.
  Parallel task-list facts now use parser-owned accessors across semantic
  flow/slot/lambda/DAG checks, AIR/HIR/RIR scans, C/LLVM parallel lowering,
  capture discovery, and specialization discovery; the shape smoke rejects
  non-parser `data.parallel.*` reads.
  With-statement slot type/alias/body/security facts now use parser-owned
  accessors across semantic flow/slot/lambda/DAG checks, AIR/HIR/RIR/module/
  runtime-none scans, MIR resource/type helpers, and C/LLVM with-slot/local-
  binding/type lookup emission; the shape smoke rejects non-parser
  `data.with_stmt.*` reads.
  Block statement-list and pin metadata facts now use parser-owned accessors
  across semantic flow/slot/lambda/intent/DAG checks, AIR/HIR/RIR/module/
  runtime-none scans, CFG lowering, debugger traversal, and C/LLVM block,
  async, select, local-binding, projection invalidation, and specialization
  paths. The only remaining raw `data.block.*` access is the explicit
  parser-owned HIR teardown slot in `src/compiler/hir_destroy.c`; the shape
  smoke rejects all other semantic/compiler/codegen block payload reads.
  Match subject/case/default and match-case pattern/guard/body facts now use
  parser-owned accessors across semantic flow/coverage/lambda/DAG/intent checks,
  AIR/HIR/RIR/module/runtime-none scans, CFG lowering, C/LLVM match lowering,
  projection invalidation, and specialization discovery; the shape smoke rejects
  non-parser `data.match_stmt.*` and `data.match_case.*` reads.
  Lambda parameter/body/return/async facts now use parser-owned accessors across
  semantic expression typing, type inference, DAG precollect, intent control,
  AIR/runtime-none scans, callable registration, and C/LLVM lambda lowering; the
  shape smoke rejects non-parser `data.lambda_expr.*` reads.
  Event subscribe/unsubscribe and invoke target/argument facts now use
  parser-owned accessors across semantic event contracts, lambda capture, DAG
  precollect, AIR/module scans, and C/LLVM event lowering; the shape smoke
  rejects non-parser `data.event_op.*` and `data.event_invoke.*` reads.
  Let-binding name/type/initializer/mutability/alias facts now use parser-owned
  accessors across semantic event/lambda/ownership/DAG checks, slot analysis,
  AIR/HIR/RIR/module/runtime-none scans, MIR local-binding/pending-use facts,
  parallel capture, C let emission, and LLVM let/lambda/event lowering; the
  shape smoke rejects all non-parser `data.let_decl.*` reads.
  Scalar literal value facts now use parser-owned accessors across semantic
  type/flow/builtin query checks and C/LLVM literal/log/type inference
  lowering; the shape smoke rejects non-parser `data.number.*`,
  `data.string.*`, and `data.boolean.*` reads.
  If-statement condition/then/else facts now use parser-owned accessors across
  semantic flow/slot/lambda/intent/DAG checks, AIR/HIR/RIR/module/runtime-none
  scans, CFG lowering, debugger traversal, parallel capture, specialization,
  and C/LLVM branch lowering; the shape smoke rejects non-parser
  `data.if_stmt.*` reads.
  Lightweight semantic type inference now also uses the seam for Slice,
  Slot/Rc/Weak, Clone, and allocator builtin call inference. Semantic
  constructor validation now uses the seam for positional field arity,
  field-argument typing, borrowed-boundary checks, and world embedding
  diagnostics. C MIR match condition lowering now uses the seam for
  Option/Result destructor pattern callee and payload binding reads. C HashMap
  stdlib builtin lowering now uses the seam for map/key/value argument
  rendering and type inference. C AST match lowering now uses the same seam for
  Result/Option and enum-variant destructor pattern callee/payload reads. C
  Slot/SecureSlot/DeviceSlot builtin lowering now uses the seam for
  slot/value/token argument reads. LLVM MIR CFG match conditions and LLVM
  statement match lowering now use the same seam for Option/Result destructor
  pattern callee and payload binding reads. LLVM collection/channel let
  lowering now uses the seam for ToObject, collection constructor, Channel, and
  capacity argument reads. LLVM stdlib scalar/string/file/time IO lowering now
  uses the seam for arity checks, runtime argument arrays, and value arguments.
  LLVM call dispatch now uses the seam for callee classification, Clone,
  projection arity, hosted-method arguments, boundary calls, intent calls, and
  pointer-argument adjustment. LLVM Result/Option lowering now uses the seam
  for Ok/Err/Some payloads, None arity, unwrap/default arguments, and predicate
  operands. LLVM task/channel builtin lowering now uses the seam for
  task/channel/value/timeout arguments and query arity checks. LLVM member-call
  lowering now uses the seam for receiver callee access, static/nominal/member
  chain call arguments, and pointer-self argument adjustment. C backend MIR SSA
  contract checks now use the seam for callee traversal, ToObject/TObject
  payloads, and identifier-mapping argument walks. C stdlib List/Set collection
  lowering now uses the seam for arity, receiver/type inference arguments, and
  emitted value/index/key operands. C `let` lowering now uses the seam for
  callable initializer detection, Option constructors, collection constructors,
  projection borrows, SetNew, and struct/class constructor arguments. C
  Result/Option builtin lowering now uses the seam for Result suffix inference,
  Ok/Err predicates, unwrap/default operands, Some payload type inference, and
  Option consumer arguments. Semantic function-call checking now uses the seam
  for arity, argument type checking, generic actual capture, borrowed-boundary
  validation, ownership transfer, and assignability diagnostics. LLVM extended
  List/Map collection call lowering now uses the seam for arity, receiver
  lookup, key/index/value emission, MapKeys, and slot-source pass-through. C
  channel/task stdlib lowering now uses the seam for query arity, channel
  receiver typing, send/receive values, timeout operands, cancellation, and
  ChannelClose. LLVM statement type inference now uses the seam for member-call
  receivers, nested Slice receiver calls, slot builtin receivers, collection
  value receivers, and declared call return lookup. C stdlib parent builtin
  lowering now uses the seam for Array operations, Clone, Print, and ToString
  arity/operand/type inference. C misc stdlib lowering now uses the seam for
  FSM, timer, cooldown, and string-map wrapper arity/operand emission. C
  expression type inference now uses the seam for member-call receiver types,
  builtin arity, collection/slot/channel/device/Option operands, and ToObject
  nominal inference. C scalar/string/math stdlib lowering now uses the seam for
  arity and operands across numeric, string, random, and conversion wrappers.
  C builtin dispatch now uses the seam for Clone, domain/world/zone query
  arguments, and file/input/print/sleep operands.
  DAG evidence now exposes `type_resolution_metadata_dead_ends` to AIR as the
  only active metadata dead-end counter. The older `materializer_fallbacks`
  stats label and `type_resolution_metadata_materializer_fallbacks` mirror have
  been removed from production semantic state instead of remaining as
  compatibility aliases. `PGY_TYPE_RES_STATS=1` now prints `dead_ends` directly,
  and the DAG smoke gates it at zero. Internal dead-end
  family counters now use `type_resolution_metadata_unresolved_*` naming, and
  resolver-inventory smoke rejects fallback-era family counter names under
  `src/semantic`. CFG/MIR DEF use-edge
  collection now consumes instruction-carried
  `inst->ast` and no longer reopens block source-statement inventory as an
  initializer fallback. MIR BRANCH/RETURN instructions also carry
  `source_terminator_kind`, and `mir_validate(...)` rejects terminators whose
  HIR provenance is missing or mismatched. AIR now consumes those validated
  MIR terminator facts as global `AIR_EVIDENCE_MIR_TERMINATOR` nodes and
  exposes `mir_terminator_evidence_count` in `pgy.air.graph.v1`, so CFG
  terminator provenance is visible to CI/LSP consumers instead of remaining
  MIR-validator-only state. Strict AIR also emits a global missing-evidence
  drift when real MIR input is present for boundaries but no MIR terminator
  evidence was attached. MIR cleanup evidence is now fact-owned as well:
  `mir_block_has_expected_cleanup_edge_fact(...)` centralizes the cleanup fact
  name expected for each block, and AIR only counts cleanup evidence when the
  source block carries that expected MIR cleanup-edge payload. This prevents a
  plain cleanup successor from being treated as proof without the matching MIR
  fact. AIR also has a strict regression for a reachable pin boundary whose MIR
  pin block carries a local pin cleanup fact but no registered cleanup root:
  AIR must collect no pin cleanup evidence and must report missing strict
  evidence, so cleanup-root truth stays owned by MIR.
- 2026-05-14 owner-size/source-inventory checkpoint:
  production owner size is back under the 600 LOC signal without reintroducing
  `.inc` files or implementation-style header blocks. The latest slices are
  responsibility-named rather than `_helpers`-named: AST domain/world and intent
  step accessors, C lambda emission, C array access emission, C Channel let
  emission, LLVM HashMap raw export lookup, and runtime raw map key exports now
  have their own owners. The smoke contracts were updated to track the new
  owners instead of stale monolith paths. Gates: `test-inc-size-test-smoke`,
  `production-header-size-test-smoke`, `build-source-inventory-test-smoke`,
  `test-parser`, `test-transpile`, `perf-contract-test-smoke`, and
  `mir-declaration-inventory-test-smoke`.
- 2026-05-14 relation endpoint source-of-truth tightening:
  relation `between` endpoint kind/type facts are now read through AST accessors
  by semantic relation validation, zone relation contract checks, and DAG
  relation precollect/stage consumers. Parser-owned storage/printing/destruction
  still owns the raw payload, but semantic/compiler/codegen consumers are
  shape-gated against reopening `data.relation_decl.between_*` directly.
  Gates: `test-semantic`, `type-resolution-dag-test-smoke`,
  `type-resolution-resolver-inventory-test-smoke`, and
  `semantic-core-shape-test-smoke`.
- 2026-05-14 party/roster metadata source-of-truth tightening:
  party generic params, party `extends`, and roster generic params now flow
  through `ast_party_generic_params(...)`, `ast_party_extends(...)`, and
  `ast_roster_generic_params(...)` for semantic declaration validation, DAG
  inventory/stage replay, runtime-none scans, module normalization, and LLVM
  generic-default lookup. The semantic core shape smoke now blocks direct
  metadata payload reads from semantic/compiler/codegen owners.
- 2026-05-14 class metadata source-of-truth tightening:
  class generic params and where clauses now flow through
  `ast_class_generic_params(...)` and `ast_class_where_clause(...)` across
  semantic validation, generic contracts, DAG metadata/dead-end accounting,
  stage replay, module normalization, C specialization, and LLVM generic
  default lookup. The same smoke blocks direct class metadata payload reads
  from semantic/compiler/codegen owners. Enum payload parameter consumers now
  use `ast_enum_variant_param_count(...)` and `ast_enum_variant_param(...)` only
  on parser/semantic/MIR-header construction paths; backend constructor and
  enum emission consume MIR declaration metadata, and codegen enum variant AST
  access is ratcheted at zero.
- 2026-05-14 ability/role metadata source-of-truth tightening:
  ability generic params, where clauses, required fields, and method lists now
  flow through AST accessors, and role generic params, where clauses, and
  parallel blocks use the same boundary. Semantic validation, module contracts,
  DAG precollect/stage replay, module normalization, runtime-none scanning, C
  ability vtable emission, and LLVM generic-default lookup no longer reopen
  those declaration payloads directly. Ability/role declaration names also use
  read-only accessors outside the explicit mutable-name owner
  `module_normalizer.c`. Ability visibility/innate policy now uses
  `ast_ability_access(...)`, `ast_ability_has_explicit_access(...)`, and
  `ast_ability_is_innate(...)`, and the semantic core shape gate rejects all
  non-parser ability/role payload reads outside that mutable-name owner.
- 2026-05-04 CFG/MIR intent-step consumer tightening:
  C and LLVM intent step collection now classify step instructions through
  `mir_instruction_intent_step_name(...)` instead of rechecking
  `source_ast_type != AST_INTENT_STEP`. AST payloads remain only as
  expression/step emission payloads, not as the step metadata source of truth.
  Gate: `cfg-body-dataflow-test-smoke`, `perf-contract-test-smoke`, and
  `test-mir` (`41/0`).
- 2026-05-04 source artifact hygiene tightening:
  tracked ELF/PE/Mach-O executables are not allowed under `examples/` or
  `tests/cases/`. Stale generated example and ABI/backend case binaries were
  removed from the tracked tree; `build-source-inventory-test-smoke` now scans
  those fixture roots for executable artifacts, and `.gitignore` covers
  regenerated `tests/cases/**/main` outputs.
- 2026-05-04 AIR boundary walker tightening:
  expression-boundary counting and boundary materialization now share one
  `AIRBoundaryWalkCtx` traversal. This removes the prior split count/append
  AST walkers and keeps boundary allocation size, source spans, and appended
  boundary inventory on the same abstraction-boundary traversal. AIR AST
  boundary kind/source classification is now table-backed through
  `AIRAstBoundaryRule`, so boundary taxonomy and user-facing source labels no
  longer drift through separate switches. Gates: `test-air` (`75/0`),
  `air-drift-test-smoke`, and `air-json-schema-test-smoke`.
- 2026-05-02 debt ledger refresh:
  the current blocker map is now separated into closed seams and remaining
  source-of-truth seams. CFG/MIR use facts prefer instruction-carried
  provenance for DEF/branch/return and MIR value summaries consume DEF slot
  anchors. C backend block-local usage/pending/order facts now consume MIR
  instruction provenance instead of MIR block source statement arrays, while
  MIR lowering still carries HIR source arrays as construction input. AIR evidence inventory is the preferred consumer API for covered
  facts, but not every abstraction boundary is fully evidence-node driven.
  DAG metadata dead-ends remain zero; the remaining DAG debt is evidence/model
  coverage and semantic-owner provenance widening, not another compatibility
  fallback counter cleanup. C/LLVM hosted-method declaration views now reject
  silent AST fallback when MIR metadata is required, but declaration payloads
  inside `MIRProgram` are still AST-backed. Runtime intent exit uses active
  registry indexed lookup; the full transitive frontier scheduler remains a
  blocker.
- 2026-05-02 pending-use provenance tightening:
  C backend pending-use materialization now uses block `MIR_INST_DEF.ast`
  provenance to recover local let declarations and no longer scans
  `block->source_statements` directly. Source-order scheduling now consumes
  `MIRInstruction.source_statement_index` metadata instead of walking
  `block->source_statements`, so C backend block-local ordering also depends on
  MIR instruction provenance.
- 2026-05-02 thread-pool usage fact tightening:
  shared C/LLVM runtime thread-pool detection now treats `await` and
  `task-group` as direct runtime surfaces and scans MIR instruction `ast`,
  `expr0`, and `expr1` provenance only; source-only block arrays are no longer
  consulted for this feature-use decision. The structural AST traversal is now owned by
  `src/parser/ast_analysis.c` through `ast_uses_thread_pool_surface(...)`;
  `thread_pool_usage.c` only adapts that fact for C/LLVM MIR consumers. The
  backend entry points no longer special-case `__pgy_top_level_exec`; that
  synthetic executable must be present in the MIR routine inventory like any
  other routine. `parallel-core-contract-test-smoke` rejects reintroducing
  source-array fallback in this path.
- 2026-05-02 intent zone-authority compression:
  superseded by the who/approval separation rule. `who` is actor/provenance
  only and no longer derives `authorized by`; authority-sensitive steps in
  authority-bearing zones must spell approval explicitly or inherit it from an
  explicit action contract. Diagnostics may suggest a matching authority
  participant, but they must not mutate the step contract from `who` alone.
- 2026-05-02 intent on-receiver compression:
  intent steps can now derive omitted `who` from `on: receiver.Action(...)`
  when the receiver is an intent subject participant and the subject declares
  that action. Ambiguous receivers stay explicit. The provenance is visible in
  AST print, contract summary, DIR, AIR, and `pgy.air.graph.v1` as
  `who_from_on_receiver`. This is intentionally narrower than full
  Intent-Compress; `where`, `using`, `requires`, and `authorized by` inference
  remain separate closure work.
- 2026-05-02 intent on-receiver where/using compression:
  the same receiver/action evidence can now derive `where` from the resolved
  action's `within <Zone>` clause when no step-local zone is present. Existing
  unique-zone-binding logic then derives `using` from that zone type. Explicit
  `where` still wins, and conflicting `on` action zones fail closed by not
  inferring.
- 2026-05-02 intent on-receiver action contract compression:
  a single resolved `on: receiver.Action(...)` now inherits `requires` and
  `causes` from that action when the step has no local clause. `authorized by
  self` maps to the receiver alias. `authorized by <action-param>` also maps
  to the corresponding single `on` call argument when that argument is a
  declared intent participant identifier. Non-identifier arguments, missing
  parameter bindings, and multiple `on` calls stay explicit. The on-inference
  owner reads those action contracts through parser-owned function contract
  accessors, so compact syntax does not reopen raw `func_decl` payloads while
  materializing explicit facts for DIR/AIR.
- 2026-05-02 AIR authority provenance lift:
  derived approval is no longer semantic-only. DIR retains the legacy
  `authorized_by_derived_from_zone` field for compatibility and carries
  `authorized_by_inherited_from_action`; action-derived `where` also carries
  `where_inherited_from_action`, and action-derived `requires`/`causes` carry
  `requires_inherited_from_action` / `causes_inherited_from_action`. AIR carries
  the retained `authority_from_zone` schema field, `authority_from_action`,
  `source_from_action`,
  `requires_from_action`, and `causes_from_action`; JSON dumps expose those
  fields, and AIR diagnostics report
  `authority_provenance=action-inherited|explicit|none` on active beta paths;
  any compatibility-only zone field is labeled `legacy-zone-field`.
  The parsed AIR regression also requires action-inherited authority to match
  real RIR authority evidence (`AIR_EVIDENCE_RIR_AUTHORITY` plus
  `rir_authority_evidence_name`), not just the AIR boundary flag.
- 2026-05-02 MIR cleanup ownership repair:
  MIR statement reconstruction now restores `instruction_capacity` after
  rebuilding a block's instruction array. This closes a heap-corruption path
  where later cleanup-edge materialization wrote past the rebuilt array in pin
  regions. C MIR block mapping comments also stopped emitting raw AST pointer
  addresses, so AIR strict/relaxed backend non-impact checks compare
  deterministic artifacts instead of process-local addresses.
- 2026-05-02 MIR CFG predecessor validation tightening:
  MIR validation now checks predecessor lists in both directions. A successor
  must appear in the target predecessor list, and every recorded predecessor
  must have a matching forward edge. This closes a CFG shape hole where cleanup
  or exceptional blocks could retain stale predecessor entries after lowering
  rewrites. Gate: `make test-mir cfg-body-dataflow-test-smoke`.
- 2026-05-02 DAG generic-param evidence tightening:
  class/function/ability generic parameters and nominal staging scopes now
  register as `SYMBOL_TYPE_PARAM` carrying `TYPE_KIND_GENERIC`, not as
  class-like placeholders. The DAG smoke now requires non-zero
  `GENERIC_PARAM` evidence (`generic_param_nodes=29` locally), so generic
  parameter dependencies cannot silently regress into declaration evidence.
- 2026-05-02 DAG class-field seam removal:
  class/subject/vessel field signatures now write metadata during nominal
  staging before falling back to graph-backed skip accounting. The class
  declaration checker consumes annotation metadata and no longer calls the
  materializing type-ref helper. Superseded gate note: this was part of the
  helper-ref cap staircase that later converged to `type-ref helper refs=0`.
- 2026-05-02 DAG domain/world field seam removal:
  relation/effect/zone/world field signatures now write metadata before
  semantic owner checks consume those types. Domain and world helper owners now
  consume annotation metadata instead of the materializing type-ref helper.
  Superseded gate note: this was part of the helper-ref cap staircase that
  later converged to `type-ref helper refs=0`.
- 2026-05-02 DAG effective generic arg seam tightening:
  ability where validation now consumes centralized effective-argument type
  evidence from `collect_effective_generic_arg_types(...)`. The materializing
  helper was originally owned by `type_checker_generic_effective_args.c`; the
  2026-05-03 follow-up removes that materializer seam and also moves
  `type_checker_generic_contracts.c` plus
  `type_checker_generic_validation.c` to annotation metadata and
  `semantic_type_resolution_lookup_metadata_type_ref(...)` only. Host/domain
  slot helper reads, intent participant/value/step-where type reads, function
  parameter/return signatures, and expression-local annotations also moved to
  the metadata-only path. `type_checker_ownership_let_helpers.c` now consumes
  metadata type-ref facts plus the stable-shell arity, constructed-type, and
  unknown-bare-name diagnostic helpers. The rejected annotation-only probe
  caused broad semantic drift, so the accepted closure is metadata +
  diagnostics, not annotation-only. Semantic owners no longer call the
  materializing type-ref helper. The resolver inventory cap is now 0 type-ref
  helper references; the old central API declaration/implementation are gone,
  while fallback/materializer counters stay at 0. ABI/runtime layout remains
  unchanged. Intent role-field require checks also consume that centralized
  effective-argument type evidence, keeping `type_checker_intent_role_fields.c`
  below the 600 LOC split-review line.
- 2026-07-10 intent-observability projection ownership closure:
  MIR inventory surface usage is now the only input to native C/LLVM
  observability materialization. `src/compiler/verified_projection_plan.c`
  emits plan row 1 as `OBS0/ERASE` or `OBS1/MATERIALIZE` and fails closed when
  the fact is missing. The old codegen AST/HIR/name fallback scanner is deleted.
  The 51 source/runtime/arity/return ABI rows also have one common owner;
  semantic checking and C/LLVM consume it without per-call `BuiltinKind` or
  backend-local symbol tables. `make verified-projection-plan-test-smoke`
  prevents those aliases from returning. Full AIR-certificate projection-plan
  closure remains open.
- 2026-05-02 generic class specialization evidence tightening:
  class specialization where-clause validation consumes the same centralized
  effective generic argument type evidence instead of building a local type
  array from effective arg nodes. This removes duplicate dependency/materialize
  work without changing ABI/runtime layout. Superseded gate note: this was part
  of the helper-ref cap staircase that later converged to
  `type-ref helper refs=0`.
- 2026-05-02 intent binding owner split:
  intent participant/value lookup and transfer-target alias resolution moved to
  `type_checker_intent_bindings.c`. The role-field owner now focuses on
  ability require-field validation and zone-binding derivation, staying at 499
  LOC after the split. ABI/runtime layout is unchanged, and the DAG resolver
  inventory cap has since converged to `type-ref helper refs=0`.
- 2026-05-02 intent type owner split:
  intent-local type-ref resolution, participant/value type resolution, and
  step where-source labeling moved to `type_checker_intent_types.c`.
  `type_checker_intent_decl.c` is now 529 LOC and stays focused on intent
  orchestration validation. The materializing seam count has since converged to
  `type-ref helper refs=0`.
- 2026-05-02 DAG intent inventory owner split:
  intent declaration precollect moved from the general declaration graph owner
  to `type_checker_resolution_graph_intent.c`. This makes intent DAG inventory
  a named source owner and drops `type_checker_resolution_graph_decl.c` to 481
  LOC without changing DAG stats or fallback/materializer counters.
- 2026-05-02 DAG zone command inventory owner split:
  zone refresh/apply/link/detach/unlink/maintain dependency precollect moved to
  `type_checker_resolution_graph_zone_commands.c`. The original
  `type_checker_resolution_graph_zone_inventory.c` now owns only zone
  slot/shared/layer type inventory and is 76 LOC. This is a responsibility
  split under the 600 LOC application guide, not a mechanical slicing rule.
  DAG stats remain unchanged: graph-backed skips `1980`, metadata hits `8044`,
  fallback/materializer counters `0`.
- 2026-05-02 DAG graph validation owner split:
  `type_checker_resolution_graph_core.h` is no longer an implementation
  header included by `type_checker.c`. Cycle validation and topo ordering now
  live in `type_checker_resolution_graph_validate.c`; the core graph owner
  keeps node/edge/path/dependency primitives below the 600 LOC split-review
  signal.
- 2026-05-02 split-policy correction and helper consolidation:
  the 600 LOC rule is now documented as a split-review trigger, not a
  mechanical slicing mandate. New `_helpers` owners are discouraged unless
  they represent a real feature/fact owner. `llvm_stmt_let_collections.c`
  applies the policy by replacing parallel missing-type-argument and
  missing-runtime-export helpers with one enum-driven
  `llvm_stmt_diag_collection(...)` path. Syntax gate:
  `gcc -DPGY_LLVM_ENABLED -fsyntax-only src/codegen/llvm_stmt_let_collections.c`.
- 2026-05-02 CFG/MIR root identity validation:
  MIR validation now rejects overlapping entry, cleanup, rollback, and
  invalidation roots. This closes a cleanup-chain shape hole where a corrupted
  root could still point at a valid block index. Gate:
  `make test-mir cfg-body-dataflow-test-smoke`.
- 2026-05-03 CFG/MIR direct-call fact tightening:
  direct statement calls now carry their callee name as `MIR_INST_STMT.arg0`,
  and direct initializer calls carry their callee name as `MIR_INST_DEF.arg1`.
  Intent observability no-trace detection consumes those MIR facts and HIR
  routine `direct_calls` before falling back to structural AST traversal. Gate:
  `make test-mir cfg-body-dataflow-test-smoke test-transpile
  perf-contract-test-smoke` (`32/0` MIR tests, `710/0` transpile tests).
- 2026-05-03 CFG loop-flow consumer tightening:
  `while` and static range `for` statements now return semantic CFG flow flags
  to their parent body instead of being flattened through the generic statement
  fallback. The accepted slice is conservative: `while true { return ... }`
  satisfies non-`Void` all-path return, and
  `for i in 0..1 { return ... }` satisfies it only when the range is
  statically non-empty and no `break` path exits the loop. `for-in`, empty
  ranges, dynamic ranges/conditions, possible `break`, and non-returning
  backedges remain fallthrough. `while` static Bool truth is now consumed
  through `flow_static_bool_value(...)` instead of having the loop owner decode
  `AST_BOOLEAN` payloads directly. Gate:
  `make test-semantic cfg-body-dataflow-test-smoke` (`2497/0` semantic tests).
- 2026-05-03 DAG intent/action-contract seam tightening:
  `type_checker_intent_role_fields.c` no longer owns a second local
  materializing type-ref helper, and `type_checker_func_action_contract.c`
  consumes annotation metadata for action-contract domain-slot/parameter reads.
  This keeps direct semantic behavior stable while shrinking the resolver
  inventory cap from `12` to `10`; the later generic/host/intent/function/expr
  metadata-only slice lowered the cap to `3`; the ownership-let closure now
  removes the last semantic owner seam by consuming metadata type-ref facts plus
  the shared stable-shell/constructed-type/unknown-name diagnostic helpers. The
  cap is now `0`; the old materializing type-ref API is absent under
  `src/semantic`. Gate:
  `make test-semantic type-resolution-dag-test-smoke
  type-resolution-resolver-inventory-test-smoke`.
- 2026-05-04 DAG stable-shell vocabulary tightening:
  stable generic shell arity and constructed-shell lookup now share a
  `StableShellSpec` table instead of parallel `strcmp` chains. Slot-like shell
  materialization uses a `StableSlotShellSpec` dispatch table for `Slot`,
  `SecureSlot`, `ReadView`, `WriteView`, and `MoveToken` constructor selection.
  The resolver inventory smoke now gates these tables rather than the previous
  branch strings. Current gate:
  `type-resolution-resolver-inventory-test-smoke`,
  `type-resolution-dag-test-smoke`, and `pgy`
  (`materializer_unresolved=0`).
- 2026-05-04 DAG direct named resolver closure:
  expression/world host access and overlay world-zone slot registration now use
  metadata-only named-type lookup seams. The retired `resolve_named_type(...)`
  API and prototypes were removed, and the resolver inventory smoke rejects
  reintroducing the symbol anywhere under `src/semantic`. Named-type reads can
  no longer silently bypass DAG metadata through that compatibility entrypoint.
  The unused `type_checker_resolution_helpers.h` compatibility header is also
  gone; internal declarations live in `type_checker_internal.h`.
  Gates: `type-resolution-resolver-inventory-test-smoke`,
  `type-resolution-dag-test-smoke`, and `test-semantic` (`2500/0`).
- 2026-05-03 intent compression provenance tightening:
  `where`-derived unique zone bindings now leave an explicit
  `derived_using_from_where` fact instead of looking like a local `using`
  clause in AST print/contract summaries. Gate:
  `make test-parser test-semantic cfg-body-dataflow-test-smoke`
  (`2498/0` semantic tests).
- 2026-05-03 CFG loop snapshot lifetime fix:
  `for`/`while` flow restores merged resource state before destroying the loop
  scope. This prevents loop snapshots that contain loop-local symbols from
  writing through freed scope storage during transpile/MIR lowering tests.
  Parallel task flow now restores the entry ownership snapshot before
  destroying each task scope, keeping task-local symbols out of post-scope
  writes while preserving joined conflict analysis.
  Function signature metadata misses also fail closed to `TYPE_UNKNOWN` rather
  than crashing `type_create_function(...)`. Gate: manual native MinGW
  `test-semantic` (`2500/0`) and `test-transpile` (`710/0`).
- 2026-05-03 AIR boundary evidence fact-count closure:
  HIR/RIR/MIR boundary evidence nodes must carry exactly one boundary fact.
  This keeps a single boundary proof from being widened into an ambiguous
  multi-fact evidence node. Gate: manual native MinGW `test-air` (`68/0`).
- 2026-05-03 MIR source-location materialization:
  C MIR block mapping comments now consume scalar MIR source-location facts
  (`has_source_location`, `source_line`, `source_column`) instead of reading
  `block->source_ast` in codegen. Source AST pointers remain construction and
  debug provenance, but backend comments no longer consume them directly. Gate:
  manual native MinGW `test-mir` (`32/0`), `test-air` (`68/0`), and
  `test-transpile` (`710/0`).
- 2026-05-03 MIR surface-usage fact materialization:
  MIR instructions now carry `has_surface_usage_facts` and
  `uses_thread_pool_surface` / `uses_intent_observability_surface`. C/LLVM
  thread-pool dependency checks and intent-observability no-trace detection
  consume those MIR facts first and only scan AST payloads for hand-built or
  legacy MIR without facts. The shared fact materializer is now called by base,
  cleanup, and intent MIR append paths. Gate: manual native MinGW
  `test-semantic` (`2500/0`), `test-mir` (`35/0`), `test-air` (`70/0`), and
  `test-transpile` (`710/0`).
- 2026-05-03 MIR branch-shape materialization:
  branch and loop-init instructions now carry `MIRBranchShape` (`FOR_RANGE`,
  `FOR_IN`, `MATCH_CASE`, `SELECT_DISPATCH`). C and LLVM MIR control emitters
  consume that fact instead of classifying branch control by AST node type. AST
  payloads remain only for expression/condition emission. MIR validation and
  MIR lowering regressions also consume `branch_shape` for loop-branch
  completeness, so the fact is now part of the MIR contract, not just a backend
  convenience. Gate: manual native MinGW `test-semantic` (`2500/0`),
  `test-mir` (`32/0`), `test-air` (`68/0`), `test-transpile` (`710/0`), plus
  LLVM control owner compile smoke.
- 2026-05-03 MIR dump source-location tightening:
  `mir_dump(...)` now prints source locations from
  `MIRBasicBlock.has_source_location` / `source_line` / `source_column` instead
  of rebuilding them from `source_statements[0]` or terminator AST pointers.
  This keeps public MIR dumps aligned with materialized MIR facts. Gate: manual
  native MinGW `test-mir` (`32/0`) and `test-transpile` (`710/0`).
- 2026-05-03 MIR instruction source-location materialization:
  instructions now carry `has_source_location`, `source_line`, `source_column`,
  and `source_ast_type` facts. `mir_dump(...)` prints instruction `ast-type` /
  `line` from those facts instead of reading `inst->ast`. AST payloads remain
  available to expression emitters, but the public MIR dump path no longer
  consumes AST pointers for instruction provenance. Gate: manual native MinGW
  `test-mir` (`32/0`), `test-air` (`68/0`), and `test-transpile` (`710/0`).
- 2026-05-03 MIR AST-type consumer tightening:
  C/LLVM codegen no longer branches on `inst->ast->type`. Instruction kind
  decisions now consume `source_ast_type` / `has_source_location`; AST payloads
  remain only where expression or statement emission still needs the original
  syntax tree. Gate: manual native MinGW `test-transpile` (`710/0`), `test-mir`
  (`32/0`), `test-air` (`68/0`), and `perf_contract_smoke`.
- 2026-05-03 C backend source-array consumer tightening:
  `transpiler_mir_find_stmt_for_inst(...)` now trusts instruction-carried
  statement AST provenance first and falls back only to function-scope let
  lookup by name. Codegen no longer reads `block->source_statements`,
  `block->source_ast`, `source_terminator_*`, or `inst->ast->type` in the
  scanned C/LLVM backend owners; those block source arrays remain MIR
  construction input, not backend judgement input. Gate: manual native MinGW
  `test-transpile` (`710/0`) and `perf_contract_smoke`.
- 2026-05-03 MIR construction fact hardening:
  terminator and resource instructions now call
  `mir_instruction_record_surface_usage(...)` at construction time, not only
  through later append/rewrite paths. This keeps branch/return/resource
  instructions carrying source location, AST type, and thread-pool surface facts
  even if future construction paths bypass a rewrite helper. `MIRBasicBlock`
  also no longer stores `source_ast` or `source_terminator_*` pointers; HIR
  terminator payloads are consumed while constructing MIR terminator
  instructions and then represented by MIR instruction facts. Gate: manual
  native MinGW `test-mir` (`32/0`), `test-transpile` (`710/0`), plus
  PowerShell-equivalent contract/size scans for the `perf_contract_smoke` and
  `test_inc_size_smoke` assertions.
- 2026-05-03 MIR use-edge provenance tightening:
  DEF use-edge collection no longer walks forward through
  `block->source_statements` looking for the next plausible let/assignment. If
  a DEF instruction has no attached AST payload, the fallback is now an exact
  `source_statement_index` lookup only. This keeps use-edge facts tied to
  instruction provenance instead of implicit source-array ordering. Gate:
  manual native MinGW `test-mir` (`32/0`), `test-transpile` (`710/0`), and
  PowerShell-equivalent `perf_contract_smoke` assertions.
- 2026-05-03 MIR statement-inventory accessor seam:
  `MIRBasicBlock` now carries
  `MIRStatementInventory source_statement_inventory` instead of raw
  `source_statements` / `source_statement_count` fields. Statement population
  routes through `mir_block_source_inventory_count(...)`,
  `mir_block_source_inventory_at(...)`, and
  `mir_block_source_inventory_items(...)`, while use-edge validation consumes
  the named inventory directly. This does not remove HIR source statements from
  MIR construction yet; it makes the remaining construction input an explicit
  inventory contract instead of an open block array. Gate: manual native MinGW
  `test-mir` (`33/0`), `test-transpile` (`710/0`), and PowerShell owner/contract
  size scans.
- 2026-05-03 MIR statement-inventory validation:
  `mir_validate(...)` now rejects malformed statement inventory storage
  (`count > 0` with no `items`) and instruction source-statement indexes outside
  the named inventory. The regression fixture corrupts both shapes explicitly,
  so downstream MIR consumers no longer rely only on defensive null checks.
  Gate: manual native MinGW `test-mir` (`33/0`) and `test-transpile` (`710/0`).
- 2026-05-03 MIR HIR-pointer cleanup:
  `MIRBasicBlock` no longer stores the raw `source_hir_block` pointer. The MIR
  contract keeps only `source_hir_block_id`, which is enough for CFG mapping
  validation, MIR dumps, and C block mapping comments. This removes another
  AST/HIR-carried pointer from MIR block state without changing emitted code.
  Gate: manual native MinGW `test-mir` (`33/0`) and `test-transpile` (`710/0`).
- 2026-05-03 MIR surface-usage validator:
  `mir_validate(...)` now rejects instructions that carry AST/expression/source
  payloads without materialized surface-usage facts, and also rejects stale
  thread-pool or intent-observability facts when the instruction payloads no
  longer match the stored bits. This turns surface usage from a best-effort
  construction convention into a MIR contract: codegen can consume
  `has_surface_usage_facts`, `uses_thread_pool_surface`, and
  `uses_intent_observability_surface` without silently relying on AST rescans
  for normal lowered MIR. Thread-pool dependency detection now follows the same
  consumer rule as intent observability: HIR-backed lowered routines consume
  MIR facts only, while AST payload rescans are reserved for hand-built legacy
  MIR without HIR provenance. Gate: manual native MinGW `test-mir` (`35/0`) and
  `test-transpile` (`710/0`).
- 2026-05-08 intent observability exact classification:
  intent observability usage now uses the exact stable builtin registry rather
  than treating every `Intent*` call as runtime-observable. MIR intent inventory
  statements are classified separately through an exact sorted
  `mir_instruction_is_intent_semantic_carrier(...)`, so `IntentStep`/
  `IntentWho`/`IntentDispatch` remain protected semantic carriers while a user
  function such as `IntentDomainAction()` does not force trace runtime setup or
  survive DCE as intent metadata. The perf contract also gates common/codegen/
  semantic/resolver/LLVM observability table drift and bsearch ordering.
  Gate: native MinGW `test-mir` (`57/0`) and `perf-contract-test-smoke`.
- 2026-05-08 AIR evidence-kind classification owner:
  AIR evidence-kind knowledge and boundary/global scoping now flow through
  `air_evidence_kind_is_known(...)` and
  `air_evidence_kind_is_boundary_scoped(...)`; global-validator availability
  flows through `air_evidence_kind_has_global_validator(...)`. Inventory
  validation and global evidence validation consume the same metadata owner,
  reducing drift when new first-class evidence nodes are added. Gate: native MinGW
  `test-air` (`87/0`), `air-drift-test-smoke`, `air-json-schema-test-smoke`,
  and `perf-contract-test-smoke`.
- 2026-05-08 AIR/RIR IO boundary vocabulary owner:
  the stable IO/time boundary builtin set now lives in
  `src/compiler/io_boundary_builtin.c` and is consumed by both AIR boundary
  synthesis and RIR lowering. This removes duplicate `io_names[]` scans from
  `air_boundary.c` and `rir_builder_walk.c`, keeps the AIR/RIR vocabulary
  sorted for `bsearch`, and gates future drift in `perf-contract-test-smoke`.
  The same pass applies sorted-table classification to claim-slot codegen
  policy and parser intent header value-binding names. Gate: native MinGW
  `test-parser`, `test-rir` (`18/0`), `test-air` (`87/0`),
  `test-transpile` (`745/0`), `air-drift-test-smoke`, and
  `perf-contract-test-smoke`.
- 2026-05-08 driver diagnostic mapping owner:
  driver stage-fail JSON diagnostics now use a single `DriverDiagCodeMap` for
  code extraction, `cause_ir`, and `fix_source` mapping. This prevents parser,
  lexer, AIR, and runtime-none diagnostic metadata from drifting across
  parallel if-chains. Gate: native MinGW `diagnostics-json-test-smoke`,
  `layered-diagnostics-contract-test-smoke`, and `perf-contract-test-smoke`.
- 2026-05-08 DAG evidence naming at AIR boundary:
  AIR DAG evidence now consumes `SemanticResult` fields named for DAG evidence:
  `type_resolution_dag_generic_contract_evidence_count` and
  `type_resolution_dag_ability_consumer_evidence_count`. The old zero-only
  `type_resolution_stage_compat_*` semantic-result mirrors have been removed,
  reducing compatibility-seam vocabulary in strict AIR. Gate: native MinGW
  `test-air` (`87/0`), `type-resolution-dag-test-smoke`,
  `type-resolution-resolver-inventory-test-smoke`, and
  `perf-contract-test-smoke`.
- 2026-05-01 dogfood-first beta gate:
  the beta target is now "core stable enough to start a small WebGL/chat-game
  dogfood", not a full 1.0 compiler. Quantum, Rust-style lifetime borrow
  checking, and native LLVM wasm are not beta blockers. The first dogfood path
  is `Pergyra -> C backend --emit-c -> optional Emscripten/WebGL bridge`. Gate:
  `make dogfood-webgl-test-smoke`. The smoke validates host-import/frame-callback
  C emission and links with `emcc` only when Emscripten is installed.
- 2026-04-30 AIR payload-containment update:
  AIR boundary walking and HIR containment now also descend through event
  subscribe/unsubscribe handler payloads, party-instance assignment values,
  party shared-field initializers, world roster/zone initializers, and
  domain-slot initializers. These carrier nodes are not new AIR boundary kinds;
  they only prevent existing IO/parallel/channel/execution boundaries from
  being hidden behind a payload container. Gate: `make test-air
  air-drift-test-smoke` (`51/0` AIR tests).
- 2026-04-30 AIR event execution boundary update:
  `AST_EVENT_SUBSCRIBE` and `AST_EVENT_UNSUBSCRIBE` are now AIR execution
  boundaries with `event-subscribe` / `event-unsubscribe` sources. The handler
  payload is still traversed, so an event subscription can produce both the
  outer execution boundary and nested IO/parallel/channel boundaries. This is
  a verification-layer change only; AIR remains absent from codegen IR.
- 2026-04-30 AIR evidence provenance tightening:
  `air_validate(...)` now rejects empty HIR routine, RIR boundary, and RIR
  authority evidence provenance names. Evidence flags must carry named proof
  provenance; boolean-only or empty-string evidence is treated as
  `PGY_AIR_INVARIANT_INVALID`. Gate: `make test-air air-drift-test-smoke`
  (`51/0` AIR tests).
- 2026-04-30 AIR 1.0 scope freeze:
  AIR is now documented as the 1.0 closure target for abstraction safety, not
  as a replacement for CFG, DAG, MIR, ownership, or runtime propagation. Beta
  keeps Phase 1 narrow (`IntentNode`, `BoundaryNode`, strict evidence, drift
  facts); 1.0 requires first-class `EvidenceNode`s that audit HIR CFG, DIR,
  RIR, MIR cleanup/pin, and DAG generic/ability/module facts without becoming a
  codegen IR.
- 2026-04-30 AIR evidence-node implementation step:
  `AIREvidenceNode` is now present in the AIR data model and dump output.
  HIR routine, HIR CFG, RIR boundary, and RIR authority evidence are recorded as
  provenance-carrying nodes while the legacy per-boundary flags remain as the
  current driver compatibility seam. `air_validate(...)` rejects malformed
  evidence-node inventory.
- 2026-04-30 AIR evidence boundary-shape tightening:
  first-class evidence nodes are now validated against their boundary class.
  Global evidence cannot attach to a concrete boundary; HIR CFG evidence requires
  same-boundary HIR routine evidence; RIR authority evidence requires
  same-boundary RIR boundary evidence and a declared participant; MIR pin cleanup
  evidence can only satisfy a `pin` execution boundary. Gate: `make test-air`
  (`51/0` AIR tests).
- 2026-05-02 AIR observability schema evidence:
  the stable observability/trace schema is now represented as global
  `AIR_EVIDENCE_OBSERVABILITY_SCHEMA` evidence. The evidence provider is
  `runtime-observability-schema`, the subject is `pgy.intent.observability.v1`,
  and the fact count is derived from the runtime schema vocabulary. Gate:
  `make test-air air-drift-test-smoke air-json-schema-test-smoke
  air-backend-nonimpact-full-test-smoke`.
- 2026-05-04 AIR runtime frontier policy evidence:
  bounded frontier pass-limit policy is now represented as global
  `AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY` evidence with provider
  `pgy.runtime.frontier-policy.v1` and subject
  `bounded-frontier-pass-limit`. This closes the AIR hook for the runtime
  policy source of truth, not the full transitive frontier scheduler itself.
  Gates: `make test-air air-drift-test-smoke air-json-schema-test-smoke
  runtime-frontier-policy-test-smoke runtime-frontier-contract-test-smoke`.
- 2026-04-30 AIR MIR pin-cleanup evidence step:
  `air_collect_mir_evidence(...)` records MIR-owned `pin-unpin-cleanup-edge`
  facts as `AIR_EVIDENCE_MIR_PIN_CLEANUP` nodes for matching AIR `pin`
  execution boundaries. This keeps MIR as the cleanup source of truth while
  giving AIR a provenance-carrying audit hook for 1.0 abstraction safety.
- 2026-05-24 DAG retired-audit closure:
  `PGY_TYPE_RES_STATS=1` no longer reports zero-only recursive resolver
  counters. The retired resolver owner remains only as a quarantine sentinel,
  and the resolver inventory smoke rejects reintroducing compatibility counters
  or resolver bodies.
- 2026-04-30/2026-05-10 DAG public seam tightening:
  annotation-sensitive metadata readers are centralized behind
  metadata-owner APIs, and contract/boundary type references now consume
  metadata facts plus narrow diagnostic helpers instead of the removed
  materializing type-ref helper. Direct `annotation_or_unknown` consumers are
  capped to the program placeholder path; raw resolved-type lookup remains
  private to metadata materialization owners through
  `type_checker_resolution_metadata_internal.h`.
  `type-resolution-resolver-inventory-test-smoke` rejects re-export through the
  semantic mega-header or non-metadata owners. Local gates:
  `type-resolution-resolver-inventory-smoke`, `type-resolution-dag-smoke`
  when `SEMANTIC_TEST_BIN` is available, and targeted semantic syntax checks.
- 2026-04-30/2026-05-10 DAG declaration/helper reader tightening:
  `type_checker_ability_decl.c`, `type_checker_projection_path.c`,
  `type_checker_zone_decl_authority.c`, `type_checker_expr_call.c`,
  `type_checker_expr_host.c`, `type_checker_call_constructor.c`,
  `type_checker_intent_participants.c`, `type_checker_intent_transfer.c`, and
  `type_checker_intent_action_contract.c` are classified materializing helper
  users for declaration/field/method-return/host-expression/constructor,
  intent participant, transfer, and inherited-action reader paths. The current
  materializing helper inventory
  is capped at 15 total references, including the central declaration and
  implementation, while retired resolver calls and materializer fallbacks stay
  at `0`.
- The remaining DAG gaps are classified as evidence/modeling gaps, not
  fallback seams: domain host/slot metadata must feed authority checks,
  generic ability where-clause checks must preserve bound provenance, and
  generic defaults must expose effective-argument materialization evidence
  without reintroducing recursive fallback consumers.
- 2026-05-24 DAG stage materializer telemetry closure:
  `type-resolution-dag-test-smoke` no longer consumes the zero-only
  `stage-metadata-materialize` or `stage-materialize-family` counters. The
  stable DAG gate is now graph-backed skips, metadata inventory/reuse,
  DAG evidence, and alias diagnostic inventory.
- 2026-04-30 DAG writer inventory gate:
  resolved-type metadata recorders are restricted by smoke test to graph,
  stage-signature, and metadata materialization owners. This prevents ordinary
  semantic declaration/body owners from mutating DAG resolved-type facts
  directly and keeps the graph/materializer boundary explicit.
- 2026-04-30 DAG stage-signature fallback removal:
  signature staging no longer calls the metadata materializer after metadata
  miss. It consumes graph dependency evidence and pre-existing metadata, then
  returns `TYPE_UNKNOWN` for unresolved quiet staging. The retired
  compatibility-family recorder was deleted and the resolver inventory smoke
  rejects reintroducing the recorder or stage-signature materializer fallback.
- 2026-04-30 DAG diagnostic read-only tightening:
  metadata diagnostics now resolve generic arguments through
  `semantic_type_resolution_lookup_metadata_type_ref(...)`, not the
  materializing type-ref helper. The resolver inventory smoke rejects
  reintroducing materializer lookup in metadata diagnostics, keeping diagnostic
  code read-only with respect to DAG resolved-type fact creation.
- 2026-04-30 DAG fallback seam zero cap:
  `type-resolution-resolver-inventory-test-smoke` now reports and gates active
  fallback seams at `0` (`fallback seams=0 cap=0`). Any new semantic owner that
  wants to consume a materializing DAG seam must update the resolver inventory
  gate deliberately instead of expanding the seam invisibly.
- 2026-04-30 MIR CFG owner split:
  `mir_cfg_contract_validate.h` moved cleanup-edge fact lookup into
  `mir_cfg_contract_cleanup_fact.h`, reducing the validator owner to 584 LOC.
  The largest production owners are now below the 600 LOC split-review
  threshold; local gates: `make test-mir` and
  `make cfg-body-dataflow-test-smoke`.
- 2026-04-28 semantic owner update: function declaration and host-helper
  implementation-header debt is closed. `type_checker_func_decl.c`,
  `type_checker_func_action_contract.c`, and `type_checker_host_helpers.c`
  replace the old implementation bodies in `type_checker_program.h` /
  `type_checker_host_helpers.h`; the semantic shape gate tracks the new
  owners under the 600 LOC review threshold.
- 2026-04-28 LLVM owner update: `llvm_intent.c` and `llvm_domain.c` are now
  below the 600 LOC review threshold. Intent setup/context/cleanup ownership
  lives in `llvm_intent_setup.c`, `llvm_intent_step_context.c`, and
  `llvm_intent_cleanup.c`; domain forward declarations and struct-field
  helpers live in `llvm_domain_forward.c` and
  `llvm_domain_struct_fields.c`. Local gate: `make llvm-test-smoke`.
- 2026-04-28 C backend owner update: `transpiler.c` is now below the 600 LOC
  review threshold. Public entry/result lifecycle moved to
  `transpiler_entry.c`, runtime thread-pool requirement scanning moved to
  `transpiler_thread_pool.c`, and small declaration stubs moved to
  `transpiler_misc_decl.c`. Parity gate: `make llvm-test-backend-compare`
  (`196/0` ABI same-process, `64/64` backend compare).
- 2026-04-28 C projection overlay owner update:
  Overlay projection invalidation now lives in
  `transpiler_overlay_projection.c`; `transpiler_overlay_projection.h` and
  `transpiler_overlay_world_projection.h` are declaration-only. Hosted-method
  projection invalidation traversal lives in
  `transpiler_projection_method_invalidation.c`. Host-field / self-cell probes
  live in `transpiler_overlay_host_fields.c`, while zone effect and relation
  bind-layer emission live in compiled owners
  `transpiler_overlay_zone_bind.c` and
  `transpiler_overlay_zone_relation_bind.c`. Parity gate:
  `make llvm-test-backend-compare` (`196/0` ABI same-process, `64/64`
  backend compare).
- 2026-04-28 LLVM zone sync owner update:
  `llvm_domain_zone_sync.c` is now below the 600 LOC review threshold.
  Relation clause lowering (`link`, maintained relation, and `unlink`) lives
  in `llvm_domain_zone_sync_relations.c`, leaving zone sync orchestration and
  effect/state clause lowering in the main owner. Gates: `make pgy`,
  `make llvm-test-smoke`, and `make llvm-test-backend-compare` (`196/0`
  ABI same-process, `64/64` backend compare).
- 2026-04-28 LLVM world sync owner update:
  `llvm_domain_world_sync.c` is now below the 600 LOC review threshold.
  World command directive lowering and world state/zone-slot lookup helpers
  live in `llvm_domain_world_sync_directives.c` behind
  `llvm_domain_world_sync_internal.h`. Gates: `make pgy`,
  `make llvm-test-smoke`, and `make llvm-test-backend-compare` (`196/0`
  ABI same-process, `64/64` backend compare).
- 2026-04-29 LLVM world frontier owner update:
  bounded world frontier scheduling has a dedicated owner,
  `llvm_domain_world_frontier.c`. The main `llvm_domain_world_sync.c` now keeps
  sync orchestration only, while the frontier owner keeps
  `pgy_frontier_world_transitive_pass_limit(...)`, zone-generation dirty
  detection, derived-state recompute, and overflow abort emission. Gate:
  `make runtime-frontier-contract-test-smoke`; local sanity gate:
  `make llvm-test-smoke`.
- 2026-04-30 LLVM frontier overflow helper update:
  world derived overflow, transitive world frontier overflow, zone overflow,
  and projection-chain overflow consume the shared
  `llvm_emit_frontier_overflow_abort(...)` helper. The runtime-frontier
  contract smoke now includes the helper owner in the LLVM world/zone and
  projection contract bundles, so bounded-fixpoint hard-fail behavior is
  source-of-truth checked at one LLVM seam.
- 2026-04-29 CFG/MIR correction:
  `parallel { ... }` is not classified as CFG-owned until HIR/MIR has a real
  parallel CFG lowering. It remains AIR-visible and semantic-flow checked, but
  MIR DCE must preserve it as a side-effecting statement. This prevents channel
  sends inside `parallel` from disappearing before LLVM select/channel tests.
  `make cfg-body-dataflow-test-smoke` now gates this distinction directly with
  a parallel-send/select MIR preservation fixture.
- 2026-04-29 CFG loop fixed-point correction:
  resource snapshot equality now compares `used_states` in addition to
  consumed/released state. Loop convergence can no longer ignore borrow/use
  facts while still treating ownership facts as stable. Gate:
  `make cfg-body-dataflow-test-smoke`.
- 2026-04-29 C MIR parallel residual emission correction:
  resource hooks for `parallel`/`async`/`spawn`/`await` are treated as
  observability hooks only; they do not mirror or replace the executable
  residual statement. This fixes the C backend `parallel_channel_sum` hang
  where receives were emitted without the send task body. Backend compare also
  has a generated-executable timeout guard via
  `PGY_BACKEND_COMPARE_RUN_TIMEOUT_SECONDS`. Gate:
  `make llvm-test-backend-compare` (`196/0` ABI same-process, `65/65`
  backend compare).
- 2026-04-29 CFG/MIR pin cleanup early-exit gate:
  `src/test_mir.c` now covers a pin-region block whose terminator is
  `HIR_BLOCK_RETURN`. `cfg-body-dataflow-test-smoke` requires that fixture so
  `pin-unpin-cleanup-edge` remains an all-exit fact, not only a fallthrough
  convention.
- 2026-04-29 CFG/MIR pin cleanup branch-return gate:
  `PinBranchReturns` covers terminating `if`/`else` arms inside a pin region.
  Both arms must keep cleanup successor routing and the
  `pin-unpin-cleanup-edge` fact. This is still narrower than full branch/join
  ownership closure, but it locks another concrete all-exit cleanup case.
- 2026-04-29 CFG/MIR pin cleanup loop-control gate:
  `PinLoopControl` covers `break` and `continue` lowered as `HIR_BLOCK_GOTO`
  inside a pin region. Those loop-control exits must keep cleanup successor
  routing and the `pin-unpin-cleanup-edge` fact before the broader loop
  ownership/lifetime lattice is considered closed.
- 2026-04-30 MIR cleanup fact gate:
  `test_mir` now corrupts rollback and invalidation cleanup fact names and
  requires the MIR validator to reject both. Cleanup topology fields are not
  sufficient beta evidence unless the named MIR cleanup fact inventory is
  preserved.
- 2026-05-04 cleanup fact vocabulary gate:
  `src/compiler/mir_cleanup_fact_names.h` now owns the cleanup-edge,
  rollback/invalidation cleanup-edge, `pin-unpin-cleanup-edge`, cleanup anchor,
  and read/write pin cleanup labels. MIR cleanup generation, MIR validation,
  AIR evidence collection, and C emission contract validation consume the same
  constants, so cleanup evidence can no longer drift by duplicating literals in
  separate consumers.
- 2026-04-29 AIR inspection update:
  `pgy --air <source.pgy>` dumps the AIR verification summary after evidence
  collection and before drift failure. AIR remains verification-only and is not
  carried in `CompilerIRBundle`, but reviewers can now inspect intent,
  boundary, evidence, and drift state without reading unit-test internals.
- 2026-04-29 runtime LLVM export owner update:
  `pgy_runtime_lib_slot_array_io_string_exports.h` is now only an 8 LOC stable
  include facade. Secure-slot, device-slot, array/map, and IO/string exports
  live in separate runtime owners at 161/84/239/296 LOC without changing the
  ABI symbol names or `pgy_runtime_lib.c` include seam. The runtime object cache
  freshness list tracks the new leaf owners directly. Gates: `make pgy`,
  `make test-abi`, `make production-header-size-test-smoke`, and
  `make backend-inc-size-test-smoke`.
- 2026-04-28 LLVM runtime registry owner update:
  `llvm_runtime.c` is now below the 600 LOC review threshold. Raw collection
  export declarations live in `llvm_runtime_raw_collections.c`; channel export
  declarations live in `llvm_runtime_channels.c` behind
  `llvm_runtime_internal.h`. Gates: `make pgy`, `make llvm-test-smoke`, and
  `make llvm-test-backend-compare` (`196/0` ABI same-process, `64/64`
  backend compare).
- 2026-04-28 LLVM expression projection helper owner update:
  `llvm_expr_boundary_projection_helpers.h` is now below the 600 LOC review
  threshold. Projection nominal lookup, nested vessel path resolution,
  projection-path value loading, and `ProjectSubject` emission live in
  `llvm_expr_projection_path_helpers.h`. Gates: `make pgy`,
  `make llvm-test-smoke`, and `make llvm-test-backend-compare` (`196/0`
  ABI same-process, `64/64` backend compare).
- 2026-04-28 LLVM spawn/call helper owner update:
  `llvm_expr_host_spawn_literal_helpers.h` is now below the 600 LOC review
  threshold. Await-task result materialization, direct function-call argument
  emission, generic callee monomorphization, and spawn-expression wrapper
  lowering live in `llvm_expr_spawn_call_helpers.h`. Gates: `make pgy`,
  `make llvm-test-smoke`, and `make llvm-test-backend-compare` (`196/0`
  ABI same-process, `64/64` backend compare).
- 2026-04-29 C declaration lookup owner update:
  `transpiler_decl_lookup.c` is now below the 600 LOC review threshold.
  Current-host, owner-host, and nominal-host declaration lookup live in
  `transpiler_decl_host_lookup.c`; non-MIR host-method AST lookup is retired
  and hosted methods consume MIR metadata. The original owner keeps named
  declaration, alias, inventory, and method-list lookup. Gates: `make pgy`,
  `make test-transpile`, `make production-header-size-test-smoke`,
  `make backend-inc-size-test-smoke`, and `make llvm-test-backend-compare`
  (`196/0` ABI same-process, `65/65` backend compare).
- 2026-04-29 C type mapping owner update:
  `transpiler_type_mapping_helpers.h` is now below the 600 LOC review
  threshold. AST type-name rendering lives in
  `transpiler_type_render_helpers.h`; the original owner keeps primitive,
  collection, slot, result, and suffix mapping. Gates: `make pgy`,
  `make test-transpile`, `make production-header-size-test-smoke`,
  `make backend-inc-size-test-smoke`, and `make llvm-test-backend-compare`
  (`196/0` ABI same-process, `65/65` backend compare).
- 2026-04-29 CFG contract validator owner update:
  `mir_cfg_contract_validate.h` is now below the 600 LOC review threshold.
  CFG-owned AST control classification lives in
  `mir_cfg_contract_control.h`; pin cleanup edge validation lives in
  `mir_cfg_contract_pin.h`. The original owner keeps cleanup, successor, and
  predecessor contract validation. Gates: `make test-mir`,
  `make cfg-body-dataflow-test-smoke`, `make abi-ownership-shape-test-smoke`,
  `make production-header-size-test-smoke`, and
  `make backend-inc-size-test-smoke`.
- 2026-05-19 CFG cleanup validator owner update:
  `mir_cfg_contract_validate.c` is now 334 LOC and keeps non-cleanup CFG
  validation. `mir_cfg_contract_validate_cleanup.c` is 245 LOC and owns
  cleanup-block shape, reachable cleanup-edge facts, rollback/invalidation
  target checks, and cleanup convergence. Gates: `make test-mir`,
  `make cfg-body-dataflow-test-smoke`, `make build-source-inventory-test-smoke`,
  `make test-inc-size-test-smoke`, and `make abi-ownership-shape-test-smoke`.
- 2026-05-19 LLVM declaration authority owner update:
  zone-authority declaration prelude emission now lives in
  `llvm_decl_authority.c`. Function routine inventory orchestration now lives
  in `llvm_decl_routines.c`. `llvm_decl.c` is 278 LOC and keeps function
  declaration/body emission; the authority owner is 152 LOC and now resolves
  the current zone through MIR routine owner metadata plus
  `MIRDeclZoneAuthority` declaration-header rows before emitting the
  `pgy_zone_authority_check_export` call and structured inventory-missing
  diagnostics. `mir_decl_header_authority.c` is the 121 LOC lowering owner for
  authority subject slots and required ability refs; `mir_ability_ref.c` is the
  shared 81 LOC ability-ref capture owner. The routine owner is 106 LOC and
  owns generic-template dispatch, non-generic MIR routine emission, and
  residual missing-routine diagnostics. Gates:
  `make mir-declaration-inventory-test-smoke`,
  `make semantic-core-shape-test-smoke`,
  `make build-source-inventory-test-smoke`, `make test-inc-size-test-smoke`,
  and `make perf-contract-test-smoke`.
- 2026-04-29 MIR SSA/local type owner update:
  `transpiler_mir_ssa_names.h` is now below the 600 LOC review threshold.
  AST body local type lookup and expression fallback inference live in
  `transpiler_mir_local_type_lookup.c`; the original owner keeps SSA name
  resolution, SSA map setup, claim-shape predicates, and implicit-field
  rendering. Gates: `make pgy`, `make test-mir`,
  `make cfg-body-dataflow-test-smoke`, `make test-transpile`,
  `make production-header-size-test-smoke`, `make backend-inc-size-test-smoke`,
  and `make llvm-test-backend-compare` (`196/0` ABI same-process,
  `65/65` backend compare).
- 2026-04-29 C let slot owner update:
  `transpiler_let_emit.c` no longer owns Slot/DeviceSlot claims,
  ReadView/WriteView/MoveToken declarations, or Slot/SecureSlot sugar
  lowering directly. Those paths now live in the compiled owner
  `transpiler_let_slot_emit.c`, while `transpiler_let_slot_emit.h` is
  declaration-only. The let-declaration owner family remains below the 600 LOC
  split-review threshold without reintroducing `.inc` files. Latest focused
  gates: `make pgy`, `make test-transpile`,
  `make build-source-inventory-test-smoke`, `make test-inc-size-test-smoke`,
  `make memory-string-safety-test-smoke`, and `make perf-contract-test-smoke`.
- 2026-05-19 C zone struct owner update:
  `transpiler_zone_struct_emit.h` no longer owns generated zone struct fields
  or layer accessor bodies. Those paths now live in the compiled owner
  `transpiler_zone_struct_emit.c`, while the header is declaration-only and
  covered by the implementation-header guardrail. Focused gates:
  `make pgy`, `make test-transpile`,
  `make build-source-inventory-test-smoke`, `make test-inc-size-test-smoke`,
  `make memory-string-safety-test-smoke`, and
  `make semantic-core-shape-test-smoke`.
- 2026-05-19 C MIR match condition owner update:
  `transpiler_mir_match_condition_emit.h` no longer owns Option/Result
  destructor pattern conditions, payload binding, or match guard composition.
  Those paths now live in the compiled owner
  `transpiler_mir_match_condition_emit.c`, while CFG control lowering consumes
  only the public condition-rendering API. Focused gates: `make pgy`,
  `make test-transpile`, `make build-source-inventory-test-smoke`,
  `make test-inc-size-test-smoke`, `make perf-contract-test-smoke`, and
  `make semantic-core-shape-test-smoke`.
- 2026-04-29 C domain provenance owner update:
  projection-chain bounded recompute and hidden epoch/cause field stamping now
  live in `transpiler_domain_provenance_emit.h`. Role/ability lowering remains
  in `transpiler_domain_role_ability_emit.h`. Current sizes are 237 LOC and
  452 LOC, so this mixed propagation/role owner is below the 600 LOC
  split-review threshold. Gates: `make pgy`, `make test-transpile`, and
  `make runtime-frontier-contract-test-smoke`; parity gate:
  `make llvm-test-backend-compare` (`196/0` ABI same-process, `65/65`
  backend compare).
- 2026-05-23 C hosted-method body owner update:
  shared domain hosted-method MIR body lowering now lives in the compiled owner
  `transpiler_hosted_method_body_emit.c`. Party, roster, relation, effect,
  zone, and world hosted-method consumers no longer depend on a role/ability
  implementation-header helper for MIR routine emission. Focused gates:
  `perf-contract-test-smoke`, `mir-declaration-inventory-test-smoke`,
  `memory-string-safety-test-smoke`, `build-source-inventory-test-smoke`, and
  `test-inc-size-test-smoke`.
- 2026-05-23 C relation/effect owner update:
  relation/effect declaration emission now lives in
  `transpiler_relation_effect_emit.c`; the header is declaration-only.
  Projection sync, provenance field stamping, hosted-method forwarding, and
  hosted-method MIR body calls are consumed through explicit owner APIs.
  Focused gates: `test-transpile`, `perf-contract-test-smoke`,
  `mir-declaration-inventory-test-smoke`, `memory-string-safety-test-smoke`,
  `semantic-core-shape-test-smoke`, `build-source-inventory-test-smoke`, and
  `test-inc-size-test-smoke`.
- 2026-05-23 C zone hosted-method owner update:
  zone hosted-method forwarding/body emission now lives in
  `transpiler_zone_methods_emit.c`; the header is declaration-only and
  `transpiler.c` no longer contains a late bridge function for this path.
  Focused gates: standalone zone-method owner compile, zone-declaration owner
  compile, and `transpiler.o` compile.
- 2026-05-23 C ability-vtable owner update:
  ability-vtable specialization tag rendering, typedef naming, and generic
  ability vtable declaration emission now live in
  `transpiler_domain_role_ability_emit.c`; the header is declaration-only.
  Focused gates: `test-transpile`, `perf-contract-test-smoke`,
  `mir-declaration-inventory-test-smoke`, `memory-string-safety-test-smoke`,
  `semantic-core-shape-test-smoke`, `runtime-frontier-contract-test-smoke`,
  `build-source-inventory-test-smoke`, and `test-inc-size-test-smoke`.
- 2026-05-23 C nominal-domain owner update:
  ability, role, and party declaration emission now live in
  `transpiler_domain_nominal_emit.c`; the header is declaration-only. The
  domain-role shim includes roster and relation/effect seams directly, so
  nominal emission no longer acts as an implementation include router.
  Focused gates: `test-transpile`, `perf-contract-test-smoke`,
  `mir-declaration-inventory-test-smoke`, `memory-string-safety-test-smoke`,
  `semantic-core-shape-test-smoke`, `runtime-frontier-contract-test-smoke`,
  `build-source-inventory-test-smoke`, and `test-inc-size-test-smoke`.
- 2026-04-29 C class declaration owner update:
  non-generic class declaration lowering now lives in the compiled owner
  `transpiler_class_decl_emit.c`. `transpiler_func_class_flow_emit.h` keeps
  function fallback, generic class specialization, with-slot, and return
  lowering. Function fallback policy helpers now live in
  `transpiler_func_flow_policy.c`, and the function-flow shim is now 370 LOC.
  The class owner is 138 LOC. Gates:
  `make pgy`, `make test-transpile`, `make production-header-size-test-smoke`,
  `make backend-inc-size-test-smoke`, and `make llvm-test-backend-compare`
  (`196/0` ABI same-process, `65/65` backend compare).
- 2026-04-29 C MIR block owner update:
  small MIR emission predicate wrappers now live in
  `transpiler_mir_emit_predicates.h`. The current block statement emission
  owner is `transpiler_mir_block_emit.c`; `transpiler_mir_block_emit.h` is a
  declaration-only seam. Gates:
  `make test-mir`, `make cfg-body-dataflow-test-smoke`,
  `make production-header-size-test-smoke`, and
  `make backend-inc-size-test-smoke`.
- 2026-04-29 CFG consumer update:
  MIR statement population no longer preserves HIR-expanded control
  containers (`if`, `while`, `for`, `select`, `match`, `break`, `continue`) as
  fallback `MIR_INST_STMT` instructions when a block already has CFG successor
  edges. `for` preheader initialization is now a dedicated
  `MIR_INST_LOOP_INIT` fact consumed by C and LLVM. For-loop condition and
  backedge emission now consume the header `MIR_INST_BRANCH` metadata instead
  of re-reading `target->source_ast`. The loop variable and start/end
  expressions are carried on MIR instructions (`arg0`, `expr0`, `expr1`) and
  validated by `mir_validate()`. `mir_validate()` rejects CFG-owned control
  statements that reappear as fallback STMTs, so C/LLVM backends cannot silently
  mix MIR CFG edges with AST control-flow emission. `for value in List<T>` is
  now on the same contract: MIR owns the loop index, list-size condition,
  list-get body binding, and backedge increment in both C and LLVM. Gates:
  `make test-mir`, `make cfg-body-dataflow-test-smoke`, and
  `make llvm-test-backend-compare` (`196/0` ABI same-process, `65/65` backend
  compare). MIR DCE also consumes `mir_stmt_ast_is_cfg_owned_control(...)`
  instead of a private CFG-control AST switch, so statement population,
  validation, and DCE share one classifier.
- 2026-04-29 CFG/AIR handoff update:
  `with`, `parallel`, `unsafe`, and `defer` are now part of the CFG-owned
  boundary set when a MIR block already has successor edges. MIR statement
  population skips these boundary containers in expanded CFG blocks, and
  `mir_validate()` rejects them if they reappear as fallback `MIR_INST_STMT`
  instructions. This is the handoff point for the next AIR sprint: AIR should
  consume the boundary facts produced by CFG lowering, not duplicated AST
  fallback body containers.
- 2026-04-29 AIR execution-boundary update:
  AIR now has an explicit `execution` boundary kind for `with`, `unsafe`,
  `defer`, and pin-block AST metadata. These are sync body/execution boundaries
  and strict evidence checks HIR/CFG evidence for them, not RIR
  resource-boundary evidence. The AIR walker also descends into `with` bodies,
  so nested IO/time boundaries inside `with` are not hidden by the execution
  container. This narrows the previous abstraction-boundary gap: AIR can
  distinguish execution boundaries from ordinary AST syntax while leaving
  zone/world/parallel/channel and IO evidence rules unchanged.
- 2026-04-29 AIR await-boundary update:
  `await` is now synthesized as a stable AIR `parallel` boundary source instead
  of being only recursively scanned through its operand. Strict AIR accepts RIR
  evidence only from the exact `AwaitLocal` or `AwaitRemote` operation attached
  to the same AST boundary; a generic scope named `await` is rejected. It still
  requires HIR/CFG evidence for the implementation boundary.
  The AIR boundary AST walk now lives in `src/compiler/air_boundary_walk.c`,
  leaving `src/compiler/air_boundary.c` focused on boundary taxonomy/policy.
- 2026-04-29 AIR task-group boundary update:
  `AST_TASK_GROUP` is now synthesized as a stable AIR `parallel` boundary source
  named `task-group`. Strict AIR now requires both HIR/CFG evidence and matching
  same-AST RIR operation evidence for every stable parallel boundary. RIR
  materializes `AwaitLocal`, `AwaitRemote`, `Spawn`, `Async`, `Parallel`, and
  `TaskGroup`, so local grouped-task orchestration is no longer a HIR-only
  exception.
- 2026-04-29 AIR world-transfer evidence update:
  world handoff evidence is now same-AST specific when the AIR boundary has
  source provenance. A matching RIR `Move` / `Claim` must carry the same AST as
  the world boundary; an unrelated same-alias transfer op in the same RIR scope
  no longer satisfies strict evidence. Gate: `make test-air` and
  `make air-drift-test-smoke`.
- 2026-04-29 AIR channel evidence update:
  RIR now materializes `ChannelSend`, `ChannelRecv`, and `ChannelSelect` ops for
  channel AST boundaries. AIR channel strict evidence consumes those exact
  same-AST ops instead of treating a same-owner/same-name RIR scope as enough.
  This keeps channel evidence aligned with the already tightened `await`
  `AwaitLocal` / `AwaitRemote` policy. `make test-rir` now gates parsed-source
  channel send, receive, and select lowering into the same operations.
- 2026-04-29 AIR IO evidence update:
  RIR now materializes beta-stable IO calls as `IO` ops, and AIR IO strict
  evidence consumes only a matching source/provenance op. The parsed-source
  `ReadFile` fixture no longer remains a deliberate missing-evidence negative;
  it is a positive exact-evidence test. Builtin call source spans now reach
  `AST_CALL`, so the common parsed path no longer depends on step-level span
  fallback.
- 2026-04-29 AIR HIR evidence containment update:
  HIR CFG evidence now accepts nested boundary ASTs inside CFG-carried
  statements and terminator values, not only direct statement-pointer equality.
  This closes the execution-boundary seam where `with { ReadFile(...) }` could
  have RIR IO evidence but still miss HIR CFG evidence. The containment matcher
  now mirrors the AIR boundary walk across loops, parallel/async/task-group,
  spawn/call/assignment, arrays/tuples, await/channel/select, match, unsafe,
  defer, event invoke, and lambda bodies; a loop-condition `ReadFile` fixture
  locks the body-control case.
- 2026-04-30 AIR initializer-boundary update:
  AIR boundary walking and HIR evidence containment now descend into
  `AST_LET_DECL` and `AST_LET_DESTRUCTURE` initializers. This closes the seam
  where an implementation boundary hidden behind `let x = ReadFile(...)` inside
  an intent-step block was ordinary syntax to AIR instead of an abstraction
  boundary. Gate: `make test-air` with the `AIR synthesis captures boundary
  from let initializer` regression and `make air-drift-test-smoke`.
- 2026-04-29 HIR intent CFG evidence update:
  parsed-source intent routines now get a minimal ordered clause CFG from
  `src/compiler/hir_lower_intent_cfg.c`. `hir_lower_cfg.c` remains focused on
  function-body CFG at 598 LOC; the intent owner is 184 LOC and materializes
  priority/success/failure expressions plus each intent step's `where`,
  `using`, `intent`, contract, `on`, and `compensate` clauses as HIR CFG
  statements. This is an AIR evidence closure, not a runtime scheduler:
  strict AIR can now require HIR CFG evidence for parsed-source intent
  boundaries without accepting routine-only provenance. MIR population also
  preserves intent `MIR_INST_STMT` semantic carriers after CFG statement
  reconstruction, so participant/zone/authority/causes metadata remains MIR
  inventory instead of being treated as disposable AST fallback emission.
- 2026-05-24 DAG compatibility inventory update:
  type-resolution DAG fallback remains closed (`metadata_dead_ends=0`,
  alias/non-alias stage metadata materialization 0). The retired recursive
  resolver no longer exports zero-only call/cache counters; the inventory smoke
  keeps only a quarantine owner and rejects legacy resolver bodies or counters.
  This makes the next DAG cleanup target explicit: the recursive resolver is
  gone from the beta path, and the remaining counters are audit-only debt
  detectors. The resolver is also no longer exposed from public
  `type_checker.h`; the resolver inventory smoke rejects public header
  re-exposure and semantic regression tests that call it directly.
- 2026-04-29 CFG-owned control classifier update:
  `mir_cfg_contract_control.h` now has a real header guard and is consumed by
  both MIR statement population and MIR CFG validation. This removes the
  duplicated CFG-owned control list from `mir_stmt_population.h`, so fallback
  `MIR_INST_STMT` filtering and validator rejection use the same source of
  truth.
- 2026-04-29 ABI ownership gate update:
  `make abi-ownership-shape-test-smoke` now gates the implemented Slot/Pin ABI
  shape, runtime pin generation/thread/token invariants, C/LLVM pin/unpin
  lowering, MIR cleanup evidence, backend compare pin fixtures, and the
  Zone-Bound Handle docs contract. This does not claim non-pin handle lifetime
  is fully solved; it keeps the implemented ABI subset and the missing
  first-class Zone-Bound Handle piece in one visible gate.
- 2026-04-29 MIR declaration inventory smoke update:
  `make mir-declaration-inventory-test-smoke` is shell-only. It still rejects
  raw MIR declaration/routine inventory access outside helper owners and keeps
  MIR method metadata accessor requirements in the beta gate, but no longer
  needs Python on CI runners.
- 2026-04-29 Runtime ABI lifetime smoke update:
  `make runtime-abi-lifetime-test-smoke` is shell-only. It keeps the borrowed
  runtime string, result-owned string/array, runtime-owned file-handle, macro
  export, and ownership proof-doc checks while removing the last Python
  dependency from this ABI lifetime gate.
- Beta closure now follows the lean sprint loop in
  `docs/71_beta_execution_tickets.md`: close one implementation debt slice
  first, run the slice-local gate, then run wider regression at the slice or
  sprint boundary.
- Full regression is still mandatory before declaring a blocker closed, but it
  is not the inner edit loop. The inner loop should remove source-of-truth
  duplication, fallback seams, or owner-boundary debt.
- A test-only tightening sprint is not beta progress unless it also removes or
  constrains the underlying implementation debt.
- Production owner size is part of beta readability, not style polish:
  600 LOC is the split-review threshold for production `.c` and private owner
  `.h` files. 1,000 LOC remains the hard stop / risk line, but files between
  600 and 1,000 LOC still need a named owner-seam plan unless they are compact
  generated tables, ABI declarations, or single-purpose orchestration layers.
- 2026-05-02 split application guide: this is not a rule change. 600 LOC
  remains the signal, not the prescription. The checklist is:
  "two responsibilities?" -> split by responsibility; "one responsibility but
  large?" -> keep one owner and improve internal structure; "new owner name
  expresses the responsibility?" -> land only if yes. New `_helpers` owners are
  forbidden by default because `_helpers` does not name a responsibility;
  exceptions require a documented cross-owner shared utility caller set. The
  larger recovery path is self-host feature modules, not a risky pre-beta
  feature-folder migration.
- Current owner-size baseline: production `.inc` debt under `src/` is closed,
  but production `.c` and private owner `.h` files are not yet all below the
  600 LOC split-review threshold. The remaining 600-1,000 LOC review-band
  queue includes backend/tooling owners such as `pgy_lsp.c`, C expression
  emitters, and runtime/tooling headers.
  LLVM intent/domain declaration owners are now below the threshold after the
  setup/context/cleanup and forward/struct-field splits, and `transpiler.c`
  is below the threshold after the entry/thread-pool/misc-decl split.
  Overlay projection is also owner-backed after the host-field, zone-bind, and
  projection-invalidation splits, and `llvm_domain_zone_sync.c` is below
  the threshold after the relation-clause split. `llvm_domain_world_sync.c`
  is below the threshold after the directive-pass split. `llvm_runtime.c` is
  below the threshold after the raw collection/channel registry split, and
  `llvm_expr_boundary_projection_helpers.h` is below the threshold after the
  projection-path helper split. `llvm_expr_host_spawn_literal_helpers.h` is
  below the threshold after the spawn/call helper split. This is no longer `.inc` debt, but it is still
  beta readability debt. `llvm_internal.h` has moved below the
  threshold by splitting private API declarations into `llvm_internal_api.h`,
  fixed limits / dynamic-array helpers into `llvm_limits_internal.h`, and
  LLVM developer-trace env reads into `llvm_debug_flags.c`. The private API
  helper `llvm_ast_type_uses_pointer_self(...)` now lives in
  `llvm_domain_lookup.c`, so `llvm_internal_api.h` is declaration-only and
  included in the header body-free gate. The
  LLVM registry owner is also below the threshold after splitting resource/type
  registry behavior into `llvm_registry_resources.c`. The world semantic owner
  family is below the threshold after moving lookup/
  lifecycle helpers to `type_checker_world_helpers.c` and shared domain slot
  validation to `type_checker_domain_slots.c`. The
  AST print/constructor/type split and
  semantic domain contract / constructor-call / intent-transfer /
  intent-action-contract / intent-authority / intent-participant /
  ownership-constructor-diagnostic splits plus the
  runtime Slot Pin owner split, type-system inference/effect split,
  AST destroy/domain-destroy split, parser declaration/type split, and parser
  statement-dispatch split closed their named owner families. Semantic zone
  declaration ownership is also below the 600 LOC split-review threshold after
  shape/projection/state splits, and lifecycle authority-presence diagnostics
  now live in the zone authority owner instead of the declaration
  orchestration body.
  Expression semantic ownership is below the 600 LOC split-review threshold:
  `type_checker_expr.c` owns expression/member dispatch, `type_checker_expr_call.c`
  owns call dispatch and slot/host call behavior, and
  `type_checker_expr_host.c` owns nominal host field/method lookup through
  explicit `expr_*` seams.
  Stdlib builtin semantic ownership is also below the threshold after moving
  `List` / `Set` / `Queue` / `Array` typing into
  `type_checker_builtins_stdlib_collections.c`; the body dispatcher now
  delegates scalar, map, and collection families through focused owner seams.
  HIR
  construction/destruction owners are split into `hir.c`, `hir_routines.c`, and
  `hir_destroy.c`, while compiler driver/result/LLVM/runtime-cache ownership is
  split into `compiler.c`, `compiler_result.c`, `compiler_llvm.c`,
  `compiler_toolchain.c`, and `compiler_runtime_cache.c`. Driver pipeline
  ownership is also split so `driver_app.c` owns orchestration and
  `driver_diag.c` owns JSON diagnostic routing / AIR drift diagnostic wording.
  Module normalization is split so `module_normalizer.c` owns module-level
  orchestration / namespace shells / export scanning, while
  `module_normalizer_refs.c` owns rename-scope, shadow-name, type/generic/call,
  and AST-reference rewriting behind `module_normalizer_internal.h`. Scaffold
  ownership is split so `driver_scaffold.c` owns filesystem helpers,
  single-file scaffold templates, and command dispatch, while
  `driver_scaffold_project.c` owns simulator/project directory templates behind
  `driver_scaffold_internal.h`. RIR builder lowering is no longer carried by an
  implementation-style header: `rir_builder.c` owns general RIR lowering,
  `rir_builder_intent.c` owns intent-scope collection, `rir_facts.c` owns RIR
  fact/utility materialization, `rir_names.c` owns RIR vocabulary names,
  `rir_public_surface.c` owns RIR dump/destroy public-surface behavior, and
  `rir_validation.c` owns RIR validation / DIR contract checks, and
  `rir_flow.c` owns HIR-backed RIR flow enrichment.
  `rir_internal.h` declares the shared private seam. `rir.c` is now below the
  600 LOC split-review threshold, so the active compiler-owner queue has moved
  from these closed owner families to the next remaining source-of-truth seams:
  CFG consumers, AIR boundary consumers, DAG evaluator fallback seams, and
  MIR/LLVM declaration bootstrap parity.
- 2026-05-15 type-system slot owner split: `src/semantic/type_system_slot.c`
  now owns Slot/SecureSlot/View/MoveToken type construction and slot type
  accessors. `src/semantic/type_system.c` is 519 LOC and the new slot owner is
  110 LOC, so the type-system family is back below the 600 LOC split-review
  threshold without hiding raw `Type->data.slot` access in non-type-system
  owners. Gates: `test-inc-size-test-smoke`, `semantic-core-shape-test-smoke`,
  `build-source-inventory-test-smoke`, and `test-semantic`.
- 2026-05-15 DAG domain-stage owner split: local world/zone contract scans stay
  in `type_checker_resolution_stage_domain.c`, while world/zone label replay
  moved to `type_checker_resolution_stage_domain_label.c`. The old 591 LOC
  owner is now split into 264 LOC local-contract and 334 LOC label-replay
  owners without changing the stage API. Gates: direct object build,
  `semantic-core-shape-test-smoke`, `type-resolution-dag-test-smoke`, and
  `test-semantic`.
- 2026-05-15 semantic shape gate runtime fix: `semantic_core_shape_smoke` now
  caches the broad source payload scan for `data.*` / `resolve_type_node(...)`
  checks instead of rescanning `src/semantic`, `src/compiler`, and `src/codegen`
  for every individual pattern. The gate still rejects reopened AST/Type
  payload seams, but no longer burns a full tree walk for each check on
  Windows/Git Bash.
- 2026-05-15 AIR runtime evidence owner split: observability-schema and runtime
  frontier-policy evidence collection moved to
  `src/compiler/air_evidence_runtime.c`. `air_evidence.c` now stays focused on
  HIR/MIR evidence collection at 509 LOC, while `test_air` keeps singleton
  global evidence behavior and diagnostics unchanged.
- 2026-05-16 AIR boundary evidence validator owner split:
  `src/compiler/air_validate_boundary_evidence.c` now owns boundary-scoped
  evidence shape validation and provider/same-boundary matching.
  `air_validate_evidence.c` remains focused on inventory traversal, duplicate
  detection, and count checks. This keeps EvidenceNode inventory as the source
  of truth while preventing boundary policy from growing inside the inventory
  owner. Gates: `test-air`, `air-drift-test-smoke`, and
  `air-json-schema-test-smoke`.
- 2026-05-16 AIR drift storage owner split: `src/compiler/air_drift.c` now owns
  drift allocation, clearing, and formatted append. `air_verify.c` remains the
  strict rule owner instead of managing AIRProgram drift capacity directly.
  Gates: `test-air`, `air-drift-test-smoke`,
  `build-source-inventory-test-smoke`, and `test-inc-size-test-smoke`.
- 2026-05-16 CFG parallel/defer owner split:
  `src/semantic/type_checker_flow_parallel.c` now owns defer cleanup boundary
  checks and parallel task resource joins. The former implementation header is
  removed, keeping CFG body-flow dispatch linked through a semantic owner
  instead of including body code. Gates: `test-semantic` and
  `cfg-body-dataflow-test-smoke`.
- 2026-05-16 type-resolution program-stats owner split:
  `src/semantic/type_checker_program_stats.c` now owns `PGY_TYPE_RES_STATS`
  formatting, duplicate-label counting, in-degree reporting, and DAG evidence
  counter output. `type_checker_program.c` is reduced to top-level semantic
  orchestration plus graph validation/worklist sequencing. Gates:
  `test-semantic` and `type-resolution-dag-test-smoke`.
- LLVM MIR CFG control owner debt is partially closed: CFG-expanded range
  `for`, `select`, and `match` lowering now lives in
  `src/codegen/llvm_mir_cfg_control.c`, and `llvm_mir_block_emit.h` is below
  the 600 LOC threshold. This does not close the wider LLVM owner queue because
  declaration inventory bootstrap still has AST-carried seams and several
  backend emitters remain in the review band.
- LLVM statement ownership is also below the 600 LOC review threshold:
  `llvm_stmt.c` owns statement dispatch, defers, return/if/block emission, and
  expression-statement forwarding; `llvm_stmt_destructure.c` owns tuple and
  array-like let-destructure lowering; `llvm_stmt_select.c` owns select
  readiness and round-robin lowering; `llvm_stmt_loop_match.c` owns while,
  numeric for, and for-in loop lowering; `llvm_stmt_match.c` owns match pattern
  comparison and Option/Result payload binding; `llvm_stmt_zone_action.c` owns
  zone-action effect runtime propagation; `llvm_stmt_type_render.c` owns
  generic type-argument rendering; `llvm_stmt_let_collections.c` owns
  collection/channel/array let specializations; and
  `llvm_stmt_let_callable.c` owns callable/lambda let registration.
- LLVM MIR CFG match destructor parity is closed for the direct ABI probe:
  `llvm_mir_cfg_control.c` handles `Some/None` and `Ok/Err` tag checks and
  payload bindings, so `projection_abi` no longer compares aggregate Option
  values with `icmp` or drops `Some(v)` to `0`.
- C MIR CFG consumer parity for the same frozen surface is closed:
  `transpiler_mir_cfg_control_emit.h` owns range-loop init/header/backedge
  lowering and `Option`/`Result` match-case branch conditions for the C backend.
  Explicit CFG containers no longer fall through to opaque AST statement or
  expression emission, and pin-view SSA values are blocked from escaping a pin
  region through phi copies. The phi-copy owner was split to
  `transpiler_mir_phi_emit.h`; a later owner pass retired the former
  `transpiler_mir_ssa_emit.h` shell entirely after its local-type,
  local-binding, effective-type, signature, expression-SSA, phi, and exit-SSA
  responsibilities moved to concrete owners. MIR terminator emission now lives in
  `transpiler_mir_terminator_emit.c` behind a declaration-only header, and
  residual statement helpers were split to `transpiler_mir_stmt_emit.c` with a
  declaration-only header, so `transpiler_mir_func_emit.c` and
  `transpiler_mir_block_emit.c` are linked owners behind narrow headers. `make
  llvm-test-backend-compare` is green with ABI same-process `196 passed, 0
  failed` and backend compare `64/64 passed, 0 failed`.
- C intent declaration emission is also below the 600 LOC review threshold:
  `transpiler_intent_emit.c` now owns orchestration, while
  `transpiler_intent_prologue_emit.c` owns signature/runtime-entry emission and
  `transpiler_intent_cleanup_emit.c` owns cleanup/rollback/invalidation tail
  emission. `transpiler_intent_emit.h` is declaration-only, and the active MIR
  cleanup eligibility query now goes through the inventory-view seam instead of
  direct `ctx->mir` access. Current orchestration/prologue/cleanup owner sizes
  are 524 / 274 / 292 LOC. Latest local gates: `test-transpile` (`770/0`),
  `build-source-inventory-test-smoke`, `test-inc-size-test-smoke`, and
  `mir-declaration-inventory-test-smoke`.
- C zone declaration emission has also left the implementation-header path.
  `transpiler_zone_decl_emit.c` now owns zone sync, projection readiness,
  bounded frontier recompute, and the MIR hosted-method metadata guard, while
  `transpiler_zone_decl_emit.h` is declaration-only. The zone hosted-method
  body tail still bridges through the existing `transpiler.c` include-order
  chain, which keeps this slice low-risk but leaves a smaller helper-chain debt
  for a later owner extraction. Current zone declaration owner size is 511 LOC.
- Remaining backend debt for this area is no longer C MIR emitter owner size;
  it is the higher-level source-of-truth work: declaration/top-level inventory
  bootstrap, broader CFG/dataflow semantic consumption, and AIR boundary
  consumption.
- MIR declaration inventory has a shared active-read API seam:
  `mir_active_inventory()` and `mir_active_externs()` are the compiler-owned
  mapping from declaration kind to the current `MIRProgram` declaration
  inventory. C `transpiler_active_inventory()` and LLVM
  `llvm_active_inventory()` consume this seam instead of carrying duplicate
  backend-local `ASTNodeType -> mir->...` switches. This tightens the future
  dedicated declaration-IR migration boundary, but it does not close the
  remaining debt that the current inventory payloads are still AST-carried.
  Inventory/query/pass wrappers now live in `src/compiler/mir_public_surface.c`
  rather than the MIR lowering implementation header, and `mir_lower(...)`
  now lives directly in `src/compiler/mir.c`.
  Gate: `make mir-declaration-inventory-test-smoke`.
- Intent helper ownership is now also split below the 600 LOC review threshold:
  `type_checker_intent_helpers.c` owns condition/involves/projection-adjacent
  utilities, `type_checker_intent_action_contract.c` owns action-contract
  inheritance and redundant-step warnings, and
  `type_checker_intent_contract_summary.c` owns contract-source summary
  formatting.
- Ownership escape diagnostic ownership is split below the 600 LOC review
  threshold: `type_checker_ownership_diag.c` owns the shared borrow/escape
  diagnostic family and `type_checker_ownership_diag_constructor.c` owns the
  constructor-field escape path.
- Function-call late-helper ownership is split below the 600 LOC review
  threshold: `type_checker_helpers_late.c` owns callable dispatch, argument
  ownership flow, and return materialization; `type_checker_slot_view_active.c`
  owns active slot-view discovery and owner-escape rejection;
  `type_checker_call_contract_helpers.c` owns callee parameter contract /
  escape-summary lookup; and `type_checker_call_generic_where.c` owns call-site
  generic where-clause validation.
- Intent authority/participant ownership is split below the 600 LOC review
  threshold: `type_checker_intent_decl.c` keeps intent declaration
  orchestration, `type_checker_intent_authority.c` owns missing `authorized by`
  diagnostics and authorized participant-to-zone-authority resolution, and
  `type_checker_intent_participants.c` owns `who` participant validation plus
  zone/transfer subject-slot matching. Current sizes are 504 LOC, 242 LOC, and
  115 LOC respectively.
- CFG body-flow effect diagnostics are split into a real implementation owner:
  `type_checker_flow.c` owns body-flow orchestration and CFG fact consumption,
  `type_checker_flow_effects.c` owns branch-effect conflict,
  unreachable-statement, and effect-delta merge diagnostics, and
  `type_checker_flow_effects.h` is declaration-only. Loop-control validation
  and `break` / `continue` resource snapshot recording are also split into
  `type_checker_flow_loop_control.c`, keeping the statement dispatcher from
  owning loop-label diagnostics. This removes another implementation-style
  private-header seam from the body-safety path. Branch/join flow policy is
  now split as well: `type_checker_flow_branch.c` owns `if`/`match` branch
  snapshots, effect joins, dynamic-defer rejection, and match subject
  beta-surface checks, while `type_checker_flow.c` keeps the recursive
  dispatcher, block sequencing, with-scope flow, namespace flow, and public
  body-flow summaries. Current local gate: `test-semantic` (`2532/0`).
- HIR CFG ownership is split below the 600 LOC review threshold:
  `hir_cfg.c` owns predecessor finalization, reachability,
  dominance/frontier, dominator tree, natural loops, and CFG summary
  finalization; `hir_cfg_phi.c` owns local-def collection, SSA-name
  collection, phi-candidate placement, and phi materialization behind the
  private `hir_cfg_internal.h` seam. Current sizes are 388 LOC, 222 LOC, and 8
  LOC respectively.
- 2026-05-11 HIR routine/CFG finish owner update: `hir_routines.c` now owns
  declaration/routine construction and hidden method extraction only, while
  `hir_routine_cfg.c` owns CFG shape/predecessor validation and the ordered CFG
  finish pipeline through `hir_finish_cfg_routine(...)`. Current sizes are 454
  LOC and 133 LOC. Local gate: `test-hir` (`19 passed, 0 failed`).
- 2026-05-11 MIR SSA owner update: `mir_ssa_rename.c` now owns SSA version
  assignment and PHI input materialization, while `mir_ssa_use_edges.c` owns
  versioned use-edge population and block entry/exit value summaries through a
  private `mir_ssa_rename_internal.h` seam. Current sizes are 343 LOC, 300 LOC,
  and 14 LOC. Local gate: `test-mir` (`63 passed, 0 failed`).
- 2026-05-11 LLVM statement type-inference owner update:
  `llvm_stmt_type_infer_nominal.c` owns nominal class-name inference for
  identifiers, calls, and member access, while `llvm_stmt_type_infer.c` owns
  expression type and array element type inference. Current sizes are 98 LOC
  and 490 LOC. Local gate: LLVM-enabled object build for both owners.
- 2026-05-11 LLVM member-call owner update: `llvm_member_call_support.c` owns
  diagnostic recovery, argument vector allocation/storage, and method-name
  mangling, while `llvm_member_call_emit.c` owns concrete member-call dispatch
  and lowering. Current sizes are 94 LOC, 31 LOC, and 479 LOC. Local gate:
  LLVM-enabled object build for `llvm_member_call_emit.o` and
  `llvm_member_call_support.o`.
- 2026-05-11 LLVM scalar expression owner update: `llvm_expr_unary_core.c`
  owns unary and try-operator lowering, while `llvm_expr_scalar_core.c` owns
  callable signatures, coalesce lowering, and binary expressions. Current
  sizes are 151 LOC and 456 LOC. Coalesce diagnostics avoid raw `??` text in C
  string literals to prevent trigraph rewriting warnings. Local gate:
  LLVM-enabled object build for both owners.
- 2026-05-11 LLVM resource registry owner update:
  `llvm_registry_resources.c` now owns slot/view/device/future/channel/Rc/Weak
  variable registry rows, while `llvm_registry_resource_types.c` owns
  Slot/SecureSlot/Pin/container LLVM type-shape construction and sizeof
  constants. Current sizes are 290 LOC and 245 LOC. Local gate:
  LLVM-enabled object build for both owners, source inventory smoke, and `.inc`
  size smoke.
- 2026-05-11 LLVM domain struct registration owner update:
  `llvm_domain_struct_register.c` now owns domain struct type-body
  construction, while `llvm_domain_struct_register_fields.c` owns generated
  class-field inventory registration through
  `llvm_domain_struct_register_fields.h`. Current sizes are 269 LOC, 317 LOC,
  and 16 LOC. Local gate: LLVM-enabled object build for both owners, source
  inventory smoke, and `.inc` size smoke.
- 2026-05-11 LLVM let resource owner update:
  `llvm_stmt_let_resources.c` now owns ReadView/WriteView/MoveToken alias
  lowering and Slot/SecureSlot sugar lowering, while `llvm_stmt_let_with.c`
  owns generic let orchestration and typed registry post-processing. Current
  sizes are 243 LOC and 304 LOC. Local gate: LLVM-enabled object build for
  both owners, source inventory smoke, and `.inc` size smoke.
- 2026-05-11 LLVM zone sync clause owner update:
  `llvm_domain_zone_sync_clauses.c` now owns action-cause and detach clause
  lowering, while `llvm_domain_zone_sync.c` owns bounded frontier loop
  orchestration plus apply/maintain dispatch. Current sizes are 199 LOC and
  360 LOC. Local gate: LLVM-enabled object build for both owners, source
  inventory smoke, and `.inc` size smoke.
- 2026-05-11 LLVM backend type owner update:
  `llvm_backend_type_render.c` now owns type-name rendering, constructed type
  argument parsing, and the render context, while `llvm_backend_type_map.c`
  owns Pergyra-to-LLVM mapping policy and concrete container/resource lowering.
  Current sizes are 173 LOC and 351 LOC. Local gate: LLVM-enabled object build
  for both owners, source inventory smoke, and `.inc` size smoke.
- 2026-05-12 AIR evidence-counter owner update:
  `air_validate_summary_counters.c` now checks DAG metadata/generic/ability,
  observability schema, and runtime frontier policy counters against the
  first-class `AIREvidenceNode` inventory. MIR and RIR checks remain in the
  same owner, so summary counters stay compatibility telemetry rather than a
  second source of truth. Local gate: object build for
  `air_validate_summary_counters.o` and `air_validate_evidence.o`.
- Semantic effect/helper implementation-header debt is split:
  `type_checker_helpers_effects.h` is declaration-only, while
  `type_checker_helpers_effects.c` owns effect word parsing, declared effect
  contracts, and body-summary recording,
  `type_checker_helpers_resources.c` owns resource handles, nominal flavor
  lookup, and subject-host helpers,
  `type_checker_projection_path.c` owns projection source field-path
  resolution, and `type_checker_world_embedding.c` owns world constructor
  zone-embedding handoff diagnostics.
- Expression resolver implementation-header debt is split:
  `type_checker_expr.h` is declaration-only, the obsolete
  `type_checker_resolve.c` / `type_checker_resolve.h` compatibility owner is
  deleted, `type_checker_resolution_retired.c` is only a quarantine sentinel,
  and assignment/constructed-wrapper
  helpers live in `type_checker_type_helpers.c`.
  `type_checker_resolution_helpers.h` is also declaration-only now;
  `type_checker_resolution_helpers.c` owns
  metadata-first `resolve_named_type(...)`, alias lookup, symbol-kind labels,
  and the embedded-world-zone mutation guard. Expression dispatch, call typing,
  and host lookup/call behavior are now split across
  `type_checker_expr.c`, `type_checker_expr_call.c`, and
  `type_checker_expr_host.c`, all below the 600 LOC review threshold.
- Builtin query/slot operation implementation-header debt is split:
  `type_checker_builtins_query.c`, `type_checker_builtins_query_world.c`,
  `type_checker_builtins_query_channel.c`, and
  `type_checker_builtins_query_domain.c` own query, world-query,
  channel-query, and domain-helper behavior; `type_checker_builtins_slotops.c`,
  `type_checker_builtins_secure_token.c`, and
  `type_checker_builtins_resolve.c` own slot lifecycle/view/device-slot
  builtins, secure-token validation, and builtin name resolution. Their
  headers are declaration-only.
- Nominal builtin dispatch no longer lives in an implementation header:
  `type_checker_builtins_nominal.c` owns the main dispatcher and
  `type_checker_builtins_intent_observability.c` owns the intent
  observability builtin family. `type_checker_builtins_ownership_nominal.c`
  owns Rc/Weak/Allocator/Box validation and beta payload policy.
  `type_checker_builtins_nominal.h` is declaration-only and these owners
  remain under the 600 LOC review threshold.
- Slot analyzer summary/escape behavior is split:
  `slot_analyzer_summary.c` owns access/function-alias/parameter summaries and
  `slot_analyzer_escape.c` owns escape collection/mask materialization. The
  semantic shape gate now tracks both owners under the 600 LOC review
  threshold.

상태 표기:

- `DONE`: 구현/문서/회귀가 같은 말을 한다.
- `IN PROGRESS`: 핵심 경로는 있으나 source-of-truth 또는 coverage가 부족하다.
- `BLOCKER`: 베타 이름을 붙이기 전 반드시 닫아야 한다.
- `OUT OF BETA`: 베타 뒤로 명시 이동한다.

---
