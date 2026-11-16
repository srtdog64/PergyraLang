# 2026-11-16: general direct MIR emitter coverage for the compiler's own sources (DM1)

Track DM1 of the Claude root lane, after the
[DM0 refusal census](2026-11-16_direct_mir_drv2_refusal_census.md). Run on
WSL `Ubuntu-C-WSL`.
- Branch `worktree-agent-ac9333b20370260d3`. The legs and census ran on
  base `1fc2e549a` plus the branch; the branch was then rebased onto
  `3ad7444db`, whose commits touch no direct MIR file. The gate, the CI
  shards and the fixed point ran on `/root/tdm1` rebuilt at `821b3cc8e`.
- Census drivers, never committed: `/root/tdm1b` (base) and `/root/tdm1d`
  (branch), each with the DM0 census patches and the additions under
  "Method".
- Baseline for the leg sweeps: `/root/mg-main/bin` (main-equivalent).

## What the general emitter now covers

| Family | MIR or expression row | Owner |
|---|---|---|
| HashMap read on a record member | expression kinds `MapGet`, `MapHas`, `MapSize`, `MapKeys` whose receiver is a `LogicalRecordMember` chain ending at a Local or Parameter. C takes the member's address. LLVM reads through a stack copy of the map header; the buckets stay shared and no read changes the header. `MapSet` and `MapRemove` still need a Local or Parameter receiver. | `scalar_program/direct_mir_scalar_program_hashmap_owner.pgy`, `..._llvm_hashmap_owner.pgy`, `..._c_hashmap_owner.pgy` |
| non-Void routine that ends by calling a `Never` routine | a terminal block with no return row whose last operation is a discarded `DirectCall` to a routine that never returns. MIR reads `Never` as `Void`, so divergence is derived from the CFG: a routine diverges when every terminal block ends in `Exit` or in a call to a diverging routine (least fixed point; a Void routine's dead return row after the exit does not count). Both emitters already end such a block as unreachable (`unreachable`, `abort()`). | `scalar_program/direct_mir_scalar_program_diverging_call_owner.pgy`, `scalar_cfg/direct_mir_scalar_cfg_program_extension_admission_owner.pgy` |
| HashMap lifetime with an Int `Main` and `Exit` | readiness code 27 no longer needs a `Void` entrypoint and no longer refuses `ProcessExit`. Commit `667f11ec0` released maps only on the Void entrypoint's own exits, so it needed both. Map cleanup now runs on every return of every routine through the owned-local cleanup, the entrypoint's Int return included. `Exit` leaves the live maps to the operating system, as native `Exit` skips Main's cleanup ([decision](../decisions/2026-11-15_main_int_result_is_exit_status.md)); no code runs after it. The alias, creation and use proofs are unchanged. | `scalar_program/direct_mir_scalar_program_hashmap_lifetime_owner.pgy` |

## Gate

`tests/self_hosted/parity/direct_mir/direct_mir_general_emitter_coverage_owner.sh`
(push core shard). It passes with the branch driver and fails with the
`/root/mg-main` driver at its first positive.

| Case | Kind | Result on the branch |
|---|---|---|
| `map_member_read`: `MapHas`/`MapGet`/`MapSize`/`MapKeys` on members of a by-value, a `ref` and a local record | positive | same stdout and exit 0 on native C, native LLVM, default C, default LLVM and the direct MIR C backend |
| `never_tail`: `String` and `Int` routines that end in `Fail(...)` | positive | five legs, exit 3 |
| `never_wrapper`: a `Never` routine that ends in another | positive | five legs, exit 4 |
| `map_exit`: map locals, Int `Main`, `Exit` in a callee and in `Main` | positive | five legs, exit 2 |
| `map_int_main`: `return MapSize(counts)` from Int `Main` | positive | five legs, exit 2 |
| `map_member_write`: `MapSet` on a member of a by-value formal | neighbour | default LLVM refuses (expression set), native prints 7 |
| `map_alias_exit`: map alias with Int `Main` and `Exit` | neighbour | default LLVM refuses (`program_readiness=27`), native prints 0 |
| MIR mutation: `Fail`'s `Exit` replaced by a Void return | negative | refused, `terminal return is invalid` |

The five-leg positives were run again under ASan (`/root/legs_lib.sh`, LLVM
at `-O0`; direct MIR C with gcc ASan/UBSan): all clean.

## Census

Method:
- MIR from `pgy-self-driver --emit-mir-json-verified` of `/root/tdm1b`; both
  census drivers read the same files with `--mir-json-backend=llvm`, under a
  watchdog that kills a process above 9 GiB.
- The DM0 patches apply to the branch except the route-admission hunk,
  whose one line called the role predicate the branch then had (see
  "Role declarations"). No census input has a role declaration, so the
  counts below do not depend on it.
- Added to both census drivers: `owned_local_lifetime_owner.pgy` gained two
  Dies after DM0 (`23989a49a`). The first one stopped the `semantic` census
  inside the HashMap lifetime check, so both now record
  `lifetime code=taken-twice|taken-each-pass` and go on.
- The DM0 readiness census keeps its own copy of the block checks (codes
  5-6) and of the code-27 attribution. On the branch the copy calls the new
  divergence predicate, and code 27 is recorded per failing map local by the
  real check instead of by `Exit` sites.
- Routines are grouped as in DM0 (`routine=`, or the expression's
  `global_row` through the MIR).

Refused routines per input (a routine counts once; "only" counts the
routines for which the group is the only cause):

| Input | Base | Branch | Groups that changed (routines, only-cause) |
|---|---:|---:|---|
| `semantic` | 51 | 31 | code 27 `Exit` in a HashMap program 20 (20) -> 0 |
| `mir_lower` | 86 | 52 | `MapHas` on a member 31 (22) -> 0; `MapGet` 8 (0) -> 0; `MapSize` 3 (1) -> 0; code 6 2 (2) -> 0; code 8 root 13 2 (0) -> 2 (1) |
| `codegen` | 110 | 59 | `MapHas` 31 (22) -> 0; code 6 16 (16) -> 0; `MapGet` 9 (0) -> 0; `MapSize` 5 (3) -> 0; `MapKeys` 1 (0) -> 0; collection transition 40 (23) -> 39 (23); code 8 root 13 2 (0) -> 2 (1) |

- No routine is refused on the branch that the base did not refuse, and no
  cause appears that the base did not have. The code-8 change is the same two
  routines; one of them has no other cause left.
- The transition group loses `SemanticAstBodyTypeBundleFocusRecoveryReceiptDigest`
  on `codegen`. A `MapKeys` result is a fresh owned definition only when its
  node is ready (`direct_mir_scalar_program_owned_collection_definition_owner.pgy`);
  its receiver is a member, so the base did not know the definition. The
  runtime call is the one a Local receiver already uses.
- `semantic` stays refused as a program by code 27. With `Exit` and the Int
  entrypoint gone, the first map that fails is created outside its routine's
  entry block (`direct_mir_scalar_program_hashmap_lifetime_owner.pgy`, the
  `block_starts[result_routine]` check). The MIR has 8 such `MapNew()`
  definitions in 4 routines on `semantic` (`SemanticCallableNameTableFromNames`,
  `SemanticAstEnumFactsFromArtifact`, `SemanticAstFunctionSignatureFactsFromArtifact`,
  `SemanticEnumCallableProjectionContractReady`), 45 on `mir_lower` and 72
  on `codegen`. The base never reached this check because the first `Exit`
  refused the program.
- `mir_lower` and `codegen` stay refused as programs by the collection
  ownership transition (code 26), the owned `Array<String>` move (code 23)
  and the expression nodes still not ready, as in DM0.

## DRV-2

The DM0 DRV-2 MIR (`/root/tdm0/.tmp/dm0/driver_rung2.mir.json`, stock
`--mir-json-backend=llvm`, 9 GiB watchdog) is refused at route stage
`role-declaration`, as with the DM0 driver (320 s). It has 15 roles, none
with a `self` parameter; 78 HashMap reads written on a member; 26 non-Void
terminal blocks that end in a call to one of its 5 `Never` routines; 64
`Exit` statements (text counts over the MIR).

## Role declarations: tried and dropped

A branch commit let the general route admit role declarations whose
methods name no `self` parameter, and refused role operator nodes in
expression admission. With it, the DRV-2 MIR passed the route and was
refused at routine admission, ordinal 1050, stage
`collection_ownership_transition` (680 s, peak 3.7 GB).

The push core shard then failed `tests/self_hosted/parity/domain/role_override_mir_replacement.sh`.
In MIR every role method carries an implicit receiver
(`receiver_carriage` `mutable-identity`, `params` `[]`), and a role
`override` has the same shape as an ordinary role method. The general route
claimed the role override program, emitted the role method without the
erased `void *self` receiver that a role slot needs, and checks none of the
override identities whose mutations that gate requires refused. Commit
`821b3cc8e` puts every role program back with the role routes.

Missing facts for the general emitter: the erased role receiver ABI row and
the role override identity. Both live today in the per-shape role routes
(`direct_mir/direct_mir_role_override_*`); the last consumer is the C and
LLVM routine signature emission. The DRV-2 MIR's role methods have no
override and no party, but the general emitter would still have to emit
their receiver.

## Self-host LLVM leg (sl) on the lifetime fixtures and tests/cases

3,042 programs: the 868 lifetime fixtures and the 2,174 `tests/cases`
programs that declare `Main`. Admission is `pgy --emit-llvm` through the
installed driver.

| Driver | Admitted |
|---|---:|
| `/root/mg-main/bin` (baseline) | 782 |
| branch | 792 |

- Newly admitted: 10. Newly refused: 0.
- All 10 run ASan-clean at `-O0` on sl, with the same stdout and exit status
  as native LLVM:
  - `function_param_flow_summary/default_aggregate_move_map_field_read_valid`;
  - nine `collection_field_lifetime` fixtures: `exclusive_map_fresh_positive`
    and `map_payload_{clone,disjoint,formal_read,int,key_copy,read_before,remove_key,unrelated_unknown}_positive`.
- The same 10 also build through the direct MIR C backend and run clean
  under gcc ASan/UBSan with the same stdout and exit status.

## CI shards and fixed point at `821b3cc8e`

`/root/tdm1`, prebuilt pair, `PATH=/root/nowsl-bin`, no `bin/pgy.exe`,
`TMPDIR=/root/tdm1-tmp` so the CI build and bin directories belong to this
tree alone. An earlier core run used the shared default `/tmp` while
another tree's shard ran there; its results are not used.

| Run | Result |
|---|---|
| `PGY_CI_SELF_HOST_MODE=prebuilt PGY_CI_PUSH_LINUX_SHARD=core PGY_ALLOW_MISSING_COQ=1 make ci-push-linux` | all 64 steps ok (1,393 s) |
| same, shard `self-host` | all 14 steps ok (1,321 s) |
| `driver_bootstrap.sh`, full fixed point, seed part | `gen2 == gen3` (240,782 lines, 1,394 s) |

The core shard includes the DM1 gate, `role_override_mir_replacement.sh`
and the scalar program owner caps.

## Left, with the missing fact

- `ArrayStringPushOwned` on a member (7-8 routines). Every census case
  pushes into a member of a by-value (`value`) record formal that the
  routine grows and returns (`SelfMirCfgAddInstruction`,
  `SelfMirRuntimeValueCallAbiRows*`). The direct MIR has no current-storage
  slot for such a formal: `DirectMirScalarProgramParameterCurrentStorageLocalRow`
  admits only value-result and owned `Array<String>` formals, and the
  existing `ArrayPush` on a member of a `value` formal reaches the same Die
  at emission. Missing fact: the in-place storage of a grown, returned
  by-value record formal; owner `scalar_program/direct_mir_scalar_program_scalar_value_result_storage_owner.pgy`;
  last consumer the LLVM and C array mutation emitters. A local-root version
  was written and dropped: no small program reaches it past the route seal.
- Inout indexed-assignment facts (5 routines) and nested member push/pop
  statements (3): each routine also has a collection-transition or
  admission refusal, so neither moves the count; not changed.
- In-place `ArrayReverse` on `Array<String>` (1 routine, also transition
  refused): needs a String reverse runtime ABI row; not changed.
- `CompilerRetireArrayStorage` (5 DRV-2 routines): a manual drop that the V1
  ownership goal removes from the canonical program; not admitted.
