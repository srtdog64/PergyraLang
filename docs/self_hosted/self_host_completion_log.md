# Self-Host Completion Log

A session-by-session record of the process of completing self-hosting. The
verified *snapshot* lives in [`09_selfhost_status.md`](09_selfhost_status.md);
this file is the *journal* -- what was attempted each session, what landed, and
what the next session should pick up. Append a new entry per session; do not
rewrite history.

## 2026-07-19 - Scalar match enters the hard MIR producer frontier

- Added sparse instruction-keyed `SelfMirMatchFactRows` for pattern, variant,
  and binding identity. The active producer writes one scalar pattern per case;
  JSON projection and validation consume the row and never recover it from
  source or AST provenance text.
- `routine_match_owner.pgy` now emits the native case/default CFG shape from
  semantic statement facts. It deliberately rejects arm-local version drift
  until an N-way phi owner exists instead of selecting an SSA version by
  fallback.
- The MIR-to-artifact consumer derives each reconstructed equality graph from
  the carried subject graph plus scalar pattern fact. This closes the graph
  loss exposed by canonical MIR comparison.
- Added `match_case_int` as DRV-2 fixture 38. Focused C-built and LLVM-built
  drivers matched canonical MIR, emitted C, and runtime output. Removing the
  first match pattern fails in the final MIR consumer; a verifier mutation also
  rejects an `AST_MATCH_CASE` instruction without its match row.
- The producer creates the merge block after every arm so flat instruction
  ranges remain contiguous. The fixture executes case 1/2/3 and default and
  requires the post-match statement after every path, preventing an arm block
  from accidentally becoming the continuation owner.
- Added `match_case_assign` as fixture 39. `routine_match_merge_owner.pgy`
  consumes every live arm exit/version row and emits one N-way phi instead of
  selecting the entry or final arm version. Focused C/LLVM producers matched
  canonical MIR and returned 10/20/30/40 across all four paths.
- `phi_fact_owner.pgy` closes the final-consumer side: phi use arity must equal
  indexed CFG predecessor arity and every use/result must retain one canonical
  local identity. Deleting one match-arm input now fails before reconstruction.
- This is bounded integer single-pattern/default substitution. Variant and
  binding facts and released compiler replacement remain open.

## 2026-07-19 - Array returns consume the parser expression graph

- `SemanticAstStatementTypeFacts` now resolves array-literal return values from
  the parser-owned expression graph. A missing return graph produces a
  structural diagnostic instead of reparsing the return text.
- The last external consumer of
  `SemanticProjectionArrayLiteralMatchesDeclaredType` was removed, then the
  dead recursive text parser itself was deleted. The component contract rejects
  its return in the projection owner and in initializer, assignment, and return
  consumers.
- Added `array_return_literal` as DRV-2 MIR fixture 37. C-built and LLVM-built
  self-host drivers produced equal canonical MIR, equal emitted C, and the same
  runtime output as the native oracle. The dedicated graph gate pins the return
  instruction, array root, final element edge, and typed C array construction.
- This closes the semantic array-literal text-projection seam across
  initializer, assignment, and return lanes. It does not close arbitrary
  expression typing, the integrated producer, or released compiler replacement.

## 2026-07-19 - Assignment array literals consume graph and expected-type facts

- `SemanticAstAssignmentTypeFacts` now checks array literals through the
  parser-owned expression graph. The old text projection is statically
  rejected in this owner; statement-return array inference remains a separate
  bridge.
- The first focused run then failed closed because `EmitAssign` received the
  semantic expected type but did not use it for `Array<T>`. Assignment now uses
  the same expected-type graph emitter already used by let and return paths.
- Added `array_literal_assignment` as DRV-2 MIR fixture 36. C-built and
  LLVM-built self-host drivers produced matching canonical MIR and emitted C,
  and the generated program matched the native oracle output. Missing target
  and value expression graphs remain rejected by the assignment graph gate.
- This is one executable source-to-MIR-to-C replacement delta. It does not
  close all collection typing, the statement-return projection bridge, or the
  released compiler path.

## 2026-07-19 - Full self-source routine lowering is memory-bounded

- The historical approximately 68 GB private-allocation probe was a real
  amplification failure, not a normal compiler footprint. A bounded LLVM
  self-source probe now lowers all 1,816 routines with zero errors in 62.085
  seconds at 794.4 MB peak private memory and 728.5 MB peak working set. The
  3 GB fail-closed memory cap was not reached.
- LLVM parameter emission now registers every formal parameter's canonical
  `<name>.0` SSA identity, and block-entry scope seeding consumes both MIR
  `ssa_entry_values` and `live_in_names`. This removes parameter lookup loss and
  sibling-branch scope leakage rather than restoring types from source AST.
- `SelfMirLowerIfFromArtifact` delegates post-branch merge work to a dedicated
  owner, and the self-host codegen statement dispatcher uses flat guarded
  cases. The full probe no longer exhausts the Windows 2 MB stack on the
  dispatcher's previous deep `else if` tree. General arbitrary-depth source
  nesting remains a separate compiler-depth contract; it is not claimed closed.
- The focused aggregate-parameter loop/phi C/LLVM backend comparison passes.
  A freshly built C and LLVM self-host codegen tool emitted byte-identical C for
  the `else_if_chain` fixture, which compiled and ran with the committed output.
  The full codegen parity matrix was stopped at its five-minute focused budget,
  so no broader parity claim is made here.
- `self_host_pergyra_likeness_smoke.sh`,
  `self_hosted_component_contract_smoke.sh`,
  `self_host_hard_contract_smoke.sh`, and the production owner-size gate pass.
  This closes one executable routine-lowering resource blocker, not released
  compiler substitution or the whole bootstrap loop.

## 2026-07-18 - Codegen fixed point stops rebuilding kind tables

- The exact Pergyra-built `mir_lower` input previously crossed 25.6 GB private
  memory before termination. Allocation tracing found that the expression-graph
  kind predicate rebuilt `TypedAstKindTags()` for every query: the same growth
  sites reconstructed the 37-tag array 202,626 times.
- `TypedAstKindOwnerReady()` now proves that the canonical tags form one
  contiguous interval. The hot expression-graph consumer uses the owner's
  bounded `TypedAstKindKnown()` predicate instead of allocating and scanning a
  fresh array. Type-row lookup and bounded substring search also avoid temporary
  needle strings on this compiler-scale path.
- The same full `mir_lower` input completed in 68.5 seconds at 976.7 MB peak
  private memory. Codegen bootstrap reached `gen2 == gen3` at 32,805 generated-C
  lines and completed `SELF-HOSTING OK`; default extern-runtime C codegen was
  run-equal on 75 fixtures.
- Extern-runtime generic families now remain program-local because their
  `CType`/`ErrType` can exist only in the generated translation unit. A static
  runtime-cext contract and the executable codegen parity prevent these bodies
  from being silently moved into the shared runtime object again.
- This closes a codegen bootstrap resource blocker and one executable codegen
  rung. It does not claim whole-compiler self-hosting or native driver
  substitution.

## 2026-07-17 - Driver fixed point is split by validation budget

- The native-oracle MIR boundary removed the previous 68 GB full-source
  producer growth, but the blocking Linux bootstrap still exceeded its
  40-minute job budget on the full stage2/stage3 chain. A local Windows
  full-stage2 consumer also exceeded the 30-minute integration budget while
  staying below 101 MB private memory.
- `self-host-driver-bootstrap-test-smoke` now compares the Pergyra-built
  integrated seed with the native-built same driver on a real source and a
  TestHarness-owned sample. Both builds produce byte-identical verified MIR
  for that sample and consume the common fact to byte-identical C. This avoids
  treating native `pgy --mir-json` output without expression-graph facts as a
  valid DRV-2 input.
  `self-host-driver-bootstrap-full-test-smoke` explicitly requests the
  full-input stage2 plus `gen2 == gen3` comparison.
- The driver target consumes a seed-only codegen profile (`gen2` and parser AST
  producer). Standalone codegen fixed-point and breadth coverage remains a
  separate 30-minute Linux job instead of delaying the driver boundary.
- This is scope isolation, not a timeout increase or a semantic exemption.
  Bounded DRV-2 producer parity and the real-source oracle comparison remain
  blocking.

## 2026-07-17 - Driver fixed point consumes one oracle MIR fact

- Added explicit bootstrap file modes for verified source-to-MIR production
  and MIR-to-C consumption while preserving the real-source source-to-C oracle
  comparison.
- A local full-driver self-host source-to-MIR probe exceeded 68 GB private
  allocation before completion. The fixed-point runner now obtains each
  full-input MIR once from the native oracle, then seed and gen2 consume the
  same artifact before the byte-identical `gen2 == gen3` comparison.
- Bounded DRV-2 fixtures remain the blocking self-host MIR-producer proof. The
  full-input gate proves MIR consumption without hiding the producer memory
  blocker or claiming whole-compiler substitution.

## 2026-07-16 - Completeness checks stop rebuilding every import graph

- Split the self-host parser's normal root-program mode from its inventory
  source-unit mode. Normal parser and driver consumers still expand the full
  import graph; `--check` validates one owner file after consuming import syntax.
- Added a source-unit AST projection for the codegen completeness stage. The
  ledger no longer performs O(source count x transitive import graph) work while
  proving per-owner parser and codegen acceptance.
- The C-built parser remained byte-equal on all 188 parser fixtures. The focused
  three-source CI reproducer passed parser and codegen for the previous `Die`
  and timeout targets, and the full isolated ledger recorded parser 342/342 and
  codegen 341/342. The one codegen rejection is the committed unsupported-event
  negative fixture, not a timeout or skip.
- This does not weaken whole-program evidence: parser parity and the driver and
  bootstrap rungs remain the owners of transitive import materialization.

## 2026-07-15 - Assignment type truth reaches scalar Option emission

- Added one codegen body-type view that synthesizes initializer and iteration
  facts once, then carries the verified assignment and statement type owners
  together. Recursive statement emission borrows that view instead of copying
  compiler-scale fact structures.
- Repointed scalar `Option<T>` reassignment to the canonical assignment
  expected-type row and semantic expression graph. The migrated consumer can no
  longer recover payload kind from the codegen environment, trim `None` text,
  or call the generic text rewriter as an assignment fallback.
- Narrowed readonly recursive borrowing: a synchronous function may reborrow
  its own `ref` parameter only into the same parameter position. The escape
  summary treats that edge as a backedge, while return escape remains rejected.
  The semantic suite passed 2799/2799, including positive and negative cases.
- A compiler-scale HIR probe containing the recursive body-type view completed
  with zero diagnostics. Production semantic C owners remain below 600 lines.
- Added an owner-scoped projection probe that imports the assignment emitter
  directly. C-built and LLVM-built probe binaries emit the committed
  `Option<Int>` / `Option<String>` `Some` / `None` reassignment rows and both
  reject a missing expected type with the owned fail-closed diagnostic. The
  focused C/LLVM gate and assignment-owner static ratchets pass. The full
  71-fixture codegen matrix was not rerun, so released/default substitution is
  unchanged.
- Split the codegen parity tool-build/manifest responsibility into its own leg.
  The C tool that projects the fixture manifest is now reused by the C parity
  leg instead of compiling the complete codegen twice. The main runner is 545
  lines and the build leg is 143 lines. Reuse requires an exact fingerprint of
  the complete self-host source set, tool source, backend, and compiler binary;
  a binary or modification time alone cannot authorize reuse. This build-cost
  reduction does not replace the scheduled full-matrix parity boundary.

## 2026-07-14 - Collection values have one expression-graph consumer

- Deleted the text-based collection element emitter. Indexed assignment,
  `ArrayPush`, and `ArraySet` now consume their parser-owned expression graph
  through one expected-type renderer for Int, String, and struct elements.
- Added `str_array.pgy` as the twenty-eighth DRV-2 MIR fixture. The manifest
  records the String `ArraySet` leaf graph instead of accepting value text as
  a second semantic source.
- Focused C-built and LLVM-built drivers matched native canonical MIR, emitted
  C, and runtime output for indexed assignment, String and Int mutators, and
  struct `ArrayPush`/`ArraySet`. Missing and invalid graphs failed closed in
  both drivers.
- Static gates reject `EmitCollectionElementValue` and require the shared graph
  renderer. The complete 28-case matrix was not rerun in this slice; the next
  collection seam is the indexed-assignment target-index expression.

## 2026-07-14 - ArraySet consumes the semantic auxiliary graph

- Required the auxiliary-lane semantic expression graph for every `ArraySet`
  statement. The parser now captures argument two, self MIR carries that root,
  and the MIR importer/verifier reject a missing or invalid graph.
- Reused one collection graph-element emission owner for `ArrayPush` and
  `ArraySet`. The migrated set path cannot call the legacy struct/text emitter
  or reconstruct `CodegenAstTextNode(...)` from `expr0`.
- Added `ast_node_array_set.pgy` as the twenty-sixth DRV-2 MIR fixture. Focused
  C-built and LLVM-built driver legs each matched canonical MIR, emitted C, and
  native execution; graph removal and an invalid root both failed closed.
- Added a fail-loud single-fixture filter to the DRV-2 parity runner so a
  reached seam can be checked without executing the complete matrix. The
  default CI path remains the full inventory; the complete 26-case matrix was
  not rerun in this slice.
- Routed C assignment expected-type lookup through
  `transpiler_active_mir_identity`; direct `ctx->mir` access remains confined
  to the inventory owner allowlist.

## 2026-07-14 - ArrayPush consumes the semantic value graph

- Required the value-lane semantic expression graph for every `ArrayPush`
  statement and repointed the self-host C emitter to consume that graph.
- Split collection element projection into its own responsibility owner. The
  migrated push path cannot call the legacy struct/text emitter; array literals
  and `ArraySet` remain explicitly documented bridges.
- Added a `CodegenAstTextNode` constructor push fixture. C-built and LLVM-built
  self-host codegen both produced the expected executable output, while the
  existing enum/event negative legs remained fail-closed.
- Raised the committed codegen fixture frontier from 69 to 70. This is one
  executable consumer replacement, not whole-compiler self-host completion.
- Extended the same fixture into the 25-case DRV-2 MIR inventory. The parser
  now owns the pushed argument subtree, self MIR serializes it as `expr0_graph`,
  and the MIR importer/verifier reject its absence instead of reparsing
  `expr0`. C-built and LLVM-built drivers emitted byte-identical C and both
  generated programs printed `1`.

## 2026-07-13 - First semantic expression-shape consumer

- Extended the existing semantic expression-surface authority instead of
  creating a second expression owner. It now stores normalized atom, value,
  and auxiliary payloads plus compact top-level operator rows keyed by
  `SyntaxNodeId` and lane.
- Repointed `Log` emission and the role-operator fixture to consume the atom
  row's additive index and operator kind. The fixture proves `+` dispatch and
  `-` non-dispatch. The semantic-shape emitter fails closed on a missing or
  mismatched row and is forbidden from calling `FindTopLevelPlus`.
- Extended the same authority to scalar/String returns through the atom lane
  and ordinary scalar/String local initializers and assignments through the
  value lane. Focused Int/String return, Int/Float initializer, and Long
  assignment fixtures pass C/LLVM oracle parity; no alias owner was added.
- Added distinct top-level `||`, `&&`, equality position, and equality-kind
  rows. `if`/`while` root lowering consumes them without `FindTopLevelOp2`, and
  a shared equality projection preserves String/enum behavior. Logical
  precedence and String equality fixtures pass C/LLVM parity.
- Split semantic-shape and log projection from the large expression/statement
  emitters rather than raising the 600-line responsibility cap.
- Verified self-host codegen C emission, component contracts, and focused
  C/LLVM oracle parity including enum/event negative legs and role execution.
- Renamed the single registry identity from runtime-only expression usage to
  the unified expression surface and honestly reopened it as `BRIDGE`. The
  28-row spine is now `CLOSED=12 BRIDGE=7 ACTIVE=9`; indexed collection
  elements, wrapper internals, auxiliary consumers, and recursive child
  expressions remain compact-text backed.

## 2026-07-13 - Top-level declaration routing becomes semantic-owned

- Added declaration `NodeId` identity to enum facts and fail-closed node lookup
  accessors for enum, nominal, and role owners. Function identity reuses the
  existing semantic signature rows rather than introducing a routing alias.
- Repointed `program_emit.pgy` so function, nominal, role, enum, ability, and
  event routing consumes semantic owners. Ability/event classification reuses
  canonical `SemanticAstKindSurfaceFacts`; no consumer-specific alias owner was
  introduced. Deleted all six corresponding codegen arena predicates.
- Proved the bounded multi-owner declaration-routing model in `SoTAuthority.v`.
  The model rejects an owner-plus-AST fallback, while the live adequacy gate
  mutation-tests removal of each declaration identity row.
- Verified the component contract, seven focused C/LLVM declaration fixtures,
  payload-enum and event-declaration fail-closed behavior, and role-operator
  parity. The owner spine remains 28 rows: `CLOSED=13 BRIDGE=6 ACTIVE=9`.
- Deleted the dead codegen arena atom/type/value/aux/parameter payload views.
  The remaining mixed-expression blocker is now expression text inside semantic
  rows, not a backend-accessible arena fallback.

## 2026-07-11 - Readonly ref carriage and 250-source M2 closure

- Added readonly `ref` parameter consumption to the self-host C emitter. The
  ABI row emits `const T *`; the type environment separately records a
  dereferenced value binding and its raw readonly address. Ordinary scalar and
  member expressions consume the value binding, while ref-to-ref calls forward
  the raw address. A committed parity fixture covers all three forms.
- Fixed the typed AST projection so `Action:` under a `Subject` is the same
  canonical function kind as native AST `AST_FUNC_DECL`. The signature contract
  now proves the subject owner and the inferred `self: SubjectType` row.
- Raised the committed codegen frontier to 69 fixtures and the derived MIR JSON
  frontier to 96. The local codegen runner accepts an explicit fixture filter,
  so the readonly-ref row is proved without rerunning unrelated cases.
- Closed the six codegen failures from the previous 244/250 ledger with a
  focused 6/6 run. The two split executable contract owners passed all four
  stages at 2/2, and every M2 source/stage/intersection minimum is now 250.
- Made the likeness gate include non-ignored untracked implementation owners,
  so local evidence no longer changes merely because a new file was staged.
  Current ratchets are string-munge 107, sentinel 0, and errors-as-data 1024.
- Made `self-host-completeness-smoke` consume an explicit prebuilt `PGY_BIN`
  without rebuilding the default compiler. The impact runner now executes its
  five-source plan at 5/5 per stage without switching LLVM configuration or
  deleting the shared object graph. Clean/cache ledger parity and impact-plan
  parity both remain green.

## 2026-07-10 - Artifact body verdict replaces statement payload rescans

- Added semantic-owned statement rows for return, condition, log, exit, match,
  array-pop, and bare-call payloads. Codegen consumes the same rows through a
  fail-closed view; the earlier codegen statement payload owner is deleted.
- Added one ordered artifact expression verdict for call, undefined-use, try,
  logical, binary, and inferred-type checks. Initializer and assignment facts
  now consume it instead of reducing unknown uses to generic type gaps.
- Added return/condition/call statement type facts and a document-order body
  verdict across initializer, assignment, and statement rows. DRV-2 expands
  from 10 to 16 C/LLVM fixtures and no longer calls the source body scanner.
- Added semantic type-name canonicalization at signature/local capture so
  artifact spellings such as `Option<Int: Int>` do not become semantic aliases.
- Raised the production source inventory and every M2 minimum from 235 to 240.
  For-loop binding/iterator facts and CFG/MIR lowering remain open.

## 2026-07-10 - Assignment payload and type verdict become semantic facts

- Added `SemanticAstAssignmentFacts` as the sole artifact owner of assignment
  function/scope, target/base/index, and RHS rows. Codegen now consumes those
  rows through `semantic_assignment_codegen_view_owner.pgy`; the two earlier
  codegen AST assignment readers were deleted and ratcheted against return.
- Extracted artifact expression environment construction so initializer and
  assignment type verdicts consume the same parameter, lexical scope, visible
  local, builtin, and user-function facts.
- Added `SemanticAstAssignmentTypeFacts` and made DRV-2 fail closed on missing,
  unresolved, undefined-target, or mismatched assignment evidence. The C/LLVM
  parity frontier now includes normal and nested reassignment plus type and
  target rejection fixtures.
- Remaining hard semantic work is expression-use validation, return/condition
  body verdicts, and CFG/MIR lowering. Those must consume owned artifact facts;
  the source-scanning checker remains an oracle only.
- The owner split raises the production completeness inventory and every M2
  minimum from 232 to 235; all production owners remain under 600 lines.

## 2026-07-10 - Initializer typing becomes a hard driver fact

- Added `SemanticAstInitializerTypeFacts` as the sole artifact-native owner of
  local initializer inferred types and row verdicts. It joins signature,
  lexical scope, local binding, and initializer payload facts without reading
  source text or calling the source-scanning semantic checker.
- Added the DRV-2 `--emit-c-verified` entrypoint. Missing, unknown, undefined,
  or mismatched initializer facts now stop before codegen on that hard path.
- Kept DRV-0/DRV-1 lightweight for the existing 68-fixture codegen breadth
  frontier. Initializer evidence is computed only by DRV-2, avoiding an
  O(local-squared) fact join on every ordinary codegen check. Assignment/use/
  body verdicts and MIR lowering are still open, so this is a bounded
  hard-semantic rung rather than whole-compiler substitution.
- Unified parser/semantic character-class and trivia scanning in
  `lib/source_scan_owner.pgy`; semantic keyword/identifier reads retain their
  distinct skip policy under explicit semantic names. Moved the 77 builtin
  signature rows from `program_check_owner.pgy` into one owner consumed by both
  source and artifact semantic paths.
- Replaced quadratic AST parent/child scans with an indentation stack and
  prefix child rows. Replaced character-by-character newline discovery with a
  single line split. The cached codegen check for a 996,867-byte, 24,340-row
  DRV-1 artifact changed from 53,003 ms to 374 ms on the same machine.

## Ground rules (BDFL)

- **Verified rungs only.** No unverified fork of compiler code into Pergyra.
  Every increment must be byte/behaviour-checked against the C (and, where the
  build has LLVM, the LLVM) oracle. (`project_self_host_hard_migration_open`,
  2026-06-17.)
- **Narrow verified core, not more rewriting.** Hard self-hosting is measured by
  ratcheting compatibility fallback to zero, giving each IR layer a verifier
  that owns its contract, and golden-testing ABI/diagnostics/JSON/ordering --
  not by porting more compiler code while escape paths remain. (Scorecard
  `07_hard_self_host_scorecard.md`.)
- **Partial is acceptable; no time-forcing.** Completion is a direction, not a
  deadline. (`project_no_self_host_decision`.)
- **Parallel-work hygiene.** The BDFL owns the live capability-5 frontier
  (MIR source-payload retirement: `mir_branch_source_facts`, match/select/branch
  facts). Assisting sessions stay out of those files and work non-colliding
  areas (front-end, measurement, verifiers for untouched layers), committing
  only their own files.

## 2026-07-10 - Local initializer payload joins the semantic artifact

- Extended `SemanticAstLocalBindingFacts` with artifact-bound initializer rows
  and round-trip validation against the same parser-owned `AstTreeArtifact`.
- Repointed `EmitLet` through the fail-closed semantic codegen view, then
  deleted `ast_local_initializer_codegen_view_owner.pgy` without an alias.
- Repointed array-literal and try-let shape projection to the same semantic row,
  removing their direct `TypedAstArenaValueText` initializer reads as well.
- Tightened the component and boundary-migration gates so direct
  `CodegenAstArenaLetInitializerOrDie` reads and the retired file cannot return.
- This closes initializer payload ownership only. Initializer expression type
  verdicts, assignment/use validation, remaining body semantics, and MIR
  lowering remain open before the integrated driver is a full replacement.

## 2026-07-10 - Local declarations become artifact-native semantic facts

- Added `SemanticAstLocalBindingFacts` as the owner of local declaration node,
  enclosing function, lexical block, name, and optional declared-type rows.
  The verifier binds those rows back to the exact shared `AstTreeArtifact` and
  rejects stale names, missing initializers, invalid scopes, and missing
  function owners.
- Retired `ast_text_local_binding_owner.pgy` without an alias. `EmitLet` and
  `EmitTryLet` now consume semantic name/type rows; initializer expression text
  remains explicitly isolated in `ast_local_initializer_codegen_view_owner.pgy`
  until expression facts become typed HIR.
- Extended the HIR row owner so untyped `Let: name = value` preserves the
  binding name. Semantic facts accept the absent declared type, while the
  current bounded codegen fails closed instead of inferring it locally.
- Focused completeness passed the four changed/new owners through
  lexer/parser/semantic/codegen 4/4 and measured 228 production sources; all M2
  minima were raised to 228.
- Verified the untyped-local negative boundary, C/LLVM semantic parity at
  110/110, C/LLVM codegen parity at 68/68, and integrated driver
  `gen2 == gen3` at 17,536 generated-C lines.
- Honest boundary: initializer expression typing, assignment/use validation,
  the remaining body verdict, and MIR lowering are still outside the executable
  driver. Whole-compiler self-hosting remains incomplete.

## 2026-07-10 - Function signature facts become artifact-native

- Added `SemanticAstFunctionSignatureFacts` as the single owner of function
  owner/name, parameter node/name/type/mode, and return-type rows derived from
  the parser-owned `AstTreeArtifact`.
- Repointed self-host function emission, prototype emission, role-operator
  lookup, and function type-environment construction to that same table. The
  codegen-owned `ast_text_function_signature_owner.pgy` was deleted without an
  alias or compatibility fallback.
- Strengthened artifact binding so a same-sized stale table cannot pass: the
  verifier checks ordered function nodes, contiguous parameter ranges, owner
  and payload equality, marker uniqueness, and complete function coverage.
- Focused M2 completeness passed the two new/replacement owners through
  lexer/parser/semantic/codegen 2/2 and measured 226 total production sources;
  all M2 minima were raised to 226.
- Verified structured no-`Main` rejection, byte-identical `hello` C, C/LLVM
  semantic parity at 110/110, C/LLVM codegen parity at 68/68, and integrated
  driver `gen2 == gen3` at 17,304 generated-C lines.
- Honest boundary: local binding/body semantic facts and MIR lowering remain
  outside the executable driver. Whole-compiler self-hosting is not complete.

## 2026-07-10 - First semantic verdict enters the self-eating driver

- Added one canonical HIR node-kind owner and removed numeric kind meanings
  from inventory and codegen views. The unused alternate `NodeKind` enum was
  retired; `AstNode.kind` now consumes the same integer fact contract as the
  arena.
- Added `SemanticAstArtifactVerdict` over the parser-owned artifact. It owns
  executable `Main` cardinality, emits the shared structured semantic diagnostic
  shape, and passes evidence to codegen; codegen no longer recounts `Main`.
- The first fixed-point attempt exposed self-host C wrong-code for direct array
  literal returns. The owner was rewritten to the already-supported typed local
  plus return form instead of adding a backend exception.
- Focused completeness proved the four added HIR/semantic owners through
  lexer/parser/semantic/codegen 4/4 and raised all M2 minima to 225.
- Verified C/LLVM semantic parity at 110/110, byte-identical `hello` output,
  structured rejection of a real no-`Main` source, and integrated driver
  `gen2 == gen3` at 16,881 generated-C lines.
- Honest boundary: this is one semantic decision, not the source-scanner
  semantic checker or MIR. The next rung must move another semantic fact onto
  the shared artifact without reparsing source.

## 2026-07-10 - Parser and codegen share one HIR artifact

- Added `AstTreeArtifact` as the parser-produced boundary carrying compact text
  provenance, the shared `AstArena`, and node count. The integrated driver now
  passes that same artifact into codegen instead of asking codegen to rebuild
  the arena.
- Split shared compact-text scanning, row inventory, and arena projection into
  HIR owners. Codegen retains only its fail-closed arena view and emission
  participants.
- The first bootstrap attempt exposed a generated-C ordering bug because the
  artifact carried `Array<CodegenAstTextNode>`. The fix was not a backend
  typedef-order exception: provenance became an `AstArena` fact and temporary
  bridge nodes stopped crossing the artifact boundary.
- Verified the integrated driver fixed point at byte-identical `gen2 == gen3`
  with 14,659 generated-C lines. The emitted `hello` C remained byte-identical
  to the pre-cutover artifact.
- Honest boundary: semantic still scans source text. The next valid hard
  self-host rung is semantic consumption of this exact artifact; wiring the
  existing source scanner into the driver would create a second parser and is
  forbidden.

## 2026-07-10 - Typed AST arena ownership moves from codegen to HIR

- Moved the single `AstArena` shape/accessor owner from
  `codegen/typed_ast_node_skeleton.pgy` to
  `hir/typed_ast_arena_owner.pgy`; no compatibility file or alias remains.
- Repointed the transitional AST-text projection and compiler-stage artifact
  owner to the HIR path. Component, compiler-world, and likeness gates now
  require the new owner and reject resurrection of the codegen-owned file.
- Verified the self-host codegen and integrated parser/codegen driver both
  compile through the C backend after the move, and the driver still emits the
  expected standalone C for the `hello` fixture.
- This does not close whole-compiler self-hosting. It creates the correct shared
  owner boundary for the next rung: parser-owned arena production and semantic
  consumption. The existing semantic checker still reparses source text and
  must not be wired into the driver as a second source of syntax truth.

## 2026-07-10 - Integrated parser/codegen driver reaches a fixed point

- Split source-to-AST-to-C composition into
  `driver_pipeline_owner.pgy`. DRV-0/DRV-1 and the bootstrap entrypoint now
  consume that owner instead of allowing parser/codegen wiring to fork.
- Added `driver_bootstrap_main.pgy` as a minimal source/output-file boundary;
  it contains no compiler semantics of its own.
- Added `self-host-driver-bootstrap-test-smoke` after the codegen seed gate:
  the Pergyra-built codegen builds the integrated driver, the self-built driver matches the
  C-oracle-built driver on a real source, and driver `gen2.c == gen3.c` is
  judged by the Pergyra-owned artifact comparator.
- Verified the standalone codegen fixed point at 9,833 generated-C lines and
  the integrated parser/codegen driver fixed point at 14,566 generated-C
  lines. Existing lexer/parser/semantic/mir_lower/tool/fuzz breadth remained
  green.
- Raised the M2 production source inventory/minima from 219 to 221. A focused
  seven-source ledger passed lexer/parser/semantic/codegen 7/7 with
  `total_sources=221`.
- Added a changed-path impact row so `driver*.pgy` selects
  `self-host-driver-bootstrap-test-smoke`; impact manifest/planner parity passed
  with six proof gates, and the default runner proved its five-source
  completeness group 5/5.
- The split driver gate is 155 lines and the codegen seed gate remains 577
  lines, both below their enforced caps. One Windows landing run observed about
  1.35 GB peak working set in the first driver seed generation, so bounded
  emission-buffer work remains a performance blocker rather than being hidden
  by the successful fixed point.
- Honest boundary: this is the first integrated compiler self-rebuild loop, not
  whole-compiler self-hosting. Semantic verdict, MIR lowering, ABI-complete
  backend replacement, and the released native driver remain outside the
  fixed-point executable.

## 2026-07-10 - Completeness impact owner split raises M2 to 219/219

- Split impact-plan/run-group responsibility from
  `completeness_ledger_owner.pgy` into
  `completeness_impact_owner.pgy`, leaving the ledger owner with source
  inventory, stage vocabulary, semantic target mapping, cache fingerprints, and
  monotone minima.
- Repointed the TestHarness manifest and completeness impact planner to import
  the new owner directly, and tightened the component contract so both owners
  stay under the 600-line cap.
- Replaced positional readiness checks with named membership/out-of-range
  checks for stages, cache fingerprints, impact planner paths, impact rows, and
  impact fields.
- Ran the unfiltered `self-host-completeness-smoke`; it proved
  `sources=219`, `lexer=219`, `parser=219`, `semantic=219`, `codegen=219`, and
  `full_pipeline=219`, then the M2 minima moved from 207 to 219.
- This is still not a bootstrap-loop proof: the completeness ledger proves the
  current Pergyra self-host source breadth through the C/LLVM oracle path, while
  whole-compiler self-rebuild remains a later hard self-host rung.

## 2026-07-10 - AIR graph TestHarness path suites consume named membership

- Repointed the remaining AIR graph consumer path-suite readiness predicates in
  `test_harness_air_graph_paths_owner.pgy` from fixed `PathCount() == N` plus
  first/last `PathAt(...)` checks to suite-specific `PathKnown(...)`
  membership plus an out-of-range boundary check.
- Covered ID uniqueness, node-count integrity, reachability, ref-integrity, and
  ref-live suites. The JSON validator suite had already been converted.
- Tightened `self_hosted_component_contract_smoke.sh` so those suites cannot
  return to count/representative-row readiness while keeping ordered rows as the
  stable `test_harness_manifest.pgy` artifact shape for shell parity runners.

## 2026-07-10 - Tool TestHarness bucket removed

- Split the last remaining `test_harness_tool_paths_owner.pgy` responsibilities
  into `test_harness_linter_paths_owner.pgy` and
  `test_harness_doc_link_paths_owner.pgy`.
- Repointed `test_harness_owner.pgy` and `test_harness_manifest.pgy` to import
  those responsibility owners directly, then deleted the generic tool-path
  bucket.
- Tightened the component contract so the retired generic owner must not exist,
  both new owners stay under the 600-line cap, and linter/doc-link readiness
  consumes named path membership plus an out-of-range boundary check instead of
  `PathCount() == N` / representative `PathAt(...)` comparisons.
- Classified `abi_layout_row_owner.pgy` as an explicit ABI fact-resource owner
  for the Pergyra-likeness gate, keeping `core_string_munge_sig` at 108 while
  tightening the errors-as-data ratchet to `RESULT_USE_MIN=802`.

## 2026-07-10 - Lexer TestHarness paths move to lexer owner

- Split `test_harness_lexer_paths_owner.pgy` out of the shared TestHarness tool
  path owner for the self-host lexer parity source, backend comparator source,
  and lexer fixture directory.
- Added `CompilerHarnessLexerPathKnown(...)` and repointed
  `CompilerHarnessLexerParityReady()` from fixed `PathCount() == 3` and
  positional `PathAt(0..2)` checks to named path membership plus an
  out-of-range boundary check.
- Kept the ordered rows as the stable `test_harness_manifest.pgy` artifact
  shape for shell parity runners, while making the lexer owner the path truth
  for the compiler-frontier lexer parity rung.

## 2026-07-10 - Runtime-boundary TestHarness paths move to runtime-boundary owner

- Split `test_harness_runtime_boundary_paths_owner.pgy` out of the shared
  TestHarness tool path owner for the runtime-boundary checker source, expected
  clean artifact, required-term fixture, missing term, and negative artifact.
- Added `CompilerHarnessRuntimeBoundaryPathKnown(...)` and repointed
  `CompilerHarnessRuntimeBoundaryReady()` from fixed `PathCount() == 5` and
  positional `PathAt(0..4)` checks to named path membership plus an
  out-of-range boundary check.
- Kept the ordered rows as the stable `test_harness_manifest.pgy` artifact
  shape for shell parity runners, while making the runtime-boundary owner the
  path truth for the native-runtime-vs-self-host boundary claim.

## 2026-07-10 - Stable-subset TestHarness paths move to stable-subset owner

- Split `test_harness_stable_subset_paths_owner.pgy` out of the shared
  TestHarness tool path owner for the stable-subset checker source, expected
  clean artifact, input manifest, missing-section anchor, and negative artifact.
- Added `CompilerHarnessStableSubsetSectionPathKnown(...)` and repointed
  `CompilerHarnessStableSubsetSectionReady()` from fixed `PathCount() == 5` and
  positional `PathAt(0..4)` checks to named path membership plus an
  out-of-range boundary check.
- Kept the ordered rows as the stable `test_harness_manifest.pgy` artifact
  shape for shell parity runners, while making the stable-subset owner the path
  truth for the beta stable-subset contract checker.

## 2026-07-10 - AST read-surface paths move to source-surface owner

- Split `test_harness_ast_surface_paths_owner.pgy` out of the shared TestHarness
  tool path owner for the source_ast/source_decl ratchet checker source,
  ratchet, and synthetic growth fixture paths.
- Added `CompilerHarnessAstReadSurfacePathKnown(...)` and repointed
  `CompilerHarnessAstReadSurfaceReady()` from fixed `PathCount() == 7` and
  positional `PathAt(0..6)` checks to named path membership plus an
  out-of-range boundary check.
- Kept the ordered rows as the stable `test_harness_manifest.pgy` artifact
  shape for shell parity runners, while removing row position as the
  ast-read-surface path-suite truth.

## 2026-07-10 - Diagnostic catalog paths move to diagnostic owner

- Split `test_harness_diagnostic_paths_owner.pgy` out of the shared TestHarness
  tool path owner for diagnostic-catalog checker source, expected artifact, and
  oracle path suites.
- Added `CompilerHarnessDiagnosticCatalogPathKnown(...)` and repointed
  `CompilerHarnessDiagnosticCatalogReady()` from fixed `PathCount() == 7` and
  positional `PathAt(0..6)` checks to named path membership plus an
  out-of-range boundary check.
- Kept the ordered rows as the stable `test_harness_manifest.pgy` artifact
  shape for shell parity runners, while removing row position as the
  diagnostic-catalog path-suite truth.

## 2026-07-10 - AIR graph validator paths move to AIR graph owner

- Moved the `air-graph-json-validator-paths` suite from the shared TestHarness
  tool path owner into `test_harness_air_graph_paths_owner.pgy`.
- Added `CompilerHarnessAirGraphJsonValidatorPathKnown(...)` and repointed
  `CompilerHarnessAirGraphJsonValidatorReady()` from fixed
  `PathCount() == 9` / positional `PathAt(0..8)` checks to named membership
  plus an out-of-range boundary check.
- Kept the ordered rows as the stable `test_harness_manifest.pgy` artifact
  shape for shell parity runners, while making the AIR graph owner the path
  truth for the validator and the downstream AIR graph consumer suite.

## 2026-07-10 - ExecutionLane TestHarness paths consume named paths

- Split `test_harness_execution_lane_paths_owner.pgy` out of the shared
  TestHarness tool path owner for SEA execution-lane parity source and golden
  path suites.
- Added `CompilerHarnessExecutionLanePathKnown(...)` and repointed
  `CompilerHarnessExecutionLaneParityReady()` from fixed `PathCount() == 5`
  and positional `PathAt(0..4)` checks to named path membership plus an
  out-of-range boundary check.
- Kept the ordered path rows as the stable `test_harness_manifest.pgy` artifact
  shape for shell parity runners, while removing row position as the
  execution-lane path-suite truth.

## 2026-07-10 - ABI TestHarness paths consume named paths

- Split `test_harness_abi_paths_owner.pgy` out of the shared TestHarness tool
  path owner for ABI layout row and runtime-call ABI row path suites.
- Added `CompilerHarnessAbiLayoutRowsPathKnown(...)` and
  `CompilerHarnessRuntimeCallAbiRowsPathKnown(...)`, then repointed both
  readiness checks from fixed `PathCount() == 2` and positional `PathAt(0..1)`
  checks to named path membership plus out-of-range boundary checks.
- Kept the ordered path rows as the stable `test_harness_manifest.pgy` artifact
  shape for shell parity runners, while removing row position as ABI path-suite
  truth.

## 2026-07-10 - Compatibility TestHarness paths consume named paths

- Added `CompilerHarnessCompatibilityCorpusPathKnown(...)` and
  `CompilerHarnessCompatibilityEvolutionPathKnown(...)` to the TestHarness
  compatibility path owner.
- Repointed compatibility-corpus and compatibility-evolution path readiness
  from fixed `PathCount() == N` and positional `PathAt(0..N)` checks to named
  path membership plus out-of-range boundary checks.
- Kept ordered path rows as the stable `test_harness_manifest.pgy` artifact
  shape for shell parity runners, while removing row position as the
  compatibility path-suite truth.

## 2026-07-10 - Backend contract TestHarness paths consume named paths

- Added `CompilerHarnessBackendEmitterContractPathKnown(...)`,
  `CompilerHarnessBackendAirAccessPathKnown(...)`, and
  `CompilerHarnessBackendAbiLayoutContractPathKnown(...)` to the backend
  contract TestHarness path owner.
- Repointed backend-emitter, backend-AIR-access, and backend-ABI-layout
  path-suite readiness from fixed `PathCount() == N` and positional
  `PathAt(0..N)` checks to named path membership plus out-of-range boundary
  checks.
- Kept the ordered path rows stable for `test_harness_manifest.pgy` and the
  parity runners, while removing row position as backend contract path-suite
  truth.

## 2026-07-10 - Lexer payload fixture count consumes manifest owner

- Repointed `LexerTokenPayloadFixtureCount()` to consume
  `LexerFixtureManifestCount()` from `fixture_manifest_owner.pgy` instead of
  carrying a parallel literal.
- Tightened the self-host component and compiler-world contracts so the lexer
  token payload contract cannot reintroduce `return 8;` or compare against a
  second local fixture-count source.
- This is a SoT closure on the existing lexer payload contract; it does not
  change the lexer fixture frontier or the headline substitution percentage.

## 2026-07-10 - TestHarness readiness consumes named row facts

- Added `CompilerHarnessRowKnown(...)` and
  `CompilerHarnessProjectionKnown(...)` to the core TestHarness owner, plus
  `CompilerHarnessComparableArtifactPathKnown(...)` to the comparator path
  owner.
- Repointed `CompilerTestHarnessReady()` so row, projection, and comparable
  artifact readiness consumes named facts instead of fixed `At(0)` or
  `At(Count() - 1)` representative positions.
- Tightened the component and compiler-world contracts so the old positional
  readiness checks cannot return while ordered rows remain available for
  TestHarness manifest emission.

## 2026-07-10 - Target TestHarness path suites consume named paths

- Added `CompilerHarnessTargetCapabilityEnvelopePathKnown(...)` and
  `CompilerHarnessSandboxCapabilityPathKnown(...)` to the target/sandbox
  TestHarness path owner.
- Repointed target-capability and sandbox-capability path-suite readiness from
  fixed `PathCount() == 3` and positional `PathAt(0..2)` checks to named path
  membership plus out-of-range boundary checks.
- Kept the ordered path rows stable for `test_harness_manifest.pgy` and the
  parity runners, while removing row position as the path-suite truth.

## 2026-07-10 - ABI target-policy readiness consumes row membership

- Added `CompilerAbiLayoutTargetPolicyRowKnown(...)` to the ABI target-policy
  owner.
- Repointed `CompilerAbiLayoutTargetPolicyReady()` from `Count() == 1` and
  fixed `At(0)` field comparisons to row membership plus out-of-range field
  boundary checks.
- Kept the ordered `target_policy|0|...` artifact row stable for
  `abi_layout_row_manifest.pgy`, while removing row position as the target
  acceptance truth.

## 2026-07-10 - Parser payload fixture count consumes manifest owner

- Added manifest-owned parser fixture count facts:
  `ParserFixtureManifestCount()`, `ParserFixtureDuplicateCoverageCount()`, and
  `ParserFixturePayloadFixtureCount()`.
- Repointed `ParserAstTreePayloadFixtureCount()` to consume the manifest-owned
  payload frontier instead of carrying the historical `187` literal locally.
  The parser parity manifest remains 188 rows because it intentionally includes
  external `examples/hello.pgy` plus duplicate `generic_class` coverage; the
  payload contract now derives its 187-fixture frontier by subtracting that
  duplicate coverage row.
- Tightened self-host component and compiler-world contracts so the parser AST
  payload contract cannot reintroduce the second local fixture-count source.

## 2026-07-10 - Semantic payload frontier count gets a named owner fact

- Added `SemanticVerdictPayloadFixtureFrontierCount()` as the named frontier
  fact for the semantic verdict payload manifest.
- Repointed `SemanticVerdictPayloadContractReady()` so it compares the manifest
  row count against that frontier fact instead of carrying a local
  `SemanticVerdictPayloadFixtureCount() != 110` literal.
- Tightened the self-host component and compiler-world contracts so the old
  direct semantic fixture-count comparison cannot return.

## 2026-07-10 - Typed AST arena payload frontier gets a named fact

- Added `TypedAstArenaPayloadFixtureFrontierCount()` as the explicit frontier
  fact for the single synthetic typed AST arena bootstrap fixture.
- Repointed `TypedAstArenaPayloadFixtureCount()` and
  `TypedAstArenaPayloadContractReady()` so the codegen emission stage consumes
  that named frontier fact instead of comparing directly against `1`.
- Tightened the self-host component and compiler-world contracts so the old
  direct typed-arena payload fixture comparison cannot return.
- Verified the codegen bootstrap fixpoint after the source change:
  `SELF-HOSTING OK`, `gen2 == gen3` at 9916 generated-C lines.

## 2026-07-10 - Compiler path manifest readiness consumes count owners

- Repointed `CompilerStagePathManifestReady()` so it no longer rechecks
  `CompilerParityPathCount()`, `CompilerWorldManifestPathCount()`, and
  `CompilerWorldProjectionPathCount()` against parallel numeric literals.
- Added `CompilerWorldProjectionPrefixCount()` and made projection path count
  and projection indexing consume that prefix fact.
- Tightened the compiler-world contract so readiness validates owner-count
  boundaries and projection derivation, while forbidding the old direct
  `8`/`36`/`39` count comparisons from returning.

## 2026-07-10 - TestHarness readiness consumes count owners

- Added `CompilerHarnessRowIndexKnown()` so core TestHarness row readiness can
  check valid and out-of-bounds row indexes through the row-count owner instead
  of assuming the current final index.
- Repointed `CompilerTestHarnessReady()` so row, projection, and comparable
  artifact-path last-entry checks consume `*Count() - 1`, with explicit
  out-of-bounds checks at `*Count()`.
- Tightened the compiler-world and self-host component contracts so readiness
  cannot reintroduce the old direct `8`/`3`/`2` count comparisons or fixed
  final-index checks.

## 2026-07-10 - Incremental fact graph readiness consumes axis owners

- Added named index facts and `*IndexKnown()` boundary checks for the
  incremental graph's stage, dependency, artifact, and verifier axes.
- Repointed `CompilerIncrementalFactGraphReady()` so MIR-stage,
  ABI/runtime-fingerprint, MIR/ABI artifact, and fail-closed verifier checks
  consume those owner facts instead of repeating the current numeric row
  positions in readiness.
- Tightened the self-host component contract so the old direct `9`/`12`/`9`/`5`
  count comparisons and fixed semantic row indexes cannot return. This keeps
  the rung0 cache proof honest while precise import/module invalidation remains
  a future graph-consumer step.

## 2026-07-10 - ABI layout readiness consumes row lookup facts

- Added type-name lookup accessors for ABI row C value spelling, field order,
  materialization, and default return values.
- Repointed `CompilerAbiLayoutRowsReady()` so representative concrete ABI rows
  are validated through `CompilerAbiLayoutRowIndex(...)` consumers instead of
  fixed numeric row positions such as `Option<String>` row 9 or `Long` row 10.
- Tightened the self-host component and compiler-world contracts so direct
  concrete row-count and fixed-index ABI readiness checks cannot return. This
  reduces the active cross-backend ABI/layout row projection blocker without
  claiming full native C/LLVM ABI closure.

## 2026-07-10 - Incremental cache parity consumes owner plan

- Moved the default source/stage filters for
  `completeness_incremental_cache_parity.sh` behind
  `EmitCompilerIncrementalCacheParityPlan()` in
  `incremental_fact_graph_owner.pgy`.
- Wired `test_harness_manifest.pgy` to emit the
  `self-host-incremental-cache-parity` suite so the shell runner consumes the
  Pergyra owner row instead of hard-coding the owner source path locally.

## 2026-07-10 - Completeness cache gets clean/cache parity gate

- Added `tests/self_hosted/parity/completeness_incremental_cache_parity.sh`.
  It runs the focused completeness ledger in clean/no-cache, cache-prime, and
  cache-hit modes, compares the stable `ledger.tsv` plus focused summary
  artifacts, and fails unless the cache-hit run reports an actual cache hit.
- Added `self-host-completeness-incremental-cache-parity-test-smoke` and wired
  it into `self-host-preparation-parity-test-smoke`. This is still a rung0
  pass-marker cache proof, not precise import/module invalidation.

## 2026-07-10 - Incremental fact graph gets a self-host owner

- Added `src/self_hosted/compiler/incremental_fact_graph_owner.pgy` as the
  owner for future precise invalidation. It names
  `pgy.selfhost.incremental-fact-graph.v1`, stage artifact kinds, dependency
  fingerprint axes, cache-hit/missing-fact policy, and clean-vs-incremental
  verifier vocabulary.
- Wired `CompilerIncrementalFactGraphReady()` into `CompilerTestHarnessReady()`
  so TestHarness readiness now consumes the incremental graph contract rather
  than leaving incrementality as prose under the completeness ledger.
- Promoted the M2 completeness minima from 206 to 207 because the incremental
  graph owner is a production self-host source. The contract deliberately keeps
  the existing `pgy.selfhost.completeness-cache.v1` cache coarse until
  import/module graph fingerprints become Pergyra-owned.

## 2026-07-09 - Completeness impact plan becomes owner-owned

- Added `pgy.selfhost.completeness-impact.v1` rows to
  `completeness_ledger_owner.pgy`, mapping self-host source patterns to the
  existing completeness source/stage filter env vars and proof gates.
- Projected the impact plan through `test_harness_manifest.pgy` so shell
  runners can consume owner rows instead of local impact lists.
- Added `self-host-completeness-impact-test-smoke` to compile the Pergyra
  TestHarness manifest and verify the impact rows against Make targets and
  runner env knobs.

## 2026-07-09 - Completeness impact planner consumes owner rows

- Added `src/self_hosted/tools/completeness_impact_planner/main.pgy` as the
  first executable consumer for completeness impact rows. It accepts explicit
  changed paths, consumes `CompilerCompletenessImpact*` owner facts, emits
  `pgy.selfhost.completeness-impact-planner.v1`, and fails closed on unmatched
  paths.
- Projected the planner source, expected artifacts, and representative changed
  paths through `self-host-completeness-impact-planner-paths` in the Pergyra
  TestHarness manifest.
- Added C/LLVM parity for the clean and unmatched-path planner artifacts and
  enrolled the planner in the codegen bootstrap tool breadth rows. This is
  still rung0 impact routing, not automatic git or import-graph invalidation.
- Promoted the M2 completeness minima to 206 after the filtered completeness
  gate proved the changed production self-host sources through lexer, parser,
  semantic, and codegen.
- Follow-up: repointed the planner from `impact_id`-specific path branches to
  the row-owned `source_pattern` and `proof_gate` fields, so changing impact
  rows does not require parallel planner edits.
- Extended the planner artifact with matched impact row items, including
  source/stage filter envs and values. The shell gate now compares that richer
  JSON through the backend-output comparator instead of recovering runner
  knobs from the proof-gate list.
- Added proof-gate `run_groups` to the same artifact, grouping source-filter
  values per gate so a future runner can execute the plan without rebuilding
  path-to-env aggregation in shell.

## 2026-07-10 - Completeness run groups become runner-consumable artifacts

- Added `pgy.selfhost.completeness-impact-run-groups.v1` as the line-oriented
  run-group projection emitted by `completeness_impact_planner --run-groups`.
  This gives runners a direct TSV plan instead of forcing shell to parse JSON or
  rebuild groups from matched impact rows.
- Added `run_group_plan` to `ArtifactZone` so the new projection is compared by
  the shared backend output comparator with an explicit artifact kind.
- Extended `self-host-completeness-impact-planner-test-smoke` so the Pergyra
  planner's JSON artifact and run-group plan artifact are both checked under
  C/LLVM tool parity.

## 2026-07-10 - Completeness run-group plan gets a bounded runner

- Added `tests/self_hosted/parity/completeness_impact_run_group_runner.sh`.
  It consumes the planner's `pgy.selfhost.completeness-impact-run-groups.v1`
  TSV projection, validates all groups, and executes a bounded prefix of the
  proof-gate plan without rebuilding path-class impact decisions in shell.
- Added `self-host-completeness-impact-runner-test-smoke` and wired it into
  `self-host-completeness-impact-test-smoke`. The default runner smoke executes
  the first run group, so the source/stage filter envs are proven against the
  real `self-host-completeness-smoke` boundary while keeping the routine gate
  bounded.
- Wired the same runner into `self-host-preparation-parity-test-smoke`, so CI's
  normal self-host preparation path now proves the planner output is executable,
  not only comparable.

## 2026-07-10 - Completeness runner accepts caller-owned changed paths

- Extended `completeness_impact_run_group_runner.sh` so callers can provide
  actual changed paths through `PGY_SELFHOST_IMPACT_CHANGED_PATHS` or
  `PGY_SELFHOST_IMPACT_CHANGED_PATHS_FILE`.
- Added `PGY_SELFHOST_IMPACT_RUNNER_REQUIRE_CHANGED_PATHS`, used by the new
  `self-host-preparation-impact-test-smoke` Make entrypoint so impact mode
  cannot silently fall back to representative clean fixture paths.
- Added `scripts/self_host_impact_changed_paths.sh` and
  `self-host-preparation-impact-changed-paths-test-smoke` as the outer
  changed-path caller boundary. It can collect paths from git or explicit env
  input, but impact classification still lives in the Pergyra planner.
- Extended `PGY_SELFHOST_IMPACT_RUNNER_MAX_GROUPS` with `all`, so callers can
  execute every affected proof-gate group without re-counting planner rows in
  shell.
- The runner still consumes the Pergyra-owned run-group projection for proof
  gate execution, but it no longer has to rely on the representative clean
  fixture paths when an upstream CI or developer boundary already knows the
  changed paths.
- Tightened the self-host component contract so the runner cannot grow its own
  git-state inspection path; git diff/status/ls-files remain the responsibility
  of an outer caller until a Pergyra-owned dependency fingerprint graph exists.

## 2026-07-09 - AIR graph scalar scans move into JSON fact ownership

- Added recursive scalar-field collection facts to `json_fact_table.pgy`:
  `JsonScalarToken`, `JsonCollectScalarFieldValues`, and
  `JsonScalarFieldValues`.
- Repointed `air_graph_json_validator/scan_owner.pgy` so graph-wide scalar
  reads consume that shared JSON fact owner instead of carrying a local
  recursive JSON scanner.
- Tightened the component contract so the AIR graph scanner cannot reintroduce
  local `ReadJsonString` / `JsonValueEnd` scalar recursion.

## 2026-07-09 - Completeness harness gets a fact-owned cache rung

- Added a rung0 incremental cache contract to `completeness_ledger_owner.pgy`:
  schema plus required source-set, stage, source, check-target, tool executable,
  and producer executable fingerprints.
- Repointed `tests/self_hosted/parity/completeness_ledger.sh` so stage-tool
  builds and stage checks may reuse artifacts only when the full fingerprint
  key matches.
- Kept the cache deliberately coarse. Any production self-host source change
  invalidates the cache until import/module graph fingerprints become
  Pergyra-owned.

## 2026-07-09 - Kind usage facts enter codegen input ownership

- Added `ast_kind_usage_owner.pgy` as the named owner for codegen
  statement-shape usage facts consumed by runtime/header decisions.
- Repointed `ast_usage_owner.pgy` so aggregate runtime usage consumes
  `CodegenKindUsageFactsFromArena(...)` instead of scanning
  `CodegenAstArenaKindPresent(...)` rows itself.
- Tightened the component contract so the aggregate owner cannot reintroduce a
  kind-row scan, and promoted M2 completeness minima from 204 to 205 for
  source inventory, lexer, parser, semantic, codegen, lex+parse,
  lex+parse+semantic, and full-pipeline intersection.

## 2026-07-13 - Kind usage authority moves to semantic facts

- Added `SemanticAstKindSurfaceFacts` as the canonical artifact node-kind
  owner consumed by runtime/header projection.
- Replaced `CodegenKindUsageFactsFromArena(...)` with the semantic-only
  `CodegenKindUsageFactsFromSemantic(...)` projection and removed arena/count
  from aggregate runtime usage.
- Removed the incorrect backend-local `ArrayLiteral` alias for tag 16; the
  canonical kind is `ArrayPopStmt`.
- Locked the cutover with Coq owner/fallback theorems, source mutations, the
  23-row owner registry, and five-fixture C/LLVM parity.

## 2026-07-13 - Entrypoint selection consumes semantic signatures

- Replaced semantic and codegen arena rescans for `Main` with ordered function
  node/name rows from `SemanticAstFunctionSignatureFacts`.
- Added an `Option<Int>` codegen selection projection, removing the local `-1`
  sentinel and `CodegenAstArenaIsMainFunction` predicate.
- Locked the boundary as the ninth bounded `CLOSED` registry row and verified
  helper-before-Main plus simple Main fixtures under C/LLVM-built tools.

## 2026-07-13 - Statement routing consumes three semantic authorities

- Routed `Let`, `Assign`, and all other emitted statement kinds through their
  local-binding, assignment, and statement fact owners respectively.
- Added `Defer`, `Break`, `Continue`, and `MatchDefault` to semantic statement
  inventory, then removed twenty dead codegen arena predicates.
- Kept `Else`/`Block`/`Then` as syntax-structure traversal and verified twelve
  representative statement fixtures under C/LLVM-built tools.

## 2026-07-09 - Type usage facts enter codegen input ownership

- Added `ast_type_usage_owner.pgy` as the named owner for codegen type-surface
  usage facts consumed by runtime/header decisions.
- Repointed `ast_usage_owner.pgy` so aggregate runtime usage consumes
  `CodegenTypeUsageFactsFromArena(...)` instead of scanning
  `TypedAstArenaTypeName(...)` rows itself.
- Tightened the component contract so the aggregate owner cannot reintroduce a
  type-name row scan, and promoted M2 completeness minima from 203 to 204 for
  source inventory, lexer, parser, semantic, codegen, lex+parse,
  lex+parse+semantic, and full-pipeline intersection.

## 2026-07-09 - Completeness minima promoted to 203

- Promoted `CompilerCompletenessLedger` minima from the 195-source slice to
  203 for source inventory, lexer, parser, semantic, codegen, lex+parse,
  lex+parse+semantic, and full-pipeline intersection.
- Updated the component contract so `return 195;` is rejected and all eight
  minima must carry the 203-source closed slice.
- Synchronized the progress, M2, production-bar, and red-team docs with the
  latest all-in-one `self-host-preparation-test-smoke` evidence: 203/203 staged
  completeness and a codegen bootstrap fixpoint at 9816 generated-C lines.

## 2026-07-09 - Completeness minima promoted to 195

- Promoted `CompilerCompletenessLedger` minima from the historical 155-source
  slice to 195 for source inventory, lexer, parser, semantic, codegen,
  lex+parse, lex+parse+semantic, and full-pipeline intersection.
- Updated the component contract so `return 155;` is rejected and all eight
  minima must carry the 195-source closed slice.
- Synchronized the production-bar and M2 planning docs with the latest broad
  `self-host-preparation-parity-test-smoke` evidence: 195/195 staged
  completeness and a codegen bootstrap fixpoint at 8560 generated-C lines.

## 2026-07-06 - Backend tri-compare consumes current comparator path suite

- Updated `backend_output_tri_compare_parity.sh` to consume the seven-row
  `backend-output-comparator-paths` manifest after the comparator argv artifact
  row landed.
- Stopped re-reading comparator JSON `ok:true` in shell for dynamic C/LLVM
  stdout/stderr comparisons; the comparator exit code plus schema emission is
  the runner boundary.
- Tightened the component contract against reopening the old six-row path count
  or shell-owned `ok:true` verdict check.

## 2026-07-06 - Runtime boundary clean verdict stops shell re-grep

- Removed the runtime-boundary parity runner's clean-path shell `grep` oracle
  over the required-term manifest.
- Kept the compiled checker's `--terms` manifest only as scratch fixture setup
  input for the missing-term negative case.
- Tightened the intent/ledger wording so the clean and missing runtime-boundary
  verdicts are owned by the Pergyra checker JSON plus committed artifacts.

## 2026-07-06 - Backend comparator argv verdict consumes artifact

- Added a TestHarness-owned expected JSON artifact for the comparator argv-mode
  fixture.
- Repointed `backend_output_comparator_parity.sh` so shell still passes
  explicit artifact/projection arguments, but compares the full argv verdict
  against the committed artifact instead of grepping projection rows or
  `ok:true`.
- Tightened the component contract against reintroducing shell-owned comparator
  argv verdict interpretation.

## 2026-07-06 - AIR graph missing-key verdict consumes artifact

- Added a TestHarness-owned expected JSON artifact for the AIR graph
  validator's synthetic missing-`summary` fixture.
- Repointed `air_graph_json_validator_parity.sh` so shell still writes the
  scratch missing-key fixture and checks `rc=1`, but compares the full negative
  verdict against the committed artifact instead of grepping `ok:false`,
  `missing_keys`, or finding fields.
- Tightened the component contract against reintroducing shell-owned AIR graph
  missing-key verdict interpretation.

## 2026-07-06 - Doc-link dead-link verdict consumes artifact owner

- Added a TestHarness-owned expected JSON artifact for the doc-link synthetic
  dead-link fixture.
- Repointed `doc_link_checker_parity.sh` so shell only handles scratch
  `INDEX.md` mutation and `rc=1`; the full dead-link verdict is compared
  through `backend_output_comparator` as `run_output`.
- Removed the `missing_link` finding-kind row from
  `test_harness_tool_paths_owner.pgy`, and tightened the component contract
  against reintroducing shell-owned dead-link finding/path checks.

## 2026-07-06 - AST-read growth verdict consumes artifact owner

- Added a TestHarness-owned expected JSON artifact for the synthetic
  `source_ast_codegen` growth fixture.
- Repointed `ast_read_surface_checker_parity.sh` so shell only handles scratch
  source/ratchet creation and `rc=1`; the full growth verdict is compared
  through `backend_output_comparator` as `run_output`.
- Removed the growth finding-kind row from `test_harness_tool_paths_owner.pgy`,
  and tightened the component contract against reintroducing shell-owned
  `surface_growth` interpretation.

## 2026-07-06 - Module manifest negative verdicts consume artifact owner

- Added TestHarness-owned expected JSON artifacts for the module-manifest
  missing-modules, nested-modules, and nested-field negative fixtures.
- Repointed `module_manifest_resolver_parity.sh` so shell only handles process
  execution, scratch fixture writes, and `rc=1`; the full negative verdict
  shapes are compared through `backend_output_comparator` as `run_output`.
- Removed the missing/failure finding-kind rows from
  `test_harness_inventory_paths_owner.pgy`, and tightened the component
  contract against reintroducing shell-owned module-manifest finding checks.

## 2026-07-06 - Document checker negative verdicts consume artifact owner

- Added TestHarness-owned missing-section and missing-term expected JSON
  artifacts for the stable-subset and runtime-boundary checkers.
- Repointed both parity runners so shell only handles process execution,
  scratch mutation, and `rc=1`; the full negative verdict shape is compared
  through `backend_output_comparator` as `run_output`.
- Removed the missing field/value rows from `test_harness_tool_paths_owner.pgy`,
  and tightened the component contract against reintroducing shell-owned
  `ok:false` or `missing:1` interpretation for these runners.

## 2026-07-06 - AIR graph negative verdicts consume artifact owner

- Added TestHarness-owned negative expected JSON artifacts for AIR graph
  node-count, reachability, edge-reference, and live-reference checkers.
- Repointed those four parity runners so shell only handles process execution,
  scratch mutation where needed, and `rc=1`; the full negative verdict shape is
  compared through `backend_output_comparator` as `air_json`.
- Removed the remaining AIR graph expected-finding-kind rows from
  `test_harness_air_graph_paths_owner.pgy`, and tightened the component
  contract against reintroducing shell-owned `ok:false` or finding-kind
  interpretation for these runners.

## 2026-07-06 - AIR id uniqueness negative verdict consumes artifact owner

- Added a TestHarness-owned duplicate expected JSON artifact for the AIR graph
  id uniqueness checker.
- Repointed `air_graph_id_uniqueness_parity.sh` so shell only checks the
  duplicate fixture process boundary (`rc=1`); the duplicate verdict shape is
  compared through `backend_output_comparator` as an `air_json` artifact.
- Removed the TestHarness row that exposed only the `duplicate_id` finding
  string, and tightened the component contract against reintroducing shell-owned
  `ok:false` or finding-kind interpretation for this parity runner.

## 2026-07-06 - Examples inventory drift verdict consumes artifact owner

- Added a TestHarness-owned expected JSON artifact for the synthetic
  examples-inventory count-drift fixture.
- Repointed `examples_inventory_checker_parity.sh` so shell only handles
  scratch example omission and `rc=1`; the full count-drift verdict is compared
  through `backend_output_comparator` as `run_output`.
- Removed the examples-inventory finding-kind row from
  `test_harness_inventory_paths_owner.pgy`, and tightened the component
  contract against reintroducing shell-owned `inventory_count_drift`
  interpretation.

## 2026-07-06 - Stdlib dispatch drift verdict consumes artifact owner

- Added a TestHarness-owned expected JSON artifact for the synthetic
  stdlib-dispatch count-drift fixture.
- Repointed `stdlib_dispatch_inventory_checker_parity.sh` so shell only
  handles scratch LLVM row stripping and `rc=1`; the full count-drift verdict
  is compared through `backend_output_comparator` as `run_output`.
- Removed the stdlib-dispatch finding-kind row from
  `test_harness_inventory_paths_owner.pgy`, and tightened the component
  contract against reintroducing shell-owned `count_drift` interpretation.

## 2026-07-06 - Production size over-cap verdicts consume artifact owner

- Added TestHarness-owned expected JSON artifacts for the synthetic production
  `.c` and `.h` over-cap fixtures.
- Repointed `production_c_size_checker_parity.sh` and
  `production_header_size_checker_parity.sh` so shell only handles scratch file
  creation and `rc=1`; the full over-cap verdicts are compared through
  `backend_output_comparator` as `run_output`.
- Removed the production size finding-kind rows from
  `test_harness_size_paths_owner.pgy`, and tightened the component contract
  against reintroducing shell-owned over-cap finding/path interpretation.

## 2026-07-06 - Backend comparator negative verdicts consume artifacts

- Added TestHarness-owned expected JSON artifacts for the comparator's
  mismatch and missing-input self-test fixtures.
- Repointed `backend_output_comparator_parity.sh` so shell still mutates the
  scratch fixtures and checks `rc=1`, but compares the full negative verdicts
  against committed expected artifacts instead of grepping `ok:false`,
  `mismatch_lines`, or finding kinds.
- Removed the comparator finding-kind rows from `test_harness_owner.pgy`, and
  tightened the component contract against reintroducing shell-owned comparator
  negative verdict interpretation.

## 2026-07-06 - TestHarness manifest source consumes path owner projection

- Added `CompilerTestHarnessManifestPath()` to the compiler path manifest and
  shell projection so the bootstrap TestHarness manifest source path is a
  compiler-world fact, not a shared helper literal.
- Repointed `llvm_leg_helpers.sh` so it compiles the TestHarness manifest from
  `PGY_SELFHOST_COMPILER_TEST_HARNESS_MANIFEST_PATH` projected by
  `compiler_world_manifest.sh`.
- Tightened compiler-world and component contracts against reintroducing the old
  direct `test_harness_manifest.pgy` source literal in the shared LLVM helper.

## 2026-07-06 - TestHarness manifest removes completeness forwarding wrappers

- Removed the `EmitSelfHostCompleteness*` forwarding wrappers from
  `test_harness_manifest.pgy`; the manifest dispatch now calls the concrete
  `completeness_ledger_owner.pgy` emitters directly.
- Re-enabled the compiler-world 600-line cap for
  `test_harness_manifest.pgy` after reducing it to 534 lines.
- Tightened the hard self-host contract against reintroducing the wrapper names,
  and verified the real completeness dispatch with `self-host-completeness-smoke`
  over 157 self-hosted sources.

## 2026-07-06 - Inventory and size finding rows split from tool paths

- Split `test_harness_tool_paths_owner.pgy` by responsibility: inventory
  checker suites moved to `test_harness_inventory_paths_owner.pgy`, and
  production size suites moved to `test_harness_size_paths_owner.pgy`.
- Repointed module-manifest, stdlib-dispatch, production-C-size, and
  production-header-size parity runners so their negative finding-kind
  assertions consume TestHarness-projected rows instead of shell literals.
- Tightened the component contract with explicit 600-line caps for these
  TestHarness owners and hardcoded-finding rejection for the four runners.

## 2026-07-06 - Examples inventory drift finding consumes TestHarness owner

- Extended `examples-inventory-paths` with the expected
  `inventory_count_drift` finding kind.
- Repointed `examples_inventory_checker_parity.sh` so shell still creates the
  synthetic missing-example fixture, but the finding-kind expectation comes from
  `test_harness_tool_paths_owner.pgy`.
- Kept this in the repository-authoring guard plane: it prevents future
  LLM-written parity code from owning a TestHarness decision, while
  Fortran-derived parallel evidence remains separate language/projection work.

## 2026-07-06 - Doc-link missing finding consumes TestHarness owner

- Extended `doc-link-checker-paths` with the expected `missing_link` finding
  kind.
- Repointed `doc_link_checker_parity.sh` so shell still rewrites the scratch
  dead-link fixture, but the finding-kind expectation comes from
  `test_harness_tool_paths_owner.pgy`.
- Tightened the component contract against reintroducing the hardcoded
  `missing_link` assertion in shell.

## 2026-07-06 - AST surface growth finding consumes TestHarness owner

- Extended `ast-read-surface-paths` with the expected `surface_growth` finding
  kind.
- Repointed `ast_read_surface_checker_parity.sh` so shell still creates the
  synthetic source-growth fixture, but the finding-kind expectation comes from
  `test_harness_tool_paths_owner.pgy`.
- Tightened the component contract against reintroducing the hardcoded
  `surface_growth` assertion in shell.

## 2026-07-06 - Comparator finding kinds consume TestHarness owner

- Extended `backend-output-comparator-paths` with the expected mismatch and
  missing-input finding kinds.
- Repointed `backend_output_comparator_parity.sh` so shell still creates the
  negative mismatch and missing-input fixtures, but the finding-kind
  expectations come from `test_harness_owner.pgy`.
- Tightened the component contract against reintroducing hardcoded
  `mismatch` / `input_error` finding-kind assertions in shell.

## 2026-07-06 - AIR graph finding kinds consume TestHarness owner

- Extended the five AIR graph consumer path suites with their expected
  negative finding kinds: duplicate ids, node-count mismatch, orphan reachability,
  dangling edge endpoint, and live dangling reference.
- Repointed the five AIR graph consumer parity runners so shell still executes
  the negative fixture, but the finding kind expectation comes from
  `test_harness_air_graph_paths_owner.pgy`.
- Tightened the component contract against reintroducing hardcoded AIR graph
  finding-kind assertions in shell.

## 2026-07-06 - Runtime boundary missing finding consumes TestHarness owner

- Extended `runtime-boundary-paths` with the missing-term finding field and
  expected finding value.
- Repointed `runtime_boundary_checker_parity.sh` so shell still strips the
  TestHarness-owned runtime term, but the `"missing":1` expectation is projected
  by `test_harness_tool_paths_owner.pgy`.
- Tightened the component contract against reintroducing the hardcoded
  `"missing":1` assertion in shell.

## 2026-07-06 - Stable subset missing finding consumes TestHarness owner

- Extended `stable-subset-section-paths` with the missing-section finding field
  and expected finding value.
- Repointed `stable_subset_section_checker_parity.sh` so shell still strips the
  TestHarness-owned section anchor, but the `"missing":1` expectation is
  projected by `test_harness_tool_paths_owner.pgy`.
- Tightened the component contract against reintroducing the hardcoded
  `"missing":1` assertion in shell.

## 2026-07-06 - AIR graph JSON missing-key fixture consumes TestHarness owner

- Extended `air-graph-json-validator-paths` with the missing top-level key
  name, expected finding field, and expected finding value.
- Repointed `air_graph_json_validator_parity.sh` so shell still writes the
  scratch missing-key fixture, but the removed key and expected finding counter
  come from `test_harness_tool_paths_owner.pgy`.
- Tightened the component contract against reintroducing the hardcoded
  `summary` strip pattern or `"missing_keys":1` assertion in shell.

## 2026-07-06 - AIR ref-live corrupt fixture consumes TestHarness owner

- Extended `air-graph-ref-live-paths` with the negative fixture path, reference
  field name, source value, and corrupted target value.
- Repointed `air_graph_ref_live_parity.sh` so shell still writes the scratch
  JSON mutation, but the dangling-reference mutation shape comes from
  `test_harness_air_graph_paths_owner.pgy`.
- Tightened the component contract against reintroducing the hardcoded
  `"boundary":99` mutation in shell.

## 2026-07-06 - AIR node-count corrupt fixture consumes TestHarness owner

- Extended `air-graph-node-count-paths` with the negative fixture path,
  corrupted summary field name, and corrupted summary value.
- Repointed `air_graph_node_count_integrity_parity.sh` so shell still creates
  the mutated JSON file, but the corruption target and value come from the
  Pergyra TestHarness owner.
- Tightened the component contract against reintroducing the hardcoded
  `"evidence_count":99` mutation in shell.

## 2026-07-06 - AST read surface growth fixture consumes TestHarness owner

- Extended `ast-read-surface-paths` with the synthetic growth source path,
  growth source line, and growth ratchet row.
- Repointed `ast_read_surface_checker_parity.sh` so shell creates the scratch
  source-growth fixture from TestHarness-projected rows instead of owning the
  synthetic `source_ast` payload and ratchet line.
- Tightened the component contract against reintroducing the hardcoded
  `source_ast_codegen` growth fixture in shell.

## 2026-07-06 - Stdlib dispatch drift fixture consumes TestHarness owner

- Extended `stdlib-dispatch-inventory-paths` with the synthetic drift fixture's
  strip pattern and deletion count.
- Repointed `stdlib_dispatch_inventory_checker_parity.sh` so shell mutates the
  scratch LLVM dispatch file from TestHarness-projected rows instead of owning
  the `"stdlib ` pattern and `12` deletion count.
- Tightened the component contract against reintroducing the hardcoded awk
  drift policy in shell.

## 2026-07-06 - Production LOC negative fixtures consume TestHarness owner

- Extended `production-c-size-paths` and `production-header-size-paths` with
  synthetic over-cap fixture path and line-count rows.
- Repointed both production size parity runners so shell creates scratch files
  from TestHarness-projected rows instead of owning the synthetic filenames and
  1001/701 line-count boundaries.
- Tightened the component contract against reintroducing shell-owned synthetic
  production LOC fixture constants.

## 2026-07-06 - Doc-link dead-link fixture consumes TestHarness owner

- Extended `doc-link-checker-paths` with the live doc-link target and the
  synthetic missing target used by the dead-link negative fixture.
- Repointed `doc_link_checker_parity.sh` so shell rewrites the scratch
  `INDEX.md` using TestHarness-projected rows instead of hardcoding the broken
  link pair.
- Tightened the component contract against reintroducing the hardcoded
  `100_beta_readiness_checklist.md -> XX_NONEXISTENT_FAKE_DRIFT.md` pair in
  shell.

## 2026-07-06 - Stable subset negative anchor consumes TestHarness owner

- Extended `stable-subset-section-paths` with the missing-section anchor used
  by the negative fixture.
- Repointed `stable_subset_section_checker_parity.sh` so the shell runner
  removes the TestHarness-projected anchor instead of hardcoding the
  `Ownership Stable Subset` section title.
- Tightened the component contract against reintroducing the hardcoded section
  regex in shell.

## 2026-07-06 - Module manifest negative fixtures consume TestHarness owner

- Extended `test_harness_tool_paths_owner.pgy` so
  `module-manifest-resolver-paths` also carries the missing-modules,
  nested-modules, and nested-field negative JSON fixture bodies.
- Repointed `module_manifest_resolver_parity.sh` to write those owner rows into
  scratch input files instead of deriving or authoring negative manifests in
  shell with `sed` or JSON heredocs.
- Tightened the component contract so the old shell-owned negative fixture
  construction cannot return.

## 2026-07-06 - AIR node-count clean oracle uses expected artifact only

- Removed the redundant shell `grep -oE` / `wc` / `awk` clean-count oracle from
  `air_graph_node_count_integrity_parity.sh`.
- Clean `ids`, `declared`, `intents`, `boundaries`, and `evidence` counts now
  have one source of truth: the TestHarness-projected `expected/clean.json`
  compared through `backend_output_comparator`.
- Kept the corrupted-summary negative fixture as the live behavioral check and
  tightened the component ratchet so the shell count oracle cannot reappear.

## 2026-07-06 - AIR ref-live clean oracle uses expected artifact only

- Removed the redundant shell live-reference count oracle from
  `air_graph_ref_live_parity.sh`.
- Clean boundary-reference, intent-reference, and total dangling counts now have
  one source of truth: the TestHarness-projected `expected/clean.json` compared
  through `backend_output_comparator`.
- Kept the corrupted-reference negative fixture as the live behavioral check
  and tightened the component ratchet so the shell dangling-count oracle cannot
  reappear.

## 2026-07-06 - AIR graph validator clean oracle uses expected artifact only

- Removed the redundant shell summary-count and capability/effect residual
  count oracle from `air_graph_json_validator_parity.sh`.
- Clean `intents`, `boundaries`, `evidence`, `drifts`, `effect_sites`,
  `env_effect_sites`, and `missing_keys` counts now have one source of truth:
  the TestHarness-projected `expected/clean.json` compared through
  `backend_output_comparator`.
- Kept the live `pgy --air-json` drift artifact comparison and missing-key
  negative fixture, and tightened the component ratchet so the shell clean
  count oracle cannot reappear.

## 2026-07-06 - Module manifest clean oracle uses expected artifact only

- Removed the redundant shell `grep -c` clean-count oracle from
  `module_manifest_resolver_parity.sh`.
- Clean module, beta-blocker, and stable-subset counts now have one source of
  truth: the TestHarness-projected `expected/clean.json` compared through
  `backend_output_comparator`.
- Updated the module manifest resolver intent contract and component ratchet so
  the shell count oracle cannot reappear in that runner.

## 2026-07-06 - AIR id uniqueness clean oracle uses expected artifact only

- Removed the redundant shell duplicate-id count oracle from
  `air_graph_id_uniqueness_parity.sh`.
- Clean id, distinct-id, and duplicate-id counts now have one source of truth:
  the TestHarness-projected `expected/clean.json` compared through
  `backend_output_comparator`.
- Kept the duplicate-id negative fixture as the live behavioral check and
  tightened the component ratchet so the shell duplicate count cannot reappear.

## 2026-07-06 - AIR reachability clean oracle uses expected artifact only

- Removed the redundant shell node-count oracle from
  `air_graph_reachability_parity.sh`.
- Clean node, reachable, and orphan counts now have one source of truth: the
  TestHarness-projected `expected/clean.json` compared through
  `backend_output_comparator`.
- Kept the orphan-node negative fixture as the live behavioral check and
  tightened the component ratchet so the shell node count cannot reappear.

## 2026-07-06 - AIR ref-integrity clean oracle uses expected artifact only

- Removed the redundant shell set-difference oracle from
  `air_graph_ref_integrity_parity.sh`.
- Clean node, endpoint, and dangling counts now have one source of truth: the
  TestHarness-projected `expected/clean.json` compared through
  `backend_output_comparator`.
- Kept the dangling-endpoint negative fixture as the live behavioral check and
  tightened the component ratchet so the shell set-difference oracle cannot
  reappear.

## 2026-07-06 - Doc-link clean oracle uses expected artifact only

- Removed the redundant shell `grep -oE` / `wc -l` clean-count oracle from
  `doc_link_checker_parity.sh`.
- Clean total-link, markdown-link, and missing-link counts now have one source
  of truth: the TestHarness-projected `expected/clean.json` compared through
  `backend_output_comparator`.
- Updated the doc-link checker intent contract and component ratchet so the
  shell count oracle cannot reappear in that runner.

## 2026-07-06 - Codegen bootstrap breadth rows move behind TestHarness

- Extended `test_harness_codegen_bootstrap_paths_owner.pgy` with
  `codegen-bootstrap-samples` and `codegen-bootstrap-mir-fixtures` suites.
- Repointed `codegen_bootstrap.sh` so the fixed codegen sample list and MIR
  fixture list are read from TestHarness instead of shell string literals.
- Tightened the component contract to reject reintroduced shell-owned bootstrap
  sample or MIR fixture lists.

## 2026-07-06 - Stable subset clean oracle uses expected artifact only

- Removed the redundant shell `grep -c '^## '` clean-count oracle from
  `stable_subset_section_checker_parity.sh`.
- Clean output now has one source of truth: the TestHarness-projected
  `expected/clean.json` compared through `backend_output_comparator`.
- Tightened the component contract so the shell section-count oracle cannot
  reappear in that runner.

## 2026-07-06 - Scratch growth bounded for self-host bootstrap

- Measured local build/test scratch and found `.tmp` dominating disk use, with
  a stale self-host codegen bootstrap compiler log alone occupying hundreds of
  megabytes after a malformed generated C compile.
- C compiler stderr in `codegen_bootstrap.sh` is now captured through a bounded
  evidence log (`PGY_SELFHOST_CC_LOG_LIMIT_BYTES`, default 65536 bytes) while
  still consuming the full compiler stream.
- `make clean-scratch` now resets the whole ignored `.tmp` scratch zone instead
  of naming a partial list of known self-host/backend-compare subdirectories.

## 2026-07-06 - Compiler-world shell projection checked against Pergyra owner

- Added a `compiler-world-paths` projection to `path_manifest_owner.pgy` and
  exposed it through the Pergyra TestHarness manifest.
- Tightened `self-host-compiler-world-contract-test-smoke` so the shell
  `compiler_world_manifest.sh` projection is sorted and compared against the
  Pergyra-owned projection before the compiler-world AST shape checks run.
- Repointed the preparation smoke's semantic parity assertion from the old
  copied-source glob to `semantic-parity-paths` plus the manifest-projected
  semantic source directory.
- Updated the compiler-world docs to state that shell path rows are a checked
  projection, not an unchecked second source of truth.

## 2026-07-06 - Scratch cleanup target owns local artifact reset

- Added `make clean-scratch` for local self-host/backend-compare scratch reset.
- The target now removes the ignored `.tmp` scratch zone; ordinary `clean`
  remains limited to `BUILD_DIR` and `BIN_DIR`.
- Tightened the component contract so the cleanup surface stays named in the
  Makefile instead of living as an ad hoc local deletion recipe.

## 2026-07-06 - Coverage probes consume path owners

- Repointed `lexer_scale_probe.sh` and `parser_scale_probe.sh` so their tool
  source and backend comparator source come from `lexer-parity-paths` and
  `parser-parity-paths` instead of hardcoded self-host source constants.
- Repointed `mir_json_coverage_probe.sh` so its mir-lower and codegen tool
  sources come from `mir-json-parity-paths` instead of direct stage paths.
- Added bounded probe execution: lexer/parser scale probes default to
  `PGY_SCALE_PROBE_LIMIT=20` and require `--full` or
  `PGY_SCALE_PROBE_LIMIT=0` for the historical full corpus; MIR coverage accepts
  `PGY_MIR_COVERAGE_LIMIT` for quick source-owner wiring checks.
- Tightened the component contract so these coverage probes cannot become
  separate stage-source path owners again, and so the default scale probes do
  not accidentally behave like long-running corpus campaigns.

## 2026-07-06 - Semantic expected regen uses path owner

- Repointed `regen_expected.sh` so expected diagnostic regeneration consumes
  `semantic-parity-paths` from the TestHarness manifest before compiling.
- Removed the support script's build-dir `main.pgy` alias and copied
  `.tmp/self_hosted/lib` tree. Regeneration now compiles the manifest-projected
  semantic source in place and writes the manifest-projected expected directory.
- Tightened the component contract so the fixture regeneration path cannot
  reintroduce hardcoded semantic source/fixture/expected paths or local lib
  copies.

## 2026-07-06 - Semantic parity uses source owner in place

- Repointed `semantic_parity.sh` so the `semantic-parity-paths` TestHarness
  source row is passed to the compiler directly.
- Removed the build-dir `main.pgy` alias, copied semantic owner files, and
  copied `.tmp/self_hosted/lib` tree from the parity runner; fixture manifest
  emission still comes from the compiled semantic owner.
- Tightened the component contract so the local semantic/lib source-copy paths
  cannot return to this runner.

## 2026-07-06 - Lexer/parser scale probes use source owners in place

- Repointed `lexer_scale_probe.sh` and `parser_scale_probe.sh` so their
  self-hosted tool sources are passed to the compiler directly.
- Removed the build-dir `main.pgy` aliases plus local lexer/parser/lib source
  copies from those coverage probes. They remain probes, not parity gates, but
  they no longer manufacture shadow source trees before compiling.
- Tightened the component contract so these scale probes cannot reintroduce
  copied source aliases.

## 2026-07-06 - Completeness ledger uses stage source owners

- Repointed `completeness_ledger.sh` so lexer, parser, semantic, and codegen
  stage tools are read from TestHarness path-owner rows before compilation.
- Removed the ledger's local lexer/parser/semantic source copies and shared
  `lib` tree copies. The M2 ledger now compiles each stage from its
  source-owner location instead of a build-dir source tree.
- Tightened the component contract so copied source/lib stage aliases cannot
  return to the completeness ledger.

## 2026-07-06 - Parser parity uses source owner in place

- Repointed `parser_parity.sh` so the `parser-parity-paths` TestHarness source
  row is passed to the compiler directly.
- Removed the build-dir `main.pgy` alias, copied parser owner files, and copied
  `.tmp/self_hosted/lib` tree from the parity runner; fixture manifest emission
  still comes from the compiled parser owner.
- Tightened the component contract so the local parser/lib source-copy paths
  cannot return to this runner.

## 2026-07-06 - Lexer parity uses source owner in place

- Repointed `lexer_parity.sh` so the `lexer-parity-paths` TestHarness source
  row is passed to the compiler directly.
- Removed the build-dir `main.pgy` alias and copied lexer owner files from the
  parity runner; fixture manifest emission still comes from the compiled lexer
  owner.
- Tightened the component contract so the local lexer source-copy path cannot
  return to this runner.

## 2026-07-06 - AIR graph validator parity uses source owner in place

- Repointed `air_graph_json_validator_parity.sh` so the
  `air-graph-json-validator-paths` TestHarness source row is passed to the
  compiler directly.
- Removed the build-dir `main.pgy` alias, copied validator-owner files, copied
  `lib` tree, and copied `compiler/air_evidence_owner.pgy` shim from the parity
  runner; the validator now resolves sibling owners, shared libs, and AIR
  evidence from its manifest-projected source location.
- Tightened the component contract so the local source/lib/compiler copy paths
  cannot return to this runner.

## 2026-07-06 - Diagnostic catalog parity uses source owner in place

- Repointed `diagnostic_catalog_checker_parity.sh` so the
  `diagnostic-catalog-paths` TestHarness source row is passed to the compiler
  directly.
- Removed the build-dir `main.pgy` alias, copied checker-owner files, and
  copied `lib` tree from the parity runner; the checker now resolves sibling
  owners and shared libs from its manifest-projected source location.
- Tightened the component contract so the local source/lib copy paths cannot
  return to this runner.

## 2026-07-06 - AST read surface parity uses source owner in place

- Repointed `ast_read_surface_checker_parity.sh` so the
  `ast-read-surface-paths` TestHarness source row is passed to the compiler
  directly.
- Removed the build-dir `main.pgy` alias and copied `lib` tree from the parity
  runner; the checker now resolves imports from its manifest-projected source
  location.
- Tightened the component contract so the local source/lib copy path cannot
  return to this runner.

## 2026-07-06 - AIR graph consumers use source owners in place

- Repointed the AIR graph id-uniqueness, node-count, reachability,
  referential-integrity, and live-reference parity runners so their TestHarness
  source rows are passed to the compiler directly.
- Removed the build-dir `main.pgy` aliases, copied `scan_owner.pgy`, and copied
  `lib` trees from those five runners; each consumer now resolves
  `../air_graph_json_validator/scan_owner.pgy` and shared libs from the
  manifest-projected source location.
- Tightened the component contract so the local source/scan-owner/lib copy paths
  cannot return to these consumer runners.

## 2026-07-06 - Runtime boundary missing-term fixture consumes TestHarness owner

- Added TestHarness-owned rows for the runtime-boundary missing-term fixture
  path and term.
- Repointed `runtime_boundary_checker_parity.sh` so shell verifies that row
  against the checker `--terms` manifest and only performs scratch mutation.
- Recorded the plane split explicitly: this slice is a repository/agent
  boundary guard, while Fortran-derived parallel evidence remains a separate
  language semantics and projection axis.

## 2026-07-06 - Module manifest and stdlib dispatch parity use source owners

- Repointed `module_manifest_resolver_parity.sh` and
  `stdlib_dispatch_inventory_checker_parity.sh` so their TestHarness source rows
  are passed to the compiler directly.
- Removed the build-dir `main.pgy` aliases and copied `lib` trees from both
  parity runners; imports now resolve from the manifest-projected source
  locations.
- Tightened the component contract so the local source/lib copy paths cannot
  return to these runners.

## 2026-07-06 - Linter and runtime-boundary parity use source owners in place

- Repointed `linter_parity.sh` and `runtime_boundary_checker_parity.sh` so their
  TestHarness source rows are passed to the compiler directly.
- Removed the build-dir `main.pgy` aliases from both parity runners; these
  single-source tools no longer create shell-owned shadow source files before
  compilation.
- Tightened the component contract so the local source-copy paths cannot return
  to these runners.

## 2026-07-06 - Production size parity uses source owners in place

- Repointed `production_c_size_checker_parity.sh` and
  `production_header_size_checker_parity.sh` so their TestHarness source rows
  are passed to the compiler directly.
- Removed the build-dir `main.pgy` aliases and copied `lib` trees from both
  parity runners; each checker now resolves imports from its manifest-projected
  source location.
- Tightened the component contract so the local source/lib copy paths cannot
  return to these runners.

## 2026-07-06 - Examples inventory parity uses source owner in place

- Repointed `examples_inventory_checker_parity.sh` so the
  `examples-inventory-paths` TestHarness source row is passed to the compiler
  directly.
- Removed the build-dir `main.pgy` alias and copied `lib` tree from the parity
  runner; the checker now resolves imports from its manifest-projected source
  location.
- Tightened the component contract so the local source/lib copy path cannot
  return to this runner.

## 2026-07-06 - Doc link checker parity uses source owner in place

- Repointed `doc_link_checker_parity.sh` so the `doc-link-checker-paths`
  TestHarness source row is passed to the compiler directly.
- Removed the build-dir `main.pgy` alias and copied `lib` tree from the parity
  runner; imports now resolve from the manifest-projected checker source
  location.
- Tightened the component contract so the local source/lib copy path cannot
  return to this runner.

## 2026-07-06 - Stable subset checker parity uses source owner in place

- Repointed `stable_subset_section_checker_parity.sh` so the
  `stable-subset-section-paths` TestHarness source row is passed to the
  compiler directly.
- Removed the build-dir `main.pgy` alias and copied `lib` tree from the parity
  runner; the tool now resolves imports from its manifest-projected source
  location.
- Tightened the component contract so the local source/lib copy path cannot
  return to this runner.

## 2026-07-06 - Backend comparator parity uses source owner in place

- Repointed `backend_output_comparator_parity.sh` so the comparator source row
  from the `backend-output-comparator-paths` TestHarness suite is the compiler
  input directly.
- Removed the build-dir `main.pgy` alias and the copied `lib`/`compiler` owner
  tree from this parity runner; imports now resolve from the manifest-projected
  source location instead of a local shell-created shadow tree.
- Tightened the component contract so the old local source/dependency copy path
  cannot return to the comparator parity runner.

## 2026-07-06 - Shared comparator helper consumes TestHarness source row

- Repointed `llvm_leg_helpers.sh` so
  `pgy_selfhost_compile_backend_output_comparator` no longer defaults to a
  direct `src/self_hosted/tools/backend_output_comparator/main.pgy` source path.
  When callers omit the source, the helper reads the first row from the
  `backend-output-comparator-paths` TestHarness manifest suite.
- Kept explicit comparator-source arguments valid for runners that already read
  a dedicated suite; the default path is now owner-projected rather than
  shell-owned.
- Tightened the component contract so the old direct comparator default cannot
  return to the shared LLVM/artifact helper.

## 2026-07-06 - Completeness ledger consumes TestHarness codegen source

- Repointed `completeness_ledger.sh` so the codegen stage reads the codegen tool
  source from the `codegen-parity-paths` manifest suite instead of owning
  `src/self_hosted/codegen/main.pgy` locally.
- Kept the completeness owner as the source for source/stage/baseline rows; this
  change only closes the remaining codegen tool-source identity seam in the
  runner.
- Tightened the component contract so the direct codegen source literal cannot
  return to the completeness ledger.

## 2026-07-06 - Backend tri-compare consumes shared TestHarness/Artifact owners

- Repointed `backend_output_tri_compare_parity.sh` so the runner compiles the
  TestHarness manifest through the shared self-host helper, reads smoke/extended
  case suites from that manifest, and reads the backend-output comparator source
  from the `backend-output-comparator-paths` suite instead of owning a local
  source path.
- Removed the per-case temporary comparator source/lib/compiler copy. The
  runner now compiles the Pergyra comparator once through the shared
  ArtifactZone/TestHarness helper and reuses that binary for all C/LLVM
  stdout/stderr comparisons.
- Tightened the component contract so the old direct manifest source and
  comparator copy path cannot return to the tri-compare parity runner.

## 2026-07-06 - Real-source semantic selfcheck consumes completeness owner

- Repointed `tests/self_hosted/parity/selfcheck_sources.sh` away from its
  shell-owned `SELF_SOURCES` array. The runner now reads `semantic-parity-paths`
  for the semantic checker source and `self-host-completeness-semantic-targets`
  for the source-to-semantic-target rows from `test_harness_manifest.pgy`.
- Tightened `completeness_ledger_owner.pgy` monotone minima from 148 to 154
  after `self-host-completeness-smoke` proved 154/154 lexer, parser, semantic,
  codegen, lex+parse, lex+parse+semantic, and full-pipeline stage checks.
- Updated the component contract so direct selfcheck source arrays and direct
  semantic tool-source constants cannot return.

## 2026-07-06 - LSP parity paths consume dedicated TestHarness owner

- Added `test_harness_lsp_paths_owner.pgy` for LSP diagnostics, transport,
  request, response, session, document-store, session-state, and hover-content
  path suites.
- Repointed the nine LSP parity runners so shell consumes
  `test_harness_manifest.pgy` rows instead of owning the LSP tool source or
  expected-artifact path constants.
- Moved the stateful LSP document-store, session-state, and hover-content
  JSON-RPC request bodies into the same owner rows, so parity shell can frame
  requests but cannot own the fixture body semantics.
- Raised the M2 completeness minima to 155 so the new owner is included in the
  hard self-host production-source ratchet.

## 2026-07-06 - Fuzz generator parity consumes TestHarness source row

- Added the `fuzz-backend-generator-paths` manifest suite to
  `test_harness_codegen_bootstrap_paths_owner.pgy`, reusing the existing
  fuzz-generator source path fact instead of introducing another alias.
- Repointed `fuzz_backend_parity_generator_parity.sh` so shell compiles the
  generator source from `test_harness_manifest.pgy` rather than owning the
  `src/self_hosted/fuzz/backend_parity_generator/main.pgy` literal.
- Tightened the component contract so the direct generator source path cannot
  return to the parity runner.

## 2026-07-06 - MIR JSON parity paths consume dedicated TestHarness owner

- Added `test_harness_mir_json_paths_owner.pgy` for the
  `mir-json-parity-paths` suite: mir-lower tool source, codegen tool source,
  and backend comparator source. The codegen/comparator rows reuse the codegen
  path owner facts so those path strings do not fork.
- Repointed `mir_json_parity.sh` so shell reads those paths from
  `test_harness_manifest.pgy`; the compiled `mir_lower` owner remains the
  source of the 86-row fixture manifest through `--fixture-manifest`.
- Tightened the component contract so direct MIR-lower/codegen path constants
  cannot return to the MIR JSON parity runner.

## 2026-07-06 - Codegen bootstrap breadth consumes TestHarness owner rows

- Added `test_harness_codegen_bootstrap_paths_owner.pgy` for the
  `codegen-bootstrap-paths`, `codegen-bootstrap-components`, and
  `codegen-bootstrap-tools` suites.
- Repointed `codegen_bootstrap.sh` so the fixed-point runner consumes codegen,
  parser, comparator, mir-lower, fixture, fuzz-generator, sample-source, and
  breadth rows from `test_harness_manifest.pgy` instead of constructing
  `src/self_hosted/.../main.pgy` paths in shell.
- Kept codegen/comparator/mir-lower aliases unified by reusing the existing
  codegen and MIR JSON path owner facts.

## 2026-07-06 - Semantic parity paths consume dedicated TestHarness owner

- Added `test_harness_semantic_paths_owner.pgy` for the
  `semantic-parity-paths` suite: semantic tool source, backend comparator
  source, fixture directory, expected diagnostic directory, diagnostic code
  owner, diagnostic renderer owner, and semantic source directory.
- Repointed `semantic_parity.sh` so shell reads those paths from
  `test_harness_manifest.pgy`; the compiled semantic owner remains the source
  of the 108-row fixture/status manifest through `--fixture-manifest`.
- Kept the diagnostic code vocabulary checks in the runner, but moved their
  file inputs behind the same TestHarness path suite.

## 2026-07-06 - Parser parity paths consume dedicated TestHarness owner

- Added `test_harness_parser_paths_owner.pgy` for the `parser-parity-paths`
  suite: parser tool source, backend comparator source, fixture directory, and
  expected clean fixture path.
- Repointed `parser_parity.sh` so shell reads those paths from
  `test_harness_manifest.pgy`; the compiled parser owner remains the source of
  the 188-row source/fixture manifest through `--fixture-manifest`.
- Let the shared backend-output comparator compile helper accept an explicit
  comparator source path while preserving the old default for existing parity
  scripts.

## 2026-07-06 - Codegen parity paths consume dedicated TestHarness owner

- Added `test_harness_codegen_paths_owner.pgy` for the `codegen-parity-paths`
  suite so the codegen tool, parser AST producer, backend comparator, fixture
  directory, and expected-output directory do not grow the generic tool-path
  owner past the 600-line cap.
- Repointed `codegen_parity.sh` so shell reads those paths from
  `test_harness_manifest.pgy`; the compiled codegen run owner remains the
  source of fixture/expected row inventory through `--fixture-manifest`.
- Tightened the component contract so direct codegen/parser/comparator path
  constants and fixture directory constants cannot return to the codegen parity
  runner.

## 2026-07-06 - Driver parity tool paths consume TestHarness owner

- Added `test_harness_driver_paths_owner.pgy` for DRV-0/DRV-1 driver, parser,
  and codegen source path suites instead of extending the already-large generic
  tool-path owner.
- Repointed `driver_rung0_parity.sh` and `driver_rung1_parity.sh` so shell
  reads those tool paths from `test_harness_manifest.pgy`; fixture inventories
  remain owned by the compiled driver fixture manifest.
- Tightened the component contract so direct driver/parser/codegen path
  constants cannot return to the driver parity runners.

## 2026-07-06 - SEA value-capture producer enters self-host mirror

- Added the conservative MIR value-capture producer mirror to
  `src/self_hosted/sea/execution_lane.pgy`: a parallel boundary with movability
  evidence and no raw slot/live view/raw channel/zone-pin/MIR pin evidence is
  promoted to value-only; resource captures remain pinned/rejected.
- Extended the self-host execution-lane parity golden from 29 to 31 rows so C,
  LLVM, and the Pergyra mirror all cover the value-only producer positive case
  and the raw-slot negative case.

## 2026-07-05 - Likeness ratchet scopes compiler-core text munging

- Split the Pergyra-likeness text-munging metric into blocking
  `core_string_munge_sig` and informational `total_string_munge_sig`.
- Locked the compiler-core baseline at `core_string_munge_sig=116`, while the
  broad tracked surface remains visible as `total_string_munge_sig=166`.
- Kept real core debt in scope (`expr_rewrite`, `decl_lower`, `routine_lower`)
  and excluded tools/LSP/fuzz/path/fixture/harness routing from the core
  linchpin metric.
- Tightened `result_use` from 562 to 563 after the completeness-ledger owner
  added another typed absence/result surface.

## 2026-07-05 - M2 completeness ledger lands

- Added `CompilerCompletenessLedger` as the self-host owner for production source
  inventory scope, stage names, and monotone baseline minima.
- Added `self-host-completeness-smoke`, wired into self-host preparation parity,
  to count lexer/parser/semantic/codegen `--check` results over production
  `src/self_hosted/**/*.pgy` excluding `fixture/` and `expected/`.
- Locked the first measured baseline: sources 147, lexer 147, parser 43,
  semantic 134, codegen 23. Codegen out-of-subset is now a measured fail count,
  not a quiet skip.

## 2026-07-05 - Lexer parity paths consume TestHarness owner

- Added a `lexer-parity-paths` manifest suite to `test_harness_tool_paths_owner.pgy`
  for the lexer source, backend comparator source, and lexer fixture directory.
- Repointed `lexer_parity.sh` so shell reads those path facts from the compiled
  TestHarness manifest before compiling the lexer and comparator.
- Left fixture row ownership in the compiled lexer owner; shell still executes
  the parity loop but no longer owns the lexer path constants.

## 2026-07-05 - Backend comparator self-test paths consume TestHarness owner

- Added a `backend-output-comparator-paths` manifest suite owned by
  `test_harness_owner.pgy`, because the comparator's default comparable
  artifact paths are core TestHarness facts rather than generic tool-path rows.
- Repointed `backend_output_comparator_parity.sh` so shell reads the comparator
  source, expected clean JSON, and expected/actual comparable artifacts from the
  compiled TestHarness manifest before running clean, argv, mismatch, missing,
  and LLVM parity legs.
- At the time, shell still acted as the comparator self-test's text-equivalence
  oracle while path constants moved behind TestHarness. This exception was
  later retired by the 2026-07-06 Backend Output Comparator entry below.

## 2026-07-05 - AIR graph consumer paths consume TestHarness owner

- Added `test_harness_air_graph_paths_owner.pgy` to own the AIR graph
  id-uniqueness, node-count, reachability, ref-integrity, and ref-live path
  suites outside the generic tool-path owner.
- Repointed the five AIR graph consumer parity scripts so shell reads tool
  source, shared scan owner, expected JSON, and fixture paths from
  `test_harness_manifest.pgy`; clean and negative runs pass fixture paths into
  compiled checkers through `Args()[0]`.
- Promoted the new path owner into real-source semantic selfcheck, raising the
  accepted self-host owner/source count to 132.

## 2026-07-05 - MIR JSON parity fixture inventory consumes mir_lower owner

- Added `fixture_manifest_owner.pgy` to own the 86 positive MIR parity source
  fixture rows spanning mir_lower fixtures, supported codegen fixtures, and the
  binary-search example.
- Added `run_owner.pgy` so the `mir_lower` entrypoint stays entrypoint-only,
  then exposed the manifest through that run owner as `--fixture-manifest`.
- Repointed `mir_json_parity.sh` so shell no longer owns `MIR_FIXTURES`,
  `CODEGEN_FIXTURES`, or `EXAMPLE_FIXTURES`; it reads the compiled mir_lower
  owner's manifest before invoking `pgy --mir-json`, `mir_lower`, `codegen`,
  and the C oracle.

## 2026-07-05 - Codegen parity fixture inventory consumes codegen owner

- Added codegen fixture manifest ownership to `run/codegen_run_owner.pgy`: the
  owner walks `src/self_hosted/codegen/fixture`, checks each paired
  `expected/*_stdout.txt`, and emits fixture base rows.
- Exposed the manifest through `RunCodegenFromArgs --fixture-manifest`.
- Repointed `codegen_parity.sh` so shell no longer owns the 65-row `FIXTURES`
  list; it compiles the codegen owner, reads that manifest, and then runs the C
  oracle plus C/LLVM-built codegen parity over the manifest rows.

## 2026-07-05 - Parser parity fixture inventory consumes parser owner

- Added `fixture_manifest_owner.pgy` to own the 186 parser source/fixture
  parity rows.
- Added `run_owner.pgy` so parser CLI mode selection lives outside the
  entrypoint, and exposed the manifest through `--fixture-manifest`.
- Repointed `parser_parity.sh` so shell no longer owns `SOURCE_PAIRS`; it reads
  the compiled parser owner's manifest before running C/LLVM parser parity and
  live `pgy --ast` drift checks.

## 2026-07-05 - Driver parity fixture inventory consumes driver owner

- Added driver fixture manifest ownership to `driver_rung0_owner.pgy` for the
  three DRV artifact fixtures.
- Exposed the manifest through DRV-0 and DRV-1 entrypoints as
  `--fixture-manifest`.
- Repointed `driver_rung0_parity.sh` and `driver_rung1_parity.sh` so shell no
  longer owns `FIXTURES`; both runners read the compiled driver owner's
  manifest before checking AST text and emitted C artifacts.

## 2026-07-05 - Lexer parity fixture inventory consumes lexer owner

- Added `fixture_manifest_owner.pgy` to own the seven lexer source/fixture
  parity rows.
- Added `run_owner.pgy` so CLI mode selection lives outside the entrypoint, and
  exposed the manifest through `--fixture-manifest`.
- Repointed `lexer_parity.sh` so shell no longer owns `SOURCE_PAIRS`; it reads
  the compiled lexer owner's manifest before running C/LLVM lexer parity and
  live `pgy --tokens` drift checks.

## 2026-07-05 - Semantic parity fixture inventory consumes semantic owner

- Added semantic fixture manifest ownership to `diagnostic_owner.pgy`: the
  owner walks `src/self_hosted/semantic/fixture`, reads each paired
  `expected/*.diag` status, and emits `name:ok|error` rows.
- Exposed the manifest through `semantic_run_owner.pgy` as
  `--fixture-manifest`.
- Repointed `semantic_parity.sh` so shell no longer owns the 108-row
  `SOURCE_PAIRS` list; it compiles the semantic owner, reads that manifest, and
  then runs the C oracle plus C/LLVM-built semantic verdict parity over the
  manifest rows.

## 2026-07-05 - Stdlib dispatch inventory paths consume TestHarness owner

- Added stdlib dispatch inventory path facts to
  `test_harness_tool_paths_owner.pgy`: checker source, expected clean JSON,
  C scalar dispatch, C unary dispatch, and LLVM scalar/IO dispatch.
- Extended `test_harness_manifest.pgy` with the
  `stdlib-dispatch-inventory-paths` suite.
- Repointed `stdlib_dispatch_inventory_checker_parity.sh` so shell reads those
  paths from the manifest and runs the compiled C checker binary for clean and
  dispatch-drift fixtures before the LLVM parity leg.

## 2026-07-05 - AST read surface checker paths consume TestHarness owner

- Added AST read surface checker path facts to
  `test_harness_tool_paths_owner.pgy`: tool source, expected clean JSON, and
  `tests/ast_read_surface_ratchet.txt`.
- Extended `test_harness_manifest.pgy` with the `ast-read-surface-paths` suite.
- Repointed `ast_read_surface_checker_parity.sh` so shell reads tool/expected
  paths and the ratchet path from the manifest, then runs the compiled C
  checker binary for clean and growth fixtures before the LLVM parity leg.

## 2026-07-05 - Production size checker paths consume TestHarness owner

- Added production C/header size checker path facts to
  `test_harness_tool_paths_owner.pgy`: tool source and expected clean JSON for
  each checker.
- Extended `test_harness_manifest.pgy` with `production-c-size-paths` and
  `production-header-size-paths` suites.
- Repointed `production_c_size_checker_parity.sh` and
  `production_header_size_checker_parity.sh` so shell reads tool/expected paths
  from the manifest and runs compiled C checker binaries for clean and
  over-cap fixtures.

## 2026-07-05 - Runtime boundary checker terms consume Pergyra owner

- Added runtime boundary path facts to `test_harness_owner.pgy`: tool source
  and expected clean JSON.
- Extended `test_harness_manifest.pgy` with the `runtime-boundary-paths` suite.
- Added `--terms` manifest mode to `runtime_boundary_checker`, so the checker
  owns the required `(path, term)` rows it enforces.
- Repointed `runtime_boundary_checker_parity.sh` so shell reads tool/expected
  paths from `TestHarnessZone` and reads required-term rows from the compiled
  Pergyra checker instead of carrying a duplicate shell array.

## 2026-07-05 - Doc link checker paths consume TestHarness owner

- Added doc-link checker path facts to `test_harness_owner.pgy`: tool source,
  expected clean JSON, and `docs/INDEX.md`.
- Extended `test_harness_manifest.pgy` with the `doc-link-checker-paths` suite.
- Repointed `doc_link_checker_parity.sh` so shell reads those paths from
  `TestHarnessZone`, compiles the checker through the C backend, and passes the
  index path into both C and LLVM parity legs.
- Updated `doc_link_checker` to consume `Args()[0]` for the index path so the
  TestHarness path fact reaches the tool boundary instead of remaining a shell
  constant.

## 2026-07-05 - TestHarness tool paths split and examples inventory paths consume owner

- Split concrete parity tool/input path suites out of `test_harness_owner.pgy`
  into `test_harness_tool_paths_owner.pgy`, keeping the core TestHarness owner
  focused on row/projection/artifact vocabulary and readiness composition.
- Added examples-inventory path facts to the new owner: tool source and
  expected clean JSON.
- Extended `test_harness_manifest.pgy` with the `examples-inventory-paths`
  suite.
- Repointed `examples_inventory_checker_parity.sh` so shell reads tool/expected
  paths from the manifest, runs the compiled C checker for clean and drift
  fixtures, and keeps the LLVM leg on the same Pergyra-origin tool.
- Added the new path owner to the real-source selfcheck list so the self-hosted
  subset checks the owner it now relies on.

## 2026-07-05 - Linter parity paths consume TestHarness owner

- Added linter parity path facts to `test_harness_owner.pgy`: tool source,
  expected diagnostics, and fixture path.
- Extended `test_harness_manifest.pgy` with the `linter-parity-paths` suite.
- Added a shared manifest reader to `llvm_leg_helpers.sh` so parity scripts can
  consume `TestHarnessZone` path projections without each owning local shell
  constants.
- Repointed `linter_parity.sh` to read those paths from the manifest and pass
  the fixture path into the compiled linter through `Args()[0]`.

## 2026-07-05 - Module manifest resolver paths consume TestHarness owner

- Added module manifest resolver path facts to `test_harness_owner.pgy`: tool
  source, expected JSON, and input manifest path.
- Extended `test_harness_manifest.pgy` with the
  `module-manifest-resolver-paths` suite.
- Repointed `module_manifest_resolver_parity.sh` to read those paths from the
  manifest and run the compiled resolver with the manifest path as `Args()[0]`.
- Extended the shared LLVM-leg parity helper so C/LLVM tool runs preserve
  runtime arguments when a harness-owned input path must flow into the tool.

## 2026-07-05 - Stable subset checker paths consume TestHarness owner

- Added stable subset section checker path facts to `test_harness_owner.pgy`:
  tool source, expected JSON, and input document path.
- Extended `test_harness_manifest.pgy` with the
  `stable-subset-section-paths` suite.
- Repointed `stable_subset_section_checker_parity.sh` to read those paths from
  the manifest and run the compiled checker with the stable-subset document
  path as `Args()[0]`.
- Updated the checker report source field so the manifest owner reflects the
  actual input path consumed at runtime.

## 2026-07-05 - ArtifactZone promoted to ready

- Reclassified `Artifact Zone evidence` from ACTIVE to READY in
  `15_pre_self_host_expansion_ledger.md`.
- At the time, the measured remaining comparator self-test exception under
  `tests/self_hosted/parity` was `backend_output_comparator_parity.sh`; that
  shell text-equivalence exception was later retired by the 2026-07-06 Backend
  Output Comparator entry below.
- Kept `TestHarnessZone` ACTIVE because shell parity scripts are still the
  primary harness owner; only the artifact equality verdict surface is ready.
- Tightened `self_hosted_component_contract_smoke.sh` so the ledger must keep
  the READY row, the comparator self-test exception, and the updated work-order
  wording.

## 2026-07-05 - Backend tri-compare suite uses TestHarness owner

- Added backend tri-compare smoke/extended case-suite facts to
  `test_harness_owner.pgy`.
- Added `test_harness_manifest.pgy` as a Pergyra projection over those facts.
- Repointed `backend_output_tri_compare_parity.sh` so the default smoke and
  extended case lists come from the manifest instead of shell-owned arrays.
- Kept explicit CLI case arguments as an override for one-off debugging; the
  default CI suite now consumes `TestHarnessZone` for case selection.

## 2026-07-05 - MIR JSON run-output parity consumes ArtifactZone verdicts

- Repointed `tests/self_hosted/parity/mir_json_parity.sh` so the final
  `pgy --mir-json | mir_lower | codegen | gcc` run-output comparison against
  the C oracle is a `run_output` artifact comparison through the Pergyra
  `backend_output_comparator` owner.
- The MIR JSON gate still owns MIR fact-presence checks, reconstructed AST
  shape checks, generated-C compile checks, and C-oracle execution; the final
  run-output verdict no longer belongs to shell string equality or `diff <(...)`.
- Tightened `self_hosted_component_contract_smoke.sh` so the MIR JSON parity
  gate must keep the comparator owner path and cannot reintroduce `diff <(...)`.

## 2026-07-05 - Semantic verdict parity consumes ArtifactZone verdicts

- Repointed `tests/self_hosted/parity/semantic_parity.sh` so expected
  diagnostic verdicts and C/LLVM-built semantic checker output compare as
  `diagnostics` artifacts through the Pergyra `backend_output_comparator`
  owner.
- The semantic parity harness still keeps its semantic-specific shape checks
  (`Diagnostic: pgy.selfhost.semantic.v1`, `Status`, `Code`) and C-oracle
  diagnostic-code mapping, but the final expected-vs-actual verdict is no
  longer a shell string/diff decision.
- Tightened `self_hosted_component_contract_smoke.sh` so semantic parity must
  keep the comparator owner path and cannot reintroduce `diff <(...)`.
- Verified `self-host-component-contract-test-smoke` and
  `self-host-semantic-parity-test-smoke`; the semantic rung passed 108 fixtures
  through both C and LLVM.

## 2026-07-05 - Parser AST parity consumes ArtifactZone verdicts

- Repointed `tests/self_hosted/parity/parser_parity.sh` so committed AST fixture
  drift checks and self-host parser backend output checks compare `ast_text`
  artifacts through the Pergyra `backend_output_comparator` owner.
- The parser parity harness now builds the comparator once per run and feeds
  normalized expected/actual artifact files to it, instead of letting shell
  string comparison and `diff` own the final verdict.
- Tightened `self_hosted_component_contract_smoke.sh` so parser parity must keep
  the comparator owner path and cannot reintroduce the old local `diff <(...)`
  / `BYTE-DRIFT` verdict.

## 2026-07-05 - Self-host generated C consumes secure host-IO open fact

- Added `HostIORuntimeCSecureFileOpenFn()` to the self-host host-IO runtime ABI
  owner and repointed generated standalone C helpers for `ReadFile`,
  `WriteFile`, and handle-based `FileOpen` through that owner fact.
- The emitted POSIX helper now opens files with `O_NOFOLLOW` at open time and
  then `fdopen`s the descriptor. Windows keeps the existing `fopen` path, so
  this is a POSIX hardening slice rather than a Windows atomic-open claim.
- Tightened `self_hosted_component_contract_smoke.sh` so `program_emit.pgy`
  must consume the secure-open owner and must not reintroduce direct
  `fopen(path, mode)` / `fopen(path, "wb")` write paths.
- Verified `self-host-component-contract-test-smoke` and
  `self-host-codegen-parity-test-smoke`; the codegen parity gate passed 65
  fixtures on codegen tools built through both C and LLVM.
- Also compiled the emitted `file_handle` and `write_file` C artifacts under
  WSL gcc with POSIX feature macros and probed symlink targets; both generated
  executables left the outside file unchanged.
- Follow-up gate tightening: `codegen_parity.sh` now runs that symlink
  nofollow executable probe for generated `write_file` and `file_handle`
  artifacts on POSIX platforms, and the component contract requires the probe
  call so the behavioral check cannot silently fall out of the parity rung.

## 2026-07-05 - LSP hover owner is load-bearing and likeness ratchet tightened

- Made the LSP-2i hover-content owner visible to the driver/LSP rung ladder,
  component contract, owner manifest, LSP intent docs, and progress notes so it
  is a real self-hosted component rung rather than an orphaned parity script.
- Verified the hover-content rung with the driver/LSP wiring smoke, component
  contract smoke, and C/LLVM hover-content parity gate.
- Re-ran the self-host preparation contract layer. The contract gate reported
  `result_use=562`, above the previous `488` floor, so the Pergyra-likeness
  ratchet now requires at least 562 Result/Option/try-use markers.
- Verified `self-host-pergyra-likeness-test-smoke` after tightening the
  baseline. The metric now passes without the "tighten the baseline" warning.

## 2026-07-04 - Let row facts consume typed row input

- Added `CodegenAstTextRowFactInput` as the typed input contract for
  `ast_text_row_fact_owner.pgy`, so name/type fact derivation consumes one row
  record instead of a loose `(kind, payload)` pair.
- Moved `Let` name/type rows behind the row-fact owner during inventory
  construction. `ast_text_statement_owner.pgy` now reads `node.name` and
  `node.type_name` for `Let` facts and only keeps initializer payload slicing
  in the statement owner.
- Tightened `self_hosted_component_contract_smoke.sh` against regressing to
  `CodegenAstTextNameFactFor(kind, payload)` /
  `CodegenAstTextTypeNameFactFor(kind, payload)` and against reintroducing the
  old `Let` local split helpers.
- Kept the Pergyra-likeness ratchet closed by holding `string_munge_sig` at
  156 and tightening `result_use` from 278 to 280.
- Verified `self-host-component-contract-test-smoke`,
  `self-host-pergyra-likeness-test-smoke`, and
  `self-host-semantic-selfcheck-test-smoke` on the local Windows toolchain; the
  selfcheck accepts **110 real self-host sources** through both C and LLVM.

## 2026-07-04 - AST-text row facts split from inventory owner

- Added `src/self_hosted/codegen/input/ast_text_row_fact_owner.pgy` as the
  owner for name/type/mode rows derived from transitional AST-text inventory
  payloads.
- Extended `CodegenAstTextNode` with `name`, `type_name`, and `mode` fields.
  Function, return, role, nominal, enum, field, and parameter accessors now
  consume those fields instead of reparsing `node.payload` at each use site.
- Kept `ast_text_inventory_owner.pgy` under the 600-line component cap by
  limiting it to line inventory, kind/payload rows, bridge readiness, and cursor
  expectation diagnostics.
- Updated `OWNERS.md`, the component contract, and the real-source selfcheck
  manifest so the new row-fact owner is a first-class self-host source. The
  real-source semantic selfcheck now covers **110 sources** on both C and LLVM.
- Verified `build-source-inventory-test-smoke`,
  `self-host-component-contract-test-smoke`,
  `self-host-semantic-selfcheck-test-smoke`, `self-host-codegen-parity-test-smoke`,
  `self-host-preparation-test-smoke`, `checkedarith-failclosed-test-smoke`,
  `semantic-core-shape-test-smoke`, `slot-contract-test-smoke`, and targeted
  `spawn_future_await_slot` C/LLVM backend compare with and without AIR strict
  evidence.

## 2026-07-03 - AIR graph required keys split root facts from feature facts

- Repointed `src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy`
  so required document-root keys consume `JsonDocumentObjectFactTable` and
  `JsonObjectFactHasField` instead of whole-document `StringContains(content,
  key)` scans.
- Kept graph feature requirements (`compression_budget`, `compression_reason`,
  `execution_lane`, and `boundary_capture`) as graph-wide scalar facts consumed
  through `AirGraphScalarFieldValues`, because those keys live inside intent or
  boundary records rather than at the document root.
- Repointed summary reads through `JsonObjectFactValueBounds(root, "summary",
  ...)`, so nested `summary`-like text cannot satisfy the root summary contract.
- Registered the newly added `stdlib_option_bridges` backend-compare fixture in
  `tests/compare_backends.sh`; the default case inventory is complete again.
- Tightened the self-host preparation and component contracts against
  reintroducing root-key `StringContains` scans or treating nested AIR feature
  keys as document-root fields.
- Verified `build-source-inventory-test-smoke`,
  `backend-compare-inventory-test-smoke`, `semantic-core-shape-test-smoke`,
  `self-host-component-contract-test-smoke`, `self_host_preparation_smoke.sh`,
  AIR graph JSON validator parity, targeted `spawn_future_await_slot` C/LLVM
  backend compare, targeted air-strict backend compare, and `slot-contract`
  C/LLVM goldens on the local Windows toolchain.

## 2026-07-03 - MIR root arrays consume JSON fact tables

- Repointed `src/self_hosted/mir_lower/json_fact_read.pgy` so MIR `decls` and
  `routines` root-array discovery consumes `JsonDocumentObjectFactTable` and
  `JsonArrayObjectFactTable` instead of recomputing document end plus
  `JsonFieldArrayBounds` locally.
- Kept the existing `MirDeclArrayBounds`, `MirDeclObjectBoundsAt`,
  `MirRoutineArrayBounds`, and `MirRoutineObjectBoundsAt` facade stable, but
  moved the owner decision behind MIR fact-reader table accessors.
- Repointed `mir_fact_graph_contract_owner.pgy` so its payload fixture consumes
  `MirDeclArrayBounds`, `MirRoutineObjectBoundsAt`, `MirObjectArrayBounds`, and
  `MirObjectArrayObjectBoundsAt` instead of direct JSON array/object scans.
- Tightened both self-host component and compiler-world contracts against
  regressing the MIR payload contract to `JsonFieldArrayBounds`,
  `JsonArrayObjectBoundsAt`, or `JsonObjectFieldValueBounds`.
- Verified `self_hosted_component_contract_smoke.sh`,
  `self_host_compiler_world_contract_smoke.sh`, MIR JSON parity (**86 fixtures,
  0 clean rejects**), and real-source selfcheck (**110 sources** on C and LLVM).
  The Stable JSON blocker remains active until the wider schema/fact-table
  surface replaces remaining bounded scan compatibility helpers.

## 2026-07-03 - JSON object boundary facts enter hard-self-host substrate

- Added `src/self_hosted/lib/json_fact_table.pgy` as the bounded JSON object
  boundary owner. It validates and carries one object span, then exposes field
  span/kind accessors so consumers no longer discover object bounds locally.
- Repointed `module_manifest_resolver` so the top-level `modules` array comes
  from `JsonDocumentObjectFactTable` + `JsonObjectFactArrayBounds`.
- Extended that owner with bounded array-object facts. `module_manifest_resolver`
  now consumes module count, required-field counts, and string/bool equality
  counts through `JsonArrayObjectFact*` accessors instead of locally composing
  `JsonArrayObjectBoundsAt` and `JsonObjectHasField`.
- Tightened parity with a nested-`modules` negative fixture: a nested object may
  contain `"modules"`, but it must not satisfy the document-root manifest
  contract.
- Promoted the new owner into `OWNERS.md`, real-source selfcheck, component
  contract, and the pre-self-host expansion ledger. The broader JSON blocker
  remains active until more consumers use shared fact tables instead of bounded
  scan helpers.

## 2026-07-03 - Parser cursor owner drops sentinel mismatch flow

- Repointed the self-host parser cursor owner from `-1` mismatch sentinels to
  `Option<Int>` cursor facts. `ExpectOpt` and `ConsumeStmtTerminatorOpt` own
  the optional cursor result; the required `Expect` and
  `ConsumeStmtTerminator` APIs now fail closed through the parser error owner.
- Tightened `self_host_pergyra_likeness_smoke.sh` from `sentinel<=11` to
  `sentinel<=8` and raised the `Result`/`Option` usage floor from 246 to 258.
- Tightened the component contract so `cursor_owner.pgy` cannot reintroduce
  `return -1`. The remaining sentinel metric comes from generated C runtime
  helper ABI strings in the self-host codegen output owner, not parser cursor
  flow.

## 2026-07-03 - AST-text structural markers consume kind facts

- Moved the self-host codegen AST-text bridge's structural marker checks for
  `Program:`, `Body:`, `Block:`, and `Then:` from raw text equality to the
  owner-owned `kind` fact. `CodegenTypedAstBridgeReady` now checks the program
  root through `CodegenAstTextIsProgramRoot`, and `CodegenAstTextExpectNode`
  compares the expected marker kind instead of reopening line text.
- Tightened the component contract so root/expected marker checks cannot regress
  to `nodes[0].text != "Program:"` or `nodes[cur[0]].text != expected`.
- This reduces the mixed AST-like tree blocker but does not close it:
  `CodegenAstTextNode.text` still exists as diagnostic provenance and as the
  transitional payload source inside the bridge owner.

## 2026-07-03 - MIR-lower and lane facts enter real-source selfcheck

- Added explicit imports to the self-host `type_env` owner and MIR-lower
  owners so they declare the text, JSON fact, error, routine-inventory, and
  render facts they consume instead of relying on `main.pgy` to assemble them.
- Promoted `type_facts/type_env.pgy`, the standalone MIR-lower fact/render/
  program owners, and `sea/execution_lane.pgy` into `selfcheck_sources.sh`.
  The real-source semantic selfcheck manifest now rises from 97 to 107 accepted
  self-host owner/source files.
- Kept circular parser/codegen expression participants out of this slice; those
  need a separate import-cycle/owner-boundary decision rather than a blind
  manifest bump.

## 2026-07-02 - Codegen action owners declare their source-bundle dependencies

- Added explicit owner imports to `codegen/run/codegen_run_owner.pgy` and the
  five emission action participants (`program_emit`, `function_emit`,
  `stmt_emit`, `expr_rewrite`, `struct_value_emit`). They no longer rely on
  `codegen/main.pgy` as the only file that assembles their dependency graph.
- Promoted those six real sources into `selfcheck_sources.sh`, raising the
  real-source semantic selfcheck manifest from 91 to 97 accepted
  self-host owner/source files.
- This is a source-bundle SoT closure: `main.pgy` remains orchestration, while
  each owner declares the facts/functions it consumes. No import-stripped
  generated unit or helper concat path was introduced.

## 2026-07-01 - Stage bindings name payload contracts

- Promoted active self-host stage binding rows from
  `stage|zone|actor|intent` to `stage|zone|actor|intent|payload_contract`.
  Lexer, parser, semantic, MIR lower, and codegen `intent.md` files now name the
  payload owner each compiler-world stage readiness function must consume.
- Repointed `CompilerStageWorldBindingAt(...)` and
  `stage_artifact_owner.pgy` to the same 5-field rows, so the docs cannot claim
  payload ownership while the code still checks only placement topology.
- Tightened `self_host_pergyra_likeness_smoke.sh`,
  `self_host_compiler_world_contract_smoke.sh`, and the component contract to
  distinguish load-bearing world/zone/intent usage from decorative keyword
  counts.

## 2026-07-01 - Codegen enum name payload leaves brace-name slicing

- Extended `CodegenAstTextPayloadFor` so enum declarations record the enum name
  in `CodegenAstTextNode.payload` during inventory construction.
- Repointed `CodegenAstTextEnumName` to consume `node.payload`; enum variant
  list slicing remains in the input owner until variant rows become a separate
  payload fact.
- Tightened the component contract to reject enum-name recovery from
  `node.text` brace slicing.

## 2026-07-01 - Codegen enum variant payload leaves brace-list slicing

- Added `CodegenAstTextNode.aux_payload` and populated it from enum declaration
  rows during inventory construction.
- Repointed enum variant count/name accessors to consume the stored variant
  payload instead of recalculating brace offsets from `node.text`.
- Tightened the component contract to reject enum-variant recovery from enum
  declaration text slicing.

## 2026-07-01 - Codegen statement payloads leave statement-line slicing

- Extended the AST-text inventory with typed statement kinds and payloads for
  `Log`, value `Return`, `ArrayPop`, `ArraySet`, `ArrayPush`, and `Exit` rows.
- Repointed statement predicates and payload accessors for those rows to consume
  `CodegenAstTextNode.kind` and `payload` instead of rechecking prefixes and
  slicing `node.text`.
- Tightened the component contract so those simple statement facts cannot drift
  back into statement-owner text parsing.

## 2026-07-01 - Codegen control/dataflow statements leave line-prefix parsing

- Extended the AST-text inventory with typed statement kinds and payloads for
  `Let`, `Assign`, `For`, `While`, and `If`, plus kind facts for `Else`,
  `Defer`, `Break`, and `Continue`.
- Repointed statement predicates and payload accessors for those rows to consume
  `CodegenAstTextNode.kind` and `payload`.
- Removed the transitional `CodegenAstTextPayloadAfter` helper and tightened the
  component contract so statement-owner line-prefix parsing cannot return.

## 2026-07-01 - Codegen bare-call statements leave statement-owner text parsing

- Moved bare-call statement classification into `TypedAstTextKindOf`, backed by
  the canonical `TypedAstCallStatementKindForCallee` owner, so the inventory
  records single-call rows as typed statement facts.
- Repointed `CodegenAstTextIsBareCallStmt` and `CodegenAstTextBareCallExpr` to
  consume `CodegenAstTextNode.kind` and `payload`.
- Tightened the component contract to reject `IsSingleCall(node.text)` and
  `return node.text` inside the statement owner.

## 2026-07-01 - Runtime usage facts consume node payloads instead of line text

- Repointed `CodegenAstTextContains` in `ast_usage_owner.pgy` from
  `nodes[i].text` to typed `payload` and `aux_payload` fields.
- Added a kind-presence fact helper for statement-only runtime decisions such
  as `Log`, `Exit`, and collection mutation statements whose payload no longer
  includes the callee spelling.
- Tightened the component contract so runtime/header usage facts cannot drift
  back to line-text substring scans.

## 2026-07-01 - Call parameter mode CSV parsing moves to type facts

- Moved parameter-mode CSV count/index interpretation from
  `emission/expr_rewrite.pgy` into `type_facts/type_env.pgy`.
- Repointed inout call argument rewriting to consume `ParamModeCsvCount` and
  `ParamModeCsvAt` from the type-fact owner.
- Tightened the component contract so expression rewrite cannot regain local
  `pm` CSV parsing.

## 2026-07-01 - Codegen role payload leaves declaration-line slicing

- Extended `CodegenAstTextPayloadFor` so role declarations record the
  `Name for Type` payload in `CodegenAstTextNode.payload` during inventory
  construction.
- Repointed `CodegenAstTextRoleName` and `CodegenAstTextRoleForType` to split
  `node.payload` instead of recovering the `Role: ` prefix and `for` position
  from `node.text`.
- Tightened the component contract to reject role-name/type recovery from
  `node.text` declaration-line slicing.

## 2026-07-01 - Codegen parameter payload leaves parameter-line slicing

- Extended `CodegenAstTextNodeInventory` so `Parameters:` header context
  promotes child parameter rows to parameter kind rows and records the
  parameter payload in `CodegenAstTextNode.payload`.
- Repointed `CodegenAstTextParamMode`, `CodegenAstTextParamName`, and
  `CodegenAstTextParamType` to consume that payload instead of reading
  mode/name/type facts from `node.text`.
- Tightened the component contract to reject parameter mode and payload recovery
  from `node.text`.

## 2026-07-01 - Codegen field payload leaves Fields-line slicing

- Extended `CodegenAstTextNodeInventory` so `Fields:` header context promotes
  child `name: Type` rows to field kind rows and records the field payload in
  `CodegenAstTextNode.payload`.
- Repointed `CodegenAstTextFieldName` and `CodegenAstTextFieldType` to consume
  that payload instead of splitting `node.text`.
- Tightened the component contract to reject field name/type recovery from
  `node.text` colon slicing.

## 2026-07-01 - Emission stage consumes codegen payload fact

- Added `CompilerEmissionFactReady()` to
  `src/self_hosted/compiler/stage_artifact_owner.pgy`. It checks the
  `codegen|EmissionZone|ProgramEmitter|EmitProgramArtifact` stage binding row
  and then consumes `TypedAstArenaPayloadContractReady()` from
  `codegen/typed_ast_node_skeleton.pgy`.
- Repointed `ProgramEmitter.Emit` so backend emission must prove the codegen
  stage payload contract in addition to ABI-layout, symbol-table, and
  target-capability facts. This makes the backend nerve bundle more
  load-bearing inside `PgyCompilerWorld` instead of leaving codegen as only a
  documented stage binding.
- Tightened the compiler-world and component contract gates so the codegen
  payload binding cannot fall back to manifest-only topology.

## 2026-07-01 - Codegen execution consumes typed AST bridge guard

- Repointed the codegen run/emission boundary away from `ast: String` naming:
  `RunCodegenFromArgs`, `GenerateC`, `RejectUnsupportedCodegenBuiltins`, and
  `CodegenAstTextNodeInventory` now call the serialized input `tree_text`.
- Added `CodegenTypedAstBridgeReady(nodes, count)` in
  `input/ast_text_inventory_owner.pgy`. The guard consumes
  `TypedAstArenaPayloadContractReady()` and verifies that the owned
  `CodegenAstTextNode` inventory has a `Program:` root before emission starts.
- Tightened the `ast_string_surface` ratchet to zero and extended the component
  contract so future codegen slices cannot reintroduce `ast: String` execution
  surfaces while the typed-AST migration is underway.

## 2026-07-01 - Codegen function payload leaves line-text comparison

- Extended `CodegenAstTextNode` with a `payload` field. The inventory owner now
  records function-name and return-type payload rows while it builds the typed
  bridge node array.
- Repointed `CodegenAstTextIsMainFunction`,
  `CodegenAstTextFunctionName`, and `CodegenAstTextReturnType` to consume the
  payload field instead of re-slicing or comparing `node.text`.
- Tightened the component contract to reject the old
  `node.text == "Function: Main"` check and require the typed payload field.

## 2026-07-01 - Codegen nominal payload leaves prefix-length helper

- Extended `CodegenAstTextPayloadFor` so nominal declarations (`Struct`,
  `Class`, `Subject`, `Vessel`, `Object`, and `TObject`) record their owner
  name in `CodegenAstTextNode.payload` during inventory construction.
- Repointed `CodegenAstTextNominalName` to consume `node.payload` and removed
  `CodegenAstTextNominalPrefixLen`, which was re-decoding the same declaration
  prefix from `node.text`.
- Tightened the component contract so nominal declaration name reads cannot
  return to prefix-length slicing.

## 2026-06-30 - Pergyra-likeness load-bearing metric

- Added `compiler_world_stub_actions` to
  `tests/self_host_pergyra_likeness_smoke.sh`. The self-host likeness gate
  already checked that `PgyCompilerWorld`, resource zones, intent clusters, and
  stage binding rows exist; this new metric tracks the weaker point: compiler
  actors in `world.pgy` that still return scaffold `true` instead of consuming a
  concrete owner fact.
- Documented the distinction in
  `11_compiler_world_architecture.md` and
  `13_compiler_substrate_architecture.md`: low keyword density is not the
  problem; decorative topology is. The hard bootstrap improves when source
  intake, lexer, parser, semantic, MIR-lower, emission, and parity actors
  consume path, token, AST, verdict, MIR, artifact, and parity facts.

## 2026-06-30 - Source intake consumes path-manifest fact

- Repointed `SourceUnit.Read` so it accepts `StagePathManifest` and returns
  `CompilerStagePathManifestReady()` instead of scaffold `true`.
- Routed `paths` through `FrontendPipeline` and `IntakeSource`, making the root
  compiler world pass the path-manifest fact into source intake.
- Tightened the Pergyra-likeness ratchet from `world_stub_actions <= 7` to
  `<= 6`. The remaining scaffold actors are lexer, parser, semantic,
  MIR-lower, emission, and parity.

## 2026-06-30 - Parity actor consumes artifact/test-harness facts

- Repointed `OraclePair.Compare` from scaffold `true` to
  `CompilerArtifactZoneReady() && CompilerTestHarnessReady()`.
- Tightened `world_stub_actions` from `<= 6` to `<= 5`. The remaining scaffold
  actors are lexer, parser, semantic, MIR-lower, and emission.

## 2026-06-30 - Program emitter consumes backend projection facts

- Repointed `ProgramEmitter.Emit` from scaffold `true` to
  `CompilerAbiLayoutRowsReady() && CompilerSymbolTableReady() &&
  CompilerTargetCapabilityEnvelopeReady()`.
- Routed `TargetCapabilityZone` through `EmitProgramArtifact`, so emission is
  not just an output-buffer participant; it consumes ABI layout, symbol, and
  target acceptance facts before claiming success.
- Tightened `world_stub_actions` from `<= 5` to `<= 4`. The remaining scaffold
  actors are lexer, parser, semantic, and MIR-lower.

## 2026-06-30 - Stage actors consume artifact-envelope facts

- Added `src/self_hosted/compiler/stage_artifact_owner.pgy` as the owner for
  stage artifact envelope readiness. It binds token, AST, semantic, and MIR
  stage actors to their path-manifest entry and world/zone/actor/intent row.
- Repointed `LexerStage.Scan`, `ParserStage.BuildAst`,
  `SemanticStage.Check`, and `MirLowerStage.Lower` away from scaffold `true`
  and into that owner.
- Tightened `world_stub_actions` from `<= 4` to `<= 0`. This closes the
  decorative compiler-world actor scaffold. It does not yet prove payload
  contents; payload-level token/AST/semantic/MIR facts remain the next hard
  bootstrap depth.
- Added the separate `stage_envelope_only <= 4` ratchet so that envelope-only
  stage readiness remains visible and can be driven down without pretending the
  payload facts are already closed.

## 2026-06-30 - Lexer stage consumes token payload contract

- Added `LexerTokenPayloadContractReady()` to
  `src/self_hosted/lexer/token_owner.pgy`. The token owner now exposes the
  token-stream schema, committed fixture count, keyword classification quirks,
  and token-line output formatting as the lexer payload contract.
- Repointed `CompilerTokenStreamFactReady()` so it first checks the
  `lexer|TokenStreamZone|LexerStage|LexSource` envelope and then consumes the
  lexer token payload contract.
- Tightened `stage_envelope_only` from `<= 4` to `<= 3`. Parser, semantic, and
  MIR readiness remain envelope-only and are the next payload-depth targets.

## 2026-06-30 - Parser stage consumes compact AST payload contract

- Added `ParserAstTreePayloadContractReady()` to
  `src/self_hosted/parser/tree_text_owner.pgy`. The parser tree-text owner now
  exposes the current compact-AST text schema, committed fixture count, and root
  `Program:` / implicit-`Main` output shape as a parser payload contract.
- Repointed `CompilerAstTreeFactReady()` so it first checks the
  `parser|AstTreeZone|ParserStage|ParseTokens` envelope and then consumes the
  parser AST payload contract.
- Tightened `stage_envelope_only` from `<= 3` to `<= 2`. Semantic and MIR
  readiness remain envelope-only and are the next payload-depth targets.

## 2026-06-30 - Semantic stage consumes verdict payload contract

- Added `SemanticVerdictPayloadContractReady()` to
  `src/self_hosted/semantic/diagnostic_owner.pgy`. The semantic diagnostic
  owner now exposes the verdict schema, 107-fixture parity surface, ok/error
  status rendering, and 17-code vocabulary as the semantic verdict payload
  contract.
- Repointed `CompilerSemanticVerdictFactReady()` so it first checks the
  `semantic|SemanticVerdictZone|SemanticStage|CheckProgramSemantics` envelope
  and then consumes the semantic verdict payload contract.
- Tightened `stage_envelope_only` from `<= 2` to `<= 1`. MIR readiness remains
  the only envelope-only compiler stage.

## 2026-06-30 - MIR stage consumes fact graph payload contract

- Added `MirFactGraphPayloadContractReady()` to
  `src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy`. The MIR fact
  graph contract owner now exposes the `pgy.mir.v1` schema, 85-fixture MIR
  parity surface, declaration/routine arrays, source-local array, and
  instruction source facts as the MIR fact graph payload contract.
- Repointed `CompilerMirFactGraphReady()` so it first checks the
  `mir_lower|MirFactGraphZone|MirLowerStage|LowerProgramFacts` envelope and
  then consumes the MIR fact graph payload contract.
- Tightened `stage_envelope_only` from `<= 1` to `<= 0`. All compiler stage
  readiness functions now consume a payload contract after their
  path/world-binding envelope.

## 2026-06-30 - Typed AST arena payload contract

- Promoted `src/self_hosted/codegen/typed_ast_node_skeleton.pgy` from a
  parse-only migration sketch to the first typed AST arena payload contract.
  It now owns the `pgy.selfhost.typed-ast-arena.v1` schema, `AstArena`,
  `AstNode`, explicit `ChildAt` traversal, `AtomText` lookup, and a small
  root traversal fixture.
- Added a `typed_ast_contract` floor to
  `tests/self_host_pergyra_likeness_smoke.sh` and component-contract checks so
  the mixed AST-like tree owner cannot disappear while parser/codegen still use
  the transitional AST text bridge.
- This does not retire the bridge yet. It creates the typed owner that future
  parser/codegen cutover slices must consume before hard self-host can claim AST
  replacement.

## Verified state (rolling)

- **Lexer**: self-hosts on C+LLVM. Byte-identical to `pgy --tokens` across the 7
  committed parity fixtures (gated) and **993/993 examples + backend_compare
  sources** (scale probe, as of session 2026-06-23). Zero self-host lexer crashes.
  `main.pgy` is now only the entrypoint; character/codepoint handling,
  token classification/output formatting, and scan-loop state are split into
  source-of-truth owner modules.
- **Parser**: self-hosts on C+LLVM. Byte-identical against `pgy --ast` on 186
  committed fixtures (gated); examples scale probe last recorded 120/121 with
  zero byte-drift, zero self-host exits, 1 C-oracle skip. Parser ownership is
  partially split: parse failure rendering, source cursor/token reads, written
  type-name parsing, expression parsing, statement/block parsing, function
  declaration/signature rendering, recursive declaration dispatch,
  type/ability/event/enum/zone/effect/relation/role/intent/nominal-domain
  declaration parsing, and compact AST text formatting are separate owner
  modules. Parser tool input is single-sourced through `Args()[0]` with
  `examples/hello.pgy` as the no-arg default; scale probing no longer writes a
  `fixture/source.txt` override.
- **Backend parity**: parser compiled by C and by LLVM produce byte-identical
  output -- the core self-host correctness signal.
- **Semantic**: self-hosts a bounded function-body checker on C+LLVM across
  **107 committed fixtures**. Expression typing now owns same-type
  `Int`/`Long`/`Float` arithmetic, same-type `Bool` arithmetic,
  `String + String` concatenation typing, contextual integer-literal assignment
  to `Long` as the only widening rule in this rung, and scalar math builtin
  signatures for `Sqrt`, `Pow`, `Floor`, `Ceil`, `Random`, and `SeedRandom`, trig/log Float
  signatures from `Sin` through `Log2`, string split/join aliases, plus
  first-argument scalar utility typing for `Abs`, `Min`, `Max`, and `Clamp`.
  `Option<T>` payload typing now rejects `Some` payload drift and rejects
  `IsSome`/`UnwrapOption` on non-Option or non-concrete `Option<Unknown>`
  operands against the C/LLVM oracle.
- **Compiler core**: capability-5 single-source-of-truth is READY for the
  measured source_ast/source_decl and supported MIR-lowering frontier.
  Source-payload reads for the gated body surface have been replaced by
  dedicated MIR/source-shape facts, and the self-hosted MIR-lowering path is
  ratcheted against reading transitional `"ast"` text. The committed
  MIR-lower/codegen frontier is **86 PASS / 0 gap plus 0 clean rejects**.

## Roadmap to completion

1. **Front-end coverage to 100%** (assist-safe): lexer measured corpus is at
   993/993; parser corpus is at 120/121 with only the C-oracle `secure_slots` skip
   remaining. The next parser move is structured AST ownership rather than
   polishing a text-mirror substitute.
2. **Measurement/golden coverage** (assist-safe): committed scale probes per
   tool (lexer done); add golden probes for the other oracle dimensions the
   scorecard names (diagnostics, MIR/AIR JSON, deterministic ordering).
3. **Capability-5 breadth expansion**: keep the source_ast/source_decl and
   supported MIR-lowering ratchets at zero while broadening explicit MIR facts
   for more statement/expression surfaces. Do not reopen source-text or
   source-payload compatibility lanes.
4. **IR-layer verifiers**: each layer (AIR evidence, HIR/DAG type resolution,
   MIR CFG/body/ownership, ABI layout, backend fact consumption) gets a verifier
   that owns its contract.
5. **Post-self-host: the validation milestone**
   ([`../post_selfhost_validation_milestone.md`](../post_selfhost_validation_milestone.md)).
   The broad stdlib is written in self-hosted Pergyra (the usability bulk +
   dogfood), and the dungeon crawler is built, against falsifiable criteria for
   whether domain-meaning preservation actually pays off (differential safety,
   evidence-as-audit, legibility, ergonomics convergence). A negative result is
   allowed -- that is what makes a positive one mean something.

## Session log

### 2026-06-30 -- JSON owner `FindFrom` returns Option

- Changed the self-hosted shared JSON owner's internal `FindFrom` scanner from
  `Int` with `-1` absence to `Option<Int>`.
- Repointed JSON string/number/array-bound consumers inside `lib/json.pgy` to
  check `IsSome` before unwrapping positions. Public JSON field APIs remain
  stable; the cutover is internal to the JSON owner.
- Split JSON scan primitives into `lib/json_scan.pgy` so `lib/json.pgy` stays
  under the 600-line owner cap and keeps object/field/emit responsibility.
  Parity harnesses that previously copied only `json.pgy` now mirror
  `lib/*.pgy`, matching the shared-lib owner set.
- Tightened the component contract to require the `Option<Int>` signature and
  reject the old `Int` signature; lowered the Pergyra-likeness ratchet from
  `sentinel <= 21` to `sentinel <= 18`, and raised `result_use` from 140 to
  149.

### 2026-06-30 -- Codegen AST-text inventory drops dead parent sentinel

- Removed the unused `parent: Int` backedge from `CodegenAstTextNode` and
  deleted `CodegenAstTextParentIndex`. No consumer read that field; keeping it
  only preserved a `-1` parent sentinel as a fake fact.
- Replaced the unreachable `CodegenAstTextNominalPrefixLen` panic-tail
  `return -1` with a neutral return after `Die`.
- Tightened the component contract to reject `parent`, `CodegenAstTextParentIndex`,
  and `return -1` in `ast_text_inventory_owner.pgy`; lowered the
  Pergyra-likeness ratchet from `sentinel <= 23` to `sentinel <= 21`.

### 2026-06-30 -- Codegen text scanners stop using integer sentinels

- Changed `FindMatchingBracket`, `FindMatchingParen`, `FindTopLevelPlus`, and
  `FindTopLevelComma` in `codegen/text/text_owner.pgy` from `Int` with `-1`
  absence to `Option<Int>`.
- Repointed expression rewrite, expression scan, statement emit, struct-value
  emit, AST-text statement/inventory, and type-fact consumers to check
  `IsSome` before unwrapping scanner positions.
- Tightened the component contract to reject `return -1` in `text_owner.pgy`
  and lowered the Pergyra-likeness ratchet from `sentinel <= 27` to
  `sentinel <= 23`; `result_use` rose from 106 to 140.
- Verified with AST parsing for all touched self-host codegen owners,
  `self-host-pergyra-likeness-test-smoke`,
  `self-host-component-contract-test-smoke`,
  `self-host-codegen-parity-test-smoke`, `production-c-size-test-smoke`, and
  `self-host-codegen-bootstrap-test-smoke`.
- Design note: this does not try to raise Pergyra-likeness by adding more
  `world` or `zone` keywords. The owner responsibility is `TextOwner` producing
  typed absence facts, and emitters consuming those facts before slicing. That
  is the part that makes the bootstrap more Pergyra-like.

### 2026-06-30 -- MIR-lower routine inventory discovery stops using integer sentinels

- Changed `FindRoutine`, `RoutineNameEnd`, and `RoutineBlocksStart` in
  `mir_lower/routine_inventory_owner.pgy` from `Int` with `-1` absence to
  `Option<Int>`.
- Repointed program and routine lowering to check `IsSome` before unwrapping
  routine positions, routine-name bounds, and block-start presence.
- Tightened the component contract to reject `return -1` in the routine
  inventory owner and lowered the Pergyra-likeness ratchet from
  `sentinel <= 30` to `sentinel <= 27`; `result_use` rose from 91 to 106.

### 2026-06-30 -- MIR-lower method routine lookup stops using integer sentinel

- Changed `FindRoutineByOwnerName` in `mir_lower/routine_inventory_owner.pgy`
  from `Int` with `-1` absence to `Option<Int>`.
- Repointed class and role method declaration reconstruction in
  `decl_lower.pgy` to consume that lookup with `IsSome` / `UnwrapOption`.
- Tightened the component contract so the owner signature and consumer path
  stay Option-based, and lowered the Pergyra-likeness ratchet from
  `sentinel <= 32` to `sentinel <= 30`; `result_use` rose from 85 to 91.

### 2026-06-30 -- Codegen boolean operator scanner stops using integer sentinel

- Changed `FindTopLevelOp2` in `codegen/text/expr_scan.pgy` from `Int` with
  `-1` absence to `Option<Int>`.
- Repointed `RewriteBool` to consume top-level `||`, `&&`, `==`, and `!=`
  positions with `IsSome` / `UnwrapOption`.
- Tightened the component contract to reject `return -1` in `expr_scan.pgy`
  and lowered the Pergyra-likeness ratchet from `sentinel <= 33` to
  `sentinel <= 32`; `result_use` rose from 77 to 85.

### 2026-06-30 -- Semantic literal scanner stops using integer sentinel

- Changed `ExpectLiteral` in `semantic/text_scan_owner.pgy` from `Int` with
  `-1` absence to `Option<Int>`.
- Repointed semantic body/program consumers to `IsSome` / `UnwrapOption`
  consumption at the scanner boundary.
- Tightened the component contract to reject `return -1` in the semantic text
  scanner and lowered the Pergyra-likeness ratchet from `sentinel <= 34` to
  `sentinel <= 33`; `result_use` rose from 69 to 77.

### 2026-06-30 -- ToFloat and Exit target spellings consume runtime owners

- Added `StringRuntimeCToFloatFn` to `string_runtime_owner.pgy` and
  `HostIORuntimeCExitFn` to `host_io_runtime_owner.pgy`.
- Repointed `ToFloat` expression rewrite and `Exit(Int)` statement emission so
  self-host C emission consumes owner facts instead of local `"atof("` and
  `"exit("` literals.
- Tightened the component contract to require the owner functions and reject the
  old expression/statement-local target spellings.

### 2026-06-30 -- Float math call spelling consumes MathRuntimeOwner

- Added `MathRuntimeCSqrtFn`, `MathRuntimeCPowFn`, `MathRuntimeCFloorFn`, and
  `MathRuntimeCCeilFn` to `runtime_abi/math_runtime_owner.pgy`.
- Repointed self-host C expression rewrite for `Sqrt`, `Pow`, `Floor`, and
  `Ceil` so the emitter consumes owner facts instead of local `"sqrt("` /
  `"pow("` / `"floor("` / `"ceil("` string literals.
- Tightened the component contract to require those owner functions and reject
  the old expression-local target-library spellings.

### 2026-06-30 -- Stage path manifest carries compiler-world bindings

- Added `CompilerStageWorldBindingAt` to
  `src/self_hosted/compiler/path_manifest_owner.pgy`. Each active self-host
  stage path now has a manifest-owned row for its resource zone, actor, and
  intent (`lexer|TokenStreamZone|LexerStage|LexSource`, etc.).
- Tightened the compiler-world contract smoke so those rows cannot drift out of
  the path owner. This makes Pergyra-likeness more load-bearing: future
  bootstrap drivers can consume world placement facts instead of inferring a
  stage's role from folder names.

### 2026-06-30 -- Pergyra-likeness reports compiler-world topology

- Extended `tests/self_host_pergyra_likeness_smoke.sh` so it is no longer only
  a negative smell ratchet. It still tracks text-munging, AST-as-string,
  sentinel control flow, and Result/Option use, but it now also prints positive
  topology floors for `PgyCompilerWorld`, resource-zone declarations,
  compiler-intent surface, and intent steps bound to resource zones.
- Documented that these positive topology numbers are floors, not scores:
  adding fake zones or tiny intent files is not an improvement. The floor only
  prevents hard self-hosting from losing the visible world/zone/intent spine
  while the compiler starts eating its own slices.

### 2026-06-30 -- Shared LLVM leg consumes Artifact/TestHarness owner

- Repointed `tests/self_hosted/parity/llvm_leg_helpers.sh` so shared C-vs-LLVM
  tool-output comparison is no longer a shell string/diff verdict. The helper
  writes normalized C and LLVM stdout artifacts, builds a build-dir/PID-local
  Pergyra `backend_output_comparator`, rejects artifact paths that escape the
  repository root, and invokes it with `c_oracle` and `llvm_oracle` projection
  rows. There is no ignored shared comparator cache in the verdict path.
- The helper is used by AIR graph consumers, diagnostic/doc/examples/manifest
  checkers, size checkers, runtime boundary, stable subset, and stdlib dispatch
  inventory rungs, so one owner repoint moves those C/LLVM equality checks
  behind `ArtifactZone`, `TestHarnessZone`, and the subprocess envelope owner.
- Tightened the component contract so `llvm_leg_helpers.sh` cannot silently
  reintroduce shared ignored comparator caches, command-substitution output
  equality, `cmp`, or `diff <(...)` as the verdict owner.

### 2026-06-30 -- Lexer token parity consumes Artifact/TestHarness owner

- Repointed `tests/self_hosted/parity/lexer_parity.sh` so C-compiled,
  LLVM-compiled, and live `pgy --tokens` lexer output comparison is no longer a
  shell string/diff verdict. The harness writes token streams as artifact files
  and invokes the Pergyra `backend_output_comparator` with explicit projection
  rows.
- The lexer rung now records expected fixture vs `self_hosted` token output and
  expected fixture vs `c_oracle` live-token drift through `ArtifactZone`,
  `TestHarnessZone`, and the subprocess envelope owner.
- Tightened the component contract so lexer parity cannot silently reintroduce
  `EXPECTED_OUT`/`PERGYRA_OUT` shell equality or `diff <(...)` as the owner of
  the verdict.
- Removed the thin `CompilerAbiLayoutCValueType(type_name: String) -> String`
  ABI row alias after the broader self-host preparation gate caught the
  `string_munge_sig` ratchet at 162/161. `CompilerAbiLayoutRowIndex` now returns
  `Option<Int>` instead of the `-1` sentinel, and the self-host C ABI consumer
  reads `CompilerAbiLayoutRowCValueTypeAt(UnwrapOption(row))` directly. This
  keeps the row owner as the SoT without adding another text-to-text spelling
  path or out-of-band row-missing value.
- Tightened `tests/self_host_pergyra_likeness_smoke.sh` after the Option row
  change raised `result_use` to 69; the new minimum is 69.

### 2026-06-29 -- String and Bool arithmetic mirror the C oracle

- `src/self_hosted/semantic/expr_type_owner.pgy` now preserves C-oracle
  arithmetic result facts for same-type `Bool` arithmetic and `String + String`
  concatenation.
- `src/self_hosted/semantic/expr_validation_owner.pgy` keeps mixed
  `Int + String` rejected as `binop_type_mismatch`, matching the C semantic
  oracle, while the valid String/Bool cases are accepted.
- `tests/self_hosted/parity/semantic_parity.sh` now covers 107 fixtures on both
  C and LLVM, adding `valid_string_plus`, `valid_string_scalar_plus`, and
  `valid_bool_arith`.

### 2026-06-29 -- Option semantic contracts are concrete-gated

- `src/self_hosted/semantic/expr_type_owner.pgy` now owns `Some(expr) ->
  Option<ExprType(expr)>`, `None -> Option<Unknown>`, `None() ->
  Option<Unknown>`, and `UnwrapOption(Option<T>) -> T`.
- `src/self_hosted/semantic/call_check_owner.pgy` rejects `IsSome` and
  `UnwrapOption` when the operand is not `Option<T>` or when the operand is the
  non-concrete `Option<Unknown>` fact produced by unannotated `None`.
- `tests/self_hosted/parity/semantic_parity.sh` now covers 104 fixtures on
  both C and LLVM, including `Some("x")` payload mismatch, `None()` contextual
  success, and non-concrete Option builtin rejection.
- `tests/self_hosted/parity/selfcheck_sources.sh` accepts 87 real self-host
  source files on C and LLVM after the stricter Option contract.

### 2026-06-27 -- Final self-host reports consume JSON owner

- Repointed `air_graph_json_validator/report_owner.pgy` and
  `ast_read_surface_checker` report emission from manual `json_parts` assembly
  to `src/self_hosted/lib/json.pgy`.
- Tightened component-contract ratchets so those reports require
  `JsonEmitObject(report_fields)`, `JsonEmitArray(...)`, and reject local
  `json_parts`.
- After this slice, `rg json_parts src/self_hosted/tools -g "*.pgy"` returns no
  self-hosted tool report emitters.

### 2026-06-27 -- AIR graph reports consume JSON owner

- Repointed `air_graph_id_uniqueness`, `air_graph_node_count_integrity`,
  `air_graph_reachability`, `air_graph_ref_integrity`, and
  `air_graph_ref_live` report emission from manual `json_parts` assembly to
  `src/self_hosted/lib/json.pgy`.
- Updated their parity harnesses to mirror the JSON library into `.tmp/lib` and
  normalize CR when extracting clean JSON.
- Added component-contract ratchets requiring `JsonEmitObject(report_fields)`,
  `JsonEmitArray(...)`, and rejecting the old local `json_parts` reports.

### 2026-06-27 -- Inventory reports consume JSON owner

- Repointed `examples_inventory_checker` and
  `stdlib_dispatch_inventory_checker` report emission from manual `json_parts`
  assembly to `src/self_hosted/lib/json.pgy`.
- Updated both parity harnesses to pass compiler-native tool paths to Windows
  `pgy` and normalize CR when extracting the clean JSON line.
- Added component-contract ratchets requiring `JsonEmitObject(report_fields)`,
  `JsonEmitArray(findings)`, and rejecting the old local `json_parts` reports.

### 2026-06-27 -- Doc link checker report consumes JSON owner

- Repointed `doc_link_checker` report emission from manual `json_parts`
  assembly to `src/self_hosted/lib/json.pgy`.
- Updated the doc-link parity harness to mirror the JSON library into the copied
  tool build directory and pass a compiler-native tool path to Windows `pgy`.
- Added component-contract ratchets requiring `JsonEmitObject(report_fields)`,
  `JsonEmitArray(findings)`, and rejecting the old local `json_parts` report.

### 2026-06-27 -- Backend comparator report consumes JSON owner

- Repointed `backend_output_comparator` report emission from manual
  `json_parts` assembly to `src/self_hosted/lib/json.pgy`.
- Updated the comparator parity harness to mirror the JSON library into the
  copied tool build directory, so C/LLVM parity checks the imported owner path.
- Added component-contract ratchets requiring `JsonEmitObject(report_fields)`,
  `JsonEmitArray(findings)`, and rejecting the old local `json_parts` report.

### 2026-06-27 -- Module manifest consumes JSON array-object traversal

- Added bounded JSON array/object traversal primitives to
  `src/self_hosted/lib/json.pgy`.
- Repointed `module_manifest_resolver` so module count, required field counts,
  `beta_blocker:true`, and `status:"stable-subset"` are read from the bounded
  `modules` array rather than whole-document substring counts.
- Tightened the component contract to reject `TextScan.CountOccurrences` in the
  resolver and require the JSON owner traversal calls.

### 2026-06-27 -- Module manifest report consumes JSON owner

- Repointed `module_manifest_resolver` report emission from manual
  `json_parts` assembly to `src/self_hosted/lib/json.pgy`.
- Added `JsonDocumentHasField(...)` and made the missing-`modules` check consume
  that owner instead of a raw `"modules":` substring probe.
- Added component-contract ratchets requiring JSON owner use and rejecting the
  old raw field check.

### 2026-06-27 -- Stable subset report consumes JSON owner

- Repointed `stable_subset_section_checker` report emission from manual
  `json_parts` assembly to `src/self_hosted/lib/json.pgy`.
- Updated its parity harness to mirror the JSON library into the copied tool
  build directory, so C/LLVM parity checks the imported owner path rather than
  a repo-root accident.
- Added component-contract ratchets requiring `JsonEmitObject(report_fields)`
  and `JsonEmitArray(pieces)` for the stable-subset report.

### 2026-06-27 -- JSON schema reader first consumer

- Moved the AIR graph JSON validator's schema check and integer summary field
  reads behind `src/self_hosted/lib/json.pgy`.
- Added component-contract ratchets so the validator cannot return to a raw
  `"schema":"pgy.air.graph.v1"` substring check.
- This advances the stable JSON owner but does not close it: object/array
  iteration and all self-host report schemas still need to consume one
  structured writer/reader owner.

### 2026-06-26 -- Target capability envelope enters compiler world

- Added `src/self_hosted/compiler/target_capability_owner.pgy` as the
  self-hosted owner for target acceptance and fallback facts. It names the
  current projection set (`cpu-c`, `cpu-llvm`, `self-hosted`), the projection
  fact envelope (`intent_graph`, `effect_set`, `authority_evidence`,
  `coordination`, `slot_ownership`, `layout_shape`, `loss_budget`,
  `materialization_reason`), and explicit fallback reasons
  (`unsupported_shape`, `forbidden_loss_budget`, `retained_effect`,
  `missing_authority_evidence`, `host_only_slot_boundary`).
- Wired `TargetCapabilityZone`, `TargetCapabilityEnvelope`, and
  `TargetProjectionPlanner` into `PgyCompilerWorld`. `BackendPipeline` now
  passes through `PlanTargetProjection(...)` before emission, so backend
  replacement/fallback is represented as a compiler-world fact boundary rather
  than prose only.
- Added the owner to `StagePathManifest`, the shell compiler-world manifest,
  `OWNERS.md`, and the real-source semantic selfcheck manifest, raising that
  manifest from 72 to 73 accepted self-host owner/source files.
- Tightened the compiler-world and component gates so the target-capability
  owner, fallback vocabulary, zone, object, and pipeline step cannot disappear
  silently.

### 2026-06-25 -- Codegen SeedRandom, indexed arrays, and mir_lower breadth enter bootstrap

- Closed the self-hosted codegen `SeedRandom(seed)` builtin gap. The token
  rewrite owner now lowers it to `pgy_seedrandom`, and the program prelude owner
  emits the corresponding `srand((unsigned int)seed)` helper only when the AST
  carries that fact.
- Added `seed_random.pgy` to the codegen parity manifest. The fixture proves
  same-seed replay semantics without pinning a cross-libc random sequence.
  Added `array_index_assign.pgy` and `string_array_index_return.pgy` to close
  indexed array write/read return surfaces needed by real compiler-stage code.
  Gate: `make self-host-codegen-parity-test-smoke` now covers **63 fixtures**.
- Tightened the C ABI shape for self-hosted codegen string returns:
  `String -> const char*`, matching string params, literals, and
  `Array<String>` element reads at the boundary.
- Tightened bootstrap breadth to use `gen2` for component/tool emission. The
  previous breadth loop described a codegen-built binary but emitted those
  component/tool C files through `gen0`; now lexer, parser, semantic, and audit
  tool breadth are emitted by the Pergyra-built codegen.
- Tightened `codegen_bootstrap.sh`: `gen2` (the Pergyra-built codegen) must now
  compile `src/self_hosted/mir_lower/main.pgy` and match the C oracle-built
  `mir_lower` output on `let_log`, `forloop`, and `role_operator_dispatch`.
- Tightened `codegen_bootstrap.sh`: `gen2` (the Pergyra-built codegen) must now
  compile the backend parity fuzz generator and produce the same stdout,
  manifest, and generated `f*.pgy` corpus as the C oracle-built generator.
  Gate: `make self-host-codegen-bootstrap-test-smoke`.

### 2026-06-25 -- Semantic scalar math builtins enter the oracle parity rung

- Added semantic signatures for `Sqrt(Float) -> Float`, `Pow(Float, Float) ->
  Float`, `Floor(Float) -> Float`, `Ceil(Float) -> Float`, and
  `Random(Int) -> Int` in `program_check_owner.pgy`.
- Added three C-oracle-backed semantic fixtures:
  `valid_scalar_math_builtins`, `bad_sqrt_arg`, and `bad_random_arg`.
- Ratcheted `semantic_parity.sh` and the component contract from 68 to
  **71 committed fixtures**. The new bad fixtures prove `call_arg_type_mismatch`
  is emitted before relying on backend/native C type errors for these scalar
  builtin calls.

### 2026-06-25 -- Semantic scalar utility builtins consume first-argument facts

- Added self-hosted semantic inventory entries for `Abs`, `Min`, and `Max`.
  `expr_type_owner.pgy` now returns the first argument's known type for these
  calls, matching the C semantic owner instead of falling through to `Unknown`.
- `call_check_owner.pgy` now enforces the `Min`/`Max` second-argument
  assignability contract through `ExpressionAssignableTo(...)`, so the
  utility rule is owned by the self-hosted call checker rather than backend
  compile failure.
- Added four C-oracle-backed fixtures:
  `valid_scalar_utility_int`, `valid_scalar_utility_float`, `bad_min_mixed`,
  and `bad_max_mixed`, raising semantic parity to **75 committed fixtures**.

### 2026-06-25 -- Clamp joins first-argument scalar utility typing

- Added `Clamp` to the self-hosted semantic builtin inventory. The Pergyra
  expression type owner now returns the first argument's known type for
  `Clamp(x, lo, hi)`, matching the C scalar owner.
- Added `valid_clamp_int`, `valid_clamp_float`, and `bad_clamp_assign` fixtures.
  The bad fixture proves a `Float` first-argument `Clamp` cannot initialize an
  `Int` local through an `Unknown` fallback.
- Ratcheted semantic parity and the component contract to **78 committed
  fixtures**.

### 2026-06-25 -- Trig/log Float builtins enter semantic parity

- Added semantic inventory entries for `Sin`, `Cos`, `Tan`, `Asin`, `Acos`,
  `Atan`, `Atan2`, `Round`, `Exp`, `MathLog`, `Log10`, and `Log2`, matching
  the C scalar builtin table used by `scalar_trig_log_runtime`.
- Added `valid_scalar_trig_log_builtins`, `bad_sin_arg`, and `bad_atan2_arg`
  fixtures so unary and binary Float builtin calls are C-oracle-backed instead
  of falling through to native backend errors.
- Ratcheted semantic parity and the component contract to **81 committed
  fixtures**.

### 2026-06-25 -- String split/join aliases enter semantic parity

- Added `Join` and `StringSplit` to the self-hosted semantic builtin inventory.
  `StringJoin`/`Join` now require a string separator, while `Split`/
  `StringSplit` and `StringContains` require string arguments where the C
  scalar owner already does.
- Added `valid_string_alias_builtins`, `bad_string_contains_arg`,
  `bad_split_arg`, and `bad_join_sep` fixtures. These close the common
  self-hosted tool surface (`Split` + `StringJoin` + `StringContains`) against
  native backend fallback diagnostics.
- Ratcheted semantic parity and the component contract to **85 committed
  fixtures**.

### 2026-06-25 -- Role operator dispatch becomes a positive MIR JSON path

- Promoted `role_operator_dispatch.pgy` from a clean-reject boundary into the
  positive MIR JSON hard path. The fixture now reconstructs `Role: IntMath for
  Int`, lowers the role method with `self: Int` from the role `for_type` fact,
  and rewrites `a + b` through the MIR-owned `IntMath_Add` operator path.
- Tightened the self-hosted `mir_lower` declaration consumer so class/role
  method lists are read through bounded JSON array/object facts. This prevents
  parameter names such as `self` from being mistaken for declaration method
  names.
- The rolling MIR JSON frontier moves to **86 PASS / 0 gap plus 0 clean
  rejects**. Remaining role work is now richer/default/generic/dynamic ability
  dispatch, not a declaration-fact or source-AST fallback boundary.

### 2026-06-25 -- Role declarations become fact-owned clean rejects

- Closed the remaining MIR JSON role declaration fact seam. The C MIR
  declaration header now captures the role subject `for_type`, and
  `pgy --mir-json` emits role declarations as MIR-owned `kind:"role"` facts
  with includes, impl ability spans, and method signature facts.
- The self-hosted `mir_lower` no longer depends on an `AST_ROLE_DECL`
  unsupported fallback for this boundary. It reads the role fact, requires
  `for_type`, and rejects with an observable
  `unsupported MIR role declaration in self-host subset: IntMath for Int`
  diagnostic.
- The MIR JSON frontier remains **84 PASS / 0 gap plus 1 clean reject**, but
  the clean reject is now a semantic-support boundary: role operator/dispatch
  consumption is not implemented in the self-host subset yet. It is no longer a
  missing declaration-fact or source-AST fallback boundary.

### 2026-06-25 -- MIR JSON hard path admits mixed and nested struct-field fixtures

- Promoted `struct_mixed_fields.pgy` and `struct_nested_fields.pgy` from the
  codegen parity manifest into the MIR JSON hard path. Both already consume
  struct field facts in self-hosted codegen; this session verified the full
  `pgy --mir-json | mir_lower | codegen | gcc == C oracle` path before adding
  them to the ratcheted manifest.
- The MIR JSON frontier moves to **84 PASS / 0 gap plus 1 clean reject**. The
  remaining clean reject at this point was still unsupported role declaration
  fact coverage; the follow-up session above turns it into a fact-owned role
  semantic-support boundary.

### 2026-06-24 -- Codegen nested struct fields consume field facts

- Extended the same struct-field fact path to struct-valued fields. Previously
  self-hosted codegen could route primitive field facts, but `Line.start: Vec2`
  remained a clean reject despite the C oracle supporting it.
- `CollectStructs` now accepts previously declared struct field types,
  `struct_value_emit.pgy` recursively lowers struct-valued field literals, and
  `ExprKind` resolves dotted member chains such as `line.end.x` through
  `Struct.field=field:Type` facts instead of defaulting to `Int`.
- Added `struct_nested_fields.pgy` to the codegen parity manifest. Gate:
  `make self-host-codegen-parity-test-smoke` now proves **59 fixtures**
  run-stdout equal across C and LLVM tool builds.

### 2026-06-24 -- Codegen struct field facts expand to Bool/Float/String

- Closed the self-hosted codegen struct-field type seam. `CollectStructs` now
  records `Struct.field=field:Type` facts, `ExprKind` consumes those facts for
  member reads, and `struct_value_emit.pgy` routes literal initializers by the
  collected type instead of assuming integer fields.
- Added `struct_mixed_fields.pgy` to the codegen parity manifest. It proves
  Float field reads/arithmetic, Bool field conditions, String field reads, and
  Int field reads against the live C oracle and the self-hosted C/LLVM codegen
  tool legs.
- Gate: `make self-host-codegen-parity-test-smoke` now proves **58 fixtures**
  run-stdout equal across C and LLVM tool builds.

### 2026-06-24 -- MIR-lower hard gate admits an example-origin binary search fixture

- Promoted `examples/binary_search.pgy` into the committed MIR JSON hard path,
  proving the example through `pgy --mir-json | mir_lower | codegen | gcc`
  against the C backend oracle.
- `mir_json_parity.sh` now has an explicit `EXAMPLE_FIXTURES` inventory so
  real example-origin programs can be ratcheted without copying them into the
  self-host fixture directory.
- The MIR JSON frontier moves to **82 PASS / 0 gap plus 1 clean reject**; the
  remaining clean reject is still unsupported role declaration fact coverage.

### 2026-06-24 -- ArrayReverse exits the clean-reject boundary

- Promoted `ArrayReverse(Array<Int>)` from the unsupported self-hosted codegen
  builtin boundary into the C/LLVM codegen parity manifest and MIR JSON hard
  path.
- `type_env`, `expr_rewrite`, and `program_emit` now own the `ArrayReverse`
  decision: it type-routes as `ArrayInt`, rewrites to `pgy_ai_reverse`, and
  emits a fresh reversed `Array<Int>` value rather than mutating the caller's
  storage.
- The MIR JSON frontier moves to **81 PASS / 0 gap plus 1 clean reject**; the
  remaining clean reject is unsupported declaration fact coverage, not
  `ArrayReverse`.

### 2026-06-24 -- Semantic numeric type frontier expands to 68 fixtures

- Added `Long` and `Float` local-arithmetic verdict fixtures to the
  self-hosted semantic parity suite.
- Moved numeric arithmetic result typing behind `expr_type_owner.pgy`: same-type
  `Int`/`Long`/`Float` operands preserve their type, while mixed numeric
  arithmetic remains outside the rung instead of being guessed.
- Added the `ToFloat` builtin signature and a contextual integer-literal
  assignment rule for `Long`; no broader implicit numeric promotion is opened.
- Ratcheted `semantic_parity.sh` inventory to 68 fixtures in
  `self_hosted_component_contract_smoke.sh`.

### 2026-06-24 -- MIR-lower codegen frontier expands to 80 fixtures

- Promoted `array_combinators`, `result_int_core`, and `string_utils_core` from
  codegen-only parity into the MIR JSON fact-only parity path.
- `tests/self_hosted/parity/mir_json_parity.sh` now proves **80 fixtures / 2
  clean rejects** through `pgy --mir-json | mir_lower | codegen == C oracle`.
- Updated the self-host progress/status/scorecard docs and the component
  contract ratchet so the new MIR-lower frontier cannot drift back to the old
  count.

### 2026-06-24 -- MIR-lower frontier wording is ratcheted to the parity inventory

- Tightened `tests/self_hosted_component_contract_smoke.sh` so the MIR JSON
  positive fixture inventory must remain **77**, the clean-reject inventory must
  remain **2**, and the hard-self-host scorecard must cite the same
  **77 PASS / 0 gap plus 2 clean rejects** frontier.
- Removed the stale old fixture-count wording from
  `docs/self_hosted/07_hard_self_host_scorecard.md`.
- Updated `src/self_hosted/codegen/README.md`: round-trip codegen
  self-compilation is already achieved, so the next rung is broader MIR-JSON
  driven substitution rather than redoing the bootstrap milestone.

### 2026-06-24 -- Self-host compiler-stage owner shape is gated

- Added an executable owner-shape contract to
  `tests/self_hosted_component_contract_smoke.sh`.
- Active compiler-stage `main.pgy` files (`lexer`, `parser`, `semantic`,
  `codegen`, and `mir_lower`) must now stay entrypoint-only: exactly one
  `Main`, no local helper functions, and no control-flow/string-scan/JSON-fact/
  diagnostic construction work in the entrypoint.
- The same gate enforces the 600-line split-review cap for active
  compiler-stage owner `.pgy` files. New semantics must move behind a named
  source-of-truth owner module, not into `main.pgy` or a generic helper bucket.
- Verified with `make self-host-component-contract-test-smoke`.

### 2026-06-23 -- Semantic run boundary leaves the entrypoint

- Split `src/self_hosted/semantic/semantic_run_owner.pgy` out of `main.pgy`.
  The new owner owns missing-input diagnostics, source-bundle selection, program
  checking, and final deterministic semantic verdict emission.
- Tightened `tests/self_hosted_component_contract_smoke.sh` so semantic must
  keep the run owner imported by the entrypoint.
- Verified with: `bash tests/self_hosted_component_contract_smoke.sh`,
  `make self-host-semantic-parity-test-smoke`, `make test-inc-size-test-smoke`,
  and `make self-host-preparation-test-smoke`.

### 2026-06-23 -- Parser root Program assembly leaves the entrypoint

- Split `src/self_hosted/parser/program_parse_owner.pgy` out of `main.pgy`.
  The new owner owns root source reads, root cursor initialization, top-level
  declaration parse invocation, and final compact AST `Program:` assembly.
- Tightened `tests/self_hosted_component_contract_smoke.sh` so parser must keep
  the Program owner imported by the entrypoint.
- Verified with `bash tests/self_hosted_component_contract_smoke.sh`,
  `make self-host-parser-parity-test-smoke`, `make test-inc-size-test-smoke`,
  and `make self-host-preparation-test-smoke`.

### 2026-06-23 -- MIR lower input and Program assembly leave the entrypoint

- Split `src/self_hosted/mir_lower/mir_json_input_owner.pgy` and
  `src/self_hosted/mir_lower/program_lower.pgy` out of `main.pgy`.
  The input owner owns argv path selection, file reads, and MIR JSON schema
  gating; the Program owner owns document-order assembly and supported routine
  selection.
- Tightened `tests/self_hosted_component_contract_smoke.sh` so `mir_lower`
  must keep both owners imported by the entrypoint.
- Verified `bash tests/self_hosted_component_contract_smoke.sh` and
  `make self-host-mir-json-parity-test-smoke`; MIR JSON parity remains
  **77 fixtures / 2 clean rejects** through
  `pgy --mir-json | mir_lower | codegen == C oracle`.

### 2026-06-23 -- Codegen AST input leaves the entrypoint

- Split `src/self_hosted/codegen/ast_input_owner.pgy` out of `main.pgy`.
  It owns AST path selection, the no-argument `hello_ast.txt` probe default,
  missing-file diagnostics, and the AST `ReadFile` boundary.
- Tightened `tests/self_hosted_component_contract_smoke.sh` so codegen must keep
  the AST-input owner imported by `main.pgy`.
- Refreshed self-host LOC accounting to the current direct owner-file count:
  lexer/parser/semantic/codegen now measure **9713 Pergyra LOC** against
  254,742 C/header/inc LOC, about **3.81%**.

### 2026-06-23 -- Lexer real-source selfcheck leaves the concat bridge

- Replaced the remaining lexer `selfcheck_sources.sh` bridge with the real
  `src/self_hosted/lexer/main.pgy` entrypoint. The semantic checker now sees
  the lexer owner imports through `source_bundle_owner.pgy`; no temporary
  import-stripped unit is generated.
- Removed the obsolete `fixture/source.txt` input side channel from
  `src/self_hosted/lexer/main.pgy`. Lexer input is now `Args()[0]` or the
  no-arg `examples/hello.pgy` probe default.
- Tightened `tests/self_hosted_component_contract_smoke.sh` so the retired
  lexer concat bridge and source-file fallback cannot reappear silently.

### 2026-06-23 -- Parser input boundary unified on argv

- Removed the legacy `fixture/source.txt` source override from
  `src/self_hosted/parser/main.pgy`; no-arg runs still default to
  `examples/hello.pgy`, while parity/scale runs use `Args()[0]`.
- Rewrote `tests/self_hosted/parity/parser_scale_probe.sh` to invoke the parser
  through the same argv path as the hard parity gate and to compare AST outputs
  through files with `cmp -s`, avoiding shell string interpretation as a hidden
  comparison path.
- This keeps parser substitution as an owner-shaped tool boundary: one source
  input channel, one oracle comparison channel, and no probe-only side file.
- The stricter file-based comparison exposed a real scale-only drift:
  `if let Some(resource)` and similar payload bindings were emitted as
  `Case: Some()` because the generated parser depended on branch-local
  `String` reassignment. Moved that responsibility behind
  `ParseIfLetPayload`, which returns the payload fact directly.
- Verified `tests/self_hosted/parity/parser_scale_probe.sh --failing` with
  **120/121 byte-equal**, 0 drift, 0 self-host failures, 1 C skip
  (`secure_slots`), and `make LLVM_ENABLED=0 BUILD_DIR=.tmp/pgy-build-c
  BIN_DIR=.tmp/pgy-bin-c self-host-parser-parity-test-smoke` green at
  **188 fixtures**.

### 2026-06-23 -- Lexer measured corpus closed and escape fixture gated

- Moved `tests/self_hosted/parity/lexer_scale_probe.sh` from the legacy
  `fixture/source.txt` override to the real `Args()[0]` invocation boundary and
  widened the measured corpus from examples-only to examples +
  `tests/cases/backend_compare/**/main.pgy`.
- Fixed `src/self_hosted/lexer/scan_owner.pgy` so ordinary string scanning
  consumes backslash escapes before testing for the closing quote, matching the
  C lexer on `\"` and `\\`.
- Promoted `tests/cases/backend_compare/string_escape_sequences/main.pgy` into
  the hard lexer parity gate, moving the committed lexer fixture set from 6 to
  **7 fixtures**.
- Verified `tests/self_hosted/parity/lexer_scale_probe.sh --failing` with
  **993/993 byte-equal**, 0 drift, 0 self-host failures, 0 C skips, and
  `make LLVM_ENABLED=0 BUILD_DIR=.tmp/pgy-build-c BIN_DIR=.tmp/pgy-bin-c
  self-host-lexer-parity-test-smoke` green.

### 2026-06-23 -- Semantic checker split into SoT owner modules

- Split `src/self_hosted/semantic/main.pgy` from a 1642-line checker into a
  39-line orchestration entrypoint plus source-of-truth owners:
  `text_scan_owner`, `diagnostic_owner`, `env_owner`, `expr_type_owner`,
  `call_check_owner`, `body_check_owner`, and `program_check_owner`. Every
  semantic source file was below the 600-line owner cap; later expression
  diagnostic splitting made `body_check_owner` the largest semantic source at
  304 lines.
- Updated `semantic_parity.sh` to copy the full semantic source bundle into the
  scratch build directory instead of assuming a single-file tool.
- Verified `make LLVM_ENABLED=0 BUILD_DIR=.tmp/pgy-build-c
  BIN_DIR=.tmp/pgy-bin-c self-host-semantic-parity-test-smoke`: **65 fixtures**
  still match the C oracle verdicts.

### 2026-06-23 -- Ability declarations enter MIR JSON lowering

- Promoted top-level ability declarations from the unsupported-declaration
  boundary into the hard MIR JSON path. The C MIR JSON emitter now writes
  `kind:"ability"` declaration facts with MIR-owned method parameter and return
  type names; `mir_lower` reconstructs `[export] Ability:` from those facts.
- Taught the self-host codegen pre-passes to treat `Ability:` as a
  zero-artifact declaration host, so nested ability signatures do not leak into
  function environments or forward declarations.
- Added `ability_decl.pgy` to the MIR JSON manifest. Role declarations remain a
  clean reject until role impl/body semantics have their own owner facts.
  Verified `PGY_BIN=/tmp/pgy-PergyraLang-bin/pgy
  tests/self_hosted/parity/mir_json_parity.sh`: **77 positive fixtures plus 2
  clean rejects** pass through
  `pgy --mir-json | mir_lower | codegen == C oracle`.

### 2026-06-23 -- Semantic program input moved to a source-bundle owner

- Added `src/self_hosted/semantic/source_bundle_owner.pgy` as the owner of the
  root-source/import graph fact consumed by `CheckProgram`. `main.pgy` now only
  reads `Args()[0]`, calls `LoadSemanticSourceBundle`, and renders the verdict.
- Added `valid_import_call.pgy` plus an imported sibling fixture so semantic
  parity proves imported function signatures are consumed from the bundle. The
  semantic parity gate now covers **66 fixtures** and passes on C and LLVM.
- Removed the grep-concatenated semantic unit from `selfcheck_sources.sh`.
  `src/self_hosted/semantic/main.pgy` is now checked as a real imported source
  bundle. `CharCode` and `CharAtN` were added to the semantic builtin signature
  table so the existing lexer real-source unit stays green.

### 2026-06-23 -- MIR lower split into SoT owner modules

- Split `src/self_hosted/mir_lower/main.pgy` from a 1060-line monolith into a
  thin orchestration entrypoint plus source-of-truth owners:
  `error_owner`, `mir_json_input_owner`, `json_fact_read`, `stmt_render`,
  `routine_inventory_owner`, `routine_lower`, `program_lower`, and
  `decl_lower`. Every `mir_lower` source file is now below the 600-line owner
  cap; `routine_lower` is the largest at 431 lines.
- Preserved the fact-only lowering boundary. JSON access, declaration
  inventory reconstruction, statement rendering, and routine/CFG lowering now
  have named owners instead of sharing a generic `main.pgy` bucket.
- Verified `make LLVM_ENABLED=0 BUILD_DIR=.tmp/pgy-build-c
  BIN_DIR=.tmp/pgy-bin-c self-host-mir-json-parity-test-smoke`: **72 positive
  fixtures plus 2 clean rejects** still pass through
  `pgy --mir-json | mir_lower | codegen == C oracle`.

### 2026-06-23 -- Option<Int> match facts enter self-host MIR JSON lowering

- Added explicit MIR JSON match facts for Option-like cases:
  `match_variant` records `Some` / `None`, and `match_bindings` records
  fact-owned payload names such as `v`. The self-hosted MIR lowerer now
  reconstructs `Some(v)` as an `IsSome(subject)` branch plus
  `Let: v : Int = UnwrapOption(subject)`, and reconstructs `None` as
  `!IsSome(subject)`, without parsing transitional source text.
- Promoted the `Option<Int>` value surface into self-hosted codegen:
  `Some`, `None`, `IsSome`, and `UnwrapOption` lower through a local
  value-passed `pgy_option_int` helper.
- Added `option_int_core.pgy` to codegen parity and `option_match.pgy` to the
  MIR JSON hard path. Verified codegen parity at **55 fixtures**, MIR JSON
  lowering parity at **72 positive fixtures plus 2 clean rejects**, and
  refreshed the examples-scale survey to 48 PASS, 24 CODEGEN-gap, 36
  MIR-LOWER-gap, 13 ORACLE-skip. `option_test` now passes through the
  self-host MIR JSON -> C path.

### 2026-06-23 -- Codegen owner split and Result<Int> try enter self-host codegen

- Split `src/self_hosted/codegen/main.pgy` from a monolithic implementation into
  a thin CLI entrypoint plus responsibility-owned modules:
  `text_owner`, `type_env`, `expr_scan`, `expr_rewrite`, `stmt_emit`,
  `function_emit`, and `program_emit`. Each codegen source file is now below
  the 600-line design target while preserving the import-merged self-host
  bootstrap path.
- Promoted postfix `?` for the supported `Result<Int>` surface. `Let: x : Int =
  Call(...)?` inside a `Result<Int>` function now lowers to a temporary
  `pgy_result_int`, emits the active defer stack before propagating `Err`, and
  binds the unwrapped payload on the success path. `ToString(String)` now routes
  through the string path instead of printing a pointer-shaped integer.
- Added `result_try.pgy` to codegen parity and the MIR JSON hard path. Verified
  codegen parity at **54 fixtures**, MIR JSON lowering parity at **70 positive
  fixtures plus 2 clean rejects**, and refreshed the examples-scale survey to
  47 PASS, 25 CODEGEN-gap, 36 MIR-LOWER-gap, 13 ORACLE-skip. `pipe_and_try`
  now passes through the self-host MIR JSON -> C path.

### 2026-06-23 -- Defer body facts enter self-host MIR JSON lowering

- Added an explicit `defer_body` MIR JSON fact for `AST_DEFER_STMT` instructions.
  The self-hosted MIR lowerer now reconstructs `Defer: / Block:` from that fact
  instead of inheriting the lossy `expr0:"{...}"` inline block placeholder.
- Extended self-hosted codegen with block-local defer scope-exit emission in
  LIFO order. Return paths emit the currently active defer stack before
  returning; broader resource/defer semantics stay outside the supported subset
  until the native backend path's cleanup model is substituted.
- Added `defer_scope.pgy` to codegen parity and the MIR JSON hard path, moving
  codegen parity to **53 fixtures** and MIR JSON lowering parity to **69
  positive fixtures plus 2 clean rejects**. Refreshed examples-scale survey:
  46 PASS, 26 CODEGEN-gap, 36 MIR-LOWER-gap, 13 ORACLE-skip, and 0 measured
  STDOUT-diff / generated-C compile failures / via-run timeouts. `defer_test`
  now passes.

### 2026-06-23 -- Result<Int> values enter self-host codegen

- Promoted the non-try `Result<Int>` value surface into self-hosted codegen:
  `Ok`, `Err`, `IsOk`, `IsErr`, `Unwrap`, and `UnwrapOr` now lower through a
  local value-passed `pgy_result_int` helper instead of stopping at the result
  builtin boundary.
- Kept postfix `?` as an explicit clean CODEGEN gap because early-return
  lowering needs control-flow ownership, not token rewriting. The self-hosted
  codegen gate now rejects `?` before generated C emission with a structured
  unsupported-codegen diagnostic.
- Added `result_int_core.pgy` to the codegen parity manifest, moving codegen
  parity to **52 fixtures**. Refreshed examples-scale survey: 45 PASS, 27
  CODEGEN-gap, 36 MIR-LOWER-gap, 13 ORACLE-skip, and 0 measured STDOUT-diff /
  generated-C compile failures / via-run timeouts. `result_test` now passes;
  `pipe_and_try` fails closed at `?`.

### 2026-06-23 -- Array<Int> combinators enter self-host codegen

- Promoted the `Array<Int>` `ArraySort`, `ArrayMap`, and `ArrayFilter`
  codegen surface out of the unsupported builtin boundary. The self-hosted
  emitter now lowers them to owned C helpers for sorted shared-buffer array
  values and unary `Int -> Int` / `Int -> Bool` function references.
- Kept the clean-reject contract alive at that point by moving the negative
  builtin fixture to `ArrayReverse`, which then remained unsupported and had to
  fail before generated C emission. This was superseded on 2026-06-24 when
  `ArrayReverse(Array<Int>)` entered the supported codegen surface. The MIR JSON
  gate still proved **68 positive fixtures plus 2 clean rejects** in that
  session.
- Added `array_combinators.pgy` to the codegen parity manifest, moving codegen
  parity to **51 fixtures**. Refreshed examples-scale survey: 44 PASS, 28
  CODEGEN-gap, 36 MIR-LOWER-gap, 13 ORACLE-skip, and 0 measured STDOUT-diff /
  generated-C compile failures / via-run timeouts. `collection_ops` now passes.

### 2026-06-23 -- String utility builtin aliases enter self-host codegen

- Closed the `string_utils` examples-scale CODEGEN gap for the standard string
  utility spelling. `Join(xs, sep)` now lowers through the same self-hosted
  runtime helper as `StringJoin(xs, sep)`, and `ToFloat(s)` lowers through the
  same scalar-conversion path as the C oracle (`atof`) instead of remaining an
  unsupported builtin boundary.
- Added `string_utils_core.pgy` to the self-host codegen parity manifest. The
  fixture proves `Join(Array<String>, String)` plus `ToFloat(String)` run-stdout
  equivalence against the C oracle, moving codegen parity to **50 fixtures**.
- Refreshed the examples-scale survey after rebuilding the MIR-lower parity
  tools: 43 PASS, 29 CODEGEN-gap, 36 MIR-LOWER-gap, 13 ORACLE-skip, and 0
  measured STDOUT-diff / generated-C compile failures / via-run timeouts.
  `string_utils` now passes; remaining gaps stay fail-closed around domain
  nominal declarations, events, generics, slots/channels/futures/results, and
  collection higher-order helpers.

### 2026-06-23 -- Plain class declarations and methods enter MIR JSON lowering

- Promoted ordinary `class` declarations from the unsupported declaration
  boundary into the self-host MIR JSON path. `mir_dump_json` now emits
  `kind:"class"`, `nominal_kind:"class"`, field facts, method facts, and routine
  `owner` facts; `mir_lower` reconstructs `Class:` / `Methods:` from those MIR
  facts without reading transitional source text.
- Extended the Pergyra self-hosted codegen to lower value classes through the
  existing nominal struct ABI, including field-order-owned positional
  constructors (`Vec2(3, 7)`) and owner-prefixed method calls
  (`v.Length()` -> `Vec2_Length(v)`). Subject/object/tobject/vessel surfaces
  remain clean rejects because their projection/identity semantics need their
  own owner facts.
- The hard MIR JSON gate now proves **68 positive fixtures plus 2 clean
  rejects**. Refreshed examples-scale survey: 42 PASS, 30 CODEGEN-gap, 36
  MIR-LOWER-gap, 13 ORACLE-skip, and 0 measured STDOUT-diff / generated-C
  compile failures / via-run timeouts. `class_method_test` and `class_test`
  moved to PASS; `enum_test` moved to PASS through MIR-owned variant facts;
  `tagged_union` now reaches an explicit payload-enum MIR-LOWER gap instead of
  failing at a generic enum declaration boundary; `generic_class` now reaches an
  explicit generic-field CODEGEN-gap instead of failing at declaration
  inventory.

### 2026-06-23 -- Non-struct class declarations fail closed in MIR JSON lowering

- Closed another declaration-inventory SoT gap: non-struct `AST_CLASS_DECL`
  declarations (`class`, `subject`, `object`, `tobject`, `vessel` surfaces)
  are no longer omitted from MIR JSON `decls`. Plain `struct` declarations stay
  supported; non-struct class declarations are emitted as explicit unsupported
  facts until the self-host path owns their field/method/projection semantics.
- Added `unsupported_class_decl.pgy` to the MIR JSON gate. The gate now proves
  **65 positive fixtures plus 3 clean rejects**: unsupported ability/role,
  unsupported non-struct class, and unsupported codegen builtin boundaries.
- Refreshed the examples-scale survey after this stricter inventory rule: 40
  PASS, 29 CODEGEN-gap, 39 MIR-LOWER-gap, 13 ORACLE-skip, and 0 measured
  STDOUT-diff / generated-C compile failures / via-run timeouts. This is an
  intentional honesty correction: several previous PASS cases had non-struct
  class/domain declarations that the self-host path silently dropped.

### 2026-06-23 -- Array destructure binding facts promoted into MIR JSON lowering

- Promoted array destructuring from a clean reject into the self-host MIR JSON
  lowering subset. `mir_dump_json` now emits the MIR-owned
  `destructure_bindings` list, and `mir_lower` reconstructs each binding as a
  typed array-index `Let:` from the initializer and source-local array type
  facts. Direct `Split(...)` destructures materialize a fact-owned temporary
  array before indexing.
- Renamed the old negative destructure fixture to `array_destructure.pgy` and
  moved it into the positive manifest. The gate checks the binding facts,
  reconstructed temporary, and first binding before running the full
  `pgy --mir-json | mir_lower | codegen | gcc == C oracle` path.
- Unsupported self-host codegen builtins (`ArraySort`, `ArrayMap`,
  `ArrayFilter`, `Join`, `ToFloat`) now fail closed with `CODEGEN ERROR`
  instead of leaking undefined C symbols. `unsupported_codegen_builtin.pgy`
  locks that boundary into the same MIR JSON gate. The hard MIR JSON gate now
  proves **65 positive fixtures plus 2 clean rejects**.
- Refreshed the examples-scale survey: 50 PASS, 39 CODEGEN-gap, 19
  MIR-LOWER-gap, 13 ORACLE-skip, and **0 measured STDOUT-diff, generated-C
  compile failures, or via-run timeouts**. `word_count` moved to PASS; the
  larger `collection_ops` / `string_utils` examples now stop at explicit
  CODEGEN-gap boundaries for unsupported builtins.

### 2026-06-23 -- Loop headers are classified by CFG backedges, not phi alone

- Closed the `heap` via-run timeout class in the examples-scale MIR JSON path.
  The self-host `mir_lower` previously treated any branch block with phi facts
  as a loop header; inner `if` branches inside loops can also carry phi facts
  when they join reassigned locals, so they were reconstructed as `While:`
  nodes and could hang the via binary.
- `mir_lower` now requires an incoming successor backedge to the current block
  before a phi-bearing branch is classified as a loop. `nested_if_in_loop.pgy`
  locks the regression case into the hard MIR JSON gate: the inner break guard
  must remain an `If:`, and `right < size` / `largest == cur` must not be
  rendered as loops.
- The hard MIR JSON gate now proves **64 positive fixtures plus 2 clean
  rejects**. A direct `examples/heap.pgy` oracle-vs-self-host MIR path check
  also passes (`pgy --mir-json | mir_lower | codegen | gcc == C oracle`) without
  timeout.

### 2026-06-23 -- Destructure lowering fails closed instead of broken C

- Closed the remaining generated-C failure class in the examples-scale MIR JSON
  path. `string_utils`, `collection_ops`, and `word_count` contain destructuring
  that the current self-host `mir_lower` cannot reconstruct from binding facts
  yet; previously the `destructure` instruction fell through as a bare
  expression, yielding undeclared C identifiers.
- `mir_lower` now rejects `kind:"destructure"` with a visible `MIR-LOWER
  ERROR`, and `unsupported_destructure.pgy` locks that behavior into the hard
  MIR JSON gate. The gate now proves **63 positive fixtures plus 2 clean
  rejects**.
- Refreshed the examples-scale survey: 48 PASS, 37 CODEGEN-gap, 22
  MIR-LOWER-gap, 13 ORACLE-skip, and 1 via-run timeout (`heap`). There are now
  **0 measured STDOUT-diff cases and 0 generated-C compile failures** in this
  examples-scale path.

### 2026-06-23 -- Self-host codegen I/O path policy matches runtime default

- Closed the last measured examples-scale STDOUT-diff (`io_test`). The C
  runtime denies absolute file paths by default unless `PGY_IO_ALLOW_ABSOLUTE=1`;
  the self-hosted codegen helpers previously used raw `fopen`, so they allowed
  `/tmp/...` and produced different output.
- Added `pgy_path_allowed(...)` to the self-hosted generated helper surface and
  routed `FileOpen`, `FileExists`, `ReadFile`, and `WriteFile` through it.
  Added `io_absolute_policy.pgy` to the codegen and MIR JSON gates. Codegen
  parity is now **49 fixtures**; MIR JSON parity is now **63 positive fixtures
  plus 1 clean reject** at this point in the log.
- Refreshed the examples-scale survey: 48 PASS, 39 CODEGEN-gap, 19
  MIR-LOWER-gap, 13 ORACLE-skip, 1 CC-fail (`string_utils`), and 1 via-run
  timeout (`heap`). There are now **0 measured STDOUT-diff cases** in the
  examples-scale MIR JSON self-host path.

### 2026-06-23 -- Match-case pattern facts promoted into MIR JSON lowering

- Closed the `match_test` silent-output class for the self-host MIR JSON path.
  `pgy --mir-json` now emits `match_patterns` for match-case branch
  instructions, and `mir_lower` reconstructs integer case conditions as
  `subject == pattern` from that fact instead of treating the match subject
  itself as a Bool condition.
- Added `match_case_int.pgy` to the hard MIR JSON manifest and gate checks for
  `"match_patterns":["1"]`, `"2"`, and `"3"`, plus a reconstructed `If: x == 3`
  line. The hard MIR JSON gate now proves **62 positive fixtures plus 1 clean
  reject**.
- Refreshed the examples-scale survey: 47 PASS, 39 CODEGEN-gap, 19
  MIR-LOWER-gap, 13 ORACLE-skip, 1 STDOUT-diff (`io_test`), 1 CC-fail
  (`string_utils`), and 1 via-run timeout (`heap`).

### 2026-06-23 -- Unsupported declarations fail closed in MIR JSON lowering

- Closed the `operator_overload` silent-output class for the self-host MIR JSON
  path. `pgy --mir-json` now emits unsupported declaration facts for
  out-of-subset ability/role/enum/event declarations instead of letting
  `mir_lower` ignore the declaration inventory and generate a plausible but
  semantically incomplete C program.
- Added `unsupported_ability_decl.pgy` as a negative fixture. The hard MIR JSON
  gate now proves **61 positive fixtures plus 1 clean reject**: ability and role
  unsupported facts must be present in MIR JSON, and `mir_lower` must produce a
  visible `MIR-LOWER ERROR` rather than silently continuing.
- Refreshed the examples-scale survey after the clean-reject cutover: 46 PASS,
  39 CODEGEN-gap, 19 MIR-LOWER-gap, 13 ORACLE-skip, 2 STDOUT-diff (`io_test`,
  `match_test`), 1 CC-fail (`string_utils`), and 1 via-run timeout (`heap`).
  This intentionally moves unsupported ability/enum/event examples out of the
  silent-wrong-output bucket and into clean rejection.

### 2026-06-23 -- Random return facts promoted into MIR source-local typing

- Closed the shared `bsd_test6` / `bsd_test9` / `bsd_test11` malformed
  assignment gap. The source-local type owner knew unannotated literals such as
  `let running = true`, but did not type `let event = Random(4)`, so MIR JSON
  omitted `event -> Int` and `mir_lower` could not reconstruct a `Let:` line
  from facts.
- Added `Random() -> Int` to the MIR source-local builtin call type owner and
  promoted `random_inferred_let.pgy` into the hard MIR JSON manifest. The hard
  rung moves from 60 to **61 fixtures**.
- Refreshed the examples-scale survey: 54 PASS, 47 CODEGEN-gap, 13 ORACLE-skip,
  3 STDOUT-diff (`io_test`, `match_test`, `operator_overload`), 3 CC-fail
  (`enum_test`, `event_minimal`, `string_utils`), and 1 via-run timeout
  (`heap`).

### 2026-06-23 -- Non-empty loop break edges promoted into MIR JSON lowering

- Ran an examples-scale MIR JSON survey after closing the committed fixture
  inventory. Baseline over `examples/*.pgy`: 49 PASS, 50 CODEGEN-gap, 13
  ORACLE-skip, 5 STDOUT-diff, 3 CC-fail, and 1 MIR-LOWER-timeout. The highest
  priority class is silent wrong output, not out-of-subset rejection.
- Closed the `binary_search` stdout divergence. The CFG already encoded
  `break` as a successor edge from a block with statements to the loop exit, but
  `mir_lower` only emitted `Break` / `Continue` for empty edge blocks. It now
  consumes loop successor facts after non-empty statement blocks too.
- Added `break_after_stmt.pgy` to the hard MIR JSON fixture set, moving the
  hard rung from 59 to **60 fixtures** and preventing this edge fact from
  regressing into silent duplicate execution.
- Refreshed the examples-scale survey after the fix: 51 PASS, 50 CODEGEN-gap,
  13 ORACLE-skip, 3 STDOUT-diff, 3 CC-fail, and 1 via-run timeout (`heap`).
  Next priority remains the remaining silent-output / generated-C failures
  before broad out-of-subset feature work.

### 2026-06-23 -- Struct declaration facts close the committed MIR JSON fixture inventory

- Closed the last two measured MIR JSON fixture gaps (`struct_point`,
  `struct_param`). The C MIR JSON emitter now writes additive declaration facts
  under `decls` for `NOMINAL_DECL_STRUCT` headers using `MIRDeclHeader` /
  `MIRDeclField` metadata. The self-hosted `mir_lower` consumes those facts and
  reconstructs `Struct:` / `Fields:` tree lines before routine bodies.
- Promoted `struct_point` and `struct_param` into the hard MIR JSON manifest and
  added gate checks that their struct names and Int fields are present in MIR
  JSON and reconstructed by `mir_lower`.
- The committed MIR-lower/codegen fixture inventory now measures **59 PASS / 0
  gap** through `pgy --mir-json | mir_lower | codegen | gcc == C oracle`.

### 2026-06-23 -- Array for-each facts promoted into MIR JSON lowering

- Closed the `array_pop` and `for_each` reconstructed-C failures without opening
  source text. For collection loops the MIR JSON branch facts carry
  `expr0 == expr1 == collection`, and the routine `source_locals` fact owns the
  collection type. `mir_lower` now renders `For: item in collection` when that
  source-local type is `Array<...>`; range loops still render
  `For: i in start..stop`.
- Promoted `array_pop` and `for_each` into the hard MIR JSON manifest, moving
  the hard rung from 55 to **57 fixtures**.
- Remaining measured MIR JSON fixture boundary: **57 PASS / 2 gap**. Both
  remaining gaps (`struct_point`, `struct_param`) are self-hosted codegen struct
  support gaps.

### 2026-06-23 -- Assignment facts promoted into MIR JSON lowering

- Closed the `str_reassign` gap without adding a source-text fallback. The MIR
  JSON already carried `kind:"assign"` plus target/value facts
  (`expr0`/`expr1`); `mir_lower` now renders those facts as
  `Assign: target = value` and fails closed if an assignment instruction lacks
  either fact.
- Promoted `str_reassign` into the hard MIR JSON manifest, moving the hard rung
  from 54 to **55 fixtures**.
- Remaining measured MIR JSON fixture boundary: **55 PASS / 4 gap**. `array_pop`
  and `for_each` still fail at reconstructed-C compile time, while
  `struct_point` and `struct_param` remain self-hosted codegen gaps.

### 2026-06-23 -- MIR JSON hard rung widened to 54 and coverage probe made fail-closed

- Made `tests/self_hosted/parity/mir_json_coverage_probe.sh` fail closed: it now
  runs with `set -euo pipefail`, removes stale generated gen0
  `mir_lower.exe` / `codegen.exe` before rebuilding, and asserts both tools are
  executable before classifying coverage. This closes the measurement false
  positive where a failed gen0 build could still report PASS using stale `.tmp`
  tools.
- Surveyed the full committed MIR-lower/codegen fixture inventory through the
  same hard path (`pgy --mir-json | mir_lower | codegen | gcc`) against the C
  oracle: **54 PASS / 5 gap**. Promoted the 11 newly verified PASS fixtures into
  the hard manifest: `log_trailing_newline`, `nested_concat`,
  `str_array_concat`, `str_builtins2`, `str_case_math`, `str_greet`,
  `str_indexof`, `str_trim`, `two_logs`, `while_break`, and `while_sum`.
- Left the five measured gaps out of the hard count: `array_pop` and `for_each`
  fail at reconstructed-C compile time, while `str_reassign`, `struct_point`,
  and `struct_param` are self-hosted codegen gaps.
- `self-host-mir-json-parity-test-smoke`: proved **54/54 MIR JSON ->
  self-hosted MIR-lower -> self-hosted codegen -> C oracle parity**.

### 2026-06-23 -- Loop-control edge blocks promoted into MIR JSON gate

- Closed the `log_int_direct` gap called out below. The self-hosted
  `mir_lower` now treats empty successor blocks that flow to the active loop
  header or loop exit as MIR-owned `Continue` / `Break` facts instead of trying
  to flatten the loop CFG or re-open source text.
- Promoted `log_int_direct` into the hard MIR JSON parity manifest, taking the
  gate from 42 to **43 fixtures**.
- Removed the remaining fact-based flat-walk compatibility branch from
  `mir_lower`; an unsupported or empty CFG now fails closed instead of
  silently dropping back to instruction-order rendering.
- `self-host-mir-json-parity-test-smoke`: proved **43/43 MIR JSON ->
  self-hosted MIR-lower -> self-hosted codegen -> C oracle parity**.

### 2026-06-23 -- Ten more codegen surfaces promoted into MIR JSON gate

- Promoted ten survey-proven PASS fixtures into the hard MIR JSON parity
  manifest: `hello`, `func_call`, `for_sum`, `if_else`, `int_arith`,
  `int_neg`, `int_subdiv`, `builtin_name_literal`, `dir_walk`, and
  `exit_guard`.
- The promotion intentionally leaves `log_int_direct` / loop-control-heavy
  fixture shapes out of the hard manifest: the survey showed a `for` CFG with
  `continue`/`break` back-edges can still hang `mir_lower`, so that remains a
  real self-hosted CFG reconstruction gap rather than a hidden fallback.
- `self-host-mir-json-parity-test-smoke`: expected to prove **42/42 MIR JSON ->
  self-hosted MIR-lower -> self-hosted codegen -> C oracle parity**.

### 2026-06-22 -- Multiple Void routines promoted into MIR JSON gate

- Promoted `multi_func_void` from the coverage probe into the hard
  `mir_json_parity.sh` manifest. The self-hosted MIR lowering now proves that
  multiple Void routine declarations plus bare-call statements reconstruct from
  MIR JSON facts and run equal to the C oracle.
- `make self-host-mir-json-parity-test-smoke`: **32/32 MIR JSON -> self-hosted
  MIR-lower -> self-hosted codegen -> C oracle parity**.

### 2026-06-22 -- Bool literal canonicalization promoted into MIR JSON gate

- Closed the next measured `mir_json_coverage_probe.sh` gap:
  `reassign_block` reconstructed `If: true` from MIR facts, but the self-hosted
  codegen emitted C `if (true)` without a `stdbool.h` contract. The codegen now
  canonicalizes standalone `true`/`false` tokens outside strings/identifiers to
  C truth values `1`/`0`.
- Added `src/self_hosted/mir_lower/fixture/reassign_block.pgy` to the hard MIR
  JSON parity gate, moving the gate from 30 to **31 fixtures**.
- Verified the coverage probe now reports `reassign_block PASS`; the gated MIR
  JSON parity path remains `pgy --mir-json | mir_lower | codegen == C oracle`.

### 2026-06-22 -- MIR JSON hard rung widened to 30 fixtures

- Promoted already passing codegen fixture surfaces into the MIR JSON parity
  gate without changing `mir_lower` semantics. This keeps the move honest: only
  programs that already reconstruct from explicit MIR facts and run-equal to the
  C oracle are counted.
- The hard gate now covers 9 original `mir_lower` fixtures plus 21 selected
  codegen fixtures: args, arrays, Bool/string/Float builtins, concat/equality,
  recursion, `continue`, mixed int/string output, and file handle/read/write.
- Verified with `PGY_BIN=/mnt/e/PergyraLang/bin-codex-hard-full/pgy.exe bash
  tests/self_hosted/parity/mir_json_parity.sh`: **30/30 MIR JSON -> self-hosted
  lowering -> self-hosted codegen -> native run** equal to the C backend oracle.

### 2026-06-22 -- parser examples scale closed to oracle skip

- Extended the self-host parser text-mirror coverage for domain syntax that was
  still blocking the examples corpus: transaction/fail, party/roster/world
  surfaces, ability/role forms, object initializer postfix, dollar string
  interpolation, tuple/pattern erasure, `if let Some(...)`, loop/parallel
  expression forms, and the small type-spelling sugars used by examples.
- Verified with `parser_scale_probe.sh --failing`: **120/121 byte-equal**, zero
  byte-drift, zero self-host parser exits, and one C-oracle skip
  (`secure_slots`).
- This closes the text parser scale rung as far as the live C oracle permits.
  The hard self-hosting direction remains structured front-end ownership and
  fact/verifier expansion, not turning the text mirror into the final parser IR.

### 2026-06-20 -- control-flow reconstruction complete: MIR->C subset 9/9

Picked up the control-flow track that the prior entry scoped as a separate
increment (the BDFL said to split it so it would not become half-finished debt).
Done in two verified increments rather than one risky push:

- **CFG edges + if/else** (`69790209`): the `--mir-json` emitter now carries each
  block's `succ_true`/`succ_false` (additive; from MIRBasicBlock). `mir_lower`
  gained a string-based CFG structurer -- follow the succ edges from the entry
  block, detect the branch, compute the merge (post-dominator) by a region-exit
  walk, emit `If:/Then:/Else:` recursively, continue from the merge. `def`
  reassignments render `Assign:`; `phi` is skipped. Probe 3 -> 7 PASS.
- **while/for loops** (`d9cd034d`): the structurer detects a loop header (a `phi`
  block -> while; a branch whose condition begins `for ` -> for), emits
  `While:`/`For:` + `Block:`, recurses into the body bounded by the header (the
  back-edge returns there), and continues at the loop exit (succ_false). The
  IsLoopRoutine flat-walk split is gone; every routine structures, with the flat
  walk kept only as an empty-region fallback. Probe 7 -> **9/9 PASS**.
- **Regression lock** (`8c5094f4`): promoted the new constructs to the parity
  gate as fixtures (funcparam, ifelse, nestedif, whileloop, forloop), 4 -> 9
  gated. The 9/9 is now CI-protected, not just probe-measured.

Reconstructed ASTs byte-match the `pgy --ast` oracle for every case. The
self-host MIR->C lowering subset (multi-routine, signatures, return, if/else,
nested if, boolean conditions, reassignment, while, for) is covered end to end.

- **Next track (separate increment):** the structurer models single-entry/
  single-exit reducible if/loop shapes. Not yet modeled: a loop nested inside an
  `if` then/else region (RegionExit would walk into the loop and hit its guard),
  `break`/`continue` (early exits out of a loop), and `switch`/`match` lowering.
  Each is its own fixture-backed increment on the same machinery.

### 2026-06-20 -- mir_lower MIR->C SOT: filled multi-routine + return + signatures

The self-host MIR->C lowering path (`pgy --mir-json | mir_lower | codegen | gcc`
== C oracle) had a set of empty parts mapped by the coverage probe
(`tests/self_hosted/parity/mir_json_coverage_probe.sh`). This session filled three,
each a *read-a-fact-already-present* fill (not decompilation):

- **multi-routine** (`df370923`): `mir_lower` walked only one routine and merged
  the rest; added `FindRoutine` + a per-routine span walk. `multi_func_void` PASS.
- **return statement** (`81c09e7a`): reconstruct `Return: <expr>` from a
  `kind="return"` instruction instead of dropping it.
- **params / return-type** (`b7a68d3e`): the schema carried no signatures, so
  `mir_lower` hardcoded empty `Parameters:` / `Returns: Void`. Extended the
  `--mir-json` emitter (`mir_lifecycle.c`, additive: `"params":[{"name","type"}]`,
  `"return"`) and taught `mir_lower` to parse them. `func_param` PASS.

Probe went from **1 PASS to 3 PASS** (`string_concat`, `multi_func_void`,
`func_param`). 4-fixture mir_json parity gate green throughout; all changes
non-colliding with the BDFL's capability-5 MIR files (emitter file was clean;
`mir_lower` edits were mine; the BDFL's `mir_lower` header edit was left intact).

- **Next track (deliberately a SEPARATE session -- do not inline it):** the last
  empty part is **control-flow** (`if`/`while`/`for`, 6 probe cases, all
  `MIR-LOWER-flatten`). Unlike the three fills above, this is **CFG -> structured
  AST decompilation**, qualitatively harder: (1) schema -- emit each block's
  `succ_true`/`succ_false` (the MIR already holds them: `mir.h` MIRBasicBlock
  `succ_true`/`succ_false`/`has_succ_true`/`has_succ_false`); (2) `mir_lower` --
  a structuring algorithm that detects the branch block, identifies then/else/
  merge blocks, emits `If: cond Then {..} Else {..}` and continues from the merge
  (then loop back-edges for `while`, range for `for`, then nesting). Reaching
  byte/run-parity here is multi-step with real edge cases, so it is split off to
  avoid leaving a half-finished structurer as debt. Start with the single
  `if`/`else` case (closes `if_else`, then `nested_if`/`bool_ops`/
  `reassign_block`); `while`, `for`, nesting follow as their own increments.

- Committed `tests/self_hosted/parity/lexer_scale_probe.sh` (`c7adbb1a`) -- fills
  the noted "no committed lexer-scale probe" gap; mirrors the parser probe.
  Initial measurement: 115/121.
- `a856d3d9`: emit `DOC_COMMENT` for `///` (was skipped), matching the oracle's
  text + text-start column. 115 -> 116.
- `62d71ffa`: add missing keywords (transaction, compensate, fail, extends) and
  `$"`/`f"` `INTERPOLATED_STRING` prefixes. 116 -> **121/121, zero drift**.
- Lexer parity gate stayed green throughout (no regression on the 6 fixtures).
- Stayed out of the BDFL's capability-5 MIR files; all changes were in the
  self-host lexer + a new probe.
- **Next session**: apply the same oracle-diff method to the parser examples
  drifts (107/119), or add a golden probe for a new oracle dimension.

### 2026-06-20 -- parser examples baseline + strategic finding

- Ran `parser_scale_probe.sh`: **88/121 byte-equal, 23 byte-drift, 9 parser
  crashes, 1 C-skip**.
- Categorized the 32 failures (oracle-diff):
  - **Crashes (9) = missing parser features**, not small drifts:
    `transaction { ... compensate ... fail }` block (transaction_saga),
    `parallel`/`spawn` (parallel, structured_comments), `$"`/`f"` interpolated
    strings (party_system_demo, world_roster_city), `ability`/`role`/`vessel`
    domain constructs (role_ability_demo, six_item_alignment_demo,
    vessel_action_design), and a type-inference return (infer_return).
  - **Byte-drifts (23) = small AST-emission deltas** (e.g. walrus_test: the
    parser omits a `Returns: Void` line + a blank line).
- **Key finding**: the self-host parser (`src/self_hosted/parser/main.pgy`, 3356
  lines) has its OWN tokenizer and does NOT share the self-host lexer, so the
  lexer's DOC_COMMENT / keyword / interpolated-string fixes do NOT propagate to
  it. Closing the parser crashes means re-adding those + the block constructs in
  the parser's own front matter.
- **Strategic note (read before grinding this)**: per
  `project_self_host_pause_backend_first`, this text-mirror parser is *throwaway*
  in the substitution pivot (it is slated to be rewritten to emit *structured
  AST*, not text). Pushing its byte-exact coverage toward 100% polishes
  rewrite-bound code. It does still extend the live C/LLVM parity surface, so it
  is not worthless -- but it is diminishing-returns feature work, and the higher
  value completion step is the structured-AST rewrite, not more text coverage.
- **Decision deferred to BDFL** (surfaced, not pre-empted): grind parser text
  coverage (cheap parity-surface gains, e.g. the 23 small drifts) vs. begin the
  structured-AST parser rewrite (the real substitution step) vs. a verifier /
  golden-probe track. Lexer (121/121) was load-bearing and done; parser text
  coverage is the explicitly-cautioned area.
- **Empirical confirmation that grinding text coverage is the wrong track**: a
  tiny attempt (default empty return type to `Returns: Void`) did NOT fix the
  target (walrus_test routes through a different emission path) and would have
  broken a committed C-leg fixture (some forms emit no Returns line) -- reverted.
  This is throwaway-bound, fragile feature work; resume the parser only as the
  structured-AST rewrite. (The earlier "LLVM-blocked" note here is now stale --
  see the correction below.)

### 2026-06-20 -- CORRECTION: the parser LLVM-leg blockage was a real bug, now fixed

- The earlier "LLVM leg fails to compile the parser tool
  (`silent i32 fallback is not allowed`, line 14:9)" was NOT just an in-flight
  artifact -- it was a genuine codegen bug, now diagnosed and fixed (by the
  BDFL, in the in-flight LLVM codegen work).
- **Root cause**: a reassignment inside a control-flow block (`if`/`while`/`for`)
  is lowered to an SSA `def` (e.g. `result=x.2 ast=AST_ASSIGNMENT`), unlike a
  flat reassignment which is a plain `assign`. The SSA-DEF LLVM emission derived
  the target type from the nameless AST node instead of the source-local-type
  fact (which already holds `x->Int`), so it hit the fail-closed
  `ast_type_to_llvm`. C silently fell back to i32 (correct only by luck for Int;
  would have miscompiled a String/struct local).
- **Fix verified (no regression)**: reassignment in if/while/for/nested/else and
  the String case all compile on both backends; all four self-host tools compile
  on LLVM; **parser parity is now green on BOTH legs (188 byte-equal, c+llvm)**;
  lexer (6) and codegen (rung 0-15, 48 fixtures) parity unchanged.
- **Consequence**: the parser self-hosts on both backends again -- the LLVM leg
  is no longer blocked. The "don't grind text coverage" conclusion still holds
  (that is BDFL direction, independent of the bug); but parser work *can* now be
  validated on LLVM.

### 2026-06-23 -- parser declaration owner split

- Continued the SoT-owner module split for `src/self_hosted/parser/`.
  `main.pgy` remains the declaration orchestration loop, but the
  self-contained `type`, first-class `ability`, `event`, and `enum` branches
  now live in `decl_type_owner.pgy`, `decl_ability_owner.pgy`,
  `decl_event_owner.pgy`, and `decl_enum_owner.pgy`.
- This is deliberately not a feature expansion. It keeps the same text-tree
  output while moving semantic branch ownership out of the monolithic entry
  file. Recursive `import`/`namespace` and larger domain declarations stay in
  `main.pgy` until their dependency direction can be split without re-opening
  parser recursion.
- The self-host preparation smoke now ratchets the new owner files, imports,
  and entry functions so the branches cannot silently collapse back into
  `main.pgy`.

### 2026-06-23 -- parser entrypoint split to declaration dispatch owner

- Moved `ParseDecls` out of `main.pgy` into `decl_dispatch_owner.pgy`, making
  `main.pgy` parser-tool entrypoint orchestration only. Recursive `import`,
  `namespace`, and `within` flow remains in the dispatch owner because those
  branches genuinely call back into declaration dispatch.
- Split the larger non-recursive declaration branches into owner modules:
  `decl_zone_owner.pgy`, `decl_effect_relation_owner.pgy`, `decl_role_owner.pgy`,
  `decl_intent_owner.pgy`, and `decl_nominal_owner.pgy`. All parser owner files
  now sit below the 600-line cap; `decl_dispatch_owner.pgy` is 257 lines.
- The self-host preparation smoke now requires the new imports/functions,
  forbids `func ParseDecls` from reappearing in `main.pgy`, and enforces the
  600-line parser owner cap.

### 2026-06-23 -- parser source-path owner split

- Moved parser argv/default source selection, source-dir extraction,
  source-relative import path resolution, and imported-source marker creation
  into `src/self_hosted/parser/source_path_owner.pgy`.
- `main.pgy` now only selects the owned source path, reads the root file, and
  invokes declaration dispatch. `decl_dispatch_owner.pgy` consumes the same
  source-path owner for recursive imports instead of recomputing dirname/import
  policy locally.
- Contract ratchet: `tests/self_hosted_component_contract_smoke.sh` now requires
  `source_path_owner.pgy` as part of the parser owner surface. Verified with
  `tests/self_hosted_component_contract_smoke.sh`,
  `tests/self_hosted/parity/parser_parity.sh`,
  `make test-inc-size-test-smoke`, and
  `make self-host-preparation-test-smoke`.

### 2026-06-23 -- lexer source-input owner split

- Moved lexer argv/default source-path selection and source file read failure
  policy into `src/self_hosted/lexer/source_input_owner.pgy`.
- `main.pgy` now only orchestrates the owned input and scanner output:
  `LexerDefaultSourcePath(args)` -> `LexerReadSource(path)` ->
  `LexContent(path, content)`.
- Contract ratchet: `tests/self_hosted_component_contract_smoke.sh` now requires
  `source_input_owner.pgy` as part of the lexer owner surface.

### 2026-06-23 -- codegen run boundary owner split

- Moved codegen CLI-to-output orchestration into
  `src/self_hosted/codegen/codegen_run_owner.pgy`.
- `main.pgy` now stays entrypoint-only: it imports the codegen owners, reads
  `Args()`, and calls `RunCodegenFromArgs(args)`. AST path/file policy remains
  in `ast_input_owner.pgy`; C translation remains in `program_emit.pgy`.
- Contract ratchet: `tests/self_hosted_component_contract_smoke.sh` now requires
  `codegen_run_owner.pgy` as part of the codegen owner surface. Verified with
  `bash tests/self_hosted_component_contract_smoke.sh`,
  `make self-host-codegen-parity-test-smoke`,
  `make test-inc-size-test-smoke`, and
  `make self-host-preparation-test-smoke`.

### 2026-06-23 -- codegen struct value owner split

- Moved struct-valued expression lowering out of `stmt_emit.pgy` into
  `src/self_hosted/codegen/struct_value_emit.pgy`.
- `EmitLet`, `EmitAssign`, and `EmitReturn` still consume the same
  `EmitStructValue` boundary; literal/pass-through policy is now owned by the
  struct value owner.
- Contract ratchet: `tests/self_hosted_component_contract_smoke.sh` now requires
  `struct_value_emit.pgy`, requires `func EmitStructValue` there, and rejects
  that function in `stmt_emit.pgy`. Verified with
  `bash tests/self_hosted_component_contract_smoke.sh`,
  `make self-host-codegen-parity-test-smoke`,
  `make test-inc-size-test-smoke`, and
  `make self-host-preparation-test-smoke`.

### 2026-06-23 -- parser loop statement owner split

- Moved `while`/`loop`/`for` compact AST generation out of
  `src/self_hosted/parser/stmt_owner.pgy` into
  `src/self_hosted/parser/stmt_loop_owner.pgy`.
- `stmt_owner.pgy` now owns statement dispatch and shared block recursion;
  loop-statement syntax is consumed through the loop owner instead of a second
  in-file branch body.
- Contract ratchet: `tests/self_hosted_component_contract_smoke.sh` now requires
  `stmt_loop_owner.pgy`, requires `func ParseForStmt` there, and rejects that
  function in `stmt_owner.pgy`. Verified with
  `bash tests/self_hosted_component_contract_smoke.sh` and
  `make self-host-parser-parity-test-smoke`.

### 2026-06-23 -- parser postfix expression owner split

- Moved postfix expression-chain parsing out of
  `src/self_hosted/parser/expr_primary_owner.pgy` into
  `src/self_hosted/parser/expr_postfix_owner.pgy`.
- `expr_primary_owner.pgy` now owns primary expression roots only; postfix
  calls, indexes, member access, postfix try, object-init syntax, and
  call-only turbofish consumption are consumed through `ApplyPostfixExpr`.
- Contract ratchet: `tests/self_hosted_component_contract_smoke.sh` now requires
  `expr_postfix_owner.pgy`, requires `func ApplyPostfixExpr` there, and rejects
  `Postfix loop:` from `expr_primary_owner.pgy`. Verified with
  `bash tests/self_hosted_component_contract_smoke.sh` and
  `make self-host-parser-parity-test-smoke`,
  `make test-inc-size-test-smoke`, and
  `make self-host-preparation-test-smoke`.

### 2026-06-23 -- MIR lower routine inventory owner split

- Moved routine discovery and bounded routine header facts out of
  `src/self_hosted/mir_lower/routine_lower.pgy` into
  `src/self_hosted/mir_lower/routine_inventory_owner.pgy`.
- `routine_lower.pgy` now consumes the selected routine facts and owns CFG/body
  reconstruction only; `program_lower.pgy` and `decl_lower.pgy` consume the
  inventory owner instead of re-owning routine lookup.
- Contract ratchet: `tests/self_hosted_component_contract_smoke.sh` now requires
  `routine_inventory_owner.pgy`, requires `func FindRoutine` there, and rejects
  that owner function in `routine_lower.pgy`. Verified with
  `bash tests/self_hosted_component_contract_smoke.sh` and
  `make self-host-mir-json-parity-test-smoke`.

### 2026-06-23 -- Semantic expression validation owner split

- Moved `CheckUndefinedIdentifiers`, `CheckLogicalOperands`, and
  `CheckBinaryOperands` out of `src/self_hosted/semantic/expr_type_owner.pgy`
  into `src/self_hosted/semantic/expr_validation_owner.pgy`.
- `expr_type_owner.pgy` now owns expression type queries only; expression
  diagnostics consume `ExprType(...)` facts through the validation owner.
- Contract ratchet: `tests/self_hosted_component_contract_smoke.sh` now requires
  `expr_validation_owner.pgy`, requires `func CheckUndefinedIdentifiers` there,
  and rejects that owner function in `expr_type_owner.pgy`.

### 2026-06-25 -- Semantic diagnostic code owner

- Added `src/self_hosted/semantic/diagnostic_code_owner.pgy` as the source of
  truth for the self-hosted semantic checker's lower-case diagnostic codes.
- `diagnostic_owner.pgy` now consumes `SemanticDiagnosticCodeKnown(code)` before
  rendering an error. Unknown codes become the visible
  `unregistered_diagnostic_code` diagnostic instead of leaking as new ad hoc
  strings.
- `tests/self_hosted/parity/semantic_parity.sh` now checks expected fixture
  `Code:` fields and literal `SemanticError...("code")` call sites against the
  code owner before running C/LLVM verdict parity. This closes the self-hosted
  diagnostic-code vocabulary seam while honestly leaving a fully shared
  C/Pergyra diagnostic-code catalog for a later rung.
- Verified with `make self-host-semantic-parity-test-smoke`,
  `make self-host-component-contract-test-smoke`,
  `make self-host-preparation-contract-test-smoke`,
  `make documentation-quality-test-smoke`, and `git diff --check`.

### 2026-06-25 -- Semantic C-oracle diagnostic-code mapping

- Extended `diagnostic_code_owner.pgy` with `SemanticDiagnosticOracleCode`, the
  fixture-root mapping from self-hosted lower-case semantic codes to the current
  C oracle JSON diagnostic codes.
- Tightened `tests/self_hosted/parity/semantic_parity.sh`: invalid fixtures must
  now be rejected by the C oracle with the mapped JSON root code. A backend-native
  fallthrough with no semantic JSON code is a gate failure.
- Closed the scalar builtin signature gap in
  `src/semantic/type_checker_builtins_stdlib_scalar.c`: `Sqrt`/trig/log unary
  Float builtins, `Pow`, `Atan2`, `Random`, and `Clamp` now check their argument
  types in the semantic owner instead of letting native C compilation discover
  the mismatch later.

### 2026-06-25 -- Semantic real-source selfcheck expands to owner slices

- Expanded `tests/self_hosted/parity/selfcheck_sources.sh` from 4 to 41 real
  self-host owner/source files. The manifest now includes accepted lexer/parser/
  codegen/compiler-world slices plus audit-tool sources, not only the semantic
  and lexer entrypoints.
- Tightened `tests/self_hosted_component_contract_smoke.sh` to ratchet the
  selfcheck manifest at 41 files and require representative parser, codegen,
  compiler-world, and audit-tool sources.
- The gate still excludes broader parser/codegen entrypoints whose imported
  helper/local-binding/call surfaces are not covered by the current semantic
  subset; those remain implementation work, not manifest omissions.

### 2026-06-25 -- Semantic `Print` builtin fact and MIR-lower entrypoint selfcheck

- Added `Print` to the self-hosted semantic checker's builtin function fact
  inventory. This is not a `Log` alias: project docs and codegen already define
  `Print` as newline-free output, so the missing semantic fact made
  `src/self_hosted/mir_lower/main.pgy` fail with `undefined_function`.
- Added `valid_print_builtin` to semantic parity, raising the semantic fixture
  manifest to 86.
- Added `src/self_hosted/mir_lower/main.pgy` to the real-source selfcheck,
  raising that manifest to 42 accepted sources.

### 2026-06-25 -- Semantic source scanner skips comment braces

- Moved quoted-string, line-comment, and block-comment skipping into
  `text_scan_owner.pgy` and made statement-end, block-open, and matching-brace
  scans consume those facts consistently.
- Added `valid_comment_brace_scope`, a fixture with a `{` inside a line comment
  before a block-local binding. This prevents comment text from changing block
  scope or hiding a local binding such as codegen's `t` variable.
- The semantic parity manifest is now 87 fixtures.
- `src/self_hosted/codegen/main.pgy` now passes the real-source semantic
  selfcheck and is included in the manifest, raising that manifest to 43
  accepted sources.

### 2026-06-25 -- Compiler-world zone rule tightened

- Reaffirmed the self-host compiler-world rule that a zone is a resource
  ownership boundary, not a module/folder/phase label.
- `src/self_hosted/codegen/intent.md` now records the codegen split explicitly:
  `EmissionZone` owns emitted C, `TypeEnvZone` owns type facts, future
  symbol/name-mangling state can become a zone only if it owns mutable symbol
  facts, and `program_emit`/`function_emit`/`stmt_emit`/`expr_rewrite`/
  `struct_value_emit` remain participants over those resources.
- `tests/self_host_compiler_world_contract_smoke.sh` now ratchets that wording so
  codegen files cannot drift into fake zone wrappers merely because they are
  separate files.

### 2026-06-25 -- Parser entrypoint enters semantic real-source selfcheck

- Added `FindMatchingBraceWithin` to `text_scan_owner.pgy` and repointed scoped
  `if`/`while`/`for` body checks to consume the caller-owned body boundary
  instead of reopening an unbounded brace scan.
- Verified `src/self_hosted/parser/main.pgy` through the real import-aware
  semantic checker. It now produces `Status: ok` and is included in
  `tests/self_hosted/parity/selfcheck_sources.sh`.
- Ratcheted the real-source semantic selfcheck manifest from 43 to 44 accepted
  self-host owner/source files. The parser entrypoint still takes about 21s on
  the local Windows checker binary, so performance work remains; the semantic
  result is no longer a blocker.

### 2026-06-25 -- Seeded RNG builtin enters semantic parity

- Added `SeedRandom(Int) -> Void` to the self-hosted semantic builtin signature
  inventory. `SeedRandom` already exists in the native builtin/type table and
  C/LLVM runtime paths; the self-hosted checker was the missing consumer fact.
- Added `valid_seedrandom_builtin` and `bad_seedrandom_arg` semantic fixtures,
  proving a seeded RNG statement call is accepted and a non-Int seed reports
  `call_arg_type_mismatch` through the C-oracle-backed diagnostic mapping.
- `src/self_hosted/fuzz/backend_parity_generator/main.pgy` reached the next
  semantic frontier at this point: the `SeedRandom` fact was present, but
  `let mut`, generated-source string literals, and `WriteFile` still needed
  checker coverage before the file could enter the real-source manifest.

### 2026-06-25 -- Backend fuzz generator enters semantic real-source selfcheck

- Added `let mut` local declaration support to the self-hosted semantic body
  owner. The previous parser treated `mut` as the binding name, then recovered
  from the missing `=` by jumping to a statement end from the body start; that
  could move the cursor backwards on real sources. The recovery path now skips
  from the current declaration cursor, and `let mut name: Type = expr` consumes
  the same local fact as `let name: Type = expr`.
- Made the program-level function inventory skip quoted strings and jump over a
  function body after its signature is captured. Generated source snippets such
  as `"func Fake() -> Void { ... }"` are now string data, not declarations to
  rediscover.
- Added `WriteFile(String, String) -> Void` to the self-hosted semantic builtin
  inventory. The native builtin/type table and C/LLVM codegen already owned
  that IO surface; the self-hosted checker was the missing consumer fact.
- Added semantic parity fixtures for `let mut`, generated-source string
  literals, and `WriteFile`, raising semantic parity to 93 fixtures.
- Added `src/self_hosted/fuzz/backend_parity_generator/main.pgy` to the
  real-source semantic selfcheck manifest, raising it to 45 accepted
  self-host owner/source files. Local C-backend checker measurement after the
  fix accepted that source in about 135 ms.

### 2026-06-25 -- Fuzz generator parity enters self-host preparation

- Wired `tests/self_hosted/parity/fuzz_backend_parity_generator_parity.sh` into
  `self-host-preparation-parity-test-smoke`. The generator was already a named
  parity target; this closes the docs/CI drift where the parity README said the
  preparation gate ran the full parity set while the fuzz generator leg stayed
  focused-only.
- Tightened the self-host preparation and hard-contract smokes so the fuzz
  generator parity harness remains linked from the preparation path.

### 2026-06-25 -- Codegen owner folders follow resource zones

- Moved the self-hosted codegen owner files out of one flat folder into
  resource-shaped subdirectories: `input/`, `run/`, `text/`, `type_facts/`, and
  `emission/`.
- Kept `program_emit`, `function_emit`, `stmt_emit`, `expr_rewrite`, and
  `struct_value_emit` as emission action participants rather than pretending
  each is a zone. The filesystem split now matches the rule in
  `src/self_hosted/codegen/intent.md`: folders expose owner boundaries, not
  arbitrary call-graph nodes.
- Updated the component contract to check owner sources recursively while
  excluding `fixture/` and `expected/`, so nested codegen owners remain under
  the 600-line cap and must stay listed in `src/self_hosted/OWNERS.md`.

### 2026-06-25 -- Self-host path facts get a shared owner

- Added `src/self_hosted/lib/path.pgy` as `SelfHostPath`, the shared owner for
  self-hosted path string facts: dirname, absolute-path detection, joining, and
  `./` / `../` import-relative normalization.
- Repointed parser import handling and semantic source-bundle import expansion
  to consume `SelfHostPath` directly instead of keeping local dirname/join
  aliases in each stage.
- Updated parser parity build mirrors so `../lib/path.pgy` resolves under the
  copied `.tmp/self_hosted/parser*` source roots. The real-source semantic
  selfcheck manifest now includes `lib/path.pgy`, raising the accepted source
  count to 46.

### 2026-06-25 -- Lexer scan owner declares its real imports

- Moved lexer character/token dependencies behind `scan_owner.pgy`: the scan
  loop now imports `char_owner.pgy` and `token_owner.pgy` directly, while
  `main.pgy` stays an entrypoint that imports only the scan owner and source
  input owner.
- Added `src/self_hosted/lexer/scan_owner.pgy` to the real-source semantic
  selfcheck manifest, raising accepted self-host owner/source files to 48.
- Ratcheted component/preparation contracts so `main.pgy` cannot re-import the
  scan-loop internals and duplicate MIR declaration headers.

### 2026-06-25 -- Semantic source bundle declares path and scan facts

- Moved semantic import-expansion dependencies behind
  `source_bundle_owner.pgy`: the bundle owner now imports `../lib/path.pgy` and
  `text_scan_owner.pgy` directly because it consumes path normalization,
  comment/whitespace skipping, keyword matching, and character facts.
- Kept `semantic/main.pgy` as an entrypoint that imports the source-bundle owner
  instead of re-importing path/text-scan internals.
- Added `src/self_hosted/semantic/source_bundle_owner.pgy` to the real-source
  semantic selfcheck manifest, raising accepted self-host owner/source files to
  49.

### 2026-06-25 -- Semantic diagnostic owner declares renderer and code facts

- Moved semantic diagnostic rendering dependencies behind
  `diagnostic_owner.pgy`: it now imports the shared
  `src/self_hosted/lib/diagnostic.pgy` renderer and
  `diagnostic_code_owner.pgy` vocabulary directly.
- Kept `semantic/main.pgy` from importing diagnostic renderer/code internals;
  it now consumes the diagnostic owner as the boundary.
- Ratcheted component/preparation contracts so the entrypoint cannot re-open
  those internal imports and create another duplicate declaration path.

### 2026-06-25 -- Self-host compiler substrate architecture documented

- Added `docs/self_hosted/13_compiler_substrate_architecture.md` as the
  concrete architecture contract below the compiler-world and intent/zone
  documents. The document records the required substrates for hard
  self-hosting: path manifests, import graph ownership, deterministic
  collections, diagnostics, type facts, MIR facts, ABI/layout facts, emission
  buffers, runtime materialization policy, caching, and parity evidence.
- Linked the document from `docs/INDEX.md`, `docs/self_hosted/README.md`,
  `src/self_hosted/compiler/README.md`, and `src/self_hosted/codegen/README.md`.
- Tightened the compiler-world and preparation contract smokes so the substrate
  document stays load-bearing instead of becoming a standalone note.

### 2026-06-25 -- Parser import graph de-duplicates source materialization

- Closed the parser import graph SoT seam for duplicate source materialization.
  The native `import_resolver` now tracks every imported canonical source path
  in the `loaded` stack, not only stdlib modules, so importing the same file
  through two paths materializes its declarations once.
- Mirrored the same fact in the self-hosted parser. `source_path_owner.pgy`
  owns `ParserImportGraphSeen`, `program_parse_owner.pgy` initializes the root
  import path set, and `decl_dispatch_owner.pgy` consumes that set before
  recursively parsing an import.
- Added `import_dedup_graph.pgy` to parser parity. The fixture imports the same
  leaf directly and through a midpoint file, and the oracle AST contains the
  leaf function once. This unblocks direct owner-import growth without relying
  on entrypoint-order workarounds.

### 2026-06-25 -- Semantic entrypoint stops aggregating owner imports

- Repointed `src/self_hosted/semantic/main.pgy` to import only
  `semantic_run_owner.pgy`. The entrypoint is now a process boundary again,
  not the hidden owner of semantic dependency order.
- Moved semantic owner dependencies to the owners that consume those facts:
  the run owner imports source-bundle, diagnostic, and program-check owners;
  the program/body/call/expression owners import text-scan, environment,
  expression-type, expression-validation, and call-check facts directly.
- Ratcheted the component and preparation contracts so `semantic/main.pgy`
  cannot re-import source-bundle, diagnostic, environment, expression,
  body/call/program-check, or shared diagnostic/path internals. This keeps the
  import graph SoT in owner declarations rather than entrypoint aliases.
- Cached semantic parity compiler path classification once per script run.
  The C oracle loop previously re-read the compiler binary magic for every
  fixture path conversion on Windows; the parity contract is unchanged, but the
  hot path no longer pays that repeated probe.

### 2026-06-25 -- Compiler world stages stop sharing one generic actor

- Removed the generic `StageOwner.Consume()` shape from
  `src/self_hosted/compiler/world.pgy`. The compiler world now names
  `LexerStage`, `ParserStage`, `SemanticStage`, and `MirLowerStage` as
  separate subjects with stage-specific actions.
- Repointed `FrontendPipeline` and `MiddleEndPipeline` in
  `stage_intents.pgy` so lexing, parsing, semantic checking, and MIR lowering
  consume the actor that owns the artifact being produced, instead of a shared
  stage alias.
- Tightened `self-host-compiler-world-contract-test-smoke` to reject
  reintroducing `subject StageOwner` or `.Consume()` in the compiler-world
  source and to require the stage-specific subjects in the parsed AST.
- Updated the compiler-world, intent/zone, and substrate architecture docs so
  the self-host shape is explicitly intent/zone-driven rather than a
  C-style driver with renamed helper participants.

### 2026-06-25 -- Compiler path manifest gets a Pergyra owner

- Added `src/self_hosted/compiler/path_manifest_owner.pgy` as the Pergyra
  owner for self-host source/test/parity path values consumed by
  `StagePathManifest`.
- Imported that owner from `world.pgy` and added it to the compiler-world shell
  projection in `tests/self_hosted/compiler_world_manifest.sh`.
- Tightened `self-host-compiler-world-contract-test-smoke` so every shell
  manifest path must appear in the Pergyra owner and every path returned by the
  Pergyra owner must exist in the shell projection.
- Added the path manifest owner to the real-source semantic selfcheck manifest,
  raising accepted self-host owner/source files from 49 to 50 on both C and
  LLVM checker backends.

### 2026-06-25 -- Semantic owner files enter real-source selfcheck

- Added the already accepted semantic owner sources
  `body_check_owner.pgy`, `call_check_owner.pgy`, `expr_type_owner.pgy`,
  `expr_validation_owner.pgy`, `program_check_owner.pgy`, and
  `semantic_run_owner.pgy` to `selfcheck_sources.sh`.
- Raised the real-source semantic selfcheck manifest from 50 to 56 accepted
  self-host owner/source files. This does not add a fallback; it makes the
  current semantic checker prove its own split owner files through the same
  C/LLVM-compiled checker used for other real sources.

### 2026-06-25 -- Self-host compiler/codegen architecture stack recorded

- Expanded `docs/self_hosted/13_compiler_substrate_architecture.md` from a
  substrate checklist into the concrete self-hosted architecture stack for
  `PgyCompilerWorld`, stage fact owners, shared substrates, and the codegen
  backend resource cluster.
- Recorded the current-to-target migration map: stage entrypoints stop acting
  as dependency aggregators, path discovery moves behind `StagePathManifest`,
  diagnostics move behind shared owners, and codegen migrates from AST-text
  bridge reads toward MIR/type/ABI facts.
- Made the codegen resource contract explicit: `EmissionZone` owns emitted
  output, `TypeEnvZone` owns type facts, symbol/mangle and ABI/layout owners
  must become the single source for backend emission, and fake stmt/expr zones
  remain forbidden while they mutate the same output resource.
- Updated the self-host docs index and compiler README wording so future work
  treats `13_compiler_substrate_architecture.md` as the codegen/compiler/
  substrate architecture contract, not only a background note.

### 2026-06-25 -- Acyclic parser owners enter real-source selfcheck

- Moved the acyclic parser type/declaration owners behind direct fact-owner
  imports instead of relying only on `parser/main.pgy` import order:
  `type_name_owner`, `decl_type_owner`, `decl_event_owner`, `decl_enum_owner`,
  and `decl_effect_relation_owner`.
- Added those five parser owner files to the real-source semantic selfcheck
  manifest, raising accepted self-host owner/source files from 56 to 61.
- Left the expression/statement mutual-recursion owners out of this slice
  deliberately: the native import resolver still rejects circular imports, so
  that group needs an explicit cycle/import-owner design before it can stop
  relying on parser entrypoint materialization.

### 2026-06-25 -- Expression parser gains a cycle-safe owner boundary

- Added `src/self_hosted/parser/expr_owner.pgy` as the public owner boundary
  for the mutually recursive expression grammar. It imports the string,
  postfix, primary, and precedence participants as one cluster instead of
  asking those files to circularly import each other.
- Repointed `parser/main.pgy` to import `expr_owner.pgy` rather than the four
  expression participant files directly, and ratcheted the component/prep
  smokes so the old entrypoint aggregation cannot come back.
- Added the expression owner boundary to real-source semantic selfcheck,
  raising accepted self-host owner/source files from 61 to 62.

### 2026-06-25 -- Statement parser branch imports move behind stmt owner

- Repointed `parser/main.pgy` so statement branch files are no longer imported
  by the entrypoint. `stmt_owner.pgy` is now the public statement grammar
  boundary and imports the if/loop/parallel/match branch participants as one
  cluster.
- Added ratchets that reject `stmt_if_owner`, `stmt_loop_owner`,
  `stmt_parallel_owner`, and `stmt_match_owner` imports from `parser/main.pgy`
  while requiring them from `stmt_owner.pgy`.
- Added `stmt_owner.pgy` to real-source semantic selfcheck, raising accepted
  self-host owner/source files from 62 to 63.

### 2026-06-25 -- Parser declaration layer stops using main as import owner

- Moved parser declaration/function/program dependencies behind direct owner
  imports: `program_parse_owner` imports `decl_dispatch_owner`,
  `decl_dispatch_owner` imports the top-level declaration branch owners, and
  `function_decl_owner` imports cursor/tree/type/expression/statement facts.
- Repointed `parser/main.pgy` to import only `source_path_owner.pgy` and
  `program_parse_owner.pgy`. The entrypoint no longer owns parser cursor,
  expression, statement, function, declaration, or path-library import order.
- Added the parser declaration layer owners to real-source semantic selfcheck,
  raising accepted self-host owner/source files from 63 to 71.

### 2026-06-26 -- Pre-self-host expansion ledger becomes contract

- Added `docs/self_hosted/15_pre_self_host_expansion_ledger.md` as the
  load-bearing ledger for surfaces that must exist before broader hard
  self-hosting. The ledger classifies each surface as `READY`, `ACTIVE`, or
  `HOLD` so hard rungs cannot smuggle missing prerequisites back in as
  fallbacks.
- Recorded the active pre-hard blockers: mixed AST-like tree ownership, stable
  JSON parse/emit, subprocess execution, symbol/mangle ownership, ABI/layout
  row projection, AIR evidence zone, Artifact Zone evidence, and Pergyra-owned
  test harness records.
- Wired the ledger into the compiler-world contract smoke, the self-host docs
  index, and the top-level docs index. Refreshed the self-hosted doc-link
  checker golden count for the new index link.
- Verified with `make self-host-preparation-contract-test-smoke`,
  `tests/self_hosted/parity/doc_link_checker_parity.sh`,
  `make documentation-quality-test-smoke`, and `git diff --check`.

### 2026-06-26 -- Shared JSON read owner enters self-host substrate

- Added `src/self_hosted/lib/json.pgy` as the shared bounded JSON read
  primitive owner. It owns string, number, array span, object span, and first
  array-string reads for fact-shaped self-host tools.
- Narrowed `src/self_hosted/mir_lower/json_fact_read.pgy` to MIR-specific
  source-local fact lookup. Generic JSON scanning now comes from the shared
  owner instead of living inside the MIR-lower consumer.
- Added `src/self_hosted/lib/json.pgy` to the owner manifest and real-source
  semantic selfcheck, raising accepted self-host owner/source files from 71 to
  72.
- Tightened `self_hosted_component_contract_smoke` so generic JSON read
  functions cannot move back into `mir_lower/json_fact_read.pgy`.

### 2026-06-26 -- Self-host codegen ABI type spelling moves behind ABI layout owner

- Added `src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy` as the
  self-host C subset owner for ABI type spelling. Function parameters, returns,
  struct/class fields, local declarations, `try` temporaries, range-loop
  indices, and for-each collection temporaries now consume that owner instead
  of spelling C value types inside emission participants.
- Tightened `self_hosted_component_contract_smoke` so `function_emit.pgy` cannot
  reintroduce local `CParamType` / `CRetType` owners and `stmt_emit.pgy` cannot
  reintroduce local declaration spellings such as direct `long long` /
  `const char*` declaration strings.
- Added the ABI layout owner to real-source semantic selfcheck, raising accepted
  self-host owner/source files to 75. The broader cross-backend ABI/layout row
  projection remains ACTIVE; this slice only closes self-host C type spelling in
  the current supported subset.

### 2026-06-26 -- Self-host collection runtime helper spelling moves behind runtime ABI owner

- Added `src/self_hosted/codegen/runtime_abi/collection_runtime_owner.pgy` as
  the self-host C subset owner for `Array<Int>` / `Array<String>` runtime helper
  symbol spelling. `expr_scan`, `expr_rewrite`, and `stmt_emit` now consume that
  owner instead of locally spelling `pgy_ai_*` / `pgy_as_*` helper names.
- The owner also normalizes the current AST-text bridge spellings
  `Array<Int: Int>` / `Array<String: String>` into canonical `ArrayInt` /
  `ArrayString` facts. This is kept at the owner boundary so emitter
  participants do not each grow their own compatibility spelling checks.
- Kept `program_emit.pgy` as the generated helper definition host. This closes
  call-site spelling drift only; it does not claim the broader cross-backend
  runtime materialization or ABI row projection is complete.
- Added the runtime ABI owner to real-source semantic selfcheck, raising
  accepted self-host owner/source files to 76 on both C and LLVM.

### 2026-06-26 -- Self-host string runtime helper spelling moves behind runtime ABI owner

- Added `src/self_hosted/codegen/runtime_abi/string_runtime_owner.pgy` as the
  self-host C subset owner for supported string/text runtime helper symbol
  spelling. `expr_rewrite` and `stmt_emit` now consume that owner for `Concat`,
  string length/search/trim/replace/case/join/subspan helpers, `ToString`,
  `ToInt`, `Print`, and string `Log` helper names.
- Kept `program_emit.pgy` as the generated helper definition host. This closes
  string/text helper call-site spelling drift only; file, math, Result/Option,
  and broader cross-backend runtime materialization facts remain separate
  surfaces.
- Added the string runtime owner to real-source semantic selfcheck, raising
  accepted self-host owner/source files to 77 on both C and LLVM.

### 2026-06-26 -- Self-host Option/Result runtime helper spelling moves behind runtime ABI owner

- Added `src/self_hosted/codegen/runtime_abi/option_result_runtime_owner.pgy`
  as the self-host C subset owner for supported `Option<Int>` / `Result<Int>`
  runtime helper symbol spelling. `expr_rewrite` now consumes that owner for
  `Some`, `None`, `IsSome`, `UnwrapOption`, `Ok`, `Err`, `IsOk`, `IsErr`,
  `Unwrap`, and `UnwrapOr` helper names. `stmt_emit` consumes the same owner
  for `?` try-lowering checks and unwraps.
- Kept `program_emit.pgy` as the generated helper definition host. This closes
  Option/Result helper call-site spelling drift only; math and file/argv
  helper spelling remain direct self-host codegen surfaces.
- Added the Option/Result runtime owner to real-source semantic selfcheck,
  raising accepted self-host owner/source files to 78 on both C and LLVM.

### 2026-06-26 -- Self-host math and host I/O runtime helper spelling moves behind runtime ABI owners

- Added `src/self_hosted/codegen/runtime_abi/math_runtime_owner.pgy` as the
  self-host C subset owner for supported math/random runtime helper symbol
  spelling (`Abs`, `Min`, `Max`, `SeedRandom`, `Random`).
- Added `src/self_hosted/codegen/runtime_abi/host_io_runtime_owner.pgy` as the
  self-host C subset owner for supported host file/argv runtime helper symbol
  spelling (`FileExists`, `WriteFile`, `FileOpen`, `FileWrite`, `FileClose`,
  `FileRead`, `ReadFile`, `DirWalk`, `Args`).
- `expr_rewrite` now consumes runtime ABI owners for all supported Pergyra
  `pgy_*` runtime helper call-site spellings. The remaining direct target
  spellings in that path are C standard-library calls, not Pergyra runtime
  helper facts.
- Added the math and host I/O runtime owners to real-source semantic selfcheck,
  raising accepted self-host owner/source files to 80 on both C and LLVM.

### 2026-06-26 -- Self-host AST text line inventory moves behind input owner

- Added `src/self_hosted/codegen/input/ast_text_inventory_owner.pgy` as the
  transitional owner for raw `pgy --ast` text line inventory. It owns line
  splitting, leading indentation, blank-line filtering, and `[export]`
  normalization before `program_emit` consumes the inventory.
- Removed the unused `IndentOf` owner from `stmt_emit`; indentation is now an
  AST-text inventory fact, not a statement-emission fact.
- Tightened `self_hosted_component_contract_smoke` so `program_emit.pgy` cannot
  reintroduce raw `NextNewline(ast, pos)` / `StringTrim(raw_line)` inventory
  recovery or call a local `IndentOf(raw_line)` path.
- This does not close the mixed AST-like tree owner. It only closes the
  raw-line inventory seam while the bounded codegen rung still consumes
  compiler-emitted AST text.
- Added the AST text inventory owner to real-source semantic selfcheck, raising
  accepted self-host owner/source files to 81 on both C and LLVM.

### 2026-06-26 -- Self-host AST text cursor expectations move behind input owner

- Moved AST cursor expectation checks out of `stmt_emit.pgy` and into
  `src/self_hosted/codegen/input/ast_text_inventory_owner.pgy` as
  `CodegenAstTextExpect`.
- Repointed function and statement emitters to consume that input owner. The
  component contract now rejects a local `func ExpectText` in `stmt_emit.pgy`.
- Probed typed AST-line records as the next target:
  `Array<AstTextLine>` compiles on the C backend, but LLVM fail-closes on
  `ArrayPush` with missing concrete `Array<T>` element/runtime metadata for a
  nominal record element. The typed/tagged AST inventory remains blocked on a
  record-array C/LLVM parity owner rather than being papered over in codegen.

### 2026-06-26 -- Basic nominal-record arrays become C/LLVM parity substrate

- Added the `record_array_basic` backend-compare fixture to cover
  `Array<NominalRecord>` creation, parameter passing, `ArrayPush`, `ArraySet`,
  `ArrayPop`, indexing, and indexed member access.
- Extended the LLVM array registry to retain the canonical element type name
  next to the element `LLVMTypeRef`. Indexed member access now consumes that
  registry fact for receivers such as `rows[0].id` instead of trying to recover
  a nominal element type from the source AST.
- Added raw byte-array runtime exports for nominal record arrays and wired LLVM
  collection lowering to use them for the supported mutation surface.
- Verified the narrow fixture with the freshly built compiler:
  `PGY_BIN=E:/PergyraLang/bin/pgy.exe tests/compare_backends.sh tests/cases/backend_compare/record_array_basic`.
- Scope is deliberately narrow. This opens typed-record array inventory for the
  next self-host slice; it does not claim nominal-record support for map,
  filter, sort, slice, or arbitrary collection algorithms.

### 2026-06-26 -- Self-host AST text bridge gets typed node inventory

- Added `CodegenAstTextNode` and `CodegenAstTextNodeInventory` to
  `src/self_hosted/codegen/input/ast_text_inventory_owner.pgy`. The owner now
  stores each bridge line as a typed record with `indent` and `text` rather
  than treating the parallel `Array<Int>` / `Array<String>` pair as the first
  owned form.
- Repointed `program_emit.pgy` to consume typed nodes first, then project the
  legacy `indents` / `texts` arrays for current function and statement
  emitters. This is the first migration step enabled by nominal-record array
  C/LLVM parity.
- Tightened `self_hosted_component_contract_smoke` so `program_emit.pgy`
  cannot return to direct `CodegenAstTextInventory(ast, indents, texts)`
  consumption.
- This does not close the mixed AST-like tree owner. It removes the immediate
  record-array blocker and turns the raw line bridge into a typed owner surface;
  full closure still requires function/statement emitters to consume typed or
  tagged AST data instead of projected text lines.

### 2026-06-26 -- Program emit routes top-level declarations through typed nodes

- Repointed `src/self_hosted/codegen/emission/program_emit.pgy` so
  program-level declaration routing consumes `CodegenAstTextNode` directly:
  `Main` counting, event rejection, first-function indentation, zero-artifact
  skipping, nominal owner dispatch, role owner dispatch, and top-level function
  dispatch no longer index the projected `texts` / `indents` arrays.
- Tightened `self_hosted_component_contract_smoke` so `program_emit.pgy` cannot
  reintroduce direct `texts[...]` or `indents[...]` reads.
- At that point, the legacy projection still remained for collector, function,
  and statement emitters. This narrowed the bridge boundary but did not claim
  full tagged AST ownership.

### 2026-06-26 -- Declaration collector prepasses consume typed AST nodes

- Repointed the program-scope prepasses in
  `src/self_hosted/codegen/emission/function_emit.pgy` to consume
  `Array<CodegenAstTextNode>` directly: `BuildFunctionEnv`,
  `CollectRoleOperators`, `CollectStructs`, `CollectEnums`, and
  `CollectProtos`.
- Updated `GenerateC` to pass the typed node inventory into those prepasses
  instead of the projected `indents` / `texts` arrays.
- Tightened `self_hosted_component_contract_smoke` so those prepass signatures
  cannot regress to `indents` / `texts` inputs.
- The legacy projection remains only for function body and statement emission.
  This continues the text-bridge burn-down without claiming full tagged AST
  ownership.

### 2026-06-26 -- Function emission consumes typed AST text nodes

- Moved `EmitFunction` header, parameter, return, `Body:`, and `Block:` reads
  to consume `CodegenAstTextNode` inventories. The input owner now exposes a
  node-based cursor expectation check so function signature emission no longer
  indexes projected `texts[]` or `indents[]`.
- `program_emit.pgy` still projects legacy `indents` / `texts` arrays because
  `stmt_emit.pgy` remains the next unmigrated statement-body consumer. That
  projection is now pass-through compatibility for statement emission only, not
  a function-signature source of truth.
- Tightened the component contract to require the typed `EmitFunction`
  signature and reject direct `texts[]` / `indents[]` indexing inside
  `function_emit.pgy`.
- Recorded the Pergyra-style self-host criterion: a `.pgy` compiler slice is
  not enough by itself. It must preserve `PgyCompilerWorld`, intent-owned flow,
  resource-owned zones, single fact owners, peer backend projections, and
  parity evidence instead of becoming a C folder graph translated into Pergyra.

### 2026-06-26 -- Statement emission consumes typed AST text nodes

- Repointed `EmitStmtList` to consume `Array<CodegenAstTextNode>` directly.
  Statement-body emission now reads `nodes[idx].text` and `nodes[idx].indent`
  from the AST-text inventory owner instead of projected `texts[]` /
  `indents[]` arrays.
- Removed the legacy `CodegenAstTextProjectLegacy`,
  `CodegenAstTextInventory(ast, indents, texts)`, and
  `CodegenAstTextExpect(texts, ...)` bridge APIs from
  `input/ast_text_inventory_owner.pgy`.
- Tightened `self_hosted_component_contract_smoke` so `program_emit`,
  `function_emit`, and `stmt_emit` cannot reintroduce the parallel text/indent
  projection.
- The mixed AST-like tree blocker remains active because
  `CodegenAstTextNode.text` is still serialized AST text. The closed seam is
  the duplicated line-inventory owner, not the final tagged AST owner.

### 2026-06-26 -- JSON string emission gets a shared owner

- Promoted JSON string escaping and literal emission into
  `src/self_hosted/lib/json.pgy` via `JsonEscapeString` and
  `JsonStringLiteral`.
- Repointed the diagnostic catalog checker and AIR graph JSON validator report
  owners to consume that JSON owner for dynamic string fields instead of
  hand-splicing unescaped values into schema JSON.
- Tightened `self_hosted_component_contract_smoke` so the shared JSON emit
  primitives and representative report-owner imports cannot disappear.
- The Stable JSON parse/emit blocker remains active: schema object shape,
  object/array iteration, and a structured JSON writer are still owned by
  individual report owners.

### 2026-06-26 -- JSON object emission gets first shared consumers

- Promoted JSON field, object, and array emission into
  `src/self_hosted/lib/json.pgy` via `JsonEmitField*`, `JsonEmitObject`, and
  `JsonEmitArray`.
- Repointed `production_c_size_checker` and
  `production_header_size_checker` to build report objects and findings
  through that JSON owner instead of local `json_parts` arrays.
- Tightened `self_hosted_component_contract_smoke` so both production size
  checkers must import the JSON owner and cannot return to local report object
  string assembly.
- The Stable JSON parse/emit blocker remains active: schema-specific report
  object decisions, object/array iteration, and remaining report emitters still
  need to converge on the same owner.

### 2026-06-26 -- Parameter-mode facts and typed-node arrays close the codegen bootstrap gap

- Found a real self-host SoT bug: `pgy --ast` dropped parameter mode, so an
  `inout Array<CodegenAstTextNode>` parameter became a value parameter in the
  self-host C emitter. The generated tool copied mutations into a local array
  value, then crashed when later code read the caller's still-empty node array.
- Fixed the native AST printer and the self-host parser to preserve `inout`,
  `own`, and `ref` parameter rows. The self-host codegen now records
  per-function `pm` mode facts, lowers `inout` signatures as C pointer
  parameters with copy-in/copy-out, and rewrites call arguments to `&name` from
  that fact. `own` and `ref` are preserved but fail closed in this bounded C
  emitter until their ABI/ownership semantics have owners.
- Added the bootstrap-only `Array<CodegenAstTextNode>` record-array lane behind
  `collection_runtime_owner.pgy` and `abi_layout_owner.pgy`. Statement and
  program emission consume that lane through collection and ABI owners rather
  than spelling record-array helpers locally.
- Tightened `self_hosted_component_contract_smoke` so the old paths cannot
  return: native/self-host AST printers must preserve parameter modes, codegen
  must consume `pm` facts, inout calls must use the mode-aware rewrite, and the
  `CodegenAstTextNode` array helper names must remain behind their owners.
- Verified parser parity on 186 sources for both C and LLVM parser binaries,
  then verified the codegen bootstrap gate: `gen2 == gen3` and the
  codegen-built lexer, parser, semantic checker, mir_lower, audit tools, and
  backend fuzz generator all match their oracle-built counterparts.

### 2026-06-26 -- MIR-backed C intent fallback closes harder

- Renamed the LLVM type-alias target renderer to
  `llvm_render_alias_target_type_name_from_headers` so the helper name matches
  the declaration-header owner instead of looking like an arbitrary alias
  scratch path.
- C intent prologue emission now permits AST compatibility only for non-MIR
  intents with no binding rows. If MIR binding metadata exists but the routine
  is absent, the C backend fails closed instead of silently reopening AST
  priority/binding/value reads.
- C intent forward declaration emission now fails closed when a MIR-backed
  value binding lacks type metadata, matching the existing ordered
  `IntentBindingMetadataView` completeness checks.
- MIR callable signature metadata now stores nested return/parameter type-name
  facts through `mir_capture_type_name`, matching the same capture owner used
  by routine signatures and source-local facts.
- Tightened `mir_declaration_inventory_smoke` to reject the retired LLVM alias
  helper name and require the new C intent fail-closed diagnostics.

### 2026-06-26 -- Hard self-host expansion owners enter PgyCompilerWorld

- Added compiler-world owner files for AIR evidence, Artifact Zone evidence,
  TestHarness rows, Subprocess runner capability envelopes, cross-backend
  ABI/layout rows, and cross-backend symbol rows.
- Wired those owners into `PgyCompilerWorld` through `AirEvidenceZone`,
  `SymbolFactTableZone`, `AbiRowProjectionZone`, `ArtifactZone`,
  `TestHarnessZone`, and `SubprocessRunnerZone`.
- Updated the path manifest and shell projection so the new owners are a single
  manifest fact rather than parallel file lists.
- Kept the pre-self-host expansion ledger honest: these surfaces now have
  owner envelopes, but remain ACTIVE until live C/LLVM/self-hosted consumers
  consume the rows instead of shell/text/backend-local fallbacks.

### 2026-06-28 -- JSON owner gains top-level row bounds

- Added top-level object value bounds and array-object row iteration to
  `src/self_hosted/lib/json.pgy`, keeping JSON schema decisions in consumers
  while removing recursive key text search from row-level manifest checks.
- Repointed `module_manifest_resolver` required-field validation to consume
  `JsonArrayObjectBoundsAt` plus top-level `JsonObjectHasField` for each
  module row.
- Repointed the AIR graph JSON validator's summary count reads to consume the
  top-level `summary` object bounds before reading `intent_count`,
  `boundary_count`, `evidence_count`, and `drift_count`; the old behavior was
  an accidental recursive document-number search.
- Tightened the module manifest parity gate with a nested-field negative
  fixture: a nested `"layer"` key no longer satisfies the module row's
  top-level `layer` requirement.
- The Stable JSON parse/emit blocker remains active because this is still a
  bounded schema scanner, not a complete JSON DOM/fact table.

### 2026-06-28 -- Backend comparator consumes artifact and harness owners

- Repointed `backend_output_comparator` to import
  `artifact_zone_owner.pgy`, `test_harness_owner.pgy`, and
  `subprocess_runner_owner.pgy` alongside the JSON owner.
- The comparator report now records `artifact_kind:"run_output"` from
  a positional artifact-kind accessor and C/LLVM projection rows from
  `CompilerHarnessProjectionAt(0/1)`, plus the `oracle_compare`
  stdout/stderr and exit-code facts from `CompilerSubprocess*`, instead of
  carrying those facts as local shell/test vocabulary.
- Tightened the comparator and tri-compare parity harnesses to copy the
  compiler-world artifact/test-harness/subprocess owners into the build roots,
  so those imports are live for both direct comparator parity and C/LLVM
  tri-compare.
- Artifact Zone evidence, TestHarness substrate, and Subprocess runner remain
  active until every parity artifact and runner invocation is written through
  these rows, but the first run-output parity sink now consumes the
  compiler-world owners.

### 2026-06-28 -- Self-host C ABI spelling consumes compiler-world ABI envelope

- Repointed `src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy` to import
  `compiler/abi_layout_row_owner.pgy`.
- The self-host C ABI type spelling owner now calls
  `CompilerAbiLayoutRowsReady()` before emitting any C value type spelling, so
  supported parameter, return, local, and field spellings fail closed if the
  compiler-world ABI/layout row envelope drifts.
- This is still not full cross-backend ABI row projection: concrete native
  C/LLVM/self-hosted row consumption for field order, tag/niche, size/align,
  ownership shape, and materialization policy remains ACTIVE.

### 2026-06-28 -- AST-text bridge records parent and kind rows

- Extended `CodegenAstTextNode` from an `(indent, text)` pair to an
  `(indent, text, parent, kind)` row.
- `CodegenAstTextNodeInventory` now records a parent edge for each non-empty
  AST-text line and a coarse node kind for common compiler routing labels such
  as `Function:`, `Parameters:`, `Returns:`, `Fields:`, `Field:`, role,
  nominal, and enum declarations.
- This reduces the mixed AST-like tree blocker but does not close it:
  `CodegenAstTextNode.text` is still a serialized line payload, so complete
  closure still requires owned tagged AST data instead of line-text semantics.

### 2026-06-28 -- Symbol spelling requires compiler-world row envelope

- Added `CompilerSymbolRequireTable()` to
  `src/self_hosted/compiler/symbol_table_owner.pgy`.
- `CompilerSymbolCIdentifier()` now fail-closes before projecting a C spelling
  if the compiler-world symbol row envelope is not ready.
- This still does not close cross-backend symbol/mangle SoT: native C, LLVM,
  and self-hosted projections still need to consume the same concrete symbol
  row table instead of sharing only the vocabulary envelope.

### 2026-06-28 -- Program routing consumes AST bridge kind facts

- Added `CodegenAstTextIs*` predicates to
  `src/self_hosted/codegen/input/ast_text_inventory_owner.pgy` for function,
  main function, role, nominal, enum, event, and zero-artifact declaration rows.
- Repointed `program_emit.pgy` top-level declaration routing to consume those
  owner predicates instead of testing declaration category directly with local
  `StartsWith` checks.
- Name extraction still consumes `CodegenAstTextNode.text`, so this is a
  reduction of the mixed AST-like tree blocker, not full closure.

### 2026-06-28 -- MIR declaration lowering consumes JSON row facts

- Added MIR-specific object/string/number/array row accessors to
  `src/self_hosted/mir_lower/json_fact_read.pgy`.
- Repointed `decl_lower.pgy` declaration, field, method, parameter, and enum
  variant traversal to consume those accessors instead of guessing object spans
  with delimiter strings such as `"},{"kind":`.
- This reduces the stable JSON blocker for MIR declaration lowering. The
  blocker remains active because the shared owner is still a bounded scanner,
  not a complete schema-aware JSON fact table.

### 2026-06-28 -- Backend comparator consumes harness artifact rows

- Added comparable artifact path facts and finding-cap policy to
  `src/self_hosted/compiler/test_harness_owner.pgy`.
- Repointed `backend_output_comparator` so expected/actual fixture paths and
  mismatch finding locations come from the `TestHarness` owner rather than
  tool-local string constants.
- Tightened the component contract to reject backend comparator fixture-path
  literals outside the harness owner.

### 2026-06-28 -- Function emit consumes AST bridge kind facts

- Removed local declaration-kind predicates from
  `src/self_hosted/codegen/emission/function_emit.pgy`.
- Repointed function env, role-operator, struct, enum, and prototype prepasses
  to consume `CodegenAstTextIs*` predicates from the AST text inventory owner.
- This removes another duplicate category classifier from the transitional AST
  text bridge. Name extraction still consumes `CodegenAstTextNode.text`, so the
  mixed AST-like tree blocker remains active.

### 2026-06-28 -- Subprocess owner records oracle compare plan facts

- Added `oracle_compare` timeout and env-allowlist plan facts to
  `src/self_hosted/compiler/subprocess_runner_owner.pgy`.
- Repointed `backend_output_comparator` report emission to record the
  subprocess schema, timeout, env allowlist, stream, and exit-code facts from
  the subprocess owner.
- This reduces shell-owned policy drift for C/LLVM oracle comparison. It does
  not close the subprocess runner blocker because Pergyra still lacks a
  subprocess execution primitive for running the envelope directly.

### 2026-06-28 -- Routine MIR lowering consumes JSON fact owner accessors

- Added shared JSON array-string accessors in `src/self_hosted/lib/json.pgy`
  and a MIR-specific `MirObjectArrayStringFactAt` accessor in
  `src/self_hosted/mir_lower/json_fact_read.pgy`.
- Repointed `routine_lower.pgy` and `routine_inventory_owner.pgy` so routine
  CFG/source facts consume `MirObjectStringFact` / `MirObjectArrayStringFactAt`
  instead of local `JsonFieldString` and `JsonFirstArrayString` calls.
- Tightened the component contract so MIR routine lowering cannot reintroduce
  those direct JSON field scans. The Stable JSON blocker remains active until
  the shared owner becomes a complete schema-aware DOM/fact table.

### 2026-06-28 -- Routine header facts move to the routine inventory owner

- Added routine name, parameter, return, and blocks-start fact accessors to
  `src/self_hosted/mir_lower/routine_inventory_owner.pgy`.
- Repointed `routine_lower.pgy` so function AST-tree headers consume those
  owner facts instead of directly scanning `"params"`, `"return"`, or routine
  name JSON fields.
- Tightened the component contract against those direct routine-header scans.
  This keeps moving MIR lowering toward owner-owned facts while the Stable JSON
  blocker remains active for the broader DOM/fact-table work.

### 2026-06-28 -- Source-local MIR facts use array row accessors

- Repointed `SourceLocalType` in
  `src/self_hosted/mir_lower/json_fact_read.pgy` from manual
  `"source_locals"` string scanning to `MirObjectArrayObjectBoundsAt`.
- Tightened the component contract so the source-local fact reader cannot
  reintroduce local `ReadJsonString` or direct `"source_locals"` key scans.
- This keeps source-local type recovery on the same MIR fact-owner path as
  declaration, routine header, and CFG fact reads.

### 2026-06-28 -- Statement MIR array facts use row accessors

- Repointed `RenderDestructureFromFacts` and `RenderDeferFromFacts` in
  `src/self_hosted/mir_lower/stmt_render.pgy` from manual
  `destructure_bindings` / `defer_body` string scanning to
  `MirObjectArrayStringFactAt`.
- Tightened the component contract so statement rendering cannot reintroduce
  those direct array key scans or local `ReadJsonString` calls.
- This moves destructure and defer reconstruction onto the same MIR fact-owner
  path as declaration, routine header, CFG, and source-local reads.

### 2026-06-28 -- Match pattern MIR facts use array row accessors

- Removed the obsolete `JsonFirstArrayString` helper from
  `src/self_hosted/lib/json.pgy` and added `JsonArrayStringCount`.
- Added `MirObjectArrayStringFactCount` in
  `src/self_hosted/mir_lower/json_fact_read.pgy`.
- Repointed `routine_lower.pgy` match-case condition rendering from manual
  `match_patterns` string scanning to array count plus row accessors.
- Tightened the component contract so match-pattern reconstruction cannot
  reintroduce local `match_patterns` key scans.

### 2026-06-28 -- Program and instruction kind reads consume MIR facts

- Repointed `program_lower.pgy` from direct routine-name string reads to
  `RoutineNameEnd` from the routine inventory owner.
- Repointed `routine_lower.pgy` instruction kind reads from local
  `ReadJsonString` to `MirObjectStringFact`.
- Tightened the component contract so program assembly and instruction-kind
  lowering cannot reintroduce those local string reads.

### 2026-06-28 -- Routine discovery consumes MIR routine row facts

- Added MIR fact accessors for object end, field-value bounds, and `routines`
  array row bounds in
  `src/self_hosted/mir_lower/json_fact_read.pgy`.
- Repointed `routine_inventory_owner.pgy` so routine discovery consumes the
  MIR `routines` array rows and each row's `kind` / `name` facts instead of
  globally scanning `"name"` keys or checking raw `,"kind":"function|method"`
  suffixes after a name string.
- Repointed routine span advancement to the containing routine object's end, so
  program assembly does not re-scan instruction `name` facts while looking for
  the next routine.
- Tightened the component contract so routine inventory discovery cannot
  reintroduce local `ReadJsonString` calls, global name scans, or
  function/method suffix peeks.
  The Stable JSON blocker remains active until the owner is a full schema-aware
  DOM/fact table, but this removes one more local compatibility scan from
  `mir_lower`.

### 2026-06-28 -- Routine block boundaries consume MIR array facts

- Added `RoutineBlocksBounds` to
  `src/self_hosted/mir_lower/routine_inventory_owner.pgy`.
- Repointed `RoutineHeaderEnd` and `RoutineBlocksStart` from direct
  `"blocks"` key scans to the MIR object array-bounds fact owner.
- Tightened the component contract so routine inventory cannot reintroduce
  direct `"blocks"` field scans while computing header/body boundaries. This is
  still within the ACTIVE Stable JSON blocker, but the remaining `mir_lower`
  raw scans are now concentrated in CFG block/instruction traversal.

### 2026-06-28 -- CFG successor reads consume block number facts

- Repointed `ReadSucc` in `src/self_hosted/mir_lower/routine_lower.pgy` from
  direct `succ_true` / `succ_false` key scans to `MirObjectNumberFact`.
- Removed the local digit-run reader that only existed to parse successor
  values after raw key lookup.
- Tightened the component contract so successor reconstruction cannot
  reintroduce local `FindFrom(json, kw, ...)` scans. The remaining
  `routine_lower` raw scans are block lookup and instruction traversal.

### 2026-06-28 -- CFG block lookup consumes block row facts

- Repointed `BlockBounds` in `src/self_hosted/mir_lower/routine_lower.pgy`
  from raw `"id":<n>,"reachable"` marker scans to
  `MirObjectArrayObjectBoundsAt(..., "blocks", ToInt(id), ...)`.
- Added an explicit row `id` fact check before returning the block span, so the
  sequential-id subset assumption is verified instead of hidden in string
  lookup.
- Tightened the component contract so block lookup cannot reintroduce raw block
  marker or `reachable` scans. The remaining `routine_lower` raw scans are now
  instruction traversal and branch/phi detection.

### 2026-06-28 -- CFG instruction traversal consumes row facts

- Added `BlockInstructionBoundsAt`, `BlockInstructionKind`, and
  `BlockInstructionOfKindBounds` in
  `src/self_hosted/mir_lower/routine_lower.pgy`.
- Repointed branch condition, match binding, statement emission, and phi
  detection from local `FindFrom(json, "\"kind\"...")` scans to the block
  `instructions` array row facts.
- Tightened the component contract so `routine_lower.pgy` cannot reintroduce
  local `FindFrom(json, ...)` scans. The `mir_lower` path now consumes
  declaration, routine, block, successor, source-local, statement-array,
  match-pattern, and instruction facts through the MIR JSON fact owner surface.

### 2026-06-28 -- Comparator consumes artifact pair rows through argv

- Added projection-index validation to
  `src/self_hosted/compiler/test_harness_owner.pgy`, including the
  `self_hosted` projection row.
- Repointed `backend_output_comparator` so artifact paths and projection row
  indexes can be supplied through `Args()` while preserving the no-arg fixture
  owner defaults.
- Reworked the backend tri-compare harness to compile the Pergyra comparator
  once and run it on explicit C/LLVM artifact paths, rather than copying
  stdout/stderr into fixed fixture locations to imply the compared artifacts.
- Tightened the component contract and comparator parity gate so the
  `self_hosted` projection row is exercised by a real comparator invocation.

### 2026-06-28 -- AIR graph count reads stop using integer sentinels

- Repointed the AIR graph JSON validator, node-count integrity checker, and
  live-reference checker so summary count reads return `Bool` evidence plus an
  `inout` value row instead of `-1` out-of-band failure values.
- Threaded missing-count evidence into each tool's verdict path, so malformed
  AIR graph summaries fail as explicit checker state rather than by arithmetic
  over a sentinel value.
- Tightened `self_host_pergyra_likeness_smoke.sh` by lowering the sentinel
  ratchet from 39 to 38 after the conversion.

### 2026-06-28 -- AIR graph summary counts use one scan owner

- Renamed the AIR graph summary count reader to
  `AirGraphSummaryIntField` in
  `src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy`.
- Repointed the node-count integrity checker and live-reference checker to
  import that scan owner instead of carrying local summary-number scanners.
- Tightened the component contract so these downstream AIR tools cannot
  reintroduce `ExtractIntField` locals for `intent_count`, `boundary_count`,
  or `evidence_count`.

### 2026-06-29 -- AIR graph scalar facts use one scan owner

- Added `AirGraphScalarFieldValues` to
  `src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy`.
- Repointed `air_graph_id_uniqueness`, `air_graph_node_count_integrity`,
  `air_graph_ref_integrity`, `air_graph_reachability`, and
  `air_graph_ref_live` to consume scalar graph facts from that owner instead
  of rebuilding local `"id"` / `"from"` / `"to"` / `"root"` / `"boundary"` /
  `"intent"` token scanners.
- Tightened the component contract so AIR graph consumers cannot reintroduce
  `ExtractIds`, `ExtractByKey`, `CountKey`, `FirstTokenAfter`, or direct
  `StringIndexOf(rest, ...)` fact recovery. This reduces the ACTIVE Stable JSON
  blocker, but does not close it: the owner is still a bounded schema scanner,
  not a complete JSON DOM/fact table.

### 2026-06-29 -- MIR JSON schema gate consumes document field facts

- Repointed `src/self_hosted/mir_lower/mir_json_input_owner.pgy` from a raw
  `"schema":"pgy.mir.v1"` substring search to
  `JsonDocumentStringFieldEquals(json, "schema", "pgy.mir.v1")`.
- Tightened the component contract so the MIR lowering entry boundary cannot
  reintroduce local schema-string recovery. This is still within the ACTIVE
  Stable JSON blocker, but the MIR input gate now consumes the same document
  field owner as other self-hosted JSON consumers.

### 2026-06-29 -- Codegen runtime usage facts consume AST node inventory

- Added `src/self_hosted/codegen/input/ast_usage_owner.pgy` with
  `CodegenRuntimeUsageFactsFromNodes`.
- Repointed `program_emit.pgy` so runtime/header decisions consume the
  `CodegenAstTextNode` inventory instead of rescanning the whole `ast` string
  for `Args`, arrays, strings, IO, random, Result/Option, and related helper
  triggers.
- Tightened the component contract so `program_emit.pgy` cannot reintroduce
  `StringIndexOf(ast, ...)` for runtime usage facts. This reduces the ACTIVE
  mixed AST-like tree blocker while the transitional `CodegenAstTextNode.text`
  payload remains.

### 2026-06-29 -- Self-host C ABI spelling consumes compiler row projection

- Extended `src/self_hosted/compiler/abi_layout_row_owner.pgy` from a row
  vocabulary envelope to concrete supported C ABI row projections for scalar,
  array bridge, `Result<Int>`, and `Option<Int>` types, including
  materialization policy.
- Repointed `src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy` so
  parameter, return, local, and field C type spelling consumes those compiler
  rows first. The only remaining local lookup is the `TypeEnvZone` user-struct
  fact, which is a separate owner.
- Tightened the self-host component/world gates so ABI spelling cannot
  reintroduce collection-runtime kind lookup or bypass the compiler row owner.
  This reduces the ACTIVE ABI/layout row projection blocker; native C/LLVM
  still need to consume the same concrete row table before the blocker closes.

### 2026-06-29 -- Codegen parity run output consumes Artifact/TestHarness owner

- Repointed `tests/self_hosted/parity/codegen_parity.sh` so generated
  run-output comparison is no longer a local shell string verdict. The script
  builds the Pergyra `backend_output_comparator` once and invokes it for each
  committed-expected/live-oracle and expected/generated run-output artifact
  pair.
- The comparator consumes `ArtifactZone`, `TestHarnessZone`, and subprocess
  envelope rows, so codegen parity now records the `run_output`,
  `c_oracle`, and `self_hosted` projection facts through the Pergyra owner.
- This reduces the ACTIVE Artifact Zone and Test Harness blockers; they remain
  active until diagnostics, IR JSON, ABI/layout, emitted C/LLVM, and all other
  parity scripts are also projections of these Pergyra-owned records.

### 2026-06-30 -- AST-text payload reads move behind the input owner

- Extended `src/self_hosted/codegen/input/ast_text_inventory_owner.pgy` with
  owner-owned accessors for function names, return types, nominal names, role
  names and `for` types, parameter mode/name/type rows, and field name/type
  rows.
- Repointed `program_emit.pgy` and `function_emit.pgy` so declaration routing,
  signature emission, struct collection, role-operator discovery, and prototype
  collection consume those accessors instead of carrying local AST-text payload
  parsers.
- Tightened `self_hosted_component_contract_smoke` so the removed local
  emission parsers cannot reappear. This reduces the ACTIVE mixed AST-like tree
  blocker; it does not close it because `CodegenAstTextNode.text` remains the
  transitional bridge payload inside the input owner.

### 2026-06-30 -- AST-text marker and enum rows move behind the input owner

- Added owner-owned predicates for `Parameters:`, `Returns:`, and `Fields:`
  marker nodes, so signature and struct prepasses no longer compare marker
  strings in emission code.
- Added enum declaration accessors for enum name, variant count, and variant
  name rows. `CollectEnums` now consumes these rows instead of parsing the
  `Enum: Name { ... }` line locally.
- Tightened the component contract to reject the old marker-string and enum-line
  parsing shapes in `function_emit.pgy`. This further reduces the ACTIVE mixed
  AST-like tree blocker while leaving statement-body AST text as the remaining
  intentional bridge surface.

### 2026-06-30 -- Let and Assign statement facts move behind the input owner

- Added owner-owned `Let` and `Assign` statement predicates plus accessors for
  let name/type/initializer and assign target/value rows in
  `ast_text_inventory_owner.pgy`.
- Repointed `stmt_emit.pgy` so `EmitLet`, `EmitTryLet`, and `EmitAssign`
  consume `CodegenAstTextNode` facts instead of splitting `Let:` / `Assign:`
  lines locally.
- Tightened the component contract to reject the old `line: String` statement
  parser shapes and direct `Let:` / `Assign:` dispatch in `stmt_emit.pgy`. The
  broader statement-body text bridge remains active for control flow, calls,
  collection statements, and expression payloads.

### 2026-06-30 -- Simple statement facts move behind the input owner

- Added owner-owned predicates and payload accessors for `Log`, bare/value
  `Return`, `Defer`, `ArrayPop`, `Exit`, `Break`, and `Continue` statement
  rows.
- Repointed `stmt_emit.pgy` so these statements consume
  `CodegenAstTextNode` facts instead of matching and slicing the AST text line
  locally.
- Tightened the component contract so those statement dispatch shapes cannot
  reappear in `stmt_emit.pgy`. `ArraySet`/`ArrayPush`, control-flow statements,
  else-if routing, bare call statements, and expression payload parsing remain
  the next AST-text bridge surfaces.

### 2026-06-30 -- Array mutation statement facts move behind the input owner

- Added owner-owned predicates and payload accessors for `ArraySet` and
  `ArrayPush` statement rows, including `target`, `index`, and `value` facts.
- Repointed `stmt_emit.pgy` so collection mutation statements consume
  `CodegenAstTextNode` facts and only choose the collection runtime helper plus
  emitted C spelling locally.
- Tightened the component contract to reject direct `ArraySet` / `ArrayPush`
  AST-text dispatch and argument splitting in `stmt_emit.pgy`. The remaining
  statement-body bridge surfaces are control-flow statements, else-if routing,
  bare call statements, and expression payload parsing.

### 2026-06-30 -- Control-flow statement facts move behind the statement owner

- Split `src/self_hosted/codegen/input/ast_text_statement_owner.pgy` out from
  the AST-text inventory owner so statement rows have their own fact boundary
  instead of inflating the node-inventory owner.
- Added owner-owned accessors for `For`, `While`, `If`, `Else`, and `else if`
  routing facts. `stmt_emit.pgy` now consumes loop variables, range bounds,
  foreach collections, conditions, and else routing through that owner instead
  of slicing AST text locally.
- Tightened the component contract to reject the old control-flow string
  dispatch and parsing shapes in `stmt_emit.pgy`. The remaining bridge surfaces
  are bare call statements and expression payload parsing; the mixed AST-like
  tree blocker remains active until typed/tagged AST rows replace line text.

### 2026-06-30 -- Bare call statement facts move behind the statement owner

- Added owner-owned bare-call statement predicate and expression accessors to
  `ast_text_statement_owner.pgy`.
- Repointed `stmt_emit.pgy` so void/user call statements consume the statement
  owner instead of calling `IsSingleCall` on raw `node.text` in the emitter.
- Tightened the component contract to reject the old bare-call `IsSingleCall(t)`
  and `RewriteExpr(t, ...)` path. The remaining bridge surface is expression
  payload parsing; the mixed AST-like tree blocker remains active until
  typed/tagged AST rows replace line text.

### 2026-06-30 -- Qualified call spelling consumes the symbol owner

- Repointed `RewriteQualifiedCalls` so namespace-qualified calls use
  `CompilerSymbolCQualifiedName(owner, member)` instead of locally inserting an
  underscore between owner and member text.
- Tightened the component contract to require the symbol owner consumer in
  `expr_rewrite.pgy` and reject the old direct `out = Concat(out, "_")`
  spelling path.
- This reduces the self-host symbol/mangle SoT gap for expression lowering. The
  broader symbol/mangle blocker remains active until native C, LLVM, and
  self-hosted projections consume one concrete cross-backend symbol row table.

### 2026-07-02 -- SEA lane facts consume boundary-owned evidence

- Repointed AIR lane capture classification away from source-kind/boundary-kind
  guesses. `AIRBoundaryNode` now carries RIR/MIR evidence summary bits for
  await-local, movability requirement, deterministic fork-join, raw channel,
  raw slot, live view, pin cleanup, and value-only capture, and
  `air_execution_lane.c` consumes those bits to build `BoundaryCaptureFact`.
- Extended the self-host SEA mirror with a typed `BoundaryLaneInputFact` so the
  Pergyra implementation consumes the same evidence shape instead of
  reconstructing lane decisions from source labels. The parity golden now covers
  the 10 C policy rows plus 17 AIR evidence-shape rows on both C and LLVM.
- Tightened the lane gates so `air_boundary_source_kind(boundary)`,
  `BoundarySourceKind`, `source_kind`, and source-string lane APIs cannot
  reappear in this path. Remaining work is producer depth, especially complete
  MIR value-capture evidence for all boundary shapes, not facade routing.

### 2026-07-02 -- Codegen Option<String> payload enters the ABI/runtime row owners

- The typed AST arena fixture exposed a real self-host codegen subset gap:
  `Option<String>` functions and locals had semantic support in the source
  language, but the self-host C emitter only had `Option<Int>` ABI/runtime facts.
- Added `Option<String>` to `compiler/abi_layout_row_owner.pgy` and consumed it
  through `abi_layout_owner.pgy`; added payload-aware Option constructor/None
  symbol facts to `runtime_abi/option_result_runtime_owner.pgy`; and moved
  `Some(...)` emission to a typed constructor rewrite that consults `ExprKind`
  before index lowering erases array element payload facts.
- Added `option_string_core` to the codegen parity suite. Verified
  `self-host-component-contract-test-smoke`,
  `self-host-codegen-parity-test-smoke` (**64 fixtures**, C and LLVM), and
  `self-host-codegen-bootstrap-test-smoke` (`gen2 == gen3`, 5484 generated-C
  lines, plus lexer/parser/semantic/mir_lower/tool/fuzz generator parity).

### 2026-07-02 -- Zone lane pinning becomes RIR-evidence-owned

- Split the last coarse SEA zone pin decision out of boundary kind. Zone
  boundaries now become `PinnedZone` only when
  `has_rir_zone_pin_evidence` is produced by the RIR boundary evidence owner;
  a hand-built zone boundary with no RIR zone-pin evidence stays
  `LocalAsync`.
- Tightened the producer side as well: RIR raw-slot/live-view capture evidence
  is now limited to resource-capturing parallel boundaries, so a world
  transfer step is not over-pinned merely because slot operations appear under
  the same step AST.
- Updated the self-host SEA mirror and parity golden from 27 to 29 rows, and
  fixed the AIR unit-test link slice so `test-air` links the lane owner objects
  that `air.c` consumes.
- Verified `execution-lane-policy-test-smoke`,
  `self-host-execution-lane-parity-test-smoke`,
  `sea-execution-lane-golden-test-smoke`, `air-json-schema-test-smoke`,
  `self-host-preparation-test-smoke`, and `test-air` (139/139).

### 2026-07-02 -- Self-host text and JSON absence become Option-owned

- Moved self-host codegen statement delimiter lookup behind `FindTextFrom`,
  returning `Option<Int>` instead of leaking `StringIndexOf`'s `-1` sentinel
  into statement-row consumers. `Let`, `Assign`, `For in`, and range `..`
  parsing now prove delimiter presence before slicing.
- Converted the shared self-host JSON end scanners (`JsonArrayEnd`,
  `JsonObjectEnd`, `JsonDocumentObjectEnd`, and `JsonValueEnd`) to
  `Option<Int>` and repointed the AIR graph validator, module manifest
  resolver, and MIR fact readers to unwrap only after an explicit presence
  check.
- Tightened the component contract so direct statement-owner delimiter
  `StringIndexOf` scans, JSON `return -1` absence paths, and direct
  `JsonDocumentObjectEnd`/`JsonValueEnd` integer consumers cannot reappear.
  The Pergyra-likeness ratchet now permits only 11 remaining sentinel-style
  paths and requires 222 `Result`/`Option`-style uses.
- Verified the narrow and bootstrap gates:
  `self-host-pergyra-likeness-test-smoke`,
  `self-host-component-contract-test-smoke`,
  `self-host-codegen-parity-test-smoke` (64 fixtures on C and LLVM), and
  `self-host-codegen-bootstrap-test-smoke` (`gen2 == gen3`,
  self-host-built codegen tool matches the oracle on committed probes).

### 2026-07-02 -- Codegen statement owner enters semantic selfcheck

- Promoted `src/self_hosted/codegen/input/ast_text_statement_owner.pgy` into
  the real-source semantic selfcheck manifest after verifying the checker
  accepts it as a normal imported source unit.
- Tightened the component contract so the selfcheck manifest must include the
  statement fact owner and the accepted real-source count is now 91 on both C
  and LLVM. This keeps the active codegen statement SoT owner inside the hard
  self-host semantic pass condition instead of only inside source-shape smoke
  checks.

### 2026-07-03 -- MIR JSON string facts become Option-owned

- Added `JsonObjectStringFieldOpt(...) -> Option<String>` to the shared
  self-host JSON owner. The old `JsonObjectStringField(...) -> String` remains
  as a compatibility wrapper, but absence and malformed string payloads now have
  a typed fact path instead of relying on the empty-string sentinel.
- Added `MirObjectStringFactOpt(...) -> Option<String>` in the MIR JSON fact
  reader and repointed the MIR fact graph payload contract to consume that fact
  for routine name, instruction source type, and expression payload checks.
- Tightened `self_hosted_component_contract_smoke.sh` so the MIR fact graph
  contract cannot return to direct `JsonObjectStringField(json, ...)` sentinel
  comparison.
- Verified `tests/self_hosted_component_contract_smoke.sh`,
  `tests/self_hosted/parity/selfcheck_sources.sh` with C backend
  (**107 real sources accepted**), and
  `tests/self_hosted/parity/mir_json_parity.sh` (**86 fixtures**, 0 clean
  rejects).

### 2026-07-03 -- JSON emit splits from read owner and number facts become Option-owned

- Split self-host JSON emission helpers into `src/self_hosted/lib/json_emit.pgy`.
  The read owner `json.pgy` now owns bounded object/array/string/number reads,
  while the emit owner owns string escaping and object/array/field emission.
- Added `JsonObjectNumberFieldOpt(...) -> Option<String>` and
  `MirObjectNumberFactOpt(...) -> Option<String>`. The legacy string-returning
  functions remain as compatibility wrappers, but AIR graph summary counts now
  consume the typed number-presence fact instead of checking an empty-string
  sentinel.
- Promoted `json_emit.pgy` into the real-source selfcheck manifest and updated
  the owner ledger so JSON read/emit responsibilities are no longer stored in
  one growing file.
- Verified `tests/self_hosted_component_contract_smoke.sh`,
  `tests/self_hosted/parity/air_graph_json_validator_parity.sh`, and
  `tests/self_hosted/parity/selfcheck_sources.sh` with C backend
  (**108 real sources accepted**).

### 2026-07-03 -- JSON emit consumers declare the emit owner directly

- Removed the transitional `json.pgy -> json_emit.pgy` import path. JSON read
  consumers keep importing `json.pgy`, while report/render consumers that call
  `JsonEmit*`, `JsonStringLiteral`, or `JsonEscapeString` now import
  `json_emit.pgy` directly.
- Tightened the component contract so `json.pgy` cannot resume re-exporting the
  emit owner, and the current emit consumers must keep the direct owner import.

### 2026-07-03 -- Option `?` enters the self-host semantic and codegen subset

- Added self-host semantic support for Option try operands. `ExprType` now maps
  `Option<T>?` to `T`, and expression diagnostics reject `?` on non-Option
  operands through the existing structured diagnostic surface.
- Extended self-host C codegen try-let lowering so an Option-returning function
  emits an `is_some` guard and returns the enclosing function's typed `None`
  on failure. Result try-let lowering remains unchanged.
- Added semantic fixture `valid_option_try_payload` and codegen fixture
  `option_try`, raising semantic parity to 108 fixtures and codegen parity to
  65 fixtures.
- Kept the Pergyra-likeness ratchet closed by routing try-operand extraction
  through a bounds fact instead of new text-in/text-out helper functions, and
  by treating the split JSON emit owner as the same text-domain exception as
  the JSON read owner.
- Repaired the C MIR SSA local type lookup order found while rerunning the
  broader gates: exact versioned DEF type facts still win first, but unique
  source-local type facts now precede AST annotation rendering.
- Verified with `self-host-preparation-test-smoke`, C/LLVM self-host codegen
  parity, and `cfg-body-dataflow-test-smoke` on the local Windows toolchain.

### 2026-07-03 -- AIR summary counts consume JSON fact-table rows

- Added `JsonObjectFactObjectTable`, `JsonObjectFactStringFieldOpt`, and
  `JsonObjectFactNumberFieldOpt` to the shared self-host JSON fact-table owner.
  AIR graph summary count readers now consume the nested `summary` object and
  number field through that owner instead of carrying raw summary bounds into
  `scan_owner.pgy`.
- Tightened the component contract so the AIR graph validator cannot return to
  `JsonObjectFactValueBounds(root, "summary", summary_bounds)` plus direct
  `JsonObjectNumberFieldOpt(content, bounds...)` in the consuming tool.
- Tightened the Pergyra-likeness `result_use` ratchet from 273 to 278 so the
  new typed absence facts cannot be removed without failing the gate.
- Verified `self-host-component-contract-test-smoke`,
  `self-host-air-graph-consumer-parity-test-smoke`, and
  `self-host-semantic-selfcheck-test-smoke` (110 real sources accepted on both
  C and LLVM).
- Broader CI-repro checks also passed locally: `build-source-inventory`,
  `checkedarith-failclosed`, `semantic-core-shape`, `slot-contract`,
  `self-host-preparation` (including bootstrap `SELF-HOSTING OK`),
  `llvm-test-backend-compare`, and `air-strict-backend-compare-test-smoke`
  (885/885 backend-compare fixtures each).

### 2026-07-04 -- Self-host C codegen consumes target capability envelope

- Repointed `src/self_hosted/codegen/run/codegen_run_owner.pgy` to consume
  `CompilerTargetCapabilityEnvelopeReady()` before `GenerateC`. The
  `TargetCapabilityZone` is now load-bearing for the self-host C codegen run
  boundary instead of being only a compiler-world vocabulary fact.
- Tightened `tests/self_hosted_component_contract_smoke.sh` so the run owner
  must import `target_capability_owner.pgy` and call the readiness predicate.
- Updated the pre-self-host expansion ledger and codegen intent doc to say
  this closes the self-host C run-boundary omission, while native C/LLVM
  target-specific consumers remain active work.
- No executable cases were run for this slice; validation was limited to static
  owner/ratchet inspection under the validation isolation policy.

### 2026-07-04 -- Backend comparator rejects positional artifact kind

- Tightened `self_hosted_component_contract_smoke.sh` so
  `backend_output_comparator` must consume `CompilerRunOutputArtifactKind()`
  and `CompilerArtifactKindKnown(CompilerRunOutputArtifactKind())` instead of a
  positional `CompilerArtifactKindAt(...)` row.
- The artifact-zone owner still pins the exact vocabulary order internally,
  including `CompilerArtifactKindAt(8) == CompilerRunOutputArtifactKind()`, but
  the comparator consumer is now ratcheted to the named run-output artifact.
- No executable cases were run for this slice; validation was limited to static
  owner/ratchet inspection under the validation isolation policy.

### 2026-07-04 -- Tri-compare verdict owned by artifact comparator

- Removed the duplicate shell `files_equal` / `show_diff` veto from
  `backend_output_tri_compare_parity.sh`. The script still builds C and LLVM
  outputs, but the stdout/stderr equality verdict is now the
  `backend_output_comparator` JSON report with the expected schema and
  `ok:true`.
- Fixed the tri-compare native runner so comparator argv is forwarded to the
  generated comparator binary on both the normal path and the Windows fallback.
  The comparator now receives the expected/actual artifact paths and projection
  row indexes instead of silently running its default fixture while shell
  comparison carried the real verdict.
- Tightened `self_hosted_component_contract_smoke.sh` so the tri-compare script
  must keep comparator argv forwarding, schema/`ok:true` checks, and cannot
  reintroduce local `cmp -s`, `diff -u`, `files_equal`, or `show_diff` verdict
  ownership.
- Updated the pre-self-host expansion ledger to record tri-compare as a
  TestHarness/ArtifactZone consumer instead of a shell-owned comparison with a
  Pergyra advisory check.
- No executable cases were run for this slice; validation was limited to static
  owner/ratchet inspection under the validation isolation policy.

### 2026-07-04 -- Codegen bootstrap fixpoint consumes emitted-C artifact owner

- Added `CompilerEmittedCArtifactKind()` to the self-host ArtifactZone owner and
  changed `backend_output_comparator` so callers can pass an explicit
  artifact-kind row value. The comparator still defaults to `run_output`, but
  validates every provided artifact kind through `CompilerArtifactKindKnown(...)`
  before writing the report.
- Repointed the codegen bootstrap fixpoint check (`gen2.c == gen3.c`) from
  shell `cmp` / `diff` ownership to the Pergyra
  `backend_output_comparator` with artifact kind `emitted_c` and self-hosted
  projection rows.
- Tightened `self_hosted_component_contract_smoke.sh` so comparator artifact
  kind reporting cannot regress to hardcoded `run_output`, and so bootstrap
  must compile/use the shared comparator for the emitted-C fixpoint artifact.
- No executable cases were run for this slice; validation was limited to static
  owner/ratchet inspection under the validation isolation policy.

### 2026-07-04 -- AIR graph validator consumes compiler AIR evidence envelope

- Repointed `src/self_hosted/tools/air_graph_json_validator/run_owner.pgy` to
  consume `CompilerAirEvidenceEnvelopeReady()` before reading AIR fixtures. This
  makes the compiler-world AIR evidence vocabulary load-bearing for the first
  self-host AIR JSON consumer.
- Updated `tests/self_hosted/parity/air_graph_json_validator_parity.sh` so the
  isolated tool build copies `air_evidence_owner.pgy` into the compiler owner
  slot required by the new import.
- Tightened `self_hosted_component_contract_smoke.sh` and
  `self_host_preparation_smoke.sh` so the AIR graph validator run boundary
  cannot return to a fixture-only AIR scan without the compiler evidence owner.
- Updated the AIR graph validator intent and pre-self-host expansion ledger.
  This closes the run-boundary omission, but live AIR evidence-row consumption
  remains active work.
- No executable cases were run for this slice; validation was limited to static
  owner/ratchet inspection under the validation isolation policy.

### 2026-07-04 -- AIR graph clean JSON parity uses artifact comparator

- Repointed `tests/self_hosted/parity/air_graph_json_validator_parity.sh` clean
  expected-vs-actual JSON equality to the Pergyra
  `backend_output_comparator` instead of direct shell string comparison.
- Kept shell grep as count ground truth and left live AIR fixture drift
  detection for a later slice; only the clean artifact equality verdict moved
  behind ArtifactZone/TestHarness owner rows in this slice.
- Tightened `self_hosted_component_contract_smoke.sh` and
  `self_host_preparation_smoke.sh` so this parity leg must keep
  `compare_clean_json_with_owner` and the shared comparator helper calls.
- Updated the pre-self-host expansion ledger to list AIR graph validator clean
  JSON parity as another ArtifactZone/TestHarness consumer.
- No executable cases were run for this slice; validation was limited to static
  owner/ratchet inspection under the validation isolation policy.

### 2026-07-05 -- AIR graph live drift uses AIR artifact owner

- Repointed the two live `pgy --air-json` fixture drift checks in
  `tests/self_hosted/parity/air_graph_json_validator_parity.sh` from shell
  `diff -q` to `backend_output_comparator` with artifact kind `air_json`.
- Preserved the existing `PGY_AIR_GRAPH_JSON_SKIP_DRIFT=1` escape hatch for
  C-only lanes whose committed fixtures are pinned against an LLVM-enabled
  shape; the verdict path is still the Pergyra artifact owner when the drift
  rung is active.
- Tightened `self_hosted_component_contract_smoke.sh` so the validator parity
  script must keep `compare_air_json_file_with_owner`, must name `air_json`, and
  cannot reintroduce `diff -q`.
- Verified with `self-host-component-contract-test-smoke` and
  `self-host-air-graph-consumer-parity-test-smoke`; the latter exercised the
  live-drift path with `live-drift=ok`.

### 2026-07-05 -- Diagnostic report parity uses ArtifactZone

- Repointed `diagnostic_catalog_checker_parity.sh` clean, missing-code, and
  missing-input report JSON equality from shell string comparison to
  `pgy_selfhost_compare_expected_text_artifact_with_owner(...)` with artifact
  kind `run_output`.
- Repointed `linter_parity.sh` C and LLVM diagnostic JSON payload equality from
  direct shell string comparison to the same comparator with artifact kind
  `diagnostics`.
- Tightened `self_hosted_component_contract_smoke.sh` so those parity scripts
  cannot reintroduce local expected-JSON string reads or C/LLVM string drift
  checks.
- Verified with `self-host-diagnostic-catalog-parity-test-smoke` and
  `self-host-linter-parity-test-smoke`.

### 2026-07-05 -- More report parity uses ArtifactZone

- Repointed clean report JSON equality in `runtime_boundary_checker`,
  `stable_subset_section_checker`, `stdlib_dispatch_inventory_checker`,
  `production_c_size_checker`, and `production_header_size_checker` parity
  scripts from local shell string comparison to
  `pgy_selfhost_compare_expected_text_artifact_with_owner(...)`.
- Used artifact kind `run_output` for these reports; script-local grep/count
  checks still provide each tool's domain-specific ground truth. The production
  size checks keep their existing `max_lines` normalization by comparing a
  normalized expected artifact against normalized live output through the same
  owner.
- Tightened `self_hosted_component_contract_smoke.sh` so those scripts cannot
  reintroduce local `EXPECTED_JSON="$(cat "$EXPECTED_JSON_FILE")"` verdicts or
  `clean JSON parity FAIL` branches.

### 2026-07-05 -- Fuzz generator output parity uses ArtifactZone

- Repointed `fuzz_backend_parity_generator_parity.sh` manifest, generated
  source, stdout, and stderr file equality from local `cmp`/`diff` helpers to
  `pgy_selfhost_compare_expected_text_artifact_file_with_owner(...)`.
- Used artifact kind `emitted_self_hosted`, matching the existing
  codegen-bootstrap fuzz-generator corpus comparison owner.
- Tightened `self_hosted_component_contract_smoke.sh` so the fuzz generator
  parity script cannot reintroduce `files_equal`, `show_diff`, `cmp -s`, or
  `diff -u` verdict paths.

### 2026-07-05 -- Scale probes consume ArtifactZone comparisons

- Repointed `lexer_scale_probe.sh` and `parser_scale_probe.sh` byte-equality
  checks from local `cmp -s` to a non-fatal `backend_output_comparator` verdict.
- The lexer probe records token text as `run_output`; the parser probe records
  AST text as `ast_text`. The probes remain coverage measurements rather than
  hard parity gates, so mismatch still increments the drift counters instead of
  aborting the run.
- Tightened `self_hosted_component_contract_smoke.sh` so these probes cannot
  reintroduce direct `cmp -s` verdicts.

### 2026-07-04 -- Codegen bootstrap corpus consumes emitted-self-hosted artifact owner

- Added `CompilerEmittedSelfHostedArtifactKind()` to the self-host ArtifactZone
  owner and made `CompilerArtifactZoneReady()` check the emitted-self-hosted
  row through the named owner function.
- Repointed codegen bootstrap fuzz-generator manifest and generated `f*.pgy`
  corpus comparisons from shell-owned `cmp` / `diff` helpers to the shared
  `backend_output_comparator` with artifact kind `emitted_self_hosted`.
- Generalized the bootstrap comparator helper so the emitted-C fixpoint and
  emitted-self-hosted corpus checks use the same ArtifactZone/TestHarness
  verdict path.
- Tightened `self_hosted_component_contract_smoke.sh` so the bootstrap script
  cannot reintroduce `compare_emitted_c_with_owner`, `files_equal_text`,
  `show_file_delta`, `cmp -s`, or `diff -u`.
- No executable cases were run for this slice; validation was limited to static
  owner/ratchet inspection under the validation isolation policy.

### 2026-07-04 -- AIR graph consumers compare clean JSON through ArtifactZone

- Added `pgy_selfhost_compare_expected_text_artifact_with_owner()` to the shared
  self-host parity helper. The helper normalizes an expected text artifact and
  a live text artifact, then asks `backend_output_comparator` for the verdict
  with an explicit artifact kind.
- Repointed the AIR graph id-uniqueness, node-count, reachability,
  ref-integrity, and ref-live clean JSON equality checks from local shell
  string comparison to the shared helper with artifact kind `air_json`.
- Kept shell grep/count checks as local ground truth for the specific graph
  invariant; only the JSON artifact equality verdict moved behind
  ArtifactZone/TestHarness.
- Tightened `self_hosted_component_contract_smoke.sh` so those AIR graph
  consumer parity scripts must call the helper and cannot reintroduce local
  `EXPECTED_JSON="$(cat "$EXPECTED_JSON_FILE")"` clean JSON verdicts.
- No executable cases were run for this slice; validation was limited to static
  owner/ratchet inspection under the validation isolation policy.

### 2026-07-04 -- General self-host report JSON parity uses ArtifactZone

- Repointed clean report JSON equality in `ast_read_surface_checker`,
  `doc_link_checker`, `examples_inventory_checker`, and
  `module_manifest_resolver` parity scripts from local shell string comparison
  to `pgy_selfhost_compare_expected_text_artifact_with_owner(...)`.
- Used artifact kind `run_output` for these reports because the compared
  artifact is the tool's stdout JSON report, not AIR/MIR source JSON.
- Kept the script-local shell grep/count checks as ground truth for each
  tool-specific invariant; only the committed-expected vs live-report equality
  verdict moved behind ArtifactZone/TestHarness.
- Tightened `self_hosted_component_contract_smoke.sh` so these scripts cannot
  reintroduce local `EXPECTED_JSON="$(cat "$EXPECTED_JSON_FILE")"` clean JSON
  verdicts.
- No executable cases were run for this slice; validation was limited to static
  owner/ratchet inspection under the validation isolation policy.

### 2026-07-05 -- Diagnostic catalog parity consumes TestHarness path owner

- Added the `diagnostic-catalog-paths` suite to the self-host TestHarness tool
  path owner. The suite owns the checker source, clean/missing expected JSON
  artifacts, diagnostic code owner, docs owner, and C oracle path.
- Repointed `diagnostic_catalog_checker_parity.sh` so those paths come from the
  compiled Pergyra manifest instead of shell constants.
- Extended the diagnostic catalog run boundary so code/docs owner paths can be
  passed through `Args()`. The no-arg defaults remain the same, but hard parity
  now proves the input boundary consumes owner-supplied paths.
- Tightened `self_hosted_component_contract_smoke.sh` so the manifest suite,
  path-arg consumption, and source/expected/oracle shell-constant removal cannot
  regress.
- Promoted `diagnostic_catalog_checker/run_owner.pgy` into the real-source
  semantic selfcheck manifest, raising the accepted self-host owner/source count
  to 130.

### 2026-07-05 -- AIR graph validator consumes TestHarness path owner

- Added the `air-graph-json-validator-paths` suite to the self-host TestHarness
  tool path owner. The suite owns the validator source, AIR evidence owner,
  expected clean JSON, committed AIR fixtures, and live AIR source paths.
- Repointed `air_graph_json_validator_parity.sh` so those paths come from the
  compiled Pergyra manifest instead of shell constants.
- Extended the AIR graph validator run boundary so fixture paths can be passed
  through `Args()`. The no-arg defaults remain the same, but hard parity now
  proves the input boundary consumes owner-supplied fixture paths.
- Promoted `air_graph_json_validator/run_owner.pgy` into the real-source
  semantic selfcheck manifest, raising the accepted self-host owner/source count
  to 131.

### 2026-07-06 -- AST read surface parity removes shell clean oracle

- Repointed `ast_read_surface_checker_parity.sh` so the clean report verdict is
  only the self-hosted checker JSON compared against committed
  `expected/clean.json` through the shared ArtifactZone/TestHarness comparator.
- Removed the parity script's duplicate shell `grep`/`wc` clean-count
  recomputation and stopped invoking `tests/ast_read_surface_smoke.sh` inside
  the self-hosted parity rung. The shell smoke remains a separate production
  ratchet gate over the same ratchet spec.
- Kept the synthetic source_ast growth fixture so the self-hosted owner still
  proves fail-closed behavior when the measured surface grows.
- Tightened `self_hosted_component_contract_smoke.sh` so the shell clean-count
  oracle and embedded shell smoke cannot return to this parity rung.

### 2026-07-06 -- Production C size parity removes shell clean oracle

- Repointed `production_c_size_checker_parity.sh` so the clean verdict is the
  self-hosted checker JSON compared through ArtifactZone/TestHarness, with only
  `max_lines` normalized to avoid line-count fixture churn.
- Removed the parity script's duplicate shell `find`/`wc`/`awk` implementation
  for production `.c` inventory, violation count, and max-line count.
- Kept the synthetic 1001-line `.c` fixture as the fail-closed proof for the
  cap semantics, and kept the C/LLVM leg parity.
- Tightened `self_hosted_component_contract_smoke.sh` so the shell count oracle
  cannot return to the production C size parity rung.

### 2026-07-06 -- Production header size parity removes shell clean oracle

- Repointed `production_header_size_checker_parity.sh` so the clean verdict is
  the self-hosted checker JSON compared through ArtifactZone/TestHarness, with
  `max_lines` normalized for the same reason as production C size.
- Removed the parity script's duplicate shell `find`/`wc`/`awk` implementation
  for production header inventory, violation count, and max-line count.
- Kept the synthetic 701-line `.h` fixture as the fail-closed proof for the
  header cap semantics, and kept the C/LLVM leg parity.
- Tightened `self_hosted_component_contract_smoke.sh` so the shell count oracle
  cannot return to the production header size parity rung.

### 2026-07-06 -- Stdlib dispatch inventory removes shell clean oracle

- Repointed `stdlib_dispatch_inventory_checker_parity.sh` so the clean verdict
  is the self-hosted checker JSON compared through ArtifactZone/TestHarness.
- Removed the parity script's duplicate shell `grep` count and drift-tolerance
  implementation for the C and LLVM stdlib dispatch tables.
- Kept the synthetic LLVM-entry deletion fixture as the fail-closed proof for
  `count_drift`, and kept the C/LLVM leg parity.
- Tightened `self_hosted_component_contract_smoke.sh` so shell dispatch-count
  oracles cannot return to this parity rung.

### 2026-07-06 -- Diagnostic catalog parity removes shell clean counters

- Repointed `diagnostic_catalog_checker_parity.sh` so the clean structured
  counter verdict is the self-hosted checker JSON compared through
  ArtifactZone/TestHarness.
- Kept `tests/diagnostic_registry_smoke.sh` as the C exit-class bridge, but
  removed shell `grep`/`sort`/`sed` reconstruction of `codes`, `documented`,
  `duplicates`, and `orphans`.
- Kept the synthetic missing-code and missing-input fixtures as fail-closed
  proof for the report owner and run boundary, and kept the C/LLVM leg parity.
- Tightened `self_hosted_component_contract_smoke.sh` so shell diagnostic
  counter oracles cannot return to this parity rung.

### 2026-07-06 -- Stable subset section docs align with self-host oracle

- Updated `stable_subset_section_checker/intent.md` to match the already-landed
  parity script: clean truth is the self-hosted checker JSON compared through
  ArtifactZone/TestHarness, plus a synthetic missing-section fixture.
- Removed stale wording that described shell `grep -c '^## '` as the auxiliary
  clean oracle.
- Tightened `self_hosted_component_contract_smoke.sh` so the stale shell-oracle
  wording cannot return to the intent contract.

### 2026-07-06 -- Backend output comparator removes shell text oracle

- Removed the final clean-fixture shell text-equivalence block from
  `backend_output_comparator_parity.sh`. The comparator now owns artifact
  equality even in its own parity rung.
- Kept the comparator bootstrap expected-JSON check plus the synthetic mismatch
  and missing-input fixtures as the proof surface.
- Updated the pre-self-host expansion ledger so ArtifactZone evidence no longer
  describes `backend_output_comparator_parity.sh` as a shell external oracle.
- Tightened `self_hosted_component_contract_smoke.sh` so the shell text
  equivalence block and stale wording cannot return.

### 2026-07-06 -- Subprocess runner emits owner-owned plan JSON

- Added `pgy.selfhost.subprocess-plan.v1` emission to
  `subprocess_runner_owner.pgy`. The plan records use case, executable path,
  argv, cwd, env allowlist, timeout, stream, and exit-code facts through named
  owner functions.
- Repointed `backend_output_comparator` so its `source` object embeds the
  nested subprocess plan from `CompilerSubprocessOracleComparePlanJson(...)`
  instead of leaving those execution facts as free fields owned by the parity
  runner.
- Kept the subprocess blocker ACTIVE: shell still launches the process today.
  This slice only makes the future runner's input envelope structured and
  Pergyra-owned.
- Verified with `self-host-component-contract-test-smoke`,
  `self-host-compiler-world-contract-test-smoke`, and
  `backend_output_comparator_parity.sh`.

### 2026-07-07 -- Typed AST arena removes placeholder node row

- Replaced the typed AST arena's placeholder `nodes: Array<Int>` field with
  parallel typed node facts for kind, atom, atom-presence, child spans, child
  edges, and atom text.
- Added `NodeId` lookup accessors that return `Option` for node kind, child
  lookup, and atom text, so later parser/codegen cutovers can consume owned
  typed rows instead of line-text payloads.
- Expanded the readiness fixture from raw child/atom arrays to a concrete
  Program -> FuncDecl(Main) -> Block arena traversal.
- Opened the matching self-host C-emitter surface for struct fields whose type
  is `Array<Int>` or `Array<String>` by routing field initializers through the
  collection runtime owner. Member array reads such as `arena.kinds[i]` now
  consume field type facts before lowering to collection runtime accessors.
- Tightened `self_hosted_component_contract_smoke.sh` so the typed AST owner
  cannot regress to the old placeholder field or local array-field guessing
  without failing the gate.
- The mixed AST-like tree blocker remains ACTIVE: codegen still consumes the
  transitional `pgy --ast` text bridge. This slice makes the replacement owner
  concrete; it does not claim full AST replacement.
- Verified with `self-host-component-contract-test-smoke`,
  `self-host-compiler-world-contract-test-smoke`, and
  `self-host-codegen-bootstrap-test-smoke` (`SELF-HOSTING OK`).

### 2026-07-07 -- Typed AST arena projection becomes bridge-load-bearing

- Added `CodegenAstTextTypedArenaFromNodes(...)`, which projects the real
  `CodegenAstTextNode` inventory into the `AstArena` row contract instead of
  leaving the typed arena as a standalone fixture.
- Added `CodegenAstTextTypedArenaProjectionReady(...)` and made
  `CodegenTypedAstBridgeReady(...)` consume it. The bridge now checks node
  count, per-row kind facts, atom lookup facts, and a root child edge through
  typed arena accessors before codegen emission can proceed.
- Tightened `self_hosted_component_contract_smoke.sh` so the bridge readiness
  path cannot regress to fixture-only proof.
- The mixed AST-like tree blocker remains ACTIVE: emission still consumes the
  transitional AST-text node inventory. The next cutover is repointing specific
  emission consumers from `CodegenAstTextNode` payload/provenance to `NodeId`
  arena facts under oracle parity.
- Verified with `self-host-component-contract-test-smoke`,
  `self-host-compiler-world-contract-test-smoke`, and
  `self-host-codegen-bootstrap-test-smoke` (`SELF-HOSTING OK`, `gen2 == gen3` at
  6906 generated-C lines).

### 2026-07-07 -- Typed AST arena gains parent and indent facts

- Split the AST-text-to-arena projection out of
  `ast_text_inventory_owner.pgy` into
  `ast_text_typed_arena_owner.pgy`, keeping the line inventory owner below the
  600-line cap and naming the projection responsibility directly.
- Extended `AstArena` with parent and indent rows plus
  `TypedAstArenaParentId(...)` and `TypedAstArenaIndent(...)` accessors. The
  fixture now checks Program -> FuncDecl(Main) -> Block parent/indent facts.
- Updated the real bridge projection so every `CodegenAstTextNode` row maps to
  row-aligned kind, atom, parent, indent, and child-edge facts before
  `GenerateC` can emit.
- The mixed AST-like tree blocker remains ACTIVE: emission still consumes
  transitional `CodegenAstTextNode` rows. The next cutover can now move
  traversal decisions from raw `indent` reads to `NodeId` parent/indent facts.

### 2026-07-07 -- Program emission consumes typed arena traversal facts

- Added `CodegenAstArenaIndentOrDie(...)`,
  `CodegenAstArenaParentOrDie(...)`, and
  `CodegenAstArenaIsDescendantOf(...)` to the typed AST projection owner.
- Repointed `program_emit.pgy` top-level emission traversal so the first
  function indent, zero-artifact declaration skipping, nominal method body
  scanning, and role method body scanning consume typed arena indent/descendant
  facts instead of direct raw `nodes[i].indent` comparisons.
- Tightened `self_hosted_component_contract_smoke.sh` so the old
  `nodes[first_fn].indent` and owner-body `nodes[cur].indent > ...` paths cannot
  return in `program_emit`.
- The mixed AST-like tree blocker remains ACTIVE: function and statement
  emission still consume transitional text-node rows. This slice moves one
  program-level structural traversal seam behind the typed arena owner.

### 2026-07-07 -- Function and statement emission consume typed arena depth facts

- Repointed `function_emit.pgy` declaration collectors and signature emission so
  parameter rows, owner-depth checks, role operator scans, struct field scans,
  and prototype scans consume `CodegenAstArenaIndentOrDie(...)` or
  `CodegenAstArenaIsDescendantOf(...)` from
  `ast_text_typed_arena_owner.pgy` instead of direct
  `CodegenAstTextNode.indent` reads.
- Repointed `stmt_emit.pgy` statement-list depth checks, else-if nested depth,
  and match-case depth scans to the same typed arena projection owner.
- Tightened `self_hosted_component_contract_smoke.sh` so direct
  `nodes[i].indent`, `nodes[j].indent`, `nodes[idx].indent`, and
  `nodes[cur[0]].indent` reads cannot return in the function/statement emitters.
- The mixed AST-like tree blocker remains ACTIVE because statement payloads and
  declaration payloads still come from transitional AST-text rows. This slice
  closes the emission-depth traversal SoT seam, not the full text bridge.
- Verified with `self-host-component-contract-test-smoke`,
  `self-host-compiler-world-contract-test-smoke`, and
  `self-host-codegen-bootstrap-test-smoke` (`SELF-HOSTING OK`, `gen2 == gen3` at
  7025 generated-C lines).

### 2026-07-07 -- Program-owned typed arena projection flows through emitters

- Kept `GenerateCUnit(...)` as the single builder of the AST-text-to-typed-arena
  projection and threaded the resulting `AstArena` fact through
  `BuildFunctionEnv`, `CollectStructs`, `CollectRoleOperators`, `CollectProtos`,
  `EmitFunction`, and recursive `EmitStmtList` calls.
- Tightened `self_hosted_component_contract_smoke.sh` so function/statement
  emitters must accept `arena: AstArena` and must not rebuild
  `CodegenAstTextTypedArenaFromNodes(nodes, count)` locally.
- This is a performance and SoT hygiene slice: the typed arena projection now
  has one program-level owner in the codegen path, while payload semantics still
  remain inside the transitional AST-text bridge.
- Verified with `self-host-component-contract-test-smoke`,
  `self-host-compiler-world-contract-test-smoke`, and
  `self-host-codegen-bootstrap-test-smoke` (`SELF-HOSTING OK`, `gen2 == gen3` at
  7019 generated-C lines).

### 2026-07-07 -- Typed arena owns name, type, and mode rows

- Extended `AstArena` with parallel `type_names`, `has_type_names`, and `modes`
  rows while keeping identifiers and payload atoms in the existing atom table.
- Added typed arena accessors for atom, type-name, mode, and parameter type
  lookup, including the existing `self` parameter owner fallback for methods.
- Repointed function/declaration emission so function names, return types,
  parameter names/types/modes, nominal/role/enum names, role target types, field
  names/types, and prototype signature rows consume typed arena facts instead of
  `CodegenAstTextNode.name`, `type_name`, or `mode`.
- Tightened `self_hosted_component_contract_smoke.sh` so the targeted
  `CodegenAstText*Name/Type/Mode` accessors cannot return in
  `function_emit.pgy`.
- The mixed AST-like tree blocker remains ACTIVE because expression and
  statement payload strings still flow through the transitional AST-text bridge.
- Verified with `self-host-component-contract-test-smoke`,
  `self-host-compiler-world-contract-test-smoke`, and
  `self-host-codegen-bootstrap-test-smoke` (`SELF-HOSTING OK`, `gen2 == gen3` at
  7127 generated-C lines).

### 2026-07-07 -- Statement single-payload reads consume typed arena atoms

- Repointed statement emission for `Log`, value `Return`, `ArrayPop`, `Exit`,
  `While`, `If`, `Match`, match cases, and bare-call statements so the payload
  string is consumed from `CodegenAstArenaAtomOrDie(...)`.
- Tightened `self_hosted_component_contract_smoke.sh` so those statement
  payloads cannot return to `CodegenAstText*` payload accessors inside
  `stmt_emit.pgy`.
- Compound statement payloads (`Let`, `Assign`, `ArraySet`, `ArrayPush`, and
  `For`) intentionally remain in `ast_text_statement_owner.pgy` until they have
  row-shaped typed arena facts instead of several ad hoc payload strings.
- The mixed AST-like tree blocker remains ACTIVE, but the single-atom statement
  payload seam is no longer owned by statement emission.
- Verified with `self-host-component-contract-test-smoke`,
  `self-host-compiler-world-contract-test-smoke`, and
  `self-host-codegen-bootstrap-test-smoke` (`SELF-HOSTING OK`, `gen2 == gen3` at
  7127 generated-C lines).

### 2026-07-07 -- Let name/type emission consumes typed arena rows

- Repointed `EmitLet(...)` and `EmitTryLet(...)` so the local name and declared
  type consume `CodegenAstArenaAtomOrDie(...)` and
  `CodegenAstArenaTypeNameOrDie(...)`.
- Left `CodegenAstTextLetInitializer(...)` in the statement owner deliberately:
  initializer text is still a compound expression payload and needs a separate
  typed expression row before it can leave the bridge.
- Tightened `self_hosted_component_contract_smoke.sh` so `stmt_emit.pgy` cannot
  return to `CodegenAstTextLetName(node)` or
  `CodegenAstTextLetTypeName(node)`.
- Verified with `self-host-component-contract-test-smoke`,
  `self-host-compiler-world-contract-test-smoke`, and
  `self-host-codegen-bootstrap-test-smoke` (`SELF-HOSTING OK`, `gen2 == gen3` at
  7127 generated-C lines).

### 2026-07-07 -- Typed arena owns Let and Assign value rows

- Extended `CodegenAstTextRowFactInput` projection with value facts, then
  threaded those facts through `CodegenAstTextNode` and `AstArena` as
  `value`/`has_value` rows backed by the shared atom table.
- Added `TypedAstArenaValueText(...)` and `CodegenAstArenaValueOrDie(...)`.
- Repointed `EmitLet(...)`, `EmitTryLet(...)`, and `EmitAssign(...)` so `Let`
  initializer plus `Assign` target/RHS consume typed arena rows instead of
  `CodegenAstTextLetInitializer(...)` or `CodegenAstTextAssign*` accessors.
- Left `ArraySet`, `ArrayPush`, and `For` in the statement owner because their
  payloads need multi-field typed rows, not a single value row.
- Verified with `self-host-component-contract-test-smoke`,
  `self-host-compiler-world-contract-test-smoke`, and
  `self-host-codegen-bootstrap-test-smoke` (`SELF-HOSTING OK`, `gen2 == gen3` at
  7211 generated-C lines).

### 2026-07-07 -- ArrayPush consumes typed arena target/value rows

- Extended `CodegenAstTextNameFactFor(...)` and
  `CodegenAstTextValueFactFor(...)` for `ArrayPush(receiver, value)`.
- Repointed `stmt_emit.pgy` so `ArrayPush` statement emission consumes
  `CodegenAstArenaAtomOrDie(...)` for the receiver and
  `CodegenAstArenaValueOrDie(...)` for the pushed expression.
- Tightened `self_hosted_component_contract_smoke.sh` so
  `CodegenAstTextArrayPushTarget(...)` and
  `CodegenAstTextArrayPushValue(...)` cannot return in `stmt_emit.pgy`.
- `ArraySet` and `For` remain in the statement owner because they need more than
  the existing atom/value row pair.
- Verified with `self-host-component-contract-test-smoke`,
  `self-host-compiler-world-contract-test-smoke`, and
  `self-host-codegen-bootstrap-test-smoke` (`SELF-HOSTING OK`, `gen2 == gen3` at
  7226 generated-C lines, including the `array_push` fixture).

### 2026-07-07 -- ArraySet consumes typed arena target/index/value rows

- Extended `CodegenAstTextRowFactInput` projection with an auxiliary value row
  for `ArraySet(receiver, index, value)`, then threaded that row through
  `CodegenAstTextNode` and `AstArena` as `aux_value` / `has_aux_value`.
- Added `TypedAstArenaAuxValueText(...)` and
  `CodegenAstArenaAuxValueOrDie(...)`.
- Repointed `stmt_emit.pgy` so `ArraySet` statement emission consumes
  `CodegenAstArenaAtomOrDie(...)` for the receiver,
  `CodegenAstArenaValueOrDie(...)` for the index, and
  `CodegenAstArenaAuxValueOrDie(...)` for the assigned value.
- Tightened `self_hosted_component_contract_smoke.sh` so
  `CodegenAstTextArraySetTarget(...)`, `CodegenAstTextArraySetIndex(...)`, and
  `CodegenAstTextArraySetValue(...)` cannot return in `stmt_emit.pgy`.
- `For` remains in the statement owner because it still needs multi-field typed
  rows.
- Verified with `self-host-component-contract-test-smoke`,
  `self-host-compiler-world-contract-test-smoke`, and
  `self-host-codegen-bootstrap-test-smoke` (`SELF-HOSTING OK`, `gen2 == gen3` at
  7319 generated-C lines).

### 2026-07-07 -- For consumes typed arena loop payload rows

- Extended `CodegenAstTextNameFactFor(...)`,
  `CodegenAstTextValueFactFor(...)`, and
  `CodegenAstTextAuxValueFactFor(...)` for `For: loop in source` rows.
- Repointed `stmt_emit.pgy` so range loops consume the loop variable from the
  arena atom row, range start from the value row, and range end from the
  aux-value row. Foreach loops consume the loop variable from the atom row and
  the collection expression from the value row.
- Tightened `self_hosted_component_contract_smoke.sh` so
  `CodegenAstTextForLoopVar(...)`, `CodegenAstTextForIsRange(...)`,
  `CodegenAstTextForRangeStart(...)`, `CodegenAstTextForRangeEnd(...)`, and
  `CodegenAstTextForEachCollection(...)` cannot return in `stmt_emit.pgy`.
- Statement emission payloads now route through typed arena rows; the mixed
  AST-like tree blocker remains ACTIVE because `CodegenAstTextNode.text` is
  still the bridge provenance/payload inside the input owner.
- Verified with `self-host-component-contract-test-smoke`,
  `self-host-compiler-world-contract-test-smoke`, and
  `self-host-codegen-bootstrap-test-smoke` (`SELF-HOSTING OK`, `gen2 == gen3` at
  7353 generated-C lines).

### 2026-07-07 -- Enum variant lists consume typed arena aux rows

- Extended `CodegenAstTextRowFactInput` with `aux_payload` so enum variant
  lists are captured once during AST-text inventory construction and projected
  into `AstArena` aux-value rows.
- Added `CodegenAstArenaEnumVariantCount(...)` and
  `CodegenAstArenaEnumVariantNameAt(...)`.
- Repointed `CollectEnums(...)` so payload-free enum variant emission consumes
  typed arena facts instead of `CodegenAstTextEnumVariant*` accessors.
- Tightened `self_hosted_component_contract_smoke.sh` so
  `CodegenAstTextEnumVariantCount(nodes[i])` and
  `CodegenAstTextEnumVariantNameAt(nodes[i], value)` cannot return in
  `function_emit.pgy`.
- Payload-bearing enum variants remain a fail-closed frontier.
- Verified with `self-host-component-contract-test-smoke`,
  `self-host-compiler-world-contract-test-smoke`, and
  `self-host-codegen-bootstrap-test-smoke` (`SELF-HOSTING OK`, `gen2 == gen3` at
  7421 generated-C lines).

### 2026-07-07 -- Program routing consumes typed arena kind predicates

- Added typed arena declaration predicates for function, Main function, event,
  zero-artifact, nominal, and role nodes.
- Repointed `program_emit.pgy` so declaration routing, Main counting, event
  rejection, and top-level function selection consume `AstArena` kind/atom facts
  instead of `CodegenAstTextIs*` predicates over transitional text nodes.
- Tightened `self_hosted_component_contract_smoke.sh` so those old declaration
  predicates cannot return in `program_emit.pgy`.
- Verified with `self-host-component-contract-test-smoke`,
  `self-host-compiler-world-contract-test-smoke`, and
  `self-host-codegen-bootstrap-test-smoke` (`SELF-HOSTING OK`, `gen2 == gen3` at
  7474 generated-C lines).

### 2026-07-07 -- Declaration collectors consume typed arena kind predicates

- Added a typed arena enum declaration predicate so all declaration collectors
  can route through the same arena kind/atom owner.
- Repointed `BuildFunctionEnv(...)`, `CollectRoleOperators(...)`,
  `CollectStructs(...)`, `CollectEnums(...)`, and `CollectProtos(...)` so
  function/role/nominal/enum/zero-artifact routing consumes `AstArena` facts
  instead of `CodegenAstTextIs*` predicates over transitional text nodes.
- Tightened `self_hosted_component_contract_smoke.sh` so those old declaration
  predicates cannot return in `function_emit.pgy`.
- Verified with `self-host-component-contract-test-smoke`,
  `self-host-compiler-world-contract-test-smoke`, and
  `self-host-codegen-bootstrap-test-smoke` (`SELF-HOSTING OK`, `gen2 == gen3` at
  7480 generated-C lines).

### 2026-07-07 -- Function markers consume typed arena kind predicates

- Added typed arena predicates for Program root, Parameters, Returns, and Fields
  marker nodes.
- Repointed `function_emit.pgy` so signature/prototype/struct prepass marker
  routing consumes `AstArena` kind facts instead of `CodegenAstTextIs*` marker
  predicates over transitional text nodes.
- Repointed `CodegenTypedAstBridgeReady(...)` so Program-root validation also
  consumes the typed arena kind fact.
- Tightened `self_hosted_component_contract_smoke.sh` so those old marker
  predicates cannot return at those consumption points.
- Verified with `self-host-component-contract-test-smoke`,
  `self-host-compiler-world-contract-test-smoke`, and
  `self-host-codegen-bootstrap-test-smoke` (`SELF-HOSTING OK`, `gen2 == gen3` at
  7505 generated-C lines).

### 2026-07-07 -- Statement dispatch consumes typed arena kind predicates

- Added typed arena statement predicates for supported self-host codegen
  statements, plus else-if and match-case/default routing helpers.
- Repointed `stmt_emit.pgy` so statement dispatch consumes `AstArena` kind facts
  instead of `CodegenAstTextIs*Stmt` predicates over transitional text nodes.
- Tightened `self_hosted_component_contract_smoke.sh` so the old statement-kind
  predicates cannot return in `stmt_emit.pgy`.
- Verified with `self-host-component-contract-test-smoke`,
  `self-host-compiler-world-contract-test-smoke`, and
  `self-host-codegen-bootstrap-test-smoke` (`SELF-HOSTING OK`, `gen2 == gen3` at
  7637 generated-C lines).

### 2026-07-07 -- Emission marker expectations consume typed arena kind facts

- Added typed arena expectation helpers for Parameters, Body, Block, and Then
  structural markers.
- Repointed `function_emit.pgy` and `stmt_emit.pgy` so emission participants no
  longer call `CodegenAstTextExpectNode(...)`.
- Tightened `self_hosted_component_contract_smoke.sh` so the old expectation
  call cannot return in emission participants.
- Verified with `self-host-component-contract-test-smoke`,
  `self-host-compiler-world-contract-test-smoke`, and
  `self-host-codegen-bootstrap-test-smoke` (`SELF-HOSTING OK`, `gen2 == gen3` at
  7701 generated-C lines).

### 2026-07-07 -- Runtime usage facts consume typed arena rows

- Added `CodegenRuntimeUsageFactsFromArena(...)` so runtime/header usage facts
  consume the already-built `AstArena` atom/type/value/aux-value/kind rows.
- Repointed `program_emit.pgy` away from the raw-node usage bridge, then deleted
  that compatibility entrypoint so the arena path is the only usage fact owner.
- Tightened `self_hosted_component_contract_smoke.sh` so production emission
  cannot call `CodegenRuntimeUsageFactsFromNodes(nodes, count)`, and usage facts
  cannot reintroduce direct `nodes[i].payload` / `nodes[i].aux_payload` scans or
  the deleted `CodegenRuntimeUsageFactsFromNodes(...)` alias.
- Builtin-name usage detection remains string-based over arena rows until a
  dedicated expression usage row owner exists.
- Verified with `self-host-component-contract-test-smoke`,
  `self-host-compiler-world-contract-test-smoke`, and
  `self-host-codegen-bootstrap-test-smoke` (`SELF-HOSTING OK`, `gen2 == gen3` at
  7710 generated-C lines).

### 2026-07-07 -- Retire dead AST-text statement owner

- Deleted `src/self_hosted/codegen/input/ast_text_statement_owner.pgy` after
  statement dispatch and payload reads moved to typed arena row facts.
- Removed the dead imports from `codegen/main.pgy`, `program_emit.pgy`,
  `function_emit.pgy`, and `stmt_emit.pgy`.
- Tightened `self_hosted_component_contract_smoke.sh` so the retired file and
  its imports cannot return.
- Updated the self-host architecture, owner registry, and expansion ledger so
  statement facts are attributed to `ast_text_row_fact_owner.pgy` plus the typed
  arena projection instead of the deleted alias owner.
- Verified with `self-host-component-contract-test-smoke`,
  `self-host-compiler-world-contract-test-smoke`, and
  `self-host-codegen-bootstrap-test-smoke` (`SELF-HOSTING OK`, `gen2 == gen3` at
  7247 generated-C lines).

### 2026-07-07 -- Runtime usage facts split typed arena lanes

- Replaced the generic `CodegenAstArenaContains(...)` runtime/header usage scan
  with lane-specific predicates in `ast_usage_owner.pgy`.
- Type/header requirements now read only typed arena `type_name` rows; builtin
  call requirements scan only expression-bearing atom/value/aux rows through
  `ContainsCallOutsideStrings(...)`; `None` uses a token-boundary expression
  scan; statement-only needs continue to use arena kind facts.
- Tightened `self_hosted_component_contract_smoke.sh` so the old whole-arena
  usage predicate cannot return and the new type/call/token usage owners must
  exist.
- This narrows the transitional AST-text bridge but does not close the mixed
  AST-like tree blocker: expression payloads are still strings until dedicated
  expression rows replace them.

### 2026-07-07 -- Let try detection moves behind typed arena owner

- Added `CodegenAstArenaLetInitializerHasTry(...)` so the typed arena/input
  owner decides whether a `Let` initializer contains the supported `(?...)`
  try surface.
- Repointed `stmt_emit.pgy` to consume that predicate instead of directly
  calling `ContainsOutsideStrings(CodegenAstArenaValueOrDie(...), "(?")`.
- Tightened the component contract so the local emission-side try scan cannot
  return.
- This is still a transitional string-backed expression fact, not final typed
  expression ownership.

### 2026-07-07 -- Let try inner extraction moves behind typed arena owner

- Added `CodegenAstArenaLetTryInner(...) -> Option<String>` so the typed
  arena/input owner owns the supported `(?...)` shape check and inner-expression
  extraction for `Let` initializers.
- Removed `TryExprInner(...)` from `stmt_emit.pgy`; `EmitTryLet(...)` now
  consumes the owner fact and keeps only return-shape/code-emission decisions.
- Tightened the component contract so emission cannot reintroduce local try
  expression parsing.
- This still does not claim full expression-row ownership; it removes one more
  emission-local parser from the transitional bridge.

### 2026-07-07 -- Indexed assignment target parsing moves behind typed arena owner

- Added typed arena assignment target facts:
  `CodegenAstArenaAssignTargetIsIndex(...)`,
  `CodegenAstArenaAssignIndexReceiverOrDie(...)`, and
  `CodegenAstArenaAssignIndexExprOrDie(...)`.
- Repointed `EmitAssign(...)` so indexed assignment lowering consumes those
  target facts instead of parsing `name` locally with `StringIndexOf(name, "[")`.
- Tightened the component contract so assignment target parsing cannot return
  to the emission participant.
- This keeps array-index assignment shape under the input owner while the
  expression payload itself remains string-backed.

### 2026-07-07 -- Array literal element facts move behind input owner

- Added `ast_text_array_literal_owner.pgy` for `Let` array initializer shape
  and element access:
  `CodegenAstArenaLetInitializerStartsArrayLiteral(...)`,
  `CodegenAstArenaLetArrayLiteralElementCount(...)`, and
  `CodegenAstArenaLetArrayLiteralElementAt(...)`.
- Repointed `EmitLet(...)` so growable-array initialization consumes those
  facts instead of locally validating `[`/`]` and splitting elements with
  `FindTopLevelComma(rem)`.
- Tightened the component contract so array literal parsing cannot return to
  `stmt_emit.pgy`.
- This keeps array literal row-shape ownership in the input layer;
  element expressions are still string-backed until dedicated expression rows
  replace the transitional bridge.

### 2026-07-07 -- Runtime collection symbols consume kind-code facts

- Repointed `CollectionRuntimeCNewFn`, `CollectionRuntimeCPushFn`,
  `CollectionRuntimeCSetFn`, `CollectionRuntimeCGetFn`,
  `CollectionRuntimeCLenFn`, and `CollectionRuntimeCPopFn` to consume
  `kind_code: Int` instead of `kind: String`.
- Emitters now normalize collection type/kind spelling at the owner boundary
  and pass `CollectionRuntimeKindCode(...)` into helper-symbol selection.
- Tightened `self_host_pergyra_likeness_smoke`: core text-to-text signatures
  ratchet from 115 to 111 and result/option evidence ratchets from 591 to 678.
- Tightened the sentinel metric from 8 to 3 after excluding generated C runtime
  text in `program_emit.pgy`; the metric now counts self-host control-flow
  sentinels rather than C ABI sentinel strings embedded in emitted code.

### 2026-07-07 -- Self-host semantic constructor fields accept bare rows

- Fixed `NominalConstructorParams(...)` so self-host semantic constructor
  seeding accepts both `let field: Type;` and bare `field: Type;` nominal rows.
- This closes the `AstArena(...)` selfcheck failure where imported typed arena
  structs were seeded as 0-arity constructors despite having field rows.
- Tightened the component contract so the old mandatory-`let` nominal field
  scan cannot return.

### 2026-07-07 -- Expression sequence facts move behind text owner

- Added `text/expr_sequence_owner.pgy` for top-level comma-separated expression
  sequence count/index facts.
- Repointed `RewriteCallArgsWithModes(...)`,
  `RewriteStructLiteralCallArg(...)`, `EmitStructLit(...)`, and the
  array-literal input owner to consume `ExprSequenceItemCount(...)` /
  `ExprSequenceItemAt(...)` instead of each reimplementing top-level comma
  splitting.
- Tightened the component contract so `expr_rewrite.pgy` and
  `struct_value_emit.pgy` cannot reintroduce local `FindTopLevelComma(rem)`
  loops.
- Tightened `self_host_pergyra_likeness_smoke` result/option evidence from
  678 to 681 because sequence items now flow through `Option<String>` owner
  accessors.
- This reduces the mixed AST-like tree blocker by moving another expression
  payload shape behind a named owner; it does not close the blocker because the
  payload values themselves remain transitional expression text.

### 2026-07-07 -- Struct literal field-entry facts move behind text owner

- Added `text/struct_literal_field_owner.pgy` for struct literal field-name and
  field-value entry facts.
- Repointed `RewriteStructLiteralCallArg(...)` and `EmitStructLit(...)` so
  explicit `field: value` entries and positional field fallback consume the new
  owner-owned typed field-entry row rather than parsing
  `StringIndexOf(part, ": ")` and `CsvAt(field_names, field_pos)` locally in
  emission participants.
- Tightened the component contract so expression and struct-value emission
  cannot reintroduce local struct field-entry parsing.
- Tightened `self_host_pergyra_likeness_smoke` result/option evidence from
  681 to 686 because struct field entries now flow through owner-returned facts.
- This keeps another text-backed expression shape behind a named owner while
  preserving fail-closed type routing in the emission participants.

### 2026-07-07 -- Struct literal call-envelope facts move behind text owner

- Added `text/struct_literal_call_owner.pgy` for struct literal `Name(...)`
  recognition and the typed `StructLiteralCallFact` type-name/inner-payload
  row.
- Repointed `RewriteStructLiteralCallArg(...)`, `EmitStructValue(...)`, and
  `EmitStructLit(...)` so emission participants consume the owner envelope
  facts before routing field values.
- Folded the field-entry name/value accessors into a typed
  `StructLiteralFieldEntryFact` row so this slice does not add string-to-string
  compiler-core surface.
- Tightened the component contract so struct emission cannot reintroduce local
  `StartsWith(e, Concat(..., "("))`, `StringIndexOf(e, "(")`,
  `FindMatchingParen(e, op)`, or end-paren scans for this path.
- This reduces the mixed AST-like tree blocker by moving another text-backed
  expression envelope behind a named owner; it does not close the blocker
  because expression payloads are still string-backed.

### 2026-07-07 -- Payload-free enum literal projection moves behind text owner

- Added `text/enum_literal_owner.pgy` for payload-free enum literal projection
  facts.
- Repointed call-argument enum literal rewriting and match-case value lowering
  so emission participants consume owner-returned `EnumLiteralProjectionFact`
  rows instead of locally trimming tokens, rebuilding `Enum.Member` keys, or
  re-rendering enum symbols.
- Tightened the component contract so `expr_rewrite.pgy` and `stmt_emit.pgy`
  cannot reintroduce local enum-key lookup for these paths.
- This reduces the mixed AST-like tree blocker by moving another expression
  token projection behind a named owner; it does not close the blocker because
  expression payloads are still string-backed.

### 2026-07-09 -- Runtime usage builtin vocabulary moves behind owner rows

- Added `CodegenUsageBuiltinGroup*` rows to `ast_usage_owner.pgy` so
  runtime/header usage decisions consume owner-owned builtin callee vocabulary
  through `CodegenAstArenaBuiltinGroupPresent(...)`.
- Repointed `CodegenRuntimeUsageFactsFromArena(...)` away from spelling every
  builtin call name inline in the usage fact body. Type/kind facts still come
  from the typed arena, and the expression matching engine remains
  string-backed until dedicated expression usage rows replace transitional
  atom/value/aux text.
- Tightened `self_hosted_component_contract_smoke.sh` to require the builtin
  group row owner functions and the grouped `Args`/string usage consumers.
- Verified with `self-host-component-contract-test-smoke` and
  `self-host-codegen-bootstrap-test-smoke` (`SELF-HOSTING OK`, `gen2 == gen3`
  at 8999 generated-C lines).

### 2026-07-09 -- Expression usage facts split from runtime/header usage

- Added `ast_expression_usage_owner.pgy` as the owner for expression usage facts,
  builtin-callee group rows, and the transitional `None` token scan.
- Repointed `ast_usage_owner.pgy` so `CodegenRuntimeUsageFactsFromArena(...)`
  consumes `CodegenExpressionUsageFacts` plus type/kind facts instead of
  reopening expression payload scans or builtin group matching locally.
- Tightened `self_hosted_component_contract_smoke.sh` so the codegen owner
  surface includes the new expression usage owner and `ast_usage_owner.pgy`
  rejects `ContainsCallOutsideStrings`, direct typed arena expression payload
  reads, and direct builtin group scans.
- This reduces the mixed AST-like tree blocker. It does not close it because the
  expression usage owner still derives facts from transitional arena
  atom/value/aux text until typed expression rows exist.
- Verified with `self-host-component-contract-test-smoke`,
  `build-source-inventory-test-smoke`, `self-host-pergyra-likeness-test-smoke`
  (`result_use` ratcheted to 734), and `self-host-codegen-bootstrap-test-smoke`
  (`SELF-HOSTING OK`, `gen2 == gen3` at 9131 generated-C lines).

### 2026-07-09 -- For statement payload facts leave stmt emission

- Added `ast_text_for_stmt_owner.pgy` for self-host codegen `For` facts:
  loop variable, range-vs-foreach classification, range start/end, and foreach
  collection.
- Repointed `stmt_emit.pgy` so `For` lowering consumes those owner accessors
  instead of reading `TypedAstArenaAuxValueText(arena, idx)` directly.
- Tightened `self_hosted_component_contract_smoke.sh` so the codegen owner
  surface includes the new `For` statement owner and `stmt_emit.pgy` rejects
  reopening direct range-end payload reads.
- This reduces the mixed AST-like tree blocker for statement emission. It does
  not close the blocker because the owner still reads the transitional typed
  arena payload rows until typed expression/statement rows replace the AST-text
  bridge.

### 2026-07-09 -- Single-payload statement facts leave stmt emission

- Added `ast_text_statement_payload_owner.pgy` for self-host codegen statement
  payload facts: `Log`, value `Return`, `ArrayPop`, `Exit`, `While`, `If`,
  `Match`, match case, and bare-call payloads.
- Repointed `stmt_emit.pgy` so those statement forms consume owner accessors
  instead of directly calling `CodegenAstArenaAtomOrDie(arena, idx)` for their
  payloads.
- Tightened `self_hosted_component_contract_smoke.sh` so the codegen owner
  surface includes the new statement payload owner and `stmt_emit.pgy` rejects
  reopening those direct atom reads.
- This reduces the mixed AST-like tree blocker for statement emission. It does
  not close the blocker because the owner still reads transitional typed arena
  payload rows until typed statement rows replace the AST-text bridge.

### 2026-07-09 -- Collection statement payload facts leave stmt emission

- Added `ast_text_collection_stmt_owner.pgy` for self-host codegen `ArraySet`
  and `ArrayPush` statement payload facts.
- Repointed `stmt_emit.pgy` so collection mutation lowering consumes owner
  accessors for target/index/value payloads instead of directly reading arena
  atom/value/aux rows.
- Tightened `self_hosted_component_contract_smoke.sh` so the codegen owner
  surface includes the new collection statement owner and `stmt_emit.pgy`
  rejects reopening those direct arena payload reads.
- This reduces the mixed AST-like tree blocker for statement emission. It does
  not close the blocker because the owner still reads transitional typed arena
  payload rows until typed statement rows replace the AST-text bridge.

### 2026-07-09 -- Local binding and assignment facts leave stmt emission

- Added `ast_text_local_binding_owner.pgy` for self-host codegen `Let` binding
  name/type/initializer facts.
- Added `ast_text_assignment_owner.pgy` for self-host codegen `Assign`
  target/RHS facts.
- Repointed `stmt_emit.pgy` so `EmitLet`, `EmitTryLet`, and `EmitAssign`
  consume those owner accessors instead of directly reading arena
  atom/type/value rows.
- Tightened `self_hosted_component_contract_smoke.sh` so the codegen owner
  surface includes the new owners and `stmt_emit.pgy` rejects reopening those
  direct arena payload reads.
- This reduces the mixed AST-like tree blocker for statement emission. It does
  not close the blocker because the owners still read transitional typed arena
  payload rows until typed statement rows replace the AST-text bridge.

### 2026-07-09 -- Function signature facts leave function emission

- Added `ast_text_function_signature_owner.pgy` for self-host codegen function
  name, parameter mode/name/type, and return type facts.
- Repointed `function_emit.pgy` so `EmitFunction`, `BuildFunctionEnv`,
  `CollectRoleOperators`, and `CollectProtos` consume signature owner
  accessors instead of directly reading arena atom/mode/type rows for function
  signatures.
- Tightened `self_hosted_component_contract_smoke.sh` so the codegen owner
  surface includes the new function signature owner and `function_emit.pgy`
  rejects reopening those direct signature payload reads.
- This reduces the mixed AST-like tree blocker for function emission. It does
  not close the blocker because the owner still reads transitional typed arena
  payload rows until typed declaration rows replace the AST-text bridge.

### 2026-07-09 -- Declaration facts leave function/program emission

- Added `ast_text_declaration_owner.pgy` for self-host codegen declaration
  facts: nominal names, role names/target types, enum names, and struct field
  name/type rows.
- Repointed `function_emit.pgy` and `program_emit.pgy` so declaration scanning,
  method owner tracking, struct collection, enum collection, role-operator
  lookup, and prototype owner tracking consume declaration owner accessors
  instead of directly reading arena atom/type rows.
- Tightened `self_hosted_component_contract_smoke.sh` so the codegen owner
  surface includes the new declaration owner and the emitters reject reopening
  those direct declaration payload reads.
- This reduces the mixed AST-like tree blocker for declaration emission. It
  does not close the blocker because the owner still reads transitional typed
  arena payload rows until typed declaration rows replace the AST-text bridge.

### 2026-07-09 -- Compatibility corpus report count consumes owner fact

- Repointed `compatibility_evolution_checker/report_owner.pgy` so its readiness
  self-check consumes `CompilerCompatibilityChangeCount()` for complete-row
  counts instead of restating the current seed corpus size as a local literal.
- Tightened `self_hosted_component_contract_smoke.sh` so the report owner must
  bind `complete_count` from `CompatibilityEvolutionZone` and rejects reopening
  the old repeated `9` complete-count call.
- This keeps the compatibility corpus consumer aligned with the production-bar
  rule: the compatibility owner owns cardinality; report emission only consumes
  that fact.

### 2026-07-09 -- Expression usage scans consume expression-part fact

- Added `CodegenExpressionParts` inside `ast_expression_usage_owner.pgy` as the
  single transitional fact row for expression atom/value/aux lanes, represented
  with explicit presence bits plus text to stay within the current self-host C
  ABI subset.
- Repointed builtin-call and `None` token scans so they consume
  `CodegenExpressionParts` instead of reopening `TypedAstArena*Text(arena, i)`
  in each scanner.
- Tightened `self_hosted_component_contract_smoke.sh` so expression usage scans
  must go through `CodegenAstArenaExpressionPartsAt(...)` and reject the old
  direct lane-read scan shape.
- This narrows the remaining mixed AST-like tree blocker to one bridge seam.
  The blocker remains active until typed/tagged expression rows replace the
  transitional arena text behind `CodegenExpressionParts`.

### 2026-07-09 -- Try-let initializer checks consume one owner fact

- Added `CodegenLetTryInitializerFact` inside `ast_text_try_let_owner.pgy` as
  the single transitional fact row for `Let` initializer text used by try-let
  lowering.
- Repointed `CodegenAstArenaLetInitializerHasTry(...)` and
  `CodegenAstArenaLetTryInner(...)` so both consume an `Option<String>` view of
  the fact instead of each reopening `TypedAstArenaValueText(arena, node_id)`.
- Tightened `self_hosted_component_contract_smoke.sh` so the try-let owner must
  expose the fact seam and may read the initializer payload only once.
- This reduces the mixed AST-like tree blocker inside the input owner surface.
  The blocker remains active until typed statement payload rows replace the
  transitional arena value row behind the fact.

### 2026-07-09 -- For range-end checks consume one owner fact

- Added `CodegenForRangeEndFact` inside `ast_text_for_stmt_owner.pgy` as the
  single transitional fact row for the optional `For` range-end payload.
- Repointed `CodegenAstArenaForIsRange(...)` and
  `CodegenAstArenaForRangeEndOrDie(...)` so both consume an `Option<String>`
  view of that fact instead of each reopening
  `TypedAstArenaAuxValueText(arena, node_id)`.
- Tightened `self_hosted_component_contract_smoke.sh` so the `For` owner must
  expose the fact seam and may read the auxiliary payload only once.
- This reduces the mixed AST-like tree blocker inside the input owner surface.
  The blocker remains active until typed statement payload rows replace the
  transitional arena auxiliary-value row behind the fact.

### 2026-07-09 -- Array literal and enum variant payloads consume owner facts

- Added `CodegenLetArrayLiteralFact` inside
  `ast_text_array_literal_owner.pgy` so array-literal starts/body checks consume
  one initializer fact instead of reopening the generic arena value accessor.
- Added `CodegenEnumVariantPayloadFact` inside
  `ast_text_enum_variant_owner.pgy` so payload-free enum variant count/name
  accessors consume an `Option<String>` view instead of carrying missing
  payload as an owner-local empty-string fact.
- Tightened `self_hosted_component_contract_smoke.sh` so both owners must
  expose the fact seams and may read their transitional payload rows only once.
- This reduces the mixed AST-like tree blocker inside the input owner surface.
  The blocker remains active until typed statement/declaration payload rows
  replace the transitional arena value and auxiliary-value rows behind the
  facts.

### 2026-07-09 -- Backend-emitter contract absorbs C slot fail-closed rows

- Expanded `backend_emitter_contract_owner.pgy` from 5 to 12 required rows so
  the self-host checker now covers the current C slot/resource fail-closed
  runtime-row callsites from the broad native backend gate.
- Updated the backend-emitter expected clean and negative artifacts so their
  row counts come from the owner contract rather than a stale shell-only count.
- Tightened `self_hosted_component_contract_smoke.sh` so the expanded row count
  and representative C slot/resource runtime-row diagnostics remain load-bearing.
- This moves more of the MIR-owned runtime-call truth contract into hard
  self-host parity. It still does not close the full production ABI blocker
  because native C/LLVM backends must eventually consume the same concrete
  runtime-call ABI row table instead of only proving selected source terms.

### 2026-07-09 -- Runtime-call ABI rows include native resource table

- Expanded `runtime_call_abi_row_owner.pgy` from 87 to 237 concrete rows.
- The runnable `runtime_call_abi` artifact now projects the native
  Slot/SecureSlot/DeviceSlot resource runtime-call table as `native-resource`
  rows with `mir_abi_resource_row` materialization, covering claim/read/write/
  release, pin/unpin, and device submit-read spellings.
- Updated the committed expected artifact and component contract so representative
  native resource rows are ratcheted by the self-host gate.
- This connects the backend-emitter source-term contract to a self-hosted
  runtime-call ABI row artifact. The production ABI blocker remains open until
  native C/LLVM emitters consume this concrete table rather than proving only
  selected source terms.

### 2026-07-09 -- Native resource runtime rows become executable facts

- Added `mir_abi_resource_runtime_row_count`,
  `mir_abi_resource_runtime_row_type_name`,
  `mir_abi_resource_runtime_row_operation`, and
  `mir_abi_resource_runtime_row_symbol` so the native MIR resource runtime-call
  table can be inspected as rows instead of only queried by lookup helpers.
- Added `MIR ABI resource runtime row table exposes native resource rows` to
  `test_mir`, covering row count, slot, secure slot, pin/unpin, device slot,
  submit-read, and out-of-range fail-closed access.
- Added `mir_abi_resource_runtime.o` to `MIR_CORE_OBJECTS` so MIR tests link
  the native runtime-call table owner directly.
- Tightened `abi_ownership_shape_smoke.sh` so the native row-access API and
  executable row-table test remain load-bearing.
- This gives the self-host `runtime_call_abi` projection a native executable
  row surface to compare against in later parity work. Full closure still
  requires C/LLVM backend emitters to consume concrete row records, not merely
  source-term lookup helpers.

### 2026-07-09 -- Native runtime-call artifact rows compare against MIR rows

- Added `MIR ABI native resource rows match self-host runtime-call artifact`
  to `test_mir`.
- The test opens the committed self-host
  `src/self_hosted/compiler/expected/runtime_call_abi_rows.txt` artifact and
  compares every `native-resource` row against the native
  `mir_abi_resource_runtime_row_*` accessors.
- Tightened `abi_ownership_shape_smoke.sh` so this artifact-to-native-row
  comparison remains load-bearing.
- This closes the first executable drift gate between the self-host
  runtime-call ABI artifact and the native MIR resource runtime-call row table.
  The production ABI blocker remains open until C and LLVM emitters consume the
  concrete rows directly.

### 2026-07-09 -- Native runtime-call rows expose artifact shape facts

- Extended the native `mir_abi_resource_runtime_row_*` API with row domain,
  target kind, and materialization accessors.
- Updated the artifact comparison test so `native-resource`, `function`, and
  `mir_abi_resource_row` come from the MIR row owner instead of test-local
  constants.
- Tightened `abi_ownership_shape_smoke.sh` against dropping those row-shape
  facts from the native ABI owner.

### 2026-07-09 -- Runtime-call ABI rows carry call-shape facts

- Added `call_shape` as the sixth `runtime_call_abi` row fact and bumped the
  self-host row artifact schema to `pgy.selfhost.runtime-call-abi-row.v2`.
- Added native `mir_abi_resource_runtime_row_call_shape(...)` so Slot,
  SecureSlot, DeviceSlot, pin/unpin, and submit-read call signatures are exposed
  by the MIR ABI row owner instead of being inferred by backend tests.
- Updated the self-host artifact and native comparison so every
  `native-resource` row must match the MIR-owned call shape, not just domain,
  operation, symbol, target kind, and materialization.
- This closes one more runtime-call ABI SoT seam. The production ABI blocker
  remains open until C/LLVM emitters consume the concrete row records directly
  for call emission.

### 2026-07-09 -- Runtime-call ABI row records enter backend consumers

- Added the public `MIRResourceRuntimeRow` view plus
  `mir_abi_resource_runtime_row_at`, `mir_abi_resource_runtime_row_by_type_name`,
  and `mir_abi_resource_runtime_row_by_kind` so backend code can consume a
  row record instead of asking only for a runtime symbol string.
- Refactored the native resource runtime table so domain, target kind,
  materialization, and call shape are stored on the table row itself.
- Repointed representative C and LLVM consumers:
  `transpiler_slot_builtin_emit.c` now resolves source slot builtins through
  row records, and `llvm_expr_identifier_slot_helpers.c` validates the MIR-owned
  auto-read call shape before emitting the runtime call.
- Tightened `abi_ownership_shape_smoke.sh` and `backend_fail_closed_smoke.sh`
  so these representative consumers cannot regress to symbol-only lookup.
- This does not finish the production ABI blocker; the remaining callsites
  still need to move from `mir_abi_resource_runtime_fn_by_kind` /
  `mir_abi_resource_runtime_fn_by_type_name` to concrete row-record consumption.

### 2026-07-09 -- Slot/device builtin consumers validate runtime-call rows

- Extended row-record consumption from the first representative consumers into
  `transpiler_slot_runtime_row.c`, `transpiler_let_slot_emit.c`, and
  `llvm_expr_slot_device_calls.c`.
- These paths now resolve Slot/SecureSlot/DeviceSlot operations through
  `mir_abi_resource_runtime_row_by_kind(...)` and check the MIR-owned
  `call_shape` before emitting claim/read/write/release/submit-read calls.
- Updated `abi_ownership_shape_smoke.sh`, `backend_fail_closed_smoke.sh`,
  `perf_contract_smoke.sh`, and the self-host backend-emitter contract owner so
  these paths cannot regress to symbol-only runtime ABI lookup.
- The production ABI blocker remains open: MIR resource-op emission, pin
  emission/cleanup, LLVM runtime declaration registries, and remaining
  slot/member callsites still have symbol-only compatibility consumers.

### 2026-07-09 -- C MIR resource ops consume runtime-call row records

- Repointed `transpiler_mir_resource_op_core.c` from symbol-returning
  `mir_abi_resource_runtime_fn(...)` / `mir_abi_resource_runtime_fn_by_kind(...)`
  calls to concrete `MIRResourceRuntimeRow` lookups.
- C MIR Claim/Read/Write/Release emission now validates the MIR-owned
  `call_shape` before rendering the runtime call.
- Updated `abi_ownership_shape_smoke.sh`, `backend_fail_closed_smoke.sh`, the
  self-host backend-emitter contract owner, and the ABI-layout row contract
  owner so this core MIR path cannot regress to symbol-only lookup.
- The production ABI blocker remains open for pin emission/cleanup, LLVM
  runtime declaration registries, and remaining slot/member compatibility
  callsites.

### 2026-07-09 -- C pin emission/cleanup consume runtime-call row records

- Repointed C MIR pin-region enter/exit emission in
  `transpiler_mir_pin_emit.c` to consume concrete `MIRResourceRuntimeRow`
  records for PinRead/PinWrite and Unpin instead of symbol-only lookup.
- Repointed source-level C pin block enter/cleanup attribute emission in
  `transpiler_block_emit.c` to consume row records for PinRead/PinWrite and
  UnpinCleanup. Source statement auto-release was closed in the following
  row-record slice.
- Both paths now validate the MIR-owned `call_shape` before emitting a runtime
  call or cleanup attribute, and both consume the shared
  `transpiler_slot_runtime_expected_call_shape(...)` owner instead of carrying
  local pin-only call-shape tables.
- Tightened `abi_ownership_shape_smoke.sh`, `backend_fail_closed_smoke.sh`,
  `perf_contract_smoke.sh`, and the self-host backend-emitter contract owner so
  these C pin paths cannot regress to symbol-only runtime ABI lookup.
- The production ABI blocker remains open for LLVM pin/unpin declaration
  registries, LLVM pin-region emission, and remaining slot/member
  compatibility callsites.

### 2026-07-09 -- C source auto-release consumes runtime-call row records

- Repointed source statement auto-release in `transpiler_block_emit.c` from the
  symbol-returning `mir_abi_resource_runtime_fn_by_kind(...)` accessor to
  concrete `MIRResourceRuntimeRow` records for Release.
- The same block-level runtime-row helper now covers PinRead/PinWrite,
  UnpinCleanup, and Release, and validates each row through the shared
  `transpiler_slot_runtime_expected_call_shape(...)` owner before emitting C.
- Tightened `abi_ownership_shape_smoke.sh`, `backend_fail_closed_smoke.sh`, and
  `perf_contract_smoke.sh` so `transpiler_block_emit.c` cannot regress to the
  symbol-only row accessor.
- The production ABI blocker remains open for LLVM pin/unpin declaration
  registries, LLVM pin-region emission, LLVM/source with-slot cleanup releases,
  and remaining slot/member compatibility callsites.

### 2026-07-09 -- LLVM MIR pin-region emission consumes runtime-call row records

- Repointed secure LLVM MIR pin-region enter/exit emission in
  `llvm_mir_pin_region.c` from symbol-returning
  `mir_abi_resource_runtime_fn_by_kind(...)` to concrete
  `MIRResourceRuntimeRow` records for PinReadInit/PinWriteInit and Unpin.
- The LLVM pin-region emission path now validates the row-owned `call_shape`
  before looking up the registered runtime function.
- Tightened `abi_ownership_shape_smoke.sh`, `backend_fail_closed_smoke.sh`,
  `perf_contract_smoke.sh`, and the self-host backend-emitter contract owner so
  the LLVM pin-region path cannot regress to symbol-only runtime ABI lookup.
- The production ABI blocker remains open for LLVM Slot/SecureSlot/DeviceSlot
  runtime declaration registries, LLVM/source with-slot cleanup releases, and
  remaining slot/member compatibility callsites.

### 2026-07-10 -- MIR JSON fact-only frontier expands to 90 fixtures

- Added `array_index_assign`, `long_scalar`, `option_try`, and
  `string_equality_concat` to the MIR parity fixture manifest owner.
- Raised the MIR fact graph payload count and parity shell ratchet from 86 to
  90 fixtures, with the clean-reject count still at 0.
- Updated the hard self-host scorecard, status ledger, production-bar review,
  and self-host progress ledger so the current frontier says 90 PASS / 0 gap
  plus 0 clean rejects.
- This broadens the actual `pgy --mir-json | mir_lower | codegen == C oracle`
  compiler-core replacement path across Long scalar flow, array index
  assignment, `Option` `?` propagation, and string equality-plus-concat flow.

### 2026-07-10 -- MIR JSON fact-only frontier reaches all codegen fixtures

- Added the remaining committed self-host codegen fixtures to the MIR parity
  manifest: `c_reserved_binding`, `enum_match`, `float_signature`,
  `seed_random`, and `string_array_index_return`.
- Repointed self-host codegen boolean equality rewriting so payload-free enum
  comparisons consume the enum literal projection owner; `d == North` now
  lowers through `Direction_North` instead of emitting an undefined bare C
  symbol.
- Raised the MIR fact graph payload count and parity shell ratchet from 90 to
  95 fixtures, still with 0 clean rejects.
- The MIR path now covers 26 MIR-lower fixtures, all 68 committed codegen
  fixtures, and the example-origin `examples/binary_search.pgy` fixture.

### 2026-07-10 -- MIR parity manifest owns codegen fixture coverage

- Added `MirParityCodegenFixtureCoverageReady()` to the MIR parity fixture
  manifest owner.
- The manifest owner now walks `src/self_hosted/codegen/fixture` and refuses to
  emit the MIR parity fixture list if any committed codegen fixture is absent
  from the MIR fact-only path.
- Tightened `self_hosted_component_contract_smoke.sh` so this coverage contract
  cannot be removed without failing the component contract gate.

### 2026-07-10 -- Driver rungs consume the codegen fixture frontier

- Repointed `driver_rung0_owner.pgy` so DRV-0/DRV-1 fixture manifests consume
  `CodegenParityFixtureManifestRows()` from `codegen/fixture_manifest_owner.pgy`
  instead of carrying a separate three-sample fixture list.
- Ratcheted both driver parity runners to 68 fixtures, matching the committed
  codegen parity frontier, and tightened the component contract so hard-coded
  driver fixture paths cannot return.
- Added process-local compile caching in the shared LLVM/parity helper for the
  TestHarness manifest and backend output comparator. This keeps the broader
  DRV fixture frontier from recompiling the comparator for every artifact
  comparison while still rebuilding on the next script invocation.
- This broadens the driver artifact rung without claiming released/native
  driver replacement: the driver still assembles self-parser and self-codegen
  facts and compares against the current oracle.

### 2026-07-10 -- Codegen fixture frontier becomes a shared owner

- Split `src/self_hosted/codegen/fixture_manifest_owner.pgy` out of the codegen
  run boundary so fixture discovery is owned by a lightweight manifest owner.
- Repointed codegen run, DRV-0/DRV-1, and MIR parity fixture manifests to
  consume that owner. The MIR manifest no longer carries a copied 68-row
  codegen fixture list; it keeps only the MIR-specific core rows plus the
  example-origin fixture and projects codegen rows from the shared owner.
- Tightened the component contract so copied codegen fixture paths cannot return
  to the MIR manifest.

### 2026-07-10 -- MIR payload count consumes the manifest owner

- Repointed `MirFactGraphPayloadFixtureCount()` to return
  `MirParityFixtureCount()` instead of carrying its own `95` literal.
- Tightened the component and compiler-world contracts so the payload contract
  cannot reopen a second fixture-count source of truth while the shell parity
  ratchet still checks the current 95-fixture frontier.

### 2026-07-10 -- Runtime-call ABI readiness consumes row keys

- Added `CompilerRuntimeCallAbiRowIndex(domain, operation)` and
  `CompilerRuntimeCallAbiRowFor(domain, operation)` to the self-host
  runtime-call ABI row owner.
- Repointed `CompilerRuntimeCallAbiRowsReady()` so representative runtime helper
  and native-resource rows are checked by their `domain|operation` key instead
  of fixed artifact indexes.
- Kept the runnable manifest's ordered artifact output intact, but tightened
  `self_hosted_component_contract_smoke.sh` so readiness cannot reintroduce
  the `CompilerRuntimeCallAbiConcreteRowCount() == 237` or `RowAt(22) ==`
  position-as-truth checks.

### 2026-07-10 -- Symbol table readiness consumes named row facts

- Added `CompilerSymbolTableRowIndex` / `CompilerSymbolTableRowKnown` and
  `CompilerSymbolProjectionIndex` / `CompilerSymbolProjectionKnown` to the
  self-host compiler symbol table owner.
- Repointed `CompilerSymbolTableReady()` from fixed row/projection index
  comparisons to named fact membership plus explicit out-of-range boundary
  checks.
- Tightened the component and compiler-world contracts so the old
  `CompilerSymbolTableRowCount() == 7`, `SymbolTableRowAt(0) == ...`, and
  `SymbolProjectionAt(0) == ...` readiness shape cannot return.

### 2026-07-10 -- Target capability readiness consumes named facts only

- Repointed `CompilerTargetCapabilityEnvelopeReady()` from fixed projection,
  target-fact, and fallback-reason indexes to named `Known(...)` membership
  checks plus out-of-range boundary checks.
- Kept the manifest's ordered projection/fact/fallback artifact intact, but
  made readiness insensitive to row insertion order.
- Tightened the component and compiler-world contracts so the old
  `ProjectionCount() == 3`, `FactCount() == 8`, `FallbackReasonCount() == 5`,
  and `At(0) == ...` readiness shape cannot return.

### 2026-07-10 -- ArtifactZone readiness consumes artifact-kind membership

- Repointed `CompilerArtifactZoneReady()` from the 24 fixed artifact-kind index
  checks to `CompilerArtifactKindKnown(...)` membership plus an out-of-range
  boundary check.
- Kept ordered `CompilerArtifactKindAt(index)` for manifest/projection output,
  but made readiness independent of artifact insertion order.
- Tightened the component and compiler-world contracts so the old
  `CompilerArtifactKindCount() == 24` and representative
  `CompilerArtifactKindAt(n) == ...` readiness shape cannot return.

### 2026-07-10 -- Sandbox capability readiness consumes named envelope facts

- Repointed `CompilerSandboxCapabilityEnvelopeReady()` from fixed
  capability/frame-budget/ambient-rule/boundary-rule counts and representative
  indexes to named `Known(...)` membership plus out-of-range boundary checks.
- Kept the ordered sandbox capability manifest intact for stable artifact
  comparison, but made readiness independent of insertion order across
  capabilities, budgets, ambient-denial rules, and host-call boundary rules.
- Tightened the component contract so the old `Count() == N` and representative
  `At(n) == ...` readiness shape cannot return.

### 2026-07-10 -- Compatibility evolution readiness consumes named surfaces

- Added named membership checks for compatibility surfaces, obsolete migration
  fields, and compatibility change kinds in `compatibility_evolution_owner.pgy`.
- Repointed `CompilerCompatibilityEvolutionReady()` from fixed
  `SurfaceAt(n)`, `ObsoleteMigrationFieldAt(n)`, `ChangeKindAt(n)`, and
  `ChangeRowAt(n)` positions to out-of-range boundary checks plus
  `Known(...)`/`ChangeRowForSurface(...)` lookups.
- Kept the ordered compatibility-evolution artifact and corpus rows stable for
  manifest/corpus parity, but made readiness independent of row insertion
  order across source, ABI, behavior, diagnostic, AIR, MIR, runtime trace,
  capability profile, and stdlib compatibility surfaces.

### 2026-07-10 -- AIR evidence readiness consumes named proof facts

- Added `CompilerAirEvidenceFactKnown(...)` to the compiler AIR evidence owner.
- Repointed `CompilerAirEvidenceEnvelopeReady()` from fixed
  `FactCount() == 7` and representative `FactAt(n)` checks to named proof-fact
  membership plus an out-of-range boundary check.
- Tightened the component and compiler-world contracts so AIR evidence
  readiness cannot return to index-position truth while the AIR graph validator
  continues to consume the envelope before fixture validation.

### 2026-07-10 -- Subprocess runner consumes oracle facts by membership

- Repointed subprocess fact, use-case, and oracle env-allowlist readiness from
  fixed `Count() == N` and representative `At(n)` checks to membership loops
  plus out-of-range boundary checks in `subprocess_runner_owner.pgy`.
- Changed `backend_output_comparator` to consume
  `CompilerSubprocessUseCaseKnown(CompilerSubprocessOracleCompareUseCase())`
  instead of reading `CompilerSubprocessUseCaseAt(0)`.
- Kept ordered env-allowlist projection stable for expected artifacts while
  removing row position as the subprocess bridge truth.

### 2026-07-10 -- Backend AIR access readiness consumes forbidden terms by name

- Added source-extension and forbidden-term membership lookups to
  `backend_air_access_contract_owner.pgy`.
- Repointed `CompilerBackendAirAccessContractReady()` from exact
  source-extension/forbidden-term counts and representative `At(n)` checks to
  named membership plus out-of-range boundary checks.
- Kept ordered source-extension and forbidden-term rows for stable checker
  artifact emission, but made readiness independent of row insertion order.
- Repointed the forbidden-hit self-test to the owner-named primary forbidden
  term instead of `CompilerBackendAirAccessForbiddenTermAt(0)`.
- Tightened the component contract so the old backend AIR access count/index
  readiness shape cannot return while the parity gate continues to prove the
  clean and forbidden-hit artifacts across C/LLVM-built self-host tools.

### 2026-07-10 -- Backend emitter readiness consumes source-term rows by name

- Added required-row and forbidden-row membership lookups to
  `backend_emitter_contract_owner.pgy`.
- Repointed `CompilerBackendEmitterContractReady()` from exact
  required/forbidden counts and representative `At(n)` checks to named
  `(path, term)` membership plus out-of-range boundary checks.
- Kept ordered required/forbidden rows for stable checker artifact emission,
  but made readiness independent of row insertion order.
- Repointed backend-emitter missing-required, missing-input, and forbidden-hit
  self-tests to owner-named primary rows instead of `At(0)`.
- Tightened the component contract so the old backend-emitter count/index
  readiness shape cannot return while the parity gate continues to prove the
  clean and three negative artifacts across C/LLVM-built self-host tools.

### 2026-07-10 -- Backend ABI layout contract splits from ABI rows

- Added `backend_abi_layout_contract_owner.pgy` as the owner for native/backend
  ABI source required/forbidden terms while importing `abi_layout_row_owner.pgy`
  for the cross-backend ABI row readiness dependency.
- Repointed the backend ABI layout checker self-tests and report owner to the
  contract owner and to named primary rows instead of `At(0)` positional rows.
- Repointed `CompilerBackendAbiLayoutContractReady()` from representative
  count/index checks to named `(path, term)` membership plus out-of-range
  boundary checks, keeping ordered rows only as the checker artifact shape.
- Registered the new owner in `PgyCompilerWorld` path manifests, OWNERS, and
  component/compiler-world contracts so the split cannot become a hidden shell
  side list.

### 2026-07-10 -- Loop body facts replace the codegen-owned For bridge

- Added `SemanticAstIterationTypeFacts` for range-bound and foreach-collection
  verdicts plus lexical loop-binding type rows.
- Extended DRV-2 from sixteen to nineteen fixtures with a positive loop body,
  undefined iterable, and invalid loop-body initializer; C-built and
  LLVM-built drivers must emit identical C or structured diagnostics.
- Repointed self-host codegen `For` lowering to
  `SemanticAstStatementFacts` through the fail-closed semantic codegen view and
  deleted `ast_text_for_stmt_owner.pgy`.
- Fixed C value-result lowering so a nested `inout` return expression is
  evaluated before outer copy-out; a C/LLVM differential fixture prevents the
  lost-update regression.

### 2026-07-11 -- DRV-2 composes the self-host MIR consumer

- Refactored DRV-2 around one `CompileArtifactToCVerified` owner so source and
  MIR JSON inputs converge before semantic verification and codegen.
- Added `pgy --self-driver --mir-json <file>` as an explicit bridge path. The C
  oracle produces `pgy.mir.v1`; Pergyra owns reconstruction, semantic verdicts,
  and C emission with no source-text fallback.
- Renamed the MIR-lower global `Die` definition to `MirLowerFailClosed`, removing
  the stage collision that prevented MIR-lower and codegen owners from sharing
  one binary.
- Extended DRV-2 parity with four MIR intersection fixtures. C/LLVM-built
  drivers must emit identical C, compile it, and run-equal to the native C
  oracle; malformed schema input must fail through the MIR diagnostic owner.
- Added `SemanticAstNominalConstructorFacts` so nominal construction and method
  programs consume parser-owned ordered field rows instead of the legacy source
  constructor scan. The `class_method` fixture now crosses the integrated MIR
  boundary, and nominal Option helper emission carries its required Bool-header
  fact into C header projection.

### 2026-07-11 -- DRV-2 owns bounded source-to-MIR production

- Added Pergyra-owned MIR program, expression, routine lowering, artifact
  lowering, verifier, and JSON projection owners under `src/self_hosted/mir/`.
  The producer consumes typed arena and semantic facts; it does not recover
  semantic state from provenance or source text.
- Added typed nominal-subkind and ordered nominal-field-name facts so MIR
  declaration production does not reopen AST text scans for class/struct kind
  or constructor field order.
- Rewired DRV-2 source compilation through typed artifact -> semantic verifier
  -> Pergyra MIR producer -> MIR verifier -> `pgy.mir.v1` -> Pergyra MIR
  consumer -> codegen. The C MIR producer is now comparison-only on this rung.
- Added `--emit-mir-json-verified` and `--canonicalize-mir-json` to the
  self-host driver. The producer omits the transitional `ast` compatibility
  field, and the gates reject its reintroduction.
- Extended the four MIR intersection fixtures to compare canonical C-oracle and
  Pergyra-produced MIR, emitted C, compiled execution, and C/LLVM-built
  self-host driver outputs. Unsupported facts remain fail-closed.
- Fixed the progress metric to include `src/self_hosted/mir/`. The corrected
  current inventory is 25,389 frontend/backend Pergyra LOC over 283,326 C
  reference LOC (8.96%), while the broader Pergyra compiler-core inventory is
  40,192 LOC. The first ratio does not use the compiler-core inventory as its
  denominator. Bounded DRV-2 replacement is live; released/default native
  replacement remains honestly 0%.

### 2026-07-12 -- DRV-2 consumes iteration facts and restores MIR JSON fixed point

- Added `DriverRung2VerifiedFacts` so initializer and iteration facts computed
  by the hard semantic verifier reach the MIR producer instead of being
  discarded at the driver boundary.
- Added MIR-owned iteration projection rows. Range loops retain `Int` binding
  facts, while array foreach loops consume the semantic element type and carry
  collection SSA uses; no source or AST-text fallback was added.
- Promoted `for_each.pgy` into the DRV-2 MIR producer/consumer inventory,
  moving that bounded intersection from 9 to **10 fixtures**. The gate directly
  checks `Int` and `String` binding rows and both collection-use rows.
- Added `SelfMirParallelCaptureRows` and emitted the required
  `parallel_capture_boundaries` MIR JSON field. This repairs the self-produced
  JSON fixed point after the native schema made capture facts mandatory.
- Focused evidence: component contract green; C-built and LLVM-built DRV-2
  each prove 19 semantic body fixtures and 10 MIR source/consumer/run fixtures.
  The filtered MIR JSON gate also proves range/foreach execution plus valid,
  unknown-kind, and invalid-writer parallel-capture input contracts.
Released/default replacement remains 0%.

### 2026-07-13 -- Recursive conditions consume semantic expression handles

- Added `SemanticExpressionGraphFacts` under the existing expression-surface
  authority. The graph uses stable node indexes and explicit left/right edges;
  it is not a second codegen expression owner.
- Repointed self-host `if` and `while` condition emission to recurse over graph
  handles. The migrated emitter cannot call `RewriteBool`,
  `FindTopLevelOp2`, or `Substring` to recover precedence from text.
- Added a grouped-precedence negative-space case, `(flag || other) && !other`,
  so flattening the graph would change expected output and fail the gate.
- Limited graph materialization to live condition atoms instead of eagerly
  duplicating every expression lane. This keeps the current compiler-scale
  memory surface proportional to the migrated consumer set.
- C-built and LLVM-built self-host codegen tools emitted byte-identical C and
  ran equal for `bool_logic`, `string_equality`, and
  `string_equality_concat`.
- The expression owner remains `BRIDGE`: the graph producer still lowers the
  compact parser payload. Parser-arena production and non-condition recursive
  expressions are the next owner/consumer seam.

### 2026-07-13 -- DRV-2 condition graphs survive MIR JSON without reparse

- Added instruction-owned `SelfMirExpressionGraphRows` and projected
  `expr0_graph` objects into self-produced `pgy.mir.v1`.
- Added the MIR JSON graph reader and bound graph roots to reconstructed
  artifact condition NodeIds before semantic verification and codegen.
- Rewired both DRV-2 source compilation and direct `--mir-json` compilation
  through `CompileMirJsonTextToCVerified`. The hard consumer fails closed when
  a condition graph is missing; only the named `--canonicalize-mir-json`
  C-oracle bridge may derive the graph from legacy compact expression text.
- Split expression-surface row capture from graph production. Hard MIR
  consumption builds structural rows and injects MIR-owned graph facts, so
  semantic artifact matching no longer rebuilds a comparison graph from text.
- Removed recursive return of the eight-array graph-copy aggregate after it
  caused an LLVM-built driver access violation on nested equality conditions.
  The replacement validates and copies the semantic graph's postorder
  contiguous subtree interval.
- Focused evidence: C-built and LLVM-built DRV-2 drivers produced
  byte-identical MIR JSON and emitted C across all 10 MIR fixtures; source and
  MIR paths were byte-identical, and all 10 programs ran equal to the native C
  oracle. Removing `expr0_graph` from a condition fixture was rejected.
- Scope remains bounded: parser payload to semantic graph production and
  non-condition recursive expression graphs are still `BRIDGE`; released and
  default replacement remain 0%.

### 2026-07-13 -- Parser/HIR owns DRV-2 condition graph production

- Added canonical `AstExpressionArena` and `AstExpressionGraphRows` under HIR,
  with shape, edge-order, root, and union-reachability validation.
- Changed the parser precedence walk to return `ParserExpressionFact` and emit
  logical/equality graph nodes while parsing. Compact expression text is now a
  parity/provenance projection for this migrated slice, not its semantic owner.
- Carried ordered condition roots through `ParserProgramBuild` and
  `AstTreeArtifact`. Declaration and implicit-`Main` lanes preserve emitted
  tree order.
- Rewired DRV-2 source verification to
  `SemanticAstArtifactAnalyzeTyped`. A text-created artifact with no typed graph
  is rejected; hard source and MIR consumers are statically forbidden from
  invoking `SemanticExpressionGraphBuildFromText`.
- Renamed the transitional analyzer to
  `SemanticAstArtifactAnalyzeCompactBridge`. It remains available to older
  standalone semantic fixtures, DRV-0/DRV-1 breadth paths, and the named
  C-oracle canonicalizer only. A function-scoped hard gate forbids it in
  `VerifyArtifactForMirProduction`.
- Focused evidence: parser C/LLVM parity is byte-equal across 188 sources;
  DRV-2 producer-first parity passes both backends across 19 source fixtures
  and 10 MIR fixtures; the public `pgy --self-driver` path is artifact-equal
  and fails closed when its self-host driver is unavailable.
- Scope remains bounded: non-condition recursive expressions,
  indexed/wrapper/auxiliary lanes, and initial compact-tree arena construction
  remain `BRIDGE`; released/default replacement remains 0%.

### 2026-07-13 -- DRV-2 value expressions consume parser-owned graphs

- Extended the parser/HIR expression arena with relational, additive, and
  multiplicative node kinds while preserving precedence in parser-owned child
  edges.
- Bound required roots by canonical `(owner kind, lane)` rows for `let`,
  assignment, and value return in addition to `if` and `while`. Assignment
  targets remain under the existing assignment-fact owner and are not duplicated
  as expression roots.
- Required `expr0_graph` on migrated MIR definition, branch, and value-return
  instructions. The MIR consumer maps those rows back to typed artifact owners
  and fails closed on missing or mismatched root text.
- Repointed scalar/String/Float/Long initializer, assignment, and return
  emission to the graph consumer. The compact graph builder and compact
  expression analyzer now carry explicit `CompactBridge` names and are blocked
  from the DRV-2 hard source path.
- Renamed the parser transport from `condition_graphs` to `expression_graphs`;
  no compatibility alias remains. Split the expression-surface executable
  contract and routine-entry seeding from their production owners to keep each
  owner below 600 lines without hiding responsibility.
- Focused evidence: readiness contracts 6/6, hard contract, component contract,
  and owner-size gate are green. C-built and LLVM-built DRV-2 drivers pass the
  19-source/10-MIR producer-first parity gate. The strengthened mutable-local
  fixture emits byte-identical C across both tool builds and the generated
  program exits 0.
- Scope remains bounded: call/index/wrapper/unary internals,
  log/collection/auxiliary lanes, and compact-tree arena construction remain
  `BRIDGE`; released/default replacement remains 0%.

### 2026-07-13 -- DRV-2 index reads consume parser-owned topology

- Added `AstExpressionNodeIndex` and changed postfix parsing to preserve the
  receiver and index expression as child handles in the parser-owned graph.
  `ApplyPostfixExpr` was removed; unsupported postfix forms remain explicit
  leaf bridges instead of creating a second index parser.
- Added MIR JSON `kind: index` projection/consumption and repointed graph
  codegen to select the collection runtime get ABI from the receiver type fact.
  `RewriteExprFromSemanticGraph` is gate-forbidden from calling
  `RewriteIndexing`.
- Strengthened `valid_array_builtins` with a real `xs[0]` read and promoted it
  into both DRV-2 manifests. The bounded replacement frontier is now 20 source
  fixtures and 11 source/MIR producer-consumer fixtures.
- Focused evidence: component contract and owner-size gates are green. C-built
  and LLVM-built DRV-2 drivers pass the 20-source/11-MIR producer-first gate;
  the array program runs equal to the native C oracle, source/MIR C is equal,
  and missing or invalid graph mutations fail closed.
- Scope remains bounded: call/wrapper/unary internals,
  log/collection/auxiliary lanes, and compact-tree arena construction remain
  `BRIDGE`; released/default replacement remains 0%.

### 2026-07-13 -- DRV-2 logical-not and numeric-negate own unary edges

- Added explicit `logical_not` and `negate` graph kinds plus the owner-level
  0/1/2 arity rule. Unary nodes carry one operand edge; the unused storage cell
  stays at canonical zero and MIR JSON projects the absent edge as `null`.
- Replaced the `ParseUnaryFact` leaf collapse for `!expr` and `-expr` with
  `ParserExpressionUnary`. Semantic, MIR producer/consumer, reachability, JSON,
  and codegen now consume the same arity fact. A unary row with a second child
  fails the MIR verifier.
- Codegen emits logical-not and numeric-negate from the operand handle. The
  root text is not reparsed, and canonical outer-parenthesis ownership remains
  byte-identical with the native oracle, including the existing `-1` CFG case.
- Promoted `valid_option_none_literal.pgy` into the source/MIR intersection.
  C-built and LLVM-built DRV-2 drivers pass the 20-source/12-MIR producer-first
  gate, native execution oracle, source/MIR C artifact comparison, and missing
  or malformed graph mutations.
- Scope remains bounded: call/try/member/pipe/object-init internals,
  borrow/receive/spawn/await unary forms, log/collection/auxiliary lanes, and
  compact-tree arena construction remain `BRIDGE`; released/default
  replacement remains 0%.

### 2026-07-13 -- DRV-2 direct identifier calls own argument spines

- Added `call` and `call_argument` graph kinds. A call root owns its callee;
  every argument row owns the prior call spine and one argument expression.
  Parser, semantic, MIR JSON, and consumer verifiers reject malformed arity or
  a call-argument row whose left edge is not a call spine.
- Direct identifier calls now emit argument order, `inout`/`ref` parameter
  modes, runtime ABI aliases, `Some`/`ArrayLength` specialization, and struct
  constructors from carried graph facts. The hard consumer is gate-forbidden
  from reopening the legacy `RewriteInoutCallArgs` or `ExprSequenceItemAt`
  argument scanners.
- Promoted `valid_call_int.pgy` into the source/MIR intersection. C-built and
  LLVM-built DRV-2 drivers pass the 20-source/13-MIR producer-first gate with
  byte-identical MIR JSON and C, native execution parity, and malformed call
  mutations rejected.
- The owner-level unknown arity sentinel was removed; unknown node kinds now
  fail closed. The likeness gate is green at sentinel 0 and its Result/Option
  use floor is ratcheted to 1,395.
- Scope remains bounded: qualified/member calls, try/pipe/object-init
  internals, literal-only argument bridges, borrow/receive/spawn/await unary
  forms, log/collection/auxiliary lanes, and compact-tree arena construction
  remain `BRIDGE`; released/default replacement remains 0%.

### 2026-07-13 -- DRV-2 simple member access and method calls own edges

- Added `member_access` graph rows with receiver/member child handles. Parser,
  semantic, MIR JSON, and consumer verifiers require a leaf member name and
  reject malformed member rows.
- Repointed simple `self.field` emission and `v.Method(arg)` dispatch to the
  graph owner. Method emission resolves the receiver type and method signature
  rows, inserts the receiver at parameter offset zero, and projects explicit
  arguments from the call spine. The hard member consumers are gate-forbidden
  from reopening the legacy member-call, qualified-call, field-access, or
  parenthesis scanners.
- Strengthened `class_method.pgy` with field reads and an explicit method
  argument. C-built and LLVM-built DRV-2 drivers pass the 20-source/13-MIR
  producer-first gate with byte/run parity and malformed graph rejection.
- Scope remains bounded: nested or namespace-qualified member calls,
  try/pipe/object-init internals, literal-only argument bridges,
  borrow/receive/spawn/await unary forms, log/collection/auxiliary lanes, and
  compact-tree arena construction remain `BRIDGE`; released/default
  replacement remains 0%.

### 2026-07-13 -- DRV-2 Log arguments own parser subtrees

- The expression-statement parser now extracts a verified single-argument
  subtree from the `Log(...)` call spine and binds it to
  `(TypedAstKindLogStmtTag, AstExpressionLaneAtom)`. It does not slice the
  rendered call text to recover the argument.
- Semantic and MIR verifiers require the Log root. Source lowering attaches it
  to the Log instruction, and direct MIR consumption rejects a missing or
  malformed `expr0_graph` instead of rebuilding it from `expr0`.
- `EmitLog` now renders through `RewriteExprFromSemanticGraph` and is
  gate-forbidden from using the former ToString prefix, substring, or
  semantic-shape paths. The first run found topology-dependent output: rich
  self MIR selected direct scalar printing while the approved compact oracle
  graph selected the runtime ToString alias. Removing that specialization made
  both graph representations emit the same canonical C.
- C-built and LLVM-built DRV-2 drivers pass the 20-source/13-MIR
  producer-first gate with byte/run parity and graph-removal/invalid-root
  rejection. Expression result-type classification remains a separate
  text-backed seam; released/default replacement remains 0%.

### 2026-07-14 -- DRV-2 bare-call statements consume parser call graphs

- Added `TypedAstCallStatementKindForCallee` as the single owner for direct
  call-statement classification. The typed-text bridge consumes that owner;
  no compatibility alias for the retired classifier remains.
- The parser binds the complete direct-call graph to the bare-call atom lane.
  Semantic and MIR verifiers require it, source lowering carries it, and the
  MIR JSON consumer rejects a missing or invalid graph.
- Repointed bare-call emission to `RewriteExprFromSemanticGraph` and removed
  the unused string payload accessor. The component contract rejects both the
  accessor and `RewriteExpr(call_expr, env)` fallback if they return.
- Focused evidence: C-built and LLVM-built DRV-2 drivers emitted identical MIR
  JSON and generated C for `param_carriage`; both generated
  `Mutate(&value);`, and execution matched the native oracle (`2`, `2`, `42`).
  Removing only the bare-call graph failed closed with the same diagnostic on
  both builds.
- The full 20-source/13-MIR corpus was not refreshed in this slice: it exceeded
  the five-minute focused-gate budget after making progress without an
  implementation error. Released/default replacement remains 0%.

### 2026-07-14 -- DRV-2 pipe syntax canonicalizes to call graphs

- Replaced `ParsePipeFact`'s rendered-call leaf collapse with the existing
  parser-owned `call` / `call_argument` graph. The left pipeline value becomes
  argument zero and explicit pipe-call arguments retain source order.
- Added a readiness witness for `5 |> Double |> Add(3)` and a component ratchet
  that rejects returning to `ParserExpressionLeaf` inside the pipe owner.
- Added `pipe_carriage.pgy` as the fourteenth DRV-2 MIR fixture. Its initializer
  is a nine-node nested call spine rooted at `Add(Double(5), 3)`.
- Focused evidence: C-built and LLVM-built drivers emitted identical MIR JSON
  and C; native-oracle and self-produced canonical MIR JSON were identical;
  both executables printed `13`. Removing only the initializer graph failed
  closed with the same diagnostic under both drivers.
- This closes pipe graph production/carriage/consumption, not the adjacent
  try (`?`), object-init, special-unary, or structured-leaf bridges. The full
  20-source/14-MIR matrix was not refreshed and released/default replacement
  remains 0%.

### 2026-07-14 -- DRV-2 postfix try owns operand graphs

- Postfix `?` now produces `AstExpressionNodeTry` with one parser-owned operand
  edge. The rendered compatibility surface remains separate from the graph
  node text, so codegen never has to rediscover the operand boundary.
- Deleted the local-binding try-operand string row and its dedicated codegen
  view. `try_let_emit_owner.pgy` receives the semantic expression-graph view,
  requires a Try root, and renders only the operand child. Missing or malformed
  `expr0_graph` fails closed.
- The named compact bridge still recognizes legacy/native try text, but it
  immediately constructs the canonical Try graph. It is limited to
  canonicalization and soft compatibility paths and cannot feed hard codegen
  as a string fallback.
- Focused evidence: C-built and LLVM-built DRV-2 drivers emitted byte-identical
  self MIR. Native and self canonical MIR were byte-identical with Try nodes
  retained. All source/raw/canonical routes emitted C with SHA-256
  `92AB69DB2E436CA221F6D3B5526C1FB3FD8FDBD3A778460EBCD5718743ECA159`, and the
  executable matched `option_try_stdout.txt`. Removing all expression graphs
  failed closed under both drivers.
- The full 20-source/15-MIR matrix was not refreshed in this focused slice.
  Released/default replacement remains 0%.

### 2026-07-14 -- DRV-2 nested field reads consume recursive member graphs

- The parser already owned `line.end.x` as nested `member_access` rows. The
  remaining gap was the hard C emitter's leaf-only receiver check.
- Added recursive receiver-type projection over expression node handles. Each
  edge consumes the current receiver type and `LookupFieldType` row; the hard
  consumer does not split or scan the rendered dotted path.
- Added `nested_member_access.pgy` as the sixteenth DRV-2 MIR fixture. C-built
  and LLVM-built drivers emitted byte-identical MIR and C, source-first and
  MIR-first output matched, and the generated executable matched the native
  oracle output `3`.
- Removing `expr0_graph` or replacing its root with an invalid node failed
  closed with `MIR instruction expression graph is missing or invalid` under
  both drivers. The full 20-source/16-MIR producer-first gate passed for C and
  LLVM driver builds.
- Scope remains bounded: nested-receiver method calls, namespace-qualified
  calls, object-init internals, and auxiliary type classification remain
  `BRIDGE`; released/default replacement remains 0%.

### 2026-07-14 -- DRV-2 nested receiver calls consume recursive type facts

- Repointed hard instance-call emission so a receiver no longer has to be a
  leaf binding. `line.end.LengthPlus(2)` resolves `line.end` recursively from
  member edges and field/type rows, then consumes the `Vec2.LengthPlus`
  signature row and emits the receiver as argument zero.
- Added `nested_member_call.pgy` as the seventeenth DRV-2 MIR fixture. C-built
  and LLVM-built drivers produced byte-identical self MIR and generated C, and
  the generated executable matched the native oracle output `9`.
- Missing and invalid expression graphs fail closed under both drivers. The
  hard consumer cannot call the legacy member, qualified-call, field-access,
  parenthesis, or dotted-path type scanners.
- Native C MIR does not yet carry the rich graph on this legacy route, so the
  explicitly named `--canonicalize-mir-json` oracle bridge reuses the Pergyra
  expression parser to construct the comparison graph. Direct source and
  `--mir-json` hard consumers still require `expr0_graph` and cannot invoke that
  bridge.
- The full producer-first gate passed at 20 source fixtures and 17 MIR fixtures
  for both driver backends. Namespace-qualified call classification,
  object-init internals, and auxiliary/result-type classification remain
  `BRIDGE`; released/default replacement remains 0%.

### 2026-07-14 -- DRV-2 carries canonical namespace-call targets

- Added `SemanticExpressionCallTargetFact` as the semantic owner for qualified
  callable identity. `Math.Add(...)` resolves once to canonical `Math_Add`, and
  self MIR carries that target as explicit `call_target_kind` and
  `call_target_name` fields on the call node.
- Direct `--mir-json` consumption validates the carried target against the
  reconstructed semantic signature inventory. Replacing the namespace target
  with `none` fails closed under both C-built and LLVM-built drivers.
- Hard member-call emission now consumes the call-node target first. The old
  receiver-text branch that reconstructed `receiver.method` as a namespace
  symbol is deleted and gate-forbidden; target-less member calls must have a
  receiver type and remain instance-method calls.
- A six-array semantic graph arena triggered an LLVM aggregate ABI crash in
  `SemanticExpressionGraphAppendNode`. The landed representation keeps one
  optional canonical target-name row in the hot semantic arena while preserving
  the explicit kind/name pair at the MIR boundary. C/LLVM contracts and output
  remain equal without widening that hot value bundle past the working ABI.
- Added `namespace_call.pgy` as the eighteenth DRV-2 MIR fixture. The full
  producer-first gate passed at 20 source fixtures and 18 MIR fixtures for both
  driver backends; MIR JSON and emitted C are byte-identical, and execution
  matches the native oracle output `7`.
- Scope remains bounded: object-init internals, special unary/literal argument
  bridges, and auxiliary/result-type classification remain `BRIDGE`;
  released/default replacement remains 0%.

### 2026-07-14 -- DRV-2 carries for-range and identifier-foreach graphs

- `ParseForStmt` now records the lower or collection expression in the value
  lane and a range upper expression in the auxiliary lane during the canonical
  parser walk. Compact oracle canonicalization uses the same lane contract.
- MIR attaches the value graph to `loop-init` and the range-stop graph to the
  `branch`. Direct `--mir-json` consumption requires those rows and maps them
  back to the corresponding semantic lanes; it does not reparse `expr0` or
  `expr1`.
- Hard codegen emits range bounds and identifier foreach collections from the
  graph handles. The old `IntEval(start/end)`, `ExprKind(collection)`, and
  `RewriteExpr(collection)` statement-text decisions are gate-forbidden.
- C-built and LLVM-built drivers emitted byte-identical C for `forloop`
  (`D39BE7851A72DCA92E8CB123D8EFDFA9F9C894A8F3FD0EBA804BF8155B57F7D3`)
  and `for_each`
  (`C17441A36810B68CE423B1299384C74FDE0E65D5EFF5CF2278B2975D6CB356DD`).
  Both legs ran equal (`0 / 1 / 2` and `60 / abbccc`). Removing only the
  range-stop or foreach-value graph failed closed under both drivers.
- The full 20-source/18-MIR matrix exceeded the five-minute focused budget and
  was terminated. This entry therefore claims only the two focused fixtures;
  arbitrary-expression foreach type classification remains `BRIDGE`, and
  released/default replacement remains 0%.

### 2026-07-14 -- DRV-2 deletes enum call-argument text classification

- Added `enum_call_argument.pgy` as the nineteenth bounded MIR fixture. Its
  qualified argument is represented as the parser-owned
  `member_access(Direction, East)` subtree inside the `IsEast(...)` call
  spine.
- Hard `RewriteSemanticCallArgument` no longer invokes
  `EnumPayloadFreeArgumentProjectionFactOpt(source, expected_type, env)`.
  Enum value projection now follows the same graph-member and type-environment
  rows as ordinary expression emission.
- C-built and LLVM-built drivers emitted byte-identical C with SHA-256
  `E4E901D03F43C7429A2E9E033FCC12651D58718897453F0875AE4285D82409A3`;
  both generated programs printed `east`.
- Removing only `IsEast(Direction.East)`'s `expr0_graph` failed closed under
  both drivers even though the source expression and expected enum parameter
  remained. The static component gate forbids the deleted classifier inside
  the hard argument consumer.
- This is focused fixture evidence, not a refreshed full matrix. Array and
  struct literal argument bridges remain open; released/default replacement
  remains 0%.

### 2026-07-14 -- DRV-2 array literals consume parser expression graphs

- Added `ast_node_array_literal.pgy` as the twenty-seventh bounded MIR fixture.
  Its `Array<CodegenAstTextNode>` initializer is represented by one
  `array_literal` root, ordered `array_element` edges, and a constructor call
  graph for the element value.
- Hard `Let` emission consumes the parser-owned graph and the expected element
  type. The semantic local-binding body-string rows and dedicated array-literal
  codegen view are deleted; sequence splitting and struct-text recovery are
  gate-forbidden in this consumer.
- Focused C-built and LLVM-built drivers produced byte-identical canonical MIR
  and generated C. Both emitted the typed array runtime push and rejected a
  missing or invalid expression graph with the same fail-closed diagnostic.
- The complete 27-case matrix was not rerun in this focused slice. Indexed
  assignment RHS emission remains the next collection-value `BRIDGE`, and
  released/default replacement remains 0%.

### 2026-07-14 -- DRV-2 indexed assignment consumes target graphs

- The parser now records an indexed assignment's complete left-hand expression
  in the assignment auxiliary lane. Self MIR owns that tree as `expr1_graph`
  beside the RHS `expr0_graph`; the JSON importer preserves the same lane order.
- Hard collection-set emission recursively consumes the receiver and index node
  handles. `CodegenSemanticAssignmentIndexExprOrDie` and the
  `IntEval(idx_expr, env)` text-recovery path are deleted and gate-forbidden.
- C-built and LLVM-built focused drivers emitted byte-identical 3,819-byte self
  MIR for `indexed_assignment.pgy`. Native and self canonical MIR were
  byte-identical, and both generated programs printed `2`.
- Renaming `expr1_graph` made both drivers fail closed with
  `MIR instruction expression graph is missing or invalid`. The complete
  28-case matrix was not rerun in this focused slice; released/default
  replacement remains 0%.
- Two fixture-specific verifier helpers returned the preceding failed base-name
  comparison for non-target fixtures, so `set -e` aborted filtered runs before
  the selected MIR case. Their non-target exits now return success explicitly;
  the official focused C and LLVM gates each completed with 20 shared body
  fixtures and exactly one indexed-assignment MIR fixture.

### 2026-07-14 -- DRV-2 Log formatting consumes statement type facts

- `DriverRung2VerifiedFacts` now carries the already verified
  `SemanticAstStatementTypeFacts` into the hard C-emission entry. The hard path
  does not rerun semantic analysis to recover this row.
- `stmt_emit.pgy` resolves the Log node's inferred type through the stable
  node-handle query owner and passes that fact to `EmitLog`. The emitter no
  longer invokes `ExprKind` or reclassifies the statement payload.
- `SemanticAstStatementTypeQueryContractReady` rejects wrong-kind, unverified,
  `Unknown`, and missing rows. A small owner contract compiled and ran under C
  and LLVM with byte-equal output; the component, hard-substitution, and
  Pergyra-likeness gates passed.
- Restored the missing `ast_node_array_literal_stdout.txt` golden (`1`) and
  raised the shared codegen/DRV-0/DRV-1/MIR-lower manifest frontier from 70 to
  71. The source fixture had already landed without its required golden.
- A filtered `str_array` DRV-2 run was attempted, but compiling the full
  self-host driver exceeded the five-minute focused budget while holding about
  718 MB. It was terminated, so this entry does not claim full-driver C/LLVM
  parity. The remaining broad expression result-type classification is still
  `BRIDGE`; released/default replacement remains 0%.

### 2026-07-14 -- DRV-2 Match consumes statement subject type facts

- Hard Match emission now resolves the subject type through
  `SemanticAstStatementTypeFacts` at the Match node handle. The previous
  `ExprKind(match_subject, env)` text classifier is deleted and gate-forbidden.
- The statement-type owner contract now contains a real Match row and requires
  the subject `x: Int` to infer as `Int`; wrong-kind, unverified, `Unknown`, and
  missing rows still fail through the shared query owner.
- The owner contract compiled and ran under C and LLVM with byte-equal output,
  and the component contract passed. AST production for the integrated codegen
  source also passed.
- Rebuilding the complete self-host codegen tool did not yield usable parity
  evidence: native `pgy` terminated without a diagnostic during the later
  compiler stages. This entry therefore claims the producer contract and hard
  consumer ratchet, not full codegen-tool C/LLVM parity. With Log and Match
  migrated, `selfhost.statement_result_type` is `CLOSED`; remaining text-backed
  expression classification belongs to `selfhost.expression_surface`.
  Released/default replacement remains 0%.

### 2026-07-15 -- semantic owners split below 600 lines

- The native function checker no longer owns a second capability-name table.
  `capability_analyze.c` renders the stable diagnostic vocabulary, leaving
  `type_checker_func_decl.c` at 571 lines.
- Resource snapshot capture/conflict analysis and snapshot restore/destruction
  are separate responsibilities. The former is 538 lines; the new lifecycle
  owner is 72 lines.
- Self-host signature production/query (436 lines) is separate from reverse
  artifact matching (168 lines). Expression graph facts (532 lines) are
  separate from their compact-bridge executable contract (71 lines).
- `test_inc_size_smoke.sh` now covers native semantic C/header owners and
  self-host semantic Pergyra owners with a hard 599-line maximum. The gate
  passed, and an executable owner-split contract compiled and printed
  `owner-split-ok`.

### 2026-07-15 -- indexed assignment type ownership closed

- `SemanticAstAssignmentTypeFacts` now owns the assignment target type and
  index result type. The index type is projected from expression-graph handles;
  codegen no longer recovers the collection kind through
  `LookupKindType(env, arr_name, "v")`.
- Missing target-type and expected-type rows fail closed in the focused
  assignment projection probe. Static ratchets forbid payload retyping and the
  removed codegen lookup.
- The focused projection probe is now byte-equal under C and LLVM. Both legs
  reject missing expected and indexed-target type rows, so the registry row is
  `CLOSED`. The earlier timeout remains useful historical performance evidence,
  but it is no longer the current correctness status.

### 2026-07-15 -- initializer type projection and gate dashboard landed

- MIR routine input now carries semantic initializer NodeId/type rows. Local
  declaration lowering consumes those rows, allowing an unannotated local to
  inherit its verified initializer type without a source or backend guess.
- Semantic downstream consumers no longer rebuild initializer inference. A
  focused C/LLVM probe rejects both a missing row and a row with no type.
- `gate_dashboard` is written in Pergyra. Its manifest owns nine active gate
  targets, tiers, budgets, states, and owner facts; result artifacts reject
  unknown and duplicate IDs. The thin process bridge consumes the manifest
  budget through the portable timeout owner, and over-budget results fail.
  This is evidence infrastructure and does not count as a compiler-path
  substitution delta.

### 2026-07-15 -- scalar operator result typing consumes expression graph

- Fully graph-owned scalar operator trees derive result types from semantic
  graph nodes. Their verdict path no longer calls `ExprType` on source text.
- The initializer-to-MIR probe lowers `seed + 2` identically under C-built and
  LLVM-built tools. Keeping its source/root text unchanged while corrupting
  only the `2` graph leaf fails as `undefined_symbol` under both tools. Replacing
  that leaf with a String fails as `binop_type_mismatch` from graph child types.
- This is an executable closure delta inside the active mixed-expression rung.
  Call/member/composite trees remain bridged, so released/default replacement
  remains 0%.
- The likeness gate now builds one path-tagged, comment-stripped source corpus
  and reuses it for every metric instead of reading the full tree five times.
  The same Windows checkout dropped from 116.3 seconds to 56.3 seconds, inside
  the 60-second static-gate budget. This is process evidence, not substitution.

### 2026-07-15 -- direct scalar call returns consume graph callees

- Direct named calls with concrete `Bool`, `Int`, `Long`, `Float`, or `String`
  returns now derive their result from the expression-graph callee and the
  canonical callable return table. Their verdict path does not call
  `ExprType(text, ...)` for that declared capability.
- The existing initializer-to-MIR probe gained a positive direct-call mode and
  a source-preserving negative. Changing only the graph callee from
  `ToIntValue(Int) -> Int` to `ToTextValue(Int) -> String` produces
  `let_type_mismatch`; C-built and LLVM-built tools agree on the positive MIR
  row and every negative diagnostic.
- Call-argument validation, member/namespace calls, generic/aggregate returns,
  and composite expressions remain bridge-owned. This slice does not close the
  expression surface or raise released/default replacement.

### 2026-07-15 -- direct scalar-leaf call arguments consume graph handles

- Direct calls with concrete scalar return/parameter rows and scalar-leaf
  arguments now validate arity and argument types from the call-spine view.
  This declared capability does not invoke the source-text call parser.
- The source expression and root remain `ToIntValue(2)` while the negative
  mutates only the graph argument leaf to a String. C-built and LLVM-built
  probes agree on `call_arg_type_mismatch`; all prior initializer negatives
  remain green.
- Nested argument expressions, generic and aggregate signatures, collection
  and Option/Result policy, and member/namespace calls remain explicit bridges.

### 2026-07-15 -- direct scalar call arguments consume expression trees

- Expanded the active initializer-to-MIR rung from scalar-leaf direct-call
  arguments to graph-owned scalar operator trees. The call consumer validates
  operand diagnostics, arity, and parameter assignment from node handles.
- Kept `ToIntValue(1 + (2 * 3))` unchanged in the fixture while mutating only
  the nested graph's `2` leaf to a String. C-built and LLVM-built probes both
  fail closed with `binop_type_mismatch`; the unchanged positive projects
  `x: Int` to MIR with byte-identical output.
- Fixed the expression-surface verification boundary for parser-canonical
  spelling. Exact spelling is the fast path; a mismatch is accepted only when
  the same compact parser owner produces that canonical root. No source type
  reconstruction or alternate callable owner was added.
- The expression surface remains `BRIDGE`. Nested call expressions in an
  argument, generic/aggregate signatures, collection and Option/Result policy,
  and member/namespace calls remain outside this executable subset. The full
  codegen matrix was not rerun and released/default substitution remains 0%.

### 2026-07-15 -- nested direct scalar calls consume graph handles

- Extended the same concrete scalar call capability to nested direct calls.
  Call capability and argument verdicts recurse over parser-owned call-spine
  handles and do not invoke the source call parser.
- `ToIntValue(ToIntValue(2))` projects `x: Int` identically under C-built and
  LLVM-built probes. Keeping the source unchanged while changing only the inner
  graph callee to `ToTextValue(Int) -> String` fails at the outer call as
  `call_arg_type_mismatch` under both backends.
- The expression surface remains `BRIDGE`. Scalar operators containing call
  nodes, member/namespace calls, generic/aggregate signatures, and collection
  or Option/Result policy remain outside this executable subset. The full
  codegen matrix was not rerun and released/default substitution remains 0%.

### 2026-07-15 -- scalar operators and direct calls share one graph verdict

- Replaced the direct-call-only verdict owner with one concrete scalar graph
  owner. It composes leaf, scalar-operator, and concrete direct-call nodes;
  operator diagnostics remain owned by the existing node-level scalar verdict.
  No compatibility alias for the retired owner or function names remains.
- `1 + ToIntValue(2)` projects `x: Int` identically under C-built and
  LLVM-built probes. Keeping the source unchanged while changing only the graph
  callee to `ToTextValue(Int) -> String` fails as `binop_type_mismatch` under
  both backends.
- The expression surface remains `BRIDGE`. Member/namespace calls,
  generic/aggregate signatures, and collection or Option/Result policy remain
  outside this executable subset. The full codegen matrix was not rerun and
  released/default substitution remains 0%.

### 2026-07-15 -- namespace calls consume carried static targets

- Replaced the direct-leaf-only call type owner with a static-call owner. It
  consumes `SemanticExpressionCallTargetFact`; namespace consumers do not
  rebuild `Math_Add` from `Math.Add` source spelling. No compatibility alias
  for the retired owner or function names remains.
- `Math.Add(2)` projects `x: Int` identically under C-built and LLVM-built
  probes. Keeping the source/member graph unchanged while changing only the
  carried call target to the String-returning `ToTextValue` fails as
  `let_type_mismatch` under both backends.
- The expression surface remains `BRIDGE`. Receiver-bound member calls,
  generic/aggregate signatures, and collection or Option/Result policy remain
  outside this executable subset. The full codegen matrix was not rerun and
  released/default substitution remains 0%.

### 2026-07-15 -- receiver calls consume semantic owner-qualified targets

- Moved `SemanticMemberAccessView` out of the codegen emitter into the
  semantic expression-graph owner. The callable environment now retains
  declaration ownership as `Owner_Method` instead of collapsing methods with
  the same local name into one flat row.
- Replaced the static-call-only result owner with one resolved-call owner.
  Direct and namespace calls keep parameter offset zero; a bounded
  receiver-leaf member call resolves from its lexical type and consumes offset
  one for the implicit `self` parameter.
- `box.Get()` projects `x: Int` identically under C-built and LLVM-built
  probes. With source text unchanged, changing only the graph member handle to
  `Text` selects `Box_Text() -> String` and fails as `let_type_mismatch` under
  both backends. The self-host codegen entrypoint also compiles after consuming
  the moved semantic member view.
- This does not claim receiver-target carriage in MIR or hard codegen. The
  emitter still derives the instance method target from its codegen type
  environment, so `selfhost.call_target_identity` remains `ACTIVE`; the next
  executable delta must carry and consume that same target. The full codegen
  matrix was not run and released/default substitution remains 0%.

### 2026-07-15 -- receiver call target reaches MIR and hard codegen

- Extended the existing expression-graph target row from name-only carriage
  to canonical `(kind, name)` carriage. The body-type fixpoint writes
  `member/Box_Get` after initializer types make the receiver binding known.
- Self MIR copies the target row. Hard codegen consumes `Box_Get` directly and
  emits `Box_Get(box)`; it no longer reconstructs the target from receiver type
  plus member spelling. A missing target fails at MIR production.
- The focused probe reaches semantic analysis, MIR, and hard codegen. C-built
  and LLVM-built probes agree on all positive modes and source-preserving
  negative diagnostics, including the missing-target case.
- Growing the graph arena from four to six arrays exposed an LLVM-only crash
  at an aggregate `inout` boundary. Graph facts and compact construction are
  now separate responsibilities; recursive construction carries six row
  arrays and the gate rejects reintroduced graph-aggregate mutation. Surface
  token/call queries were also split from the 522-line fact owner.
- Generic or chained receiver resolution remains active. This entry does not
  close the complete call-target row, run the full matrix, or raise released
  substitution above 0%.

### 2026-07-15 -- generic receiver calls consume canonical carried targets

- Added `SemanticCanonicalTypeNameFact` to the existing canonical type-name
  owner. It carries canonical and nominal-base names together. Call-target
  resolution consumes that typed fact for `Box<Int>` rather than parsing
  generic spelling locally or introducing a string-to-string helper.
- A generic receiver parameter calling `box.Count()` carries
  `member/Box_Count` through semantic analysis and self MIR. Hard codegen emits
  `Box_Count(box)` from that carried row. Removing only the generic target row
  fails at MIR production.
- C-built and LLVM-built focused probes agree on the positive artifact and the
  missing-target diagnostic. The prior non-generic receiver fixture remains in
  the same gate.
- Chained field receiver resolution remains active because it requires
  nominal field facts at the call-target consumer. The complete call-target
  row is not closed, the full matrix was not run, and released/default
  substitution remains 0%.

### 2026-07-15 -- chained receiver calls consume nominal field facts

- Added a graph receiver-type owner that recursively follows member handles.
  Leaf bindings consume the lexical environment; member edges consume
  `SemanticAstNominalConstructorFacts` field rows after canonical nominal-base
  projection. It does not parse a dotted source path or call codegen type
  lookup.
- `holder.box.Count()` resolves `Holder.box: Box`, carries
  `member/Box_Count` through self MIR, and hard codegen emits
  `Box_Count(holder.box)` from that target. Removing only the chained target
  row fails at MIR production.
- Nominal count and field lookup queries are now `ref`, keeping borrowed facts
  read-only through recursive helpers rather than copying the aggregate. The
  focused C and LLVM probes agree on positive output and the missing-target
  diagnostic.
- `selfhost.call_target_identity` remains `ACTIVE`: direct calls still recover
  final callable identity from the callee leaf. The full matrix was not run and
  released/default substitution remains 0%.

### 2026-07-15 -- direct call target identity closes at the active rung

- Added `direct` to the semantic and self-MIR call-target kind vocabulary.
  Signature capture records canonical direct targets before body analysis;
  constructor and receiver calls are completed by the semantic body fixpoint
  when their required inventories become available.
- Self MIR carries and serializes the direct target row, the MIR importer
  validates it, and hard codegen consumes the carried name. The hard emitter
  no longer reads `callee_text` as final callable identity.
- The focused C/LLVM probe emits the same `ToIntValue(...)` call. Removing only
  the carried direct row fails before emission, and the static gate rejects
  restoring callee-leaf recovery in hard codegen.
- `selfhost.call_target_identity` is now `CLOSED` for the active self-host
  expression rung. This does not close the broader expression surface, run the
  full matrix, or raise released/default substitution above 0%.

### 2026-07-15 -- nominal aggregate call returns consume signature facts

- Replaced the scalar-only resolved-call result query with one canonical
  return-type owner plus an explicitly named concrete-scalar filter. Expression
  verdicts consume the broad return fact; scalar validation consumes the
  filtered capability.
- `MakeBox() -> Box` now projects `box: Box` into self MIR, and hard codegen
  emits `MakeBox()` from the carried target. C-built and LLVM-built probes are
  output-equal.
- A negative keeps the source expression unchanged and changes only its carried
  target to `ToTextValue() -> String`; both backends reject the initializer as
  `let_type_mismatch`. The scalar graph owner is gate-forbidden from indexing
  the signature return array as a second decision path.
- Generic substitution and composite aggregate validation remain in the
  expression bridge. The full matrix was not run and released/default
  substitution remains 0%.

### 2026-07-15 -- exact-formal generic returns consume typed signature rows

- Promoted `Generic params:` from an unknown compact-AST line to a dedicated
  typed HIR node kind. Semantic signature facts now own ordered formal-generic
  starts, counts, and names; expression consumers do not parse provenance.
- Added a bounded generic-call fact for the exact-formal shape
  `Identity<T>(value: T) -> T`. It binds `T` from graph-owned argument types
  and supplies the initializer result without calling the text `ExprType`
  path.
- A C/LLVM executable probe projects `Identity(2)` as `Int`. Keeping source
  unchanged while replacing only the carried target with a String-returning
  function is rejected as `let_type_mismatch` by both backends.
- Nested or composite generic forms, explicit generic actuals, collections,
  and Option/Result remain bridged. The full matrix was not run and
  released/default substitution remains 0%.

### 2026-07-15 -- composite generic returns consume a type-expression arena

- Added `SemanticAstSignatureTypeExpressionFacts`, a signature-owned flat arena
  of nominal type nodes and ordered generic children. Canonical spelling is
  retained for projection, but call semantics consume the structured row.
- Exact-formal argument binding now resolves both `T` and nested return shapes.
  The executable fixture proves `Wrap<T>(value: T) -> Option<T>` becomes
  `Option<Int>` for `Wrap(2)` under C and LLVM.
- The gate forbids `ExprType` in the generic consumer and requires the
  signature owner to carry the return type-expression facts. The existing
  carried-target mutation remains a fail-closed negative.
- Nested generic parameter inference, explicit generic actuals, builtin
  collection/Option/Result policy, and aggregate validation remain bridged.
  The full matrix was not run and released/default substitution remains 0%.

### 2026-07-15 -- nested generic parameters share the signature type arena

- Generalized the return-only type fact into one parameter/return expression
  arena owned by function signatures. No parallel parameter parser or binding
  alias was added.
- `First<T>(values: Array<T>) -> T` now structurally unifies `Array<Int>`, binds
  `T=Int`, and produces an `Int` initializer under C and LLVM.
- The executable negative asks the same owner to bind `Int` against `Array<T>`
  and requires `call_arg_type_mismatch`; exact and nested parameter forms use
  the same binding function.
- Explicit generic actual carriage is the next parser-owner blocker: the
  self-host postfix parser currently skips `<...>` before a call without
  preserving it in the graph. Builtin collection/Option/Result policy and
  aggregate validation also remain bridged. The full matrix was not run.

### 2026-07-15 -- explicit generic actuals survive the parser boundary

- Added parser-owned `generic_type_actual` and `generic_callee` graph nodes.
  Ordered actuals remain separate from compatibility text, and call-target
  resolution unwraps the generic spine to the canonical base callee.
- The semantic call view carries explicit actual rows into the existing
  signature type-expression binding owner. It does not parse `<...>` from the
  call spelling or retain a compact-text fallback.
- The executable positive parses a real `.pgy` source and proves
  `PickSecond<Int, String>(2, "value") -> String`. The negative changes the
  first explicit actual to `String` and requires `call_arg_type_mismatch`.
- The first negative attempt exposed that `AnalyzeCompactBridge` erased the
  parser fact and delayed failure to `let_type_mismatch`; the final gate uses
  `SemanticAstArtifactAnalyzeTyped` so this wrong route cannot count as proof.
  C/LLVM focused parity is required; the full matrix was not run.

### 2026-07-15 -- scalar Option/Result policy is graph-owned

- Initial call-target capture now projects canonical builtin signature names,
  so initializer typing does not wait for a later source-body fixpoint to learn
  that `Some`, `Ok`, `UnwrapOption`, or `UnwrapOr` is a builtin call.
- `SemanticExpressionGraphWrapperValueFactFromGraph` consumes the carried
  target, graph argument handles, and canonical builtin signature rows. It is
  statically forbidden from reopening source `ExprType` or `CheckCall` paths.
- The native C oracle and C/LLVM-built Pergyra probes accept the scalar
  Option/Result cases and reject a non-concrete `None`, a non-wrapper unwrap,
  and a mutated carried target.
- This is a bounded scalar wrapper-policy replacement. Collection policy and
  unknown or aggregate wrapper payload validation remain bridged; the full
  matrix was not run and released/default substitution remains 0%.

### 2026-07-15 -- collection mutation admission has one policy owner

- Moved mutator names, collection type recognition, and parameter-mode policy
  behind `SemanticCollectionMutationError`; source, statement, and graph
  consumers no longer own parallel copies of that decision.
- Specialized array statements consume their typed target and mode facts.
  General mutator calls consume a carried direct target and graph receiver;
  graph call checking is forbidden from replaying the source receiver policy.
- Native C-oracle and C/LLVM-built Pergyra probes accept local and `inout`
  mutation, reject value-parameter mutation, and reject carried-target drift.
- A standalone self-host semantic build exposed and removed a hidden
  transitive `StartsWith` dependency in the wrapper type owner; the owner now
  uses its own bounded prefix slice and the component gate forbids regression.
- Collection result/element typing and aggregate wrapper validation remain
  bridged. The full matrix was not run and released/default substitution
  remains 0%.

### 2026-07-15 -- aggregate field validation consumes graph types

- Added one field type/assignability owner for scalar operators, direct nominal
  returns, nested struct values, `Some(struct)`, wrapper unknowns, and
  structural integer-literal widening.
- Removed `ExprType` and source-text `ExpressionAssignableTo` from the struct
  verdict. Missing graph type facts now fail closed as a field mismatch.
- Native C-oracle and C/LLVM-built probes accept the active aggregate corpus
  and reject an invalid source field, source-preserving graph leaf type drift,
  and a missing child fact.
- The C-built and LLVM-built hard rung-2 drivers pass the complete 20-fixture
  body set plus the selected `option_struct_value_flow` MIR producer fixture.
  The body bundle keeps semantic-negative rows structurally consumable so the
  original diagnostic remains the first verdict; missing structure still
  fails closed.
- Generic/member aggregate field values and broader object initializer policy
  remain bridged. The remaining 27 MIR fixtures and broader integration matrix
  were not run; released/default substitution remains 0%.

### 2026-07-15 -- explicit generic aggregate values reach hard DRV-2

- Added `generic_struct_field_value_flow.pgy` as the twenty-ninth MIR fixture.
  Routine generic formals, ordered explicit actuals, and canonical
  `Identity<Int>` expression text survive native MIR, self MIR, and
  canonicalization.
- Removed the competing semantic top-level operator reconstruction from the
  compact expression bridge. Canonical prefix-try remains an explicit
  normalization; all other supported topology now comes from the parser graph.
- Aggregate verdicts consume graph-owned intrinsic and generic return types
  before legacy text inference. Codegen emits `Identity_Int` and does not emit
  an unspecialized `Identity` template.
- C-built and LLVM-built hard drivers pass the full 20-source battery and the
  selected generic MIR fixture. Mutating away `generics:["T"]` produces
  `generic_argument_count_mismatch`; corrupting `generic_type_actual` is
  rejected as an invalid MIR expression graph.
- This closes only explicit top-level actuals used by aggregate fields.
  Inferred actuals, member generic calls, nested generic locals, the remaining
  28 MIR fixtures, and released/default substitution remain open.

### 2026-07-15 -- inferred generic initializer actuals become semantic facts

- Added `generic_struct_field_inferred_value_flow.pgy` as the thirtieth MIR
  fixture. Its `Identity(41)` and `Identity(1)` calls contain no explicit
  generic actual syntax.
- Added `SemanticAstGenericSpecializationFacts` as the call-node keyed owner of
  resolved signature indexes and ordered actual type names. The owner derives
  inferred direct-call actuals under local initializer roots from typed graph
  and local-environment facts and fails closed when such a call escapes the
  bounded producer coverage.
- Repointed codegen specialization and direct-call emission to that semantic
  owner. Codegen no longer scans expression-graph nodes or reconstructs a
  specialization name from the direct callee spelling.
- C-built and LLVM-built hard drivers pass the full 20-source battery and the
  selected inferred-generic MIR fixture. Both emit and call `Identity_Int`.
  Mutating only the graph argument type while preserving source text fails as
  `call_arg_type_mismatch`.
- Component, hard-substitution, and owner-size contracts pass. This closes only
  inferred direct generic calls under local initializer roots. Return- and
  assignment-rooted inferred calls, member generic calls, nested generic
  locals, the other 29 MIR fixtures, and released/default substitution remain
  open.

### 2026-07-16 -- try-let consumes graph-owned operand types

- Repointed Option try-let lowering from `ExprKind(operand_text)` to
  `CodegenExpressionTypeFromGraph(graph, operand_node, env)`.
- Removed the operand-text read from `EmitTryLet`; missing graph type evidence
  now fails closed with statement provenance.
- Tightened the component contract so `EmitTryLet` cannot reintroduce either
  `ExprKind` or `SemanticExpressionGraphNodeText`.
- The C-built self-host codegen remains run-output equal across all 73
  fixtures, including `option_try`. This closes one live result-type consumer,
  not the broader legacy expression-shape classification bridge.

### 2026-07-16 -- array returns consume expected-value graphs

- Repointed `Array<T>` return emission to
  `RewriteExpectedValueWithSemanticGraph` for both literals and ordinary
  array-valued expressions.
- Removed the return-text trim, leading-bracket classification, legacy
  `EmitArrayLiteralValue`, and array-specific `RewriteExpr` branch from the
  live return owner.
- Added `array_return_literal` beside the existing `array_param` fixture so the
  C oracle and Pergyra codegen compare both graph forms. The component contract
  rejects restoration of the four text-owned reads.
- This is one executable codegen consumer migration. Result return rewriting
  and broader legacy expression leaves remain bridged; released/default
  substitution remains 0%.

### 2026-07-16 -- Result returns consume expected-value graphs

- Repointed `Result<Int>` return emission from the legacy
  `RewriteExpr(rexpr, env)` scanner to the expected-value semantic graph.
- Existing `result_int_core` and `result_try` fixtures exercise direct
  `Ok`/`Err` calls and a pipe-bearing `Ok(...)` return without adding a
  documentation-only surface.
- Added `result_int_core` to the 31-fixture DRV-2 MIR producer frontier. Its
  source/MIR consumer path inherits the mandatory missing-graph and invalid-root
  mutations.
- Closed the assignment target graph transport exposed by that DRV-2 run:
  plain targets carry a leaf graph, indexed targets carry an index graph, and
  the consumer reads target-before-RHS in semantic lane order.
- Corrected the expression-binding readiness fixture to compare the canonical
  `x + 1` spelling. It still proves missing and malformed graphs fail closed.
- Rebound carried direct-call targets against nominal-constructor inventory
  facts as well as function and builtin signatures. `Pair(...)` no longer
  fails as an unknown call target, and no expression-text recovery was added.
- Removed the semantic shortcut that trusted an already-carried call target.
  Namespace targets are re-derived from qualified callable inventory and
  member targets from receiver type plus method signature. A carried mismatch
  now fails closed rather than being silently replaced.
- Verified the complete C DRV-2 frontier: 20 source fixtures and 32 MIR
  producer fixtures pass producer-first parity with these checks enabled.
- The component contract rejects restoring the return-expression scanner.
  Other legacy expression leaves remain bridged; released/default substitution
  remains 0%.

### 2026-07-18 -- canonical self MIR consumes carried expression graphs

- Repointed `CanonicalizeMirJsonVerified` from
  `SemanticAstArtifactAnalyzeCompactBridge` to the graph decoded by
  `MirExpressionGraphFactsForArtifact` and consumed by
  `SemanticAstArtifactAnalyzeWithExpressionGraph`.
- Missing and invalid expression graphs now fail closed on the canonicalization
  path. The focused `result_int_core` C parity gate checks both mutations in
  addition to byte-equal canonical MIR.
- Split graph-less native C-oracle compatibility into the explicit
  `--canonicalize-oracle-mir-json` command. The strict
  `--canonicalize-mir-json` path cannot invoke that bridge, and the hard
  contract rejects its reintroduction.
- This replaces one live self-MIR consumer. Native MIR graph production and
  other compact expression leaves remain bridged, so released/default
  substitution remains 0%.

### 2026-07-18 -- Long literal identity is parser-owned

- Added `long_literal` as a typed expression-graph kind. The self-host parser
  now preserves both the source `L` spelling and the graph identity instead of
  letting expected-type context recover `Long` downstream.
- Semantic scalar typing, MIR JSON kind projection, MIR JSON consumption, and
  C emission consume that graph kind. A damaged kind fails closed before
  codegen can guess from the literal text.
- Corrected the native oracle's inline AST printer to serialize Long values as
  decimal integer text with `L`; `%g` had changed `42000000000L` into
  `4.2e+10L`, which is not a lossless compiler artifact.
- Added `long_scalar` to the 33-fixture DRV-2 producer frontier with a
  `42000000000L` execution case, a Long-kind negative mutation, and a malformed
  Long-payload negative mutation.
- Repaired the initializer-projection negative to mutate
  `integer_literal("2")` into an identifier leaf. The previous leaf-only
  search no longer damaged the graph after integer literal identity landed.
- Closed the aggregate-field widening seam found by exhaustive CI: `Int`
  literal to `Long` field compatibility now consumes the parser-owned
  `integer_literal` kind, including `negate(integer_literal)`, and no longer
  reclassifies leaf text with `IsIntLiteral`. Aggregate and generic-field
  negatives now mutate both kind and payload so they attack the live owner.

### 2026-07-18 -- Bool literal identity is parser-owned

- Added `bool_literal` as a typed expression-graph kind. The self-host parser
  now distinguishes `true` and `false` from identifier leaves at construction
  time and validates that the carried payload is one of those two spellings.
- Semantic scalar typing, codegen type projection, MIR JSON kind projection,
  MIR JSON consumption, and C emission consume the Bool kind. The former leaf
  text checks in semantic typing and `RewriteSemanticLeaf` were deleted and
  are rejected by the component contract.
- Reused the existing `if_else_assign` DRV-2 fixture. Changing its
  `bool_literal("false")` to `leaf("false")` fails with the structured
  `statement_type_unresolved` diagnostic; changing the payload to `truth`
  fails graph verification before emission.
- Focused DRV-2 C/LLVM producer-first parity passed, parser C/LLVM output stayed
  byte-equal for 188 sources, and codegen C/LLVM run parity passed for all 75
  fixtures. Bootstrap reached `gen2 == gen3` at 32,927 generated C lines and
  retained oracle equivalence for lexer, parser, semantic, MIR lower, tools,
  and the fuzz backend parity generator.
- This closes one executable typed-expression seam. Namespace-qualified call
  classification, object-init internals, remaining literal-only argument
  bridges, special unary forms, result-type classification, and initial arena
  construction remain active; released/default substitution remains 0%.

### 2026-07-18 -- String literal identity is parser-owned

- Added `string_literal` as a typed expression-graph kind. Plain quoted
  strings and interpolation literal segments now enter the arena through
  `ParserExpressionStringLiteral`; only interpolation value expressions remain
  ordinary expression subtrees.
- Split scalar literal construction from the canonical expression result into
  `expression_scalar_fact_owner.pgy` after the previous owner crossed its
  100-line responsibility cap. The graph owner imports the scalar owner, which
  in turn imports the common fact owner; no duplicate fact struct was added.
- Semantic scalar typing, codegen type projection, MIR JSON kind projection,
  MIR JSON consumption, and C emission consume `string_literal`. The former
  first-character quote checks in semantic and codegen leaf consumers were
  deleted and are rejected by the component contract.
- Reused the existing `str_array` DRV-2 fixture. Changing the carried `"BOB"`
  node from `string_literal` to `leaf` fails at the hard emitter, and removing
  its quotes while retaining the kind fails graph verification. The existing
  collection-mutation gate now requires the parser-owned String kind.
- Focused DRV-2 C/LLVM parity passed, parser output stayed byte-equal for 188
  sources, semantic C/LLVM verdict parity passed for 111 fixtures, and codegen
  C/LLVM run parity passed for 75 fixtures. Bootstrap reached
  `gen2 == gen3` at 32,977 generated C lines and retained full oracle breadth.
- A native Linux WSL reproduction found that validating every String byte with
  `CodegenCharAt` during repeated arena verification grew the parser producer
  to 32,091,392 KiB RSS and caused hosted-runner shutdowns. The validator now
  uses allocation-free `CodegenCharCodeAt`; the same fixed-point run completed
  in 5:04 with 803,180 KiB peak RSS and `gen2 == gen3` at 32,977 lines. The
  component contract rejects reintroducing the allocating scan.
- This replaces one live literal argument bridge. Other literal-only bridges,
  namespace-qualified call classification, object-init internals, special
  unary forms, result-type classification, and initial arena construction
  remain active; released/default substitution remains 0%.

### 2026-07-19 -- Residual assignment graphs enter the hard MIR path

- Native `MIR_INST_ASSIGN` JSON now carries the target as `expr0_graph` and the
  assigned value as `expr1_graph`. Self-produced SSA assignment definitions
  keep their existing value/target physical lane order.
- The Pergyra MIR expression owner now selects graph requirements and semantic
  ordering from the instruction kind. Both physical forms become one
  target-before-value sequence; neither graph is reconstructed from text.
- Added `array_index_assign` as DRV-2 MIR fixture 35. Its native artifact uses
  strict `--canonicalize-mir-json`, bypassing the named oracle bridge, and the
  native MIR hard consumer emits byte-identical C to the self-produced path.
- Focused C/LLVM parity passed 20 body fixtures plus this one MIR fixture and
  matched C-oracle execution. Removing either native graph fails closed with
  the structured MIR graph diagnostic. The complete 35-fixture MIR matrix was
  not rerun for this slice.

### 2026-07-19 -- Array initializer typing consumes the expression graph

- Moved `SemanticArrayLiteralView` from the codegen participant into
  `ast_expression_graph_array_literal_owner.pgy`, making semantic typing and
  emission consume one ordered array-spine projection.
- Typed `let` initializers now check declared array element types by walking
  graph handles. The initializer owner cannot trim brackets, split arguments,
  or call `SemanticProjectionArrayLiteralMatchesDeclaredType`.
- Focused C/LLVM DRV-2 parity for `ast_node_array_literal` matched canonical
  MIR, emitted C, and runtime output. Existing array graph negatives still
  reject missing or invalid roots. Assignment and return consumers were still
  open at this point and were closed by the subsequent executable deltas.

### 2026-07-19 -- Collection statement typing consumes parser graph lanes

- Split collection statement graph construction into
  `stmt_collection_graph_owner.pgy`. `ArrayPush` contributes its value root;
  `ArraySet` contributes index/value roots in value/auxiliary lanes.
- Statement typing now consumes those roots through
  `SemanticAstExpressionVerdictFromGraph`. The former index/value calls to
  `SemanticProjectionExpressionType` were deleted and are rejected by the
  component contract.
- MIR carries `ArraySet` value in `expr0_graph` and index in `expr1_graph`.
  The secondary graph attach API is no longer assignment-named, and MIR
  validation requires both the index payload and graph.
- A semantic mutation preserves source spelling `0` but changes its graph kind
  from `integer_literal` to `leaf`; the type checker rejects it instead of
  recovering `Int` from text. A second mutation removes `expr1_graph` and the
  hard MIR consumer fails closed.
- Focused C-built and LLVM-built DRV-2 parity passed all 20 body fixtures plus
  the filtered `ast_node_array_set` MIR fixture. Parser C/LLVM output remained
  byte-equal across 188 sources. This closes one collection-statement type
  seam, not released/default or whole-compiler substitution.

### 2026-07-19 -- Assignment typing consumes target and value graphs

- Assignment semantic typing now obtains the writable binding name from the
  target graph and types the graph root, or the left child of an indexed root,
  with `SemanticExpressionGraphScalarTypeName`.
- Member and indexed target types are no longer recovered by
  `SemanticProjectionExpressionType`. The same legacy fallback was removed
  from RHS scalar typing because the value verdict already consumes the value
  graph.
- The negative contract preserves source text `box.value` but changes the
  member-name child node to `missing`. C- and LLVM-built semantic checkers both
  reject the row with `assignment_type_unresolved`; a text fallback would have
  accepted it.
- Focused C/LLVM DRV-2 parity passed 20 body fixtures plus the filtered
  `indexed_assignment` MIR fixture. Existing missing-target-graph rejection
  remained green. This is one assignment-type authority closure, not a claim
  of released/default or whole-compiler self-host completion.

### 2026-07-19 -- Match scrutinee typing consumes the parser graph

- `ParseMatchStmt` now parses the scrutinee as a `ParserExpressionFact` and
  attaches it to the Match statement's Atom lane without changing AST text.
- Statement typing requires that graph and treats an unresolved match
  scrutinee as `statement_type_unresolved`. Its final
  `SemanticProjectionExpressionType` fallback and import were deleted and are
  blocked by the component contract.
- A graph-only mutation keeps `box.value` as the visible expression while
  changing the member-name child to `missing`; the C- and LLVM-built semantic
  checkers both reject it.
- Parser C/LLVM parity remained byte-equal across 188 sources, and semantic
  C/LLVM parity passed 111 fixtures. `match_case_int` is not in the 37-row
  DRV-2 producer frontier, so self-produced MIR match substitution remains
  unclaimed.

### 2026-07-19 -- Initializer typing retires the projection owner

- Initializer member/index/call result typing now relies on
  `SemanticAstExpressionVerdictFromGraph`; its post-verdict
  `SemanticProjectionExpressionType` fallback and import were removed.
- A graph-only mutation leaves `box.value` unchanged as source/root spelling
  but changes its field child to `missing`. C- and LLVM-built semantic checkers
  reject it with `initializer_type_unresolved`.
- Focused C/LLVM DRV-2 parity passed 20 body fixtures plus
  `nested_member_access`, matching canonical MIR, emitted C, and execution.
- No consumer remained for `projection_type_owner.pgy`, so the file was deleted
  rather than retained as an alias. The component contract rejects recreating
  it. Other expression bridges and whole-compiler substitution remain open.

### 2026-07-19 -- Bounded Option match carries variant and binding facts

- Parser match cases now retain their pattern as a non-executable expression
  graph lane. `AstMatchCasePatternFact` is the one HIR projection for scalar
  integer, `Some(binding)`, and `None` patterns; semantic and MIR consumers do
  not reparse compact AST or source text.
- The semantic case environment consumes that fact and the typed match-subject
  graph. A `Some(v)` arm exposes `v` as the payload of a leaf `Option<T>`
  subject only inside the enclosing case; malformed patterns or an unresolved
  subject fail closed.
- Added `option_match` as DRV-2 MIR fixture 40. The Pergyra producer carries
  `Some`/`None` variant rows and the `Some` binding row, while the final MIR
  consumer derives `IsSome(subject)`, `!IsSome(subject)`, and the bounded
  `UnwrapOption(subject)` initializer from those facts.
- Focused C-built and LLVM-built drivers matched canonical MIR, emitted C, and
  runtime output. Removing the carried `Some` binding is rejected by the final
  consumer. Parser C/LLVM output also remained byte-equal across 188 sources.
- This closes only leaf-subject `Option<T>` matching with one `Some` binding
  and zero `None` bindings. Enum variants, complex scrutinees, multiple
  bindings, and released/default compiler replacement remain open.

### 2026-07-26 -- Admitted MIR structure is reused by routine lowering

- `MirProgramRoutineIndex` now captures one routine/block/instruction
  partition plus instruction kind/source type and raw machine spans from the
  admitted `pgy.mir.v1` artifact. Machine admission and
  `MirRoutineFactIndex` consume that view rather than reopening nested arrays.
- Full structure readiness is proved once at admission. Per-routine fact
  construction uses an O(1) row guard, and the component contract rejects the
  accidentally repeated whole-program validator.
- The C/LLVM structure gate rejects malformed scalar tails, missing required
  structure, corrupted counts, and invalid row access. The integrated C driver
  builds below 3 GiB and preserves the 414-byte bounded output.
- The 300-second full-artifact run remains RED at the 16-routine marker with
  85.2 MB peak private and no gen2 output. Direct instruction kind/source type
  and machine-span consumers remain, so `mir.execution_graph` stays `BRIDGE`
  and released/default substitution is unchanged.

### 2026-07-26 -- MIR instruction-view CPU seam and phi inventory

- `06f6994d` makes routine lowering consume typed instruction and canonical CFG
  views from the admitted program index. ABI/resource common paths inspect
  exact bounds without repeatedly constructing and validating an instruction
  fact table against the 51.8 MB-backed source view.
- The measured ABI step fell from 492 ms to 9 ms, resource validation from
  646 ms to 0 ms, and routine 16 from 133,593 ms to 69,919 ms. A `ref`-only
  accessor change was measured first and did not improve the run.
- Phi `uses` now follows the producer-owned incoming-value inventory contract:
  `2 <= use_count <= predecessor_count`, with a self-result accepted only for a
  CFG-proven backedge. The `FindTopLevelComma` seven-predecessor/two-value
  counterexample then passed.
- CFG successor identity is stored once as integer block IDs. Missing edges use
  the internal sentinel; explicit negative wire targets fail closed in the
  C/LLVM executable structure gate.
- The final integrated v14 C driver built below 3 GiB and preserved the bounded
  414-byte output with SHA-256
  `0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`.
  The v13 full run reached routine 128 at 164,457 ms with only 88.6 MB peak
  private, then timed out without `mir-to-ast:done`.
- This does not complete gen2, self-hosting, or the bootstrap fixed point; no
  complete `driver_gen2.c` was emitted.

### 2026-07-26 -- Routine-local scalar facts replace repeated instruction reads

- `dd68d6f3` adds one `MirRoutineInstructionFactBundle` per routine fact-index
  construction. It captures
  `result`, `expr0`, `expr1`, `arg0`, `arg1`, `slot_anchor`, `abi_type_name`,
  and `match_variant` from the program-owned instruction spans in one strict
  pass. The program index remains structure/identity-only rather than mixing
  global and routine-local evidence lifetimes.
- Render, match, expression-graph, assignment, local-type, and phi consumers
  read that bundle. Duplicate or non-string scalar values fail closed. The
  builder also verifies that a block's instruction range ends at the next
  program-owned block boundary, preventing a corrupted count from reading the
  next routine.
- The active MIR-to-AST reconstruction reuses that bundle. The later
  expression-graph and assignment post-passes still reconstruct routine
  indexes, so cross-pass capture is not yet closed.
- Phi predecessor count is computed lazily only after a phi is encountered.
  The incoming-backedge answer now comes from `MirRoutineFactIndex` instead of
  replaying a dominator query. A current-driver `nested_if_in_loop` MIR round
  trip passed; injecting a one-predecessor header phi exited 1 with
  `MIR phi facts are missing or inconsistent`.
- Generated-C inspection corrected the previous CPU diagnosis. A Pergyra
  `String` is emitted as `char *`, and an instruction fact table stores that
  source pointer plus bounds; it did not deep-copy the complete 51.8 MB payload
  per table. The measured cost was repeated object validation and field/bound
  discovery over the same source view.
- The v23 integrated C driver built in 47,746 ms at 2,509.8 MB peak private /
  2,498.5 MB working set and preserved the exact 414-byte bounded SHA-256
  `0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`.
  The full 180-second run used only 87.0 MB peak private / 95.3 MB working set
  and reached routine 128 at 160,331 ms, 4,688 ms earlier than the v14
  300-second run's routine-128 marker. It still timed out and opened no gen2
  artifact.
- The broad DRV-2 body attempt remains RED before its MIR fixture phase because
  `valid_array_builtins` emitted C lacks `<string.h>` and current runtime panic
  declarations. This is recorded separately from the focused phi result and
  is not relabeled green.

### 2026-07-26 -- Required ABI-layout validation captures each row once

- v29-v37 pressure instrumentation narrowed the current full-consumer stall to
  required ABI layout rows. Array rows cost about 1.35 seconds and Option rows
  about 1.09 seconds because the nested layout object and field array were
  repeatedly rediscovered and rehashed.
- The v38 outer-bound-only experiment preserved those costs and therefore
  served as a falsifying result: routine 248 regressed from 290,268 ms to
  293,877 ms. Moving validation between markers is not accepted as a speedup.
- `a5d56f42` makes the routine scalar owner carry only raw ABI value spans. The
  ABI owner interprets them, captures the nested row and field rows once, and
  computes the canonical ID from that capture. The producer compatibility API
  now uses the same identity implementation; the old repeated-scan hash path
  is deleted rather than retained as a fallback.
- The C/LLVM focused gate admits the known `Array<Int>` ID `599770891`, rejects
  wrong-kind and duplicate outer tuples, and rejects truncated parallel bounds.
  The component, ABI ownership, protocol registry, and Gate SoT checks pass.
- The v39 full run stayed at 134.7 MB peak private / 140.8 MB working set and
  moved routine 192 from v38's 233,517 ms to 102,775 ms. It reached routine 640
  at 298,374 ms instead of ending near routine 248.
- The exact final-source v40 driver built in 55,007 ms at 2,565.3 MB peak
  private / 2,554.5 MB working set and preserved the 414-byte bounded SHA-256
  `0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`.
  A bounded ABI-ID mutation exits 1 with the owned diagnostic.
- This is executable replacement progress, not self-host completion. The run
  still timed out before `consumer:mir-to-ast:done`; no complete
  `driver_gen2.c`, gen2 binary, or gen3 comparison exists. The next falsifier
  is routine 704 under the same 300-second/3072 MB gate.

### 2026-07-26 -- Exact ABI validation witnesses survive one program run

- The v39 full-input census found 580 required ABI rows before routine 640 but
  only five exact tuples. The complete input contains 2,504 required rows and
  seven tuples. The repeated work was successful validation of identical facts,
  not missing semantic evidence.
- `0da9c5c2` adds a program-lifetime validation session inside the existing ABI
  owner. A required hit needs the exact raw type, canonical decimal ID,
  required state, and full raw layout payload. Only a fully captured and hashed
  row is remembered; ID-only hits and cross-run reuse remain forbidden.
- The focused C/LLVM gate admits a reordered but semantically equal layout by a
  full cache-miss validation and rejects the same ID after a nested offset
  mutation. Existing wrong-kind, duplicate, wrong-ID, and truncated-bounds
  failures remain closed under the same ABI diagnostic.
- The exact-source v41 driver built in 52,722 ms at 2,346.8 MB peak private /
  2,336.6 MB working set. Its 1,251 ms bounded run remained exactly 414 bytes
  with SHA-256
  `0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`.
- The fixed 300-second full run reached routine 640 at 228,455 ms, routine 704
  at 238,884 ms, and routine 896 at 288,574 ms. It timed out at 300,227 ms with
  157.2 MB peak private / 162.3 MB working set and no cap crossing.
- This clears the previous routine-704 falsifier but does not complete
  bootstrap. `consumer:mir-to-ast:done`, a complete `driver_gen2.c`, gen2
  compilation, and gen3 comparison are still absent. The next executable
  falsifier is routine 960 under the unchanged 300-second/3072 MB gate.

### 2026-07-26 -- Optional ABI rows reuse the routine scalar observation

- The v41 interval census showed elapsed time was explained by instruction
  volume rather than required-ABI volume. The common optional ABI path was
  still decoding a wire value that the routine scalar pass had already read.
- `bf8a56b8` carries `abi_type_value_ready` with the decoded type name. The bit
  records only a valid string or exact optional `null`; ABI semantics remain in
  `abi_layout_fact_owner.pgy`. Optional rows require exact raw `id=0` and
  `layout=null`, while required rows retain exact-tuple witness reuse and full
  canonical validation.
- The focused C/LLVM structure gate admits valid optional/required rows and
  rejects wrong type kind, wrong ID, duplicate fields, changed required layout,
  and truncated bounds with the existing ABI diagnostic. No backend split,
  ID-only authority, or second cache was added.
- The exact-source v42 driver built in 53,265 ms at 2,515.0 MB peak private /
  2,503.6 MB working set. Its 1,433 ms bounded run remained exactly 414 bytes
  with SHA-256
  `0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`.
  The wrong-ABI negative exited 1 in 551 ms with no output.
- The fixed 300-second run reached routine 704 at 162,849 ms, routine 896 at
  192,157 ms, routine 1,600 at 241,729 ms, and routine 1,920 at 293,147 ms. It
  timed out at 300,115 ms with 214.4 MB peak private / 216.6 MB working set and
  no cap crossing. Routines 704 and 896 were 76,035 and 96,417 ms earlier than
  v41, and the same window advanced 1,024 routines farther.
- This is executable CPU progress, not bootstrap completion. The historic
  3 GiB-class defect came from repeated whole-program graph/readiness
  validation that admission should perform once; it is recorded separately
  from the current low-memory timeout. There is still no
  `consumer:mir-to-ast:done`, complete `driver_gen2.c`, gen2 binary, or gen3
  comparison. The next fixed-window falsifier is routine 1,984.

### 2026-07-26 -- Scalar key dispatch narrows comparisons without moving facts

- `dfc8e406` keeps scalar capture in the existing routine-local owner. Plain
  JSON keys are dispatched by raw length before semantic comparison; a key with
  an escape still takes the complete comparison path. The focused C/LLVM gate
  proves all eleven target keys, same-length non-target rejection, escaped-key
  equivalence, and plain-plus-escaped duplicate failure.
- This change adds no helper, carrier, cache, ABI decision, or C/LLVM-specific
  path. The component ratchet requires the length guards and escaped fallback.
- The exact-source v43 driver built in 52,451 ms at 2,523.0 MB peak private /
  2,511.6 MB working set. Its 1,454 ms bounded run remained 414 bytes with
  SHA-256
  `0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`.
  The wrong-ABI input exited 1 with the owned diagnostic and no output.
- The fixed 300-second run reached routine 704 at 162,255 ms, routine 896 at
  190,875 ms, routine 1,600 at 239,277 ms, and routine 1,920 at 290,054 ms. It
  timed out at 300,268 ms with 215.1 MB peak private / 217.1 MB working set and
  no cap crossing.
- Routine 1,920 is 3,093 ms (1.06%) earlier than v42. This is a safe minor
  improvement, not the dominant CPU closure: routine 1,984, MIR-to-AST
  completion, gen2 output, compilation, and gen3 comparison remain absent. The
  next active seam is the existing CFG owner's per-edge backedge BFS, to be
  replaced by one routine-level result without adding another graph or cache.

### 2026-07-26 -- Routine CFG owner batches backedge admission

- `73133678` moves backedge-header classification behind one routine-level
  result in the existing `mir_cfg_graph_owner.pgy`. Entry reachability is
  computed once, avoiding reachability once per reachable distinct incoming
  target, and source edges are checked target-major. The old per-edge function
  is deleted and a component ratchet forbids its return.
- Malformed successor arrays/targets return an empty typed result. The
  fact-index consumer reports `cfg_backedge` on a nonempty routine instead of
  treating the malformed graph as a valid all-zero backedge set. `ec4b9eef`
  covers that consumer failure. C/LLVM fixtures preserve disconnected-cycle,
  self-loop, ordinary-loop, duplicate-target, earlier-merge, diamond, tie, and
  fallback behavior.
- The exact-source v44 driver built in 52,316 ms at 2,433.5 MB peak private /
  2,427.0 MB working set. Its 1,425 ms bounded result remained 414 bytes with
  SHA-256
  `0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`.
  The wrong-ABI input exited 1 with the owned diagnostic and no output.
- The fixed 300-second run reached routine 704 at 162,403 ms, routine 896 at
  191,236 ms, routine 1,600 at 240,535 ms, and routine 1,920 at 291,308 ms. It
  timed out at 300,682 ms with 202.7 MB peak private / 205.0 MB working set and
  no cap crossing.
- Although the static tail backedge BFS count falls from 9,144 to 4,128, the
  shared routine-1,920 marker is 1,254 ms (0.43%) later than v43. This is an
  owner/fallback closure but a CPU negative/noise result. Routine 1,984,
  MIR-to-AST completion, gen2 output, compilation, and gen3 comparison remain
  absent; structural-merge/phi caching is not authorized by this result.

### 2026-07-26 -- Routine facts carry each block's unique branch row

- `4ee29ce2` makes the existing routine-local
  `MirRoutineInstructionFactBundle` scalar pass record the unique branch global
  row for each block. Duplicate branches invalidate the bundle. No
  program-global scalar aggregate, new cache, JSON fallback, or backend split
  was introduced.
- `BlockCond`, `BlockHasLoopTransfer`, and `BlockMatchBindingLine` consume the
  carried row instead of calling `MirRoutineInstructionViewOfKind` for every
  block. The owner distinguishes valid missing from invalid data and rejects an
  out-of-block row, scalar-span mismatch, or program-owned kind other than
  `branch`. The component ratchet forbids the deleted routine-lowering search.
- The focused C/LLVM routine-index fixture covers missing, unique, duplicate,
  and forged non-branch rows. The routine-index parity and self-host component
  contract gates pass.
- The exact-source v45 driver built in 52,025 ms at 2,534.1 MB peak private /
  2,522.6 MB working set. Its 1,487 ms bounded result remained 414 bytes with
  SHA-256
  `0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`.
  The wrong-ABI input exited 1 with the owned diagnostic and no output.
- The fixed 300-second run reached routine 1,920 at 288,324 ms and, for the
  first time, routine 1,984 at 298,381 ms. It timed out at 300,345 ms with
  204.8 MB peak private / 206.9 MB working set and no cap crossing. The shared
  routine-1,920 marker is 2,984 ms (1.02%) earlier than v44.
- This is executable progress, not self-host completion. Routine 2,048,
  `consumer:mir-to-ast:done`, complete `driver_gen2.c`, gen2 compilation, and
  gen3 comparison remain absent. The next fixed-window falsifier is routine
  2,048 under the unchanged 300-second/3,072 MB gate.

### 2026-07-26 -- Routine facts carry each block's phi prefix

- `99e76e76` makes the existing `MirRoutineInstructionFactBundle` scalar pass
  record each block's leading phi count. A phi after the first non-phi records
  a negative late-phi sentinel. Missing or invalid prefix facts fail closed;
  they do not trigger a whole-block or JSON fallback.
- `MirRoutinePhiFactsReady` now reconstructs only prefix rows and still checks
  their program-owned `kind=phi`, predecessor count, arity, result identity,
  incoming uses, and CFG-owned backedge evidence. The old instruction-count
  loop and `seen_non_phi` consumer scan are statically rejected. No new cache,
  program-global scalar aggregate, or C/LLVM split was added.
- The C/LLVM routine-index fixture admits `[phi, phi, nonphi]` with prefix count
  two and rejects `[nonphi, phi]`. The routine-index parity and self-host
  component contract gates pass.
- The exact-source v46 driver built in 52,507 ms at 2,556.9 MB peak private /
  2,546.0 MB working set. Its 1,442 ms bounded result remained 414 bytes with
  SHA-256
  `0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`.
  The wrong-ABI input exited 1 with the owned diagnostic and no output.
- The fixed 300-second run reached routine 1,920 at 293,716 ms and timed out at
  300,163 ms with 202.1 MB peak private / 204.3 MB working set. It did not
  recover v45's routine-1,984 marker. The shared routine-1,920 marker is 5,392
  ms (1.87%) later than v45 despite reducing full-artifact phi view
  reconstruction from 34,091 rows to 3,532.
- This is an owner/fallback closure and CPU negative/noise result, not a
  speedup or self-host completion. Routine 2,048, MIR-to-AST completion, gen2
  output and compilation, and gen3 comparison remain absent. Do not rerun v46
  for a favorable sample or raise the fixed 300-second/3,072 MB gate.

### 2026-07-26 -- Phi-prefix admission moves to the routine boundary

- The v46 prefix accessor repeated program-row and bundle-shape admission once
  per block even though `BuildMirRoutineFactIndex` had already built one
  routine-local bundle. Across the full artifact that meant 20,022 admissions
  for 2,345 routines: 17,677 redundant admissions and at least 406,571
  redundant array-length checks.
- `a05aaf06` makes `MirRoutinePhiFactsReady` admit routine identity, exact block
  counts, and `MirRoutineInstructionFactBundleReady` once at entry. The block
  loop directly reads the carried prefix and rejects negative or oversized
  counts. The one-use prefix accessor is deleted, not retained as C-style
  helper fragmentation or a compatibility fallback.
- The existing leading/late phi C/LLVM cases remain green. A truncated prefix
  array now fails bundle admission, and the component ratchet requires exactly
  one bundle-ready call in the phi owner while forbidding the deleted helper
  and old all-instruction scan.
- The exact-source v47 driver built in 51,436 ms at 2,535.7 MB peak private /
  2,524.3 MB working set. Its 1,410 ms bounded result remained 414 bytes with
  SHA-256
  `0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`.
  The wrong-ABI input exited 1 with the owned diagnostic and no output.
- The fixed 300-second run reached routine 1,920 at 283,594 ms and routine
  1,984 at 293,201 ms. It timed out at 300,384 ms with 207.7 MB peak private /
  209.7 MB working set and no cap crossing. Routine 1,920 is 10,122 ms earlier
  than v46 and 4,730 ms earlier than v45; routine 1,984 is 5,180 ms earlier than
  v45.
- This is executable CPU progress, not self-host completion. Routine 2,048,
  `consumer:mir-to-ast:done`, complete `driver_gen2.c`, gen2 compilation, and
  gen3 comparison remain absent. The next fixed-window falsifier is routine
  2,048 under the unchanged 300-second/3,072 MB gate.

### 2026-07-26 -- Branch selection consumes admitted routine facts

- `8074d6c8` keeps branch global-row ownership in the existing routine-local
  bundle but moves selection to `MirRoutineFactIndexBranchAtBlock`. The new
  boundary validates admitted index state, routine/block identity, local/global
  instruction range, carried start/end span equality, and final program-owned
  branch kind without repeating full program-row or bundle admission.
- The old bundle accessor is deleted, and `BlockCond`,
  `BlockMatchBindingLine`, and `BlockHasLoopTransfer` use only the index owner.
  Missing uses the exact negative sentinel; other negative, out-of-block,
  forged-kind, and inconsistent rows fail closed. No scan, JSON fallback,
  cache/global aggregate, or backend split was added.
- Existing missing/valid/duplicate/forged-kind C/LLVM cases remain green, and
  an out-of-block carried row is rejected. The component ratchet forbids the
  deleted helper and full re-admission in the new accessor. The index owner
  remains at its 600-line cap.
- The exact-source v48 driver built in 51,479 ms at 2,567.8 MB peak private /
  2,557.0 MB working set. Its 1,513 ms bounded result remained 414 bytes with
  SHA-256
  `0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`.
  The wrong-ABI input exited 1 with the owned diagnostic and no output.
- The fixed run reached routine 1,920 at 285,333 ms and routine 1,984 at
  295,075 ms. It timed out at 300,615 ms with 206.3 MB peak private / 208.3 MB
  working set and no cap crossing. Those markers are 1,739 and 1,874 ms later
  than v47, so the fixed-window result is CPU negative/noise.
- The owner/fallback closure remains, but this is not a speedup or self-host
  completion. Routine 2,048, MIR-to-AST completion, gen2 output and
  compilation, and gen3 comparison remain absent. Do not rerun v48 for a
  favorable sample or enlarge the 300-second/3,072 MB gate.

### 2026-07-26 -- Direct block traversal is rejected and reverted

- `80a54268` made `EmitBlockStmts` admit one block-local slice and directly
  construct instruction/scalar values. The focused C/LLVM cross-block negative
  and component ratchet passed, and the static model removed about 1,202,928
  repeated shape checks. No fallback or backend split was introduced.
- The exact-source v49 driver built in 60,860 ms at 2,587.7 MB peak private /
  2,578.1 MB working set, materially slower than v48's 51,479 ms build. Its
  1,463 ms bounded result remained 414 bytes with SHA-256
  `0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`.
  The wrong-ABI input exited 1 with the owned diagnostic and no output.
- The fixed run reached routine 1,920 at 293,502 ms and timed out at 300,269 ms
  with 202.3 MB peak private / 205.0 MB working set. It was 8,169 ms (2.86%)
  later than v48 and did not reach routine 1,984, 2,048, MIR-to-AST completion,
  or gen2 output.
- Because the regression is material and attributable to the changed generated
  path rather than memory pressure, `85cee4ff` explicitly reverts v49. The
  accepted source and executable evidence return to v48. This preserves the
  failed experiment in history without making a structurally plausible but
  slower path the compiler default.
- Self-host completion remains absent: no routine 2,048 marker, complete
  `driver_gen2.c`, gen2 compilation, or gen3 comparison exists. The next seam
  must not repeat direct per-block aggregate construction or infer speed from
  static check removal alone.

### 2026-07-26 -- Resource raw scalar carriage is rejected and reverted

- `530682af` captured instruction `name` plus exact unique
  `runtime_call_abi_required`, `runtime_call_abi`, and
  `runtime_call_abi_aux` bounds in the existing scalar pass. The routine-local
  bundle carried those facts to the unchanged resource semantic owner; no
  cache, global aggregate, backend split, or old-read fallback remained.
- Focused routine-index C/LLVM, resource ABI C/LLVM negatives, component,
  bounded, and wrong-ABI gates passed. The exact-source driver built in 62,385
  ms at 2,445.2 MB peak private / 2,438.9 MB working set. Its bounded result
  completed in 609 ms and remained 414 bytes with SHA-256
  `0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`;
  wrong ABI exited 1 with the owned diagnostic and no output.
- The fixed run reached routine 704 at 189,951 ms, routine 896 at 222,884 ms,
  routine 1,600 at 279,085 ms, and routine 1,728 at 296,959 ms. It timed out at
  300,680 ms with 178.2 MB peak private / 182.3 MB working set and no routine
  1,792/2,048, MIR-to-AST completion, or gen2 output. This materially regressed
  v48 despite lower memory.
- `c5ee6e62` reverts the expanded scalar/bundle carrier. The failed shape stays
  in history as evidence that fewer repeated bytes do not guarantee cheaper
  generated code. Accepted performance evidence remains v48.
- Independent review exposed a separable fail-open: a non-resource instruction
  could carry a wrong-kind runtime ABI value and have it treated as absence.
  `5e12cf43` retains only the one-condition fail-closed correction plus a
  current-source C/LLVM negative and component ratchet. Markerless canonical
  native resource rows remain compatible.
- Self-host completion remains absent. There is no routine 2,048 marker,
  `consumer:mir-to-ast:done`, complete gen2 file, gen2 compilation, or gen3
  comparison.

### 2026-07-26 -- Resource local scan is rejected; the seam is abandoned

- `e6abdeaa` kept resource ABI meaning in
  `MirResourceRuntimeRowFactReady` and replaced four independent top-level
  reads with one ephemeral local scan. It added no carrier, cache, helper file,
  global aggregate, backend split, or compatibility fallback.
- Expanded current-source C/LLVM gates covered markerless and explicit-`true`
  rows, escaped and duplicate semantic keys, wrong-kind/`false` required
  markers, name edge cases, stray wrong-kind rows, and auxiliary-table
  failures. The component, bounded, and wrong-ABI gates passed.
- The exact-source v51 driver built in 56,417 ms at 2,576.8 MB peak private /
  2,565.8 MB working set. Its 1,408 ms bounded result remained 414 bytes with
  SHA-256
  `0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`.
  Wrong ABI exited 1 with the owned diagnostic and no output.
- The fixed run reached routine 704 at 173,196 ms, routine 896 at 204,052 ms,
  routine 1,600 at 255,976 ms, routine 1,728 at 272,517 ms, and routine 1,792
  at 287,519 ms. It timed out at 300,614 ms with 192.6 MB peak private / 195.6
  MB working set and no routine 1,856/1,984/2,048, MIR-to-AST completion, or
  gen2 output.
- `6879f0c0` reverts v51. Together with rejected carrier v50, this is enough
  falsifying evidence to abandon the resource read seam. Do not attempt a
  third resource shape. Accepted performance evidence remains v48 plus the
  isolated stray-row fail-closed correction.
- The next executable seam is the two direct `succ_true`/`succ_false` block
  reads in `BuildMirRoutineFactIndex`. It must preserve the existing fact-index
  owner and fail-closed `cfg_successor` contract rather than introducing a
  program-global graph or backend-specific path.

### 2026-07-26 -- Block-successor pair capture is rejected and reverted

- `8c49f74f` replaced the two direct block reads for `succ_true` and
  `succ_false` with one order-independent capture in the existing MIR JSON fact
  transport owner. `MirRoutineFactIndex` remained the semantic owner; no
  program-global carrier, second graph, cache, backend split, or old-read
  fallback was introduced.
- Focused current-source C/LLVM and component gates covered reordered fields,
  one or both fields missing, string-valued numbers, plain and escaped
  semantic duplicates, explicit negatives, and out-of-range targets. Missing
  alone retained the internal negative sentinel, while present invalid facts
  failed at `cfg_successor`.
- The exact-source v52 driver built in 67,265 ms at 2,591.5 MB peak private /
  2,580.9 MB working set. Its 1,704 ms bounded result remained 414 bytes with
  SHA-256
  `0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`.
  Wrong ABI exited 1 with the owned diagnostic and no output.
- The correctly observed fixed run completed machine routine-index admission
  at 83,531 ms and reached routines 704/896/1,600/1,664 at
  198,093/233,293/291,565/298,472 ms. It timed out at 300,560 ms with 172.9 MB
  peak private / 176.6 MB working set and no routine 1,728/2,048, MIR-to-AST
  completion, or gen2 output.
- Machine admission and routine 704 were 15,964 and 39,276 ms later than v48.
  This is a material generated-code CPU regression, not memory pressure.
  `40037e52` reverts v52 and abandons the successor-pair seam. Do not retry it
  as another pair struct, generic wrapper, array carrier, or backend-specific
  path.
- An initial v52 full invocation omitted the observation token. Its memory and
  timeout evidence is retained under the unqualified `v52-300s` label, but it
  has no valid routine markers. Only `v52-300s-observed` is used above.
- Accepted performance evidence returns to v48 plus the isolated stray-row
  fail-closed correction. Self-host completion remains absent: no complete
  `driver_gen2.c`, gen2 compilation, or gen3 comparison exists.
