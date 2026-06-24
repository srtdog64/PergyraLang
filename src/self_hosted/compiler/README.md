# Compiler World

This directory owns the self-hosted compiler world. It is not a C-side driver
mirror, and it is not a bucket for stage harnesses.

Pergyra self-hosting should be organized around the language's own surface.
`PgyCompilerWorld` is the owner, and `CompilePergyraProgram` is the root
compiler intent. The source unit flows through derived stage zones
(`SourceIntakeZone`, `LexingZone`, `ParsingZone`, `SemanticZone`,
`MirLoweringZone`, `EmissionZone`, and `ParityZone`), and each zone is driven by
a smaller intent. The stage owners remain under `lexer/`, `parser/`,
`semantic/`, `mir_lower/`, and `codegen/`; this directory records the
orchestration contract that makes them one compiler slice instead of C-style
fragments.

The compiler world also owns `StagePathManifest`, the canonical path fact for
self-host source roots, test roots, parity harness roots, and active stage
entrypoints. That gives future hard-substitution code a way to consume paths as
facts instead of rediscovering them with recursive scans.

`world.pgy` is the current scaffold. It is parse-gated by
`make self-host-compiler-world-contract-test-smoke` and wired into
`make self-host-preparation-test-smoke`. That gate also enforces
**manifest-to-reality conformance**: every stage `StagePathManifest` names must
own a real `src/self_hosted/<stage>/` directory with `.pgy` facts, and every
on-disk stage (a dir with `main.pgy`) must be named by the world — so the
architecture manifest cannot silently drift from the stage owners. It does not
claim that the released compiler is self-hosted; it fixes the shape that hard
substitution must grow into.

## Growth Rule

`world.pgy` stays under the same 600-line cap as other self-hosted owner files.
There is no compiler-world exception: if the root world grows, the extra work is
in the wrong owner.

The split unit is a stage intent cluster, not one file per tiny intent. Keep the
root file to:

- shared compiler-world subjects/objects;
- `StagePathManifest`;
- root zones and `PgyCompilerWorld`;
- the top-level `CompilePergyraProgram` flow.

When a stage needs more detail, move that detail into a named stage owner such
as a source-intake, frontend, middle-end, backend, or parity cluster. The root
world may name the cluster, but it must not absorb the stage's facts or hidden
fallback policy.
