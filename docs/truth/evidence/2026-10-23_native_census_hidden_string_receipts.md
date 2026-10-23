# 2026-10-23: String receipt refusals the final owners hide on the canonical input

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`715950481` (local, not pushed). Measurement only; no rule changed.

The String receipts pass (`indexed_string_borrow_prepare` and
`..._finalize`, run at the end of `type_check_program`) runs only when
every final owner admits the program and no error stands. On the
canonical driver input the final owners refuse
(`collection_owned_element_requirements_finalize` among them), so the
census, which runs every final owner, never reached the pass, and the
pass also stops at its first refusal.

A scratch census build ran the pass after the final owners whatever they
returned, cleared the error flag before it, and let every receipt family
and loop continue past a refusal:

| Refusal | Sites |
|---|---|
| A shallow String store has no current independent-result proof | 900 |
| An excluded String payload owner has no current non-owning history proof | 2 |
| A shallow String store receipt was withdrawn | 1 |
| A String definition has no current independent source proof | 1 |
| A String field has no current independent-source recipe | 1 |

The other 258 refusals are unchanged: 1166 in all, against 258 counted.
The pass runs outside any routine, so the census line names a site's
line, column and the call whose result is stored (`JsonEmitObject`,
`SemanticError1`, `SelfMirSsaName`, `SelfMirJsonAbiLayoutField`, ...),
not its module.

A failed shallow String store selects no copy, unlike a refused private
push, which selects one. In a small program the pass admits a stored call
result once the callee's borrowed return has been copied (`Row(Pick(names, 1))`
with `Pick` returning `names[i]`: one Clone in `Pick`, then no receipt
refusal), so the pass itself works; whether a site on the canonical input
fails because its callee is refused elsewhere or because its result really
lends an argument is not measured.

Tool: `scratchpad/instr_patch_receipts.py` with `PGY_CENSUS_RECEIPTS=1`.
