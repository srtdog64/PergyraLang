# Self-Host Layering and Duplication Audit

Updated: 2026-08-04 (Asia/Seoul)

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
  `CheckFunction` appears twice with different signatures and responsibilities.

Exact file duplication is therefore not the main problem. The remaining debt
is responsibility density and coarse dispatch.

## Open layering debt

| Priority | Owner | Observed evidence | Required closure |
| --- | --- | --- | --- |
| P1 | `compiler/direct_mir_backend_projection_owner.pgy` | 252 lines, 24 imports, and routes selected by exact routine/block/instruction counts including 1, 3-7 | Replace each count branch only when an executable typed route reaches it. Add a negative ratchet forbidding each retired count classifier; do not create one grand dispatcher replacement. |
| P1 | `semantic/ast_expression_graph_fact_owner.pgy` | 616 lines against the 600-line repository ceiling; component contract is red | Split one owned subfact/view responsibility and return below the existing cap. Do not raise the cap. |
| P1 | `mir/json_projection_owner.pgy` | 605 lines; the default stage ceiling is 600 | Move one complete JSON section to a named projection owner while keeping MIR facts authoritative and byte/parity gates intact. |
| P2 | `codegen/emission/stmt_emit.pgy` | 621 lines under a file-specific 640 exception and 33 imports | Split complete statement families behind existing semantic statement facts, then remove the exception and restore the default ceiling. |
| P2 | `semantic/body_check_owner.pgy` and `codegen/emission/program_emit.pgy` | both declare generic `CheckFunction`; the semantic form is a legacy source-text scanner while the codegen form consumes AST/semantic state | Rename by responsibility immediately when either owner is touched. Delete the text-scanning semantic path only when typed facts reach its last consumer; no compatibility dual read. |
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
