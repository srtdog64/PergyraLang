# Runtime helpers declared with the facts that emit them (2026-11-19)

Source: main `6f7126f72` plus this change. Tree: WSL `Ubuntu-C-WSL`
`/root/ts2`. Driver from `make self-host-compiler` (1397 s):
`bin/pgy-self-driver` SHA-256
`f671f2944f7b1429bcde217566d17c4a77a0c575283d3de4691b981d41d4a685`.
The baseline driver is the one in the
[mutable declaration record](2026-11-19_scalar_cfg_mutable_declaration.md)
(`539cd85e...`, source of `9a5d7362e`).

## Defects

The [gap census](../../audits/selfhost_default_llvm_gap_census_2026-11-19.md)
found 6 programs whose LLVM artifact `clang` rejected and 1 driver panic. Each
came from two decisions that read different facts:

| Program family | What was emitted | What was missing |
|---|---|---|
| a borrowed String return normalized to an owned copy (3 programs) | a `pgy_concat` call | its definition and `strlen`/`malloc`/`memcpy`, decided only by source-level Concat or a value copy |
| an `Array<Record>` type with no push or set (1) | the record push helper, which calls `realloc` | `realloc`, declared only for a push or set |
| `MapKeys` over Bool keys (2) | an `Array<Bool>` temporary | the type (`%pgy.array.bool`, C `pgy_ab`), defined only for an `Array<Bool>` local or parameter |
| `ArraySet` on a local `Array<Record>` (1) | LLVM read the set's element type from the parameter table | the local case: index -1, driver panic |

The change:

- The concat predicate takes the owned-local lifetime fact's return clones
  (`DirectMirOwnedLocalLifetimeReturnCloneReached`). Its definition and the
  LLVM declarations both read that predicate on C and LLVM.
- The record push helper is emitted under the mutation predicate that
  declares `realloc`.
- Both `Array<Bool>` preambles also define the type for an `Array<Bool>`
  expression node, as `Array<Long>` already did.
- The LLVM set reads its element type from the mutation target owner, as C
  does.

## Results

| Check | Result |
|---|---|
| The 7 programs | native C/LLVM and default C/LLVM print the same output and exit 0 |
| New `direct_mir_runtime_helper_materialization_owner.sh` (push core step) | PASS. Six legs for the two programs without runtime calls. Five legs for the HashMap and record-set programs: the direct LLVM leg links no runtime library. |
| Same gate on the baseline driver | FAIL. Default C refuses `borrowed_string_return` ("source C machine declaration is invalid"). Run separately, all four programs fail on default LLVM (undefined `@pgy_concat` and `@realloc`, invalid `extractvalue`, driver panic). |
| `self_hosted_component_contract_smoke.sh` | PASS (the predicate owner stays at its 136-line cap) |
| `sot_authority_edge_smoke.sh` | PASS: CLOSED=70 BRIDGE=22 ACTIVE=2 |
| mutable declaration, general-emitter coverage | PASS |

On the driver built one step earlier (`88f234dd...`, same source except the
predicate's operand order), these also passed: builtin rows, array parameter,
Array<String> cleanup fact and Array<Int> return.

That driver also ran the default-LLVM sweep of the 1,398 programs. 7 moved
from compile-fail to equal and nothing else moved: equal 483 -> 490,
compile-fail 445 -> 438, refused 453 and differs 7 unchanged.
