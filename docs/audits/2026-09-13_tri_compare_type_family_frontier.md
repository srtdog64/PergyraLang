# Backend tri-compare: the type families the bounded route cannot represent

Revision: `cb7c8149ad0f4dc15c3fd2b0dfe9b61b09eb4f0d` (published main/origin/main).
Status: `ci.yml` and `platform_full.yml` are green on this revision;
`self_host_parity.yml` stops at `backend_output_tri_compare_parity.sh` with four
of fourteen cases passing. This audit is evidence, not an ABI owner, completion
score or implementation plan.

## Why the step cannot close with owner fixes

Three registries decide whether the bounded direct-MIR route can represent a
type: `compiler/abi_layout_row_owner.pgy` supplies the ABI row,
`compiler/direct_mir_scalar_cfg_type_family_owner.pgy` admits it as a source
local through `DirectMirScalarCfgSourceLocalTypeSupported`, and
`compiler/runtime_value_representation_owner.pgy` gives it a runtime
representation. Counting occurrences of each family the nine failing cases need:

```
                abi_row  local_admit  runtime_repr
Slice                 0            0             0
Box                   0            0             0
Future                0            0             0
Channel               0            0             0
WriteView             0            0             0
ReadView              0            0             0
SecureSlot            0            0             0
Zone                  0            0             0
Slot                  0            0             0
Allocator             1            0             3
```

One of the ten is representable. So the remaining cases are not stale pins or
single-owner gaps: each family needs a row, admission, a representation and
emission on both legs before any case that uses it compiles.

That ordering is measured, not assumed. The checker side of `slice_copy` is now
complete -- `SliceCopy` is registered and the chained-call mis-parse is fixed --
and the case passes the checker tool while the driver still refuses it at
`SemanticAstInitializerTypeFacts`. Repairing the typing alone moves the wall
from the semantic stage to the route stage.

## Family to case

| family | unlocks | also needs |
| --- | --- | --- |
| `Slice<T>` | `slice_copy`, `slice_surface` | tuple destructuring binding for `slice_surface` |
| `Box<T>` | `allocator_lane_boxarray`, `allocator_defer_cleanup` | `defer`; `Box<Array<Int>>` arrives as `Box<Unknown>` |
| `Slot<T>`, `ReadView<T>`, `WriteView<T>`, `SecureSlot<T>` | `pin_write_view_block`, `secure_slot_view` | a `pin` statement rule and four block attributes |
| `Channel<T>` | `channel_send_recv_basic` | an expression node kind for channel receive |
| `Future<T>` | `async_spawn_await` | `spawn`, `await`, `async func` |
| `Zone` | `intent_zone_binding` | observe with `--observe-mir-consumer-stages` |

`class_factory_field_method` needs no new family; it refuses with
`direct MIR three-routine structural shape is unsupported`.

## The first family's target bytes

`Slice<T>` is the cheapest entry and the native producer already carries its
rows (`src/compiler/mir_abi_layout.c` around line 268), so the self-hosted row
has an exact target rather than a design question. Dumped from
`PGY_NATIVE_PIPELINE=1 pgy --mir-json tests/cases/backend_compare/slice_copy/main.pgy`:

```
Slice<Int>     size=16 align=8 fields=[data@0/8/8 length@8/8/8]
               runtime_fn=pgy_slice_get_Int    inner_c_type=int32_t  representation=0
Slice<String>  size=16 align=8 fields=[data@0/8/8 length@8/8/8]
               runtime_fn=pgy_slice_get_String inner_c_type=char*    representation=0
```

The emitted `abi_layout` object names no field-order term, only the field list,
so the self-hosted row's field-order classification is internal and does not
need a vocabulary agreed with the native producer. The runtime layout is
`{ data: ptr, length: i64 }` on both legs: C `PgySlice_<Suffix>`
(`src/runtime/pgy_runtime_memory_array_slot_inline.h` around line 209) and LLVM
`%PgySlice_<Suffix>` (`src/codegen/llvm_backend.c` around line 350).

Its two source operations are `Array<T>.Slice(start: Int, len: Int) -> Slice<T>`
(`src/semantic/type_checker_expr_call.c` around line 278) and
`SliceCopy(Slice<T>) -> Array<T>`
(`src/semantic/type_checker_builtins_stdlib_array.c` around line 300).

## What this revision closed

Six commits took the set from three of fourteen to four and repaired two
regressions this work introduced. The bounded gaps within reach are closed: the
`f"{expr}"` parse form, the channel constructor with its `Channel<Unknown>`
reconciliation and the `<-` operator no longer read as a comparison, the
chained-call argument span, and `SliceCopy`'s registration. What is left is
family construction.

Two traps cost real time and are worth carrying forward. A new refusal must not
pre-empt a more specific diagnostic a negative gate pins: that broke
`platform_full` twice, once from comparing a whole call rendering and once from
skipping a refusal by operand spelling rather than by whether the type resolved.
And several owner files have their bytes pinned, so editing
`semantic/expression_operator_fact_owner.pgy` required re-pinning
`benchmarks/selfhost_source_scan_owner_evidence.json`.

## A second gate was answering with a month-old artifact

`mir_json_parity.sh` is step 75 of the same chain, so it has been shadowed by
tri-compare too. Four of its pins had drifted and are repaired in `40fa0d34`,
but the last finding is not a pin.

The gate writes each fixture's reconstruction partway through its loop body, and
every block that reads it runs after that write except the
`intent_nested_direct` block, which ran before. Its assertions therefore read
whatever the previous run had left behind -- a file dated August 1 in this tree.
With the block moved after the write and the stale files removed, the gate
reports what is actually true: `mir_lower` rejects the nested-intent document
outright.

    MIR-LOWER ERROR: MIR intent evaluation phase disagrees with its target kind

Correction (2026-09-15): the diagnosis this section first gave was wrong, and
the refusal is correct. The fixture delegated `FrontendPipeline`'s step to the
intent `IntakeSource` with `on:`, which names a receiver action; a delegation to
an intent is spelled `intent:`. `a9b842e6` corrected the fixture (line 57), and
`tests/self_hosted/parity/intent_completion_projection.py:184` already required
a nested delegation to record `arg0 == "intent"`. With the correct spelling both
producers record phase `intent` and the check passes as written. The original
reading is kept below for the record; its "two coherent repairs" do not apply,
and `target_kind` does not refuse correctly spelled delegations. See
`2026-09-15_test_harness_red_team_review_reconciliation.md` C1.

Original reading: `intent_routine_step_plan_owner.pgy` line 230 derives
`target_kind` as `action` when the evaluation has a receiver alias and `intent`
when it does not, then requires the recorded phase to equal
`SelfMirIntentTargetPhase(target_kind)`. For `FrontendPipeline`'s step, whose
`on:` names an intent rather than a receiver, that derivation asks for phase
`intent`. Both producers record `on`:

```
IntakeSource       IntentEval arg0='on' arg1='ReadRoot'
FrontendPipeline   IntentEval arg0='on' arg1='Intake'
```

The self-hosted producer takes its phase from
`SelfMirIntentTargetPhase(intents.steps.target_kinds[step_row])`, so the DIR
resolved `target_kind` as `action` for the delegation; the native producer agrees
byte for byte. The check arrived on 2026-09-09 in `419b1745`, and `target_kind`
is read nowhere else in that owner, so its only effect is to refuse every
delegating step.

Two coherent repairs, and the choice is a decision about the Intent document
format rather than a defect to patch:

- Classify a step whose target names an intent as `target_kind` `intent` in the
  DIR, so both producers record phase `intent` and the check passes as written.
  This changes MIR bytes for intent fixtures and must land in the native DIR and
  the self-hosted DIR together.
- Have the consumer validate the recorded phase against the contract instead of
  re-deriving the kind from receiver presence. The phase projection already
  admits only `on` or `intent`, and re-deriving a fact the document carries is
  what this compiler's own rule forbids.

## The three registries do not agree, measured row by row

The 2026-09-13 architecture review asked for one semantic decision about
whether a resolved type is representable, so that the ABI, local-storage and
runtime-representation consumers cannot hold inconsistent independent lists.
Counting every ABI row against the other two registries says how far apart they
already are. `abi` is a row in `compiler/abi_layout_row_owner.pgy`; `local` is
`DirectMirScalarCfgSourceLocalTypeSupported`; `runtime` is a representation
from `CompilerRuntimeValueRepresentationFor`.

```
type                           abi  local  runtime
Int                            1    1      0
Bool                           1    1      0
Float                          1    0      0
String                         1    1      0
Array<Int>                     1    1      0
Array<String>                  1    1      0
Array<CodegenAstTextNode>      1    0      0
Result<Int>                    1    1      0
Option<Int>                    1    1      0
Option<String>                 1    1      0
Long                           1    1      0
Double                         1    0      0
Array<Long>                    1    1      0
Option<Long>                   1    0      0
Option<Float>                  1    0      0
Option<Double>                 1    0      0
Array<Bool>                    1    1      0
Option<Bool>                   1    1      0
Allocator                      1    0      1
TextBuilder                    1    0      1
Set<String>                    1    1      0
Slice<Int>                     1    0      0
Slice<String>                  1    0      0
```

Twenty-three rows, fifteen admitted as source locals, two with a runtime-value
representation. Eight rows carry a layout the route cannot store in a local and
has no runtime representation for: `Float`, `Double`, `Array<CodegenAstTextNode>`,
`Option<Long>`, `Option<Float>`, `Option<Double>` and the two `Slice` rows. The
float rows are the clearest evidence that these are three lists rather than one
decision: their layout is known and their arithmetic is scalar, yet the bounded
route declines a `Float` local, and nothing in either owner records why.

So the review's rule has a measurable target on this revision: the count of
rows whose three answers disagree, today eight. A canonical descriptor is worth
building when it removes that disagreement rather than renaming it -- each of
the eight needs a recorded reason (a missing emission, a deliberate exclusion)
before one owner can answer for all three consumers without losing a fact one
of them holds today.
