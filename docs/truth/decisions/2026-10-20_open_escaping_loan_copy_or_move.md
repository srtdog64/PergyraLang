# OPEN 2026-10-20: a getter's loan that cannot be proved, copy or move

Asked by: the Claude lane. Waiting for the user.

## The case

A getter that only reads its by-value aggregate argument and returns a
part of it lends that argument
([decision](2026-10-19_by_value_getter_lends_when_used_again.md)). The
caller may use the argument again only when the loan proof holds: the
loan stays in the routine, and nothing changes the owner while it lives.
When the proof fails, the call moves the argument and every later use is
refused.

At `2d30ec766` that is 1188 refusals of getters on formals and 125 on
locals, from about 330 lending calls. The census
([record](../evidence/2026-10-20_native_census_aliases_none_views.md))
shows what the loans do:

- a String element of a local Array returned through `AppendLine`, which
  can return its argument;
- a String of a by-value fact pushed into the caller's `inout` output;
- a fact struct built from an artifact and returned in a larger result.

Under the planned automatic cleanup each of these would outlive, or be
changed under, the storage it borrows. The canonical program must stay
unchanged, so the compiler has to resolve them.

## Options

| | Copy the result (recommended) | Copy at the store | Keep the move |
|---|---|---|---|
| What happens | where the loan proof fails, the call's result becomes an independent `Clone` of what the getter returned | the String or value is copied only where it leaves: the push, the constructor field, the return | the argument moves and a later use is refused |
| Where it is decided | after the flow proof, at the call; MIR shows the `Clone` | at type check, before the proof that would show the loan | (unchanged) |
| Copies | one deep copy of the result per execution of those ~330 calls | only what leaves; fewer | none |
| Refusals removed | the 1313 above | the same and some of the 673 "callee keeps it" moves | none; V1 stays blocked |
| Work | a landing fact read by MIR lowering; `Clone` already runs on C and LLVM for String, Option and deep struct | every type-check-time owner of a push or constructor field learns the landing | none |

Copying the result keeps the existing semantic facts true: the result
was already treated as possibly dependent, and a copy only makes it
independent. A copy at the store would change what the receiver Array
owns, which the collection owners decide at type check.

The emitted code frees no String yet, so each copy also stays allocated
until the process ends. A later move landing or the cleanup model can
take a copy back where a measurement shows it matters.
