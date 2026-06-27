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

- **Lexer**: self-hosts on C+LLVM. Byte-identical to `pgy --tokens` across the 7
  committed parity fixtures (gated) and **993/993 examples + backend_compare
  sources** (scale probe, as of session 2026-06-23). Zero self-host lexer crashes.
  `main.pgy` is now only the entrypoint; character/codepoint handling,
  token classification/output formatting, and scan-loop state are split into
  source-of-truth owner modules.
- **Parser**: self-hosts on C+LLVM. Byte-identical against `pgy --ast` on 189
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
  **93 committed fixtures**. Expression typing now owns same-type
  `Int`/`Long`/`Float` arithmetic, contextual integer-literal assignment
  to `Long` as the only widening rule in this rung, and scalar math builtin
  signatures for `Sqrt`, `Pow`, `Floor`, `Ceil`, `Random`, and `SeedRandom`, trig/log Float
  signatures from `Sin` through `Log2`, string split/join aliases, plus
  first-argument scalar utility typing for `Abs`, `Min`, `Max`, and `Clamp`.
- **Compiler core**: capability-5 single-source-of-truth is READY for the
  measured source_ast/source_decl and supported MIR-lowering frontier.
  Source-payload reads for the gated body surface have been replaced by
  dedicated MIR/source-shape facts, and the self-hosted MIR-lowering path is
  ratcheted against reading transitional `"ast"` text. The committed
  MIR-lower/codegen frontier is **85 PASS / 0 gap plus 0 clean rejects**.

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
- The rolling MIR JSON frontier moves to **85 PASS / 0 gap plus 0 clean
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
  `CompilerArtifactKindAt(6)` and C/LLVM projection rows from
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
