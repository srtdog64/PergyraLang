# Self-Host Status (verified snapshot)

Branch main. This snapshot records what is verified to self-host right now,
measured by building pgy and running
`make self-host-preparation-test-smoke` on 2026-06-19. The parser/lexer
front-end figures below were refreshed on 2026-06-18 with
`make self-host-lexer-parity-test-smoke self-host-parser-parity-test-smoke`
and `src/self_hosted/parity/parser_scale_probe.sh --failing`. The gate runs the
self-hosted tools on C and, when the current `pgy` build includes the LLVM
backend, LLVM. `LLVM_ENABLED=0` jobs still prove the C leg and require an
explicit LLVM-leg skip instead of treating the build configuration as a tool
failure. The C compiler remains the oracle.

## Verified

Front-end self-hosts on both backends in LLVM-enabled builds.

- Lexer (src/self_hosted/lexer/main.pgy): compiles on C and LLVM. Token output
  is byte-identical to `pgy --tokens` across the 6 committed source fixtures,
  and the live drift guard confirms those fixtures still match the current
  oracle. The broader lexer scale measurement remains 191 of 195 historical
  sources byte-equal; there is not yet a committed lexer-scale probe script.
- Parser (src/self_hosted/parser/main.pgy): compiles on C and LLVM and compares
  byte-identical against `pgy --ast` on 188 committed source fixtures. It parses
  the domain grammar, not just generic constructs: it dispatches on zone, world,
  party, role, and intent keywords, plus bind, if, within-zone, and intent
  steps, intent retry declaration metadata, with full expression precedence.
  The parity set includes a deep nested generic type fixture so LLVM
  depth/type-name handling is covered. The current examples scale probe is
  107 of 119 byte-equal against live `pgy --ast`, with 4 byte-drifts, 7
  self-host parser exits, and 1 C-oracle skip (`secure_slots`).
- Backend parity: the parser compiled by the C backend and by the LLVM backend
  produce byte-identical output. This is the core self-host correctness signal,
  the language compiles its own pass to the same result on both backends.

Single source of truth (capability 5) is closed for the measured
source_ast/source_decl frontier, but not for every body source-payload
compatibility path.

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
  at the DEF owner. Standalone resource constructor expressions still fail
  closed. Assignment DEF emission preserves the source assignment side effect
  before storing the resulting value into the SSA local, so host-field writes do
  not regress to local-only updates.
- LLVM await DEF emission, C pending SSA-use materialization, and LLVM source
  DEF copy consume MIR `expr0` / `expr1` plus local-decl/source-statement flags;
  those paths no longer open `mir_instruction_source_payload`.
- LLVM MIR for-in and with-slot resource-claim diagnostics use MIR expression
  anchors instead of opening source payload statements.
- C resource mirroring compares MIR source-statement indexes instead of source
  payload pointer identity, and the C resource hook consumes DEF `expr1` type
  annotations instead of recovering local-decl payloads.
- The remaining capability-5 body tail is the narrower source-payload
  expression/shape surface: match/select/resource shape consumers and selected
  diagnostics. Capability 5 should stay ACTIVE until those reads are replaced
  by dedicated MIR facts or reduced to provenance-only diagnostics.
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
- MIR source-local type facts are source-name keyed. LLVM MIR alloca/type
  consumers normalize SSA-versioned names such as `push_fn.1` back to
  `push_fn` before consuming the fact, so branch/phi locals do not reopen AST
  body scans on the self-hosted codegen LLVM leg.
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
  dispatch inventory checker, and runtime boundary checker all pass their
  current self-host preparation parity gates.
- `make self-host-preparation-test-smoke` is green on the current LLVM-enabled
  Windows build: lexer, parser, semantic, codegen parity, codegen bootstrap,
  backend tri-compare, MIR JSON lowering, production size/header checkers, and
  the self-hosted audit tools all pass their C/LLVM legs.
- Every one of the 22 self-host tool parity gates now exercises both backends
  when the compiler build includes LLVM.
  Previously 12 of them built and ran their Pergyra tool only with the default
  (C) backend, leaving each tool's LLVM compilation ungated. Each now compiles
  its tool with the C and the LLVM backend and asserts the two native binaries
  produce byte-identical output, via the shared
  `src/self_hosted/parity/llvm_leg_helpers.sh` (`assert_llvm_leg`) for the
  `--run` tools and inline legs for the lexer and backend-output comparator. At
  the time of writing all 22 pass, so the gap was harness coverage, not an LLVM
  backend defect; the gates now hold the C/LLVM equality invariant for the whole
  tool corpus. C-only CI builds keep the C leg mandatory and report an explicit
  LLVM-leg skip from the parity harness.
- Five Pergyra-origin AIR graph consumers now run as soft self-host evidence:
  node-id uniqueness, live-dump node-count integrity, live-dump back-reference
  range checking, fixture-shaped edge referential integrity, and root
  reachability via a push-only worklist. They are not compiler-core
  substitution yet because they do not replace
  `src/self_hosted/air/`, but they prove deterministic graph traversal and
  invariant-checking substrate on both C and LLVM.
- The semantic substitution rung has reached rung-2:
  `src/self_hosted/semantic/main.pgy` checks a bounded function-body subset and
  now types expressions through unary not, top-level binary operators
  (arithmetic yields Int, comparison and logical yield Bool), and function calls
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
  skipping when either side is Unknown. Known limitation: arithmetic result
  typing is simplified to Int, which diverges from the C rule (binary `+ - * /`
  is `type_equals` and yields the left operand type, with a numeric guard only on
  unary `-`), so `"a" + "b"` (String) and `true + false` (Bool) would be accepted
  by C but mis-typed here. This stays latent because no fixture and no real
  self-host source uses arithmetic on String or Bool operands (string building
  uses `Concat`); closing it would require operand-type-aware arithmetic typing.
  `src/self_hosted/parity/semantic_parity.sh` compares its verdicts with the C
  compiler accept/reject oracle on C and LLVM across 61 committed fixtures that
  close the diagnostic matrix for every check across every statement position
  (typed let/return, arithmetic, comparison, call-return, call-argument,
  call-arity, branch-condition, scoped-block, assignment-type, bare-call-statement,
  binary-, logical-, and unary-not-operand-agreement, builtin-table calls, and
  undefined-identifier cases), all
  emitted as `pgy.selfhost.semantic.v1` diagnostic blocks through
  `src/self_hosted/lib/diagnostic.pgy` and byte-equal on both backends. It
  checks that `if` / `while` conditions are
  `Bool` (`condition_not_bool`), that a simple local assignment `name = expr`
  matches the variable's declared type (`assign_type_mismatch`), and that
  expression-statement calls (`Foo(args);`) satisfy the callee's arity and
  argument types -- not only calls in `let` / `return` position. Simple
  identifier expressions, including identifiers nested inside compound
  arithmetic/call-argument expressions, now report `undefined_symbol` when
  absent from the local environment. It also checks scoped `if` / `while`
  bodies without leaking block-local `let` bindings into the parent
  environment. The 61-fixture parity set is the current gate; direct runs over
  full self-host sources are not yet claimed as a gate because the recursive
  block scan still needs a real-source stability pass before it can cover the
  lexer, parser, linter, and semantic checker sources themselves.
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
binary- and logical-operand-agreement typing (comparison and arithmetic operands
must share a type; `&&`/`||` operands must be Bool) in let, return, condition,
and assignment positions, and simple/compound undefined-identifier
diagnostics are covered, and verdicts stay
byte-equal beside the C type checker on 61 committed fixtures across both
backends. The checker now covers the common statement forms (let, return,
assignment, if/while body, if/while condition, bare call), and the fixture
matrix exercises each diagnostic in every position where it can fire. The next
increments
require deeper machinery: real-source semantic stability over the self-hosted
lexer/parser/linter/semantic sources, a broader symbol table of builtins/types,
and a stable diagnostic-code catalog shared with the C oracle, before moving
into declaration-heavy semantic owners.

## How to reproduce

    make pgy
    make self-host-preparation-test-smoke

After changing diagnostic rendering, the `SemanticReason` / `SemanticFix`
tables, or fixtures, regenerate the expected verdict blocks from the tool itself
(the checker is the single source of truth for its own output) and review the
diff before committing:

    src/self_hosted/parity/regen_expected.sh
