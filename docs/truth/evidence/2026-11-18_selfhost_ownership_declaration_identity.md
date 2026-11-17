# Ownership joins consume declaration identity

Local integration observed 2026-11-18 KST on parent `6ff288d70`, with the
changes carried by this commit. No new production owner, release rule,
language syntax, annotation workaround or program-shape route was added.

## Reached seam

The scalar local issuer already preserves wire declaration identities when
spellings collide. The ownership join instead selected a local by spelling.
In the frozen complete DRV-2 MIR this affects 21 fact joins in seven routines:
`AstMatchCasePatternFactFromText`, `SemanticAstSignatureTypeTextShape`,
`SemanticAstCollectionAggregateTraceAffectedFields`,
`CodegenProgramFunctionDefinitionBlock`,
`SelfMirLowerTrackedStatementFromArtifact`, `EmitCanonicalNominalDecl` and
`DirectMirScalarProgramBuiltinSignatureFactForCall`. This is a wire/source
inventory, not a claim that every other admission stage accepts them.

The existing scalar identity owner now normalizes ownership-bearing,
noncolliding no-wire locals from the admitted declared-source prefix too.
Its shared binding query takes that prefix explicitly, so synthetic rows
with no declaration ID do not invalidate the prefix. Existing binding-move
callers preserve their complete-inventory ranges.

The transition join consumes only the canonical declaration ID and checks
name/kind agreement. It has no spelling fallback. Binding-move, owned
definition, block-flow and member-restoration callers share that same join.
Existing caps remain: local issuer 300/300, identity owner 33/40 and shared
binding-move owner 149/151 code lines.

## Generation

Editing authority: `D:\PergyraLang`; validation: WSL `/root/mg-main`.
Normalized production `.pgy` map: 2,840 files, SHA-256
`f6564345e611533d79d03c2e78f34da41dd889ac6e504068d97b09f2bda7225a`.

| Artifact | SHA-256 |
|---|---|
| Native seed | `97f33eab89e7e264ea37c6f9239b886e4129e639e3dc337c556e205216443c48` |
| Pergyra-built codegen gen2 | `ea6c8e08b8bf8d3ff143768be2ac269b339847d6345c27d464a989fd8b2fc1c8` |
| Codegen gen2 C == gen3 C, 124,083 lines | `8973cf4a2a5dc65411ed31f981c9d599caf2efc5bc73afe1deeed1394b7c5533` |
| Installed driver | `22b6a8f549c47d72e43384497ed56085772d67ac06fa5017de2feca9f334cb52` |
| Installed driver C | `e70908906da5defd1a1f6ebb534d1b0172f3617758a83a0e60f7b0575531437b` |

## Executed gates

Logs are under `.tmp/selfhost-memory-20261117/` in the validation tree.

- `string-binding-bootstrap.log`: full codegen bootstrap passed, including
  the actual fixed point, lexer/parser/semantic/MIR-lower, fourteen tools
  and generated fuzz corpus parity. Native build: zero errors, 17 existing
  unreachable-statement warnings.
- `string-binding-installed-build.log` and its validated process receipt:
  actual Pergyra-built installed driver passed its source/manifest smokes.
- `string-binding-gate.log`: existing 18+4 refusals and 8x2 plus 2x2 native
  executions remain; six new C/LLVM identity mutations refuse without an
  artifact. Six new executions cover the self-host shadowed source and both
  producers' noncolliding no-wire control. The latter's emitted MIR actually
  omits LocalRefs and carries an ownership row. Evidence directory:
  `.tmp/self_hosted/direct_mir_array_string_cleanup.8Tm8xB`.
- The declaration-prefix unit admits ID 17 while excluding its synthetic
  suffix. Missing, ambiguous, invalid and out-of-range declarations each
  exit 1 with the appropriate owned diagnostic.
- `string-binding-retire.log`: the String bridge's source parity, `-O0`
  ASan/LSan and five unchecked memory faults passed on the new seed
  (`string-retirement.W9qHfL`). Role C/LLVM execution, erased receivers,
  caller-cell mutation and source/MIR negatives passed; the hand-off reader
  preserved all 44 forged-row refusals (`string-binding-roles.log`,
  `string-binding-handoff.log`).
- Component, documentation and SoT checks passed. The ownership registry
  remains ACTIVE; no whole-language memory-safety closure is claimed.

## Full inputs and limits

The unchanged frozen complete MIR, SHA-256
`e7767d3059972797baf41654ececd912bfb91a0755547afea6336e6af8bbdf19`,
now passes the old transition refusal at ordinal 1054 and refuses at ordinal
1126, `SemanticExpressionGraphArenaEmpty`. Peak RSS: 3,543,780 KiB.
Its retired empty-literal constructor input and retained/handoff rows still
need the full lifetime/transition contract; no local exemption was added.
This is a later refusal, not a successful full compile or a speedup claim.
The 14 GiB/1,800 s receipt records exit 1 and a drained process group.

`string-binding-memory.log`, `.tmp/self_hosted/selfhost-memory.Eo8Zuk`:

| Complete input/path | Peak KiB | Existing ceiling KiB |
|---|---:|---:|
| Semantic MIR consumer | 776,668 | 819,200 |
| MIR-lower source-to-C | 5,249,316 | 8,388,608 |

Source/MIR C bytes agree, SHA-256
`b8029e4553b17f9ba56547fb45a4c66a7d473c4e81dfbc7bc4b96d0241e637e6`.
The full one-process DRV-2 source-to-C goal was not remeasured here; its
[last observed result is OOM](2026-11-18_selfhost_string_temporary_seed_bridge.md).

## Limits kept visible

Native MIR of the new shadow source hits an earlier block-inventory refusal
(a formal-based variant hits formal-wire identity). It is not counted as a
native passing shadow case. Existing native controls remain executed.
An initial native diagnostic probe used excessive memory before its limit
was corrected and was stopped; it is not acceptance or benchmark evidence.
The successful refusal trace instead used the installed driver's generated
C with only refusal logging, within the established process boundary.

Full S1/S2, compiler Clone execution transport and the shared storage guard
remain open. Publication and exact-SHA CI belong to [main](../state/main.md).
