# General route: the foreach element is an operation; String elements (2026-11-19)

Source: main `983b6ca4b` plus this change and the builtin-rows gate of `2eae64138`. Tree: WSL `Ubuntu-C-WSL`
`/root/ts2`. Driver from `make self-host-compiler`: SHA-256
`9184372d1779cb0ce852a217be81c5c10b6b731917794c18ca1493f7df9c41ab`. The ASan
probe and the sweep's first run used the build one step earlier
(`ede71c93...`), whose declaration lookup skipped every id-0 row. The sweep
did not move between the two builds.
The comparison driver is the one in the
[range bound record](2026-11-19_general_route_range_expression_bound.md)
(`6a5dc54e...`).

## Before

Slice 1 bound `v = xs[cursor]` at body entry as a block effect from an
expression row that no operation owned. Every owner that maps an expression
row to its routine through operations and blocks missed that row:

- The Array storage preservation proof gave up, so a caller's
  `Array<String>` cleanup refused "storage another holder still has".
- An `Array<String>` element bound this way was outside the ownership
  owners that already handle `let v = xs[i]`.

## Change

- The body entry is now the first operation of the body block: a definition
  of the binding's element value (`iteration:<loop>:element`) from
  `xs[cursor]`, with no MIR row.
  - The ownership owners see it as they see `let v = xs[i]`.
  - The collection ownership receipt plan, which pairs MIR rows with
    operations, inserts an empty receipt at that block.
  - Plan readiness lets that one value sit on the loop's binding slot.
- Deleted: the element row field and column, the block-effect emitters, and
  the element-row use count and readiness.
- `Array<String>` elements are admitted.
  - A String walk is refused at `collection_range` when its loop body
    contains `ArrayPop` or `ArraySet`, or reassigns the walked iterable by
    name. The loop body is the natural loop.
  - Each of these can release the element the binding borrows.
- A loop binding is a declared source local with binding id 0. The no-wire
  declaration lookup refused every name in such a routine.
  - It now skips an id-0 row only when its name is the binding of one of the
    routine's `loop-init` rows. Any other id-0 row in the declared prefix
    stays invalid.
  - The declaration identity probe gained a `loop` mode, which the
    `Array<String>` cleanup gate runs.

## Results

| Check | Result |
|---|---|
| `direct_mir_general_route_collection_iteration_owner.sh` | PASS. 11 programs are equal on native C/LLVM, default LLVM and the general emitter's C. Four String walks (alias, push into another array, return, concat) also run on the general emitter's C under ASan with no freed-memory access. Two forged MIR and two releasing String walks (pop, set) are refused. |
| builtin rows, runtime helper, mutable declaration, coverage, array parameter, component contract, SoT edge | PASS |
| `direct_mir_array_string_cleanup_fact_owner.sh` with the `loop` probe mode | PASS (refused 18+4+6+2, admitted 8x2 plus native MIR, binding and nested cases) |
| `make ci-push-linux`, prebuilt mode, 86 steps | 83 PASS. Steps 5 and 85 fail only locally: "driver not installed beside pgy" (install layout). Step 52 flagged the language-word inventory, regenerated here (`loop` now names 7 files). |
| ASan probe of the general emitter (C and LLVM), 9 String programs | Alias, push, return and the index forms run equal with no ASan error and no leak. Concat runs equal with no ASan error and leaks 16 bytes (C) or 8 bytes (LLVM). The pop and set walks are refused. |
| Default-LLVM sweep against the range-bound driver | 3 moved from refused to equal and nothing else moved: equal 513 -> 516 |

Rebased on `ff6edf0e5` (default-route native runtime parity):

- The combined build passed the gates above, the `Array<String>` cleanup
  gate, the language-keyword registry and `default_route_native_runtime_parity_owner.sh`.
- Its sweep moved only the 7 default-LLVM differences that `ff6edf0e5`
  fixes: 2 now equal, 4 refused, 1 compile-fail, none differs.

The 3 programs are `codegen/fixture/log_int_direct`,
`codegen/fixture/str_array_concat` and
`tests/cases/function_param_flow_summary/for_string_binding_backing_valid`.

## Found on the way (not changed here)

- **Native admits freed-memory access.** A foreach over `Array<String>`
  whose body runs `ArraySet` or `ArrayPop` on the walked array prints freed
  memory on native C and LLVM.
- **The general route admits the same with an index binding.** It compiles
  `let name = names[i]; ArrayPop(names); Log(name)` and ASan reports
  heap-use-after-free on C and LLVM. The baseline driver `f671f294...` does
  the same.
- **Common cause.** The MIR receipt marks the pop an owned String pop while
  an element borrow is live. The foreach refusal above keeps the new form
  out; it does not repair either front end.
