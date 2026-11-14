"""Subject identity/lifetime falsifiers for the native operator runner."""


def fixtures():
    box = "subject Box { var value: Int; }\n"
    add = """
func operator_add_Box(a: Box, b: Int) -> Int {
    a.value = a.value + b;
    return a.value;
}
"""
    for label, main, want in (
        ("temporary", "Log(Box(40) + 2);", "42\n"),
        ("direct_temporary", "Log(operator_add_Box(Box(40), 2));", "42\n"),
        ("local_identity", "let a = Box(40); Log(a + 2); Log(a.value);", "42\n42\n"),
        ("direct_local", "let a = Box(40); Log(operator_add_Box(a, 2)); Log(a.value);", "42\n42\n"),
    ):
        yield "subject_" + label, box + add + (
            "func Main() -> Void {" + main + "}"
        ), want

    yield "subject_forwarded", box + add + """
func Forward(a: Box) -> Int { return a + 3; }
func Main() -> Void { let a = Box(40); Log(Forward(a)); Log(a.value); }
""", "43\n43\n"

    yield "subject_self", """
subject Box {
    var value: Int;
    func Plus(self) -> Int { return self + 2; }
}
""" + add + """
func Main() -> Void { let a = Box(40); Log(a.Plus()); Log(a.value); }
""", "42\n42\n"

    yield "subject_member", box + add + """
class Holder { let box: Box; }
func Main() -> Void {
    let a = Holder(Box(40)); Log(a.box + 2); Log(a.box.value);
}
""", "42\n42\n"

    yield "subject_both", box + """
func operator_add_Box(a: Box, b: Box) -> Int {
    a.value = a.value + b.value; b.value = 7; return a.value;
}
func Main() -> Void {
    Log(Box(20) + Box(22));
    let a = Box(20); let b = Box(22);
    Log(a + b); Log(a.value); Log(b.value);
}
""", "42\n42\n42\n7\n"

    yield "subject_rhs", box + """
func operator_add_Int(a: Int, b: Box) -> Int { b.value = 42; return b.value; }
func Main() -> Void {
    Log(2 + Box(40)); let b = Box(40); Log(2 + b); Log(b.value);
}
""", "42\n42\n42\n"

    yield "subject_evaluation_order", box + add + """
func Left() -> Int { Log(1); return 40; }
func Right() -> Int { Log(2); return 2; }
func Main() -> Void { Log(Box(Left()) + Right()); }
""", "1\n2\n42\n"

    yield "subject_nested_result", box + add + """
func Main() -> Void { Log((Box(40) + 2) + 3); }
""", "45\n"

    yield "subject_rhs_evaluation_order", box + """
func operator_add_Int(a: Int, b: Box) -> Int { return b.value; }
func Left() -> Int { Log(1); return 2; }
func Right() -> Int { Log(2); return 42; }
func Main() -> Void { Log(Left() + Box(Right())); }
""", "1\n2\n42\n"

    yield "subject_readonly_temporary", box + """
func operator_add_Box(ref a: Box, b: Int) -> Int { return a.value + b; }
func Main() -> Void { Log(Box(40) + 2); }
""", "42\n"

    yield "subject_inout_forward", box + add + """
func Forward(inout a: Box) -> Int { return a + 2; }
func Main() -> Void { let a = Box(40); Log(Forward(a)); Log(a.value); }
""", "42\n42\n"

    inout_add = """
func operator_add_Box(inout a: Box, b: Int) -> Int {
    a.value = a.value + b; return a.value;
}
"""
    yield "subject_inout_local", box + inout_add + """
func Main() -> Void { let a = Box(40); Log(a + 2); Log(a.value); }
""", "42\n42\n"
    yield "reject_subject_inout_temporary", box + inout_add + """
func Main() -> Void { Log(Box(40) + 2); }
""", {
        "c": {"code": "PGY_C_TYPE_UNSUPPORTED"},
        "llvm": {"code": "PGY_LLVM_TYPE_UNSUPPORTED"},
    }

    yield "subject_free_precedes_role", """
subject Box {}
ability Arithmetic { func Add(self, rhs: Int) -> Int; }
role BoxMath for Box {
    impl Arithmetic { func Add(self, rhs: Int) -> Int { return 321; } }
}
func operator_add_Box(a: Box, b: Int) -> Int { return 789; }
func Main() -> Void { Log(Box() + 2); }
""", "789\n"

    yield "reject_subject_wrong_rhs", box + add + """
func Main() -> Void { Log(Box(40) + true); }
""", None
    yield "reject_subject_unrelated_type", box + add + """
subject Other { var value: Int; }
func Main() -> Void { Log(Other(40) + 2); }
""", None
    yield "reject_subject_escape", box + add + """
func Factory() -> Box { return Box(40); }
func Main() -> Void { Log(Factory() + 2); }
""", {"stage": "semantic", "code": "PGY_SEM_ANCHORED_HANDLE_COPY"}
