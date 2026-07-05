# Hard Self-Host Contract

Hard self-host is active as staged substitution. It is not a claim that the
released compiler is already built by Pergyra code.

The contract is stricter than "there is Pergyra code in `src/self_hosted/`":
a Pergyra implementation counts only when it substitutes a real compiler stage
or pass, runs in CI, and agrees with the oracle on the owned artifact.

## Definition

A hard self-host rung is active only when all are true:

- the Pergyra implementation has an explicit `intent.md` input/output/oracle
  contract;
- the C compiler remains the primary oracle during hard substitution;
- LLVM remains the second oracle whenever the current build enables it;
- the Pergyra implementation is the third value in the comparison, never the
  tie-breaker between C and LLVM;
- diagnostics, AIR JSON, MIR JSON, emitted ABI shape, runtime materialization
  classification, generated C, run stdout, or another declared owned artifact
  is compared byte-for-byte or by a documented stable schema;
- the rung is wired into `make self-host-preparation-test-smoke` or a named
  focused target that the preparation gate calls.

## SoT Rule

SoT is not a separate cleanup project during hard self-host. It is a pass
condition.

A self-hosted implementation must not guess an AST, MIR, AIR, DAG, ABI, or
diagnostic fact from an older representation when a typed owner fact should
exist. If a fact is missing, the fix is one of:

- add the missing fact to the owning IR or verifier;
- add a structured unsupported diagnostic for the rung;
- extend the declared input contract and compare that extension against the C
  oracle.

The fix is not a local C-side compatibility fallback, a self-hosted text parse
that reconstructs hidden semantic meaning, or a success path that silently
chooses a default.

## Bridge Rule

Bridge code is allowed. Fallback is not.

Allowed bridges:

- shell scripts that run the C compiler oracle;
- shell scripts that run the LLVM oracle;
- generated text artifacts such as `--tokens`, `--ast`, `--mir-json`,
  `--air-json`, emitted C, diagnostics, and stdout;
- scratch copies under ignored `.tmp/self_hosted/...` build directories;
- negative fixtures that prove clean rejection for out-of-subset input.

Forbidden fallbacks:

- self-hosted code rereading source AST text to recover a semantic fact that
  should be a MIR/AIR/DAG/ABI fact;
- C or LLVM backends accepting a stable path by guessing a missing MIR fact;
- JSON or diagnostic output invented in `main.pgy` when a shared library owns
  the format;
- advisory-only checks for a rung that is counted as compiler-internal
  substitution.

## Owner Shape Rule

Hard rungs are organized by source-of-truth owner, not by a folder-local
monolithic `main.pgy`. For active compiler-stage rungs, `main.pgy` is the CLI
entrypoint only: it may import owners, read process arguments through the
declared input owner, and call the run/lower/emit owner. It must not define
local helper functions or perform semantic control-flow, JSON fact lookup,
string scanning, diagnostic construction, or hidden compatibility parsing.

Every active compiler-stage owner file stays at or below the 600-line
split-review cap. When an owner grows past that threshold, split by the
responsibility that owns the fact; do not create a generic helper bucket.

The self-hosted source tree must also keep source owners separate from oracle
machinery. `src/self_hosted/` is for Pergyra source and owner documentation.
`tests/self_hosted/` is for parity scripts, committed fixtures, expected
outputs, and migration probes. Mirroring the C implementation's fragmented test
and harness layout inside `src/self_hosted/` is not a valid hard-self-host
architecture.

## Compiler World Rule

The self-hosted compiler flow is owned by
`src/self_hosted/compiler/world.pgy`. Stage directories own facts, but the
compiler action itself must be visible as Pergyra `world`, `zone`, and `intent`
declarations. The current compiler world names `PgyCompilerWorld` as the owner
and `CompilePergyraProgram` as the root compiler intent, then derives source
intake, lexing, parsing, semantic checking, MIR lowering, emission, and parity
as smaller stage zones/intents. Hard-substitution work should extend that
vocabulary instead of creating a second driver-shaped folder graph.

No Compiler World exception exists for the 600-line cap. `world.pgy` is a root
topology and manifest owner, not a place to accumulate every compiler-stage
detail. If compiler-world detail grows, split by stage intent cluster
(source-intake, frontend, middle-end, backend, parity) and keep stage facts in
their source-of-truth owners.

## Active Hard Rungs

The active hard rungs are:

- `src/self_hosted/lexer/`: token output parity against `pgy --tokens`;
- `src/self_hosted/parser/`: AST text parity against `pgy --ast`;
- `src/self_hosted/semantic/`: bounded diagnostic verdict parity against the
  C semantic accept/reject oracle;
- `src/self_hosted/codegen/`: bounded AST-text to C emission parity against
  C/LLVM run output;
- `src/self_hosted/mir_lower/`: MIR JSON fact-only lowering for the supported
  CFG subset and selected args/array/string/Bool/Float/file/recursion/struct
  fixture surfaces, plus clean rejection for unsupported declaration facts,
  chained through the self-hosted codegen and compared against the C backend
  oracle.

Peripheral tools under `src/self_hosted/tools/` remain useful dogfood, but they
do not count as compiler-internal substitution unless they replace a compiler
stage or pass.

Coverage probes may be broader than the hard rung. A probe can be cited as
progress only if it compiles its self-hosted tool from source, fails closed when
that compile does not produce a runnable tool, and cannot reuse a stale generated
binary. The parser scale probe is coverage evidence only; the hard parser rung
remains the committed C/LLVM byte-parity fixture gate.

## Promotion Ladder

Each candidate moves through this ladder:

1. Scaffold: source and `intent.md` exist, but no substitution claim.
2. Parity: Pergyra output agrees with the C oracle on committed fixtures.
3. Dual-backend parity: the Pergyra implementation builds and runs through C
   and LLVM where LLVM is available.
4. Hard substitution: the CI preparation gate treats parity failure as a real
   compiler regression.
5. Broadened substitution: the fixture set expands only after the previous
   rung stays green.

No step may broaden by adding a hidden fallback.

## CI Owner

`tests/self_host_hard_contract_smoke.sh` owns this contract. It does not replace
the heavy parity scripts; it proves the hard rungs, docs, and Makefile wiring
stay aligned so the heavy scripts remain load-bearing.

The Makefile keeps the fast and heavy paths separate:

- `self-host-preparation-contract-test-smoke` checks structure, manifests,
  documentation, owner shape, and compiler-world parsing.
- `self-host-preparation-parity-test-smoke` runs the heavy C/LLVM/Pergyra
  parity bundle.
- `self-host-completeness-smoke` is the M2 completeness ledger: it counts the
  production self-host source inventory through lexer, parser, semantic, and
  codegen stage checks. It also records cumulative pipeline intersections
  (`lex_parse`, `lex_parse_semantic`, and `full_pipeline`) and committed
  baseline source identities, so a previously passing pipeline file cannot
  disappear behind a count-preserving replacement. Unsupported codegen input is
  reported as a measured failure count, not as a successful skip. The current
  codegen stage consumes AST text emitted by the self-host parser, not
  C-oracle `pgy --ast`. The `full_pipeline` number is still a stage-check
  intersection, not a claim that typed self-semantic facts already feed codegen
  end to end.
- For focused local validation, `PGY_SELFHOST_COMPLETENESS_STAGES` may name one
  or more stages such as `parser` or `semantic,codegen`. Focused mode compiles
  and checks only those stage owners and enforces only their stage minima. It
  does not update or prove the cumulative pipeline identity ratchet; the
  unfiltered completeness smoke remains the CI/load-bearing proof. Makefile and
  platform CI step lists must not set this variable directly.
- `self-host-preparation-test-smoke` is the development/CI wrapper that runs
  both.

Normal compiler builds must not imply the heavy self-host parity bundle. Test
included verification is opt-in locally and mandatory only for the full
preparation gate.

Hard self-host validation also follows the repository validation isolation
policy in `../152_validation_isolation_policy.md`. A self-hosted rung may run
only the contract/parity evidence for the owner it substitutes unless a broader
compiler-world owner changed or the user explicitly asks for broad parity.
