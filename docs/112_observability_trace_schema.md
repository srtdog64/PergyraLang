# Observability And Trace Schema Beta Contract

Status: beta-freeze-source-of-truth.

This document freezes the beta observability/tracing schema. The stable
contract is intentionally narrow: intent `last/history/active/recent`, authority
failure snapshot, and backend-identical trace strings.

Executable gate: `make observability-schema-test-smoke`.

Runtime active-intent lookup uses the active-index handle table as the stable
fast path. The linear active-registry scan is only a malformed-index
compatibility fallback, and the index insertion path must reuse tombstone slots
in both the inline runtime path and exported runtime-lib path so repeated
enter/exit workloads do not permanently degrade to the fallback.
`PGY_INTENT_ACTIVE_INDEX_MAX` is used as a hash-mask table size, so it must stay
a power of two; both runtime paths enforce this with a preprocessor guard.
Executable gate: `make runtime-intent-observability-contract-test-smoke`.

## Stable Intent Schema

Stable intent query families:

- `IntentLast*`: last completed intent snapshot.
- `IntentHistory*`: step history for the last completed intent.
- `IntentActive*`: currently executing intent registry.
- `IntentRecent*`: recent completed intent ring.

Active registry aggregate queries use the active registry index: for example,
`IntentActiveName(i)`, `IntentActiveHandle(i)`, and
`IntentActiveStepCount(i)`. Active step field queries use a stable active intent handle plus a step index: for example,
`IntentActiveStepName(handle, step)` and `IntentActiveStepOk(handle, step)`.
Use `IntentCurrentHandle()` when querying the currently executing intent from
inside an intent step.

Stable fields:

- `name`
- `handle`
- `trace_id`
- `priority`
- `concurrent`
- `step_count`
- `failed`
- `trace`
- `failure`
- step `name`
- step `zone`
- step `phase`
- step `participant`
- step `slot`
- step `from_zone`
- step `from_slot`
- step `to_zone`
- step `to_slot`
- step `ok`
- step `failure`

Trace strings are runtime-borrowed strings. Callers must not free them. The
runtime copies each returned value into a thread-local borrowed snapshot, so the
pointer is independent of later registry mutation but is only stable until a
later borrowed string query on the same thread reuses that snapshot slot.

## Stable Authority Failure Schema

Stable authority snapshot fields:

- `ok`
- `zone`
- `participant`
- `code`
- `reason`

Stable authority codes:

- `ok`
- `missing-zone`
- `missing-participant`
- `authority-token-mismatch`

Authority snapshot strings are runtime-borrowed strings. Callers must not free
them, and the value is valid until the next authority validation updates the
snapshot.

## Runtime Registry Rules

- Active registry order is observable only through the tested active query
  surface; beta does not promise a richer scheduler timeline.
- Recent registry order is newest-first for `IntentRecent*`.
- History step order is execution/materialization order for the last completed
  intent.
- C and LLVM backends must produce the same values for the stable schema.

## Explicitly Out Of Beta

- General event streaming schema.
- Multi-instance timeline query language beyond `IntentRecent*`.
- Structured JSON trace export.
- Distributed tracing correlation protocol.
- Runtime registry mutation hooks for user code.
- Stable binary trace format.

## Evidence

`make observability-schema-test-smoke` runs the stable intent and authority
schema fixtures on C and LLVM:

- `intent_trace_abi`
- `intent_recent_abi`
- `intent_active_abi`
- `intent_failure_abi`
- `authority_failure_abi`
