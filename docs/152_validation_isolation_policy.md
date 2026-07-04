# Validation Isolation Policy

Status: `beta-closure-validation-policy`

This document fixes how Pergyra chooses validation scope during source-of-truth
closure and hard self-host work.

The purpose is not to avoid evidence. The purpose is to respect the ownership
boundaries that the compiler already created. If a fact is isolated behind one
owner, unrelated surfaces must not be re-executed just because a CI summary
lists many targets.

## Core Rule

Run a gate only when the changed owner can affect the artifact that gate owns.

Read-only inspection is always allowed. Case execution, backend compare shards,
full CI wrappers, and `test-all` are not default follow-up actions. They require
one of these conditions:

- the changed owner emits or mutates the artifact that the broader gate checks;
- the narrow owner gate reports a cross-boundary failure;
- a release, merge, or push checkpoint explicitly asks for broad confidence;
- the user explicitly asks to run the broader gate.

A CI failure list is triage input, not a dependency graph. First inspect the
failed log tail, identify the owner surface, and then choose the smallest gate
that proves that owner did not drift.

## Isolation Surfaces

| Surface | Owned Artifact | Narrow Evidence | Escalate Only When |
|---|---|---|---|
| Build/source inventory | Makefile source lists, CI shell selection, generated source inventory | `build-source-inventory-test-smoke` | a source list, build script, shell runner, or generated inventory path changed |
| Lexer/parser | token stream, compact AST text, parser diagnostics | lexer/parser focused smokes, frontend fixture for the changed syntax | downstream behavior changed because the token/AST contract changed |
| Semantic/type/DAG | structured diagnostics, checked type facts, resolver metadata | `semantic-core-shape-test-smoke`, `type-resolution-*` gates, focused semantic fixture | the change alters emitted IR facts or backend-visible type metadata |
| AIR/evidence | intent/effect/authority/coordination evidence, AIR JSON | AIR schema/drift/intent-compression gates | backend erasure/materialization or self-host evidence consumes the changed AIR fact |
| MIR CFG/body | CFG, branch/join, cleanup, local type facts, body dataflow | `cfg-body-dataflow-test-smoke`, focused MIR fixture | a backend consumes the changed MIR body fact |
| MIR declaration inventory | `MIRDeclHeader`, `MIRRoutine` signature facts, binding rows, method/field/generic metadata | `mir-declaration-inventory-test-smoke` | emitted C/LLVM shape changes for a fixture that consumes the changed declaration fact |
| ABI/layout/runtime shape | slot/pin/handle ownership shape, layout facts, runtime ABI boundary | ABI/slot/runtime shape gates for the touched owner | a backend projection or runtime boundary consumes the changed ABI fact |
| C backend projection | emitted C text and C run behavior | C-focused smoke or named fixture for the changed emitter | LLVM parity is needed because the same MIR/AIR/ABI fact is shared |
| LLVM backend projection | emitted LLVM IR and LLVM run behavior | LLVM-focused smoke or named fixture for the changed lowering | C parity is needed because the same MIR/AIR/ABI fact is shared |
| Backend compare | C/LLVM behavioral parity on committed cases | named fixture or shard only for the changed projection surface | user asks for broad parity, or a shared projection owner changed |
| Self-host rung | Pergyra implementation artifact compared to oracle | the rung's contract smoke or focused parity fixture | the rung is being promoted or a shared compiler-world owner changed |
| Docs/proofs | documented contract, proof model, methodology binding | documentation/proof smoke for the touched document family | implementation code also changed the contract being documented |

## Protocol

For every source-of-truth change:

1. Name the owner surface before running anything.
2. Inspect the changed files and choose the narrow evidence row from the table.
3. Run no case if the evidence can be established by read-only inspection or
   documentation review.
4. If execution is needed, run only the narrow owner gate first.
5. Escalate to targeted fixtures only when the narrow gate proves the changed
   fact reaches that fixture's artifact.
6. Escalate to full backend compare, `test-all`, platform CI wrappers, or shard
   sweeps only after explicit user approval or a release checkpoint.

The same rule applies when fixing CI. A failing target outside the touched
surface is not automatically part of the current change. It is a separate owner
failure until its log tail proves otherwise.

## Example: MIR Generic Metadata In C Specialization

Changing `src/codegen/transpiler_generic_specialization_emit.c` to consume
`MIRDeclGenericParam` facts touches the MIR declaration inventory surface and
the C generic-specialization projection.

Allowed default evidence:

- inspect the diff for AST semantic rereads;
- run `mir-declaration-inventory-test-smoke` if execution is allowed.

Optional targeted evidence, only when requested:

- backend compare fixtures that exercise generic specialization.

Not default evidence:

- slot contract;
- checked arithmetic;
- semantic core shape;
- full backend compare;
- platform CI wrappers;
- `test-all`.

Those gates own different artifacts. Running them without a crossed owner fact
weakens the isolation discipline the compiler is trying to prove.

## Hard Self-Host Impact

Hard self-hosting does not loosen this policy. A self-hosted rung must still
name its owner, oracle, artifact, and narrow evidence. A parity bundle is
load-bearing only for the rung it owns. It must not become a habit of rerunning
unrelated compiler surfaces after every isolated SoT edit.

