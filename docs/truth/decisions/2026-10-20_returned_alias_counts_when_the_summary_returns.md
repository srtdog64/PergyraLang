# 2026-10-20: a returned alias counts when the formal's summary returns it

Decided by: the Claude lane. It replaces one clause of the
[call-result rule](2026-10-20_call_result_holding_formal_storage_is_its_path.md):
"a local that takes such a result is that result". The user can overturn
it.

## Rule

1. A local that takes a call result holding the formal is an alias, as a
   local that takes a path is.
2. A returned value that holds an alias of the formal, the alias itself
   or a constructor of it, is a getter's return when the formal's summary
   so far has the RETURN bit. The bit is kept per formal, so callers
   already treat the whole result as a loan of the argument. Without the
   bit the return stays refused: the summary does not follow an alias.
3. A bare formal or alias that takes another value holding the formal, as
   in `build = AddInstruction(build, row)`, stays what it was, and later
   uses are proved as before. A local that is neither still refuses such
   a value: the walk visits a loop body once, so a use before the
   assignment would not see it.

## Why

- Rule 2 covers what the replaced clause covered: a getter call already
  gives the formal's summary its RETURN bit. With rule 2, the clause was
  no longer falsified by any gate input, so it was removed.
- On the canonical driver input, the most common place a formal's storage
  proof failed was `x = F(x, ...)` on a by-value formal (52 routines), and
  the second was returning a constructor of an alias (50 routines).

Replaces: the clause named above.
