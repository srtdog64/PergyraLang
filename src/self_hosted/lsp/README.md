# LSP Substitution Track

The default public `pgy-lsp` process now hands a no-argument invocation to the
installed Pergyra-built `pgy-self-lsp`. The old C live loop remains reachable
only through explicit `pgy-lsp --native-pipeline`; it is an oracle, never a
retry or fallback. `pgy-lsp --dump-diagnostics SOURCE` uses the same installed
sibling, with the C diagnostic path likewise explicit-native only. A missing
sibling fails closed at the public boundary.

`main.pgy` routes no arguments to `live_session_owner.pgy`. That owner performs
repeated byte-stream reads, retains partial `Content-Length` frames, owns the
initialize/shutdown/exit lifecycle, admits `didOpen` and strictly advancing
`didChange` revisions, and emits exact JSON-RPC frames without newline
translation. Malformed framing, incomplete EOF, stale/same-version changes,
and buffer overflow are fatal at this boundary. Buffered request/session/store
owners remain parity artifacts and are not execution fallbacks.

The live state deliberately owns one current document, matching the native
production loop being replaced. `document_revision_owner.pgy` owns its URI,
version, and exact text. `document_feature_index_owner.pgy` builds one typed
revision-scoped declaration index on open/change; document symbol, definition,
references, and rename consume that index without re-reading the program root.
Hover consumes the admitted revision, completion remains registry-directed,
and diagnostics are projected from the semantic diagnostic owner. The tooling
index is not the compiler semantic-artifact owner and supplies no semantic
proof; that distinction is why this rung does not claim LSP-3 completion.

The focused executable gate is
`tests/self_hosted/parity/lsp_live_session_owner.sh`. It keeps one process
alive across initialize, open, hover, a fragmented change, updated hover,
shutdown, and exit. It also rejects stale/same-version changes, incomplete EOF,
and a public launcher with no sibling. `tests/tooling_conformance_smoke.sh`
guards the wider editor-facing contract.

This rung does not claim multi-document live ownership, incremental semantic
analysis, or whole-compiler LSP completion. `document_store_owner.pgy` still
owns buffered multi-document projection fixtures; `request_owner.pgy`,
`response_owner.pgy`, `session_owner.pgy`, and `session_state_owner.pgy` remain
bounded replay/probe owners. `fixture/` and `expected/` are committed parity
contracts, not live semantic authority.
