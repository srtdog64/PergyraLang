# Red-team counterexample campaign (2026-09-21)

Status: ATTACK RECORD. No semantic authority, progress counter or
implementation queue; executable gates and owners override it. Snapshot:
`main@67aa1aede9265accf1f0319981e072dbb7d08b57` with another session's
uncommitted collection-ownership work in the tree.

Six independent lanes attacked the compiler and its harness in parallel:
external MIR admission, oracle identity, test reachability, runtime semantics,
mutation testing of the gates, and adversarial review of the six most recent
commits. Each lane wrote its expected result before running a case and kept
all inputs and outputs under `.tmp/redteam-2026-09-21/<lane>/`. The companion
packet `2026-09-21_selfhost_semantic_parity_redteam_packet.md` belongs to the
collection-ownership lane and is not edited here; its S3 and A3 results are
reported below.

## Which binaries were attacked

The shared `bin/pgy.exe` is not landed code. It was built at 20:22 from the
uncommitted working tree (`3698a5e5...`) and rebuilt by another session at
21:54 (`6a0b58bc...`). `bin/pgy-self-driver.exe` (`371966e0...`) was unchanged
throughout. The code-review lane therefore built HEAD from a `git archive`
export into `.tmp/redteam-2026-09-21/code-review/head/bin/`: `pgy.exe`
`7fa57118...`, `pgy-self-driver.exe` `392808ad...`.

**Every cell in the first table not marked "lane" was reproduced by the
integrator on that HEAD build**, with programs written independently of the lanes' (under
`.tmp/redteam-2026-09-21/verify/`), so they are defects in landed code, not in
the dirty tree. R8 and R9 were reproduced on the shared driver `371966e0...`.
Everything in the second table is reported by one lane and was not re-run by
the integrator.

Legs: native = `--native-pipeline`; self-host = the default route, which
`src/pgy_driver.c:252-261` delegates to the installed driver.

## Defects reproduced by the integrator

| ID | Counterexample | Expected (source) | native C | native LLVM | self-host C | self-host LLVM |
| --- | --- | --- | --- | --- | --- | --- |
| R1 | `let mut b: Array<Int> = a; b[0] = 99; Log(a[0]);` | `1` -- value semantics are "not permission to shallow-copy" (`docs/mut_borrow_parameters.md:50`) | **99** | **99** (lane) | **99** | refused |
| R2 | `ArrayPush(names, "a"); let copy = names; ArrayPush(copy, "b");` then two `Log`s; no drop anywhere | prints `1`, `2` | 1, 2 | -- | **heap corruption `0xC0000374`, no output** | -- |
| R3 | `Unwrap(Fail())` where `Fail` returns `Err("boom")` | panic (`docs/105`) | semantic passes with 0 errors, then gcc fails: `PgyResult_Int_String` undefined | -- | **prints uninitialized memory** (`1977106432`, `1799569408` on two runs), continues, exit 0 | -- |
| R4 | `Log(ToString(9223372036854775807L))` | the literal | **-9223372036854775808** | **-9223372036854775808** | correct | refused |
| R5 | `Log(ToString(3.14))`, `Log(ToString(-3.5))` | `3.14`, `-3.5` | correct | correct | **`3`, `-3`**, exit 0 | refused |
| R6 | `Both(a, a)` with two `inout Int` parameters | refused | refused | -- | **accepted, prints 10** (one update lost) | lane: same |
| R7 | user `func Contains(...) -> Bool { return false; }` | the user function, or a refusal | **calls the builtin** | lane: same | calls the user function | lane: same |
| R8 | `--emit-mir-json-verified` on `backend_compare/slice_copy`, then `--mir-json-backend=c` | compilable C | -- | -- | **rc 0**, emits `pgy_array_slice_Int(&(pgy_scalar_routine_1()), ...)`; gcc: `lvalue required` | lane: compiles, correct output |
| R9 | a valid MIR document followed by `\0{"schema":"evil"}garbage` | refused (exact root grammar and EOF) | -- | -- | **accepted, artifact published** | lane: same |
| R10 | a refused compile with `-o` naming an existing file | the stale file is removed | **kept** | **kept** | **kept** | removed |

Why each one matters:

- **R1** is on the native pipeline, the harness's reference. Native already
  refuses the same copy for `Array<String>` ("cannot be shallow-copied"), so
  the rule exists and is missing for `Array<Int>`. The runtime lane also
  reports `ArrayDrop(a); ArrayDrop(b)` after the copy corrupting the heap and a
  read after `ArrayDrop(a)` returning reused memory.
- **R2** is an ordinary program on the default user route. The code-review lane
  traces it to the literal-shape cleanup policy
  (`direct_mir_scalar_program_array_string_cleanup_policy_owner.pgy:8-68`)
  freeing string literals, and to the handoff claim at `be2e0e16` that bounded
  Direct-MIR consumers reject non-empty ownership rows holding only on the
  graph-admission route.
- **R3** reads memory that was never written and reports success. The native
  side is a second defect: its semantic stage admits a type its C emitter
  cannot name.
- **R4** is invisible to every C-versus-LLVM gate, because both native legs
  agree on the wrong value. The lane locates the cause in
  `src/parser/ast_constructors.c:587`, which parses every number literal with
  `strtod`; `Log(9007199254740993L)` also prints `...992` natively.
- **R5**: the lane traces `ToString` on a Float to `pgy_tostr(long long v)`.
  No gate uses `ToString(Float)`; the codegen parity fixtures use `Log(Float)`.
- **R8** breaks C/LLVM parity from one verified MIR document, and
  `slice_copy_semantic_bridge_owner.sh` does not project slice MIR through the
  direct C route.
- **R10**: a script that compiles and then runs `out.exe` runs the previous
  binary after a refused compile. The mutation lane found the C route's
  stop-after-refusal guard can be disabled with every one of
  `hashmap_admission`'s 16 "refusal before publication" checks still green,
  because each check uses a fresh directory.

## Defects reported by one lane (not re-run by the integrator)

Severity classes: SILENT-WRONG (wrong value, exit 0) > MEMORY (heap corruption,
use after free, out-of-bounds read) > UNSOUND-ACCEPT > DIVERGENCE >
OVERSTRICT > DIAGNOSTIC.

| Lane | Finding | Legs | Class |
| --- | --- | --- | --- |
| runtime | `Consume(own s)` releases a Slot, then the caller's `Read(s)` prints 5 | self-host C; native refuses | SILENT-WRONG |
| runtime | `let t = s; Release(t); Read(s)` prints 1 | self-host C; native refuses ("Slot handles cannot be copied") | SILENT-WRONG |
| runtime | `defer` under dynamic `if`/`while` accepted; loop defers run once with the final value | self-host C; native refuses | UNSOUND-ACCEPT |
| runtime | `let x: Int = 4294967297;` prints 1 | native C/LLVM, self-host C | SILENT-WRONG |
| runtime | `ToInt("9999999999")`: 2147483647 / 1410065407 / 9999999999 held in an `Int` | native / self C / self LLVM | DIVERGENCE |
| runtime | `CharCode("ab", 100000, 3)` segfaults or reads out-of-bounds bytes | all legs | MEMORY |
| runtime | subject duplicated through `[a]`, `ArrayPush(xs, a)`, `Holder(a)`; copies diverge (100 vs 90) | native, self-host C | UNSOUND-ACCEPT |
| runtime | `Mix(s, s.a)`, `Both(s.a, s.a)` lose an update | native C (semantics compares bare names only) | SILENT-WRONG |
| runtime | `defer { return; }` overflows the native compiler's stack (`0xC00000FD`) after 0 semantic errors | native C | crash |
| runtime | `Pair(b.Bump(), b.n)`: 10 on C, 11 on LLVM | native and self-host | DIVERGENCE |
| runtime | for-each over an array being pushed to: 101 iterations natively, 3 on self-host | -- | DIVERGENCE |
| runtime / code-review | packet S3: an `Array<String>` of unknown ownership passes `ArrayDropOwnedStrings`; with non-empty elements, heap corruption | self-host C/LLVM; native refuses | MEMORY |
| code-review | MapKeys ownership fact is updated only at `let` (`type_checker_ownership_let.c:545`); an index write or reassignment leaves it stale and a later drop corrupts the heap | native C/LLVM, public C | MEMORY |
| code-review | `ArraySort` returns the same descriptor; dropping both frees twice, and the refusal diagnostic (`collection_ownership_fact.c:296-300`) recommends `ArraySort` | native C/LLVM | MEMORY |
| code-review | only an all-literal array literal counts as borrowed; `["x", Concat("a","b")]`, `[]` plus `ArrayPush("x")`, or a function returning literals is UNKNOWN, and UNKNOWN is allowed to drop | all legs | MEMORY |
| code-review | a non-identifier receiver (`ArrayDropOwnedStrings(b.items)`) has no binding and is admitted | native | MEMORY |
| code-review | `selfhost.zone_authority_rows` is CLOSED, yet self-host semantics and `--emit-mir-json-verified` accept `authorized by: worker` against an approver-only zone; only the MIR consumer refuses | self-host | UNSOUND-ACCEPT |
| code-review | `Array<Carrier<Future<Int>>>` locals and parameters verified into MIR; native refuses | self-host | UNSOUND-ACCEPT |
| code-review | user functions named `MapKeys` or `IsCancelled` compile to the builtin | native | SILENT-WRONG |
| mir-admission | duplicate JSON keys resolve differently per key: root `schema` first-wins, root `routines` last-wins, instruction `id` first-wins (538 of 544 fixtures accept a conflicting second `id`) | direct MIR | UNSOUND-ACCEPT |
| mir-admission | non-JSON inside skipped values is accepted (`[{]`, `NaN`, raw LF in a string, `\q`); `json_bounded_fact_read.pgy:48-132` counts only its own bracket kind | direct MIR | UNSOUND-ACCEPT |
| mir-admission | valid JSON is refused: whitespace after a scalar, a pretty-printed document (diagnostic: "machine-layer facts are missing or invalid") | direct MIR | OVERSTRICT |
| mir-admission | four route owners hard-code an InstructionId (`direct_mir_literal_log_plan_owner.pgy:156` and three others), so nine fixtures refuse every sparse relabeling the identity owner declares valid | direct MIR | OVERSTRICT |
| mir-admission | commit grows about quadratically with block count (1.5e4 blocks: 6.3 GB); 1e5 blocks crash with `0xC0000005` under a 10 GB cap, no budget refusal | direct MIR | crash |
| mir-admission | `-o ./<input>` bypasses the "paths must differ" check and overwrites the input | direct MIR | data loss |

## What the harness cannot see

| ID | Finding | Evidence |
| --- | --- | --- |
| H1 | 568 of 1013 tracked test entry points (56%) run on no automatic runner. 297 hang only off `self_host_parity.yml`, which has 26 failures and 2 cancellations in 28 runs and **no success**; its schedule was removed in `713be04b` (2026-09-14). Goals 12-37 of its make invocation were shadowed in 28 of 28 runs. 247 scripts appear in no CI log at all; of 146 run at HEAD, 81 fail. | run counts verified with `gh run list`; the rest from the reachability lane (`classification.tsv`, `results_head.tsv`) |
| H2 | The CLOSED registry row `projection.direct_mir_scalar_cfg_graph_plan` cites `one_mir_scalar_cfg_graph_projection.sh`, which no Makefile, workflow or script references and which fails at HEAD. 130 of the 245 gates cited by `docs/semantics/sot_owner_spine_registry.md` do not run automatically; 31 of 63 CLOSED rows cite at least one. `src/self_hosted/OWNERS.md` cites `intent_cell_transfer.py` as kernel evidence; it is unreached and fails. | reference absence verified; counts and failures from the lane |
| H3 | Size-cap gates are red at HEAD while push CI is green: `type_checker_ownership_let.c` is 621 lines against a cap of 599, and four runtime headers grew past 600 in `612cb85a`. They run only in the weekly workflow. | `type_checker_ownership_let.c` count and cap verified; the headers from the lane |
| H4 | `tests/gate_script_reachability_smoke.py` counts a Makefile mention as reached (so local-only and dispatch-only scripts pass), checks only `.sh`, and its `\bsh\b` pattern turns `require_file` lines into execution edges. | lane (`why_gate.py`) |
| H5 | About twenty legs labelled "native oracle" run the self-host driver; `driver_rung0/1_parity.sh` build their "C driver oracle" with the same command as the candidate (two builds differ only in a temp path). The meta-gate check added after the 2026-09-15 reconciliation is a fixed list of the three files that document named (`gate_subject_declaration_smoke.sh:115-119`), so it closed the instances, not the class. | list verified; the twenty legs from the lane |
| H6 | No gate compares native and self-host runtime output across a corpus. `collect_runtime_divergence.py` is manual. The backend-compare corpus shows six self-host runtime differences (all Float), and R4 shows the native oracle itself wrong where both of its backends agree. | lane |
| H7 | Mutation testing: removing the duplicate-InstructionId check survives every gate except one that tests an installed binary; disabling the C route's stop-after-refusal survives all 50 `hashmap_admission` checks; a behaviour-preserving rename turns four text-pin gates red; three gates are already red at HEAD, so they cannot kill anything (`default_llvm_installed_self_host_owner.sh`, `default_c_compile_installed_self_host_owner.sh` whose missing-driver assertion is unreachable, and the component contract on any fresh clone). | `.tmp/redteam-2026-09-21/mutation/summary.md` |
| H8 | Skips pass: `tests/fuzz_parity.py` skips 185 of 200 generated programs and exits 0; the tri-compare LLVM availability probe goes through the self-host driver and reports "SKIP: LLVM backend unavailable" with rc 0 when the driver is missing, masking a tri-compare that is currently red. | lanes |
| H9 | A Markdown-only push skips the gates that read Markdown: push gates read 41 tracked `.md` files, the Markdown branch runs 8 scripts that read none of them. A scratch commit deleting one required sentence was classified Markdown-only and passed all 8. | reachability lane (`md_only_demo.txt`) |
| H10 | `direct_mir_document_admission_owner.sh` pins its sparse-ID control on one seed; the same mutation applied to `hello` is refused (see the hard-coded IDs above). | mir-admission lane |

## Earlier findings, rechecked

| Finding (2026-09-15) | Now |
| --- | --- |
| F1 three oracle legs delegate | The three named legs declare `--native-pipeline` since `aeccbd64`; the class remains (H5). |
| F2 meta-gate one-directional | A fixed three-file list (H5). |
| F3 unreached scripts | The five named scripts joined the platform `harness-evidence` shard on 2026-09-15; its first run (2026-09-20) failed on `one_mir_string_case_math_projection.sh`. The class grew (H1). |
| F4 persistent work directory | Fixed: `mir_json_parity.sh:79` uses `mktemp -d` per run. |
| F6 Markdown-only gate | Fixed: `protocol_registry_smoke.sh` runs in push core. The reverse direction remains (H9). |

Closed since the earlier reports, per the runtime lane: `35_subject_param_alias`
prints 90 on every leg, all four `zone_spawn_*_boundary_rejected` fixtures are
refused on every leg, Channel parameters are refused natively, and the concat
evaluation-order case is fixed.

Companion packet: **S3 fails** (see above). **A3 is answered the other way
round**: the tri-compare C and LLVM legs share the native front end, not the
self-host one, so C-equals-LLVM cannot see a native front-end defect such as R4.

## Priority register

Nothing here has started.

| Priority | Change | Retires |
| --- | --- | --- |
| P0 | Refuse the `Array<Int>` shallow copy natively as `Array<String>` already is | R1 |
| P0 | Stop the self-host scalar-program route freeing literals; refuse non-empty ownership rows there as the graph route does | R2, S3 |
| P0 | `Unwrap`/`UnwrapOption` panic on the self-host routes; native emits every type its semantics admits | R3 |
| P0 | Remove a stale `-o` artifact on refusal on native C, native LLVM and self-host C; add a pre-existing-output case to the C gates | R10, M6 |
| P0 | Make `self_host_parity.yml` report instead of stop (run goals to completion with pass/fail/unreached counts) or stop citing its gates as evidence; reopen CLOSED rows whose cited gates no runner reaches | H1, H2 |
| P1 | Integer literals parsed as integers natively; `ToString(Float)` on the self-host C route | R4, R5 |
| P1 | A corpus gate comparing all four legs' runtime output with independent expectations | H6, R4-R7 |
| P1 | Name resolution: user functions shadow builtins identically on both pipelines, or both refuse | R7 |
| P1 | Strict JSON at the external MIR boundary: EOF after NUL, duplicate keys, skipped-value validity, valid whitespace | R9 and the mir-admission rows |
| P1 | Source-built negatives for the duplicate-ID check, so a source mutant is visible without a driver rebuild | H7 (M2) |
| P1 | Replace the three-file oracle list with a check on every leg labelled oracle | H5 |
| P2 | Size-cap gates in push CI; the reachability ratchet counting executions, not mentions; the Markdown bypass | H3, H4, H9 |
| P2 | An explicit size budget for external MIR, and the dominance query's per-call search | mir-admission scale rows |

## Not measured

Linux, sanitizers and Rocq were not run. Self-host LLVM refuses most programs
using subjects, slots, floats or nominal types before emitting code, so that
leg is unmeasured for them. Channels, `pin` and `ArrayDrop` are unsupported on
the self-host routes. Leak and release balance were not measured. The external
MIR 1e5-block case ran under a 10 GB cap only.

Evidence: `.tmp/redteam-2026-09-21/` -- `verify/` (integrator reproductions),
`runtime-semantics/` (213 programs with `// EXPECT:` headers), `code-review/demo/`
and `code-review/head/bin/`, `mir-admission/verdicts.tsv`,
`oracle-integrity/`, `reachability/classification.tsv`, `mutation/summary.md`.
