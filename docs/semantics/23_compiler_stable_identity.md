# Compiler Stable Identity Contract

Status: `partial-gate-backed`

## Stable Surface

`SyntaxNodeId` is the nonzero `uint32_t` identity assigned by
`src/parser/ast_identity.c`. A parser result receives a complete preorder
assignment. An import-resolver result receives a second complete assignment
only after all imported statements have been normalized and merged. Therefore
the merged program, not any pre-merge module-local numbering, is the identity
domain consumed by semantic lowering, MIR source-shape capture, and codegen.

The assignment owner uses a `uint64_t` cursor and refuses values above
`UINT32_MAX`. Parser and import boundaries propagate that refusal instead of
returning a partially numbered artifact.

## Identity Domains And Epochs

`SyntaxNodeId` is stable only inside one finalized program revision. A native
parser artifact and a self-host artifact produced from the same source may use
different numeric values because they are different producers and currently
do not share a lossless syntax-identity graph. The self-host compact typed AST
uses its own arena identity; it must not guess native numbering from text-row
ordinals or a constant offset.

Serialized `source_syntax_id` values therefore identify facts inside the MIR
document's own revision. Every reference in that document must join the same
owner epoch. A transform that reconstructs a program starts a new identity
epoch and must remap declaration IDs and all dependent references, including
domain-topology field IDs, atomically. Keeping an old topology ID beside a new
declaration ID is invalid even if their names match.

## Invariants

1. `0` is invalid and means that no syntax identity exists.
2. Every node reachable from one finalized program has one unique ID.
3. Import merge must reassign the whole merged program before returning it.
4. A consumer must not join nodes from different finalized programs by raw
   `SyntaxNodeId`; revision and source-unit handles are still required for that
   cross-program case.
5. Overflow is a structured compilation failure, never saturation at
   `UINT32_MAX` and never wraparound to `1`.
6. A serialized field reference joins `(owner, field name, source_syntax_id,
   field_kind)` within one revision; raw ID equality across producers is not a
   parity requirement.
7. Canonicalization either remaps an identity family and every dependent edge
   together or rejects the transform. Numeric offset repair and name-only
   compatibility lookup are forbidden.

## Executable Evidence

`make stable-identity-test-smoke` uses two imported modules whose captured
lambdas previously both received ID `13`. The old artifact emitted duplicate
`pgy_lambda_env_13` declarations and failed native C compilation. The gate now
requires two distinct emitted identities and matching C execution. Linux CI
also runs the same fixture through LLVM.

The gate additionally locks the checked API, the post-merge reassignment, the
overflow guard, and negative duplicate/zero-ID mutations.

## Remaining Obligation

This closes uniqueness within one merged program. It does not yet close the
full stable-identity gate from `docs/180_compiler_logical_spine_handles_gates.md`:

- `CompilationRevisionId` and `SourceUnitId` remain implicit; provenance still
  uses `origin_path` strings.
- routine, entity, symbol, type, boundary, block, instruction, and value joins
  still need typed identities and foreign/stale-ID rejection.
- a serialized identity table and incremental invalidation policy are not yet
  owned.
- the self-host parser still needs lossless `SourceSyntaxIdentityFacts` for
  hidden type/expression nodes and canonical child order; compact AST text
  cannot reconstruct the native traversal generally.
- non-empty MIR-to-AST canonicalization still needs one atomic declaration and
  topology identity-remap owner. Until then it intentionally fails closed.

Accordingly the status is partial, not a claim that cross-layer identity is
closed.
