# MIR-only·ABI-first backend closure

## What “MIR-only” means here

Pergyra does not have a second backend pipeline that jumps from syntax to C or
LLVM. The production path is:

```text
AST → Semantic → HIR/DIR/RIR → AIR synthesis + verification
    → MIR lowering + validation → AIR MIR evidence + re-verification
    → AIR-bound Verified Projection Plan → C/LLVM projection
```

`Only` applies to the last projection boundary. It means that a backend which
has crossed AIR and the verified projection plan may consume MIR-owned facts,
ABI rows, and target facts only; it must not reconstruct semantic decisions by
re-reading AST type or declaration payloads.

## Why closure had not happened

The entrypoints were already named `transpile_mir_only` and
`llvm_codegen_from_mir`, but those names described admission, not complete
consumer migration:

1. `MIRProgram` still carried AST declaration arrays for the top-level C
   orchestration pass. `emit_program` selected those arrays and passed AST
   declarations to nominal/domain emitters.
2. Several routine consumers had MIR signature fields but still used the AST
   type node when a type-name fact was absent. A MIR body could therefore be
   admitted while its prototype or parameter declarator reopened AST recovery.
3. Runtime-call ABI rows still had a constructed-resource spelling path for
   dynamic nominal types. Before the current rung, MIR carried the spelling
   and call shape but not a stable `RuntimeCallAbiId`, so row identity was
   still a materialization seam.
4. The protocol registry still marks runtime-call compatibility and the
   lowering API as bridge/open. The last consumers are wider than the bounded
   parity fixtures, so the SoT cannot be promoted by documentation alone.

AIR is not the missing step. Production `compiler_emit_c` and LLVM entrypoints
obtain the projection plan through
`pgy_verified_projection_plan_intent_observability_with_air` and obtain the
spawn execution-lane rows through `pgy_verified_spawn_lane_plan_from_air`.
The backend entrypoints reject a missing or unverified plan, so a direct
MIR-only probe remains intentionally available only as a failing/unit-test
surface, not as a production admission proof.
They also validate the plan identity digest and require the AIR certificate
fingerprint before emitting either C or LLVM.

## First executable closure rung

The first rung closes routine signature admission after AIR/MIR:

- owner: `MIRRoutine.has_signature`, `param_type_names`,
  `MIRCallableSig`, and `return_type_name`;
- last consumers: C function body emission, forward declarations, and MIR
  emission contract validation;
- forbidden fallback: `transpiler_mir_ast_type_supported` on an active MIR
  routine;
- missing facts fail closed with a MIR inventory diagnostic;
- callable parameters and callable returns are rendered from MIR type names,
  not from the retained `AST_EVENT_HANDLER_TYPE` node;
- gate: `make mir-only-signature-test-smoke`, included by
  `make backend-fail-closed-test-smoke`.

This rung does not claim that all backend semantic reads are gone. The next
consumer migrations are declaration inventory/projection facts, MIR expression
facts, and the single runtime-call ABI row authority. Each must retain the same
AIR-bound admission and negative gate before the status can move from `PARTIAL`
to `CLOSED`.

### Versioned MIR-lowering admission

The native MIR entry now has an explicit in-process protocol contract. Every
caller constructs `MIRLowerRequest` through `mir_lower_request_init`, carrying
the stable `pergyra.compiler-lowering-api` identity, version `1`, and the HIR,
RIR, and semantic owners. `mir_lower` rejects a missing, unknown, or mismatched
identity/version before it can allocate a `MIRProgram`; it also retains the
existing missing-HIR/semantic failure. The driver, MIR test harnesses, and
transpile tests all use this one request shape, so an old positional call cannot
silently bypass the admission contract.

This is a protocol-admission bridge, not a claim that lowering itself is
already self-hosted: the native C lowering owner remains the current producer,
while `src/self_hosted/mir_lower` is a separate projection lane. The permanent
gate is `make mir-lowering-api-test-smoke`, included by the backend fail-closed
gate; it checks the identity/version validation and rejects positional callers.

The active C MIR function emitter now keeps that boundary fail-closed for
parameter and local registration as well. Once strict MIR signature admission
has succeeded, a missing parameter or local type-name fact cannot be repaired
from the retained `FuncParam.type` AST node, including the generic-substitution
and versioned-local setup paths. The legacy AST conversion remains available
only before MIR admission. `tests/backend_fail_closed_smoke.sh` carries the
negative source-shape checks, while the C/LLVM 75-fixture codegen parity and
DRV-2 producer-first body/MIR parity gates exercise the owner-carried path.

### Declaration inventory sub-rungs

The C backend's nominal forward-typedef pass is now MIR-owned. It reads the
declaration kind and stable name from `MIRDeclHeaderInventory`; it no longer
walks the AST declaration arrays or calls the AST declaration-name owner for
classes, parties, rosters, relations, effects, zones, and worlds. A missing
header name produces a MIR inventory diagnostic and stops emission. The
`mir-declaration-inventory-test-smoke` gate and the MIR-only backend gate keep
this AST path from being reopened.

This is intentionally a sub-rung, not a claim that every domain emitter is
AST-free. Class, relation, effect, party, roster, zone, and world dispatch now
select their declaration from MIR headers. The wider LLVM declaration/bootstrap
inventory remains a declaration-side replacement seam.

### Declaration type materialization sub-rung

The declaration views now mark a selected MIR header as metadata-required even
for header-only callers. C field/slot emitters for relation, effect, party,
roster, and world declarations consume `MIRDeclField.type_name` through the
bounded C type-name owner. If that row is absent, emission reports a MIR
inventory diagnostic; it does not recover the AST type node. The non-MIR
compatibility branch remains available only for the legacy AST-owned path.

This closes a narrower but important ABI seam: the nominal layout's C type is
now derived from the same MIR declaration row that owns the field identity,
rather than from a second AST payload. Relation/effect and party/roster
dispatch now select `emit_*_decl_from_mir_header` directly from the inventory.
LLVM nominal registration now consumes the same field `type_name` rows through
`pergyra_type_to_llvm`, including the class-field slot binding pass; an active
MIR row without that name fails before `LLVMStructSetBody` or slot scope
registration. The retained AST field node remains only in the explicit
non-MIR compatibility branch. The remaining declaration work is the AST-only
role implementation surface and the wider LLVM declaration/bootstrap paths.

World derived-state and command-directive rows are now captured and verified in
`MIRDeclHeader`. When MIR is active, both the C and LLVM world emitters consume
those rows for world layout, derived-state, and activate/maintain/deactivate
passes. A missing header, count, name, source kind, slot, detail, aggregate
input, aggregate input target, or directive target fails closed. Aggregate
inputs and directive state references are resolved through MIR-backed world
zone/state views; world layout, frontier, derived-state, and `HasZone` query
consumers use the same rows, and neither backend rescans the AST to recover
the decision.
Class constructor argument lowering is now a bounded ABI sub-rung. The MIR
class field row's `type_name` is the sole active-MIR expected-type fact: C sets
its context-sensitive expression type from that name, and LLVM maps the same
name through its backend type builder. If the row is missing, either backend
emits a MIR inventory diagnostic before lowering; the active LLVM path cannot
fall through to `mir_decl_field_type` or a compatibility AST field. The
`class_compare_return` C/LLVM fixture and the constructor fail-closed checks
cover this seam. This does not close the wider LLVM declaration/bootstrap
inventory, which remains a separate open seam.

Shared/domain constructor arguments and defaults now use the same bounded C
owner. Party, roster, relation, effect, zone, and world constructors select
the MIR view's field/slot `type_name` for active-MIR expected-type lowering;
the retained AST type node is not an expected-type recovery source. A missing
row fails with the constructor field type-name inventory diagnostic. LLVM class
shared-field defaults likewise pass the MIR type name through
`llvm_emit_constructor_field_arg` before inserting the initializer, so
`None`/collection context cannot be inferred from an unowned expression.
The `party_roster_host_methods` and `relation_effect_projection_sync`
fixtures cover the C/LLVM runtime parity for this slice. The wider LLVM
declaration/bootstrap inventory and runtime-call/layout bridge remain separate
open seams.

Projection path and nominal-member type lookup now use the same rule. Once AIR
has admitted an active MIR plan, class-field projection helpers and C nominal
member lookup accept only `MIRDeclField.type_name`; a missing row or field
metadata emits the backend inventory diagnostic before nested-path resolution.
The retained AST field type is still reachable only from the explicit
non-MIR compatibility branch. This keeps projection path selection and slot
member typing on one declaration ABI fact owner in both C and LLVM.

### Callable declaration sub-rung (AIR-bound)

The LLVM routine forward-declaration path now consumes `MIRCallableSig` rows
for `EventHandler` parameters and returns. The row is produced during MIR
signature capture and reaches LLVM only after the AIR-bound verified projection
plan has admitted the backend; LLVM does not use the retained
`AST_EVENT_HANDLER_TYPE` node to reconstruct the function-pointer ABI on an
active MIR routine. A missing callable row, parameter type name, or callable
arity overflow is reported through the MIR inventory diagnostic and stops
declaration emission. The C path keeps the same MIR callable owner, so the two
backends project the ABI differently while consuming one row.

The executable evidence is the LLVM object compile, the full LLVM compiler
link, `mir-declaration-inventory-test-smoke`, and `tests/llvm_smoke.sh` (which
includes `event_system`). This is a bounded routine-declaration sub-rung, not
full LLVM declaration/bootstrap closure: the wider domain bootstrap inventory
and runtime-call ABI rows remain separate open seams.

The same declaration owner is now fail-closed for ordinary routine ABI rows.
When a routine is active MIR, `llvm_decl.c` accepts only its carried return
type name or callable signature and its ordered parameter type names/callable
signatures. The retained `ASTNode` type is consulted only when the explicit
legacy AST-compatibility branch is selected. Missing MIR declaration facts
produce the inventory diagnostic before LLVM type construction, and the
backend gate rejects the old type-name-first helper from returning to this
path. Thus forward declaration and body emission share the same MIR ABI
authority instead of reopening a declaration/body split.

Hosted domain and role method forward declarations now use the linked
`MIRRoutine` callable rows as well. A method's ordinary parameter and return
types come from `MIRDeclMethod` type-name rows; an `EventHandler` shape comes
from the linked routine's `MIRCallableSig`. If neither row is available, the
forward declaration emits a MIR inventory diagnostic instead of invoking the
old AST parameter helper. The role-operator path applies the same rule to its
rhs and return ABI facts. The role subject/physical receiver projection is a
separate machine/domain seam and remains explicitly tracked rather than being
silently treated as closed here.

The same `MIRCallableSig` owner now remains in force after declaration: LLVM
function-body type construction and parameter alloca binding consume the
carried callable row, and register callable names from its rendered parameter
and return names. Active MIR cannot fall back to `ast_type_to_llvm` for an
`EventHandler` slot; the signature metadata gate rejects a missing callable
row before emission. This keeps AIR as the admission boundary while allowing
C and LLVM to lower one MIR ABI row through backend-specific type builders.

Event declarations now have the same explicit ABI carriage. During MIR
declaration capture, `mir_decl_header_set_event_params` stores the ordered
parameter names and source type names in the event's `MIRDeclHeader`; the MIR
header validator rejects incomplete or non-event rows, and MIR destruction
owns their storage. The C event emitter consumes those rows through the
bounded C type-name owner, while LLVM consumes the same names through its
LLVM type builder. A missing event header, parameter name, or parameter type
is a MIR inventory error in either backend; neither active path re-queries
`ast_event_param`/`ast_let_type`. The retained AST event branch is only the
legacy compatibility lane. This is an ABI sub-rung under the AIR-admitted
projection plan, not a claim that event operation bodies or all LLVM bootstrap
state have independently closed. `tests/llvm_smoke.sh` exercises the
`event_system` fixture on LLVM, and the same fixture is run through the C
backend parity path.

The generic LLVM await-type consumer follows the same owner direction. When
the backend future registry has not yet materialized a binding, active MIR
await inference reads the `Future<T>`/`RemoteFuture<T>` source-local fact from
the current `MIRRoutine`; it does not treat the registry miss as proof that
the source binding is untyped. A missing MIR source-local fact still fails
closed. This closes the `device_slot_remote` path without adding an AST type
recovery lane.

Callable `let` bindings and function-reference values now use the same
carriage rule. In an active MIR routine, an `EventHandler` annotation, lambda,
or callable function declaration is registered from the MIR source-local row
and its `MIRCallableSig` parameter/return rows. A callable value returned by a
call carries the routine's nested callable row instead of retaining an AST
return node. LLVM function-pointer construction and callable return inference
consume those carried rows; a missing callable local, parameter, or return
fact is a MIR inventory error and stops emission. The legacy AST registration
APIs remain only on the non-MIR compatibility path, so this sub-rung does not
create a second ABI owner.

The routine parameter ABI now follows the same hard boundary in LLVM body
emission and parameter alloca binding. Once
`llvm_mir_routine_signature_metadata_complete` has admitted a routine,
parameter LLVM types come only from the carried `param_type_name` or
`MIRCallableSig`; resource-slot parameters use that same row before adding
their slot/token carriage. Pointer-self and value-result decisions likewise
use MIR carriage/type facts. A missing row emits the MIR inventory diagnostic
and stops emission. The former `FuncParam.type` recovery helpers are not
reachable from either active MIR consumer, and the negative backend gate
rejects their reintroduction. Callable locals are registered with the nested
MIR callable rows, preserving one callable ABI owner through declaration,
body, and scope binding.

Boundary-call argument lowering now consumes the same parameter ABI rows.
Active MIR uses the carried callable signature for callable expected types and
the carried type name/carriage for pointer-self and value-result handling.
Only the explicit generic/extern compatibility lane may inspect the AST
parameter type. A missing active-MIR ABI fact fails before argument emission,
so an LLVM boundary call cannot silently repair its expected type from
`FuncParam.type`.

On-demand generic-class method specialization follows the same rule. Its
specialized prototype consumes the linked `MIRDeclMethod` and `MIRRoutine`
rows, including callable parameter/return signatures, before the specialized
MIR body is emitted. The old AST template/body branch is removed from this
active path; a missing generic method ABI row is a MIR inventory failure.

LLVM generic-class layout specialization now applies the same boundary to
aggregate ABI rows. Active MIR requires the generic class header and every
field `type_name` from `MIRDeclHeaderInventory`; missing rows fail closed
before `LLVMStructSetBody`. AST template lookup and field type conversion are
retained only for the non-MIR compatibility lane, so a generic aggregate cannot
silently acquire a second layout owner.

Hosted/member-call type inference now follows that same ABI owner. When the
active MIR path asks for a method return type, it first consumes the linked
routine's `MIRCallableSig` (for callable returns) and then the method's carried
return type-name. A retained AST return node is no longer a repair source after
MIR admission; if both MIR rows are absent, inference fails with a MIR
inventory diagnostic. The legacy AST conversion remains behind the explicit
non-MIR compatibility path, so AIR-bound projection still has one admitted
fact owner.

The role-operator receiver now follows the physical-side half of that rule:
LLVM lowers the `role_subject_type_name` row carried by the MIR role header and
rejects an active MIR role whose receiver name row is absent. The old
`for_type` AST conversion is only a non-MIR compatibility path, keeping the
machine/domain receiver from becoming a second layout authority.

LLVM nominal method registration now closes the matching prototype seam for
enum and class methods. Active MIR registration consumes the linked routine's
`MIRCallableSig` for callable returns/parameters and the `MIRDeclMethod`
type-name rows for ordinary ABI types. Missing rows fail with a method-specific
MIR inventory diagnostic; `llvm_register_required_ast_type` is retained only
for the explicit non-MIR compatibility lane. The negative backend gate rejects
both AST return recovery and AST parameter recovery in this registrar, and the
`role_override_mir`, `event_system`, and `class_compare_return` parity cases
exercise the resulting C/LLVM signatures.

The C role-declaration path now applies the same boundary to role overrides.
`AST_OVERRIDE_FUNC` implementations are produced as role-owned `MIRDeclMethod`
rows after the ordinary role implementation spans. HIR creates the matching
hosted `MIRRoutine`, the declaration-header validator accounts for the explicit
override suffix count, and C/LLVM consume the same method view for body/prototype
emission. Override rows are not ability-vtable entries. The former
`MIR-only C path missing role override method metadata` diagnostic is therefore
retired; the backend smoke gate rejects its reintroduction. The non-MIR
compatibility lane keeps legacy AST override emission only for the explicit
unit-test compatibility path.

LLVM domain aggregate registration now consumes the carried field
`type_name` rows for domain slots and shared fields. Active MIR no longer passes
the retained AST type nodes into `ast_type_to_llvm`; a missing row emits a MIR
inventory diagnostic before `LLVMStructSetBody`. The AST field conversion helper
remains only for the explicit non-MIR compatibility lane, so domain layout has
one MIR ABI owner across C and LLVM.

The same field row is now authoritative for projection lookup. Current-field
class discovery and nested domain projection paths reject a MIR field whose
`type_name` is absent instead of recovering `MIRDeclField.type` from the AST.
This keeps projection navigation and aggregate layout on the same carried ABI
fact and makes a missing row observable at the last consumer.

Constructor Channel rejection now uses that same field-row owner in both
backends. The active MIR C and LLVM guards classify class fields, shared fields,
and domain slots from `MIRDeclField.type_name`; they do not use the retained AST
type node to decide whether aggregate construction is legal. A missing
constructor field type name fails closed with the backend's MIR inventory
diagnostic before the ordinary Channel rejection path can run. The AST type
helpers remain only in the explicit non-MIR compatibility branch. This keeps
the machine-facing constructor ABI and the AIR-admitted projection plan on one
field fact rather than duplicating a classification decision in each backend.

The MIR match-condition payload consumer applies the same rule to a call used
as an option/result subject. Its subject type comes from the routine callable
row or carried return type-name; an active MIR routine with only an AST return
node now fails closed before payload extraction. This prevents pattern
lowering from quietly rebuilding the match payload layout at the LLVM edge.

### Expression call-target carriage sub-rung

DRV-2 fixture 81, `class_method_self_return`, closes a bounded chained-member
case. The semantic expression-graph owner resolves the receiver type of
`MakeStat(...).PromoteIf(...)` from the carried `MakeStat` call target and its
callable return-type row. Expression text is not a type owner.

The MIR JSON consumer is the last legitimate admission consumer for those
call-target rows. It now rejects an expression graph unless every call node
has a canonical direct, namespace, or member target with a nonempty name.
Semantic body analysis cannot repair a missing target after admission. The
focused producer-first gate compares native and self canonical MIR, emitted C,
diagnostics, and runtime output with both C-built and LLVM-built self drivers;
its negative mutation removes only the `Stat_PromoteIf` target and must fail at
the MIR expression-graph boundary. This is call-target carriage closure for the
bounded fixture, not global expression or backend closure.

### Method owner-field carriage sub-rung

DRV-2 fixture 82, `class_method_self_chain`, closes implicit field bindings in
method bodies without rewriting `val` or `limit` into source-level
`self.<field>` text. The semantic environment owner joins the function's
canonical owner row with the nominal declaration's ordered field rows. The
codegen binding owner projects those same carried declaration facts into
`self.val` and `self.limit` C bindings. Parameter and local rows are appended
after owner fields so ordinary lexical shadowing remains deterministic.

The negative gate mutates only the `val` declaration row in self-produced MIR;
the expression graph still contains the bare `val` leaf. The self MIR consumer
must reject it with the structured `undefined_symbol` diagnostic. It may not
recover the field from source text, an AST root, or a backend-local class scan.
Focused C-built and LLVM-built self-driver parity covers canonical MIR,
emitted C, diagnostics, and runtime output. This closes one method-field
carriage seam; it does not make DRV-2 the released default driver.

### Runtime-call row consumer sub-rung (AIR-bound)

The active LLVM Slot/SecureSlot consumers now enter through the single
`llvm_slot_runtime_row_for_operation` owner in
`src/codegen/llvm_runtime_row.c`. Runtime declaration inventory remains in
`llvm_runtime.c`; it is no longer physically mixed with row selection. The
row owner reads the concrete
`MIRResourceRuntimeRow`, checks the operation's call shape, and rejects a
machine-layer runtime-operation mismatch before LLVM function lookup. The
LLVM MIR pin-region path uses that same owner for secure pin initialization
and unpin cleanup, and LLVM identifier auto-read uses it for Slot/SecureSlot
Read, so pin/read-specific call-shape checks are not duplicated in separate
LLVM consumers. SecureSlot runtime declaration registration also resolves
Claim/Read/Write/Release/pin rows through that owner. The consumer files no
longer call the symbol-only
`mir_abi_resource_runtime_fn_by_kind` API. A missing row is still observable
as `NULL` so the explicitly bounded nominal/structural lowering path can run;
external-helper paths fail closed with the existing MIR ABI diagnostic.
The backend fail-closed gate also rejects any direct resource-row lookup in
LLVM consumers outside the shared owner implementation.

The native MIR registry is split on the same boundary. Static canonical rows
and `RuntimeCallAbiId` remain in `mir_abi_resource_runtime.c`; constructed
nominal spelling and call-shape materialization live in
`mir_abi_resource_runtime_constructed.c`; exact source/ABI lookup and derived
pin rows live in `mir_abi_resource_runtime_mir.c`. All three are below the
production owner cap, and the Makefile inventory plus ABI ownership gate names
the responsibility split.

Pin enter/exit has no source call node to own a runtime row. Lowering therefore
attaches `PinReadInit`/`PinWriteInit`, `Unpin`, and `UnpinCleanup` as auxiliary
rows on the concrete `PinRead`/`PinWrite` MIR instruction. The pin lookup is a
view over those instruction-owned rows; it never reconstructs a row from the
global ABI table or from a `SecureSlot<T>` spelling. LLVM uses the same MIR
owner for layout identity validation. If the authorizing instruction or its
auxiliary row is absent, the pin path fails closed. The JSON MIR projection
emits the primary `runtime_call_abi` plus the ordered `runtime_call_abi_aux`
set so this multi-operation ownership remains inspectable.

The active C MIR resource-op path now has the corresponding producer seam:
MIR lowering resolves the static or constructed `Slot<T>` family row once and
copies it into `MIRInstruction.resource_runtime_fact`, including the runtime
symbol, materialization, and call shape. C MIR resource emission consumes that
fact and rejects a missing row instead of reconstructing a symbol from a type
suffix. C pin/block/builtin/let-slot emitters now route through the same
`transpiler_slot_runtime_row_for_source_operation` owner; its only direct kind lookup
is the explicit non-MIR compatibility branch. The self-host MIR producer now
attaches the same row to each machine-layer instruction from its carried
operation plus the routine builder's semantic local/parameter type fact. It
uses `runtime_call_abi_structured_fact_owner.pgy`; it does not parse a row
string, reopen source text, or call the C oracle. The self-hosted `mir_lower`
consumer validates the nested `runtime_call_abi` fact before it suppresses
evidence-only resource rows.
It now validates the same ordered `runtime_call_abi_aux` array on that
instruction: every auxiliary row must retain the MIRResource owner, the
primary ABI type, a declared operation, the canonical payload, and its stable
row identity. This prevents the self-host JSON lane from accepting a valid
primary Claim while ignoring a mutated Read/Write or pin lifecycle row. The
rung-2 auxiliary-ID mutation is owned by
`driver_rung2_resource_runtime_abi_negative_owner.sh`; the wider aggregate and
compatibility corpus remains a separate BRIDGE.

Slot-sugar initialization is also explicit carriage rather than a backend
exception. Its `MIR_INST_DEF` carries the canonical Claim row, the concrete
Write/Read auxiliary rows used by sugar, and the layout ID.
The shared MIR verifier validates every present runtime-call row regardless of
instruction kind, and C/LLVM may consume the active DEF row only when its
operation matches the requested operation. An implicit Write sharing the same
source node as Claim must reject the mismatched exact row and continue through
the MIR-owned same-type operation projection; it cannot treat Claim as Write.

Class field Claim groups occur outside a routine instruction stream, so their
ABI truth lives on `MIRDeclFieldClaim`, not in an active-instruction lookup.
The declaration row carries the Claim runtime-call row, `RuntimeCallAbiId`,
layout pointer, and `LayoutId`; declaration validation rejects a missing or
mutated row. The C class emitter consumes that row directly and cannot recover
the runtime symbol from `inner_type_name`. This is a distinct declaration
owner, not an exception that reopens the static ABI table in the backend.

One executable statement may contain more than one resource operation, for
example `Write(a, Read(b) + Read(c))`. Such a statement cannot own one
meaningful runtime-call row. Lowering therefore keeps the authoritative row on
each `MIR_INST_RESOURCE_OP` and copies it to a DEF/STMT consumer only when the
consumer has exactly one resource source. RIR assigns every resource operation
the stable identity of its enclosing source statement. MIR carries that fact
onto both resource operations and executable statement consumers; the linker
uses source-statement order, the carried stable statement identity, or the
exact source-operation identity. It does not compare source lines or re-read a
call's callee/arguments. Multiple matches remain uncollapsed. C and LLVM
nested-call emitters use the source-stable operation identity to select the
exact MIR row. When a MIR routine and source call are present, failure to find
that exact row is an error; the backend must not recover it from the ABI table.
The focused
`while_loop_slot_read` and `three_slots_cross_update` backend comparison is the
multi-operation regression gate.

Synthetic consumers are owner-derived, never table-fallback consumers. A
typed `Slot<T>`, `SecureSlot<T>`, or `DeviceSlot<T>` DEF and a destructure
Claim carry the canonical Claim row and any concrete slot-sugar Read/Write
auxiliary rows on that MIR owner instruction. When an auto-read, pin helper, or
other source expression has no unique resource-op identity, the C and LLVM row
owners may select the requested operation only from an existing same-type MIR
runtime owner or its auxiliary row set. If every same-type owner fact is
removed, lookup returns no row and the backend fails closed; the static ABI
table cannot authorize the operation by itself. The permanent MIR gate is
`MIR synthetic slot consumers derive rows from an existing owner`, including
the negative owner-removal assertion.

`with slot` cleanup is also an MIR ordering fact. The lowering inventory moves
the with-owned Release after all body-tail consumers before the block becomes
backend input. Release is excluded from the residual-statement suppression
predicate, so a body `Read` nested in `Print` remains concrete and is not
silently replaced by an observability export. The C source-order fixture
ratchets claim/write/read/print/release order for one and two with slots; LLVM
consumes the same MIR order and row identity.

The runtime-call row now has one declared SoT owner: `SFAbiRuntimeCallRows`
under `SOMirAbi`. The self-host `runtime_call_abi` artifact is a projection of
that owner, not a second authority. MIR lowering now materializes a stable
`runtime_call_abi_id` from the canonical `domain|abi_type|operation` key after
the single row lookup. C, LLVM, and self-host MIR consumers validate the ID
against the same key. Table position is deliberately irrelevant. Symbol,
target kind, materialization, and call shape are separately compared with the
canonical row owner, so a valid ID cannot authorize a mutated payload. A
changed payload or carried ID fails closed in native MIR, C, LLVM, and the
self-host MIR consumer. The self-host projection uses `Long` for the hash
intermediate so its arithmetic is byte-equal to the native `uint64_t` owner,
while the carried wire value remains a bounded positive `Int`/`uint32_t`.
`LayoutId` and `RuntimeCallAbiId` occupy disjoint fixed namespaces: layout IDs
use `0x2...` and runtime-call IDs use `0x4...`. Tests reject a zero, duplicate,
or wrong-namespace runtime-call ID and a changed layout-row identity.

This closes the stable-identity sub-rung for static and constructed nominal
rows. `pergyra.runtime-call-abi.v2` remains `BRIDGE` because the complete
aggregate/runtime compatibility corpus and every legacy compatibility path
have not yet migrated behind the same policy. The compatibility policy is no longer
documentation-only: `driver_diag_compatibility_manifest_validate_file` now
requires the seven `runtime_call_abi_*` rows, rejects duplicates/unknown keys,
and fails closed on a changed policy value before native compilation starts.
Self-host projection and C/LLVM row consumers still use the same declared
policy; the remaining bridge is the wider artifact/consumer migration, not
the existence of a policy string. The focused evidence
is `abi-ownership-shape-test-smoke`, `backend-fail-closed-test-smoke`,
`machine-layer-pipeline-test-smoke`, and
`tests/self_hosted/parity/mir_abi_first_lane.sh`. The stable-ID negative
ratchet is also covered by `src/tests/mir/test_mir_runtime_call_abi.cases.h`
and the driver rung-2 wrong-ID mutation in
`tests/self_hosted/parity/driver_rung2_resource_runtime_abi_negative_owner.sh`.

The LLVM runtime-row selector now fails closed when a source-level MIR
operation is active but no routine-owned row exists. The one explicit exception
is the module-level runtime declaration phase: before a routine is active and
with no source node, LLVM consumes the canonical ABI row vocabulary only to
declare exported runtime functions. This prevents declaration setup from
being mistaken for a source-operation fallback. The C selector applies the
same active-operation guard, including unknown operation spellings, so both
backends have the same source-level failure boundary. The broader
runtime-call compatibility corpus remains a separate bridge.

The active-MIR helper `mir_abi_resource_runtime_row_for_mir_abi` is now
instruction-owned as well. It delegates only to
`mir_abi_resource_runtime_instruction_for_abi` and returns that concrete
routine row; it no longer reconstructs a row from the global ABI table after
finding a routine owner. Thus a missing operation row is observable at the
MIR consumer instead of being repaired by backend-time table lookup. The
negative portion of `backend-fail-closed-test-smoke` rejects either global row
lookup from this helper.

### Static ABI layout identity sub-rung (AIR-bound)

Static `MIRTypeLayout` rows now have a content-derived `LayoutId` materialized
on every MIR instruction that carries a layout. The owner is
`mir_abi_layout_id()` in `src/compiler/mir_abi_layout.c`; its hash covers the
canonical type name, size/alignment, ordered field rows, representation/tag
facts, runtime type, and niche policy. It deliberately excludes table index and
pointer address, so C and LLVM can consume the same logical identity even when
their in-memory tables differ.

`mir_fact_surface_validate.c` rejects a missing or changed `abi_layout_id`
before AIR-bound projection reaches either backend. The negative MIR fixture is
`MIR validator rejects missing ABI layout identity` in
`src/tests/mir/test_mir_lowering_part_a_1.cases.h`, and the ABI ownership shape
gate requires both the owner and the mutation. This is a bounded identity rung:
dynamic nominal layouts still use their runtime-call row. The native `pgy.mir.v1`
producer now carries the complete static `MIRTypeLayout` row beside every
instruction (`abi_type_name`, `abi_layout_id`, `abi_layout_required`, and
`abi_layout`), so the self-host consumer can validate the row without reopening
the source type or a process-local table. `abi_layout_required=false` is an
explicit dynamic case and must carry ID `0` plus `null`; a missing tuple,
partial row, or mutated ID fails closed in `mir_lower`. The permanent mutation
gate is the device-slot rung in
`tests/self_hosted/parity/driver_rung2_mir_abi_layout_negative_owner.sh`.

The self-host producer now materializes a bounded fixed-row subset directly:
the fixed scalar `Slot<T>`, `DeviceSlot<T>`, and `SecureSlot<T>` rows for
`T` in `{Int, Long, Float, Double, Bool, String}`, plus explicit-tag
`Option<T>` rows for `T` in `{Int, Long, Float, Double, Bool, String}` and
`Result<T>` rows for `{Int, Bool, String}`. It computes the same
content-derived `LayoutId` from the serialized row; the device-slot rung
observes `707638132` on both native and self-host producers, and the negative
owner mutates the required row and its identity. Unknown and target-dependent
rows still use the explicit dynamic tuple, so this closes producer carriage
for the expanded fixed subset, now including the `Array<Int>` and
`Array<String>` aggregate rows through the `array_sum_filtered` and `str_array`
fixtures. The producer and self-host collection runtime now also carry
`Array<Long>`, `Array<Float>`, and `Array<Bool>` through the
`array_scalar_aggregate_core` fixture; the focused C/LLVM parity lane checks
their layout rows, emitted C, host compilation, and runtime output. This is
still a bounded aggregate subset, not a claim that every generic or
target-specific static row has migrated.
The producer checks `CompilerAbiLayoutTargetPolicyReady()` before materializing
any row: the pointer-bearing `String` subset is an admitted `selfhost-c`
projection, not a universal pointer-width default. A future target must carry
its own size/align facts before it can promote those rows from the dynamic
tuple.
Therefore `pergyra.abi.core.v1` remains `BRIDGE` until the remaining static
row families and aggregate/generic/target-specific layout consumers migrate
behind the same owner.

The constructed-nominal exception is now owner-directed rather than a backend
guess: `mir_abi_resource_runtime_row_is_constructed_nominal()` recognizes only
the ABI owner's `constructed-resource` / `constructed_resource_runtime_spelling`
materialization pair. LLVM may accept the absent static layout row only for
that explicit compatibility edge; a missing layout on a native/static row
still fails closed. No backend may derive a symbol or layout from the nominal
type spelling.

The C last consumer is now fail-closed as well. When MIR is active,
`transpiler_mir_resource_op_core.c` must receive the instruction's complete
layout and matching `LayoutId`; it cannot reconstruct a layout with
`mir_abi_lookup(abi_type_name)`. That lookup remains only on the legacy
non-MIR path. The backend guard is covered by
`tests/backend_fail_closed_smoke.sh`, while AIR-bound validation remains the
earlier shared admission gate for both C and LLVM.

LLVM versioned-DEF copying follows the same rule. Once a MIR routine is active,
`llvm_mir_source_def_copy.c` takes Future/Channel/nominal registration from the
routine's `MIRSourceLocalType` fact (or the instruction's carried ABI type) and
does not inspect `inst->expr1` as an AST type annotation. The AST registration
branch remains explicitly legacy-only; an active MIR instruction with neither
owner fact reaches the existing typed-metadata failure later in the LLVM MIR
consumer instead of recovering a type from source syntax.

Captured closure locals carry two different facts in one source-local row:
`type_name` identifies the closure storage struct, while the callable return
and parameter fields preserve the declared `func(...) -> ...` signature. LLVM
uses the storage fact for allocation and the callable fact for invocation
registration. Dropping either half is a missing MIR fact; LLVM does not infer
the signature again from the lambda AST.

The view-backed C hook follows the same boundary. Its active MIR path consumes
only the carried owner slot and `MIRTypeLayout`; no AST type annotation is
available as a recovery source. An active MIR routine with no owner-carried
layout reaches the existing missing-owner/layout diagnostic instead of
reopening the AST node.
Statement identity is carried by the shared `mir_set_inst_source_statement_fact`
owner for DEF and non-CFG instructions, so multiple resource rows emitted from
one source statement retain the same statement identity without rescanning the
AST. The fixture `MIR keeps multiple resource runtime rows distinct within one
statement` ratchets both distinct row ownership and that shared identity path.
The ABI ownership shape gate carries a negative ratchet for this branch.

The final C view hook now consumes that carriage directly. The former
`transpiler_mir_find_prior_borrow_source_for_view` and
`transpiler_mir_find_prior_resource_layout_for_slot` inventory scans are
deleted; `resource_owner_slot_anchor` and `type_layout` are the only active MIR
inputs. Removing either owner fact therefore reaches the existing
`view-backed resource op is missing owner slot ABI metadata` or typed-layout
failure instead of searching another routine or reopening `expr1`. The
backend-fail-closed gate permanently rejects reintroducing those scans.

### Parallel-capture projection row (AIR-bound, bounded)

The first non-observability projection row family is now executable for
parallel capture dispositions. `SemanticParallelCaptureBoundaryFact` remains
the semantic producer and `MIRParallelCaptureBoundaryFact` is the MIR SoT;
`PgyVerifiedParallelCapturePlan` is only the AIR-bound carrier. The carrier
records the boundary stable identity, capture name, MIR disposition kind,
writer task, AIR certificate fingerprint, revision, and a mutation-checked
digest. Its row disposition is explicit: snapshot-copy maps to
`materialize`, while join-index-disjoint and join-readonly map to `retain`;
unknown kinds are classified as `reject` and fail before carrier publication.

The producer requires a certified AIR boundary for every MIR capture boundary,
valid MIR capture validation, and a non-mutated plan identity. An AIR parallel
boundary with no capture rows is a valid empty carrier; the producer does not
invent a semantic row for it. The last legitimate consumers are the C async/
parallel-join emitters and the LLVM async/parallel-join emitters. They consume
only `pgy_verified_parallel_capture_disposition_find`; direct
`mir_parallel_capture_disposition_find` calls are forbidden after the AIR
admission boundary. A missing carrier, certificate, boundary, row, or digest
fails closed in both backend entrypoints.

The focused negative/source-shape gate is
`make parallel-capture-projection-test-smoke`, included by
`make backend-fail-closed-test-smoke`. The C/LLVM executable fixture
`tests/cases/parallel_snapshot/snapshot_read.pgy` proves the carrier through
both real compiles and the expected `42/1` runtime result; the broader
runtime-call ABI compatibility corpus remains a separate bridge.

### Spawn-lane stable identity sub-rung (AIR-bound)

The verified spawn-lane plan no longer stores an `AST_SPAWN_EXPR` pointer as
its row key. AIR computes `ast_node_stable_id` once, rejects a zero identity,
and publishes `source_stable_id -> ExecutionLane`. C and LLVM consumers pass
the stable ID of their current node only to the verified-plan lookup; they do
not compare pointer identity or reclassify the lane from source spelling.
Hand-built transpiler fixtures assign stable IDs before installing a plan, and
the ABI ownership gate rejects the pointer-shaped row from returning. This is
a bounded identity closure for the AIR spawn projection; the wider MIR JSON
transport of spawn rows remains a separate bridge.

See also:

- `docs/180_compiler_logical_spine_handles_gates.md` for the owner/gate matrix;
- `docs/192_protocol_abi_api_registry.md` for protocol and ABI status;
- `tests/mir_only_signature_smoke.sh` for the executable negative ratchet.

## Hard self-host operating lane

### Objective card

- objective: replace the bounded DRV-2 machine path with a Pergyra producer and
  consumer before expanding the default driver;
- priority: canonical MIR identity, ABI/projection rows, emitted artifact,
  diagnostics, then runtime output;
- fact owners: `SelfMirProgramFacts`, the machine-layer declaration consumer,
  ABI layout rows, and runtime-call ABI rows;
- last legitimate consumers: the self-host MIR verifier and C emitter;
- forbidden fallback: native MIR, AST compatibility text, or C-derived facts
  entering `CompileSourceToMirJsonVerified` or `CompileSourceToCVerified`;
- focused gate: `make self-host-mir-abi-first-test-smoke`.

`DRV-2` is the active replacement lane. The C compiler is a read-only oracle,
not a provider for the self-host producer. The blocking comparison order is:

1. self-produced canonical MIR JSON;
2. MIR-carried projection and ABI rows;
3. MIR-consumed C artifact under C-built and LLVM-built self drivers;
4. byte-stable diagnostics for rejected fixtures;
5. runtime output against the native C oracle.

The explicitly named `--canonicalize-oracle-mir-json` mode may normalize only
the native oracle artifact for comparison. It is not callable from
`CompileSourceToMirJsonVerified` or `CompileSourceToCVerified`. If the self
producer lacks a fact, the producer fails before canonicalization or emission.

Machine-layer `DeviceSlot`, `RemoteFuture`, and routine-parameter programs are
DRV-2 fixtures 78 through 80. They require the target-owned declaration fixture
`tests/self_hosted/fixtures/machine_layer_declaration.json` as an external ABI
input, carry its physical grant together with self-produced contact rows into
canonical MIR, and run through both self-driver backend builds. Fixture 80
also proves that `DeviceSlot<Int>` parameter carriage and concrete Read and
Release instructions consume the self-produced ABI row. The C oracle does not
generate this comparison input. Omitting the declaration or removing the
concrete Read row is a negative fixture, and neither the C oracle nor native
MIR may fill the missing row.

The MIR-to-tree comparison bridge treats `resource-op` as evidence, not as a
second executable statement. Executable `def`/`stmt` rows retain the machine
contact facts. Reinterpreting a `resource-op.expr0` as source text would create
an AST fallback and is blocked by the hard-self-host contract.

### Target projection carriage

The DRV-2 C emitter no longer accepts target readiness from the global envelope
alone. `target_capability_owner.pgy` remains the vocabulary owner,
`target_projection_fact_owner.pgy` derives one admitted projection carriage
row, and `CompilerEmissionArtifact` records its schema, projection, and the
owner-derived target capability fingerprint. The self-host fingerprint is a
bounded positive carriage representation of the same ordered envelope; the
native planner retains the full-width target fingerprint for C/LLVM plans.
`GenerateCFromVerifiedSemanticArtifact` consumes that row; its verifier checks
the carried values and fingerprint against the canonical owner and rejects an
empty, changed, or mutated projection before C text emission. The negative
owner is
`tests/self_hosted/parity/driver_rung2_target_projection_negative_owner.sh`,
with fingerprint mutation covered by
`tests/self_hosted/parity/target_capability_manifest_parity.sh`;
the focused `device_slot_routine`, `option_string_core`, `array_sum_filtered`, and `str_array` lanes pass under
both C-built and LLVM-built Pergyra drivers.

This row is deliberately still `BRIDGE`. It proves that the selected `cpu-c`
projection, the ordered required-fact/fallback vocabulary, and a mutation-
checked fingerprint survive to the last emitter. AIR remains the verification
layer upstream of native `VerifiedProjectionPlan`: C/LLVM still receive only
the AIR-bound plan row, while this self-host DRV-2 carriage does not claim to
be an AIR certificate. It does not yet carry a native full-width target
fingerprint, concrete size/alignment/endian facts, object format, AIR evidence
references, or an LLVM/self-hosted physical projection. Those values must
become target-owned facts before `target.capability_profile` can be promoted.

### Comparison cadence

Oracle comparison is not postponed until the whole compiler exists. Every
executable replacement rung must compare its own bounded canonical MIR,
ABI/projection rows, emitted artifact, rejection diagnostic, and runtime result
against C/LLVM before the next owner surface opens. What is deferred is the
full CI matrix, not the local semantic comparison.

The expansion order after the bounded machine fixtures is:

1. declaration and routine fact production;
2. CFG, SSA, cleanup, and cancellation consumption;
3. aggregate layout and runtime-call ABI projection;
4. additional machine-contact and target-capability rows;
5. whole-corpus diagnostics, artifacts, and runtime parity;
6. fixed-point bootstrap and default-driver replacement.

Each item advances only when the Pergyra producer supplies every required fact,
the consumer rejects a removed fact, and C remains read-only. Broad
`self-host-preparation-test-smoke` and platform CI run at coherent integration
boundaries; they are not a prerequisite for every owner-sized implementation
commit.

### Bounded all-path ArrayString owner move

The current CFG/cleanup candidate extends the existing
`DirectMirScalarProgramOwnedArrayStringMoveFact`; it does not create a second
ownership authority. A four-block `if/else` may retire one named local only
when both successor arms contain the same admitted owner-handle call identity,
each arm contains exactly one transfer, both converge on the same terminal
merge, and the merge has no use of that local. The fact seals straight or
alternative coverage together with call-block, branch-block, and merge-block
rows. Extension and GraphPlan digests carry that identity, and final plan
readiness rechecks the rows against the sealed CFG before either backend emits.

The shared cleanup policy remains the only last consumer. C and LLVM do not
perform branch-local ownership analysis. One-sided transfer, later use after
merge, duplicate transfer in one arm, missing branch coverage, and forged
callable/parameter/ABI identity all fail before artifact publication. This is
bounded alternative retirement, not branch-OR dataflow: loops, early exits,
break/continue, nested alternatives, formal/member/literal/fresh-result moves,
and general ownership flow remain separate rungs.

The focused gate is
`tests/self_hosted/parity/direct_mir_alternative_owned_array_string_parameter_owner.sh`.
It produces MIR once, executes both C and LLVM artifacts for true and false
inputs, checks callee cleanup and absent caller cleanup, and runs the negative
fixtures and mutations through both projections.

### Bootstrap boundary

The focused MIR/ABI-first lane is green, but compiler-scale codegen bootstrap
is not. A seed-only regeneration attempt did not emit `gen1.c`; it was stopped
after roughly 15 minutes at 8.98 GB working set and 12.5 GB private bytes to
avoid exhausting the workstation. This is the already isolated codegen string
amplification blocker, not evidence that DRV-2 lacks a semantic fact. No fixed
point or whole-compiler bootstrap claim follows from the focused lane.
