# 2026-10-20: a value of Strings and copies holds no container storage

Decided by: the Claude lane. It refines the
[call-result rule](2026-10-20_call_result_holding_formal_storage_is_its_path.md).
The user can overturn it.

## Rule

A value holds no Array or other container storage when it is a String, a
copy-only value, an Option of one, or a non-generic struct whose fields
all are (`semantic_ownership_type_holds_no_container`). Two proofs use the
same answer:

- the storage proof of a formal treats reading such a value through a path
  or a call result as a read, as it already did for a String;
- the loan proof does not check a local the loan reached when the local
  is such a value.

## Why

- Copying such a value shares only immutable Strings, and String lifetimes
  are the String owners' concern. Nothing reached through it can push,
  set or release storage a caller's descriptor names.
- On the canonical driver input the call-result rule refused every
  reassigned getter result of type `Option<String>` or of a struct of
  Strings and Ints (`SemanticAstIntentVariantSeal`,
  `SemanticAstIntentActionCallFact`), and every loan of a
  `TypedAstArena*Text` getter whose `Option<String>` was unwrapped while
  the arena was used again.

Replaces: nothing.
