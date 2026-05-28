# `src/self_hosted/` -- Soft Self-Host Track

This directory holds **Pergyra-language implementations** of
compiler-adjacent tools. It is the working surface for Stage 1 (soft
self-host) and Stage 2 (partial self-host) of the staged roadmap.

**This directory is not the compiler core.** The C implementation under
`src/` remains the oracle for soft and partial self-host. See
[docs/self_hosted/01_staged_roadmap.md](../../docs/self_hosted/01_staged_roadmap.md).

## Entry Contract

Every tool in this directory must satisfy the agent-entry contract
([docs/self_hosted/00_agent_entry.md](../../docs/self_hosted/00_agent_entry.md)):

- **one** explicit input contract,
- **one** explicit output contract,
- **one** smoke test,
- **one** parity check against the C implementation.

A tool that does not yet pass this contract is a *scaffold* and must say so in
its `intent.md`.

## Layout

```
src/self_hosted/
  README.md                       -- this file
  PROGRESS.md                     -- honest substitution percentage
  tools/                          -- Stage 1 soft self-host tools
    <tool_name>/
      intent.md                   -- input/output contract + oracle
      main.pgy                    -- Pergyra implementation or scaffold
      expected/                   -- baseline outputs the C oracle agrees with
  parity/                         -- C / LLVM / Pergyra comparison harness
    README.md
```

## Current Status

- **2026-05-26** -- directory bootstrapped. First tool:
  `tools/diagnostic_catalog_checker/`. Pergyra implementation is a rung-2
  partial checker; the C-side reference still lives in
  `tests/diagnostic_registry_smoke.sh`. Parity rung 1 checks clean-repo exit
  class and schema-header visibility; rung 2 checks clean JSON plus live
  `codes`, `documented`, `missing`, `duplicates`, and `orphans` counters
  against shell drift detectors, then verifies a synthetic missing-code fixture
  returns `ok:false` with a `findings[]` entry and exit code `1`, plus a
  missing-input fixture that returns `input_error` and exit code `1`. **No claim
  of self-host is implied by the existence of these files.**
- **2026-05-27** -- soft self-host track now contains **10 rung-2 tools**:
  diagnostic catalog checker, stable subset section checker, AIR graph JSON
  validator, backend output comparator, module manifest resolver, stdlib
  dispatch inventory checker, doc link checker, production header size checker,
  production C size checker, and examples inventory checker. The shared
  `lib/text_scan.pgy` owns reusable scan helpers used by multiple tools.
  `make self-host-preparation-test-smoke` now runs every parity script, not just
  the scaffold check. This is dogfood evidence only; the compiler core remains C.
- **2026-05-28** -- first compiler-internal substitution candidate:
  `tools/lex_minimal/` reimplements the token output for `examples/hello.pgy`
  and `examples/array_literal.pgy` in Pergyra and gates byte-equality against
  `pgy --tokens`. This is rung-1 only: it proves the side-by-side substitution
  loop, not full lexer parity.

## Non-Negotiable Rules

1. Do not start a full compiler rewrite from this directory.
2. The C compiler remains the oracle during soft/partial self-host.
3. Prefer stable JSON/IR inputs over direct compiler internals.
4. A tool ships only when its parity check passes the C oracle.
5. Build artifacts belong under ignored scratch space such as `.tmp/`; do not
   leave compiler outputs beside `main.pgy` sources.
