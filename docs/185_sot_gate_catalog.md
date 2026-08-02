# SoT Gate Catalog

Status: `ACTIVE`

This document is the architectural index for source-of-truth gates. It does
not own compiler facts or executable gate rows. Fact authority remains in
`docs/semantics/sot_owner_spine_registry.md`; executable dashboard identity,
tier, budget, and Make targets live in the **single Gate SoT**
`src/self_hosted/compiler/gate_dashboard_owner.pgy`.

## 0. Single Gate SoT rule

`src/self_hosted/compiler/gate_dashboard_owner.pgy` is the only authority for
gate identity, Make target, tier, budget, declared state, blocking policy, and
owner-fact binding. Every other gate-related file is one of:

- a validator (`scripts/sot_registry_gate.py`,
  `scripts/protocol_registry_gate.py`);
- an execution consumer (`Makefile`, shell smoke/parity scripts);
- a result or golden projection (`src/self_hosted/tools/gate_dashboard/`);
- an architectural explanation (this document and related docs).

The protocol crosswalk and its single-owner check run as subchecks of
`sot-authority-edge-test-smoke`; they must not become additional dashboard gate
IDs or a second gate manifest. A copied gate list, status count, tier, budget,
or current health summary is a Gate SoT violation.

## 1. Objective Card

- Objective: prevent a second producer, undeclared fact carrier, fallback
  reconstruction, or stale cache from becoming semantic authority.
- Priority: semantic identity, one owner, declared projections, fallback
  removal, executable negative evidence, then patch size.
- Fact owner: the owner registry.
- Last legitimate consumers: the paths declared by each registry row.
- Forbidden fallback: AST/program-root re-scan, source-text/JSON semantic
  reconstruction, backend-local type/layout guesses, and cache-only answers.
- Verification: static authority edges first, then the active rung's executable
  missing-fact and C/LLVM/self-host parity gate.

## 2. Canonical Inputs

| Input | Role | Authority status |
|---|---|---|
| `docs/semantics/sot_owner_spine_registry.md` | owner rows and derived fact-carrier classification | canonical |
| `docs/semantics/proofs/SoTAuthority.v` | formal projection of registry owner/fact pairs | checked projection |
| `scripts/sot_registry_gate.py` | generic registry and relation validator | consumer |
| Per-rung parity scripts | executable missing/corrupt fact evidence | consumer |
| `src/self_hosted/compiler/gate_dashboard_owner.pgy` | active gate identity, tier, budget, and target rows | operational owner |
| `src/self_hosted/tools/gate_dashboard/main.pgy` | declared-state plus observed-result dashboard | consumer |

The gate must not carry a copied owner list or copied status count. The Coq
mapping, registry summary, producer definitions, and self-host fact-owner file
coverage are compared to the registry at execution time.

## 3. Gate Inventory

| Gate | Cost budget | State | What failure means |
|---|---:|---|---|
| `make sot-authority-edge-test-smoke` | 60 s | LANDED | duplicate producer, unclassified fact owner, stale derived row, registry/Coq drift, single-Gate-SoT drift, protocol-crosswalk drift, forbidden layer input, or a CLOSED consumer reopened a named fallback |
| `make sot-authority-adequacy-test-smoke` | 60 s | LANDED, bounded | current typed-expression owner/source bindings or their negative source mutations drifted; this is not whole-compiler extraction evidence |
| `make self-host-codegen-assignment-projection-parity-test-smoke` | 5 min | LANDED, focused | semantic assignment target/expected type is missing, guessed, or differs across C/LLVM projection |
| `make self-host-initializer-projection-parity-test-smoke` | 5 min | LANDED, focused | semantic initializer row/type is missing, a graph-owned concrete scalar tree or resolved direct/namespace/receiver target is recovered from source text, or C/LLVM MIR projection differs |
| `make self-host-generic-return-parity-test-smoke` | 5 min | LANDED, focused | typed formal-generic or signature type-expression rows are missing, ordered explicit actuals are dropped or rebuilt from compact text, exact/nested parameter or return binding falls back to text, explicit/inferred conflict or structural mismatch is accepted, carried target mutation is ignored, or C/LLVM verdicts differ |
| `make self-host-one-mir-option-struct-value-flow-projection-test-smoke` | 5 min | LANDED, focused | an Option-of-nominal return/local loses its outer or inner ABI receipt, tag/payload geometry is reconstructed, the latest Option value/call/unwrap/member path is flattened or stale, nominal routing retries the plain plan, or C/LLVM exact output differs |
| `make self-host-gate-dashboard-parity-test-smoke` | 5 min | LANDED | Pergyra manifest/JSON golden drift, an unknown/duplicate result is accepted, or C/LLVM dashboard projection differs |
| `sot-missing-fact executable matrix` | 5 min per active rung | PARTIAL | a last consumer accepted a missing/corrupt canonical fact or emitted output through fallback |
| `layer-input capability checks in authority-edge gate` | 60 s | LANDED | self-host codegen re-produced semantic facts/imported parser owners, native backend read AIR/raw AST at its public boundary, or the one declared AST-text bridge drifted |
| `cache-shadow authority gate` | 5 min focused | NEXT | cache-on/off differs, stale owner revision hits, or a cache answers without canonical owner evidence |
| `CLOSED promotion gate` | 60 s + focused negative | PARTIAL | a row was promoted without old-path deletion, declared projection, fail-closed missing-fact behavior, and negative evidence |

## 4. Authority-Edge Rules

The static gate models these relations:

```text
owns(owner, fact)
writes(producer, fact)
reads(consumer, fact)
projects(derived_fact, source_fact)
caches(cache_fact, source_fact)
bridges(temporary_fact, source_fact)
reconstructs(consumer, source_carrier)
```

Required invariants:

1. A fact has exactly one authority root. One authority may expose several
   owner-local write operations; this is not multiple authority.
2. Every `*_fact_owner.pgy` is either an authority path or a classified
   `projection`, `cache`, `bridge`, or `local_view`.
3. A projection may copy representation but may not make a new semantic
   decision.
4. A `CLOSED` consumer may not contain a named reconstruction fallback.
5. Missing owner facts fail closed at the actual last consumer. A grep mutation
   alone cannot promote a row to production-grade closure.
6. Cache facts carry owner revision or equivalent freshness identity and are
   reproducible with the cache disabled.

## 5. Gate Placement

- Static authority-edge and bounded adequacy gates run in
  `self-host-preparation-contract-test-smoke`.
- Executable missing-fact and backend parity gates run only for the active
  substitution rung in the parity lane.
- Full C/LLVM/self-host matrices run at scheduled or merge boundaries.
- A new fact family must update the registry before its producer can land.
  Adding another path-specific grep script is not an acceptable substitute.

## 6. Closure Evidence

`CLOSED` is bounded to one registry row. Promotion requires all of the
following:

```text
owner identity fixed
all fact carriers classified
old producer/read path deleted
missing fact rejected by the real consumer
stable diagnostic or verifier failure observed
named fallback absent
negative gate prevents reintroduction
```

The current authority-row count and status are owned by
`docs/semantics/sot_owner_spine_registry.md` and validated by
`scripts/sot_registry_gate.py`. This catalog deliberately does not repeat the
numbers; historical counts in older completion logs are not current gate
state.

`selfhost.assignment_type_verdict` reached `CLOSED` on 2026-07-15. The
semantic body-type bundle is produced once per driver path, transferred through
an explicit `own` boundary, and projected by codegen without semantic
reconstruction. The focused probe is positive-output equal under C/LLVM and
rejects both a missing assignment expected type and a missing indexed-target
type. It also rejects the former source-expression and backend-environment type
guess patterns before compiling the probe.

`selfhost.initializer_type_verdict` reached `CLOSED` on 2026-07-15. Semantic
downstream producers and MIR now validate the projection shape without
re-running initializer inference. MIR routine input carries NodeId/type rows,
and an unannotated local consumes the inferred type while an annotated local
retains its declared storage type. C and LLVM probes emit the same MIR local;
removing either the row or inferred type fails at the MIR projection boundary.
The same gate preserves the scalar source/root text while corrupting one graph
leaf; both backends reject `undefined_symbol`. This closes result-type
reconstruction only for the declared scalar-operator capability subset. A
String-leaf mutation is also rejected as `binop_type_mismatch` from graph child
types. Concrete scalar returns from direct named calls now consume the graph
callee plus the canonical callable return table. A negative keeps source/root
text unchanged, changes only the callee leaf from an `Int` function to a
`String` function, and both backends reject the initializer as
`let_type_mismatch`. Direct calls whose return and parameter rows are concrete
scalars also consume graph-owned scalar argument trees, arity, and argument
types.
Changing only the graph argument from `Int` to `String` is rejected as
`call_arg_type_mismatch`. `ToIntValue(1 + (2 * 3))` is positive-output equal
under C/LLVM, and changing only the nested graph leaf to a String is rejected
as `binop_type_mismatch`. Parser-canonical root spelling is verified by the same
compact parser owner. Concrete nested direct calls recurse through graph call
spines: `ToIntValue(ToIntValue(2))` is positive-output equal under C/LLVM, and
changing only the inner graph callee to a String-returning function is rejected
at the outer call as `call_arg_type_mismatch`. One concrete scalar graph owner
now composes scalar operators and direct calls. `1 + ToIntValue(2)` is
positive-output equal under C/LLVM, and changing only its graph callee to a
String-returning function is rejected as `binop_type_mismatch`. The retired
direct-call-only verdict owner and names are gate-forbidden. Receiver-bound
member calls, generic calls, wrapper/collection policies, and aggregate
signatures remained bridge consumers at this rung; the later entries below
record their bounded migrations. This does not close the full expression
surface.

Namespace-qualified static calls consume the call target already carried by
`SemanticExpressionCallTargetFact`. `Math.Add(2)` resolves through `Math_Add`
under C/LLVM parity. A source-preserving mutation that changes only the carried
target to the String-returning `ToTextValue` is rejected as
`let_type_mismatch`. The direct-leaf-only call type owner is deleted and
gate-forbidden. Receiver-bound member calls, generic calls,
wrapper/collection policies, and aggregate signatures remained bridge
consumers at this rung; later entries record their bounded migrations.

The next bounded receiver-member slice carries the same target through self
MIR and hard codegen. `box.Get()` stores `member/Box_Get`, hard codegen emits
`Box_Get(box)`, and a missing carried row fails before emission under C and
LLVM. Compact graph construction threads row arrays; the focused gate rejects
an `inout SemanticExpressionGraphArena` or graph-fact aggregate because that
shape produced an LLVM-only crash. Generic receiver locals consume a typed
canonical type-name fact: `Box<Int>.Count()` carries `member/Box_Count`
through MIR and emits `Box_Count(box)`. Removing only that row fails under C
and LLVM. Chained field receivers consume graph handles plus nominal field
facts: `holder.box.Count()` carries the same target and emits
`Box_Count(holder.box)` under both backends. The gate rejects dotted source or
codegen field-type recovery in this owner. Direct calls now carry the `direct`
target kind and canonical name through semantic analysis and self MIR. Hard
codegen consumes only that row; removing it fails before emission under C and
LLVM, and the gate rejects callee-text identity recovery in the emitter. The
bounded `selfhost.call_target_identity` family is therefore closed. This does
not close generic substitution, composite aggregate validation, or the broader
expression surface.

Nominal aggregate call returns now consume the closed target row and canonical
signature return fact. `MakeBox() -> Box` reaches self MIR and hard codegen
under C/LLVM parity; changing only the carried target to a String-returning
function is rejected as `let_type_mismatch`. Generic substitution and
composite aggregate validation remain bridge work. The focused gate,
`make self-host-generic-return-parity-test-smoke`, proves exact and nested
parameter binding plus exact and nested return substitution. It requires a typed
generic-parameter HIR row, a signature-owned parameter/return type-expression arena,
graph-owned actual argument types, and a source-preserving carried-target
mismatch under C/LLVM. It rejects an `ExprType` fallback in the generic-call
owner. Ordered explicit actual carriage is also graph-owned and conflict-gated.

Scalar Option/Result builtins now have a separate graph-policy owner. Initial
call-target capture includes canonical builtin signatures before initializer
typing, and the final expression verdict consumes the carried direct target,
the builtin signature projection, and graph argument handles. The focused
`make self-host-wrapper-policy-parity-test-smoke` gate accepts `Some`,
`UnwrapOption`, `Ok`, `UnwrapOr`, `IsSome`, and `IsOk`, rejects non-concrete
`None` and non-wrapper arguments with the native C-oracle diagnostic class,
and rejects a source-preserving carried-target mutation. The owner is forbidden
from calling `ExprType` or `CheckCall`. Collection result/element typing,
unknown or aggregate wrapper payloads outside the covered graph capability,
and composite aggregate validation remain bridge work.

Caller-visible collection mutation admission now has one canonical policy
owner. Specialized `ArrayPush`/`ArraySet`/`ArrayPop` statement facts consume it
directly, while general mutator calls consume the carried call target and graph
receiver node. The graph call checker is explicitly barred from replaying the
source receiver policy. The focused
`make self-host-collection-policy-parity-test-smoke` gate accepts local and
`inout` mutation, rejects a value-parameter mutation with the native C-oracle
diagnostic class, and rejects carried-target drift under C/LLVM-built probes.
Collection result/element typing and unknown or aggregate wrapper payloads
remain bridge work.

Aggregate field validation now consumes graph-owned value types and structural
assignability. The struct verdict no longer calls source `ExprType` or
`ExpressionAssignableTo`; exact types, `None`/`Err` wrapper unknowns, scalar
operators, direct nominal returns, nested struct values, `Some(struct)`, and
structural `Int`-literal-to-`Long` widening are covered. The focused
`make self-host-aggregate-field-policy-parity-test-smoke` gate compares native
C with C/LLVM-built probes and injects source-preserving leaf type drift plus a
missing child fact. Generic/member aggregate field values remain bridge work.
The adjacent hard-driver check builds rung 2 with both C and LLVM, runs its 20
body fixtures, and filters MIR parity to `option_struct_value_flow`; it is
bounded evidence, not a claim about the other 27 MIR fixtures.

`make self-host-gate-dashboard` executes only the selected Pergyra-owned tier
(`static` by default) and records a separate result artifact. `NOT_RUN` is not
green, unknown or duplicate result IDs fail closed, and an over-budget run is
reported independently from semantic gate state. The process bridge consumes
each manifest budget through the repository's portable timeout owner, so the
budget also stops the gate instead of merely annotating it afterward.
Dashboard code, tests, and LOC remain supporting evidence and do not increase
substitution progress.

## 6b. Parallel-Track SoT Rows (2026-07-17, docs/188 R2/R8/R9)

- **Chunk policy owner**: `src/self_hosted/parallel/chunk_policy_owner.pgy`
  is the auto-chunk policy SoT; C runtime (`pgy_parallel_chunk.h`), exports,
  and both emitters are projections. Gate:
  `tests/selfhost_parallel_chunk_policy_smoke.sh` (comparator-backed golden
  + C==LLVM legs + stable-identifier pins parsed from the compiled
  manifest). Pin doctrine: stable identifiers only — arithmetic spellings
  are not pinned (the 2026-07-17 relocation/rename false-positive is the
  origin story); semantic equivalence is the executable golden's job.
- **Measurement ledger canon**: `benchmarks/PARALLEL_RESULTS.md` is the
  canonical parallel-performance ledger; `BN_RESULTS.md`, docs/186 progress
  notes, the TODO board, and session memory are derived narrations. On
  remeasure, update the canon first and let the derived copies cite it.
- **Golden-refresh rule**: a commit that refreshes any expected/golden
  artifact must state in its message what changed upstream and why the
  golden follows — a silent refresh turns a drift detector off.
- **Parallel production aggregate**:
  `make parallel-production-contract-test-smoke` runs the nested-parallel
  witness, worker-invariance (+ `PGY_WORKERS` warn pin), channel-pool
  starvation, chunk-policy owner, budget chunk-charge, and join emit-shape
  gates. It is wired once in Linux CI, where both C and LLVM voices are
  available, instead of multiplying the same integration cost across all
  platform jobs.

## 6c. docs/189 Repair Gates (2026-07-18, C-compiler-dev red team)

Standalone gates from the repair campaign. They are wired through the
Makefile and run as the `redteam-repair-contract-test-smoke` cluster:

- `tests/runtime_bc_contract_smoke.sh` — bitcode-twin contract pins:
  `.bc` build mirrors `-fwrapv`/`-fno-strict-aliasing`, strip predicates
  cover the stateful/panic-carrying families, the exclusion loop enforces
  external linkage, freshness is a directory scan, and the checked
  float->int twins exist in both runtime homes (docs/189 C4-C7).
- `tests/surface_boundary_hygiene_smoke.sh` — C-reserved-word rejection
  for function/parameter names (SoT twin:
  `symbol_table_owner.pgy` CompilerSymbolCReservedWord) and the Channel
  copy-edge rules (let-copy/mut/return fail closed; constructor-born
  immutable lets and SSA-renamed `let double` stay legal) (C11+C12).
- `tests/adversarial_input_smoke.sh` — termination-contract exercise:
  deep nesting (parser 400 cap), operator bomb (4096 cap), 1MB token,
  garbage bytes, and a 10MB valid source, all under hard timeouts with
  hang/crash/accept outcomes distinguished (C8/C10/C14). Complements
  `tests/semantic_termination_security_smoke.sh` (step budget + NUL),
  which landed with the CI-repair track.
- `tests/emitted_c_warning_clean_smoke.sh` — emitted C for a 15-fixture
  representative slice must compile warning-clean at
  `-Wall -Wextra -Werror` (suppressions only for the structural
  single-TU inline-runtime noise) (C14).
- Panic-class gate extension: `tests/runtime_panic_codegen_smoke.sh`
  now also pins `float_to_int_oob` / `float_to_long_oob` /
  `float_nan_to_int` on both backends (C1); the Coq side is
  `CheckedArith.v` `checked_f2i` (`f2i_none_iff` / `f2i_some_exact` /
  `f2i_some_representable` / `f2i_total`, machine-checked).

Campaign-close status (2026-07-18):

- ✅ **Blocking-pool compensation mis-attribution** (docs/189 C13-④,
  commit `25fd43fa`): workers stamp a thread-local `g_pgy_thread_pool`;
  the pool struct carries its own lifecycle wiring; the tick compensates
  the resolved pool. NULL wiring refuses rather than guessing.
- ✅ **Real compile-speed contract** (C14-④, commit `44f99988`):
  `perf_contract_smoke.sh` gains a measured end-to-end C compile of a
  60-statement fixture (catastrophic-only 60s ceiling,
  `PGY_COMPILE_SPEED_CEILING_MS` overrides). The synthetic parser-unit
  block stays as the summary-format + fail-open guard.
- ✅ **Gate wiring** (commit `ee8fb209`): the four gates get Makefile
  targets + a `redteam-repair-contract-test-smoke` aggregate, wired into
  Linux CI; the differential-fuzz oracle (`fuzz-backend-parity-matrix`
  fixed seeds + `fuzz-backend-parity-campaign` env-seed rotated by
  `GITHUB_RUN_NUMBER`) is now gated, closing the "free oracle only ran on
  a fixed corpus" gap.
- ✅ **Semantic flow-universe pointer lifetime**: stable resource-flow
  identity owns an index plus copied declaration metadata, never a
  scope-owned `Symbol *`. `dir-resource-flow-identity-test-smoke` forbids
  the old pointer cache and requires the nested-block regression; the
  semantic ASan/UBSan battery executes the block-teardown UAF shape.
- ✅ **ASan and `.bc`-on Linux CI** (commit `72421388`, strengthened by
  `sanitizers-linux`): the sanitizer unit battery and a runtime-bitcode-enabled
  backend comparison are explicit Linux steps. The separate bounded sanitizer
  job first runs an intentional heap-UAF calibration witness, so missing or
  ineffective sanitizer support fails rather than producing false confidence.
- ✅ **Adversarial memory corpus**: `pgy.memory-adversarial.v1` inventories
  source rejection, MIR/runtime guards, sanitizer witnesses, and named open
  residues. `memory-adversarial-catalog-test-smoke` forbids undefined C
  execution from becoming a semantic oracle and rejects closure rows without a
  live fixture and Makefile gate.

Genuine residues (workstream-scale or runner-gated, not forgotten):

- **C-backend runtime prelink** (C14-③): the emitted C `#include`s the
  runtime's static-inline bodies, so an object cache alone cannot skip
  the 14k-line recompile — it needs an inline→extern ABI restructuring
  of the runtime (with the twin discipline and strip list following). A
  workstream, not a patch.
## 7. Next Execution Order

1. Keep the mixed expression bridge as the active executable rung. Concrete
   scalar trees composed from operators and resolved direct, namespace, and
   bounded, generic-local, and chained-field receiver calls are graph-owned and
   their target reaches MIR and hard codegen. Nominal aggregate returns are
   carried; scalar wrapper policy and collection-mutation admission are also
   graph/owner-directed. Extend composite consumers one at a time.
   Do not let an unsupported tree
   silently fall through a typed/text dual read.
2. Extend the layer-input capability inventory as native AST/HIR bridges reach
   the active executable rung; do not add global zero claims early.
3. Add cache-disabled and stale-revision mutation cases to compiler-scale
   semantic memo and future incremental compilation caches.
4. Migrate existing CLOSED rows from source-copy mutation evidence to actual
   executable missing-fact fixtures as each row becomes the active rung.
