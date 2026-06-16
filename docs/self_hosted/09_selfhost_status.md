# Self-Host Status (verified snapshot)

Branch main. This snapshot records what is verified to self-host right now,
measured by building pgy and running
`make self-host-preparation-test-smoke` on 2026-06-16. The gate runs the
self-hosted tools on C/LLVM where applicable and keeps the C compiler as the
oracle.

## Verified

Front-end self-hosts on both backends.

- Lexer (src/self_hosted/lexer/main.pgy): compiles on C and LLVM. Token output
  is byte-identical to `pgy --tokens`.
- Parser (src/self_hosted/parser/main.pgy): compiles on C and LLVM and compares
  byte-identical against `pgy --ast` on 188 committed source fixtures. It parses
  the domain grammar, not just generic constructs: it dispatches on zone, world,
  party, role, and intent keywords, plus bind, if, within-zone, and intent
  steps, intent retry declaration metadata, with full expression precedence.
  The parity set includes a deep nested generic type fixture so LLVM
  depth/type-name handling is covered.
- Backend parity: the parser compiled by the C backend and by the LLVM backend
  produce byte-identical output. This is the core self-host correctness signal,
  the language compiles its own pass to the same result on both backends.

Single source of truth (capability 5) is closed for the measured
source_ast/source_decl frontier.

- Codegen source_ast frontier is at 0, all 127 original reads retired.
- Compiler-side source_ast is at 0. `MIRDeclHeader.source_ast` and
  `mir_decl_header_source_decl` are removed, and the ratchet is locked at 0.
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
  validator, stable subset checker, and stdlib dispatch inventory checker all
  pass their current self-host preparation parity gates.
- Every one of the 16 self-host tool parity gates now exercises both backends.
  Previously 12 of them built and ran their Pergyra tool only with the default
  (C) backend, leaving each tool's LLVM compilation ungated. Each now compiles
  its tool with the C and the LLVM backend and asserts the two native binaries
  produce byte-identical output, via the shared
  `src/self_hosted/parity/llvm_leg_helpers.sh` (`assert_llvm_leg`) for the
  `--run` tools and inline legs for the lexer and backend-output comparator. At
  the time of writing all 16 pass, so the gap was harness coverage, not an LLVM
  backend defect; the gates now hold the C/LLVM equality invariant for the whole
  tool corpus.
- The semantic substitution rung has reached rung-2:
  `src/self_hosted/semantic/main.pgy` checks a bounded function-body subset and
  now types expressions through unary not, top-level binary operators
  (arithmetic yields Int, comparison and logical yield Bool), and function calls
  resolved against a signature table seeded with built-ins and the program's own
  `func` return types. It also checks call-argument types positionally against
  each callee's parameter signature, emitting `call_arg_type_mismatch` when a
  known argument type disagrees with the declared parameter type, and checks
  call arity against the parameter count, emitting `call_arity_mismatch` when
  the number of arguments differs from the declaration.
  `src/self_hosted/parity/semantic_parity.sh` compares its verdicts with the C
  compiler accept/reject oracle on C and LLVM across 30 committed fixtures
  (typed let/return, arithmetic, comparison, call-return, call-argument,
  call-arity, branch-condition, scoped-block, assignment, bare-call-statement,
  and undefined-identifier cases), all
  byte-equal on both backends. It checks that `if` / `while` conditions are
  `Bool` (`condition_not_bool`), that a simple local assignment `name = expr`
  matches the variable's declared type (`assign_type_mismatch`), and that
  expression-statement calls (`Foo(args);`) satisfy the callee's arity and
  argument types -- not only calls in `let` / `return` position. Simple
  identifier expressions, including identifiers nested inside compound
  arithmetic/call-argument expressions, now report `undefined_symbol` when
  absent from the local environment. It also checks scoped `if` / `while`
  bodies without leaking block-local `let` bindings into the parent
  environment. The checker is sound on the committed
  real sources: it returns `SEMANTIC OK` on the self-hosted lexer, parser,
  linter, and on its own source, with backslash-escape-aware string scanning so
  embedded quote literals do not desync operator detection.
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
typing, scoped `if`/`while` body typing, simple local assignment typing, and
simple/compound undefined-identifier
diagnostics are covered, and verdicts stay
byte-equal beside the C type checker on 30 committed fixtures across both
backends. The checker now covers the common statement forms (let, return,
assignment, if/while body, if/while condition, bare call). The next increments
require deeper machinery: a broader symbol table of builtins/types and a stable
diagnostic-code catalog shared with the C oracle, before moving into
declaration-heavy semantic owners.

## How to reproduce

    make pgy
    make self-host-preparation-test-smoke
