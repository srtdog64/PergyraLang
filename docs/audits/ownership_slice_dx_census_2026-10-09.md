# Ownership cutover: actual Slice source census

Source boundary: all 2531 tracked `src/self_hosted/**/*.pgy` files, under
checkpoint `5651c916c87e030cc3ef579180abdfbf1814d1cc`; those source bytes are
unchanged by the subsequent native resource-flow producer repair.
Scratch probe/logs are in the frozen-baseline run directory recorded in
`ownership_cutover_frozen_baseline_2026-10-09.md` (`slice_dx_census.c`,
`slice_dx_census.mk`, `slice-dx-census.tsv`). The probe uses the repository's
real lexer and parser, not regex matches in comments/generated strings.
Every relevant file's AST Slice call count matches its lexer count; an
unvisited Slice construction refuses the census. The native object set is
isolated and no installed compiler is replaced.

| Source shape | Count |
|---|---:|
| Slice member constructions | 76 |
| Immediate call arguments | 76 |
| Local Slice constructions | 0 |
| Other construction parents | 0 |
| Slice formal parameters, default mode | 32 |
| Slice formals with own/ref/inout | 0 |
| Slice return type declarations | 0 |

The raw text search had 77 lines: one occurs in a contract's source-string
fixture and is not a construction executed by that enclosing Pergyra source.
The AST census excludes it. `ArrayLength(backing)` inside Slice's own argument
list precedes view construction; it is not, by itself, a live-backing conflict.

Decision: production epilogues use direct recovery jumps (CL6 reference stays
flag-based until its direct-jump/pointer refinement is checked). Compiler-local
writable views use CL7 whole-backing focus, without a resource lease ledger.
Do not apply its no-call restriction indiscriminately to the existing
call-bounded Slice arguments: readonly call duration/provenance and transitive
effects need their actual producer. Do not convert these calls to hidden
independent copies or add source annotations to sidestep that obligation.

This is a source-shape/DX census, not proof that every callee transitively
reads only, that arbitrary aliases are nonescaping, or that a full static
liveness/physical descriptor issuer exists. New writable local, retained,
returned, multiple-view and mutable-call cases remain P1 falsifiers. No P1,
compiler/runtime CLOSED or universal zero-DX-cost claim follows.
