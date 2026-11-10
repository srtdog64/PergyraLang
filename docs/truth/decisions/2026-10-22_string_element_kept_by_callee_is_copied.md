# 2026-10-22: a String element a callee may keep is copied where it is passed

Decided by: the Claude lane. It applies
[a String argument no proof admits is copied](2026-10-21_string_argument_no_proof_admits_is_copied.md)
to an `Array<String>` element handed to a declared callee. The user can
overturn it.

## Rule

A call that takes an element of an `Array<String>` binding records it as
the binding's pending element handoff. When the call is refused because
no proof admits the callee keeping or returning that element (the loan
outlives the Array, or the Array is released or changed while it lives),
the next analysis passes a copy instead: `F(..., Clone(rows[i]), ...)`.
The copy is the callee's to keep, and the Array's later release or
change no longer reaches it.

A call that keeps the whole Array is distinct from the element rule. When
`Clone` owns the complete Array value, the compiler may store that whole
Clone at the keeping boundary and the caller may continue using its
original (`array_descriptor_kept_then_read_landed_valid`). An element copy
does not stand in for the Array, and a shallow or unsupported whole-value
Clone remains refused
(`array_descriptor_uncopyable_kept_then_read_invalid`).

## Why

`AppendLine(block, param_lines[pi])` in the parser returns its line when
the accumulator is empty, so the accumulated block may be an element of
the local `param_lines`, and the block leaves the routine. The refusal
was right, but no copy could land for it, unlike the same String passed
as a plain argument. Ten negative fixtures that dropped or changed the
Array after such a call are now admitted with the copy. The eight that
link give the same output on C and LLVM, and C with ASan is clean; the
other two call an `extern "C"` function the fixture does not define. They
are `*_landed_valid` now, each with its Clone count.

Replaces: nothing.
