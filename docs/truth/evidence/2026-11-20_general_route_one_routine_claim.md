# General route claims one-routine programs (2026-11-20)

Source: main `fae2390af` plus this change. Tree: WSL `Ubuntu-C-WSL`
`/root/ts2`. Driver from `make self-host-compiler`: SHA-256
`da3180590480d203fbe8f0031cac8c6cd36252ffe1c13e5e6daa2ae70265d908`.
The comparison driver is the combined build in the
[String foreach record](2026-11-19_general_route_string_foreach.md)
(`fae2390af`'s source).

## Before

The scalar program route refused a one-routine program at
`single-program-shape` unless one of these held:

- a control-flow claim owner or a builtin claim owner claimed it;
- it carried a record or an enum.

Before foreach, deleting that guard moved 5 programs from equal to refused
(foreach, `ArrayLength` bound). After the slices that followed, a scratch
driver without the guard moved only 4 programs, each from refused to equal.

## Change

- The guard is deleted. The general route takes every one-routine program
  its admission accepts.
- `direct_mir_scalar_program_builtin_route_owner.pgy` is deleted; the guard
  was its only reader. The program-level control-flow claim is deleted too.
  The routine-level claim stays, because the default C route's substitution
  rule reads it.
- The `projection.direct_mir_scalar_cfg_program_extension` registry row no
  longer lists the deleted owner, and its owner cap row is gone.
- Test changes:
  - `tests/concept_semantics/loop_statement_execution.py` lost a check that
    searched an owner's source text for the deleted call. Text checks of
    that kind were removed on 2026-10-13.
  - The collection-iteration gate gained a one-routine program, a
    `continue` in a literal range plus a String walk. The guard refused it.
  - The mutable declaration gate runs its one-routine foreach on five legs.
    The general route's Array index calls the bounds-panic runtime export,
    which the general emitter's LLVM leg does not link.

## Results

| Check | Result |
|---|---|
| collection iteration (12 programs), builtin rows, runtime helper, mutable declaration, coverage, array parameter, component contract, SoT edge, `sot_registry_gate.py` | PASS |
| `make ci-push-linux`, prebuilt mode, 87 steps | Only step 25 (mutable declaration, before the five-leg change) failed for the change, and it passes after the change. Steps 5, 34 and 86 fail only locally: a TMPDIR outside the repository, a removed `bin/pgy.exe` copy, and no Rocq. |
| Default-LLVM sweep against the String foreach driver | 4 moved from refused to equal and nothing else moved: equal 518 -> 522 |

The 4 programs are `codegen/fixture/for_continue` and three under
`tests/cases/backend_compare/`: `for_in_array_string_stdlib`,
`for_range_explicit` and `process_args_count`.
