"""Future source-admission parity; never compile or execute test inputs."""
import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile


def main() -> int:
    root = pathlib.Path(__file__).resolve().parents[2]
    subprocess.run([sys.executable, str(root / "scripts/render_native_primitive_type_projection.py"), "--check"],
                   cwd=root, check=True, timeout=30)
    native, driver = (pathlib.Path(arg).resolve() for arg in sys.argv[1:3])
    parent = root / ".tmp/concept_semantics/future_lifecycle"
    parent.mkdir(parents=True, exist_ok=True)
    work = pathlib.Path(tempfile.mkdtemp(prefix="run.", dir=parent))
    prefix = "async func Work(x: Int) -> Int { return x * 3; }\nfunc Choice() -> Bool { return true; }\n"
    binding = "let t: Future<Int> = spawn Work(14);\n"
    retired = "let value: Int = await t;\n"
    cases = [
        ("awaited", binding + retired, True, ""),
        ("fallthrough", binding + 'Log("spawned");', False, ""),
        ("early-return", binding + "return;", False, ""),
        ("cancel-only", binding + "Cancel(t);", False, ""),
        ("cancel-and-await", binding + "Cancel(t);" + retired, True, ""),
        ("one-branch", binding + "if Choice() {" + retired + "}", False, ""),
        ("both-branches", binding + "if Choice() {" + retired + "} else {" + retired + "}", True, ""),
        ("branch-early-return", binding + "if Choice() { return; }" + retired, False, ""),
        ("retired-branch-return", binding + "if Choice() {" + retired + "return; }" + retired, True, ""),
        ("nested-local-leak", "if Choice() {" + binding + "}", False, ""),
        ("nested-local-retired", "if Choice() {" + binding + retired + "}", True, ""),
        ("loop-local-retired", "while Choice() {" + binding + retired + "}", True, ""),
        ("loop-local-break", "while Choice() {" + binding + "break; }", False, ""),
        ("loop-local-continue", "while Choice() {" + binding + "continue; }", False, ""),
        ("loop-possibly-skipped", binding + "while Choice() {" + retired + "break; }", False, ""),
        ("alias-binding", binding + "let other: Future<Int> = t;" + retired, False, ""),
        ("mutable-binding", binding.replace("let t", "let mut t") + retired, False, ""),
        ("immediate-await", "let value: Int = await spawn Work(14);", True, ""),
        ("unowned-spawn", "spawn Work(14);", False, ""),
        ("own-transfer", binding + "Drain(t);", True,
         "async func Drain(own pending: Future<Int>) -> Void { let value: Int = await pending; }\n"),
        ("unretired-own-parameter", "", False,
         "async func Drain(own pending: Future<Int>) -> Void { Log(\"pending\"); }\n"),
        ("same-name-inner-value", binding + "if Choice() { let t: Int = 7; Log(t); }" + retired, True, ""),
        ("same-name-inner-future", binding + "if Choice() {" + binding + retired + "}" + retired, True, ""),
        ("two-handles-retired", binding + "let other: Future<Int> = spawn Work(2);" + retired
         + "let second: Int = await other;", True, ""),
        ("literal-true-loop-break", binding + "while true {" + retired + "break; }", True, ""),
        ("literal-false-branch", binding + "if false { let ignored: Int = await t; }" + retired, True, ""),
        ("short-circuit-unreached", binding + "let skip: Bool = false && ((await t) == 42);" + retired, True, ""),
        ("default-match-retired", binding + "match 1 { case 0: let first: Int = await t; default: let second: Int = await t; }", True, ""),
        ("enum-match-retired", binding + "let flag: Flag = On; match flag { case On: let first: Int = await t;"
         + "case Off: let second: Int = await t; }", True, "enum Flag { On, Off }\n"),
        ("member-name-not-binding", binding + retired + "let boxed: Boxed = Boxed(9); Log(boxed.t);", True,
         "struct Boxed { let t: Int; }\n"),
        ("deferred-await", binding + "defer { let value: Int = await t; }", False, ""),
        ("short-circuit-conditional-leak", binding + "let used: Bool = Choice() && ((await t) == 42);", False, ""),
        ("short-circuit-or-leak", binding + "let used: Bool = Choice() || ((await t) == 42);", False, ""),
        ("short-circuit-taken", binding + "let used: Bool = true && ((await t) == 42);", True, ""),
        ("cancel-argument", "Cancel(42);", False, ""),
        ("cancel-owned-payload", "", False,
         "async func Stop(own task: Future<Array<Int>>) -> Void { Cancel(task); let values: Array<Int> = await task; }\n"),
        ("cancel-enum-payload", "", True,
         "enum Status { Done, Aborted }\nasync func Stop(own task: Future<Status>) -> Void { Cancel(task); let value: Status = await task; }\n"),
    ]
    results = []
    for name, body, accepted, extra in cases:
        source = work / f"{name}.pgy"
        source.write_text(prefix + extra + "async func Main() -> Void {\n" + body + "\n}\n", encoding="utf-8")
        for origin, binary in (("native", native), ("public", driver)):
            command = ([str(binary), "--native-pipeline", "--mir-json", "--error-format=json"]
                       if origin == "native" else [str(binary), "--emit-mir-json-verified"])
            result = subprocess.run(command + [source.relative_to(root).as_posix()],
                                    cwd=root, capture_output=True, text=True,
                                    encoding="utf-8", errors="replace", timeout=45)
            message = result.stdout + result.stderr
            (work / f"{name}.{origin}.out").write_text(result.stdout, encoding="utf-8")
            (work / f"{name}.{origin}.err").write_text(result.stderr, encoding="utf-8")
            if accepted:
                ok = result.returncode == 0 and json.loads(result.stdout).get("schema") == "pgy.mir.v1"
            else:
                native_codes = {"cancel-argument": "PGY_SEM_BUILTIN_ARGS_INVALID",
                                "cancel-owned-payload": "PGY_SEM_REMOTE_FUTURE_MISUSE"}
                public_codes = {"cancel-argument": "cancel_argument_invalid",
                                "cancel-owned-payload": "cancel_payload_unsupported"}
                expected = (native_codes.get(name, "PGY_SEM_TASK_LIFECYCLE") if origin == "native"
                            else "Code: " + public_codes.get(name, "task_lifecycle_invalid"))
                ok = result.returncode == 1 and expected in message and '"pgy.mir.v1"' not in result.stdout
            results.append({"case": name, "origin": origin, "passed": ok, "exit": result.returncode})
            print(f"[future-lifecycle] {'PASS' if ok else 'FAIL'}: {name} {origin}", flush=True)
    report = {"native_sha256": hashlib.sha256(native.read_bytes()).hexdigest(),
              "driver_sha256": hashlib.sha256(driver.read_bytes()).hexdigest(), "results": results}
    (work / "report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    failures = sum(not row["passed"] for row in results)
    print(f"[future-lifecycle] {len(results)} checks / {failures} failures; evidence: {work}")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
