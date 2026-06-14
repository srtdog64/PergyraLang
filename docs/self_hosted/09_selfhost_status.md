# Self-Host Status (verified snapshot)

Branch codex/sot-selfhost-closure. This snapshot records what is verified to
self-host right now, measured by building pgy and running the self-hosted
passes on both backends against the C compiler as oracle.

## Verified

Front-end self-hosts on both backends.

- Lexer (src/self_hosted/lexer/main.pgy): compiles on C and LLVM. Token output
  is byte-identical to `pgy --tokens`.
- Parser (src/self_hosted/parser/main.pgy, 3309 lines): compiles on C and LLVM.
  It parses the domain grammar, not just generic constructs: it dispatches on
  zone, world, party, role, and intent keywords, plus bind, if, within-zone,
  and intent steps, with full expression precedence. Output is identical to a
  committed fixture (multi_log) and matches `pgy --ast` on hello.pgy except for
  one trailing blank line that the parity scripts normalize.
- Backend parity: the parser compiled by the C backend and by the LLVM backend
  produce byte-identical output. This is the core self-host correctness signal,
  the language compiles its own pass to the same result on both backends.

Single source of truth (capability 5) is nearly closed.

- Codegen source_ast frontier is at 0, all 127 original reads retired.
- Compiler-side source_ast is at 2, the declaration-header payload assignment
  and accessor, tracked by the ratchet.
- C class/zone collection-specialization scans are MIR-routine based and no
  longer recover method body AST; the remaining routine source-decl bridge is
  lookup/projection compatibility.

Substrate progress.

- DirWalk deterministic directory snapshot added (filesystem_directory_walk
  gate), removing one of the three self-host substrate gaps.

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
    pgy src/self_hosted/lexer/main.pgy --emit-c   # and --emit-llvm
    pgy src/self_hosted/parser/main.pgy --emit-c  # and --emit-llvm
    # build each to native, run against pgy --tokens / pgy --ast, diff
