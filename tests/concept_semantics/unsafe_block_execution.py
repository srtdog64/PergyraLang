"""Unsafe is a typed statement effect, not an effect-free block alias.

Only fixed valid scalar programs execute. Invalid sources stop at admission.
--admission-only builds a common-verdict probe; no production parity is claimed.
"""
import hashlib
import json
import os
import pathlib
import subprocess
import sys
import tempfile


def main():
    root = pathlib.Path(__file__).resolve().parents[2]
    native, driver = (pathlib.Path(x).resolve() for x in sys.argv[1:3])
    admission_only = "--admission-only" in sys.argv[3:]
    parent = root / ".tmp/concept_semantics/unsafe_block"
    parent.mkdir(parents=True, exist_ok=True)
    work = pathlib.Path(tempfile.mkdtemp(prefix="run.", dir=parent))
    original = root / "tests/concept_semantics/word_deletion/cases/26_unsafe_block/orig.pgy"
    cases = [
        ("original", original.read_text(encoding="utf-8"), "boundary\n3\n", "", ""),
        ("empty", "func Main() -> Void with effects unsafe { unsafe { } }", "", "", ""),
        ("nested-scope", "func Main() -> Void { let n: Int = 1; unsafe { let n: Int = 2; Log(n); unsafe { n = n + 1; Log(n); } } Log(n); }", "2\n3\n1\n", "", ""),
        ("shadow-initializer", "func Main() -> Void { let n: Int = 5; unsafe { let n: Int = n + 2; Log(n); } Log(n); }", "7\n5\n", "", ""),
        ("shadow-parameter", "func Work(n: Int) -> Int { unsafe { let n: Int = 2; Log(n); } return n; } func Main() -> Void { Log(Work(5)); }", "2\n5\n", "", ""),
        ("inout-branch", "func Touch(inout n: Int, flag: Bool) -> Void { unsafe { if flag { n = 7; } else { n = 9; } } } func Main() -> Void { let n: Int = 1; Touch(n, true); Log(n); Touch(n, false); Log(n); }", "7\n9\n", "", ""),
        ("inout-shadow", "func Touch(inout n: Int) -> Void { n = 7; unsafe { let n: Int = 2; Log(n); } } func Main() -> Void { let n: Int = 1; Touch(n); Log(n); }", "2\n7\n", "", ""),
        ("inout-call-shadow", "func Set(inout n: Int) -> Void { n = 7; } func Touch(inout n: Int) -> Void { Set(n); unsafe { let n: Int = 2; Log(n); } } func Main() -> Void { let n: Int = 1; Touch(n); Log(n); }", "2\n7\n", "", ""),
        ("inout-unchanged", 'func Touch(inout n: Int) -> Void { unsafe { let n: String = "inner"; Log(n); } } func Main() -> Void { let n: Int = 5; Touch(n); Log(n); }', "inner\n5\n", "", ""),
        ("inout-return-call", "func Set(inout n: Int) -> Int { n = 7; return 9; } func Touch(inout n: Int) -> Int { unsafe { return Set(n); } } func Main() -> Void { let n: Int = 1; Log(Touch(n)); Log(n); }", "9\n7\n", "", ""),
        ("inout-assignment-call", "func Set(inout n: Int) -> Void { n = 7; } func Touch(inout n: Int) -> Void { unsafe { Log(n); Set(n); n = n + 1; Log(n); } } func Main() -> Void { let n: Int = 1; Touch(n); Log(n); }", "1\n8\n8\n", "", ""),
        ("inout-value-input", "func Set(inout n: Int) -> Void { n = 7; } func Work(n: Int) -> Void { unsafe { Set(n); Log(n); } } func Main() -> Void { let n: Int = 1; Work(n); Log(n); }", "7\n1\n", "", ""),
        ("inout-scalar-types", 'func Touch(inout n: Int, inout flag: Bool, inout text: String, inout wide: Long) -> Void { unsafe { n = 7; flag = true; text = "changed"; wide = 23L; } } func Main() -> Void { let n: Int = 1; let flag: Bool = false; let text: String = "before"; let wide: Long = 3L; Touch(n, flag, text, wide); Log(n); if flag { Log(text); } Log(wide); }', "7\nchanged\n23\n", "", ""),
        ("shadow-branch", "func Main() -> Void { let n: Int = 1; unsafe { let n: Int = 2; if n == 2 { n = 9; } else { n = 8; } Log(n); } Log(n); }", "9\n1\n", "", ""),
        ("shadow-type", 'func Main() -> Void { let n: Int = 1; unsafe { let n: String = "inner"; Log(n); } Log(n); }', "inner\n1\n", "", ""),
        ("loop-flow", "func Main() -> Void { let n: Int = 0; loop { unsafe { n = n + 1; if n < 3 { continue; } break; } } Log(n); }", "3\n", "", ""),
        ("return", "func Work() -> Int with effects unsafe { unsafe { return 7; } } func Main() -> Void with effects unsafe { Log(Work()); }", "7\n", "", ""),
        ("unreachable-tail", "func Work() -> Int { let left: Int = 7; while true { unsafe { return left; } } return left; } func Main() -> Void { Log(Work()); }", "7\n", "", ""),
        ("unused-body", "func Unused() -> Void { unsafe { } } func Main() -> Void with effects local { Log(1); }", "1\n", "", ""),
        ("forward", "func Forward() -> Int { return Work(); } func Work() -> Int { unsafe { return 7; } } func Main() -> Void with effects unsafe { Log(Forward()); }", "7\n", "", ""),
        ("missing-effect", "func Main() -> Void with effects local { unsafe { } }", None, "PGY_SEM_EFFECT_CONFLICT", "declared_effect_missing"),
        ("wrong-effect", "func Main() -> Void with effects io { unsafe { } }", None, "PGY_SEM_EFFECT_CONFLICT", "declared_effect_missing"),
        ("unused-bound", "func Unused() -> Void with effects local { unsafe { } } func Main() -> Void { }", None, "PGY_SEM_EFFECT_CONFLICT", "declared_effect_missing"),
        ("nested-effect", "func Main() -> Void with effects local { if true { unsafe { } } }", None, "PGY_SEM_EFFECT_CONFLICT", "declared_effect_missing"),
        ("transitive-effect", "func Forward() -> Int { return Work(); } func Work() -> Int { unsafe { return 7; } } func Main() -> Void with effects local { Log(Forward()); }", None, "PGY_SEM_EFFECT_CONFLICT", "declared_effect_missing"),
        ("callable-effect", "func Work(n: Int) -> Int { unsafe { return n; } } func Invoke(op: func(Int) -> Int) -> Int { return op(1); } func Main() -> Void with effects local { Log(Invoke(Work)); }", None, "PGY_SEM_EFFECT_CONFLICT", "declared_effect_missing"),
        ("parallel", "func Main() -> Void { parallel { unsafe { } } }", None, "PGY_SEM_PARALLEL_SECURE_FORBIDDEN", "parallel_unsafe_forbidden"),
        ("nested-parallel", "func Main() -> Void { parallel { if true { unsafe { } } } }", None, "PGY_SEM_PARALLEL_SECURE_FORBIDDEN", "parallel_unsafe_forbidden"),
        ("wrong-body", 'func Main() -> Void { unsafe { let n: Int = "wrong"; } }', None, "PGY_SEM_TYPE_MISMATCH", "let_type_mismatch"),
        ("scope-escape", "func Main() -> Void { unsafe { let n: Int = 2; } Log(n); }", None, "PGY_SEM_UNDEFINED_SYMBOL", "undefined_symbol"),
        ("task-exit", "async func Work() -> Int { return 1; } async func Main() -> Void { unsafe { let task: Future<Int> = spawn Work(); } }", None, "PGY_SEM_TASK_LIFECYCLE", "task_lifecycle_invalid"),
    ]
    env = dict(os.environ, PGY_SELF_DRIVER_BIN=str(driver), PGY_DEBUG_PIPELINE_TIMING="1")
    env.pop("PGY_NATIVE_PIPELINE", None)
    checks = []

    def run(stem, command, timeout=60):
        result = subprocess.run([str(x) for x in command], cwd=root, env=env, capture_output=True, timeout=timeout)
        (work / (stem + ".out")).write_bytes(result.stdout)
        (work / (stem + ".err")).write_bytes(result.stderr)
        return result

    def relative(path):
        return path.relative_to(root).as_posix()

    def check(stem, passed):
        checks.append({"case": stem, "passed": passed})
        print(f"[unsafe-block] {'PASS' if passed else 'FAIL'}: {stem}", flush=True)

    probe = work / "probe.exe"
    if admission_only:
        # Reuse the existing common body/diagnostic receipt validator.
        built = run("probe-build", [native, "--native-pipeline", "--backend=c", "--opt=dev",
                                    "tests/self_hosted/fixtures/future_lifecycle_admission_fact.pgy", "-o", relative(probe)], 180)
        if built.returncode:
            print(built.stdout.decode(errors="replace") + built.stderr.decode(errors="replace"))
            return 1
    for name, source_text, expected, native_code, public_code in cases:
        source = work / (name + ".pgy")
        source.write_text(source_text, encoding="utf-8")
        for origin in ("native", "public"):
            stem = name + "." + origin
            if origin == "native":
                command = [native, "--native-pipeline", "--mir-json", "--error-format=json", relative(source)]
            elif admission_only:
                command = [probe, relative(source), "accept" if expected is not None else public_code]
            else:
                command = [driver, "--emit-mir-json-verified", relative(source)]
            result = run(stem + ".admission", command)
            if origin == "public" and admission_only:
                passed = result.returncode == 0 and b"future lifecycle facts and receipt verified" in result.stdout
            elif expected is None:
                code = native_code if origin == "native" else "Code: " + public_code
                passed = result.returncode == 1 and code.encode() in result.stdout + result.stderr and b'"pgy.mir.v1"' not in result.stdout
            else:
                try:
                    passed = result.returncode == 0 and json.loads(result.stdout).get("schema") == "pgy.mir.v1"
                except (ValueError, AttributeError):
                    passed = False
            check(stem + ".admission", passed)
            if expected is None or admission_only or not passed:
                continue
            for backend in ("c", "llvm"):
                leg = stem + "." + backend
                exe = work / (leg + ".exe")
                command = [native, relative(source), "--backend=" + backend, "--opt=dev", "-o", relative(exe)]
                if origin == "native":
                    command.append("--native-pipeline")
                built = run(leg + ".compile", command)
                passed = built.returncode == 0 and (origin == "native" or b"[pipeline timing]" not in built.stdout + built.stderr)
                if passed:
                    executed = run(leg + ".run", [exe], 10)
                    passed = executed.returncode == 0 and not executed.stderr and executed.stdout.decode().replace("\r\n", "\n") == expected
                check(leg, passed)
    if not admission_only:
        # C-only route-boundary controls, not Float GraphPlan/LLVM coverage.
        # Primitive local inventory cannot classify a Float literal's family.
        for name, body, expected in (
            ("unclaimed-float", "Log(1.5);", "1.500000\n"),
            ("unclaimed-float-body", "unsafe { Log(1.5 + 2.25); }", "3.750000\n"),
        ):
            source = work / (name + ".pgy")
            source.write_text("func Main() -> Void { " + body + " }", encoding="utf-8")
            for origin in ("native", "public"):
                stem = name + "." + origin + ".c"
                exe = work / (stem + ".exe")
                command = [native, relative(source), "--backend=c", "--opt=dev", "-o", relative(exe)]
                if origin == "native":
                    command.append("--native-pipeline")
                built = run(stem + ".compile", command)
                passed = built.returncode == 0 and (origin == "native" or b"[pipeline timing]" not in built.stdout + built.stderr)
                if passed:
                    executed = run(stem + ".run", [exe], 10)
                    passed = executed.returncode == 0 and not executed.stderr and executed.stdout.decode().replace("\r\n", "\n") == expected
                check(stem, passed)
    failures = sum(not row["passed"] for row in checks)
    hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in (native, driver, original, pathlib.Path(__file__))}
    if admission_only:
        hashes[str(probe)] = hashlib.sha256(probe.read_bytes()).hexdigest()
    (work / "report.json").write_text(json.dumps({"scope": "owner-admission-only" if admission_only else "production", "hashes": hashes, "checks": checks, "failures": failures}, indent=2), encoding="utf-8")
    print(f"[unsafe-block] {len(checks)} checks / {failures} failures; evidence: {work}")
    return int(failures != 0)


if __name__ == "__main__":
    raise SystemExit(main())
