"""Function-scoped general C/LLVM String-array projection contract."""

import pathlib
import re
import sys


def emitted_function_body(program, signature):
    match = re.search(signature + r"\s*\{", program)
    if match is None:
        raise RuntimeError(f"emitted function is absent: {signature}")
    depth = 1
    for position in range(match.end(), len(program)):
        if program[position] == "{":
            depth += 1
        elif program[position] == "}":
            depth -= 1
            if depth == 0:
                return program[match.end():position]
    raise RuntimeError(f"emitted function is unterminated: {signature}")


def emitted_requirement(condition, message):
    if not condition:
        raise RuntimeError(message)


def llvm_bounds_guard(body, label):
    # An unsigned comparison is also a complete lower/upper bounds guard.
    unsigned = re.search(
        r"%(\S+) = icmp ult i64 %index, %length\s+"
        r"br i1 %\1, label %(\S+), label %(\S+)", body
    )
    signed = re.search(
        r"%(\S+) = icmp slt i64 %index, 0\s+"
        r"%(\S+) = icmp sge i64 %index, %length\s+"
        r"%(\S+) = or i1 %\1, %\2\s+"
        r"br i1 %\3, label %(\S+), label %(\S+)", body
    )
    emitted_requirement(unsigned is not None or signed is not None, f"{label}: bounds guard is absent")
    guard = unsigned if unsigned is not None else signed
    read_label, panic_label = (guard[2], guard[3]) if unsigned is not None else (guard[5], guard[4])
    emitted_requirement(
        re.search(re.escape(panic_label) + r":\s+call void @pgy_runtime_panic_out_of_bounds_export\([^\n]*\)\s+unreachable", body),
        f"{label}: invalid bounds do not enter the owned panic boundary",
    )
    emitted_requirement(re.search(re.escape(read_label) + r":\s", body), f"{label}: guarded access block is absent")
    emitted_requirement("load ptr" not in body[:guard.end()], f"{label}: pointer read precedes bounds admission")


def check_emitted(work):
    c = (work / "base.c").read_text(encoding="utf-8")
    llvm = (work / "base.ll").read_text(encoding="utf-8")
    c_main = emitted_function_body(c, r"int main\(void\)")
    llvm_main = emitted_function_body(llvm, r"define i32 @main\(\)")
    emitted_requirement("pgy_r0_block_" in c_main and "pgy.r0.block." in llvm_main, "baseline did not select the general route")
    emitted_requirement("pgy_as_len(" in c_main and "call i64 @pgy_as_len(" in llvm_main, "general ArrayLength call is absent")
    emitted_requirement('= "BOB";' in c_main and 'c"BOB\\00"' in llvm, "general setter lost the admitted String value")
    for body, read, setter, label in (
        (c_main, "pgy_as_get(", "pgy_as_set(", "C"),
        (llvm_main, "call ptr @pgy_as_get(", "call void @pgy_as_set(", "LLVM"),
    ):
        reads = [match.start() for match in re.finditer(re.escape(read), body)]
        emitted_requirement(len(reads) == 2 and body.count(setter) == 1, f"{label}: general read/set inventory drifted")
        emitted_requirement(reads[0] < body.index(setter) < reads[1], f"{label}: admitted set/read order drifted")
    c_get = emitted_function_body(c, r"static const char\s*\*\s*pgy_as_get\(pgy_as a, long long i\)")
    c_set = emitted_function_body(c, r"static void pgy_as_set\(pgy_as \*a, long long i, const char\s*\*\s*v\)")
    for body, length, access, label in (
        (c_get, r"a\.length", "return a.data[", "C read"),
        (c_set, r"a->length", "a->data[", "C set"),
    ):
        guard = re.search(r"if\s*\(i < 0 \|\| i >= \(long long\)" + length + r"\)\s*\{\s*PGY_RUNTIME_PANIC\(", body)
        emitted_requirement(guard is not None, f"{label}: length bounds guard is absent")
        emitted_requirement("PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS" in body and "PGY_RUNTIME_PANIC_REASON_ARRAY_INDEX_OUT_OF_BOUNDS" in body, f"{label}: owned bounds panic identity is absent")
        emitted_requirement(access in body and guard.end() < body.index(access), f"{label}: access precedes its guard")
    llvm_get = emitted_function_body(llvm, r"define internal ptr @pgy_as_get\(%pgy\.array\.string %array, i64 %index\)")
    llvm_set = emitted_function_body(llvm, r"define internal void @pgy_as_set\(ptr %array, i64 %index, ptr %value\)")
    llvm_bounds_guard(llvm_get, "LLVM read")
    llvm_bounds_guard(llvm_set, "LLVM set")
    emitted_requirement("%length = extractvalue %pgy.array.string %array, 1" in llvm_get, "LLVM read guard does not consume the length field")
    for suffix, signature, read, setter in (
        ("c", r"int main\(void\)", "pgy_as_get(", "pgy_as_set("),
        ("ll", r"define i32 @main\(\)", "call ptr @pgy_as_get(", "call void @pgy_as_set("),
    ):
        reordered = emitted_function_body((work / f"set-log-reordered.{suffix}").read_text(encoding="utf-8"), signature)
        reads = [match.start() for match in re.finditer(re.escape(read), reordered)]
        emitted_requirement(len(reads) == 2 and reads[1] < reordered.index(setter), f"{suffix}: reordered read is not before set")
    print("general C/LLVM route, admitted read/set order and function-scoped runtime bounds guards are valid")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: one_mir_string_array_emitted_contract.py VERIFIED_WORK_DIR")
    check_emitted(pathlib.Path(sys.argv[1]))
