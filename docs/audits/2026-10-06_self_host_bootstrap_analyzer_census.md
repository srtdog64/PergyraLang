# Self-Host Bootstrap Analyzer Census

Date: 2026-10-06 (Asia/Seoul)

Status: read-only audit record for the CI bootstrap closure. It does not own
compiler semantics, registry status, active progress or a successor rung.
The SoT registry, `docs/agent_work_directives/inout_array_release_2026-10-05.md`
and the executable gates named below remain authoritative.

## Why this census exists

`self-host-codegen-bootstrap-linux` had been red since 2026-10-02. Every
Linux job (`backend-compare-toolchain-linux`, `build-linux`, sanitizers,
`self-host-bootstrap`, `self-host-contracts`, the 20 backend-compare shards)
needs that job, so their state was unknown. The last fully green bootstrap was
`6a846fd4` (2026-10-01). Between it and `49f8eaf1` the self-host collection
ownership analyzer grew by about 14.5k lines (the P0 inout/ArrayDrop lane)
while the compiler source it must admit stayed mostly unchanged.

The bootstrap feeds the whole self-host source closure through that
analyzer. The analyzer stops at the first refusal, so fixing one refusal
only revealed the next one. To see the whole picture, a temporary diagnostic
build of `ast_collection_ownership_scan_owner.pgy` logged every refusal and
continued instead of returning. That build was never committed. It recorded
the code, boundary, statement, function, module and callee of each refusal.

## Method

- Probe: `tests/self_hosted/fixtures/nominal_constructor_source_arity_probe.pgy`
  built with `bin/pgy.exe --native-pipeline --opt=release --backend=c`.
  The probe was then run as `<probe> <entry.pgy> diagnostic`.
- The input is the import closure of each bootstrap entrypoint. For example,
  `src/self_hosted/codegen/main.pgy` has 627 files and takes about 10
  minutes per census.
- After each batch of fixes the census was rerun on the full closure.
  Small reproductions confirmed each rule change before it was kept. Every
  positive was paired with refusals that keep the old boundary.

## Results

### Codegen closure (`src/self_hosted/codegen/main.pgy`)

| Census | Refusals | Largest classes |
|---|---:|---|
| 1 | 156 | shallow mutation 64, formal element use 32, formal indexed read 21, ArrayPush 15 |
| 2 | 117 | formal element use 32, shallow mutation 29, formal indexed read 21 |
| 3 | 74 | formal indexed read 25, formal element use 22 |
| 4 | 17 | owned String drop 4, indexed read 3, ArrayPush 3 |
| 5 | 5 | owned String drop 3 |
| 6 | 1 | formal indexed read 1 |
| 7, 8 | 0 | `body_ok=true` (every body analyzer, not only ownership) |

Local `tests/self_hosted/parity/codegen_bootstrap.sh` on this checkout
reached the following:

- gen0 → gen1 → gen2 → gen3.
- `fixpoint ok: gen2 == gen3 (100133 lines)`. gen1.c and gen2.c are also
  byte-identical.
- The Pergyra-built tool emits the same C as the oracle-built one on all
  eight samples.

The first full run then stopped in breadth: gen2 refused the lexer component.
That led to the breadth census below. After it, the same script on this
checkout printed `SELF-HOSTING OK` (fixpoint, eight samples, lexer, parser,
semantic, mir_lower, the 14 tools and the fuzz generator; 3170 seconds).

### Breadth closures compiled by gen2 in the same job

| Entry | First census | Now |
|---|---:|---:|
| `lexer/main.pgy` | 3 | 0 |
| `parser/main.pgy` | 2 | 0 |
| `semantic/main.pgy` | 20 | 0 |
| `mir_lower/main.pgy` | 36 | 0 |
| `fuzz/backend_parity_generator/main.pgy` | 0 | 0 |
| 14 audit tools (`codegen_bootstrap_tools.txt`) | 4 tools failing (12) | 0 |

`compiler/driver_bootstrap_main.pgy` is listed in the harness paths but is
not compiled by `codegen_bootstrap.sh`. Its closure stops earlier, in the
named-value boundary analyzer (`named_value_boundary_argument_required` in
`SelfMirLowerControlTransferFromArtifact`). That analyzer is outside the
instrumented scan, so its full count is unknown.

### Last mir_lower refusals and how they closed

- `MirIntentRoutineStepPlanFromOwners` bound a borrowed step name. It now
  takes an owned copy.
- `MirIntentStepSubjectBindingFromOwners` passed
  `declarations.field_identities.field_*`, a two-level member path of a ref
  formal, to `IntentSubjectSlotSelect`. The analyzer admits only one-level
  member reads, so a narrow adapter now borrows the field-identity index as
  its own formal. The member-place follow-up below removes that adapter.

## Classes and their resolution

The refusals collapsed into about fifteen root causes. Each was resolved
either by making an analyzer rule precise, or by changing source whose
ownership really was ambiguous.

### Analyzer rules made precise (fail-closed refusals kept)

All of these are covered by
`tests/self_hosted/parity/collection_bootstrap_closure_rules.sh`: C and LLVM
analysis, 8 positives and 16 refusals each.

1. **Loop-local constructor field escape.**
   - Owner: `ast_collection_call_effect_owner.pgy`.
   - A fresh loop-local array stored into a constructor field now records
     its exact site instead of the loop sentinel 0. Each iteration rebinds a
     fresh generation.
   - Still refused: a push after the escape, and an array declared outside
     the loop.
2. **Statement mutation after a proved shallow call.**
   - Owner: `ast_collection_ownership_statement_transition_owner.pgy`.
   - Statement-level `ArrayPush`, `ArraySet` and `ArrayPop` on a fresh
     exclusive descriptor that a proved shallow inout call wrote back are
     admitted. The argument path already had the same current-descriptor
     exception.
   - Still refused: after consumption, after an alias, after a deep drop,
     and in deferred code.
3. **Local struct field lending.**
   - Owner: new `ast_collection_member_read_local_root_owner.pgy`. The
     member-move identity now carries `string_array_field`.
   - A direct `Array<String>` field of a local struct may be lent to a
     readonly call. Whole-root lending to a readonly or ref formal is also
     admitted. A returned aggregate may capture the root only in the
     terminal tail.
   - Still refused, blocking the root for the whole body: any whole-root
     alias, any writable or consuming argument, any sequence-field copy or
     store, any deferred use, and any member move.
4. **Block-local owned literal transfer.**
   - Owner: `ast_owned_string_literal_transfer_owner.pgy`.
   - A local source may be transferred into `[x]` and released with
     `ArrayDropOwnedStrings` inside one straight-line block, including a
     loop body. Before this change it had to be at function top level.
   - Formal sources keep the function region, because a loop would
     transfer them twice.
   - Still refused: a source declared outside the block, a conditional
     drop, and a use after the drop.
5. **Comparison before an own-formal literal transfer.**
   - The formal path now admits the same comparison reads before the
     transfer that the local path already admitted.
   - Still refused: a comparison after the transfer.
6. **Function-exit tail consumption.**
   - Owners: `ast_collection_ownership_argument_transfer_owner.pgy` and
     `ast_collection_owned_generation_order_owner.pgy`.
   - A consumption in a tail that exits the function (`return`, `exit`)
     retires only that tail scope. Later uses on the normal path stay live.
   - Still refused: a non-terminal consumption, a later use in the same
     return expression, and a `break` tail.
7. **Aggregate release plan demand chains.**
   - Owners: the schema, lineage and uniqueness owners, plus
     `ast_collection_call_retirement_owner.pgy` and
     `ast_collection_repeated_local_generation_owner.pgy`.
   - Demands carry their caller edge chain. Loops that do not enclose the
     origin no longer count as repeated retirement. Branch-nested scopes
     inside a loop body count as repeated generation.

### Source whose ownership was ambiguous

- **Shared empty descriptors.** `SelfMirDeclarationRowsEmpty()` gives every
  row family one descriptor, and the builder copied those fields and then
  grew them. Each family now starts from its own `[]` (38 refusals).
- **Per-row snapshot structs.** The domain runtime assignment and
  participant-role builders built a fact struct from the growing arrays on
  every row. Validation now runs once over the completed facts (28
  refusals).
- **Borrowed element binding, forwarding or return.** These now take an
  explicit owned copy (`Concat("", x)`) at the boundary. Examples are
  `args[0]` returns, `let name = names[i]`, and elements passed to user
  functions. Where the callee only reads characters, an in-place check was
  added instead (`SemanticArrayTypeNameAt`, which reads with `CharCode`
  and `StringLength`).
- **Parallel-row parameter lists.** These now borrow the struct that owns
  the rows:
  - `MirIntentModeProjectionFromCarriers`, `MirIntentPriorityProjectionFromCarriers`,
    `MirIntentBindingProjectionFromCarriers` and
    `MirIntentPhaseProjectionFromCarriers` take
    `ref carriers: MirIntentRoutineCarrierProjection`. The struct moved to
    `intent_routine_carrier_projection_schema_owner.pgy` to avoid a cyclic
    import.
  - The intent step emitters take `ref steps: SelfDirIntentStepFacts`.
  - `SelfMirDeclarationAppendMethodContract` takes
    `ref facts: SemanticAstActionContractFacts`.
- **Index stores on inout formals.** `xs[i] = v` is an unproved element use;
  these now use `ArraySet`.
- **Mixed owned and borrowed pushes, and `[fresh]` literals.** These now
  start from `[]` and use `ArrayPushOwnedString`, so one descriptor owns
  every row through its single release.
- **Nested retaining calls in a return.** These are hoisted to a `let`
  before the terminal tail (compound terminal storage effect).
- **`Args()` passed directly to an entry function.** It is now bound to a
  local first, as codegen already did.

## Debt left on purpose

- `EmitStmtList` passes `Concat("", x)` copies to `CodegenPrefixOwnedStatementLine`
  instead of transferring `EmitLet`, `EmitAssign`, `EmitLog` and
  `WrapExprWithSemanticGraph` results. The owned-result domain no longer
  proves those producers fresh. The pins now name the copy form.
- `EmitFunctionWithSpecialization` no longer deep-drops its fragment
  epoch.
  - The fragments are struct fields, array rows and literal-or-fresh
    locals, and none is a proved owned String.
  - The leak is bounded by the emitted C size (about 7 to 15 MB for the
    compiler).
  - A negative pin rejects the old release.
- Line caps. Owners grew past their caps while the gate was hidden behind
  the bootstrap red.
  - Thirty-two caps were re-pinned to measured size.
  - Six semantic owners over the 600-line stage default moved to
    `tests/fixtures/self_hosted_responsibility_caps.tsv`.
  - Each one needs a responsibility split.
- Analyzer gaps observed but not changed:
  - Element uses through struct-formal members are not tracked.
  - The storage provenance of a `Clone` result is unproved for `ArrayDrop`.
  - `ArrayDrop` after an inout call inside a loop is still refused. This
    is the P0 frontier.
  - `ast_owned_string_literal_transfer_owner.pgy` is at 598 of 600 lines.

## Member-place follow-up

While closing the adapter, a shape-by-operation matrix showed that the
analyzer treats storage as an `Array<String>` binding only. Arrays reached
through struct member paths were judged differently from the same arrays
passed directly:

- Too strict: nested member paths (`formal.a.b`, `local.a.b`) lent to a
  readonly reader were refused.
- Unsound, and already present before this work: a String element borrowed
  from a struct field survived a later aggregate release in eight shapes
  (callee return, callee push, local binding, local push, constructor capture,
  identity call, inout root binding, extracted nested release).
- Unsound, and already present before this work: mutation through a copy of a
  struct (`let copy: T = place`) or through a plain struct parameter's field
  shares the descriptor with the source. The accepted contract in
  `docs/mut_borrow_parameters.md` forbids that untracked alias mutation.
- Too strict for direct array formals only: an element passed to a user
  `String` formal that only compares it is refused, while the same call
  through a struct member is admitted.

The fix is the next commit, with its own audit and falsifier gate.

## Bootstrap time

The CI run of `18648b8b` cancelled `self-host-codegen-bootstrap-linux` at its
30-minute limit; the last green run (`6a846fd4`) took 26 minutes. The job
analyzes the codegen closure once per generation and again for breadth.

- Release observer of the codegen closure: 553 to 672 s per analysis; the
  collection ownership stage was 65 to 72 percent of it, and its scan was
  most of that stage.
- A gprof build (no ASLR) put 87 percent of samples, 249 s over 4,988,590
  calls, in `SemanticExpressionGraphRootStartAt`, which found the previous
  root by visiting every earlier root slot on each call.
- Scanning back to the nearest earlier slot with a root gives the same slot.
  The same closure then took 214 s with the same verdict.

The timeout was not raised.

## Next steps, in order

1. Push and read the first CI run in which the Linux jobs behind the
   bootstrap actually run. Their failures are unknown until then.
2. Land the member-place analyzer fix and remove the adapter.
3. Restore the two owned-transfer debts above and split the re-pinned
   owners. Then return to the P0 rung.
