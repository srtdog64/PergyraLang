# Hard Self-Host Readiness Scorecard

This scorecard measures the ten non-negotiable capabilities from the gap
analysis (05) against the current tree. Each capability is gated by a smoke
test; tests/self_host_readiness_scorecard.sh verifies the gates are present and
prints the tier without a build. This document records the reasoning behind
each tier and the work that remains.

## Verdict

Hard self-hosting cannot start today. The infrastructure is mature: every one
of the ten capabilities has a gate, and the tree carries 73 smoke gates total.
The blocking distance is small and well bounded. Seven capabilities are ready,
two have a working subset that needs substrate breadth, and one is the open
item on the critical path. The process-argument tooling gap is closed by
`Args() -> Array<String>`, but it does not change the hard-self-host verdict
until compiler-internal substitution actually consumes it.

As a planning estimate, hard self-host readiness is roughly 85-90%: seven
capabilities are READY, two are SUBSET, and the only ACTIVE capability is now a
compiler plumbing deletion task rather than an unknown design gap. The
critical-path codegen SoT burn-down is closed: the codegen frontier went from
127 original source_ast reads to 0. The compiler-side source_ast tail is now 2
reads: the declaration-header payload assignment and accessor. Source-type/
location scalar provenance has been split to source_node names, and method and
field declaration back-pointers are removed,
and MIR validation no longer compares generic, enum, method, or field metadata
against original AST nodes. The remaining AST-returning declaration-header
compatibility API is now separately ratcheted as source_decl codegen 2 /
compiler 1, and routine source-decl compatibility is ratcheted at codegen 5.

## Tiers

READY means the capability is gated and its Phase 1 mechanism is complete.
SUBSET means it works over a limited surface and substrate maturity remains.
ACTIVE means it is on the critical path and still in progress.

| # | Capability | Tier | Gate | Remaining gap |
|---|-----------|------|------|---------------|
| 1 | Module/package resolver | READY | module_smoke, package_module_resolver_smoke, type_resolution_resolver_inventory_smoke | deterministic imports and cycle diagnostics gated; a resolver tool is already self-hosted |
| 2 | Collections + iteration | SUBSET | stdlib_surface_smoke, stage4_determinism_smoke | List/Set/HashMap exist over a key-type subset (String, Int, Long, Bool); MapKeys order is locked for stable key types and Stage 4 insertion-order determinism is gated; broaden symbol/record/handle keys and ordered set snapshots |
| 3 | String/path/Unicode policy | READY | unicode_policy_smoke, source_utf8_smoke, memory_string_safety_smoke | stable comparison and normalization stance gated |
| 4 | Arena/ownership ergonomics | SUBSET | verify_arena_closure, runtime_abi_lifetime_smoke, abi_ownership_shape_smoke | the allocation mechanism exists; the per-pass scratch/result/persistent lanes that remove manual boilerplate do not yet |
| 5 | CFG/MIR body as SoT | ACTIVE | cfg_body_dataflow_smoke, ast_read_surface_smoke, mir_or_abort_invariant_smoke, ast_read_surface_checker_parity | non_cfg fallback locked at 0; backend source_ast frontier locked at 0; compiler declaration-header payload remains at 2; source_decl is ratcheted at codegen 2 / compiler 1, and routine_source_decl_codegen is ratcheted at 5. This is task 74 |
| 6 | AIR as verifier | READY | air_json_schema_smoke, air_drift_smoke, air_backend_nonimpact_smoke | pgy.air.graph.v1 evidence export gated; drift count enforced at 0 |
| 7 | DAG type resolution SoT | READY | type_resolution_dag_smoke, type_resolution_resolver_inventory_smoke | recursive resolver compat path retired; metadata_dead_ends enforced at 0 |
| 8 | Scoped unsafe/raw escape | READY | raw_escape_contract_smoke | unsafe is scoped and capability-bound; raw pointers gated out of domain code |
| 9 | Debug info Phase 1 | READY | debug_hygiene_smoke | C #line directives and LLVM DILocation implemented |
| 10 | Runtime profile selection | READY | runtime_none_contract_smoke | runtime-none profile gated with diagnostics for unsupported features |

## Critical path

Capability 5 is the only ACTIVE item and the one true blocker. Its mechanism is
mostly done: non_cfg body facts come from MIR and are locked at zero fallback,
and backend source_ast readers are locked at zero. The work left is the
compiler-side deletion in 06: remove the declaration-header source declaration
payload boundary and the routine source-decl compatibility sites. Closing it makes CFG/MIR the unconditional source of truth, which
the gap analysis names as the precondition that lets compiler passes be
rewritten in the language.

## Next substrate work

After capability 5, the two SUBSET items are the substrate maturity that turns
soft self-hosting (compiler-adjacent tools, already real) into something that
can carry a compiler pass. Capability 2 now has stable `MapKeys` order for the
stable key subset, and `stage4_determinism_smoke` proves stable output across
insertion orders for C/LLVM-generated Pergyra programs. It still needs broader
symbol/record/handle key types and ordered set snapshots. Capability 4 needs arena lanes that remove manual resource
boilerplate from every pass.

Three concrete hard-self-host substrate/tool lifts remain after the current SoT
burn-down:

- broader deterministic collection iteration for symbol/record/handle keys and
  ordered set snapshots, beyond the Stage 4 HashMap key-subset gate;
- parser LLVM depth/type-inference parity, so parser dogfood is no longer
  effectively C-backend-only;
- deterministic filesystem directory walking, so self-hosted validators can
  enumerate source trees without shell `find`/`grep` ownership.

These are the axis the gap analysis calls systems substrate, distinct from the
domain-oriented surface the language is already strong on.

## Sequencing

The order that keeps each step verifiable is: close capability 5 (task 74);
broaden capabilities 2 and 4; expand the self-hosted tool set from validators
toward the MIR dump diff and resolver helpers named in 05; then, and only then,
rewrite the first real compiler pass, starting with the lexer, against the C
compiler as oracle. Starting with a parser, type checker, or backend rewrite is
explicitly out of order.

## Measured gaps (blocker burn-down)

The three non-READY capabilities were measured against the tree to make each
one actionable. Every remaining step needs the build loop, which is the reason
none can be closed from a static pass alone.

Capability 5 (CFG/MIR SoT, task 74). Mechanism mostly complete: non_cfg body
facts are MIR-owned and locked at zero fallback, the source_ast ratchet is now
codegen 0 / compiler 2, source_decl is ratcheted at codegen 2 / compiler 1,
routine_source_decl_codegen is ratcheted at 5, and the shared ratchet manifest
is verified by both the shell smoke and a
Pergyra-written ast_read_surface_checker parity rung. The
dead slot-source accessors have been deleted. Against the original codegen
frontier of 127, all 127 reads are retired. LLVM domain/role and C
hosted-method forward declarations now consume
MIRDeclMethod metadata without source AST back-pointers, and C/LLVM hosted
method bodies use the linked MIRRoutine as their body provenance owner. LLVM
nominal method registry and LLVM hosted/member call emission also no longer
read method source back-pointers in MIR-active paths. LLVM function routine
lookup now keys off MIR routine names instead of source AST identity. The
zone action effect-sync path also consumes MIRDeclMethod flags without method
source back-pointers, and C member-call emission only loads an AST method body
for the lazy projection-invalidation scan. Shared-field constructor defaults now
consume MIRDeclField initializer metadata instead of recovering source AST
nodes, and the unused shared-field source accessors are retired. C class/zone
specialization scans now reach method bodies through linked MIRRoutine
provenance instead of the hosted-method source view, and the C/LLVM routine
source thin aliases are retired. C/LLVM method body compatibility now goes
through backend helpers that follow the MIRDeclMethod routine link, so direct
backend method source back-pointer reads are retired. C MIR emission-contract
compatibility now validates routine kind/name/signature facts without opening
routine source_ast, and LLVM intent forward declarations now use MIR routine
binding metadata directly. Non-generic LLVM function routine forward
declarations now use MIR routine signatures directly by passing no source
declaration into the compatibility emitter; generic template registration
remains a source-declaration compatibility boundary. LLVM function routine
emit/validation now consumes MIR routine kind/name/generic facts without
recovering source declarations; source-declaration compatibility diagnostics
live at the body emit boundary. C host-method
lookup now performs method-name matching from MIRDeclMethod metadata, while
LLVM host-method lookup keeps its non-MIR fallback on the explicit compatibility
array and the unused LLVM hosted method source accessor is retired, along with
the thin LLVM MIR method source alias. C/LLVM routine source thin aliases are
retired, routine source declaration checks use compiler-owned
`mir_routine_source_decl_of_type`, declaration lookup uses
`mir_decl_header_source_decl`, and no backend `.c` file contains a source_ast
read. The remaining work is removal of the routine source-decl compatibility
sites and the compiler-side declaration header back-pointer after compatibility
lookup stops returning origin AST declarations, followed by a compiler
source_ast ratchet ceiling of zero. Method and field
back-pointers are already removed, and the old header-shape AST recomputation
arm is gone. Build-gated.

Capability 2 (collections). Measurement: integer keys are implemented
(pgy_runtime_map_int_key_inline.h covers i32 and i64), and `MapKeys` now returns
stable sorted snapshots for String, Int, Long, and Bool keys. The remaining gap
is broader key-type coverage and set iteration snapshots: a compiler needs
symbol/record/handle-like keys and deterministic traversal for stable output.
Build-gated.

Capability 4 (arena ergonomics). Measurement: the lanes already exist inside the
compiler. Pass-local scratch arenas are present in HIR (hir.h), MIR (mir.h, used
for SSA rename), semantic (semantic.c), and LLVM lowering (llvm_internal.h), and
a persistent arena exists in the registry. The mechanism and the
scratch-versus-persistent split are both in place. The remaining gap is narrow:
exposing the same lane discipline ergonomically to code written in the language,
so a rewritten pass does not hand-roll allocation boilerplate. This is the least
distant of the three. Build-gated.

The honest summary is that all three blockers are now mechanism-complete or
narrowly scoped, and each remaining step is a build-loop change rather than a
design question. The single critical-path item is capability 5; capabilities 2
and 4 are substrate breadth that follows it.
