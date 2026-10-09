# Ownership cutover: proof integration and measured P0 obstructions

Observed: 2026-10-09 KST, GPT. Same shared `main @
a75da80435e0d051f71cfacf76d44ea825ed87d7`. No production source, workflow,
official installation, stage, commit, push, CI dispatch or GUI message was
changed by this work. These are dirty-tree diagnostics, not a frozen P0
baseline or an automatic-memory-management implementation.

The latest user instruction, "전체 작업해", authorizes the full cutover.
It does not remove the cutover plan's input-freeze, P1 dependency or final
installation gates. The prior freeze preceded Claude's resumed proof work.
A new all-writer freeze and second reviewed checkpoint remain pending.

## Independent proof integration

GPT did not edit Claude's model definitions. The existing permanent consumer
`tests/coq/OwnershipCutoverPreflightAudit.v` now imports the two new models
and independently consumes their exact theorem types and positive/negative
witnesses. The focused wrapper includes them in its fresh kernel snapshot.

- Actual ordinary procedure-table call: two inouts and an independent
  outcome packet recover on normal/return and handled-error exits. The
  witness uses the real core call rule, not a `recovery_exec` premise.
- Repeated formals, actual-value alias, readonly-local alias, escaping
  break/continue and mentions of compiler-issued names are refused.
- Scoped writable view: element writes are reflected in the original
  backing, backing length remains fixed inside the scope, and a subsequent
  push and final teardown execute without leaked blocks in the model.
- Growth/copy/call of the suspended backing or view, read-then-grow,
  branch/loop growth, self-view and invalid range cases are refused.

Observed commands, with `OPAMROOT=/home/c/.local/share/pergyra-rocq/opam`
in WSL `Ubuntu-E-WSL`:

```sh
timeout 300s bash scripts/run_rocq_toolchain.sh \
  bash tests/ownership_cutover_preflight_smoke.sh
timeout 1800s bash scripts/run_rocq_toolchain.sh \
  bash tests/formal_semantics_smoke.sh
```

Focused: PASS, nine kernel-checked modules, zero assumptions, no admits or
unsafe kernel features. Full integration: PASS, 69 owners plus eight
permanent consumers, 77 modules and the approval export/binding consumer.
Only the two existing approved Slot abstractions are assumed. The existing
nested-list scheme, Stdlib deprecation, module masking and approval load-path
warnings remain visible in the logs; PASS does not mean warning-free.

Logs under `.tmp/ownership-cutover-2026-10-09/`:
`p1-importing-focus.log`, `p1-formal-integration.log`.

| Input | SHA-256 |
|---|---|
| `OwnershipCleanCallLowering.v` | `8db2031418cad5e1d91b0e9b9323b79d2a50ed611f54e8e0511c3d8285572026` |
| `OwnershipCleanViewScope.v` | `f7c7a5c58bce85fbb0ec0d2c612a544bfc1d90c57d65f1e81baccf5f09c8fd04` |
| `tests/coq/OwnershipCutoverPreflightAudit.v` | `8d62aed402f19da456f614f46839eb646bdf86be11fd95c1dc09404dbb5a902f` |
| `tests/ownership_cutover_preflight_smoke.sh` | `b8bfefc34dc93b386b376ed15a486a4105207afe4123d1864f3bb22bb5383c3c` |

**Not closed:** source-place/expression evaluation order, direct-jump
epilogue refinement, physical pointer/bundle/descriptor refinement, actual
ABI contract and final-generation lifetime issuers. The view model admits
a narrow whole-backing scope, not arbitrary escaped/passed/stored/multiple
views. A lexical survey found 77 `.Slice(` lines in 21 self-host source
files; some are quoted fixtures. This is not a semantic call count or proof
that the restricted model accepts the existing compiler. Do not turn the
restriction into a blanket production rejection without the P1 DX review.

## Fresh native diagnostic and executable coverage

An isolated native MSYS2 `make compiler LLVM_ENABLED=1 -j2` completed in
302386 ms, exit 0. The existing v3 measurement receipt binds 2048 native
source/Make inputs and declared tools without byte drift. LLVM 22 is linked.
The resulting executable is:

`.tmp/ownership-cutover-2026-10-09/native-bin/pgy.exe`

SHA-256:
`8b26d1d82cf94481ff76a26eaa83b151e6f38c4f3b2099dc4c5c7e17d24af754`.

The build's root-only observer did not follow detached MSYS compiler/link
workers (zero compile/link samples). Its small root-process pressure is
**partial coverage**, not evidence that the complete build fits 3 GiB.
Receipt: `pressure/native-build-diagnostic.summary.json`.

Using that fresh executable, native-only
`tests/self_hosted/parity/public_array_drop.sh` PASS on C and LLVM.
Evidence: `.tmp/self_hosted/public-array-drop.s2zel6/` and
`p0-native-array-drop.log`. The shared summary describes all-stage routes,
but this invocation selected `PGY_ARRAY_DROP_STAGE=native`; source-C,
public-driver and issued-MIR stages were not run. This verifies the existing
manual release contract, not automatic teardown.

The isolated native unit shard built and executed lexer/parser, semantic,
transpile, memory, DIR, AIR, RIR, MIR and HIR batteries. Reported batteries
have zero failed tests. The shard as a whole returned 1 when its subsequent
MIR match-binding integration tried the public path without a same-source
self-host driver. Log: `p0-native-unit-shard.log`. No stale official driver
was substituted. Therefore the integration shard is **not PASS**.

## Same full input refuses the unchanged memory ceiling

Every run below uses the complete import-composed input:

```sh
pgy.exe --native-pipeline --mir-json \
  src/self_hosted/compiler/driver_bootstrap_main.pgy
```

The existing pressure owner binds all 2531 tracked self-host source files
and the actual executable; all runs have `binding_changed = false`.
The cap remains **3072 MiB**, with a 1800 s integration budget and
`StopOnLimit`. The direct-process observer sees the compiler itself; this
is not the MSYS detached-worker coverage gap above. No MIR output was issued.

| Receipt label | Executable | Elapsed ms | Peak process-tree private MiB | Verdict |
|---|---|---:|---:|---|
| `native-full-driver-mir-diagnostic` | fresh reference | 78958 | 3210.3 | 88, cap refusal |
| `native-full-driver-mir-stages` | same reference, existing timing flags | 78205 | 3080.2 | 88, cap refusal |
| `native-full-driver-mir-profile` | scratch MIR counters v1 | 84144 | 3078.6 | 88, cap refusal |
| `native-full-driver-mir-profile-v2` | scratch MIR counters v2 | 88733 | 3074.9 | 88, cap refusal |

Receipts, samples and logs are under `pressure/` beneath the run directory.
The measurement owner terminates its own command on the cap; a wrapper's
exit 1 must not replace the summary's explicit pressure verdict 88.
Existing stage stderr reaches semantic/HIR/DIR/RIR/AIR validation and then
`mir_lower`. Stage-specific CSV has zero recognized rows because its parser
recognizes a different stage prefix; the native stderr, not that CSV, owns
this diagnostic localization. Semantic errors are zero in the reference
stage run, with 21 existing warnings.

Scratch counters were added only to a copy of `src/compiler/mir.c`, using
the existing fact/block/SSA operations and scratch-arena counters. Native
objects and the separately named diagnostic executable remain under `.tmp`.
Two existing argument-evaluation-order fixtures produce byte-identical MIR
with the reference and each diagnostic. Their reference SHA-256 identities:

- `call_argument_evaluation_order`: `64ecba14b01169e5bfd5bd46133f0475c480b2f5271605e6ad683aa02b9ce136`.
- `call_argument_evaluation_order_method`: `e47006e717a73088ceafff3489a9661ede215bfb95287093ce95b0e529163eef`.

V2 executable SHA-256:
`efac65f4dbd05b93ca238f8f76db67bdeda82de821712a701f16aad9210b115f`;
scratch source SHA-256:
`151ae8ca0bd9a4ce6c43b601d369f3eacf8aafbada2fff9c385c543309cc7b4d`.
The v1 scratch executable/source were replaced by this local instrumentation
revision after its run completed. Its original identities remain in its
receipt; do not claim the v1 binary is still available at the overwritten path.

V2 contains 68813 complete counter rows, from routine 0 to routine 2085;
the incomplete final line is excluded. At the first routine entry private
memory is already 2824933376 bytes (2694.066 MiB). The last complete row is
3223805952 bytes. Aggregated **net private-byte increases within a routine**:

| Existing operation | Net MiB over the completed intervals |
|---|---:|
| `mir_copy_resource_flow_symbols` | 116.69 |
| HIR block projection | 105.98 |
| `mir_copy_loop_flow_facts` | 48.50 |
| SSA rename | 46.03 |
| Statement/machine/runtime instruction population | 26.95 |
| Use-edge construction | 18.89 |
| Analysis recompute | 10.68 |

These are observed OS net deltas, not allocation totals, heap-live peaks,
asymptotic costs or proof of redundant work. The large entry memory predates
these MIR per-routine operations. Source inspection confirms that resource
and function-parameter flow transfers take the matching routine's rows,
not the whole program's parameter summaries. No global-copy hypothesis or
unchecked borrowing optimization is accepted as fact. Heap peak-live and
cumulative allocation counters remain **UNMEASURED**.

### Follow-up: admitted-kind census and a bounded scratch repair

The resumed diagnostic used separately named v3 and v4 MIR scratch copies.
They read the already admitted semantic/HIR/RIR rows, not an AST reconstruction.
Their two argument-evaluation-order fixtures remain byte-identical to the
fresh reference. Same complete input, same cap, no in-run binding drift:

| Receipt | Elapsed ms | Peak private MiB | Verdict |
|---|---:|---:|---|
| `native-full-driver-mir-profile-v3` | 89307 | 3075.3 | 88, cap refusal |
| `native-full-driver-mir-profile-v4` | 89894 | 3086.9 | 88, cap refusal |
| `native-full-driver-flow-values` | 64190 | 1320.5 | 0, MIR issued |

V4's MIR-entry private bytes are 2,806,571,008. Its admitted census is:

| Carried fact | Before the scratch repair | After, emitted MIR |
|---|---:|---:|
| Routine inventory | 8637 | 8637 |
| Resource-flow rows | 4,755,882 | 25,434 |
| CLASS declaration rows | 4,730,448 | 0 |
| VARIABLE rows | 25,434 | 25,434 |
| Loop state rows | 6,729,502 | 52,498 |

The other fifteen symbol kinds have zero rows on this full input. CLASS rows
are approximately 99.5% of the original table. HIR and RIR each retain the
same 4,755,882 rows before MIR; HIR capacity is 5,808,232 symbols and 8,974,336
states, with 161,571,148 bytes of copied symbol names. These are exact
container/payload counts, not heap-live counters. The post-repair counts
come from a bounded streaming text census of the issued MIR; that consumer
does not independently validate the complete JSON schema or semantics.

The source chain explains the measured amplification. The snapshot type
classifier includes BORROW_TRACKED/SUBJECT_IDENTITY types while walking all
parent scopes. Nominal type declarations have those same Types but no value
storage. Each snapshot binds them into the function-local universe; loop
summaries copy their states, and HIR/RIR/MIR retain their local projections.
This is not a whole-program parameter-summary copy: that earlier hypothesis
was rejected by source inspection.

The scratch candidate keeps admission with the existing
`semantic.resource_flow_universe` owner. One predicate excludes only
SYMBOL_CLASS and SYMBOL_TYPE_PARAM, before memo lookup in snapshot selection
and at universe binding, nested-declaration recording and function sealing.
All other kinds, value-type classification, identity epochs, parameter
identity, scope ownership and missing-fact failures remain unchanged.
TYPE_PARAM names are not values; a generic value parameter is still VARIABLE
or SLOT. No downstream consumer drops or guesses missing rows. Production
source and registry status are unchanged.

This candidate replaces only two native objects in a separately linked
executable. The old production objects and official binary pair are intact.
The full pressure run binds all 2531 tracked self-host sources, both scratch
sources, their declaration header and Make overlay. The overlay subsequently
gained separate unit-test/profile targets; its old in-run hash remains in
the receipt. The command and two candidate source bytes are preserved.

Observed candidate verification:

- Full source-to-MIR pipeline finishes under the original ceiling, with
  0 semantic errors and 21 existing warnings. No safety validation is skipped.
  This is dirty-tree native evidence, not official P0 or self-host substitution.
- Semantic unit battery, with classification-memo revalidation: **3107
  passed, 0 failed**. HIR **26**, DIR **15**, RIR **26**, AIR **147**, MIR
  **217** passed, each with 0 failures. These are observed unit executions,
  not completion of their follow-on self-host/default-route Make shards.
- Independent admission probe checks every current kind, forbids binding
  CLASS/TYPE_PARAM, retains a generic value parameter's stable identity,
  and refuses a snapshot when the active function's universe is missing.
- A stable-ID source fixture retains the typed aggregate parameter, generic
  value parameter, nested Slot after scope teardown and the loop summary
  with a legitimate empty state span. The actual old producer fails this
  row-count oracle with exit 1. Its CFG/instructions, source bindings,
  signatures and other ownership facts match the candidate on this fixture.
- The two existing argument-evaluation fixtures remain byte-identical.
- Existing DIR/RIR resource-flow identity and loop-summary gates PASS,
  including malformed/duplicate identity refusals and 1..4 loop re-entry.
  The DIR gate's printed ASan-coverage label is structural registration,
  not a newly run ASan battery.
- Native-only public ArrayDrop gate PASS on C/LLVM: the positive storage,
  inout and declaration-order cases execute and the existing refusals remain.
  Its shared final message mentions all-stage/public/MIR-input routes;
  those routes were NOT executed with `PGY_ARRAY_DROP_STAGE=native`.
- Type aliases cannot masquerade as releasable storage: `Release(SlotType)`
  and `ArrayDrop(RowsType)` both refuse at semantic admission with their
  existing specific diagnostics, exit 1.

The full MIR artifact is 378,143,208 bytes, SHA-256
`52290d05634c1a95e81d10c3804f7745cd6d05f3fb6b18edbe7252a92985b402`.
It contains 8637 routine resource-count fields, 25,434 VARIABLE rows and
52,498 loop states. The capped old executable never emitted complete MIR,
so no full old/new MIR byte-parity or end-to-end speedup claim is made.

Identities (SHA-256):

- V3 source `fd057b073713dd627f63f252ed6df95f9b442265cc10b251df63903fdb5c32be`;
  executable `bd48c54520c27e6ddae40b5bec4d7dc3a72698c25bea8878c655f58ac0492f0c`.
- V4 source `06591cad940fef88ee3f3745fc86abe0370894126f08685be3c0e95aa8fab2f3`;
  executable `b372db7963b660b585420db5f099133dbc9c3457a22d04bbdaa17877646b2991`.
- Candidate snapshot source
  `2d3e90c4fa5f881f30f13ec3e4d1f15e80b2cb92b9fe25352733a7a369daa394`;
  universe source
  `e812fc10ff3a5e54348a0289fce5430c8ecc5fd2e424b378b7958b30e22ec271`;
  declaration header
  `becff05ae14fb30071f41b17c845240245c747221d67c374a3bd195a7139215f`.
- Candidate executable
  `0daa8b6b9818b9d9d137ca56d53a33127fef811be92b156b0cf673e6c3e60531`.

Scratch sources, overlays, `flow_value_admission_probe.c`, the exact source
fixture, its PowerShell oracle, streaming census, binaries and logs remain
under `.tmp/ownership-cutover-2026-10-09/`. ArrayDrop execution artifacts are
under `.tmp/self_hosted/public-array-drop.tOrq0N`.

### Continuation: independent producers, generation reuse and the C consumer

The importing admission probe now separately reaches the nested-declaration
and function-seal paths with Future type aliases, without a prior branch
snapshot. Both exclude CLASS/TYPE_PARAM while preserving the actual Future
value's stable row. The first v2 probe cleared the context between cases;
its printed "epoch reset" was not evidence of same-context generation reuse.
The separately named v3 probe adds that actual falsifier: retire and begin
the universe on the SAME context, retain the old symbol memo, insert another
value first, then bind the retained value. Its index changes from 0 to 1;
snapshot and sealed parameter identity both agree. All checks PASS. This
does not establish epoch-wrap, arbitrary stale-snapshot or concurrent safety.

V3 identities (SHA-256): source
`9d8b570fe14018260430e96bff22bd3e9bdebafb96ea3073fd46ee55a5dc5a9d`,
overlay `0dbe0d32f22cd3fcced00e1b58cab0a5821e8bdd3f31cce03734141b50ff75fb`,
executable `3fc5364934ebc37a6cca815ef331c2db16f690c1c38e5e685cb3361c925f8357`.
Logs: `flow-value-admission-v3-build.log` and `flow-value-admission-v3.log`
under the scratch directory. The prior v2 source/binary remains preserved.

A fresh importing proof run also PASSes: nine kernel-checked modules,
zero declared abstractions/admits/unsafe kernel features. The first launch
refused because OPAMROOT was absent, before proof compilation. The rerun
uses the existing explicit project prefix
`/home/c/.local/share/pergyra-rocq/opam` with `run_rocq_toolchain.sh`; no system
toolchain fallback or version change. Log: `preflight-resume-kernel-v2.log`.
This is still model integration, not production P1 refinement.

The same candidate compiler subsequently reaches the native C consumer on
the complete `driver_bootstrap_main.pgy` graph. Receipt
`pressure/native-full-driver-flow-values-c-oracle-v1.summary.json` records:

- exit 0 in **104087 ms**; peak OS private **1461.1 MiB (1.427 GiB)**;
  unchanged **3072 MiB** limit, no cap refusal, output capture complete;
- 2538 unchanged file bindings: command, pressure probe, the one additional
  executable binding, and 2535 inputs (all 2531 self-host sources plus the
  two candidate sources, declaration header and Make overlay);
- existing native pipeline -> MIR/AIR validation and region admission ->
  `c_runner_execute` -> `compiler_emit_c`; the emit-only runner checks the
  verified projection-plan artifact identity before reporting success;
- 0 errors, the same 21 existing warnings; generated C **53,340,103 bytes**,
  SHA-256 `5ec64ef8800705f9aae097accfb254860434b8b5311700959099faa5509303b4`;
  path `.tmp/ownership-cutover-2026-10-09/driver-flow-value-native-oracle-v1.c`.

No GCC/LLVM machine-code compilation of that full artifact was run. This
emit-only route starts no detached compiler worker, so the run's root-tree
observation covers the direct compiler; it does not repair the earlier
MSYS build's incomplete worker measurement. No heap-live/cumulative counter,
old/new full-output parity, speedup or final executable behavior is inferred.
The DRV-2 builder explicitly separates native oracle generation from its
Pergyra-seed source compiler. This artifact cannot substitute for a same-source
self-host driver, fixed point, production ownership-clean implementation,
frozen P0 or installed-pair/remote CI evidence.

Continuation checks: documentation-quality and ownership-clean-direction
self-test PASS; the latter also rejects match and tool/I/O failure and
remains structural evidence only. All 27 current text inputs decode as
strict UTF-8; 28 short-status paths include the excluded `gmon.out`, staged 0.
`src`/`.github` diff is still empty. Official compiler/driver hashes remain
`f6559da9...1c9` and `707dcd40...78ec7`. No fresh all-writer freeze confirmation,
production source edit, commit, push, official installation or remote CI
operation occurred in this continuation.

Next boundary: obtain a fresh all-writer stop confirmation, fix the second
checkpoint and record its full P0 (including the existing memory refusal).
Then migrate this demonstrated admission repair to the existing universe
header/source and snapshot owner, put the importing/stable-ID regression in
the existing semantic battery, and rerun the same complete input and reached
native consumer gates on a fresh production-source executable. Do not turn
the scratch candidate into an official installed compiler. Heap-live and
cumulative allocations, P1 physical call/view refinements, complete P0,
P2-P7 and exact-SHA remote CI remain OPEN.

## Reached static obligations

- Ownership-clean direction, documentation quality and gate reachability:
  PASS. Reachability reports 930 scripts, 861 target-reachable, 69 manual,
  zero undeclared/stale/dual entries. These are structural checks.
- Gate single owner and protocol registry: PASS (10 protocol rows).
- Component contract: timeout 124 at its existing 60 s budget. Its initial
  14 checker self-tests passed, but the whole gate is **unverified**.
- Pergyra likeness: FAIL, sentinel 73 versus max 20. The ceiling was not
  loosened and no unrelated source cleanup was opened to hide it.
- CI profile initially rejected five full-only steps because it expected
  four. The workflow already has the fifth native clock/scalar ABI stage.
  GPT changed the consumer to require five and explicitly pin that stage's
  existing two commands; the workflow was not changed. Positive rerun PASS
  in WSL. Three isolated copies of the actual consumer reject an unguarded
  native step, missing clock command and missing widening command, each with
  exit 1 and the intended diagnostic. An earlier MSYS run lacked Git on its
  narrowed PATH; that environment error is not a semantic verdict.
- Executing the two newly pinned commands with the same fresh native
  executable exposed a CRLF/LF oracle mismatch on Windows, both at byte 5.
  The preserved clock diagnostic is exactly `true\r\ntrue\r\n`, verified
  as bytes in `native-clock-diagnostic.out`. GPT normalized only line-final
  CR in those test consumers, retaining raw output. No runtime output or
  compiler semantics changed. Rerun PASS: C/LLVM clock fixture plus LLVM
  `Long` ABI; C/LLVM scalar widening and narrowing/incompatible argument
  refusals. Independent comparator controls still refuse an incorrect
  Boolean and a missing line. Logs: `native-runtime_now_compiler_smoke.log`
  and `native-call_scalar_widening_smoke.log`.
- CI profile consumer SHA-256:
  `6b441dadcfc962ee7b8d827f4a3e16664cdd9539db19201b19c80ed4526f6cf6`.
- Clock consumer SHA-256:
  `cefed4405027a4ff398dc95d42426138982aa7be468c45e4163412a5f0766b57`;
  scalar consumer SHA-256:
  `6ccd604362eb0e4f12800c982db9cef87afdfe3aeb760d79de6b97744e4be69d`.

No remote CI ran. No SoT row or self-host substitution rung was marked
CLOSED. Full P0, production call/view issuers, P2-P7 consumers, DRV-2,
installed-pair validation and final candidate gates remain OPEN.

Follow the active
[`execution directive`](../agent_work_directives/ownership_cutover_execution_2026-10-09.md).
All new evidence stays in `.tmp`; the pre-existing `gmon.out` and shared
changes are preserved. A second reviewed checkpoint must include every
approved gate input but exclude profiling outputs and scratch artifacts.
Final GPT snapshot: 28 short-status paths, staged 0; 27 text inputs decode as
strict UTF-8, `gmon.out` excluded. Production `src` and `.github` have no diff
against the base. Documentation/direction/reachability and diff checks PASS;
this does not turn the listed red/unrun obligations green.
