# Whole-Repository Red-Team Reproduction Audit

Date: 2026-09-24 (Asia/Seoul)

Status: read-only audit record. This document does not own compiler
semantics, active progress, registry status, or a successor implementation
rung.

## Scope and Baseline

The audit covered the current dirty `main` checkout, the active self-host
collection-ownership rung, runtime gates, the SoT registry, gate
reachability, stable-subset claims, handoff navigation, and language-surface
safety counters.

The current source was rebuilt in isolated `.tmp` directories without
changing tracked source:

```text
C-only compiler:
  D:/PergyraLang/.tmp/analysis-build/bin/pgy.exe
  SHA-256: 18014D35F8588094BA2C775B49574ED1CDD699C7067194942CBC38582A0223E7

C + LLVM compiler:
  D:/PergyraLang/.tmp/analysis-llvm/bin/pgy.exe
  SHA-256: 10DF1DFC32954A90059B1E7A8E52C4A08A54792DAE3E256ECB614F80AE3CDD10
```

The checkout remained at:

```text
HEAD: e48ca6320d7cba1dd47984a1d14e92b7dad6fb2d
branch: main
status entries: 182
tracked modified: 85
untracked: 97
tracked diff: 85 files changed, 7552 insertions, 482 deletions
git diff --check: PASS
```

ASan and TSan could not run because the installed MinGW toolchain has no
`libasan` or `libtsan` link libraries. No intentional unsafe executable was
run. Findings are limited to compile-only admission, safe runtime oracles,
generated C, and executable gate results.

## Confirmed Findings

### P0: `Array<T>` value copy aliases backing storage on both native backends

`tests/self_hosted/fixtures/p0_array_value_copy_isolation.pgy` declares a
local `Array<Int>` value from another local and mutates the new value. The
required output is:

```text
1
99
```

The fresh C + LLVM compiler produced:

```text
99
99
```

on native C, native LLVM, and installed self-host C. The generated C at
`.tmp/analysis-r1-array-copy.c` contains:

```c
_pgy_ssa_copy_1 = _pgy_ssa_source_1;
pgy_array_set_Int(&_pgy_ssa_copy_1, (size_t)(0), 99);
```

`PgyArray_*` owns a backing pointer, length, capacity, and allocator. The MIR
local assignment owner in `src/codegen/transpiler_mir_assignment_emit.c`
renders the assignment as a direct C struct assignment, with no typed
copy-versus-move fact and no fresh backing allocation.

Required closure: one typed collection value-materialization fact at every
observable let, assignment, and call boundary, separate lifetime evidence,
fresh storage for value copies, and mutation/reassignment/copy-fact deletion
negative gates.

### P0: Installed self-host accepts shallow `Array<String>` aliasing

The fresh P0 oracle was run with:

```text
PGY_BIN=.tmp/analysis-llvm/bin/pgy.exe
PGY_SELF_DRIVER_BIN=bin/pgy-self-driver.exe
bash tests/self_hosted/parity/p0_runtime_counterexamples.sh
```

It reported seven failures: native C and LLVM R1 copy aliasing, installed
self-host R1 aliasing, installed R2 shallow `Array<String>` acceptance,
installed R3 `Unwrap(Err)` success and post-error execution, and installed R5
`Float`/`Double` `ToString` precision loss.

The expected R5 output is:

```text
3.14
0.5
100
0
0.333333
6
```

The installed self-host C output was:

```text
3
0
100
0
0
6
```

The fresh native source rejects the shallow-copy negative and handles
`Result` and `Float` correctly; the production installed artifact does not.
The installed negative owner gate fails on
`deep_drop_nonbinding_literal_invalid`.

Required closure: rebuild and install the self-host driver from current
source, then make the installed owner consume the same typed collection,
Result, and formatter facts as native C/LLVM.

### P1: Current self-host codegen source fails native semantic admission

The fresh native compiler invoked on `src/self_hosted/codegen/main.pgy` with
native MIR JSON diagnostics produced:

```text
49 error(s), 1 warning(s)
```

The active handoff and SoT registry still record 48 errors and one warning.
The observed diagnostics include unresolved `ArrayDropOwnedStrings` element
ownership, `ArrayPop` without a stable collection ownership fact, `own
String` call-site transfer without producer provenance, and owned-element
array literal transfer/refusal mismatches.

The source cannot be promoted to an installed self-host production path
until its exact semantic facts are admitted. This proves the handoff and
registry diagnostic counts are stale, not that self-host replacement is
complete.

### P1: The active SoT registry row fails its own machine gate

`python scripts/sot_registry_gate.py` returned exit 1:

```text
[sot-authority-edge] FAIL semantic.hashmap_collection_ownership:
enforcement_gate required text is absent from
tests/hashmap_owned_string_provenance_smoke.sh:
local String producer transfer and nonbinding deep-drop C-LLVM refusals PASS
```

The `semantic.hashmap_collection_ownership` row in
`docs/semantics/sot_owner_spine_registry.md` combines multiple
`path#required-text` witnesses with semicolons and free-form prose. The
registry parser expects comma-separated evidence references and checks each
required string in the referenced file. The row is therefore not a valid
machine-checkable evidence list.

Required closure: normalize the row to the registry's actual CSV evidence
format, keep each witness path and required text independently checkable,
and run `scripts/sot_registry_gate.py` as a required gate.

### P1: Forty-nine executable gates are not reachable

`python tests/gate_script_reachability_smoke.py` returned exit 1:

```text
scripts=865 target-reachable=760 manual=56
manual-registry-historical=1 undeclared=49 stale=0 dual=0
```

The missing set includes concurrency, collection-call, mutation-ownership,
runtime-value-lifecycle, parser, and numerous Direct-MIR projection gates. A
gate that exists in the inventory but cannot be reached from the declared
build/CI topology is not current semantic evidence.

Required closure: connect each relevant gate to a named Makefile/CI target
or classify it with a narrowly justified historical/manual reason.

### P1: Stable documentation claims red `Array<T>` behavior

`docs/107_beta_stable_subset.md` says a feature is stable only when the
full `syntax -> semantic -> runtime -> C -> LLVM -> diagnostics -> regression`
chain is coherent, then lists `Array<T>` as Stable. The fresh runtime oracle
shows both native backends violating the value-copy contract. The README
simultaneously calls the beta subset a candidate being frozen.

Required closure: downgrade affected rows to explicit beta-open/reject until
their owner, negative gate, and native/installed parity are green, or
narrow the stable contract to exclude the unproven operation.

### P1: Handoff navigation is stale

`docs/current_work_handoff.md` records HEAD `951e04df` and 211 status
entries. The actual checkout is HEAD
`e48ca6320d7cba1dd47984a1d14e92b7dad6fb2d` with 182 status entries. The
handoff records the old gate inventory as 841 total, 737 reachable, 54
manual, and 50 undeclared. The current gate reports 865, 760, 56, and 49.

Required closure: add a machine-checked handoff freshness gate comparing
recorded HEAD, status class, and active gate output with the live checkout.

### P2: Self-host status census is stale

`docs/self_hosted/09_selfhost_status.md` records `49/36/1` and a 37 nonclosed
row audit. The current registry block contains 95 rows with:

```text
CLOSED=65 BRIDGE=28 ACTIVE=2
```

The status document must either be explicitly historical or generated from
the registry with a mismatch gate.

### P2: Channel starvation gate fails open without LLVM

Running the C-only fresh binary produced C passes, an LLVM skip, a claim of
success on both backends, and exit 0. The same test passes with the
LLVM-enabled compiler, but the C-only path can falsely satisfy a two-backend
contract.

The gate is `tests/channel_pool_starvation_probe.sh`; the runtime owner is
`src/runtime/pgy_parallel_pool_lifecycle.h`.

Required closure: track executed backend count and fail if a required
backend was skipped, or report only the backends actually executed.

## Additional Confirmed Surface Findings

### P0: `own` enum/ADT double-consume is accepted

The known external fixture
`F:/JDW_project/geukbit-core/tests/known_compiler_gap/owned_submission_outcome_reuse.pgy`
was compiled with the fresh C-only and C+LLVM compilers. Both produced an
artifact with exit 0 despite the intended single-consume contract.

The relevant owners are `src/semantic/type_checker_ownership_classify.c` and
`src/semantic/type_checker_ownership_call.c`. The owned enum/ADT is classified
as copy-only, so `ConsumeOutcome(outcome)` can be called twice. This fixture
must not be executed before compile-time rejection is closed.

### P0: `MapGet`/`ListGet` borrows can outlive their container

Compile-only admission was reproduced for
`MapGet -> MapRemove -> Log` and
`ListGet -> ListRemove/Set -> Log`. Relevant owners are:

- `src/semantic/type_checker_builtins_stdlib_map.c`
- `src/semantic/type_checker_builtins_stdlib_collections.c`
- `src/runtime/pgy_runtime_map_string_inline.h`

The returned element is represented as a borrowed pointer/value while the
container can free or replace its storage. The unsafe binaries were not run
in this audit, but the compile-only admission is a confirmed boundary
failure.

Required closure: carry element provenance and borrow lifetime through
semantic/HIR/MIR admission and reject invalidating mutations while the
borrow is live.

### P1: Contextless `Some` is accepted by C but rejected by LLVM

The fresh C-only compiler accepted
`tests/self_hosted/fixtures/llvm_option_contextless_some_negative.pgy` and
wrote a C artifact. The C+LLVM compiler rejected the same fixture because
contextual `Option<T>` is required and anonymous Option fallback is disabled.

The affected path is `src/codegen/transpiler_call_result_option_builtin_emit.c`.
The source owner should reject contextless `Some` before backend selection,
or both backends must consume one identical Option-context fact.

### P1: Generic unification failure is diagnosed too late

The fresh native compiler rejected
`tests/cases/generic_falsification/f_unify.pgy`, but emitted a MIR lowering
error instead of the expected semantic generic diagnostic:

```text
MIR generic call 14 cannot resolve consistent actual/formal bindings
```

Relevant lowering is `src/compiler/mir_generic_method_specialization.c`.
The actual/formal constraint decision should be owned by the semantic DAG
and consumed by MIR.

### P1: Capability clause omission can admit privileged builtins

Compile-only evidence shows a helper without a capability clause can call
`ReadFile` and receive a C artifact. The owner is
`src/semantic/callable_capability_inference.c`; policy documentation is
`docs/semantics/15_capability_sandbox.md`.

The owner must explicitly retain optional capabilities with a narrower
stable classification, or make missing capability declarations fail closed.

### P2: `ToInt` silently maps invalid text to zero or a prefix

The safe runtime fixture showed both native C and LLVM accepting:

```text
ToInt("abc") == 0
ToInt("12abc") == 12
```

Relevant paths are `src/runtime/pgy_runtime_scalar_std_inline.h` and
`src/runtime/pgy_runtime_lib_std_exports.h`. Prefer a text-conversion result
contract such as `TryToInt`, or document the compatibility rule with a
negative gate.

### P3: Some documentation links are broken

Repository-relative links in `docs/README_ko.md` and
`docs/71_beta_execution_tickets.md` resolve as `docs/docs/...`, and split
targets referenced by `CHANGELOG.md` are absent. The link checker must use
source-file-relative resolution; historical changelog links need archive or
rebased paths.

## Gates That Passed on the Fresh Current Tree

These are supporting evidence, not closure:

- fresh C + LLVM compiler build: PASS;
- `tests/array_drop_storage_smoke.sh`: PASS;
- `tests/collection_parameter_boundary_smoke.sh`: PASS;
- `tests/channel_pool_starvation_probe.sh` with LLVM enabled: PASS;
- `tests/hashmap_owned_string_provenance_smoke.sh`: PASS;
- `tests/documentation_quality_smoke.sh`: PASS;
- `tests/self_host_progress_metric_smoke.sh`: PASS;
- `tests/parallel_core_contract_smoke.sh`: PASS;
- `tests/ownership_relocation_cleanup_contract_smoke.sh`: PASS.

## Improvement Order

1. Close the typed `Array<T>` copy-versus-move materialization seam first.
2. Rebuild/install the self-host driver and make P0 native/installed parity
   blocking.
3. Close the self-host codegen source-admission errors without introducing
   duplicate allocation or leak fallbacks.
4. Normalize the SoT evidence row and make its machine gate mandatory.
5. Resolve unreachable gate paths through real Makefile/CI reachability or
   narrow historical/manual classification.
6. Downgrade stable rows that are not evidence-backed and repair stale
   handoff/self-host census documents.
7. Close owned-ADT double-consume, contextless-Some, generic-owner,
   capability, Map/List borrow-lifetime, and ToInt findings with focused
   negative/positive gates.

## Non-Claims

This document does not claim that Pergyra is production-ready, memory-safe,
fully self-hosted, or fully backend-parity green. It does not execute the
known unsafe ADT or borrow-lifetime binaries. It does not replace the SoT
registry, handoff, or executable gates.

## Coordination Board: Open Work by SoT Owner

This section is the one channel between the agents working on these
findings. Another note (handoff, chat, directive) should point here rather
than keep its own copy. Written 2026-09-24 by the verifier session (Claude)
at `origin/main` = `4141904d`.

### Protocol

- Claim a task by rewriting its `Status:` line as
  `CLAIMED <agent> <date>`. Hold one task at a time.
- Work in your own worktree and branch from `origin/main`. Never stage or
  commit in `D:\PergyraLang`: it holds another lane's 192-entry WIP. Never
  edit a file listed under that task's "Do not edit".
- When done, set `Status: READY <branch> <head>` and list the gates you ran,
  each with its last output line. The verifier lands it with the 3-way
  script, reruns the gates and the default-route sweep, and writes
  `LANDED <commit>` or `REJECTED <reason>`.
- Put a question or blocker under the task as `Q (<agent>):`; the answer
  goes directly below it as `A (<agent>):`.
- Commit messages: no `Co-Authored-By` trailer; they must pass
  `check-pr-text.py`.
- A new push-CI step must fit the budget: build-linux core took 16m40s on
  run 35990536278 against a 30-minute limit. Read the last green run's step
  times before adding one.

### Environment

- WSL (Linux builds, Linux-only gates) is DOWN: drive E: is full, and the
  distro's VHD lives there (`Wsl/Service/E_UNEXPECTED`). E: is the user's
  data drive; only the user frees it. Until then, build the C compiler with
  MSYS2 `make` on Windows and run text gates there; the verifier or CI runs
  the Linux gates.
- CI: `415b18d3` build-linux green; its self-host shard was cancelled at 30
  minutes by a DRV-2 rebuild. `4141904d` admits the prebuilt pair (the shard
  then ran in under four minutes) but its component contract read the
  admitted Makefile; `3659ed25` makes that check read build mode. Push CI
  run 35999155837 on `3659ed25` is GREEN on every job: build-linux 20m42s,
  self-host contract shard 10m56s, self-host bootstrap 47m. Branch new work
  from `3659ed25` or later.

### Not delegated

- `semantic.hashmap_collection_ownership` (ACTIVE rung, other lane): R1, R2,
  R3, R5, `deep_drop_nonbinding_literal_invalid`, the `mir_lower` native
  admission errors. Their WIP covers `src/self_hosted/{mir,mir_lower}`,
  `src/compiler/{hir*,mir_*}`, six `src/self_hosted/semantic` owners and
  four `src/codegen` files.
- Decisions for the user: the IoError builtin variant namespace, the `ToInt`
  contract, optional capabilities (PP-024).

### Tasks

Verifier, 2026-09-25: T1, T3, T4 and T5 landed together as
`462c10fa..9e6dc6f6` on top of `3659ed25`. The integrated tip was rebuilt on
Windows (compiler and self-host driver) and passed each card's gate, parser
parity (191 sources), semantic parity (118 fixtures), the component
contract, likeness, keyword registry, CI profile, test-semantic 2954,
test-transpile 979, test-parser, preparation-contract (without Coq) and
builtin surface. Full backend compare: 950/951; `cancel_propagation` failed
once under load and passed 10/10 alone.

Push CI run 36026319539 on `9e6dc6f6` failed one step:
`grammar-cheatsheet-contract-test-smoke` rejected T1's fixture line
`apply burn to ;` (no `;` after a world-layer fact in authored tests).
`1dbe3baa` writes it as `apply burn to }`; the row still refuses at line 10
with `expected_token`. Push CI run 36032405829 on `1dbe3baa` is GREEN on
every job (build-linux 14m32s, self-host contract shard 10m, self-host
bootstrap 48m). Branch new work from `1dbe3baa` or later.

Verifier, 2026-09-25: T8 and T6 landed together as `b4467abe..c488e14b`
on `1dbe3baa`. Windows integration verify: each card's gate, builtin surface,
semantic parity (118), codegen parity (85), component contract, likeness,
keyword registry, CI profile, cheat sheet, test-semantic, test-transpile,
test-parser, test-mir, full backend compare 953/953 and preparation-contract
(without Coq). preparation-contract first failed: the committed registry row
`diagnostic.catalog` requires the receipt gate to print "stdout/file bytes,
exact Pergyra receipt, and opaque C relay", which T8 had reworded; the gate
prints that text again (folded into `9c32e7bd`).

Push CI run 36054441549 on `c488e14b` failed two jobs, and `c9d5dd70` and
`991925c9` repair them:
- self-host-bootstrap-linux: the native oracle emission of the driver stopped
  with "MIR-only C path missing hosted self-call inference method metadata for
  'DriverSourceLlvmIntentExecution.DriverSourceLlvmRouteRefused'". T8 built
  the new enum variant directly inside a subject action; the other variants
  go through top-level functions, and now this one does too. The Windows
  integration verify did not run this emission; `pgy --native-pipeline
  --emit-c` of `driver_bootstrap_main.pgy` is now part of the verifier's
  check for any driver-source change.
- build-linux: T6's known-open row expected `unknown type name 'Crate_Int'`
  with ASCII quotes; gcc on Linux prints U+2018/U+2019. The row matches the
  two parts separately.
- Open, compiler: on the native MIR-only C path a bare enum variant
  constructor inside a hosted body is inferred as an implicit-self method
  call and refused. The wrapper function is a workaround, not the fix.

T2 landed on top as `161a78a9..34acf347` after its own Windows integration
verify (18 gates, including the manifest, DIR and vision-surface gates) and
the oracle emission on the combined tree. Push CI run 36059840496 on
`34acf347` is GREEN on every job (build-linux 17m26s, self-host contract
shard 8m34s, self-host bootstrap 39m). Branch new work from `34acf347`.

Verifier, 2026-09-25: two platform-full gates were red on main, both from
this campaign's own commits, and push CI does not run them.
`semantic_core_shape_smoke.sh` refuses direct AST/Type payload reads:
`parallel_capture_write_reach.c` (d88d7e48 and 5b340d67),
`parallel_capture_storage_reach.c` and `type_checker_call_generic_where.c`
(3c533049, a7921a5e) read `node->data.<kind>` and `type->data` directly.
`mir_declaration_inventory_smoke.sh` still pinned two terms that ed8ba8ca
reorganised. The accessor rewrite and pin moves are in `fix-shape`, verified
with T9, T10, T12 and T13 in one integration (integ4).
integ4 passed on Windows (driver oracle emission, 7 new compare cases, the
two platform gates, generic, parallel, diagnostic, builtin surface,
semantic parity, contract, likeness, unit suites, preparation-contract,
full compare 958/958) and landed as `5d39885d..0e63e724`, the accessor fix as
`0e63e724`. Push CI run 36080168245 on `0e63e724` is GREEN on every job
(the run for the partial push `fd19f0e5` was cancelled by it).
T11 landed on top as `0ec30e1f..2ca58486` after its own Windows integration
verify (driver oracle emission, 58 broken inputs, parser and semantic
parity, builtin surface, contract, likeness, keyword registry, cheat sheet,
preparation-contract). Push CI run 36083559448 on `2ca58486` is GREEN on
every job. All twelve delegated cards (T1-T6, T8-T13) are landed; branch new
work from `2ca58486`.

Found while fixing it, open: `apply burn to 42` passes the self-host parser
and then dies in DIR with `self-host DIR apply directive field identity is
missing`, with no code. The parser should refuse a non-name apply target.

| ID | SoT owner (registry row) | Finding | Delegate |
|---|---|---|---|
| T1 | `diagnostic.catalog` (self-host parse refusals) | bare `Exit(1)` in declaration parsers | yes |
| T2 | `selfhost.semantic_artifact_admission` | concurrency surface fails in MIR with no code | yes, with care |
| T3 | `abi.layout_rows` (self-host C emission) | `Try` on an enum error fails in the C compiler | yes |
| T4 | `diagnostic.catalog` (public receipt transport) | default LLVM JSON receipt malformed; native line 0 | yes |
| T5 | `semantic.symbol_type_graph` consumers (native LLVM/C) | contextless `Some`; `Option<Subject>` | yes |
| T6 | `mir.generic_specialization` | nested generic binder matches type text | yes, with care |
| T7 | `selfhost.iteration_type_verdict`, `mir.execution_graph` | `for` with early return fails in self-host MIR | hold |
| T8 | `diagnostic.catalog` (route refusal outcome) | codegen refusals print no JSON receipt | yes, after T4 |
| T9 | `projection` (native C call typing) | enum variant constructor in a hosted body taken for a self-call | yes |
| T10 | `mir.generic_specialization` consumers (native C) | specialized prototype before its struct typedef; undeclared `IsSome_Result` | yes |
| T11 | `diagnostic.catalog` (self-host parse acceptance) | parser accepts forms native refuses, then later stages die | yes |
| T12 | `semantic.symbol_type_graph` (`?` checking) | `?` error type not compared; native C drops payload conversion | yes, WIP permitting |
| T13 | `abi.layout_rows` (native constructor stores) | subject parameter stored into a subject-typed class field | yes |

**T1: bare exits in self-host declaration parsers.**
Status: LANDED 9e6dc6f6
- Bare `Exit(1)` in the 14 declaration owners: 143 to 2 (both internal
  invariants); uncoded Fail/Expect/terminator/type/expression calls: 97 to 0.
  New codes: `declaration_clause_duplicate`, `declaration_clause_invalid`,
  `intent_shape_invalid`; `surface_not_covered` now has a public identity and
  a position.
- zone_effect_pool_runtime, role_include_methods and
  relation_effect_projection_sync still do not compile on the default route;
  each is now refused as `surface_not_covered` with a span, on text and JSON.
- default_route_diagnostic_position_owner.sh: 7 imported-file spans, 45 broken
  inputs, 5 uncovered native forms x 4 default legs: PASS; it also refuses a
  bare `Exit(1)` or an uncoded refusal returning to these owners
- self_hosted_component_contract_smoke.sh, self_host_pergyra_likeness_smoke.sh
  (CORE_STRING_MUNGE_SIG_MAX 74 to 70), language_keyword_registry_smoke.sh: PASS
- parser_parity.sh: 191 sources byte-equal on c and llvm; semantic_parity.sh: 118 fixtures ok
- role_override_mir_replacement.sh, resilience_admission.py (26),
  intent_predicates.py --c-rung (38/0): PASS
- make self-host-preparation-contract-test-smoke: passes with
  PGY_ALLOW_MISSING_COQ=1 (no Coq on this box)
- Found by T1, open: the self-host parser accepts `let x: T;` and bare
  `x: T;` fields in effect and relation bodies, which native refuses; world
  members `state NAME: zone ...` and `activate NAME`, and about 20
  statement-level forms native accepts, are refused on the default route
  with `expected_token` or `statement_head_unexpected_token`.
- Finding: about 90 refusals in the intent, zone and effect/relation
  declaration parsers end with a bare `Exit(1)`. On the default route
  `zone_effect_pool_runtime` and `role_include_methods` print only
  `self-host driver failed (exit 1)`; `relation_effect_projection_sync`
  prints an uncoded `PARSE ERROR`.
- Edit scope: `src/self_hosted/parser/decl_intent_owner.pgy`,
  `decl_zone_owner.pgy`, `decl_effect_relation_owner.pgy`, the type and
  event declaration owners, and `decl_dispatch_owner.pgy` to pass the
  location context. Refuse through `ParseRefuse` with a code from
  `diagnostic_owner.pgy`.
- Do not edit: nothing in the other lane's WIP is in scope.
- Gate: new rows in
  `tests/self_hosted/parity/default_route_diagnostic_position_owner.sh`;
  `make self-host-preparation-contract-test-smoke` (likeness ratchet and the
  600-line owner cap).

**T2: concurrency surface on the default route.**
Status: LANDED 161a78a9..34acf347
- The refusal now takes a typed `SemanticArtifactAdmissionPurpose`: only
  requests that lower routine bodies are refused. `--capability-manifest` and
  `--dir` (which had the same regression) pass the inspection purpose.
  public_capability_manifest_installed_self_host_owner.sh now requires the
  default-route manifest of `budget_channel_demo.pgy` and a parallel program
  to equal native's (fails with the 2e9081ae driver).
- `parallel on`: refused at `on` with `expected_token` `{` in a function body,
  and in a role body with the new `vision_surface_not_executable`, each at
  native's line and column.
- default_route_diagnostic_position_owner.sh (47 broken inputs, 6 native-only
  statements x 4 legs), builtin_surface_parity_owner.sh, component contract,
  likeness, semantic_parity.sh (118), parser_parity.sh (191),
  grammar_cheatsheet, keyword registry, parallel_vision_surface_smoke.sh,
  unsafe_block_execution.py (146): PASS; concurrency-case sweep equal to before
- Red on origin/main, same with the pre-change driver:
  axis_carriage_probe_smoke.sh (`auth_val/forged.pgy`),
  initializer_projection_probe_parity.sh (static text check)
- Open from T2: the deleted `parallel on` parser branch held the only
  self-host parse of `every` and `continuous`; the regenerated inventory lists
  them native-only while `src/lexer/language_keyword_registry.def` still
  claims self-host support. On inspection requests a `let` directly in a
  parallel body still fails as `local_binding_invalid` with `Span: none`.
- First pass: a new owner `native_pipeline_only_statement_owner.pgy` refuses
  parallel blocks (kind 95) and channel sends (kind 94) at artifact admission
  with `statement_native_pipeline_only` and a span; a `let` inside a parallel
  body is refused at the parallel. Default-route sweep of 951 cases x {c,
  llvm}: identical results for every program before and after.
- Correction to this card: native also refuses
  `parallel { let y: Int = x + 1; Log(ToString(y)); }` (each parallel
  statement is its own task, so `y` is undefined in the second).
- Returned by the verifier: (1) `--capability-manifest` shares artifact
  admission, so the default-route manifest of a channel or parallel program is
  now refused where it used to equal native's; the refusal must apply only to
  requests that lower. (2) `parallel on`, which native never compiles, gets a
  reason saying native runs it; it needs a truthful refusal.
- Found by T2, open (other owners):
  - `Span: none` with `ast_artifact_invalid` for a generic spawn into an
    unannotated let (generic_spawn, generic_future_spawn_*, string_spawn) and
    for a parallel join expression; the parser makes parallel/async block
    expressions an opaque leaf.
  - Default C fails with no code on `self-host spawn requires a direct
    one-or-two-argument call fact` (cancel_future, cancel_propagation,
    future_cancel_propagation, future_cancel_state).
  - Misleading parser codes: `select` gets `expression_statement_terminator`;
    `parallel (x in xs) ... { }` loses its body; a bare `{ }` inside parallel
    gets `statement_head_unexpected_token`; an `async { }` statement gets
    `statement_kind_unsupported`.
  - TryRecv is `undefined_function` on the default route; default LLVM
    compiles none of the 58 concurrency cases (uncoded `CODEGEN ERROR` in
    direct-MIR admission).
  - `transaction { }` (kind 97) and `with slot<T> as v { }` (kind 96) still
    reach MIR with no code; kinds 98-100 (`fail`, event `+=`/`-=`) not probed.
- Finding: `parallel { Log("a"); }` fails with `AST node is outside bounded
  MIR producer ... kind=95`, and `ch <- 10;` with `kind=94`, both with no
  code. A `let` inside a parallel body is refused as `local_binding_invalid`
  with `Span: none`. Refuse these at admission with a coded, positioned
  diagnostic (as `builtin_native_pipeline_only` does for channel builtins),
  or implement them.
- Edit scope: self-host semantic admission owners and
  `native_pipeline_only_builtin_owner.pgy`.
- Do not edit: `ast_body_type_bundle_owner.pgy`,
  `ast_collection_ownership_verdict_owner.pgy`,
  `ast_expression_graph_collection_mutation_owner.pgy`,
  `builtin_effect_projection_owner.pgy`, `builtin_signature_owner.pgy`,
  `collection_mutation_policy_owner.pgy`. The registry row is ACTIVE, so
  record here which admission owner you change before editing it.
- Gate: diagnostic-position rows; `builtin_surface_parity_owner.sh`.

**T3: `Try` on an enum error, default C.**
Status: LANDED ede9dd16
- Cause: `EmitTryLet` chose the temporary's C type from the function's
  return-type text and always declared `pgy_result_int`. It now reads the
  operand's Result type from the semantic expression graph; the return type
  only decides how the error leaves (same type, same error type with a
  different payload, or a codegen refusal).
- default_route_scalar_value_owner.sh (push step, pin stays 65): both cases on
  native C, native LLVM and default C, a payload-converting and an
  Int-returning try, and a refused error-type mismatch: PASS (pre-fix driver:
  `default-c did not compile try-enum-error`)
- builtin_surface_parity_owner.sh, self_hosted_component_contract_smoke.sh,
  self_host_pergyra_likeness_smoke.sh, default_route_diagnostic_position_owner.sh: PASS
- codegen_parity.sh: rung-0..21 parity ok, fixtures=85, backends c and llvm
- compare_backends.sh on both cases: passed
- io_result_builtin_owner.sh: FAIL on native-c, the NTFS read-only-directory
  row, same with the pre-fix driver (Windows only)
- Default LLVM still refuses both programs before emission (direct-MIR
  admission under `src/self_hosted/compiler`), with no code; not T3's owner.
- Found by T3, open:
  - Native checkers do not compare a `?` operand's error type with the
    function's; a mismatch runs on native LLVM and fails in the C compiler on
    native C.
  - Native C returns the operand's Result where a Result with another payload
    is declared (`incompatible types when returning type
    'PgyResult_Int_Fault'`); native LLVM converts it.
  - `let v: Int = Pick(3)?;` over an Option in `Main` is accepted natively; the
    default C route now refuses it in codegen instead of failing in the C
    compiler.
  - Default C try-lets over non-Int payloads are still refused by the text
    allowlist `TrySurfaceAllowedLineWithin` in `text/text_owner.pgy`.
- Finding: `try_chain_enum_err` and `try_class_method_chain` fail in the C
  compiler (`main.c:119:34: error: invalid initializer`) on the default C
  route.
- Edit scope: `src/self_hosted/codegen/emission/result_runtime_emit_owner.pgy`
  and the Try emission owner.
- Do not edit: `expr_semantic_call_emit_owner.pgy` (WIP, and at the 600-line
  cap), `runtime_call_rewrite_owner.pgy`, `ast_expression_usage_owner.pgy`,
  `runtime_abi/{collection,string,string_runtime_symbol}_owner.pgy`,
  `text/owned_string_join_owner.pgy`.
- Gate: both cases print native C's output on the default route; add a
  `builtin_surface` or diagnostic-position row that holds it.

**T4: public receipt and native line.**
Status: LANDED 462c10fa (finding b); finding a moved to T8
- io_result_builtin_owner.sh (Windows copy without the NTFS read-only-dir row): PASS
- make test-semantic: 2954 passed, 0 failed; make test-transpile: 979 passed, 0 failed; make test-mir: 216 passed
- self_hosted_component_contract_smoke.sh, default_route_diagnostic_position_owner.sh, containment_embedding_copy_owner.sh: PASS
- tests/diagnostics_json_smoke.sh: spec-limit FAIL, also red on origin/main (stale fixture: 33 enums share variant `A`)
- Finding: under `--error-format=json`, any direct-MIR codegen refusal on the
  default LLVM route prints `self-host JSON diagnostic receipt is
  malformed`. Native reports `struct IoError` at line 0 (`enum IoError` gets
  line 1).
- Edit scope: the launcher's receipt reader (find the message in `src/`) and
  the native parser's struct declaration line.
- Do not edit: WIP `src/compiler/hir*`, `src/compiler/mir_*`.
- Gate: a JSON default-LLVM leg in the diagnostic-position gate; line 1 for
  the struct form in `io_result_builtin_owner.sh`.

Q (agent-t4): finding (a) is not a launcher misread; the driver writes no
receipt. For containment_embedding_declared_forms.pgy the JSON and text
source-LLVM driver requests print the same stdout (`CODEGEN ERROR: direct MIR
identity cell declaration inventory is invalid`, rc 1), while a semantic
refusal on the same route relays a valid receipt. Default C codegen refusals
do the same. Every such refusal is `Die` (codegen/text/text_owner.pgy, about
2500 call sites), which cannot see the admitted JSON choice: Pergyra has no
process state, and `Args()` there breaks every `with caps` caller (probed).
Options: (1) a runtime-held refusal format set by the CLI request owner,
which needs src/common/pgy_builtin_type_table.c (another lane's WIP); (2)
typed direct-MIR route refusals carried to DriverSourceLlvmIntentOutcome and
rendered by the shared wire renderer, which covers route refusals only and
is outside this card's scope. Which one, if either?

A (verifier): (2). A refusal is data the route owner returns, not a global
the renderer reads, so it matches the Result-first rule; (1) would also wait
on the other lane's file. It becomes card T8 below. Internal `Die` calls stay
text-only until a later card; finding (a) remains open, not fixed.

**T5: Option typing on the native backends.**
Status: LANDED 12ba034b..72a7897e
- compare_backends.sh on option_some_semantic_type, option_subject_payload, io_result_in_hosted_bodies: 3/3 passed
- full compare_backends.sh: 951/951 passed
- make test-transpile: 979 passed; make test-semantic: 2954 passed
- make test-mir: native test_mir 216 passed; stops at match_binding_type_fact_smoke.sh without a self-host driver (passes with PGY_NATIVE_PIPELINE=1)
- generic_falsification_smoke.sh: f_where_g1 runs on both backends
- llvm_option_member_assignment_context_owner.sh: PASS (fixture renamed llvm_option_statement_some.pgy)
- The let/match forms are fixed through the same checker-sealed type: the
  checker also records the payload type on `UnwrapOption(o)`, and MIR's
  source-local owner reads it before any callee lookup (no callee spelling;
  `assignment_projection_probe_parity.sh` forbids "UnwrapOption" there).
- Finding: LLVM refuses contextless `Some` (`IsSome(Some(5))`,
  `let x = Some(5)`, `Some(5)` in a condition) that C accepts, and without a
  consumer borrows the enclosing return type for its layout. `Some(card)`
  for a subject parameter (`Option<Subject>`) fails in code generation on
  both native backends. LLVM should take the expression's semantic type.
- Edit scope: `src/codegen/llvm_*` and the C Option emission.
- Do not edit: `llvm_expr_array_calls.c`, `llvm_expr_call_dispatch.c`,
  `transpiler_expr_call_spawn_emit.c`,
  `transpiler_expr_stdlib_builtin_policy.c`, `src/common/pgy_builtin_type_table.c`.
- Gate: new `tests/cases/backend_compare` cases; the two known-open
  `f_where_g1` rows in `tests/generic_falsification_smoke.sh` must flip to
  passing (update them).

Q (agent-t5): the checker's `Option<payload>` type for `Some(x)` was not
recorded where a backend can read it. The branch adds
`semantic_value_type_name` to `AST_CALL` (`src/parser/ast.h`, call accessors,
`ast_destroy.c`) and seals it in
`src/semantic/type_checker_builtins_stdlib_variant.c`. Both are outside the
edit scope and outside every do-not-edit list. Keep, or move?

A (verifier): keep. It follows the call node's existing checker-sealed facts
(`semantic_diverges`, `semantic_callee_declared_callable`) and has one reader,
`llvm_option_some_layout_type`.

Q (agent-t5): `let x = Some(5);`, `let n = UnwrapOption(Some(7));` and
`match Some(5) {...}` still fail on native LLVM with "MIR-only LLVM path
missing source-local type metadata". The MIR source-local owner
(`mir_source_local_expr_types.c` -> `mir_source_local_expr_call_facts.c`)
has no row for Some or UnwrapOption; native C passes only through its own
fallback inference. Stopped because the card listed `src/compiler/mir_*`.

A (verifier): go ahead. Neither file is in the other lane's WIP; the card's
`mir_*` pattern was wider than the WIP. Still do not edit
`mir_source_local_types.c`, `mir_types.h`, `mir_branch_source_facts.c`,
`mir_json_dump.c`.

- Left open by T5, outside both findings: a subject parameter stored in a
  subject-typed class field (`Box(card)`) fails on both native legs.

**T6: nested generic binder.**
Status: LANDED a7921a5e..c488e14b (pin change folded into a7921a5e)
- The native checker now decides a generic call's type arguments (explicit
  argument, then the matching part of an argument's type, then the default)
  and seals them on the call; MIR copies them. The text matcher is deleted
  with no fallback; a call with nothing sealed is refused
  (`PGY_MIR_TOPOLOGY_INVALID`). Conflicting and unbindable calls are refused
  at semantic with a code and a position.
- generic_nested_failclosed_smoke.sh (now run inside the push-step
  generic_falsification_smoke.sh; pin stays 65): PASS, fails on base
- full compare_backends.sh: 953/953; make test-semantic 2988, test-transpile
  983, test-mir 216, test-parser: PASS; semantic_parity.sh 118: PASS
- self_hosted_component_contract_smoke.sh: FAIL until the pin below changes

Q (agent-t6): the component contract (WIP) lines 8582-8583 require
`mir_generic_method_captured_return_type(`, which this change deletes.
Replace with reject_text for it and for `mir_generic_binding_type_names(`,
plus require_text `ast_call_semantic_generic_arg_type_name(call, i)` in
`src/compiler/mir_generic_method_specialization.c`.

A (verifier): yes; the verifier makes that pin change at integration (the
other lane's hunks in that file are at lines 4381-4387 only).

Q (agent-t6): `generic_falsification_smoke.sh` now runs the nested smoke;
keep, or a step of its own?

A (verifier): keep.

Q (agent-t6): a generic call's value type is not substituted for a
constructed parameter: `let s: String = First(xs)` with `xs: Array<Int>` is
accepted (native C fails in the C compiler, LLVM prints an empty line; base
behaves the same). The fix is in `type_check_function_symbol_call` in
`src/semantic/type_checker_helpers_late.c`, which is WIP.

A (verifier): recorded as T6b; it waits for the collection lane's WIP.

**T6b: substitute a generic call's return type from its sealed binding.**
Status: OPEN (blocked on `type_checker_helpers_late.c`, WIP)
- Also found by T6, open: native C emits a specialized prototype before the
  `Crate_Int` typedef for a generic struct parameter; native C emits an
  undeclared `IsSome_Result` for `IsSome` on `Option<Result<Int,String>>` in a
  specialized function; native LLVM reads a class parameter `T` as another
  declaration's `T: Sortable` bound; native LLVM fails to verify a generic
  method with an `Option<T>` parameter; the checker types `h.item` on a
  `Holder<Int>` as unknown (`type_checker_expr.c`, WIP).
- Finding: `TakeOpt(Some(5))` for `TakeOpt<T>(o: Option<T>)` fails in MIR
  lowering on both native backends; the binder matches type-name text
  instead of consuming the semantic binding.
  `tests/generic_nested_failclosed_smoke.sh`, outside CI, is red.
- Edit scope: the MIR generic specialization owner under `src/compiler`.
- Do not edit: `mir_types.h`, `mir_branch_source_facts.c`,
  `mir_json_dump.c`, `mir_source_local_types.c` (WIP). If the fix needs
  `mir_types.h`, stop and write a `Q` here.
- Gate: `generic_nested_failclosed_smoke.sh` green, then into the push steps
  within the budget.
- Note from T5 (`eac716eb`): `nested_param.pgy` (`TakeOpt(Some(5))`) now
  binds `T = Int` and prints `1` on both native legs, because the binder
  reads the MIR type T5 fixed. The binder still matches type text. The gate
  still expects LLVM to refuse this case, so it now fails with "compiled but
  must fail closed"; T6 updates that row.

**T8: typed codegen refusals on the default routes.**
Status: LANDED b4467abe..9c32e7bd
- New owner `codegen_route_refusal_owner.pgy`; four default-LLVM route
  refusal sites return a typed refusal (single-routine scalar route, terminal
  multi-routine scalar route, identity-cell inventory, unsupported CFG shape).
  Text keeps the `CODEGEN ERROR: ...` line; JSON is one receipt with native
  LLVM's identity (`PGY_LLVM_TYPE_UNSUPPORTED`, stage `llvm_codegen`, layer
  `backend`, location null).
- containment_embedding_copy_owner.sh (JSON default-LLVM leg added): PASS;
  public_llvm_ir_json_diagnostic_receipt_owner.sh (3 typed route refusals): PASS;
  both fail with the pre-change driver
- default_route_diagnostic_position_owner.sh, self_hosted_component_contract_smoke.sh
  (the two approved pins), self_host_pergyra_likeness_smoke.sh (70/70, 20/20),
  semantic_parity.sh (118), codegen_parity.sh (85 fixtures): PASS
- route-rejected text gates (domain topology, region user callee, two-int
  nominal, multi-routine, root intent takeover): PASS
- Still text-only: default C (T8b) and `Die` inside a route that already
  claimed the program. Red on base, same with the pre-change driver (Windows):
  direct_mir_physical_record_return_owner.sh (clang link lacks
  `pgy_runtime_panic_internal_invariant_export`),
  direct_mir_nested_intent_program_c_owner.sh, one_mir_bool_logic_projection.sh,
  one_mir_constructed_generic_member_projection.sh,
  one_mir_inferred_generic_member_projection.sh, one_mir_array_return_projection.sh.
- Finding: a direct-MIR or self-host C code-generation refusal ends in
  `Die`, which prints text and exits, so `--error-format=json` gets no
  receipt and the launcher prints `self-host JSON diagnostic receipt is
  malformed`.
- Direction: carry route refusals as typed outcomes to
  `DriverSourceLlvmIntentOutcome` (and the C emission outcome) and render
  them with the shared public receipt renderer. Do not add process-global
  state. Do not touch `src/common/pgy_builtin_type_table.c` (WIP).
- Gate: a JSON default-LLVM and default-C leg for a codegen refusal that
  requires a code and a location, or the exact public codegen identity.

Q (agent-t8): the refusals this card names are decided inside the direct-MIR
multi-routine dispatcher, and two pins in
`tests/self_hosted_component_contract_smoke.sh` (on the WIP list) freeze that
code: lines 8393-8398 pin `return UnwrapOption(composite_intent_payload);` and
its nested-intent twin, and lines 19995-19997 require
`DirectMirScalarProgramRouteAdmissionDie(scalar_program_admission)`. May I
edit those two pins, or ship only the single-routine refusal? A pin-free
alternative is an `inout Option<refusal>` parameter with an empty payload.

A (verifier): edit the two pins. The other lane's hunks in that file are only
at lines 4381-4387, so the 3-way landing does not touch them. Keep what each
pin claims: the composite and nested intent payloads are still returned before
the scalar route, and the terminal owner still consumes the scalar route
admission, now as a typed refusal. Do not use the `inout Option` form: an
out-parameter with a sentinel payload is the DX debt AGENTS.md names.

Q2 (agent-t8): on the default C route no source program reaches a refusal
decided at a route boundary; C refusals die later inside claimed projections.
For the default-C JSON leg: (a) type the substitution refusal with no source
gate, (b) also convert the scalar-CFG claimed-projection refusals, or (c) drop
the C leg?

A (verifier): (c). Ship the default-LLVM route refusals with their JSON gate.
The default-C JSON receipt for in-route refusals stays open as T8b; an
untestable branch (a) is not added.

**T8b: JSON receipts for in-route codegen refusals.**
Status: OPEN (from T8's Q2)
- Finding: default C refusals inside claimed projections (for example
  `let mut xs: Array<Long> = [1L]; ArrayPush(xs, 2L);` ->
  `direct MIR scalar CFG program expression admission is invalid:
  stage=left-edge`, the same on default LLVM) end in `Die` and give no JSON
  receipt.

**T9: an enum variant constructor inside a hosted body, native C.**
Status: LANDED 5d39885d
- Cause: C call type inference sent every bare call the checker had not
  resolved to a declaration to the host-method lookup. Since e44fd90a the
  checker sets `semantic_callee_declared_callable` on bare calls it resolved
  to a host method; host method rows are now read only for those calls. No
  new fallback.
- enum_variant_in_hosted_bodies (subject action, class method, zone method):
  1/1 passed, 0/1 with the 34acf347 compiler; full compare 954/954;
  test-transpile 983, test-semantic 2988; backend_fail_closed, perf_contract: PASS
- Native oracle emission of the driver: rc 0; with the c9d5dd70 workaround
  reverted locally it also passes now (the workaround stays).
- Open from T9: an unannotated `let o = Done(n);` fails on both native legs
  anywhere (C "cannot determine C type for MIR local", LLVM "missing
  source-local type metadata"); the owner `mir_source_local_types.c` is WIP.
  Pre-existing red: mir_declaration_inventory_smoke.sh.
- Finding: on the native MIR-only C path a bare call inside a subject or
  class body is inferred as an implicit-self method call, so an enum variant
  constructor there fails with "MIR-only C path missing hosted self-call
  inference method metadata" (it broke the driver in `c488e14b`; `c9d5dd70`
  works around it with a function).
- Gate: a backend_compare case that builds variants of a payload enum
  directly inside a subject action and a class method, on both native legs.

**T10: generic specialization emission order and names, native C.**
Status: LANDED 97c05a4e..fd19f0e5
- (a) `ensure_generic_class_specialization` wrote the struct typedef to
  `ctx->helpers`, after `ctx->decls` where the prototypes go; it now writes to
  the requester's declaration stream, fields first. Covers plain prototypes,
  class fields and nested instances, not only `Open_Int`.
- (b) a new naming owner in `transpiler_option_context.c` builds the Option
  suffix from the whole inner type and fails closed on an unknown one;
  IsSome, IsNone, UnwrapOption and Some use it.
- nested smoke: `nested_struct_param` now runs on C (fails on base);
  generic_struct_layout_before_use and option_of_result_consumers: 2/2 (0/2
  on base); full compare 955/955; test-transpile 989, test-semantic 2988;
  generic_falsification, generic_method_specialization, perf_contract
  (pin moved to the new suffix call), backend_fail_closed,
  abi_ownership_shape, runtime_panic_contract, component contract, keyword
  registry: PASS; native oracle emission byte-identical to base
- Open from T10: a class method calling a method of a generic instance
  (`self.top.Get()` with `top: Crate<Int>`) fails on both native backends
  (C `implicit declaration of function 'Crate_Int_Get'`: the instance's
  method prototypes still go to `ctx->helpers`); a lambda block body
  returning `None` as `Option<Result<Int, String>>` is refused on native C.
- Finding: native C emits a specialized prototype before the `Crate_Int`
  typedef it uses (`tests/cases/generic_nested_failclosed/nested_struct_param.pgy`),
  and emits an undeclared `IsSome_Result` for `IsSome` on
  `Option<Result<Int,String>>` inside a specialized function.
- Gate: the known-open `nested_struct_param` row in
  `tests/generic_nested_failclosed_smoke.sh` becomes a run row on C; a
  backend_compare case for the `IsSome` form.

**T11: self-host parse acceptance gaps.**
Status: LANDED 0ec30e1f..2ca58486
- A new `ParseNameIn` (source_refusal_owner.pgy) reads a required name and
  refuses a missing operand, a number or a reserved word the registry does
  not allow as a name. The zone owner reads every apply, link, refresh,
  publish, bind, authority and state operand and every slot name through it;
  the effect and relation owners take `shared` fields only. Each refusal is
  `expected_token` at native's line and column.
- default_route_diagnostic_position_owner.sh: 58 broken inputs (11 new, each
  also checked against native's position): PASS, fails with the base driver
- parser_parity.sh (191), semantic_parity.sh (118), component contract,
  likeness (70/70, 20/20), keyword registry (inventory regenerated), grammar
  cheat sheet: PASS; native oracle emission of the driver: rc 0
- default-route sweep, 953 cases x {c, llvm}: identical before and after
- Open from T11: an undeclared slot operand (`apply poison to ghost`) still
  dies in DIR with no code where native refuses it with
  `PGY_SEM_ZONE_CONTRACT_INVALID` (`ast_zone_rule_verdict_owner.pgy` checks
  only `by` participants, and `zone_contract_invalid` prints `Span: none`);
  registry name-words (effect, zone, world, ...) are still accepted as
  operands; `maintain` is `surface_not_covered`; `activate battle` in a world
  is refused although native compiles it; default LLVM fails any program with
  a zone or effect declaration with an uncoded "direct MIR compile-time
  declaration erasure requires exactly one declaration".
- Finding: `apply burn to 42` passes the self-host parser and dies in DIR
  with "self-host DIR apply directive field identity is missing" and no code;
  `let x: T;` and bare `x: T;` fields in effect and relation bodies are
  accepted where native refuses them.
- Gate: rows in `default_route_diagnostic_position_owner.sh`, each checked
  against native's refusal.

**T12: `?` operand checks on the native routes.**
Status: LANDED 00887bdd..34653936
- (a) `type_check_try_error_type` in `type_checker_expr_ops.c`: when both the
  operand and the function return Result, their error types must match
  (payloads may differ; a one-argument `Result<T>` has error type String).
  A mismatch is refused at semantic on both native legs with
  `PGY_SEM_TYPE_MISMATCH` at the operand's position.
- (b) native C's try exit (`transpiler_result_try_leave_stmt`) rebuilds the
  error in the function's Result when payloads differ, as the default C route
  does; native C and LLVM now print the same output.
- try_result_payload_conversion (new) and five existing try cases: 6/6;
  full compare 954/954; test-semantic 2995, test-transpile 983;
  default_route_scalar_value_owner.sh now runs its payload-conversion row on
  native C too (fails with the pre-fix compiler); diagnostic registry,
  keyword registry, build-source inventory, component contract: PASS;
  native oracle emission of the driver: rc 0
- Open from T12: default C refuses a one-argument `Result<Int>` try into
  `Result<String>`/`Result<Bool>` (`ResultRuntimeFactForType` has no fact for
  a one-argument Result); default LLVM still refuses explicit
  `Result<T, E>` signatures before emission.
- Decision for the user: a Result `?` inside a function returning Option,
  and an Option `?` inside a function returning Result, are accepted and
  panic at run time on both legs. Refuse them at semantic?
- Finding: neither native checker compares a `?` operand's error type with
  the function's (a mismatch runs on native LLVM and fails in the C compiler
  on native C); native C returns the operand's Result where a Result with
  another payload is declared (`incompatible types when returning type
  'PgyResult_Int_Fault'`), while native LLVM converts it.
- If the checker's owner file is on the WIP list, only the C conversion part
  is in scope.

**T13: a subject parameter stored into a subject-typed class field.**
Status: LANDED 1aa4567d..4845f790
- A subject parameter, or `self` in a subject method, is now stored into a
  class or relation field as a value on both native legs. One C function
  (`transpiler_call_subject_arg_policy.c`) and one LLVM function
  (`llvm_operand_value_for_storage`) decide from the parameter's carriage;
  `Some(x)` from T5 now uses the same decision.
- subject_param_constructor_store (fails on both legs on base) and
  option_subject_payload: 2/2; full compare 954/954; test-transpile 983,
  test-semantic 2988; generic_falsification, keyword registry,
  backend_fail_closed, abi_ownership_shape, component contract: PASS
- Open from T13: native C refuses a class method that returns a class
  holding its host by value ("cyclic by-value type declaration dependency";
  `transpiler_type_decl_schedule.c` counts method signatures as layout
  dependencies), so a subject method returning `Box(self)` runs on LLVM only;
  native C `Holder<Int>` returned from a function fails on `Holder_Int`
  (same class as T10).
- Decision for the user: the checker refuses `let k = card;` ("Subjects
  cannot be copied into a new binding") and an implicit subject copy into a
  zone or world constructor (d9649ccb), but accepts copying a subject into a
  class field, from a local and now from a parameter. Should a class-field
  store also require an explicit copy? T13 only made parameters behave like
  locals.
- Finding: `Box(card)`, a subject parameter passed to a class whose field is
  subject-typed, fails on both native legs (found by T5; T5 fixed the same
  shape for `Some(card)`).
- Gate: a backend_compare case on both native legs.

**T7: `for` with an early return in self-host MIR.**
Status: HOLD until the collection lane lands its `mir_lower` WIP
- Finding: `func Has(xs: Array<Int>, v: Int) -> Bool { for x in xs { if x ==
  v { return true; } } return false; }` passes the self-host semantic
  checker, then fails with `MIR instruction expression graph is missing or
  invalid` on default C and `range flow/type identity is invalid` on default
  LLVM.
- A read-only diagnosis may start now; record it here.

## 7. Follow-up Red-Team: Collection Event Witness Closure

The active `semantic.hashmap_collection_ownership` rung was attacked again
after the initial audit. The first mutation changed the empty-literal origin
and its `produce-storage` receipt event together by `+100000`. The previous
reader accepted that coherent forgery. The initial attempt to use one
instruction-local `(event, destination)` pair was rejected by both independent
implementation reviews because `AddTwo(left, right)` correctly emits one call
event with two destination receipts. That design was withdrawn.

The selected design keeps destination fan-out in the existing typed
`collection_call_boundaries` owner and adds only the general MIR
`expr0_syntax_id` event anchor. `def` receipts require the exact
`AST_LET_DECL` binding and initializer event; direct builtin transfer/release
receipts additionally require the typed MIR `arg0` operation. Generic calls
remain validated by their existing call-boundary rows. A fabricated event
without a MIR instruction, a coherent origin/receipt event mutation, a
direct-builtin wrong-kind mutation, duplicate receipt, and transition mutation
now fail closed.

Focused evidence from the fresh C+LLVM compiler:

```text
tests/collection_own_parameter_boundary_smoke.sh: PASS
  includes positive two-actual fan-out
  includes forged origin+receipt event
  includes forged direct-builtin operation kind
  includes forged transition, duplicate, event, destination, and witness rows
tests/collection_ownership_fact_projection_smoke.sh: PASS
tests/collection_split_owned_producer.sh: PASS
tests/owned_statement_prefix_exact_transfer_smoke.sh: PASS
test_mir collection ownership subset: 225 passed, 0 failed
git diff --check: PASS
```

The exact fresh compiler used for these results was:

```text
D:/PergyraLang/.tmp/redteam-current/bin/pgy.exe
SHA-256: 228A389C1DB52079063870F66DA5E74F046F7C2D3E24B2B9656796E25D54262F
built: 2026-09-24 14:58:14 Asia/Seoul
```

At completion, `HEAD` and `origin/main` were
`761531dc85f7fae2caea84fa296ff2403a3cf4bb`. The worktree contained 192
status entries (87 tracked modifications and 105 untracked paths), including
concurrent work outside this audit packet.

The SoT row remains `ACTIVE`. The same fresh evidence keeps these blockers:

```text
tests/self_hosted/parity/p0_runtime_counterexamples.sh: 7 failures
  R1 native C/LLVM/self-C aliasing
  R2 installed shallow Array<String> acceptance
  R3 installed Unwrap(Err) success and post-error execution
  R5 installed Float/Double precision loss

tests/self_hosted/parity/collection_ownership_semantic_owner.sh:
  installed self-host accepted deep_drop_nonbinding_literal_invalid

src/self_hosted/mir_lower/main.pgy native admission:
  10 errors, 1 warning
```

The self-host source admission errors include unresolved deep-drop provenance
and exact callee-effect/caller ownership joins in
`SelfDirIntentStepFromArtifact` and
`SelfDirIntentStepAppendActionContractRange`. Therefore the new independent
event witness closes a focused reader falsifier, but it does not substitute
the production self-host consumer and does not justify `CLOSED`.

## 8. Read-only Collection Ownership Schema/Witness Red-Team (2026-09-24 21:30 KST)

This addendum is the current-head result for the collection ownership event
witness/schema packet. It supersedes the broad "forged witness" and "shared-call
receipt reader parity" PASS language in section 7 only where explicitly stated
below. No tracked source file was modified. Temporary JSON mutants and command
captures were kept under ignored `.tmp/codex_collection_ownership_audit/`.

### Current authority snapshot

```text
HEAD: 4141904d5946fa7d727a13a07721f646c988f5fe
branch: main
HEAD origin/main: 4141904d5946fa7d727a13a07721f646c988f5fe
git status --short entries: 192
fresh compiler: .tmp/redteam-current/bin/pgy.exe
fresh compiler SHA-256: 228A389C1DB52079063870F66DA5E74F046F7C2D3E24B2B9656796E25D54262F
fresh compiler timestamp: 2026-09-24 14:58:14 Asia/Seoul
```

The focused native C+LLVM compiler hash was stable across repeated checks. The
working tree remained dirty and concurrent, so this is exact current-dirty-tree
evidence, not exact published-HEAD evidence.

### Focused gates that are actually green

```text
PGY_BIN=.tmp/redteam-current/bin/pgy.exe \
  bash tests/collection_own_parameter_boundary_smoke.sh
PASS

PGY_BIN=.tmp/redteam-current/bin/pgy.exe \
  bash tests/collection_ownership_fact_projection_smoke.sh
PASS

PGY_BIN=.tmp/redteam-current/bin/pgy.exe \
  bash tests/self_hosted/parity/collection_split_owned_producer.sh
PASS

PGY_BIN=.tmp/redteam-current/bin/pgy.exe \
  bash tests/owned_statement_prefix_exact_transfer_smoke.sh
PASS
```

The first gate still executes the intended positive two-actual call. Native
MIR for `inout_two_actuals_owned_push_valid.pgy` has one `AddTwo(left,right)`
instruction with event `28`, and two transfer receipts fan out to destinations
`20` and `24`. The focused gate's two-actual control exits `0`; duplicate,
coherent origin/event forgery, and a transfer event relabelled with a release
event are rejected with `collection_ownership_receipt_row` or
`collection_ownership_receipt_set`.

This validates the selected fan-out shape for its covered control. It does not
validate every source-positive transfer receipt against an independent MIR
operation instruction.

### P0 reprocessed: source-positive transfer receipt skips its event witness

`owned_string_local_array_transfer_valid.pgy` emits a valid
`transfer-element` receipt with:

```text
source_binding_syntax_id: 5
destination_binding_syntax_id: 11
event_syntax_id: 15
```

No emitted instruction has event `15`: the native array literal instruction has
event `14`, and the owned String producer call has event `7`. The unchanged
valid control is accepted. Replacing only the transfer receipt event with
`100015` is also accepted with exit `0`; the fabricated event has no MIR
instruction. Relabelling the transfer to the real release event `16` is rejected,
so event uniqueness/transition checks work, but existence of the source-positive
transfer event is not required.

The exact owner causing this fail-open path is
`src/self_hosted/mir_lower/collection_ownership_fact_owner.pgy`:
`MirCollectionOwnershipReceiptInstructionEventReady` returns `true` for every
`transfer-element` with `source_syntax_id > 0`, before scanning instructions
(`collection_ownership_fact_owner.pgy:134`, with the unconditional transfer
return at `collection_ownership_fact_owner.pgy:156`). The native schema also
does not emit a distinct collection operation instruction for an element move
inside an array literal: `mir_json_dump.c:343` projects `inst->expr0` and stable
AST IDs, while the ownership receipt remains the semantic operation row.
Accordingly, the fix owner is not a new destination-pair field. The exact
closure should be a routine-local typed collection-operation fact (or an exact
literal transfer/effect ordinal anchored to the destination def) owned by the
native MIR lowering/JSON writer and consumed by the self-host reader. It must
prove event identity, operation kind, source binding, destination binding, and
fan-out multiplicity. The consumer replacement owner is
`BuildMirCollectionOwnershipFacts`; do not mark this field bridge `CLOSED`
until the old instruction-local fallback is deleted and source-positive
forgeries fail closed.

### P0 reprocessed: producer-local witness is self-consistent forgery, not instruction identity

A second independent mutation starts from the valid owned String producer
control. It changes all three local copies of one forged call ID:

```text
owned_string_producer_facts[0].producer_call_syntax_id
source_locals[producer].initializer_call_syntax_id
the producer def instruction's expr0_call_syntax_id
```

The reader accepts the coordinated value with exit `0`. The native instruction
graph still contains the original call identity. `MirCollectionOwnershipProducerInstructionReady`
only compares those two caller-controlled IDs and instruction/source shape;
it never joins the producer call to the instruction's independently owned
`expr0_syntax_id` or typed expression-call fact. In
`routine_instruction_scalar_capture_owner.pgy`, the three new ID fields are
tracked and parsed, but the end-of-object readiness check requires only
`kind` and `source_type` (`routine_instruction_scalar_capture_owner.pgy:122`).
`MirRoutineInstructionFactBundleReady` consequently cannot distinguish a
missing ID from a present zero. An independent deletion of
`expr0_syntax_id` from the producer instruction is also accepted, while
deletion of `expr0_call_syntax_id` is currently rejected by a different,
operation-independent source anchor.

Exact closure owner: the native instruction identity writer plus the typed
instruction bundle/readiness owner. Require each schema-v1 instruction to carry
exactly one `source_syntax_id`, one `expr0_syntax_id`, and, for `AST_CALL`, a
nonzero `expr0_call_syntax_id` equal to `expr0_syntax_id`. A String producer row
must additionally require that exact typed call identity and the semantic
builtin/declaration identity needed to distinguish a forged same-name call.
The negative gate must mutate one identity at a time and then all coordinated
copies after the same independent anchor is left unchanged.

### P0/SoT finding: direct builtin operation identity still has a fallible fallback

`empty_local_owned_push_valid.pgy` has a direct
`ArrayPushOwnedString(values,"owned")` operation at event `9`. Replacing only
the MIR instruction `arg0` with `Log`, while preserving event `9`, is accepted
with exit `0`. The current reader classifies the unrecognized name as "not a
direct builtin," then falls back to the generic instruction-event check. That
fallback is exactly why a fake operation kind survives. Changing the same event
to `100009` is rejected only because receipt-set uniqueness fails, not because
the fake `Log` operation at event `9` is rejected.

`MirCollectionOwnershipDirectBuiltinOperation` uses text routing `arg0` and a
name allowlist (`collection_ownership_fact_owner.pgy:187`), and
`MirCollectionOwnershipReceiptOperationMatches` only constrains the small set
of supported receipt kinds. The fallback at
`collection_ownership_fact_owner.pgy:835` admits a receipt with an unknown
operation spelling if the event exists. The native writer's
`expr0_semantic_stdlib_callee` is already present but this path does not
consume it for operation identity. The exact owner is therefore the native
operation-fact producer plus
`MirCollectionOwnershipDirectBuiltinOperation`: carry typed builtin kind and
semantic call identity, reject unknown/relabelled names, and remove the
unknown-name fallback to the generic event check.

### Source admission and bridge status

The native projection and focused MIR tests are green, but the production
self-host source is not admitted. Compiling
`src/self_hosted/mir_lower/main.pgy` with the fresh C+LLVM compiler fails with
`10 error(s), 1 warning(s)` and emits no artifact. The failures include
unresolved `ArrayDropOwnedStrings` provenance and the exact
`SelfDirIntentStepFromArtifact` /
`SelfDirIntentStepAppendActionContractRange` callee-effect/caller-ownership
join. The installed self-host semantic-owner gate is independently red:

```text
PGY_BIN=.tmp/redteam-current/bin/pgy.exe \
PGY_SELF_DRIVER_BIN=bin/pgy-self-driver.exe \
bash tests/self_hosted/parity/collection_ownership_semantic_owner.sh
FAIL: installed self-host accepted deep_drop_nonbinding_literal_invalid
```

Therefore the `semantic.hashmap_collection_ownership` SoT row is not closed by
this packet. The direct MIR event/fan-out shape is a useful partial closure, but
three semantic witness gaps remain: source-positive transfer event identity,
producer-call identity, and direct-builtin operation identity. The existing
focused gate must remain green while the next falsifier is the three accepted
mutants above. The old instruction-local/source-text fallback must be deleted
before the bridge can be considered closeable; adding another caller-owned ID
field or generic event-existence check will not close the SoT.

### Final baseline correction

After the append began, a concurrent CI-only commit advanced the checkout from
`4141904d5946fa7d727a13a07721f646c988f5fe` to
`3659ed25a1f3b65983bb2eeea181d9a1f269d862`; the latter is now
`HEAD == origin/main`. The seven audited source/gate file hashes and timestamps
were unchanged across that move, so every command and finding above remains
valid for the same collection packet. Re-anchor any future rerun to
`3659ed25` or the next exact HEAD; do not silently combine this addendum with a
later compiler schema change.

## Read-only stale-documentation audit at `84a2318d`

Audit scope was the ten requested documentation files. The shared channel is
ignored by `D:\PergyraLang\.gitignore:195`, so this append is intentionally
local and does not alter the tracked documentation set.

Authoritative baseline captured before the audit:

- `HEAD == origin/main == 84a2318d93cf01fd6fc31a551363d7281bb8dc54` on
  `main`.
- The working tree had 198 `git status --short` entries.
- The machine-gated registry block in
  `D:\PergyraLang\docs\semantics\sot_owner_spine_registry.md:43` contains 95
  rows: `CLOSED=66 / BRIDGE=27 / ACTIVE=2`. The two ACTIVE rows are
  `selfhost.semantic_artifact_admission` at line 102 and
  `semantic.hashmap_collection_ownership` at line 110.
- `python scripts/sot_registry_gate.py` exits 1 because the
  `semantic.hashmap_collection_ownership` row names required text that is
  absent from `tests/hashmap_owned_string_provenance_smoke.sh`. The registry is
  therefore not currently machine-green.
- `python tests/gate_script_reachability_smoke.py` exits 1 with
  `scripts=875 target-reachable=768 manual=56
  manual-registry-historical=1 undeclared=51 stale=0 dual=0`.
- GitHub CI run
  `https://github.com/srtdog64/PergyraLang/actions/runs/36113563646` is for
  the exact current commit. At the final snapshot it was still `in_progress`:
  30 jobs had completed successfully and
  `self-host-bootstrap-linux` remained in progress. It is not yet exact-head CI
  green evidence.

### Finding S1 (P1): the active handoff no longer records its exact working-tree or CI state

- Locations:
  `D:\PergyraLang\docs\current_work_handoff.md:7`,
  `D:\PergyraLang\docs\current_work_handoff.md:8`,
  `D:\PergyraLang\docs\current_work_handoff.md:16`, and
  `D:\PergyraLang\docs\current_work_handoff.md:60`.
- Evidence: the handoff correctly names HEAD `84a2318d`, but records 197 dirty
  status entries; the direct rerun found 198. It names historical runs
  `35603375178`, `35625507309`, and `35625615465`, while the exact-current-head
  run is now `36113563646` and is still in progress. The document therefore
  combines a correct commit anchor with stale dirty-tree and CI snapshots.
- Direction: keep the current commit/branch, refresh the dirty count and
  exact-head run, and label the older run IDs as historical. Do not infer
  current CI green until run `36113563646` is completed and its substantive
  job set is inspected.

### Finding S2 (P1): the active handoff contains superseded registry and gate censuses

- Locations:
  `D:\PergyraLang\docs\current_work_handoff.md:239`,
  `D:\PergyraLang\docs\current_work_handoff.md:240`,
  `D:\PergyraLang\docs\current_work_handoff.md:283`,
  `D:\PergyraLang\docs\current_work_handoff.md:284`, and
  `D:\PergyraLang\docs\current_work_handoff.md:570`.
- Evidence: the active card says 50 undeclared scripts over 841 or 839 total,
  with 737 or 735 reachable and 54 manual. The current executable is
  `875/768/56/51` (`total/reachable/manual/undeclared`). The same card calls the
  registry `95 authorities / 191 derived, CLOSED=63, BRIDGE=29, ACTIVE=3`,
  while the current registry block is `CLOSED=66 / BRIDGE=27 / ACTIVE=2`, and
  the registry gate itself is RED.
- Direction: generate the active-card census directly from the two parsers or
  replace it with explicit command output. Move superseded counts below the
  historical archive boundary rather than deleting the evidence. A
  `ACTIVE=2` census plus the failing registry gate does not support a
  whole-SoT-closure claim.

### Finding S3 (P1): `docs/INDEX.md` promotes superseded status documents as current authority

- Locations:
  `D:\PergyraLang\docs\INDEX.md:20`,
  `D:\PergyraLang\docs\INDEX.md:23`,
  `D:\PergyraLang\docs\INDEX.md:39`,
  `D:\PergyraLang\docs\INDEX.md:42`,
  `D:\PergyraLang\docs\INDEX.md:46`, and
  `D:\PergyraLang\docs\INDEX.md:231`.
- Evidence: the index calls `100a` “Active status, current blockers,” `100d`
  “Immediate execution order,” and `71` an execution-ticket breakdown, while
  those files now explicitly identify themselves as historical at
  `D:\PergyraLang\docs\100a_beta_active_status.md:3` and
  `D:\PergyraLang\docs\100d_beta_execution_log.md:3`. The index also places
  hard self-hosting after beta, but the current contract explicitly says hard
  self-host is active staged substitution at
  `D:\PergyraLang\docs\self_hosted\10_hard_self_host_contract.md:3` and defines
  the active-rung condition at line 12.
- Direction: reclassify `100a`, `100d`, and `71` as historical evidence in
  the index; make the current handoff and registry the navigation authority;
  describe self-hosting as an active bounded substitution track rather than a
  purely post-beta track. This is an index correction, not a rewrite of the
  historical documents.

### Finding S4 (P1): the vision document's “current state” conflicts with the active self-host owner policy

- Locations:
  `D:\PergyraLang\docs\00_vision.md:70`,
  `D:\PergyraLang\docs\00_vision.md:72`,
  `D:\PergyraLang\docs\00_vision.md:86`, and
  `D:\PergyraLang\docs\00_vision.md:375`.
- Evidence: the document says self-hosting is a post-beta target and orders
  beta closure before dogfooding. Its “Current state” then says the compiler
  core is implemented in C and the stable contract is C/LLVM dual emission.
  The current hard-self-host contract instead treats staged substitution as
  active while explicitly withholding any whole-product self-host claim. The
  vision wording can be retained as long-term philosophy, but it cannot remain
  the current execution policy.
- Direction: preserve the intent-first and long-term credibility sections, but
  replace the “Current state” block with bounded status: the released compiler
  is not wholly self-hosted, production Pergyra rungs are substituting named C
  consumers, and the active rung is selected by the hard-self-host contract.
  Move the old beta-first sequence into a dated historical or superseded
  section.

### Finding S5 (P2): `09_selfhost_status.md` is correctly labeled historical but still contains live-looking authority prose

- Locations:
  `D:\PergyraLang\docs\self_hosted\09_selfhost_status.md:3`,
  `D:\PergyraLang\docs\self_hosted\09_selfhost_status.md:24`,
  `D:\PergyraLang\docs\self_hosted\09_selfhost_status.md:37`, and
  `D:\PergyraLang\docs\self_hosted\09_selfhost_status.md:817`.
- Evidence: the top banner correctly calls the file historical and supplies
  the current `66/27/2` registry count, but later prose uses “Focused
  evidence,” “Current,” and “Current installed boundary,” repeats the old
  `49/36/1` count, and presents run `33071044311` as exact-head GREEN 29/29.
  A reader who enters at those sections can mistake a dated checkpoint for the
  current status.
- Direction: preserve the body as a dated snapshot, but add a single
  historical boundary above line 24 and demote or relabel the later
  “Current installed boundary” heading. Do not update old run evidence in
  place; point readers to the handoff and registry for current state.

### Finding S6 (P2): the changelog is neither current nor explicitly historical, and contains broken repository links

- Locations:
  `D:\PergyraLang\CHANGELOG.md:7`,
  `D:\PergyraLang\CHANGELOG.md:9`,
  `D:\PergyraLang\CHANGELOG.md:333`,
  `D:\PergyraLang\CHANGELOG.md:853`,
  `D:\PergyraLang\CHANGELOG.md:865`, and
  `D:\PergyraLang\CHANGELOG.md:899`.
- Evidence: the newest named entry is 2026-07-28 even though the repository
  has current September work. Historical links point to `/mnt/e/PergyraLang`
  paths, and the five split `.inc` links at lines 899-900 target files that no
  longer exist. Repository-relative link resolution found 14 missing link
  instances in this file.
- Direction: either add the missing unreleased entries and convert historical
  links to repository-relative paths, or explicitly freeze the changelog after
  2026-07-28 and point current changes to release notes generated from later
  evidence. Do not rewrite old “current” wording inside dated release notes;
  historical prose is valid in its original context.

### Finding S7 (P2): remaining relative-link defects are outside the already-repaired target list

- Locations:
  `D:\PergyraLang\docs\README_ko.md:309` and
  `D:\PergyraLang\docs\00_vision.md:117`.
- Evidence: `LICENSE` in `docs/README_ko.md` resolves to
  `D:\PergyraLang\docs\LICENSE`, which is absent; the repository license is at
  the repository root. The intent-design link in `docs/00_vision.md` contains
  a `docs/` prefix inside a file already under `docs/`, so it resolves to
  `D:\PergyraLang\docs\docs\01_intent_first_design.md`. The current dirty
  corrections to the other `README_ko` links and both `71_beta_execution_tickets`
  links are present and should be preserved.
- Direction: use `../LICENSE` from `docs/README_ko.md` and
  `01_intent_first_design.md` from `docs/00_vision.md`. Add a repository
  relative-link checker that treats machine-specific absolute paths and
  historical missing split files as explicit exceptions rather than silently
  accepting them.

### Finding S8 (P2): the Korean documentation index still presents implemented surfaces as TODO

- Locations:
  `D:\PergyraLang\docs\README_ko.md:278`,
  `D:\PergyraLang\docs\README_ko.md:280`, and
  `D:\PergyraLang\docs\README_ko.md:285`.
- Evidence: the TODO list still includes the Effect System and package manager,
  while the current self-host status at
  `D:\PergyraLang\docs\self_hosted\09_selfhost_status.md:825` describes
  compiler-bearing package commands and the repository has named semantic
  effect owners. The index does not distinguish “some surface exists” from
  “beta-stable and fully closed,” so the list is either false as a capability
  inventory or misleadingly incomplete as a closure board.
- Direction: split the TODO into `implemented / reachable / beta-open /
  future`, with an owner and focused gate for every current item. Link the beta
  stable subset and SoT registry rather than maintaining a second unqualified
  status list.

### Preservation classification

- `D:\PergyraLang\docs\100d_beta_execution_log.md` and
  `D:\PergyraLang\docs\self_hosted\self_host_completion_log.md` are journals,
  not live status owners. Their dated old registry counts, CI runs, and “next”
  work are legitimate history once the index stops routing readers to them as
  current authority. The completion log is not strictly reverse-chronological
  near its tail, so readers must use entry dates rather than file position.
- `D:\PergyraLang\docs\100a_beta_active_status.md` may remain a preserved beta
  closure snapshot, but its filename and index description should be paired
  with an unmistakable historical label.
- `D:\PergyraLang\CHANGELOG.md` should preserve dated release claims; only its
  current/unreleased boundary and links need correction.
- The active `D:\PergyraLang\docs\current_work_handoff.md` and the registry/gates
  remain authoritative. On this baseline, the overall SoT is not closed:
  27 rows remain `BRIDGE`, 2 remain `ACTIVE`, the registry gate is RED, gate
  reachability is RED, and exact-head CI is still in progress.

## Collection ownership SoT exact-falsifier rerun

Read-only rerun on `HEAD 84a2318d93cf01fd6fc31a551363d7281bb8dc54`.
No compiler, test, or fixture source was changed. The only intended write is
this gitignored shared-audit append. The active self-host card was read first;
its collection-ownership P0 rung remains open.

### Fresh execution baseline

The compiler was rebuilt from the current dirty source into an isolated tree:

```text
binary: .tmp/collection-sot-audit/bin/pgy.exe
sha256: 644AFD50BAA23E9DD7C9A584400949856CB515C2D15E0D523CB0857825C12CF7
build: MSYS2 bash + make -j8 with isolated BUILD_DIR/BIN_DIR
gate: PGY_BIN=.tmp/collection-sot-audit/bin/pgy.exe \
      bash tests/collection_own_parameter_boundary_smoke.sh -> exit 0
```

The focused gate's `PASS` is not a closure witness for the three exact
falsifiers below. It does not mutate a source-positive transfer event to a
nonexistent event, coordinate the producer fact with instruction
`expr0_call_syntax_id`, or relabel a direct collection builtin to `Log`. Its
producer negative coordinates only the producer fact and source-local field;
its operation negative uses another receipt/event shape. A Git Bash reader
attempt was invalid (`exit 127`, DLL resolution) and discarded. The following
results use native Windows invocation of the reader built by that passing gate
from the same fresh compiler.

### Confirmed P0-1: nonexistent source-positive transfer event is admitted

Current native output has transfer `source=5 destination=11 event=15`, but no
instruction event `15` (producer call `7`, literal def `14`, release call
`16`). Replacing only the receipt event with `100015` still gives:

```text
control-source-positive      exit 0
source-positive-forged-event exit 0
stdout  2:11:14:0:owned-elements:retired:owned-literal-transfer
stderr  empty
```

Owner cause:
`MirCollectionOwnershipReceiptInstructionEventReady` returns `true` for every
positive-source `transfer-element` before scanning instructions
(`src/self_hosted/mir_lower/collection_ownership_fact_owner.pgy:134`, return at
line 156). The native writer projects `inst->expr0` rather than owning a typed
routine-local collection-operation fact (`src/compiler/mir_json_dump.c:343`).

### Confirmed P0-2: coordinated producer-call forgery is admitted

The valid producer is `binding=5 call=7`; source-local initializer and producer
instruction `expr0`/`expr0_call` are also `7`. Changing all three serialized
copies to `1007` while leaving native provenance unchanged still gives:

```text
control-source-positive        exit 0
producer-coordinated-forgery   exit 0
stdout  2:11:14:0:owned-elements:retired:owned-literal-transfer
stderr  empty
```

Owner cause: the producer validator joins three caller-controlled sibling
fields but not the instruction's independently owned `expr0_syntax_id` or
typed semantic call/builtin identity
(`src/self_hosted/mir_lower/collection_ownership_fact_owner.pgy:109`, line 717).
Scalar parsing records identity fields, but object readiness requires only
`kind` and `source_type`
(`src/self_hosted/mir_lower/routine_instruction_scalar_capture_owner.pgy:123`).

### Confirmed P0-3: direct collection builtin relabeled as `Log` is admitted

`empty_local_owned_push_valid.pgy` has transfer event `9`; its instruction is
`ArrayPushOwnedString`. Replacing only serialized `arg0` with `Log` still gives:

```text
control-direct-builtin         exit 0
direct-builtin-log-relabel     exit 0
stdout  2:5:8:0:owned-elements:retired:empty-literal
stderr  empty
```

Owner cause: `MirCollectionOwnershipDirectBuiltinOperation` routes authority
through `arg0` text and returns `None` for an unknown name
(`src/self_hosted/mir_lower/collection_ownership_fact_owner.pgy:187`). The
consumer then falls back to generic event existence (same file line 825).
Although the native writer emits `expr0_semantic_stdlib_callee`, the typed
instruction bundle does not consume it.

### Minimum closure plan by owner

1. **Native operation producer plus self-host consumer.** Own a routine-local
   typed inventory containing routine/instruction anchor, event, operation kind,
   source/destination binding, semantic callee/builtin identity, and effect
   ordinal. Perform an exact receipt-to-operation join, delete the unconditional
   transfer return, retain the green two-actual fan-out, and add nonexistent
   event, wrong kind/destination, missing/duplicate row, and cross-routine reuse.
2. **Instruction identity writer plus typed bundle readiness.** Require unique
   schema-v1 `source_syntax_id`, `expr0_syntax_id`, and `expr0_call_syntax_id`;
   for `AST_CALL`, require nonzero call identity equal to `expr0_syntax_id` plus
   sealed semantic callee identity. Reject single-field deletion/mutation,
   coordinated sibling rewrite, user-call/same-name builtin spoof, and missing
   or zero identity.
3. **Direct builtin operation owner.** Carry typed builtin kind and semantic
   call identity in the operation inventory instead of `arg0` text. Missing or
   unknown kind must fail; delete the `None` fallback to generic event existence.
   Add `Log`/non-collection relabel, unknown builtin, user-defined callee, and
   changed event/destination negatives.

### Closure verdict

`semantic.hashmap_collection_ownership` is **not closeable on this baseline**.
The positive two-actual fan-out is green, but all three exact P0 forgeries are
accepted. Do not mark the row `CLOSED`, delete the remaining legacy path, or
treat the focused gate's current `PASS` as CI/bridge evidence. Closure requires
the typed operation replacement, exact negatives, installed self-host admission,
and substantive CI on the resulting exact commit.

## Current coordination checkpoint — HEAD `84a2318d`

The local handoff is intentionally refreshed only for the current checkout and
navigation state. The SoT registry remains the authority:

```text
HEAD == origin/main == 84a2318d93cf01fd6fc31a551363d7281bb8dc54
registry rows: 95
registry census: CLOSED=66 BRIDGE=27 ACTIVE=2
active rows: selfhost.semantic_artifact_admission,
            semantic.hashmap_collection_ownership
scripts/sot_registry_gate.py: RED
tests/gate_script_reachability_smoke.py: RED (875/768/56/51)
```

Exact-head GitHub CI run `36113563646` is still `in_progress`; 30 jobs are
complete successfully and `self-host-bootstrap-linux` is the remaining
in-progress job. This is not CI-green evidence. The worktree is dirty and the
current CI run covers the clean pushed commit, not the dirty local changes.

The collection ownership row is not closed. The focused native reader gate
passes, but three independently reproduced fail-open mutants remain:

1. a source-positive array-literal transfer event can be fabricated because
   the reader returns success before checking an independent operation event;
2. a producer row, source-local initializer, and instruction call ID can be
   changed together without an independently sealed producer-call identity;
3. a direct `ArrayPushOwnedString` event can be relabeled as `Log` in the
   instruction row and the generic event fallback accepts it.

Documentation cleanup in this checkpoint is limited to navigation truth:
`100a`, `100d`, `09_selfhost_status.md`, and the self-host completion log are
now marked historical; README_KO and beta ticket relative links were repaired;
the current handoff and registry are the only live status sources. The local
shared channel remains ignored by Git and is not a compiler authority.
## Exact-head CI terminal update

GitHub Actions run
`https://github.com/srtdog64/PergyraLang/actions/runs/36113563646`
completed successfully for exact clean head
`84a2318d93cf01fd6fc31a551363d7281bb8dc54`; all jobs are terminal
successful and the run has no failed or cancelled job. This supersedes the
earlier in-progress checkpoint above for clean-head CI only. It does not cover
the current dirty worktree, the new documentation edits, the registry evidence
normalization, or the in-progress collection-operation inventory work.
## Documentation and registry checkpoint

The documentation pass now:

- labels `100a_beta_active_status.md`, `100d_beta_execution_log.md`,
  `09_selfhost_status.md`, and `self_host_completion_log.md` as historical
  evidence rather than current status owners;
- routes current status through `current_work_handoff.md` and the SoT registry;
- classifies hard self-hosting as active staged substitution, not a completed
  whole-compiler replacement;
- repairs the confirmed README_KO and beta-ticket relative links;
- keeps the shared audit channel itself Git-ignored.

`tests/documentation_quality_smoke.sh` passes. The SoT registry machine gate
now passes with the truthful active census:

```text
95 authorities
192 derived fact carriers
CLOSED=66 BRIDGE=27 ACTIVE=2
```

The installed ownership gate remains red and is recorded in the registry as
an active blocker, not as passing evidence. Gate reachability remains red
with 51 undeclared executable scripts. These documentation repairs do not
close the collection ownership bridge.

## Verifier lane checkpoint — HEAD `fbc55f86` (BRIDGE row closures)

Two registry rows closed and landed on origin/main (census
`CLOSED=67 BRIDGE=26 ACTIVE=2`):

- `semantic.function_param_flow_summary` — `84a2318d`, CI run 36113563646
  green. Deleted the context-free AST summary seams, the program-root name
  scan in `slot_analyzer_find_function_decl`, and the never-incremented
  `depth` budget; added HIR and MIR/AIR unknown-routine refusal tests.
- `abi.runtime_call_rows` — `327faa87` + `fbc55f86` (CI pending at this
  checkpoint). Deleted `mir_abi_resource_runtime_row_by_kind` /
  `fn_by_kind`; backends without MIR fail closed; the self-host constructed
  runtime rows are now compared with the native owner by test-mir.

Found on the way (open, not fixed here):

- Gate bug, large: `! grep -Fq ...` statement lines never fail a
  `set -euo pipefail` script (bash exempts `!` pipelines from errexit).
  `tests/perf_contract_smoke.sh` has 713 such lines; several other gates
  have 7-19. Offered to the user as a separate task.
- `type_check_claim_slot` reports "ClaimSlot takes no arguments" for
  `ClaimSecureSlot(level)` as well (wrong callee name in the message).
- Default route: a class with a destructured claim field
  (`tests/cases/backend_compare/secure_field_slot`) is refused with
  `call_arity_mismatch` (expected 2, actual 3 on the secure Write), a
  wrong code for an unsupported surface.
- `20633bd9` fixed `import_resolver.c` IoError composition, which a
  verifier-lane commit (`2334dcf5`) had written with raw snprintf offset
  accumulation; `memory-string-safety-test-smoke` (Linux platform shard,
  not push) was red since 2026-09-24.
- `abi.layout_rows` now records that a Slot over a nominal payload has no
  layout row (the constructed-row exemption in C and LLVM).

---

## 2026-09-25 collection operation inventory: root-cause session

Agent-facing channel. This section records what was actually executed and
observed on this dirty tree, not what was intended.

### Baseline reproduced

Exact HEAD at session start: `fbc55f86c552b866a6e7da98f845f0cffe4453f9`.
Worktree: ~205 `git status --short` entries, all pre-existing concurrent
compiler ownership work. No commit was made by this session.

`make -s -j8 BIN_DIR=.tmp/r1-inv-test test-mir` in a fresh BIN_DIR:

    === Results: 217 passed, 9 failed ===

The 9 failures were all in
`src/tests/mir/test_mir_collection_ownership_validation.cases.h`, a file that
grew from 117 lines at HEAD to 844 lines in the worktree. So the 9 reds were
the new inventory's own tests, not pre-existing regressions. The pre-existing
suite was green before the inventory landed.

### Root cause of the 9 reds (measured, not inferred)

Instrumented `mir_seal_collection_operation_inventory` behind
`PGY_DEBUG_COLLECTION_INVENTORY` and observed the exact failing rows. Two
distinct defects, both join-key errors rather than missing work:

1. **Direct builtin transfer demanded a user-call boundary.**
   `mir_collection_builtin_operation_ready`, case
   `PGY_COLLECTION_RECEIPT_TRANSFER_ELEMENT`, required exactly one matching
   `MIRCollectionCallBoundaryFact` whenever the anchor was a `MIR_INST_STMT`.
   `PgyCollectionOwnershipReceipt.function_syntax_id` is
   `current_function_decl` (src/semantic/collection_ownership_fact.c:42,336,360),
   i.e. the *enclosing* function, and `ArrayPushOwnedString` is a stdlib
   builtin, not a user callee — it legitimately has no boundary row. Measured
   `bcount=0` with `collection_call_boundary_count == 1` (the one row belonged
   to a different call) and `bcount=0` with `collection_call_boundary_count == 0`.

2. **Release receipts were anchored by a one-past-end sentinel.**
   `instruction_id = routine->instruction_count` is not a lowered
   instruction. Every `ArrayDropOwnedStrings` release receipt took this
   fallback, so validation then failed with `no unique lowered instruction
   owner`. `mir_collection_release_operation_ready` also rejected
   `MIR_INST_STMT`, which is exactly the shape of an explicit drop call.

3. **Ordering bug that made (1) fatal.** In the mis-kind dispatch inside
   `mir_seal_collection_operation_inventory`, the generic
   `AST_CALL + TRANSFER_ELEMENT` boundary-join branch was evaluated *before*
   `mir_collection_builtin_operation_ready`. A resolved builtin call was
   therefore classified as a user function call and then required a boundary
   row that cannot exist. This is why fixing (1) alone was not enough.

### Fixes applied in src/compiler/mir_branch_source_facts.c

- `PGY_COLLECTION_RECEIPT_TRANSFER_ELEMENT` / `MIR_INST_STMT`: anchor on the
  resolved builtin call itself (event id + receiver binding + resolved
  builtin kind) and require `source_binding_syntax_id == 0`, which is the
  semantic contract for a runtime-duplicating push. No boundary join.
- `PGY_COLLECTION_RECEIPT_RELEASE`: removed the `MIR_INST_STMT` rejection so
  an explicit `ArrayDrop` / `ArrayDropOwnedStrings` statement call can own
  the release.
- Mis-kind dispatch: a resolved builtin call now short-circuits to
  `builtin_owner` before the user-call boundary join.

All temporary `PGY_DEBUG_COLLECTION_INVENTORY` instrumentation was removed
afterwards; `rg colinv src/compiler/mir_branch_source_facts.c` returns nothing
and the file is back to 1662 changed lines.

### Result after the fix

    === Results: 227 passed, 0 failed ===

All 9 new inventory negatives now execute for real (they previously could
not even reach their assertions, because sealing failed first):
typed-collection-inventory forgery, carrier in-memory mutation, owned String
producer row, empty-origin transition, inout effect, pop effect, branch/loop/
self-recursive inout, repeated-formal edges, and the admitted exact call edge.

`make ... test-mir` continues past MIR 227/227 and past
`domain_runtime_topology_smoke.sh`, `destructure_type_fact_smoke.sh` and
`match_binding_type_fact_smoke.sh` (each "0 error(s), 0 warning(s)").

### SoT registry

`scripts/sot_registry_gate.py` failed on the new self-host file:

    [sot-authority-edge] FAIL self-host fact owner has no authority/derived
    classification: src/self_hosted/mir_lower/collection_operation_fact_owner.pgy

The rule (scripts/sot_registry_gate.py:329-338) requires every
`src/self_hosted/**/*_fact_owner.pgy` to be an authority or a declared
derived carrier. Registered it as a derived projection of the same owner:

    src/self_hosted/mir_lower/collection_operation_fact_owner.pgy |
      MirCollectionOperationFacts | semantic.hashmap_collection_ownership |
      projection

After that:

    [sot-authority-edge] 95 authorities, 193 derived fact carriers;
    CLOSED=67 BRIDGE=26 ACTIVE=2

**`semantic.hashmap_collection_ownership` is still `ACTIVE` and is NOT closed.**
The MIR inventory slice is now green; the rung is not.

### Why the rung is still ACTIVE (open, verified)

- `make ... test-mir` still exits non-zero. MIR 227/227 passes, then the
  self-host emit of `src/self_hosted/mir_lower/main.pgy` fails.
- The self-host failure is a real arity bug, not a test artifact. Reverting
  only `src/self_hosted/mir_lower/collection_ownership_fact_owner.pgy` to its
  HEAD version while removing the new `collection_operation_fact_owner.pgy`
  reproduces:
  `call_arity_mismatch`, `func: BuildMirCollectionOwnershipFacts`,
  `expected: 6`, `actual: 9`.
  A 7th field `operation_facts: MirCollectionOperationFacts` was added to
  `struct MirCollectionOwnershipFacts` and used at the constructor call, but
  the `func BuildMirCollectionOwnershipFacts` signature was never extended to
  accept and thread it. This is a defect in the Pergyra-side consumer half,
  independent of the C-side fix above.
- `tests/self_hosted/parity/mir_json_instruction_writer_byte_parity.sh` was
  already reported RED by the earlier agent for a second arity defect:
  `MirRoutineInstructionFactBundle` constructor arity (21 vs 23 args). Not
  yet independently reproduced in this session; do not treat as closed.
- Installed self-host parity remains unproven. The installed
  `bin/pgy-self-driver.exe` predates these keys, so no positive installed
  parity claim is admissible.
- The three P0 falsifiers from the previous section of this document are
  unaffected by the MIR inventory work. R1 (`Array<Int>` local copy aliases
  storage), R2 (shallow `Array<String>` copy), R3 (`Unwrap(Err)` continues),
  R5 (Float/Double `ToString` precision) are untouched by this diff and
  remain RED on their own oracles.

### Not claimed

- No whole-rung substitution. `PgyCompilerWorld` / installed driver reach is
  unchanged; this is a native MIR producer + Pergyra MIR reader slice.
- No negative-residue gate was added for the removed boundary requirement.
  That is a known gap: nothing yet prevents a future re-introduction of a
  name-only or boundary-free transfer join. See the open item below.

### Next falsifying fixture

The next falsifier is a coordinated forgery that exploits the newly weakened
join: a `TRANSFER_ELEMENT` receipt with `source_binding_syntax_id == 0` whose
event id is rewritten to point at a *non-transfer* resolved builtin call in the
same routine, with the semantic receipt rewritten in the same commit. The
current guard is `event id + receiver binding + resolved builtin kind`, which
is a real improvement over "any boundary", but it is not yet a proof against a
coordinated rewrite of both sides.

---

## Verifier lane checkpoint — HEAD `463406e9` (language-word registry)

`lexer.language_word_registry` is CLOSED and landed (census
`CLOSED=68 BRIDGE=25 ACTIVE=2`). Two series on top of `0ed07144`:

- `a63fe84e..b186c7da`: CI for `0ed07144` (run 36125932250) was red on two
  Linux gates that `327faa87` broke. protocol-registry still named the
  deleted `mir_abi_resource_runtime_row_by_kind`, and
  parallel-capture-projection required the deleted LLVM module-level branch
  text. `a63fe84e` repoints both. The rest of the series: whole-word export
  detection, effect-clause words / `innate` / private abilities, world
  members and zone slot groups, duration literals.
- `761aa88e..463406e9`: rollback policies and routine head words selected by
  identity, a raw-selector census over every parser owner, the parallel join
  header, identity refusals for `select`, `pin`, `is`, `reflect` and `give`,
  and `every`/`continuous` declared unimplemented. Native-only words: 29 at
  `0ed07144`, 0 now, and the language-word smoke requires 0.

Found on the way:

- Fixed, `b186c7da`: the default route accepted `let x: Int = 5ms;` and
  `7foo` as numbers and printed 12; native refuses `7foo`.
- Fixed, `33e8d5e0`: the self-host export detector matched `export` inside
  `is_export`, so a parser module counted as exporting. The import prefix
  then renamed its private `ParserEffectWordRead` declaration but not a
  return-type annotation naming it, and the bootstrap refused its own
  parser. That rename gap is still open; nothing on the current path
  reaches it.
- Fixed, `12240532`: the self-host statement form
  `parallel (x in xs) { Log(x); }` was skipped to the first `;`, so the join
  body's `}` closed the enclosing block.
- Open: the self-host parser prints `Let: mut x` where native prints
  `Let: x`. Parser parity fixtures avoid `let mut`, so no gate sees it.
- Open: the default route refuses a parallel join expression with
  `initializer_type_unresolved`, a wrong code for an uncovered surface.
- Open: `zone_effect_pool_runtime` on the default route now reaches the
  `HasLayer` refusal (`builtin_native_pipeline_only`), but its span is the
  zone line 7:1, not the call at 13:13.
- Open: `tests/vscode_language_graph_smoke.sh` ignores every path that
  contains `.tmp`, so it fails in any worktree under `.tmp/`.
- Still open from the last checkpoint: `ClaimSecureSlot(level)` reports
  "ClaimSlot takes no arguments", and `! grep -Fq` lines never fail a
  `set -e` gate.
