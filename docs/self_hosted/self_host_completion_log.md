# Self-Host Completion Log

A session-by-session record of the process of completing self-hosting. The
verified *snapshot* lives in [`09_selfhost_status.md`](09_selfhost_status.md);
this file is the *journal* -- what was attempted each session, what landed, and
what the next session should pick up. Append a new entry per session; do not
rewrite history.

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

## Verified state (rolling)

- **Lexer**: self-hosts on C+LLVM. Byte-identical to `pgy --tokens` across the 6
  committed parity fixtures (gated) and **121/121 of the examples corpus**
  (scale probe, as of session 2026-06-20). Zero self-host lexer crashes.
- **Parser**: self-hosts on C+LLVM. Byte-identical against `pgy --ast` on 188
  committed fixtures (gated); examples scale probe last recorded 120/121 with
  zero byte-drift, zero self-host exits, 1 C-oracle skip.
- **Backend parity**: parser compiled by C and by LLVM produce byte-identical
  output -- the core self-host correctness signal.
- **Compiler core**: capability-5 single-source-of-truth is READY for the
  measured source_ast/source_decl and supported MIR-lowering frontier.
  Source-payload reads for the gated body surface have been replaced by
  dedicated MIR/source-shape facts, and the self-hosted MIR-lowering path is
  ratcheted against reading transitional `"ast"` text.

## Roadmap to completion

1. **Front-end coverage to 100%** (assist-safe): lexer corpus is at 121/121;
   parser corpus is at 120/121 with only the C-oracle `secure_slots` skip
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
- The hard MIR JSON gate now proves **67 positive fixtures plus 2 clean
  rejects**. Refreshed examples-scale survey: 41 PASS, 30 CODEGEN-gap, 37
  MIR-LOWER-gap, 13 ORACLE-skip, and 0 measured STDOUT-diff / generated-C
  compile failures / via-run timeouts. `class_method_test` and `class_test`
  moved to PASS; `generic_class` now reaches an explicit generic-field
  CODEGEN-gap instead of failing at declaration inventory.

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

- Made `src/self_hosted/parity/mir_json_coverage_probe.sh` fail closed: it now
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
  src/self_hosted/parity/mir_json_parity.sh`: **30/30 MIR JSON -> self-hosted
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
(`src/self_hosted/parity/mir_json_coverage_probe.sh`). This session filled three,
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

- Committed `src/self_hosted/parity/lexer_scale_probe.sh` (`c7adbb1a`) -- fills
  the noted "no committed lexer-scale probe" gap; mirrors the parser probe.
  Initial measurement: 115/121.
- `a856d3d9`: emit `DOC_COMMENT` for `///` (was skipped), matching the oracle's
  text + text-start column. 115 → 116.
- `62d71ffa`: add missing keywords (transaction, compensate, fail, extends) and
  `$"`/`f"` `INTERPOLATED_STRING` prefixes. 116 → **121/121, zero drift**.
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
