# 2026-10-22: a copy changed while a loan of its source lives copies the loan where it is made

Decided by: the Claude lane. It closes a use-after-free the checker
admitted; it extends
[the local copy rule](2026-10-21_local_copies_refuse_the_other_side_after_a_change.md).
The user can overturn it.

## Rule

A local copy of a path (`var grown = graph.rows`) shares the storage the
path holds. When a change through the copy (a push, a set, an element
store, an `inout` hand-off) reaches that storage while a getter's loan of
the source (`facts = WithGraph(graph)`) is still recorded, the loan's
owner has changed: the change is refused at once, and the next analysis
copies the loan where it is made (`Clone(WithGraph(graph))`), as for any
loan whose owner changes, when `Clone` copies it whole. Otherwise the
refusal stays.

Before, the copy rule refused only the source's next use. A loan holds the
same storage under another binding, so a program that only read the loan
afterwards was admitted.

## Why

This program was admitted at `3cdb56256` and already at the older
branch base `4f7692f38` (where no landing ran at all), and C with ASan
reports heap-use-after-free on `facts.graph.rows[0].name` (C and LLVM
builds dump core):

```pergyra
func WithGraph(graph: Graph) -> Facts { return Facts(graph, true); }
let facts: Facts = WithGraph(graph);
var grown: Array<Row> = graph.rows;
ArrayPush(grown, Row("b", ids)); // four pushes reallocate the rows
Print(facts.graph.rows[0].name);
```

Fixtures: `returned_alias_lends_formal_grown_invalid` (refused, Array of
structs), `lent_path_copy_grown_landed_valid` (copy lands, prints `75`).

Replaces: nothing.
