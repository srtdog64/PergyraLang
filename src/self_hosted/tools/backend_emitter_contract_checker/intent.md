# Backend Emitter Contract Checker Intent

## Intent

Guard the backend dumb-emitter contract. Backend emitters must consume MIR and
ABI runtime rows instead of synthesizing names, layout spellings, or runtime
call spellings from local string rules.

## Input Contract

The tool consumes backend emitter contract rows from
`src/self_hosted/compiler/backend_emitter_contract_owner.pgy`. Required-path,
required-term, forbidden-path, and forbidden-term rows define the full scan
surface. The tool reads repository files from the current working directory and
uses `report_owner.pgy` for report construction.

## Output Contract

The tool emits one JSON line with schema
`pgy.selfhost.backend-emitter-contract.v1`. It exits `0` when every required
consumer term is present and every forbidden local-synthesis term is absent. It
exits `1` with `findings[]` for missing input files, missing required terms,
forbidden hits, or the negative self-test modes.

## Oracle

`tests/self_hosted/parity/backend_emitter_contract_checker_parity.sh` compiles
this tool through the C backend and, when available, the LLVM backend. The clean
artifact byte-matches `expected/clean.json`; the missing-required,
missing-input, and forbidden-hit self-tests byte-match their expected JSON files
and exit `1`. Shell owns harness execution and artifact comparison only; it must
not duplicate backend-emitter policy with grep logic.
