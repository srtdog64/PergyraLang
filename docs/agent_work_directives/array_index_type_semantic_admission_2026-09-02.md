# Array Index Type Semantic Admission

Status: ACTIVE — LOCALLY GREEN, PUBLICATION/EXACT CI PENDING

Exact base revision: `a77c450f34c4533d905c3c55aee51d25aaebfa7f`

This directive coordinates one bounded executable self-host semantic repair.
It is not a semantic owner, SoT registry, progress increment, general
collection campaign, or completion claim.

## Shared objective card

- Objective: make the installed Pergyra semantic expression graph reject every
  reached array access whose index type is not `Int` before MIR, C, or LLVM
  artifact publication, while preserving the exact valid `Int`-index program.
- Priority order: consume existing parser-owned graph topology and typed scalar
  facts; validate reachable nested index nodes before artifact admission;
  preserve the native public diagnostic identity; retain valid MIR/C behavior;
  fail closed without creating another collection type authority.
- Fact owners: `ast_expression_graph_fact_owner.pgy` retains node identity and
  child topology; `ast_expression_graph_scalar_type_owner.pgy` retains the
  typed index-expression result; the index operand verdict in
  `ast_expression_graph_scalar_verdict_owner.pgy` owns non-`Int` rejection;
  `ast_expression_verdict_owner.pgy` is the last orchestration consumer before
  MIR admission. Existing diagnostic-code and public-receipt owners retain
  public identity, and the shared C wire remains transport only.
- Production entrypoints: the installed public/self-driver MIR diagnostic,
  public C and LLVM artifact requests, and their explicit-native parity oracle.
- Direct bypass to delete: `SemanticAstExpressionVerdictFromGraph` asks for an
  index value type only when the expression root itself is `Index`. A nested
  `primes[""]` argument under `Log(...)` therefore returns an unresolved value
  to the call path but is never rejected; public MIR publishes it.
- Last legitimate consumer: `SemanticAstExpressionVerdictFromGraph` must walk
  the already admitted expression graph and reject a reached non-`Int` index
  before MIR construction or artifact processing observes the expression.
- Forbidden fallback: expression-text scanning, AST/program-root rescanning,
  a second array type table, C/LLVM semantic mapping, native retry or preflight,
  message parsing, fixture spelling checks, backend-only rejection, changing
  public diagnostic transport, or widening this rung to the separate
  non-indexable-target and heterogeneous-array-literal families.
- Focused gate:
  `tests/self_hosted/parity/array_index_type_semantic_admission_owner.sh`.
- Falsifying cases: the exact `examples/array_literal.pgy` program must remain
  green through direct self-driver, public/native MIR, and public C execution;
  changing only its final `primes[4]` to `primes[""]` must reject through direct
  self-driver and public/native MIR/C/LLVM with no artifact or native retry.
  The direct Pergyra code must retain expected `Int` and actual `String`; the
  public identity must be exactly `PGY_SEM_TYPE_MISMATCH`,
  `semantic:array_access:index_non_int`, and `use-int-index`. The deletion
  minimum `Log([][""])` must reject through the same semantic owner.

## Opening evidence

- The current launcher SHA-256 is
  `820CD461426B19551F5804438DB58CAB216808405B16241799CBDD295E7B84C5`;
  the current Pergyra-built self-driver SHA-256 is
  `45D3EF8DFC97031317DF106F7E14A17B5A383961F761746131CF7FA12BE26014`.
- On the exact base, public and native AST accept `Log([][""])`. Public MIR
  exits zero and publishes it, while native MIR/C reject with the exact typed
  identity above. Public C reaches only the malformed-receipt boundary.
- A stronger declaration-backed pair removes unrelated support ambiguity.
  The unchanged `examples/array_literal.pgy` shape succeeds through
  public/native MIR and C. Changing only its third index to `String` leaves
  public MIR successful, makes public C reach malformed receipt, and makes
  native MIR/C reject with the same exact index identity.
- Direct self-driver text and JSON MIR diagnostic modes currently exit zero and
  publish the invalid declaration-backed expression. The missing boundary is
  therefore self semantic admission, not merely public receipt projection.
- Source inspection confirms the graph already owns `AstExpressionNodeIndex`,
  its right child, and a scalar type result: the current index type owner
  returns no element type when the right child is not `Int`, but no graph-wide
  verdict consumes that fact for a nested call argument.
- SoT opens unchanged at `88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`, with 9
  blockers. This is one reached executable consumer seam inside the active
  semantic artifact family, not a row closure. Project forecast remains 83%.

## Coordination bounds

- Independent edit scope: the existing graph scalar verdict and expression
  verdict consumer, exact diagnostic vocabulary/receipt mappings, two invalid
  and one valid fixture, one focused production gate, Make/installed aggregate
  wiring, the applicable SoT evidence note, and current coordination snapshots.
- Forbidden overlap: no other task may edit or publish this executable rung.
  Native semantic owners, C/LLVM diagnostic transports, collection ABI,
  heterogeneous literal semantics, protected untracked paths, and concurrency
  direction documents are read-only.
- Allowed budgets: source/static gates within 60 seconds, the focused parity
  gate within 5 minutes after one required self-driver rebuild, and one
  installed CLI integration shard within 30 minutes. Full matrices run only
  after publication.
- Integration owner and gate: the primary task owns integration; the focused
  gate above is the falsifier and
  `tests/self_hosted/parity/installed_driver_cli_mode_owner.sh` is the single
  local integration boundary.
- Outputs remain implementation candidates until the focused and integration
  gates are observed green. No source edit, fixture, or test result changes
  SoT status or progress by itself.

## Local implementation evidence

- `SemanticExpressionGraphIndexAccessErrorFromTree` walks the already admitted
  graph from the expression root, including call callee/argument spines and
  ordinary child topology. A reached `Index` consumes its right-child scalar
  type and returns `array_index_type_mismatch` unless it is exactly `Int`.
- `SemanticAstExpressionVerdictFromGraph` is the one last consumer. No parser,
  native semantic, MIR builder, C/LLVM backend, process wire, or message parser
  gained array-index policy.
- The new diagnostic vocabulary row maps only the exact Pergyra code to
  `PGY_SEM_TYPE_MISMATCH`, `semantic:array_access:index_non_int`, and
  `use-int-index`. Its wording-independence contract is executable and the
  vocabulary count ratchet is now the observed 40 rows.
- A fresh Pergyra-built DRV-2 is installed at SHA-256
  `D4E2125E3E5164330145D93C2A9660E7CB7E24A3186EFBB3ABFFF7FFEF87DB9B`.
  Direct private text/JSON, public/native MIR/C/LLVM, the declaration-backed
  pair, deletion minimum, and valid C execution pass the focused gate.
- The complete installed-driver aggregate passes, including all pre-existing
  public diagnostic receipt identities and the new index gate. Compiler-world,
  diagnostic catalog/registry/layering, source inventory, documentation, Bash
  syntax, and diff checks are green. The broad component inventory remains an
  exact-CI obligation under its observed over-60-second local cost.
- SoT remains `88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`, with 9 blockers;
  project forecast remains 83%. Publication and exact-head CI are pending.
