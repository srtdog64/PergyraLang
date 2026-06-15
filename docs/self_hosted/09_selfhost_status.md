# Self-Host Status (verified snapshot)

Branch main. This snapshot records what is verified to self-host right now,
measured by building pgy and running
`make self-host-preparation-test-smoke` on 2026-06-15. The gate runs the
self-hosted tools on C/LLVM where applicable and keeps the C compiler as the
oracle.

## Verified

Front-end self-hosts on both backends.

- Lexer (src/self_hosted/lexer/main.pgy): compiles on C and LLVM. Token output
  is byte-identical to `pgy --tokens`.
- Parser (src/self_hosted/parser/main.pgy): compiles on C and LLVM and compares
  byte-identical against `pgy --ast` on 187 committed source fixtures. It parses
  the domain grammar, not just generic constructs: it dispatches on zone, world,
  party, role, and intent keywords, plus bind, if, within-zone, and intent
  steps, with full expression precedence. The parity set includes a deep nested
  generic type fixture so LLVM depth/type-name handling is covered.
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
  gate) and verified on C and LLVM, removing one of the three self-host
  substrate gaps.
- The Pergyra linter, backend output comparator, backend tri-compare,
  AST-read-surface checker, diagnostics catalog checker, doc/example inventory
  checkers, module manifest resolver, production size checkers, AIR graph JSON
  validator, stable subset checker, and stdlib dispatch inventory checker all
  pass their current self-host preparation parity gates.

## Not yet self-hosted

The middle and back of the compiler are still C only:

- semantic analysis (type checking),
- HIR/DIR/MIR lowering,
- C and LLVM backend emission.

These are the bulk of the remaining hard-self-host work. The front-end being
done and parity-verified means the method is proven; the remaining passes
follow the same recipe, write the pass in Pergyra, run it beside the C version,
compare output, expand coverage.

## Recommended next pass

Semantic analysis, staged like the parser: start with a bounded subset
(name resolution or a single type rule), run it beside the C type checker on
the committed fixtures, compare diagnostics or typed-AST output, expand. Keep
the C type checker as the oracle and keep rollback trivial, per stage 3 of the
roadmap. Do not start MIR lowering or backend emission in Pergyra before the
semantic subset runs at parity.

## How to reproduce

    make pgy
    make self-host-preparation-test-smoke
