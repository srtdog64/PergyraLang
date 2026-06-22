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
- diagnostics, IR JSON, emitted ABI shape, generated C, run stdout, or another
  declared owned artifact is compared byte-for-byte or by a documented stable
  schema;
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

## Active Hard Rungs

The active hard rungs are:

- `src/self_hosted/lexer/`: token output parity against `pgy --tokens`;
- `src/self_hosted/parser/`: AST text parity against `pgy --ast`;
- `src/self_hosted/semantic/`: bounded diagnostic verdict parity against the
  C semantic accept/reject oracle;
- `src/self_hosted/codegen/`: bounded AST-text to C emission parity against
  C/LLVM run output;
- `src/self_hosted/mir_lower/`: MIR JSON fact-only lowering for the supported
  CFG subset, chained through the self-hosted codegen and compared against the
  C backend oracle.

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
