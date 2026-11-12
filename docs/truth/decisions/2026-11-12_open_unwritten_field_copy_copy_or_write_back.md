# OPEN 2026-11-12: a grown field copy that is never assigned back: copy or write back?

Asked by: the Claude lane. Not decided; neither rule below is changed.

## The case

```
func Update(inout tables: Tables, count: Int) -> Void {
    var i: Int = 0;
    while i < count {
        var rows: HashMap<String, Int> = tables.rows;
        if !MapHas(rows, ToString(i)) { MapSet(rows, ToString(i), i); }
        i = i + 1;
    }
}
// Main: Update(tables, 100); Log(MapSize(tables.rows));
```

The two front ends admit this program and print different results:

- Native prints `0`. It applies
  [the 2026-10-22 rule](2026-10-22_inout_place_changed_through_a_copy.md):
  `rows` is copied (`Clone`) where it is taken, so `tables.rows` keeps its
  old value.
- The self-host prints `100`. Its focus recovery (GPT lane, landed with
  `8a56b31e4`) treats `rows` as a focus of `tables.rows` and writes it back
  at each exit of its scope.

The same split covers the focus recovery inputs in
`tests/self_hosted/parity/fixture/collection_field_lifetime/focus_recovery_*`.
For `focus_recovery_bare_fallthrough` the self-host prints `9 17 row`; native
prints `1 1` and then stops on an index out of range. V1 requires
runtime-equal output for every program native admits, so one rule has to go.

## The choice

| | Copy (native today) | Write back (self-host today) | Refuse |
|---|---|---|---|
| `var x = root.field`, grown, not assigned back | `x` is a copy; `root.field` keeps its value | the compiler assigns `x` back at each exit | refused until the code assigns it back or calls `Clone` |
| Matches | Swift-like `var` (2026-11-08), every copy-where-the-proof-fails rule | none of the recorded rules | the 2026-10-25 explicit `Clone` direction |
| Work | self-host stops writing back and copies (or refuses) there; the focus gate's expected outputs change | native replaces the 10-22 copy with a write-back | both front ends refuse; positives that rely on either rule are rewritten |
| Cost at run time | one whole copy per such site | none | none |

The Claude lane recommends the copy: it is what native already does, and it
is the value semantics the `let`/`var` decision chose. A program that wants
the change in `root.field` says so with `root.field = x`, which neither
front end copies.
