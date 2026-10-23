# Nominal-field storage identity and bounded nested Clone

Date: 2026-10-23 KST

## Objective card

- Objective: keep constructor fields as distinct storage paths and admit only
  the nested Array Clone layouts that both native backends copy independently.
- Priority: semantic identity, fail-closed unsupported layouts, behavioral
  falsifiers, then regression breadth.
- Fact owners: the checked `SemanticNominalResultSnapshot` owns the mapping
  from each constructor actual to its field; the publication result relation
  owns whether `Clone` is physically independent.
- Last consumer: the default-aggregate move walk uses those facts to select a
  copy landing before a shared descriptor can be changed.
- Forbidden fallback: granting exclusivity to every member move, or treating
  all recursive aggregates as deep-cloneable.
- Falsifiers: sibling fields constructed from one Array must print `99, 1`
  after only the first field is changed; nested Array Clone must preserve the
  copied value after the original inner Array changes; unsupported aggregate
  Clone remains refused.

## Fixed candidate

- Parent revision: `78824421763967c1edfe30987bbf6e997931d642`.
- Candidate binary SHA-256:
  `02a8e9a8753dab410d35bdf5f553260287bcf653a386bd22d868ae9347ab456c`.
- Field-path owner SHA-256:
  `39574b93de4a513a50e291065d3c219b5cd93cc8f9f7a41eb5a52b435efea60d`.
- Clone classifier SHA-256:
  `b1338ecdf1fb297f1dd7c4c05b8c747ee00a34b6c08d1bbdc9ba992889922b00`.

## Before and after

The parent binary compiled the sibling-descriptor falsifier and produced
`99, 99`: both constructor actuals were recorded at the aggregate root, so a
change through `.first` was also visible through `.second`. The parent binary
also refused the later use of an explicitly cloned `Array<Array<Int>>` even
though the C and LLVM runtimes already cloned that exact layout deeply.

With the candidate:

- sibling descriptor C output: `99, 1`;
- sibling descriptor LLVM output: `99, 1`;
- `nested_array_copy_smoke.sh`: PASS, including C/LLVM Clone parity;
- `function_param_flow_summary_smoke.sh`: PASS, including unsupported struct
  Array Clone refusals and one copy landing for each newly supported nested
  Array case;
- `string_clone_release_smoke.sh` with Clang: PASS for C and LLVM; deleting
  emitted cleanup is detected as a leak in both backends;
- native transpile tests: 1,014 passed, 0 failed;
- native semantic tests: 3,240 passed, 0 failed;
- self-host component contract: 14 checks and owner line caps PASS.

## Open evidence

`aggregate_storage_transfer_smoke.sh` still reports the same 12 pre-existing
mismatches: six admitted legacy alias cases across C and LLVM, plus the
`post_field_return` diagnostic mismatch on both backends. The new sibling
positive adds no failure. Those rows are not reclassified here because an
admitted automatic copy and a real alias defect require different ownership
evidence.

This record does not claim V1 closure, canonical admission, bootstrap, an
installed-driver pass, or CI green. Those remain integration conditions on a
single published commit.
