# Backend ABI Layout Contract Checker Intent

## Intent

Guard MIR-owned ABI layout and runtime-call truth at backend consumption sites.
The checker prevents C, LLVM, and self-hosted backend code from reintroducing
layout spelling, runtime function spelling, or compatibility aliases as local
backend facts when the ABI row owner already publishes them.

## Input Contract

The tool consumes ABI layout contract rows from
`src/self_hosted/compiler/abi_layout_row_owner.pgy`. Required-path and
forbidden-term rows are the only semantic input. The tool reads repository files
from the current working directory and uses `report_owner.pgy` for all JSON
finding and report emission.

## Output Contract

The tool emits one JSON line with schema
`pgy.selfhost.backend-abi-layout-contract.v1`. It exits `0` when every required
term is present and every forbidden term is absent. It exits `1` with
`findings[]` for missing input files, missing required terms, forbidden hits, or
the negative self-test modes.

## Oracle

`tests/self_hosted/parity/backend_abi_layout_contract_checker_parity.sh`
compiles this tool through the C backend and, when available, the LLVM backend.
The clean artifact byte-matches `expected/clean.json`; the missing-required,
missing-input, and forbidden-hit self-tests byte-match their expected JSON files
and exit `1`. The shell harness may provide fixture paths and compare artifacts,
but ABI layout truth stays in the Pergyra owner rows consumed by this tool.
