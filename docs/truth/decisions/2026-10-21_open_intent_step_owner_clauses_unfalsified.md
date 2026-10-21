# OPEN 2026-10-21: the rows check's intent-step owner clauses lost their falsifier

Asked by: the Claude lane. Not decided; both clauses stay as they are.

## The case

`function_param_flow_backing_rows_capture.c` lets the row that holds an
intent call own two kinds of a frame's relation: an unsupported relation
whose call is the step's action call, and a pending one. `ec8ba8ed6`
falsified the unsupported clause with
`intent_step_unsupported_route_valid`, whose step action sorted a copy:
that sort was a call the analysis did not model. Since
[ArraySort is modelled](2026-10-21_array_sort_permutes_in_place.md) no
fixture reaches either clause. The fixture is kept as
`intent_step_sorted_copy_route_valid`.

Other unmodelled calls were tried in the step action: a call through a
function value (named or a lambda) and a class constructor. The first
fails Route closed for another reason ("required return backing relation
is incomplete") with and without the clause; the second is modelled. The
pending clause was never reached (five shapes tried on 2026-10-20).

## The choice

| | Keep both clauses | Delete both clauses |
|---|---|---|
| Behavior | an intent step's unsupported or pending relation keeps its caller's rows current | such a relation makes the caller's rows stale, and the store fails closed |
| Safety | unchanged (the clause only admits a conservative summary) | stricter |
| Test rule | two conditions no input falsifies | none |

The Claude lane recommends deleting both: they guard a case no current
program reaches, and the deletion only refuses more.
