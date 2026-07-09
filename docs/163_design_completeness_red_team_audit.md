# 163. Design Completeness Red-Team Audit

Status: `red-team-audit`. Written 2026-07-05.

Scope: self-hosted architecture, SoT closure, M2 completeness, runtime
materialization, ABI/backend parity, and proof/documentation alignment. This is
not a new roadmap. It is a check on whether the current design is complete
enough to keep executing without lying to itself.

## Verdict

The design is directionally correct and now much more honest than the earlier
"self-hosted soon" framing. It is not complete.

The strongest part is the contract stack:

- `docs/self_hosted/10_hard_self_host_contract.md` defines hard self-host as
  staged substitution, not "Pergyra files exist".
- `docs/160_m2_completeness_execution_plan.md` defines the M2 completeness
  ledger and explicitly separates stage intersections from true self-parser ->
  self-semantic -> self-codegen bootstrap.
- `docs/self_hosted/15_pre_self_host_expansion_ledger.md` classifies required
  substrate as `READY`, `ACTIVE`, or `HOLD`.
- `docs/162_target4_unified_mir_consumption_blueprint.md` identifies the
  remaining backend risk as duplicate MIR consumption, not merely missing
  output parity.
- `docs/151_generic_axis_composition.md` has a closed generic-axis decision
  table instead of ad hoc generic behavior.

The weakest part is that several gates still prove source shape, vocabulary, or
artifact equality more than they prove implementation ownership. That is
acceptable while rungs are active, but it must stay named as active debt.

## Current Evidence

Observed local ledger state from the latest `.tmp/self_hosted/completeness`
manifests:

| Metric | Current |
|---|---:|
| production self-host sources | 205 |
| lexer pass | 205 |
| parser pass | 205 |
| semantic pass | 205 |
| codegen pass | 205 |
| lex+parse pass | 205 |
| lex+parse+semantic pass | 205 |
| full stage intersection | 205 |

Interpretation: parser is no longer the dominant blocker. The next real
completeness wall is no longer source breadth. The codegen stage check now
consumes AST text emitted by the self-host parser, not C-oracle `pgy --ast`.
The remaining hard-self-host blocker is replacing that AST-text bridge with
typed self-parser/self-semantic owned facts without adding hidden fallbacks.

The monotone ledger is now tightened at `205/205` for lexer, parser, semantic,
codegen, lex+parse, lex+parse+semantic, and full stage intersection. The three
pipeline baselines are emitted from the same source inventory owner rather than
from copied path lists, so new production self-host sources must pass all
selected stages in the same gate.

The 600-line owner cap is currently respected for self-hosted `.pgy` files. The
largest observed files are under the cap:

- `src/self_hosted/codegen/emission/expr_rewrite.pgy`: 564 lines.
- `src/self_hosted/semantic/expr_type_owner.pgy`: 527 lines.
- `src/self_hosted/lib/json.pgy`: 506 lines.
- `src/self_hosted/compiler/world.pgy`: 474 lines.

## Red-Team Findings

### R1. `READY` / `ACTIVE` State Drift

`docs/self_hosted/15_pre_self_host_expansion_ledger.md` is the right abstraction,
but some rows under "Ready Surfaces" contain text saying the surface remains
active until native C/LLVM/self-hosted consumers share the same concrete owner.

Risk: contributors can cite the table placement as `READY` while ignoring the
row caveat that the global surface is still `ACTIVE`.

Required fix: split rows into two states when needed:

- `READY for current self-host C subset`.
- `ACTIVE for global C/LLVM/self-host consumption`.

Do not let a row be both by prose.

### R2. Compiler World Is Structurally Present, Not Fully Semantic Yet

`PgyCompilerWorld`, zones, and `CompilePergyraProgram` are now visible, and the
contract gate enforces their presence. That is necessary. It is not sufficient.

Current risk: a gate can prove the compiler has a world-shaped manifest without
proving every stage action consumes the relevant zone facts instead of local
string/path/index assumptions.

Required fix: promote critical `Compiler*Ready()` checks from vocabulary
readiness to live consumption evidence. For each stage intent, require at least
one negative fixture where removing the owner fact fails the stage.

### R3. Typed AST Bridge Is Still A Bridge

The current `CodegenAstTextNode` bridge is much better than raw line scanning:
it has parent edges, kind rows, name/type/mode facts, and statement accessors.
But the mixed tree blocker remains active because line text still exists as the
transitional payload.

Risk: the bridge can become a second AST representation with text hidden behind
owner APIs.

Required fix: close this in one direction only: typed/tagged arena facts replace
line-text semantics. Do not add more line-text fact accessors unless they are
provenance-only.

### R4. JSON Fact Ownership Is Bounded, Not General

The shared JSON owners have removed many local scanners, but the ledger is
honest: this is still bounded scan/fact-table behavior, not a complete shared
JSON DOM or schema-aware fact graph.

Risk: new tools may reintroduce local `StringIndexOf` schema recovery because
the shared owner does not expose the exact nested fact they need.

Required fix: when a new tool needs a JSON field, add the fact to the shared
owner or reject the tool. Do not let document-local scanners count as hard
self-host progress.

### R5. C Oracle Is Necessary But Dangerous

C remains the primary oracle during substitution. That is pragmatic and correct.
The danger is copying C's silent fallback behavior.

Required fix: every pass-count increase must come with one of:

- C/LLVM parity on a fixture where both are known-good.
- a negative fixture proving missing facts fail closed.
- a schema/golden comparison that cannot be satisfied by prose text matching.

C-only acceptance is not enough evidence when the C path has known compatibility
fallbacks.

### R6. Runtime Materialization Needs Per-Program Accountability

The current position is correct: Pergyra does not need universal zero-cost
erasure. It needs no hidden runtime materialization. The design already says
retain/materialize must be evidence-owned.

Risk: a runtime call can still be viewed as "implementation detail" unless the
artifact records which World/Zone/Intent/Slot fact retained it.

Required fix: runtime-call scans should report forbidden calls in erase
fixtures, and retained calls must cite a boundary/materialization fact. This is
stronger than "zero runtime" and matches the language's evidence-first identity.

### R7. Target #4 Is The Real Backend Architecture Gap

Behavioral C/LLVM parity is necessary but not sufficient. The dominant remaining
backend design risk is duplicate MIR consumption: C and LLVM can read the same
MIR facts through separate readers and drift later.

Required fix: execute `docs/162_target4_unified_mir_consumption_blueprint.md` as
a structural refactor track after the M2-critical semantic/codegen ledger stops
moving. Do not mistake backend compare green for shared fact consumption.

### R8. Source Identity Is Not Always A Stage Check Unit

The completeness ledger is right to count every production self-host source.
But a source file is not always a valid standalone semantic-check unit. Parser
expression parsing, parser statement parsing, and codegen expression rewriting
are mutually recursive owner clusters. Their split files are source identities,
but their semantic truth is the owning cluster boundary.

Risk: forcing every split participant to check standalone pushes the codebase
toward circular imports, duplicate stubs, or fake compatibility wrappers. That
would make the ledger greener while making the architecture less true.

Required fix: keep source identity at 155, but let
`CompilerCompletenessLedger` emit the stage-specific semantic check target for
internal participants. Shell runners may execute that mapping, but must not own
it. A source passes semantic completeness only when its declared owner check
target passes, and baseline identity remains the original source path.

### R9. Import Expansion Must Not Recheck The World

The semantic completeness check used to expand transitive imports as full source
text. That made large driver roots recheck parser and codegen bodies inside the
same source identity, creating timeout-shaped failures and hiding the actual
M2 signal.

Risk: the ledger can spend most of its time proving imported bodies that already
have their own source identity. Worse, timeout handling previously lost the
`timeout` exit code and counted those infrastructure failures as normal semantic
failures.

Required fix: `--check` source bundles should seed imported signatures and
nominal facts, then body-check only the root source identity. Timeout exit codes
must remain fatal infrastructure failures, not pass-count failures.

## Completeness Grade

| Area | Grade | Red-team note |
|---|---|---|
| Self-host contract | Strong | honest definition, bridge/fallback split, CI owner named |
| M2 completeness measurement | Medium-high | monotone source identity is right; stage check-unit ownership must stay explicit |
| Parser self-host | Strong | 205/205 in completeness; parity fixture surface broad |
| Semantic self-host | Medium | 205/205 source-stage checks pass, but deep semantic parity is still bounded by the current subset |
| Codegen self-host | Medium | fixed-point exists for subset; 205/205 source-stage checks pass via self-parser AST text |
| CompilerWorld shape | Medium | vocabulary present; live fact consumption still needs negative evidence |
| Typed AST | Medium-low | bridge is controlled, but still line-text backed |
| JSON facts | Medium | shared owners exist; not yet a full schema/fact substrate |
| ABI/layout policy | Medium | policy is sharp; global consumers not all unified |
| C/LLVM backend architecture | Medium-low | output parity is stronger than before; duplicate MIR readers remain |
| Runtime materialization policy | Medium | philosophy is right; retained-call attribution must become mechanical |

Overall: the design is executable, but not finished. It is mature enough to
continue M2 hard self-host work if each change raises the completeness ledger or
closes one named `ACTIVE` blocker. It is not mature enough to declare self-hosted
or backend architecture complete.

## Next Work Order

1. Replace the codegen stage's AST text bridge with typed self-parser /
   self-semantic owned facts, then keep the 205/205 ledger green.
2. Keep applying the `READY for subset` / `ACTIVE global` split whenever a
   ledger surface mixes hard-rung readiness with native/global completion.
   `Target capability envelope` is the first gated split.
3. Start typed AST replacement at the top codegen consumers:
   `expr_rewrite`, `decl_lower`, and `routine_lower`.
4. Extend retained-runtime attribution from fixture-level A/B/C and floor-excess
   symbols toward finer callsite/materialization-owner rows where the physical
   oracle can distinguish them.
5. Execute Target #4 shared MIR consumption only after the M2 semantic/codegen
   ledger stops yielding cheap wins.

## Non-Goals

- Do not rewrite the runtime in Pergyra as a self-host prerequisite.
- Do not claim universal zero-cost erasure.
- Do not broaden CI locally for isolated owner edits.
- Do not add another generic helper layer to make the architecture look cleaner.
- Do not treat docs/proofs as closure when implementation facts still route
  through compatibility or bridge paths.
