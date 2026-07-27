# Callable Contract Vocabulary

Updated: 2026-07-27 (Asia/Seoul)

`src/semantic/callable_contract_vocabulary.def` is the semantic source of truth
for the closed values accepted by `with caps` and `with effects`.

## Objective card

- Objective: carry one capability/effect vocabulary from source clauses to
  diagnostics, MIR, self-host validation, runtime grants, and tooling-facing
  projections without a consumer-local spelling table.
- Priority: stable identity and membership; canonical wire order and `local`
  exclusivity; mask/manifest projection; negative ratchet; compact generated
  consumers.
- Fact owner: `src/semantic/callable_contract_vocabulary.def`.
- Last legitimate consumers: native/self-host MIR validation and the runtime
  capability grant parser.
- Forbidden fallback: local spelling arrays, chained word predicates, literal
  known-mask unions, duplicate folding, `local` mixing, or projection drift.
- Verification and falsifier: `tests/callable_contract_vocabulary_smoke.sh`
  plus the DRV-2 action wire mutations; duplicate words, reversed canonical
  order, unknown values, and both `local` mix orders must fail closed.

## Boundary decision

`with`, `caps`, and `effects` select grammar productions and therefore belong
to the language-word registry. The values inside those productions do not all
become lexer keywords. They belong to this semantic registry:

```text
with caps    -> io_read io_write network clock random env render audio input
with effects -> secure remote nondeterministic collapse unsafe io alloc
                authority local
```

This split prevents a semantic ABI from becoming a lexer table. The lexer may
continue to return an identifier for contextual contract values. The parser
selects the clause, then asks this owner whether the value is a member.

## Owned facts

Each of the 18 rows owns:

- a stable `PgyCallableContractWordId`;
- capability/effect axis;
- source and MIR wire spelling;
- the existing `PGY_CAP_*` or `EFFECT_*` mask symbol;
- canonical serialization rank;
- zero policy;
- capability manifest spelling.

The numeric masks remain owned by `pgy_runtime_capability.h` and
`type_system.h`. Registry order never computes a bit. `PGY_CAP_ALL` is not the
known capability mask: the registered mask is `0x1ff`, while `all` and `none`
are runtime-only `PGY_CAP_GRANT` aliases and are invalid in source `with caps`.

`local` is not a bit. It is the single explicit zero-valued effect and is
exclusive. Both `local, secure` and `secure, local` fail closed. Duplicate
words and noncanonical MIR arrays also fail closed rather than being silently
folded by mask OR.

## Projection graph

```text
callable_contract_vocabulary.def
  -> callable_contract_vocabulary.c/.h        native parser/semantic/MIR
  -> generated self-host Pergyra projection  parser/semantic/MIR verifier
  -> generated runtime capability projection PGY_CAP_GRANT
```

`scripts/render_callable_contract_vocabulary.py --check` rejects stale checked-
in projections. Consumer-local arrays, spelling chains, known-mask OR chains,
or a second canonical order are forbidden fallbacks.

## Verification

`tests/callable_contract_vocabulary_smoke.sh` checks the 9+9 schema, stable IDs,
ranks, masks, `local`, runtime-only aliases, projection freshness, native C
lookup, source positives/negatives, and absence of the retired local tables.
The DRV-2 ActionContract parity gate additionally mutates MIR arrays for
duplicate, unknown, noncanonical, and mixed-local cases before C/LLVM output.

This closes ActionContract declaration carriage. It does not by itself make a
Pergyra action `SUBSTITUTING`; only deletion of a real C-owned production path
with execution/parity/negative evidence can do that.
