# Self-Host Layering and Duplication Audit

Updated: 2026-08-05 (Asia/Seoul)

This is a bounded architecture audit of the production self-host path. It is
not a general cleanup queue and it does not override the active executable
rung, the SoT registry, or executable gates.

## Objective card

- Objective: keep each admitted compiler fact behind one responsibility-named
  owner while the Pergyra implementation replaces real C-owned paths.
- Priority: semantic identity and one SoT, owner-directed facts, fallback
  removal, negative ratchet, then file size and conventional layering.
- Fact owners: the registered semantic/MIR/GraphPlan/runtime-ABI owners.
- Last legitimate consumers: the installed direct-C and direct-LLVM projection
  roots.
- Forbidden fallback: fixture/name/output routing, expression-text recovery,
  backend MIR reads, a second graph, generic helper buckets, or raising an LoC
  cap to accommodate mixed responsibility.
- Verification: focused executable parity and mutation gates first; component
  ownership/cap gates second.

## What was closed in the current rung

The `str_trim.pgy` rung displaced the reached unary String transform rejection
through GraphPlan v23. It kept expression identity, canonical signature,
readiness, runtime behavior, C/LLVM syntax, and target materialization in
responsibility-named owners. Small search/transform and scalar-runtime
compositors kept the existing 50-line C expression, 85-line LLVM expression,
and 20-line C materialization caps unchanged.

The first combined LLVM artifact then turned the previously recorded foreign-
declaration seam into an executable failure: concat and trim each emitted
`declare ptr @memcpy(ptr, ptr, i64)`. One plan-derived foreign-declaration owner
now emits each required `strlen`, `malloc`, `memcpy`, `strstr`, `strncmp`,
collection, conversion, and abort declaration once. Runtime materialization
owners retain only their semantic bodies. This is bounded declaration
composition, not a general FFI registry.

The executable evidence is
`tests/self_hosted/parity/one_mir_string_trim_projection.sh`. It proves exact
C/LLVM execution, display neutrality, semantic/already-trimmed/empty cases,
six malformed no-artifact families, and exact declaration cardinality in the
combined artifact. StringIndexOf, window, case/math replace, collection,
concat, and nested String regressions all remain green. Every new owner is in
`scalar_program_owner_caps.tsv`; no existing cap was raised.

### Previous StringIndexOf window closure

The `str_indexof.pgy` rung displaced the reached String search rejection
through GraphPlan v22 and closed three responsibility-density faults:

1. the arithmetic admission coordinator mixed an already established
   condition-bound proof with unrelated arithmetic families;
2. StringIndexOf result semantics, target expression syntax, and runtime
   materialization had no sealed owner shared by C and LLVM;
3. checked LLVM Substring materialization had accumulated inside the window
   compositor and would have exceeded its fixed cap when the native invalid-
   window contract was restored.

Condition-bound proof now has its own owner. A second exact proof admits only
the reached `p + 1` and `StringLength(source) - p - 1` relation from one
earlier same-block StringIndexOf definition over the same source. Canonical
signature, expression-kind/readiness, runtime-ABI/result-range, and target
materialization owners carry the search operation. LLVM checked Substring has
a responsibility owner and its former file is again a small compositor.

The executable evidence is
`tests/self_hosted/parity/one_mir_string_indexof_projection.sh`. It covers
display-only equality, semantic mutation, missing and empty needles, exact
C/LLVM execution, and seven malformed families with no artifact. Every new
owner is capped in `scalar_program_owner_caps.tsv`; no existing cap was raised.
No fixture route, output evaluator, second graph/emitter, backend MIR read,
generic helper bucket, or native fallback was introduced.

The runtime correction is also architectural evidence: the absent-search case
exposed that self-host Substring did not own the native invalid-window
contract. Both targets now derive checked/clamped behavior from the String
runtime owner instead of relying on valid fixture inputs.

### Previous ordered-call and String case/math closure

The `str_case_math.pgy` rung displaced the reached terminal multi-routine
rejection through GraphPlan v21. It exposed five layering faults that a
fixture route would have hidden:

1. ordered formal parameters were first represented as
   `Array<DirectMirRoutineParamFact>`, which is outside the current self-host
   aggregate ABI and also couples signature identity to storage representation;
2. the parameter JSON cursor did not advance after each admitted object, so a
   three-row signature re-read row zero and silently lost the route;
3. direct-call admission assumed exactly one argument instead of consuming the
   already persisted argument chain;
4. StringReplace/Abs/Min/Max semantics and materialization had no sealed runtime
   subfact shared by both targets;
5. plan construction still owned final digest/readiness/mutation verification
   and had drifted to 87 lines against its 85-line gate.

Ordered parameter identity is now a one-pass admission plus typed parallel
fact arrays; the first complete parameter survives only as the old bounded
projection. Direct calls use SyntaxNodeId and ordered n-ary rows. One runtime
subfact and target projections drive actual C/LLVM StringReplace and integer
math bodies. A depth-bounded constant-DAG magnitude owner proves the reached
addition without relaxing unbounded signed add. Plan construction delegates
one final verification owner and is 73 lines. Expression rejection also has a
shared row/source diagnostic instead of an opaque outer failure.

Every new owner is in `scalar_program_owner_caps.tsv`; no existing cap was
raised. The executable evidence is
`tests/self_hosted/parity/one_mir_string_case_math_projection.sh`, including
display and routine-order equality, semantic mutation, eight negative families,
and exact C/LLVM execution.

The `str_builtins2.pgy` rung displaced one exact four-block dispatch family
without opening a second graph. It also exposed and closed four responsibility
seams:

1. the shared GraphInput let the legacy String-array literal/mutation plan
   claim Split-produced definitions before the typed program arena;
2. expression shape readiness encoded `SubtractInt` as the last valid kind;
3. String and String-collection runtime requirements duplicated the same kind
   presence scan;
4. collection projection readiness called a function owned by its importer,
   leaving dependency direction implicit.

The program now uses a named GraphInput entry that suppresses only external
collection plans for this owner. The expression-kind identity owner supplies
the last kind, one query owner supplies kind presence, and one generated-ABI
readiness owner supplies both projections. The captured Array<String> ABI
compares every offset across definitions, and C derives assertions from that
fact. Existing caps stayed fixed; new owners have their own caps.

The `str_builtins.pgy` failure exposed two layering faults rather than a need
for a fixture exception.

1. A branch-shaped String route was incorrectly the only claimant for a
   general scalar builtin program. The new builtin route owner now admits a
   semantic envelope and delegates identity/signature checks to the canonical
   builtin registry.
2. The minimum sealed GraphPlan shape was hidden in an Array-reverse-named
   owner. It is now a general minimum-plan owner; Array and collection shapes
   retain their stricter conditions.

Multi-argument topology, typed n-ary operands, expression kind IDs, Bool
readiness, runtime-ABI identity/readiness, and target String-window syntax and
materialization were split into responsibility-named owners. Existing hard
caps were not raised. The C runtime's exact length and substring bodies now
have one block owner and are reused by the complete String runtime block.

The executable evidence is
`tests/self_hosted/parity/one_mir_string_window_builtin_projection.sh`.
It proves both targets, semantic output mutation, malformed signature/edge/type
negatives, no artifact on rejection, and no retry through the legacy local
inventory path.

## Measured inventory

- Exact duplicate SHA-256 groups among production self-host `.pgy` files
  (excluding fixtures and tools): **0**.
- Generic `helper` paths under `src/self_hosted`: **0**. This does not excuse
  policy-free naming inside other files; the rule remains shrink-only.
- Production files above 600 lines: four. One tool is also above 600 lines.
- Duplicate production function spelling other than entrypoint `Main`:
  `CheckFunction` and `CharAt` each appear in two owners with different
  signatures or responsibilities. `Die` also crosses a production text owner
  and a fuzz/tool boundary. These are not byte duplication, but the first two
  are composition and naming seams when their graphs meet.
- LLVM runtime foreign declarations now have one plan-derived composition
  owner. The formerly repeated `strstr`, `memcpy`, and `abort` spellings were
  removed from responsibility-specific runtime bodies after the combined
  StringTrim/concat artifact reproduced an actual duplicate-declaration error.

Exact file duplication is therefore not the main problem. The remaining debt
is responsibility density and coarse dispatch.

## Open layering debt

| Priority | Owner | Observed evidence | Required closure |
| --- | --- | --- | --- |
| P1 | `compiler/direct_mir_backend_projection_owner.pgy` | 252 lines, 24 imports, and routes selected by exact routine/block/instruction counts including 1, 3-7. The `str_builtins2` four-block semantic family is now claimed earlier by GraphPlan v20, but the generic exact-count branch still serves other programs. | Replace each remaining count branch only when an executable typed route reaches it. Add a negative ratchet for each displaced semantic family; do not create one grand dispatcher replacement. |
| P1 | `semantic/ast_expression_graph_fact_owner.pgy` | 616 lines against the 600-line repository ceiling; component contract is red | Split one owned subfact/view responsibility and return below the existing cap. Do not raise the cap. |
| P1 | `mir/json_projection_owner.pgy` | 605 lines; the default stage ceiling is 600 | Move one complete JSON section to a named projection owner while keeping MIR facts authoritative and byte/parity gates intact. |
| P2 | `codegen/emission/stmt_emit.pgy` | 621 lines under a file-specific 640 exception and 33 imports | Split complete statement families behind existing semantic statement facts, then remove the exception and restore the default ceiling. |
| P2 | `semantic/body_check_owner.pgy` and `codegen/emission/program_emit.pgy` | both declare generic `CheckFunction`; the semantic form is a legacy source-text scanner while the codegen form consumes AST/semantic state | Rename by responsibility immediately when either owner is touched. Delete the text-scanning semantic path only when typed facts reach its last consumer; no compatibility dual read. |
| P2 | `semantic/text_scan_owner.pgy` and `lexer/char_owner.pgy` | both declare generic `CharAt` for different text boundaries | Rename by responsibility when either owner is reached; do not create a shared helper unless one policy-free byte/character contract is actually identical and imported by both. |
| P2 | `air/mir_break_cfg_certificate_fact_owner.pgy` | 627 lines under an explicit 650 ceiling | Split certificate construction from readiness/projection when the break-CFG executable rung is next active; then lower the exception. |
| P3 | `tools/initializer_projection_probe/main.pgy` | 876 lines, but it is a test/probe tool rather than the production compiler path | Split scenario construction from reporting only when this probe is changed. It does not block self-host substitution. |

## Ratchet policy

LoC uses two boundaries: the global stage ceiling prevents responsibility
accumulation, and a lower responsibility-owner cap prevents newly split files
from becoming replacement buckets. Exceptions are temporary shrink targets,
not precedent.

No broad cleanup is opened while an executable rung is active. A listed item
becomes active only when it is the reached owner or blocks the focused gate.
The acceptance unit is always: move one fact behind its owner, migrate the last
consumer, fail closed on absence, delete the old read, and add a negative gate.
