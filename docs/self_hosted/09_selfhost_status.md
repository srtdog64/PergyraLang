# Self-Host Status (verified snapshot)

Branch main. This is a dated evidence snapshot, not a live CI assertion for
the current commit. It records what was verified by building pgy and running
`make self-host-preparation-test-smoke` on 2026-06-21. The lexer/parser scale
figures below were refreshed on 2026-06-23 with
`make self-host-lexer-parity-test-smoke self-host-parser-parity-test-smoke`
and `tests/self_hosted/parity/lexer_scale_probe.sh --failing` /
`tests/self_hosted/parity/parser_scale_probe.sh --failing`. The gate runs the
self-hosted tools on C and, when the current `pgy` build includes the LLVM
backend, LLVM. `LLVM_ENABLED=0` jobs still prove the C leg and require an
explicit LLVM-leg skip instead of treating the build configuration as a tool
failure. The C compiler remains the oracle.

For work after this snapshot, follow `docs/152_validation_isolation_policy.md`:
rerun only the owner gate for the touched self-host rung unless a broader
compiler-world owner changed or broad parity is explicitly requested.

## Verified

Front-end self-hosts on both backends in LLVM-enabled builds.

- Lexer (src/self_hosted/lexer/): compiles on C and LLVM. `main.pgy` is only
  the entrypoint; character/codepoint handling, token classification/output
  formatting, and scan-loop state are owned by separate modules. Token output
  is byte-identical to `pgy --tokens` across the 8 committed source fixtures,
  and the live drift guard confirmed those fixtures matched the then-current
  oracle. The broader lexer scale probe now measures 993 of 993 examples +
  backend_compare sources byte-equal.
- Parser (src/self_hosted/parser/): compiles on C and LLVM and compares
  byte-identical against `pgy --ast` on 188 committed source/fixture rows. It parses
  the domain grammar, not just generic constructs: it dispatches on zone, world,
  party, role, and intent keywords, plus bind, if, within-zone, and intent
  steps, intent retry declaration metadata, with full expression precedence.
  The parity set includes a deep nested generic type fixture so LLVM
  depth/type-name handling is covered. Parse failure rendering, source
  cursor/token reads, written type-name parsing, expression parsing,
  statement/block parsing, function declaration/signature rendering,
  recursive declaration dispatch, type/ability/event/enum/zone/effect/relation/
  role/intent/nominal-domain declaration parsing, and compact AST text formatting
  are owned by separate parser modules; `main.pgy` is entrypoint-only and
  delegates parser CLI mode selection to `run_owner.pgy`. Parser tool input is
  single-sourced through `Args()[0]`; the previous
  probe-only source override is retired. The last examples scale probe was
  120 of 121 byte-equal against live `pgy --ast`, with zero byte-drift, zero
  self-host parser exits, and 1 C-oracle skip (`secure_slots`).
- Backend parity: the parser compiled by the C backend and by the LLVM backend
  produce byte-identical output. This is the core self-host correctness signal,
  the language compiles its own pass to the same result on both backends.

Single source of truth (capability 5) is closed for the measured
source_ast/source_decl frontier and the supported self-hosted MIR-lowering
subset.

- Codegen source_ast frontier is at 0, all 127 original reads retired.
- Compiler-side source_ast is at 0. `MIRDeclHeader.source_ast` and
  `mir_decl_header_source_decl` are removed, and the ratchet is locked at 0.
- Residual `MIR_INST_STMT` source-payload emission is retired in C and LLVM:
  side-effect statements now flow through explicit `MIR_STMT.expr0`
  executable facts and backend emitters fail closed instead of redispatching
  the source payload as a statement.
- Source-local declaration and assignment paths no longer call raw
  source-statement emitters. View-like pin aliases are preserved through MIR
  pin facts and SSA map ownership instead of materializing a second slot.
- LLVM source-local resource constructors for `Channel<T>`, `Slot<T>`,
  `SecureSlot<T>`, and `DeviceSlot<T>` now consume MIR expected type-name facts
  plus initializer facts at the DEF owner. Standalone resource constructor
  expressions still fail closed. Assignment DEF emission preserves the source
  assignment side effect before storing the resulting value into the SSA local,
  so host-field writes do not regress to local-only updates.
- LLVM await DEF emission, C pending SSA-use materialization, and LLVM source
  DEF copy consume MIR `expr0` / `expr1` plus local-decl/source-statement flags;
  those paths no longer open `mir_instruction_source_payload`.
- LLVM MIR for-in and with-slot resource-claim diagnostics use MIR expression
  anchors instead of opening source payload statements.
- C resource mirroring compares MIR source-statement indexes instead of source
  payload pointer identity, and the C resource hook consumes DEF `expr1` type
  annotations instead of recovering local-decl payloads.
- C SSA local type/view registration consumes DEF `expr0` / `expr1` and routine
  source-local type facts. Destructured locals use MIR destructure
  binding-name/index facts, so this path no longer opens source payloads for
  binding-name recovery.
- C MIR destructure emission consumes the MIR initializer expression and
  destructure binding facts; it no longer reads `ast_let_destructure_*` from
  the source statement payload.
- C source-local LET DEF emission, generic DEF expression emission, and
  receive-payload type inference consume instruction `arg0` / `expr0` /
  `expr1` facts directly. C codegen is ratcheted against reopening
  `mir_instruction_source_payload`.
- LLVM MIR destructure emission consumes the same MIR initializer and binding
  facts through `llvm_emit_mir_destructure_inst`; the non-MIR statement emitter
  remains AST-backed for the legacy statement path.
- C and LLVM assignment emission consume MIR assignment facts. `MIR_INST_ASSIGN`
  is validated to carry target/value `expr0` / `expr1`, assignment DEFs carry
  their target in `expr1`, and C/LLVM assignment-parts emitters preserve slot,
  array, field, and projection semantics without reopening the source statement
  payload.
- MIR surface validation no longer reopens source payloads. Payload presence is
  checked through source-shape predicates, and thread-pool / intent
  observability surface-usage validation consumes MIR `expr0` / `expr1` facts.
- MIR DCE and source-statement emit validation consume source-shape scalar
  facts instead of payload presence for those decisions. Source-statement emit
  predicates and LLVM DEF emit predicates now consume MIR emit facts instead of
  treating source payload presence as a semantic condition. C and LLVM residual
  STMT emission branches consume MIR source-shape / `expr0` facts, and LLVM
  missing-return-value diagnostics use MIR topology diagnostics instead of
  source payload anchors.
  Select dispatch branches carry their readiness channel as a MIR branch
  `expr0` fact; C/LLVM condition emission no longer parses select case payloads.
  Match-case branches carry MIR-captured pattern/guard facts, and C/LLVM match
  condition, body-binding, and remap emission consume those facts instead of
  parsing the match-case source payload.
- Source line/column/stable-id/type seeding and transitional MIR JSON source
  text are now capture-time facts owned by
  `mir_instruction_capture_source_provenance(...)`. `mir_public_surface.c` and
  `mir_lifecycle.c` no longer open source payloads; lifecycle dump emission
  consumes `mir_instruction_source_inline_text(inst)`. Self-hosted `mir_lower`
  now consumes explicit MIR JSON `expr0`/`expr1`/`source_type`/`source_locals`
  facts only for the supported let/statement/return/branch/for subset plus
  selected args/array/string/Bool/Float/file/recursion fixture surfaces,
  straight-line calls, direct integer arithmetic, builtin-name string literals,
  directory walking, exit-guard branches, multiple Void routines with bare-call
  statements, Bool-literal branch reassignment, and loop-control
  `continue`/`break` edge blocks. The MIR JSON parity gate checks
  the `for` header is reconstructed from
  `arg0` plus range bounds rather than treating the lower bound as a branch
  condition, and rejects reintroducing transitional `"ast"` reads. It also
  reconstructs match-case integer branch conditions from `match_patterns`
  facts rather than parsing the source compatibility text, and keeps the
  self-hosted codegen file helpers aligned with the runtime absolute-path
  policy. Option match cases now carry `match_variant` and `match_bindings`
  facts; `Some(v)` lowers to an `IsSome(subject)` branch plus a fact-owned
  `Let: v : Int = UnwrapOption(subject)` binding, while `None` lowers to
  `!IsSome(subject)`, without parsing transitional source text. It also
  classifies phi-bearing loop headers from CFG backedges rather
  than phi presence alone, so nested `if` branches inside loops remain `If:`
  nodes. Array destructuring now consumes MIR JSON `destructure_bindings`
  facts and source-local array type facts to reconstruct typed array-index
  `Let:` bindings without parsing source text. Plain `class` declarations now
  flow through MIR-owned field/method/owner facts and reconstruct `Class:` /
  `Methods:` in the self-hosted MIR-lowering path. Payload-free enum
  declarations flow through MIR-owned variant facts, while payload enum variants
  fail closed from their `param_count` facts. Field-only class/subject/object/
  tobject/vessel declarations now flow through MIR-owned `nominal_kind` and
  field facts and reconstruct their exact AST labels in the self-hosted
  MIR-lowering path instead of being collapsed to a generic class alias. The
  hard gate is now **85
  positive fixtures plus 0 clean-reject fixtures** after
  promoting the already run-equivalent
  trailing-newline Log, nested string concat, string array concat, string
  case/index/trim builtin, string reassignment, two-log, while-break, and
  while-sum surfaces, array pop and array for-each loops, and typed struct
  field declaration/value flow. It also reconstructs break edges after non-empty
  statement blocks from CFG successor facts and consumes the MIR-owned
  `Random()` Int source-local type fact, the match-case integer pattern
  condition surface, the default absolute-path I/O rejection policy, and the
  nested-if-in-loop regression surface that closed the measured `heap`
  self-host via-run timeout, the array destructure binding surface, plain class
  declaration/method lowering, payload-free enum lowering, `Result<Int>` `?`
  early-return flow, `Result<Int>` core constructors/inspection helpers,
  `Option<Int>` value/match lowering, array sort/map/filter/reverse combinators, and
  `Join`/`ToFloat` string utility flow, and the example-origin
  `binary_search` fixture and the Int role operator-dispatch fixture. The
  coverage boundary is now measured
  at **86 PASS / 0 gap plus 0 clean rejects** across the committed
  MIR-lower/codegen/example fixture inventory. Ability declarations now consume MIR
  method signature facts and lower as zero-artifact declaration hosts in the
  self-hosted codegen pre-passes. Role declaration facts are consumed for the
  supported Int/`Arithmetic.Add` operator path, payload enum variants fail
  closed by MIR variant fact, and unsupported
  self-host codegen builtins are
  rejected before C emission, so out-of-subset operator-overload/domain nominal
  semantics and unsupported runtime helper surfaces cannot silently produce
  broken generated C.
- C class/zone collection-specialization scans are MIR-routine based and no
  longer recover method body AST; routine_source_decl_codegen is ratcheted at 0.
- C hosted method body emission binds the linked MIRRoutine body as current
  function context, and `transpiler_host_field_identifier.c` owns current-host
  field identifier lowering so stale field SSA snapshots and field/local
  shadowing do not reopen AST/source-local fallback paths.
- Type-alias target names are MIR declaration-header facts. C and LLVM now use
  the same canonical source-local type fact for alias-backed collection
  contexts; `type_alias_array_context` proves empty `Array<T>` alias literals
  compile and run on both backends.
- MIR source-local type facts are source-name keyed. C MIR-backed function
  prologue setup materializes local bindings from `MIRRoutine::source_local_types`
  instead of rescanning the AST body, and LLVM MIR alloca/type consumers
  normalize SSA-versioned names such as `push_fn.1` back to `push_fn` before
  consuming the fact. For-loop element bindings, `Array<T>.Slice(...) ->
  Slice<T>`, and `SliceCopy(Slice<T>) -> Array<T>` are captured as MIR
  source-local type facts, so branch/phi locals and collection view locals do
  not reopen AST body scans on C or LLVM self-hosted codegen legs.
- Intent retry counts are MIR declaration-header facts. `with retry(n)` is
  parsed, printed, and captured as `MIRDeclHeader.intent_retry_count`; semantic
  checking rejects non-zero retry until C and LLVM retry lowering share the same
  MIR-owned intent body wrapper, so the feature cannot become a silent no-op.
- LLVM generic class specialization type mapping consumes `MIRDeclField`
  type-name metadata before falling back to template AST compatibility.
- LLVM class constructor field arguments consume `MIRDeclField` type-name
  metadata for expected-type context before template AST compatibility.
- C/LLVM class field-slot claim helpers consume `MIRDeclFieldClaim` metadata
  instead of class destructure AST in MIR-active paths.
- C/LLVM role-slot ability tag rendering fills omitted generic actuals from
  `MIRDeclHeader` generic metadata instead of ability source declarations in
  MIR-active paths.
- C party-slot method dispatch now uses ability `MIRDeclHeader` method rows to
  choose the owning ability tag in MIR-active paths; `ast_ability_method_*`
  remains only inside the explicit non-MIR compatibility helper.
- C/LLVM declaration existence checks that only need a yes/no answer now use
  header-backed `*_decl_exists*` seams in MIR-active paths. They no longer
  recover origin AST declarations just to test class, enum, function, intent,
  callable, or constructor presence.
- C/LLVM declaration payload lookup first validates the MIR declaration-header
  row and then searches active inventory; it no longer has a
  declaration-header source_decl accessor to call.
- C projection literal/source-path lowering has a by-name MIR header path for
  ToTObject, projection-borrow materialization, member access, and domain
  provenance refresh.
- LLVM projection-borrow materialization, member access, and domain projection
  value lowering use the same by-name MIR header path for source field paths.
  The remaining declaration work is broader dedicated declaration IR coverage,
  not source_ast/source_decl payload retirement.

Substrate progress.

- DirWalk deterministic directory snapshot added (filesystem_directory_walk
  gate) and verified on C and LLVM. Examples inventory, production size, and
  ast-read-surface self-host tools now consume DirWalk directly, so their clean
  file inventories no longer depend on committed file-list aliases.
- Parser parity compiles the self-host parser through both C and LLVM, including
  deep nested generic type inference fixtures.
- The Pergyra linter, backend output comparator, backend tri-compare,
  AST-read-surface checker, diagnostics catalog checker, doc/example inventory
  checkers, module manifest resolver, production size checkers, AIR graph JSON
  validator, AIR graph consumer checkers, stable subset checker, stdlib
  dispatch inventory checker, and runtime boundary checker all passed their
  self-host preparation parity gates in the dated snapshot.
- `make self-host-preparation-test-smoke` was green on the last verified
  LLVM-enabled Windows build: lexer, parser, semantic, codegen parity, codegen
  bootstrap, backend tri-compare, MIR JSON lowering, production size/header
  checkers, and the self-hosted audit tools all passed their C/LLVM legs.
- Every one of the 22 self-host tool parity gates now exercises both backends
  when the compiler build includes LLVM.
  Previously 12 of them built and ran their Pergyra tool only with the default
  (C) backend, leaving each tool's LLVM compilation ungated. Each now compiles
  its tool with the C and the LLVM backend and writes both native outputs as
  comparable artifacts. The shared
  `tests/self_hosted/parity/llvm_leg_helpers.sh` (`assert_llvm_leg`) now invokes
  the Pergyra `backend_output_comparator` with `c_oracle`/`llvm_oracle`
  projection rows for the `--run` tools; the lexer and backend-output
  comparator keep their inline artifact legs. At the time of writing all 22
  pass, so the gap was harness coverage, not an LLVM backend defect; the gates
  now hold the C/LLVM equality invariant for the whole tool corpus. C-only CI
  builds keep the C leg mandatory and report an explicit LLVM-leg skip from the
  parity harness.
- Five Pergyra-origin AIR graph consumers now run as soft self-host evidence:
  node-id uniqueness, live-dump node-count integrity, live-dump back-reference
  range checking, fixture-shaped edge referential integrity, and root
  reachability via a push-only worklist. They are not compiler-core
  substitution yet because they do not replace
  `src/self_hosted/air/`, but they prove deterministic graph traversal and
  invariant-checking substrate on both C and LLVM.
- The semantic substitution rung has reached rung-2:
  `src/self_hosted/semantic/` checks a bounded function-body subset and now
  splits the checker into source-of-truth owner modules for source-bundle/import
  expansion, source scanning, diagnostic rendering, local environment lookup,
  expression typing, call checking, body/function checking, and program checking.
  `main.pgy` is the CLI/output boundary only. It types expressions
  through unary not, top-level binary operators
  (same-type Int/Long/Float arithmetic preserves its operand type; comparison
  and logical yield Bool), and function calls
  resolved against a signature table seeded with built-ins and the program's own
  `func` return types. It also checks call-argument types positionally against
  each callee's parameter signature, emitting `call_arg_type_mismatch` when a
  known argument type disagrees with the declared parameter type, and checks
  call arity against the parameter count, emitting `call_arity_mismatch` when
  the number of arguments differs from the declaration. It checks binary and
  logical operand agreement in `let` initializers, `return` expressions,
  `if`/`while` conditions, and assignment right-hand sides: comparison and
  arithmetic operators require equal left/right operand types (emitting
  `compare_type_mismatch` / `binop_type_mismatch`) and `&&`/`||` require Bool
  operands (`logical_operand_not_bool`), and a leading unary `!` requires a Bool
  operand (`not_operand_not_bool`), each emitted only when the operand types
  are known and disagree, mirroring the C oracle's `type_equals` rule and
  skipping when either side is Unknown. Arithmetic result typing now preserves
  same-type `Int`/`Long`/`Float` numeric operands, same-type `Bool` arithmetic,
  and C-oracle `String + String` concatenation typing while still rejecting
  mixed `Int + String` as a binary-operand mismatch.
  `tests/self_hosted/parity/semantic_parity.sh` compares its verdicts with the C
  compiler accept/reject oracle on C and LLVM across 107 committed fixtures that
  close the diagnostic matrix for every check across every statement position
  (typed let/return, arithmetic, comparison, call-return, call-argument,
  call-arity, branch-condition, scoped-block, assignment-type, bare-call-statement,
  binary-, logical-, and unary-not-operand-agreement, `let mut`, file IO builtin
  calls, scalar math builtin-table calls, trig/log Float builtin calls, string
  split/join aliases, first-argument scalar utility calls, generated-source
  string literals, String-plus and Bool arithmetic typing, Option payload typing,
  `None()` context typing, concrete
  `Option<T>` requirements for `IsSome`/`UnwrapOption`, and
  undefined-identifier cases), all
  emitted as `pgy.selfhost.semantic.v1` diagnostic blocks through
  `src/self_hosted/lib/diagnostic.pgy` and byte-equal on both backends. It
  now also gates the self-hosted semantic diagnostic-code vocabulary:
  `src/self_hosted/semantic/diagnostic_code_owner.pgy` owns the 17 lower-case
  codes, and the parity harness rejects fixture `Code:` fields or literal
  `SemanticError...("code")` call sites that are not registered there. The same
  owner maps every fixture-emitted self-hosted code to the current C oracle JSON
  root code, and the parity harness rejects invalid fixtures that fall through
  to backend-native failure or report a different C root code. This is still a
  fixture-root-code gate, not a claim that every C semantic diagnostic has a
  one-to-one Pergyra code.
  The same parity matrix checks that `if` / `while` conditions are
  `Bool` (`condition_not_bool`), that a simple local assignment `name = expr`
  matches the variable's declared type (`assign_type_mismatch`), and that
  expression-statement calls (`Foo(args);`) satisfy the callee's arity and
  argument types -- not only calls in `let` / `return` position. Simple
  identifier expressions, including identifiers nested inside compound
  arithmetic/call-argument expressions, now report `undefined_symbol` when
  absent from the local environment. It also checks scoped `if` / `while`
  bodies without leaking block-local `let` bindings into the parent
  environment. The parity set now includes an import-backed fixture, and
  `tests/self_hosted/parity/selfcheck_sources.sh` now consumes the
  `self-host-completeness-semantic-targets` manifest from TestHarness instead
  of owning a shell source list. The current completeness-owned inventory
  accepts 155 real self-host production source rows through the semantic
  checker; split parser/codegen files use the semantic target selected by
  `completeness_ledger_owner.pgy`. This is still source-stage acceptance, not a
  claim that typed self-semantic facts already drive the native backends end to
  end.
- Building the signature table reproduced the array value-semantics finding from
  the linter: a helper that `ArrayPush`es into an `Array<T>` parameter mutates a
  copy, so the table is built inline in the owning function until `inout Array<T>`
  value-result parameters land. This is the second dogfooded motivation for that
  feature.

## Not yet self-hosted

The middle and back of the compiler are still mostly C:

- semantic analysis beyond the typed let/return + operator/call verdict slice,
- HIR/DIR/MIR lowering,
- C and LLVM backend emission.

These are the bulk of the remaining hard-self-host work. The front-end being
done and parity-verified means the method is proven; the remaining passes
follow the same recipe, write the pass in Pergyra, run it beside the C version,
compare output, expand coverage.

## Recommended next pass

Do not start a broad semantic rewrite yet. The deterministic collection gap is
closed by the compiler-key policy: symbol/record-like identities are canonical
strings, handle-like identities are stable integer/long IDs, and
`stage4_determinism_smoke` verifies those shapes on C and LLVM. The allocator
pass-lane gap is also closed at the language surface: lane-named `Allocator`
constructors are present on C and LLVM, and pass authors pair them with
`defer { AllocatorDestroy(lane); }` for explicit cleanup. Semantic analysis
now runs in that shape at rung-2: expression operators, function-call return
typing, positional call-argument typing, call-arity checking (in `let`/`return`
and bare expression statements), branch condition (`if`/`while` must be `Bool`)
typing, scoped `if`/`while` body typing, simple local assignment typing,
binary- and logical-operand-agreement typing (same-type Int/Long/Float numeric
arithmetic preserves its operand type; comparison operands must share a type;
same-type Bool arithmetic and `String + String` follow the C oracle; `&&`/`||`
operands must be Bool) in let, return, condition, and assignment
positions, and simple/compound undefined-identifier diagnostics are covered,
plus Option payload/concrete-Option builtin contracts, and verdicts stay
byte-equal beside the C type checker on 107 committed fixtures across both
backends. The checker now covers the common statement forms (let, return,
assignment, if/while body, if/while condition, bare call), and the fixture
  matrix exercises each diagnostic in every position where it can fire. The
  source-bundle/import owner now gives the semantic checker a real program-input
  fact for its own imported source. The next increments require deeper
  machinery: broader real-source semantic stability over parser/codegen/linter
  sources, a broader symbol table of builtins/types, and a stable
  diagnostic-code catalog shared with the C oracle, before moving into
  declaration-heavy semantic owners.

## How to reproduce

    make pgy
    make self-host-preparation-test-smoke

After changing diagnostic rendering, the `SemanticReason` / `SemanticFix`
tables, or fixtures, regenerate the expected verdict blocks from the tool itself
(the checker is the single source of truth for its own output) and review the
diff before committing:

    tests/self_hosted/parity/regen_expected.sh
