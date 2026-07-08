# Backend AIR Access Checker Intent

## Intent

Guard the AIR verification-only boundary for backend sources. Backends may run
after AIR and MIR evidence is validated, but backend code must not include AIR
headers or recover semantic facts from AIR graph node types.

## Input Contract

The tool consumes the backend AIR access contract rows from
`src/self_hosted/compiler/backend_air_access_contract_owner.pgy` and walks the
configured backend source root from the repository working directory. It only
checks source extensions and forbidden terms published by that owner.

## Output Contract

The tool emits one JSON line with schema
`pgy.selfhost.backend-air-access.v1`. It exits `0` when all scanned backend
sources avoid the forbidden AIR terms. It exits `1` with `findings[]` when the
negative self-test is requested or a forbidden AIR access term is found.

## Oracle

`tests/self_hosted/parity/backend_air_access_checker_parity.sh` compiles this
tool through the C backend and, when available, the LLVM backend. The clean
artifact must byte-match `expected/clean.json`; the forbidden-hit self-test must
byte-match `expected/forbidden_hit.json` and exit `1`. Shell owns only harness
execution and artifact comparison, not a second AIR/backend access policy.
