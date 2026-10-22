# MIR program error-storage census

Recorded 2026-10-22 KST from local `main` base
`80c60521474685a15069a9071024059515833e3d` plus the measured source diff.
The unchanged canonical input was
`src/self_hosted/compiler/driver_bootstrap_main.pgy`.

## Change measured

- `SelfMirProgramFactsEmpty` no longer accepts expression-graph storage. It
  creates a valid empty graph internally.
- Initial artifact validation and domain projection therefore cannot transfer
  the admitted graph into a value that may become an error.
- `SelfMirProgramFactsError` rebuilds a fresh diagnostic-only value and keeps
  only the source node count, diagnostic code and diagnostic message.
- The successful lowering path remains the only production writer that
  attaches `analysis.expression_surfaces.expression_graph`, after growable MIR
  rows have stopped changing.
- The old constructor argument was removed from every source and fixture
  consumer; there is no compatibility read or graph-clone fallback.

The measured production source hashes were:

- program fact owner: `6804d89edc95752c172cc4eca36c53855ddef214d4dd20a058f89f0129cc8af2`
- domain projection owner: `fb60e6c1dd2108607dc9fef2fac2e3e7d7b553a3cbc92dfe615179dab6ea9b7a`
- artifact lowering owner: `c8e1d6b70fc31d1a36c370ca04203cc9674bb61b65194ccd402c618feddfd5db`

## Focused evidence

- The three changed production files and both affected fixtures loaded through
  the current native AST path.
- `tests/self_hosted_component_contract_smoke.sh`: PASS, including line caps
  and generator drift.
- Both changed shell gates passed `bash -n`.
- The error-storage falsifier seeds a partial graph with one root. The retired
  implementation, which copied `facts.expression_graph` into the error, would
  retain that row; the new contract requires a valid graph with zero roots and
  zero arena nodes.
- The C/LLVM behavior gate is registered after the rung builds its current C
  self-host driver, so it does not consult the stale installed driver. It is
  not recorded as a pass: the current canonical ownership baseline stops the
  native candidate before artifact publication, and the installed 2026-10-06
  driver cannot parse the current ABI owner.

## Fixed final-owner census

The prior successful instrumented compiler was reused unchanged
(`b1a0e7d3c83c50755d93b771dc9ca83d2e7b569598aecf36683edae8838ae31c`).
Only an exact snapshot of the current `src/` tree was supplied.

| Measure | Before | After |
|---|---:|---:|
| Final refusals | 342 | 332 |
| Outside `direct_mir_*` | 300 | 290 |
| Outside `direct_mir_*` moved-value refusals | 94 | 84 |
| Graph transfers into initial/error program facts | 10 | 0 |
| Hard census errors | 0 | 0 |

The run performed three landing analyses and the final analysis in 1154.78 s,
peaking at 3,980,968 KiB. A semantic multiset comparison found exactly the ten
intended removals and no added refusal. The raw comparison showed one retained
`ArrayDrop` row as removed and added because deleting the constructor parameter
renumbered its generated parameter syntax ID from `1015703` to `1015693`; its
location, node kind, callable identity and diagnostic meaning were unchanged.

The V1 finish line remains open: 332 ownership refusals remain, the unchanged
canonical program is not admitted, and bootstrap, installed-driver and
exact-SHA CI evidence were therefore not run for this red candidate.
