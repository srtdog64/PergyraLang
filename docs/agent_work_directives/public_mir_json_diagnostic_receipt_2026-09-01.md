# Public MIR JSON diagnostic receipt

Status: LOCAL IMPLEMENTATION COMPLETE; PUBLICATION AND EXACT CI PENDING
Base revision: `04a981b1afd5a625d8e0a6b411919165d34d7439`

This directive coordinates one bounded production substitution. It is not a
semantic owner, registry owner, progress claim, or successor queue.

## Shared objective card

- Objective: make default `pgy --mir --error-format=json SOURCE` consume a
  Pergyra-owned semantic diagnostic receipt for the reached
  `undefined_function` failure instead of rejecting the request in the C
  launcher or recovering code/stage from rendered message text.
- Priority order: preserve the Pergyra semantic diagnostic identity; carry one
  typed receipt; remove the public C rejection for this exact request; reject
  malformed or missing child payloads; keep explicit native MIR as an oracle.
- Fact owner:
  `src/self_hosted/semantic/public_diagnostic_receipt_owner.pgy`, consuming the
  separately carried `SemanticAstArtifactVerdict.diagnostic_code` and the
  append-only oracle-code mapping in
  `src/self_hosted/semantic/diagnostic_code_owner.pgy`.
- Last legitimate consumer: the installed Pergyra driver request reached from
  `src/compiler/self_host_mir_diagnostic_stdout_owner.c`; the C file may relay
  the admitted bytes but may not infer semantic identity from message text.
- Forbidden fallback: `driver_diag_code_from_message`, substring-based stage or
  code recovery, native-pipeline retry, a C copy of the Pergyra diagnostic
  mapping, or accepting a nonzero child with an empty/malformed JSON payload.
- Verification gate and falsifying case:
  `tests/self_hosted/parity/public_mir_json_diagnostic_receipt_owner.sh` must
  observe exact `stage=semantic`, `code=PGY_SEM_UNDEFINED_SYMBOL`,
  `cause_ir=semantic:symbol:undefined`, and
  `fix_source=import-or-declare-symbol` from the default installed path. A
  Pergyra-only message-wording mutation must preserve those fields, while a
  missing, cross-wired, or malformed receipt must fail without native timing or
  a partial JSON diagnostic.

## Edit scopes and forbidden overlap

- Pergyra receipt and projection:
  `src/self_hosted/semantic/public_diagnostic_receipt_owner.pgy`, the source-MIR request and
  execution owners, and their exact owner inventory.
- C transport adapter:
  `src/compiler/self_host_mir_diagnostic_stdout_owner.c` selects the installed
  request, while `src/compiler/self_host_public_diagnostic_wire_owner.c`
  validates and relays only the opaque wire envelope. Neither owns a diagnostic
  taxonomy.
- Gate and coordination evidence: one focused parity script, Make target,
  registry/open-reason wording, progress/handoff after the executable gate is
  green.
- Do not edit native semantic diagnostics, `driver_diag.c` message mapping for
  unrelated native stages, AIR/RIR/HIR modes, compatibility manifest parsing,
  or another self-host surface in this rung.

## Validation budget

- Static owner/ratchet checks: 60 seconds.
- Focused installed parity and message-mutation negative: 5 minutes.
- Fresh installed-driver build only after source/static checks are green.
- Full matrices remain an exact-head CI boundary after commit and push.

## Integration owner and gate

The primary task owns integration. The integration gate is
`self-host-public-mir-json-diagnostic-receipt-test-smoke`; its observations are
executable evidence, not an independent semantics or progress authority.
