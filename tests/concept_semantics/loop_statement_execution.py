"""Typed loop/while controls; only fixed valid sources may execute.

--shared-only omits public production execution during a GraphPlan owner edit.
The existing receiver probe also exposes the shared scalar GraphPlan boundary.
"""
import copy
import hashlib
import json
import os
import pathlib
import subprocess
import sys
import tempfile
from identity_cell_receiver_execution import graph_digest


def main():
    root = pathlib.Path(__file__).resolve().parents[2]
    native, driver = (pathlib.Path(x).resolve() for x in sys.argv[1:3])
    shared_only = "--shared-only" in sys.argv[3:]
    parent = root / ".tmp/concept_semantics/loop_statement"
    parent.mkdir(parents=True, exist_ok=True)
    work = pathlib.Path(tempfile.mkdtemp(prefix="run.", dir=parent))
    original = root / "tests/concept_semantics/word_deletion/cases/27_loop_while/orig.pgy"
    original_text = original.read_text(encoding="utf-8")
    cases = [
        ("original", original_text, "3\n", "", ""),
        ("while-equivalent", original_text.replace("loop {", "while true {"), "3\n", "", ""),
        ("continue", "func Main() -> Void { let n: Int = 0; loop { n = n + 1; if n < 4 { continue; } break; } Log(n); }", "4\n", "", ""),
        ("nested", "func Main() -> Void { let n: Int = 0; let total: Int = 0; loop { let k: Int = 0; loop { total = total + 1; k = k + 1; if k == 2 { break; } } n = n + 1; if n == 2 { break; } } Log(total); }", "4\n", "", ""),
        ("return", "func Main() -> Void { let n: Int = 0; loop { n = n + 1; if n == 3 { Log(n); return; } } }", "3\n", "", ""),
        ("false-while", 'func Main() -> Void { while false { Log("unreachable"); } Log(0); }', "0\n", "", ""),
        ("empty-void", "func Main() -> Void { }", "", "", ""),
        ("string-branch", (root / "src/self_hosted/codegen/fixture/string_equality_concat.pgy").read_text(encoding="utf-8"), "concat_eq_ok\n", "", ""),
        ("non-bool", "func Main() -> Void { while 3 { break; } }", None, "PGY_SEM_TYPE_MISMATCH", "condition_not_bool"),
        ("wrong-body", 'func Main() -> Void { loop { let n: Int = "wrong"; break; } }', None, "PGY_SEM_TYPE_MISMATCH", "let_type_mismatch"),
        ("unretired-task", "async func Work() -> Int { return 1; } async func Main() -> Void { loop { let t: Future<Int> = spawn Work(); break; } }", None, "PGY_SEM_TASK_LIFECYCLE", "task_lifecycle_invalid"),
    ]
    environment = dict(os.environ, PGY_SELF_DRIVER_BIN=str(driver), PGY_DEBUG_PIPELINE_TIMING="1")
    environment.pop("PGY_NATIVE_PIPELINE", None)
    rows = []
    omitted_mutations = []

    def run(stem, command, timeout=45):
        result = subprocess.run([str(x) for x in command], cwd=root, env=environment,
                                capture_output=True, timeout=timeout)
        (work / (stem + ".out")).write_bytes(result.stdout)
        (work / (stem + ".err")).write_bytes(result.stderr)
        return result

    def check(stem, passed):
        rows.append({"case": stem, "passed": passed})
        print(f"[loop-statement] {'PASS' if passed else 'FAIL'}: {stem}", flush=True)

    def relative(path):
        return path.relative_to(root).as_posix()

    probe_source = root / "tests/self_hosted/parity/fixture/identity_cell_receiver_probe.pgy"
    prebuilt = [arg.removeprefix("--probe=") for arg in sys.argv[3:] if arg.startswith("--probe=")]
    if len(prebuilt) > 1:
        raise ValueError("one shared admission probe may be reused")
    probe = pathlib.Path(prebuilt[0]).resolve() if prebuilt else work / "probe.exe"
    if not prebuilt:
        build = run("probe-build", [native, "--native-pipeline", "--backend=c", "--opt=dev",
                                    relative(probe_source), "-o", relative(probe)], 120)
        if build.returncode:
            print(build.stdout.decode(errors="replace") + build.stderr.decode(errors="replace"))
            return 1
    old_route = root / "src/self_hosted/compiler/direct_mir_scalar_program_single_string_route_owner.pgy"
    route = root / "src/self_hosted/compiler/direct_mir_scalar_program_control_flow_route_owner.pgy"
    admission_owner = root / "src/self_hosted/compiler/direct_mir_scalar_program_route_admission_owner.pgy"
    check("control-flow-owner", not old_route.exists() and
          "DirectMirScalarControlFlowProgramRouteClaimed(admitted)" in admission_owner.read_text() and
          "DirectMirScalarSingleStringProgramRouteClaimed" not in admission_owner.read_text() and
          "routine_block_counts" not in route.read_text())

    def reject_mutant(stem, document):
        path = work / (stem + ".json")
        path.write_text(json.dumps(document, separators=(",", ":")), encoding="utf-8")
        # Negative controls never enter projection, a code compiler or execution.
        refused = run(stem, [probe, relative(path)])
        diagnostic = refused.stdout + refused.stderr
        check(stem, refused.returncode == 1 and bool(diagnostic.strip()) and
              b"receiver GraphPlan admitted" not in diagnostic and
              b"empty or unreadable" not in diagnostic)

    for name, source_text, expected, native_code, public_code in cases:
        source = work / (name + ".pgy")
        source.write_text(source_text, encoding="utf-8")
        source_arg = source.relative_to(root).as_posix()
        for origin in ("native", "public"):
            stem = name + "." + origin
            command = ([native, "--native-pipeline", "--mir-json", "--error-format=json", source_arg]
                       if origin == "native" else [driver, "--emit-mir-json-verified", source_arg])
            result = run(stem + ".admission", command)
            if expected is None:
                # Invalid inputs stop at source admission and never execute.
                diagnostic = native_code if origin == "native" else "Code: " + public_code
                check(stem + ".admission", result.returncode == 1 and
                      diagnostic.encode() in result.stdout + result.stderr and b'"pgy.mir.v1"' not in result.stdout)
                continue
            try:
                passed = result.returncode == 0 and json.loads(result.stdout).get("schema") == "pgy.mir.v1"
            except (ValueError, AttributeError):
                passed = False
            check(stem + ".admission", passed)
            if not passed:
                continue
            document = json.loads(result.stdout)
            mir = work / (stem + ".json")
            mir.write_text(json.dumps(document, separators=(",", ":")), encoding="utf-8")
            admitted = run(stem + ".shared.admission", [probe, relative(mir)])
            check(stem + ".shared.admission", admitted.returncode == 0 and
                  b"receiver GraphPlan admitted" in admitted.stdout)
            source_backends = () if shared_only and origin == "public" else ("c", "llvm")
            for backend in source_backends:
                leg = stem + "." + backend
                exe = work / (leg + ".exe")
                command = [native, source_arg, "--backend=" + backend, "--opt=dev", "-o", exe.relative_to(root).as_posix()]
                if origin == "native":
                    command.append("--native-pipeline")
                built = run(leg + ".compile", command)
                passed = built.returncode == 0 and (origin == "native" or b"[pipeline timing]" not in built.stdout + built.stderr)
                if passed:
                    executed = run(leg + ".run", [exe], 10)
                    passed = executed.returncode == 0 and not executed.stderr and executed.stdout.decode().replace("\r\n", "\n") == expected
                check(leg, passed)
            for backend in ("c", "llvm"):
                leg = stem + ".shared." + backend
                projected = run(leg + ".project", [probe, relative(mir), backend])
                passed = projected.returncode == 0
                if passed:
                    code = work / (leg + (".c" if backend == "c" else ".ll"))
                    code.write_bytes(projected.stdout)
                    exe = work / (leg + ".exe")
                    command = (["gcc", "-std=c11", "-O0", "-fwrapv", "-fno-strict-aliasing",
                                relative(code), "-Isrc", "-Isrc/runtime", "-pthread", "-o", relative(exe)]
                               if backend == "c" else ["clang", relative(code), "-o", relative(exe)])
                    compiled = run(leg + ".compile", command)
                    passed = compiled.returncode == 0
                    if passed:
                        executed = run(leg + ".run", [exe], 10)
                        passed = executed.returncode == 0 and not executed.stderr and executed.stdout.decode().replace("\r\n", "\n") == expected
                check(leg, passed)
            if name == "return":
                for field, value in (("abi_type_name", "Int"), ("abi_layout_id", 1),
                                     ("abi_layout_required", True), ("abi_layout", {}),
                                     ("source_type", "AST_IDENTIFIER"), ("expr0", "1"),
                                     ("expr1", "1"), ("arg0", "n"), ("result", "n.8")):
                    mutant = copy.deepcopy(document)
                    instruction = next(i for b in mutant["routines"][0]["blocks"]
                                       for i in b["instructions"] if i["kind"] == "return")
                    instruction[field] = value
                    reject_mutant(stem + ".bad-return-" + field, mutant)
                mutant = copy.deepcopy(document)
                instructions = [i for b in mutant["routines"][0]["blocks"] for i in b["instructions"]]
                return_instruction = next(i for i in instructions if i["kind"] == "return")
                return_instruction["expr0_graph"] = copy.deepcopy(next(i["expr0_graph"] for i in instructions if i["kind"] == "branch"))
                reject_mutant(stem + ".bad-return-graph", mutant)
            if name in ("return", "false-while"):
                mutant = copy.deepcopy(document)
                block = mutant["routines"][0]["blocks"][0]
                block["reachable"] = not block["reachable"]
                reject_mutant(stem + ".crossed-reachability", mutant)
            if name == "false-while":
                branches = [i for b in document["routines"][0]["blocks"]
                            for i in b["instructions"] if i["kind"] == "branch"]
                changes = ("missing", "wrong-kind", "wrong-value")
                if not branches:
                    # Constant folding may remove the wire node. Its positive
                    # shared admission/execution above still must pass.
                    omitted_mutations.append({"case": stem, "reason": "constant branch absent",
                                              "mutations": list(changes)})
                    print(f"[loop-statement] OMIT: {stem} condition mutations; constant branch absent", flush=True)
                    changes = ()
                for change in changes:
                    mutant = copy.deepcopy(document)
                    instruction = next(i for b in mutant["routines"][0]["blocks"]
                                       for i in b["instructions"] if i["kind"] == "branch")
                    graph = instruction["expr0_graph"]
                    assert graph_digest(graph) == graph["digest"]
                    if change == "missing":
                        instruction["expr0_graph"] = None
                    else:
                        node = graph["nodes"][graph["root"]]
                        if change == "wrong-kind":
                            node.update(kind="int_literal", text="3")
                        else:
                            node["text"] = "3"
                        graph["digest"] = graph_digest(graph)
                    reject_mutant(stem + ".bad-condition-" + change, mutant)
            if name == "empty-void":
                mutant = copy.deepcopy(document)
                mutant["routines"][0]["return"] = "Int"
                reject_mutant(stem + ".empty-non-void", mutant)
                mutant = copy.deepcopy(document)
                mutant["routines"][0]["blocks"][0]["succ_true"] = 999
                reject_mutant(stem + ".empty-foreign-edge", mutant)
                mutant = copy.deepcopy(document)
                mutant["routines"][0]["blocks"] = []
                reject_mutant(stem + ".empty-missing-cfg", mutant)
    failures = sum(not row["passed"] for row in rows)
    hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in (native, driver, original, probe_source, probe, route)}
    (work / "report.json").write_text(json.dumps({"scope": "shared-only" if shared_only else "production-and-shared", "hashes": hashes, "checks": rows, "failures": failures, "omitted_mutations": omitted_mutations}, indent=2), encoding="utf-8")
    print(f"[loop-statement] {len(rows)} checks / {failures} failures; evidence: {work}")
    return int(failures != 0)


if __name__ == "__main__":
    raise SystemExit(main())
