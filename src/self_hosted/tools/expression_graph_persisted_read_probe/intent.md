# Expression Graph Persisted Read Probe - Intent / Contract

**Status:** focused executable fail-closed proof.

## Intent

Prove that the one-pass persisted expression-graph reader
(`MirExpressionGraphSequenceAppend` over a `JsonObjectFactTable`) admits
exactly the canonical `pgy.mir.v1` node shape and rejects malformed
documents instead of repairing them.

Positive lanes: the canonical three-node scalar graph and the same graph
with reordered object fields both append into a ready sequence whose root
resolves to the `add` node.

Negative lanes, each expected to fail closed with a rejected sequence:

- `--duplicate-node-field`: a node object repeats a field; the one-pass
  reader must not silently take either occurrence.
- `--unknown-header-field`: an unrecognized top-level key must not be
  skipped as ignorable padding.
- `--unreachable-node`: every persisted node must be reachable from the
  declared root; an orphan is a broken graph, not dead data.
- `--overflow-root`: a root id outside the 32-bit node range must be
  rejected before any indexing.

## Input Contract

One optional argv mode selects the document: no argument or
`--reordered-fields` for the positive lanes, or one of
`--duplicate-node-field`, `--unknown-header-field`, `--unreachable-node`,
`--overflow-root` for the negative lanes. Every document is embedded in
the probe; nothing is read from the filesystem.

## Output Contract

Positive lanes log the resolved root kind and exit 0. Negative lanes exit
nonzero after logging which malformation the reader rejected; a negative
lane that reaches a ready sequence logs
`malformed persisted graph was accepted` and exits nonzero so a fail-open
reader can never look green.

## Oracle

The sequence owner the MIR consumer itself uses
(`mir_lower/expression_graph_sequence_owner.pgy`) is the oracle: the probe
feeds documents to that exact reader, so its verdicts are the consumer's
verdicts. Drift between this probe and the consumer is a probe bug, never
a second reader.
