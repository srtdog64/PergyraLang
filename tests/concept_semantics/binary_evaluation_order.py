"""Fixed valid observations, independently specified before comparing backends.

No malformed input executes. --binary-only isolates the eager/lazy expression
rung; the default also reports the still-required Intent production legs.
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
    native, driver = (pathlib.Path(p).resolve() for p in sys.argv[1:3])
    parent = root / ".tmp/concept_semantics/binary_evaluation_order"
    parent.mkdir(parents=True, exist_ok=True)
    work = pathlib.Path(tempfile.mkdtemp(prefix="run.", dir=parent))
    fixtures = root / "tests/self_hosted/parity/fixture"
    cases = [(fixtures / "binary_evaluation_order.pgy",
              "12\n2\na11\n1\ntrue\n2\ntrue\n1\n0\n8\nfalse\ntrue\n0\ntrue\n1\n")]
    if "--binary-only" not in sys.argv[3:]:
        cases.extend([
            (root / "tests/concept_semantics/word_deletion/cases/28_intent_header_policies/orig.pgy",
             "true 1\n"),
            (fixtures / "intent_participant_observation.pgy", "true 1 1\n"),
        ])
    env = dict(os.environ, PGY_SELF_DRIVER_BIN=str(driver), PGY_DEBUG_PIPELINE_TIMING="1")
    env.pop("PGY_NATIVE_PIPELINE", None)
    checks = []

    def run(stem, command, timeout):
        result = subprocess.run([str(p) for p in command], cwd=root, env=env,
                                capture_output=True, timeout=timeout)
        (work / (stem + ".out")).write_bytes(result.stdout)
        (work / (stem + ".err")).write_bytes(result.stderr)
        return result

    for source, expected in cases:
        for origin in ("native", "public"):
            for backend in ("c", "llvm"):
                stem = source.stem + "." + origin + "." + backend
                exe = work / (stem + ".exe")
                command = [native, source.relative_to(root).as_posix(),
                           "--backend=" + backend, "--opt=dev", "-o", exe.relative_to(root).as_posix()]
                if origin == "native":
                    command.append("--native-pipeline")
                row = {"case": stem, "expected": expected, "passed": False,
                       "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest()}
                try:
                    built = run(stem + ".compile", command, 45)
                    row["compile_exit"] = built.returncode
                    if built.returncode == 0:
                        executed = run(stem + ".run", [exe], 10)
                        actual = executed.stdout.decode(errors="replace").replace("\r\n", "\n")
                        row.update(run_exit=executed.returncode, actual=actual)
                        row["passed"] = (executed.returncode == 0 and not executed.stderr and
                                         actual == expected and (origin == "native" or
                                         b"[pipeline timing]" not in built.stdout + built.stderr))
                except subprocess.TimeoutExpired as error:
                    row["operational_error"] = f"timeout after {error.timeout}s"
                checks.append(row)
                print(f"[binary-order] {'PASS' if row['passed'] else 'FAIL'}: {stem}", flush=True)
    report = {"native_sha256": hashlib.sha256(native.read_bytes()).hexdigest(),
              "driver_sha256": hashlib.sha256(driver.read_bytes()).hexdigest(), "checks": checks}
    (work / "report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    passed = sum(row["passed"] for row in checks)
    print(f"[binary-order] {passed} passed / {len(checks) - passed} failed; {work}", flush=True)
    return int(passed != len(checks))


if __name__ == "__main__":
    sys.exit(main())
