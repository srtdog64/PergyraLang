# ArrayString Literal Target Projection — 2026-08-30

Status: `IMPLEMENTATION COMPLETE — PUBLICATION CI PENDING`

Exact base: `79b70e5c007877bb85668dd5790c2b30cb1c5a47` on
`origin/main`.

This directive coordinates one executable ABI consumer migration. It does not
close or reclassify `abi.mir_array_string_layout_projection`, change a project
percentage, or create another ArrayString layout owner.

## Objective card

- Objective: make C empty and LLVM populated ArrayString literal expression
  materialization consume the target-qualified
  `DirectMirArrayStringAbiProjection` already derived at the scalar-program
  emission root. The expression chain must carry that receipt rather than
  reopening the C carrier or spelling the LLVM aggregate and alignment.
- Priority order: admitted layout identity; exact target binding; fail-closed
  absence; C/LLVM literal parity; old-read ratchet; then patch size.
- Production entrypoints: installed
  `pgy-self-driver --mir-json-backend=c|llvm` over the owned ArrayString return
  fixture and the nested/mixed ArrayString literal fixture.
- Direct bypasses to delete: the C literal consumer calls
  `CompilerAbiLayoutArrayStringCValueType()` for an empty carrier; the LLVM
  populated-literal consumer spells `%pgy.array.string` and `align 8` for its
  caller-frame backing allocation, zero initialization, and final load.
- Fact owner: `DirectMirScalarProgramArrayStringAbiFact` carries the admitted
  program receipt. `DirectMirArrayStringAbiProjection` owns the selected-target
  C carrier, LLVM structural aggregate, LLVM named value type, and storage
  alignment.
- Last legitimate consumers:
  `DirectMirScalarProgramCArrayStringLiteralExpression` and
  `DirectMirScalarProgramLlvmArrayStringLiteralExpressionAt`, reached through
  the existing C/LLVM scalar expression dispatch chains. The LLVM collection
  preamble is the declaration consumer for the same named value type.
- Forbidden fallback: global C carrier lookup in the literal consumer; literal
  `%pgy.array.string` or `align 8` in LLVM populated-literal materialization;
  deriving another projection below the emission root; accepting an
  ArrayString literal when the carried target projection is absent or drifted;
  or changing the public runtime ABI.
- Verification gates: the owned-return gate executes C/LLVM lifecycle plus
  forged-ABI negatives; the nested-expression-literal gate executes populated
  C/LLVM parity and its existing semantic negatives; the component contract
  rejects the retired literal-consumer reads and proves root-to-consumer
  carriage. The nested gate's stale duplicate 445-line cap for the centrally
  capped 490-line expression-admission owner is not an executable claim and
  must not block its behavior matrix.

## Fresh falsifier

- The exact-base installed driver passes the owned ArrayString return gate.
  Its generated C contains empty literal carriers while the C literal owner
  still obtains their type from the global ABI lookup.
- A direct installed-driver production probe of the nested/mixed literal
  fixture passes MIR generation, C/LLVM projection, compilation, execution,
  and identical stdout. The LLVM artifact nevertheless contains populated
  literal `.backing` allocations, zero stores, and loads with locally spelled
  `%pgy.array.string` and `align 8`.
- The legacy nested gate currently stops before execution on a stale inline
  `445` cap even though the central shrink-only inventory owns the same file at
  its current `490` lines. That duplicate local cap is a gate obstruction, not
  authority to raise or weaken the central cap.
- The first projection-backed LLVM implementation used the carried structural
  aggregate as the literal value type. The focused gate reached Clang and
  rejected `{ ptr, i64, i64, ptr }` where `%pgy.array.string` was required.
  This proved that the projection lacked a distinct named LLVM value-type fact;
  byte-equal structure was not sufficient type identity.

## Scope and budget

- Allowed edits: the ArrayString projection owner and LLVM named-type
  declaration consumer, the named C/LLVM literal consumers, their existing
  scalar expression dispatch/caller chain, the focused nested gate's obsolete
  duplicate cap row, the component ratchet, the registry residual note, and
  current coordination/handoff documents.
- Out of scope: ArrayString return ownership itself, cleanup, conditional or
  multiple moves, other collection families, source semantics, a generic ABI
  query layer, and registry reclassification.
- Budget: static owner/gate checks under 60 seconds, focused parity under five
  minutes, one current-source installed-driver rebuild, then bounded
  publication CI.

## Output classification

Success is one executable literal-consumer migration delta. It does not
decrement `BRIDGE=32`, increase hard `SUBSTITUTING`, close
`selfhost.semantic_artifact_admission`, or prove the wider reconstruction-
resistance target.

## Local result

- The C expression chain now carries the root-derived projection through every
  operation, control-flow return/condition, and nested assignment caller to the
  literal consumer. Empty literals obtain their carrier only after
  fact/projection cross-validation; the direct global lookup is absent.
- `DirectMirArrayStringAbiProjection` now distinguishes
  `llvm_aggregate_type` from `llvm_value_type`. Projection readiness fixes the
  named type once; the LLVM declaration and populated-literal consumer use that
  field. The literal consumer contains no `%pgy.array.string` or `align 8`.
- The stale nested-gate copy of the expression-admission cap was removed. The
  central shrink-only 490-line owner cap remains unchanged. Every modified
  capped owner is at or below its existing limit, including C literal 45/45,
  C expression 230/230, C operation 120/120, C emission 310/310, LLVM builtin
  70/70, and LLVM expression 360/360.
- Fresh exact-source DRV-2 SHA-256 is
  `19387F164F173A96E44E4DCBD2B8B94915B43593BA8FC8489F8486FEF492221E`.
  Owned-return lifecycle/forged-ABI negatives and nested/mixed populated
  literal C/LLVM parity/semantic negatives are GREEN. The full component
  contract, hard contract, SoT edge, Gate single-owner, protocol registry, and
  build-source inventory are GREEN at 88 authorities / 183 carriers /
  `CLOSED=55 BRIDGE=32 ACTIVE=1`.
- Publication remains pending exact-head remote CI. No row status or project
  percentage changed in this prerequisite migration.
