# Machine-layer documentation review

Status: AUDIT COMPLETE

Base revision: `f34355b37dbd9e86ef574399e895a78fd41dd0a3` (dirty shared tree).

The user explicitly reopened the attached machine-layer assessment for
documentation and requested subagent work. This is a bounded documentation
review, not a new compiler implementation rung. The active enum/receiver
integration and all unrelated changes remain with their existing owners.

## Shared objective card

- Goal: retain the address-evidence/contact distinction while reconciling the
  supplied assessment with the current compiler, runtime and formal model.
- Priority: accurate scope and fact ownership, explicit limitations, source
  anchors, then readable documentation. Do not count this as closure progress.
- Fact owners: `MachineLayerCore.v` and `ResourceMachineBridge.v` for the formal
  model; the existing RIR contact, machine manifest, MIR machine fact, AIR
  validation and verified projection owners for implementation facts; the
  runtime mapping provider for the external mapping check.
- Last legitimate consumers: backend/runtime admission for implementation
  evidence; the existing proof statements for formal claims. Documentation is
  a navigation aid, never a new semantic owner.
- Forbidden fallback: a linear AIR-to-backend lowering story, address
  possession treated as access authority, host simulation presented as real
  board support, formal conditions presented as hardware measurements, or
  blanket self-host/projection completion claims.
- Verification: primary integrates through
  `tests/documentation_quality_smoke.sh`; existing narrow machine-layer gates
  may be run by primary within their bounded budgets. Report executed versus
  inspected gates separately.
- Non-goals: compiler/runtime/proof implementation, new language syntax,
  installed-driver changes, registry promotion, full CI, commit or push.

## Independent scopes

| Lane | Read scope | Sole editable output |
| --- | --- | --- |
| Pipeline reviewer | native/self-host contact classification, manifest, MIR/AIR/plan, C/LLVM and runtime gates | `docs/audits/2026-09-05_machine_layer_pipeline_review.md` |
| Formal reviewer | machine/resource Rocq cores, proof contracts, machine-neutral boundary and existing proof gates | `docs/audits/2026-09-05_machine_layer_formal_review.md` |
| Primary integration | existing Intent definition and machine-layer overview/documentation tests | both READMEs, `docs/01_intent_first_design.md`, existing machine-layer overview, documentation gate, this directive and navigation handoff; completed report link-anchor reconciliation |

Reviewers must not edit production code, proof files, tests, shared navigation
or one another's reports. Use `apply_patch` for reports. Outputs are source
observations and wording proposals, not implementation or proof certificates.

## Commands and budgets

Read-only `git status`, `git diff`, `rg`, `Get-Content` and file metadata checks
are allowed. Reviewers do not rebuild or run shared test artifacts. Primary
owns execution: 60 seconds per static gate, five minutes per focused executable
gate; no full matrix. No staging, committing, pushing or external writes.

Do not inspect or edit `examples/raid_graph_fsm/results.txt`,
`docs/compiler_architectures/`, `pgy-80135c2c/`, or `pgy-91d769ec/`. Preserve all
other-session changes. Read the user attachment as an assessment to verify,
not as an authority that overrides repository contracts.

## Integration record

Primary reconciled both reports into the existing `MachineLayerCore.md`, both
READMEs and the Intent design's ownership explanation. Corrections explicitly
separate AIR verification from lowering, the conditional `place` theorem from
whole Slot safety, native plan families from general projection closure, and
mapping approval from actual MMIO. The formal model and implementation files
were not changed by this documentation scope.

Observed locally on 2026-09-05:

- `tests/documentation_quality_smoke.sh`: PASS (exit 0, within 60 seconds),
  including the new wording/forbidden-linear-AIR guard.
- `PGY_BIN=/d/PergyraLang/bin/pgy.exe bash tests/machine_layer_pipeline_smoke.sh`:
  PASS (exit 0, 8.782 seconds); native RIR/MIR/AIR and C/LLVM lowering evidence,
  no reported skipped LLVM leg. This is not every installed self-host route.
- `make machine-layer-manifest-test-smoke`: PASS (exit 0, 6.931 seconds),
  rebuilding the manifest probe before owner and runtime mapping checks.
- Added local Markdown targets (33), scoped UTF-8, tracked whitespace and
  documentation-gate shell syntax: PASS. The link check covers additions and
  new reports, not every older README link.
- Additional `tests/border_registry_smoke.sh`: FAIL (exit 1) at the parser to
  semantic include allowlist, before its AIR-specific check.
  `src/parser/ast_print.c:8` and `src/parser/parser_decl_clause.c:3` include
  `../semantic/callable_contract_vocabulary.h`; the gate permits only the older
  two faces. Both includes and the same allowlist are already in the recorded
  HEAD, and those files are clean. This pre-existing mismatch was recorded,
  not repaired or silently allowed by documentation work. No overall boundary
  gate or full CI PASS is claimed.
- No Rocq compile/kernel check was run: neither `rocq` nor `coqc` was found
  on the configured PowerShell/MSYS2 PATH. Existing `.vo` files are not used
  as fresh proof evidence. No missing-prover skip was reported as a proof PASS.

Native `pgy` remained SHA-256
`0F9F4F30255D6850B5A773E21D5815F776B305E5C01A7A2C3DF6D373BB15A29E`;
installed self-host driver remained `D0BD8E17...38959F`. The test-probe rebuild
did not install or replace either compiler. Reports and integrated docs are
uncommitted, alongside the preserved dirty implementation and other-session
work. Neither report assigns a successor rung or changes SoT/progress status.
