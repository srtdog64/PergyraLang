"""Intent phase observations and admission-only source counterexamples.

--c-rung selects native/public C while LLVM's general Intent execution remains
an explicit separate obligation. Default executes all four production legs.
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
    native, driver = [pathlib.Path(p).resolve() for p in sys.argv[1:3]]
    parent = root / ".tmp/concept_semantics/intent_predicates"
    parent.mkdir(parents=True, exist_ok=True)
    work = pathlib.Path(tempfile.mkdtemp(prefix="run.", dir=parent))
    env = dict(os.environ, PGY_SELF_DRIVER_BIN=str(driver), PGY_DEBUG_PIPELINE_TIMING="1")
    env.pop("PGY_NATIVE_PIPELINE", None)
    checks = []

    def run(stem, command, timeout=45):
        try:
            result = subprocess.run([str(x) for x in command], cwd=root, env=env,
                                    capture_output=True, timeout=timeout)
        except subprocess.TimeoutExpired as error:
            record(stem + ".timeout", False, operational_error=f"timeout after {timeout}s")
            result = subprocess.CompletedProcess(command, 124, error.stdout or b"",
                                                 (error.stderr or b"") + b"\nTimed out\n")
        (work / (stem + ".out")).write_bytes(result.stdout)
        (work / (stem + ".err")).write_bytes(result.stderr)
        return result

    def record(stem, passed, **details):
        checks.append(dict(case=stem, passed=bool(passed), **details))
        print(f"[intent-predicates] {'PASS' if passed else 'FAIL'}: {stem}", flush=True)

    def admit(stem, source, origin):
        command = ([native, "--native-pipeline", "--mir-json"] if origin == "native"
                   else [driver, "--emit-mir-json-verified"])
        return run(stem, command + [source.relative_to(root).as_posix()])

    base = root / "tests/concept_semantics"
    fixtures = base / "intent_predicates"
    cases = [(base / "word_deletion/cases/12_guard_family/pre.pgy",
              "ok=false w=0 z.w=0 fail=pre:S\n"),
             (base / "word_deletion/cases/12_guard_family/invariant.pgy",
              "ok=false w=0 z.w=0 fail=invariant-pre:S\n"),
             (fixtures / "checks_pass.pgy", "true 3 3\n"),
             (fixtures / "invariant_post_failure.pgy", "false 3 3\ninvariant-post:S\n"),
             (fixtures / "predicate_order.pgy", "pre\ninvariant\naction\ninvariant\ntrue 1\n"),
             (fixtures / "header_success_false.pgy", "false\n1\n1\n"),
             (fixtures / "header_success_observed.pgy", "completion\n1\ntrue\n1\n1\n"),
             (fixtures / "header_success_repeated.pgy", "completion\n2\ntrue\n2\n2\n"),
             (fixtures / "header_success_placement_exact.pgy", "true\n1\n50\n1\n"),
             (fixtures / "header_success_placement_unique.pgy", "true\n1\n50\n1\n"),
             (fixtures / "header_success_default.pgy", "true\n1\n1\n")]
    backends = ("c",) if "--c-rung" in sys.argv[3:] else ("c", "llvm")
    for source, expected in cases:
        for origin in ("native", "public"):
            for backend in backends:
                stem = f"{source.stem}.{origin}.{backend}"
                exe = work / (stem + ".exe")
                command = [native, source.relative_to(root).as_posix(), "--backend=" + backend,
                           "--opt=dev", "-o", exe.relative_to(root).as_posix()]
                if origin == "native":
                    command.append("--native-pipeline")
                built = run(stem + ".compile", command)
                actual, run_exit = "", None
                if built.returncode == 0:
                    executed = run(stem + ".run", [exe], 10)
                    actual, run_exit = executed.stdout.decode(errors="replace").replace("\r\n", "\n"), executed.returncode
                passed = (built.returncode == 0 and run_exit == 0 and actual == expected and
                          not executed.stderr and (origin == "native" or
                          b"[pipeline timing]" not in built.stdout + built.stderr))
                record(stem, passed, compile_exit=built.returncode, run_exit=run_exit,
                       expected=expected, actual=actual)
    template = cases[2][0].read_text(encoding="utf-8")
    for name, before, after, reason in (
        ("pre-type", "pre: w.calls == 2 && z.w.calls == 2;", "pre: 3;", "condition_not_bool"),
        ("invariant-type", "invariant: w.calls <= 3;", "invariant: 3;", "condition_not_bool"),
        ("success-type", "success: true;", "success: 3;", "condition_not_bool"),
        ("failure-type", "failure: false;", "failure: 3;", "condition_not_bool"),
        ("pre-duplicate", "pre: w.calls == 2 && z.w.calls == 2;", "pre: true; pre: false;", "duplicate Intent step predicate"),
        ("invariant-duplicate", "invariant: w.calls <= 3;", "invariant: true; invariant: false;", "duplicate Intent step predicate"),
    ):
        source = work / (name + ".pgy")
        assert template.count(before) == 1
        source.write_text(template.replace(before, after), encoding="utf-8")
        for origin in ("native", "public"):
            stem = f"{name}.{origin}.admission"
            result = admit(stem, source, origin)
            message = (result.stdout + result.stderr).decode(errors="replace")
            record(stem, result.returncode == 1 and '"pgy.mir.v1"' not in message and
                   (origin == "native" or reason in message), exit=result.returncode)
    phase_template = (root / "tests/self_hosted/parity/fixture/intent_phase_carrier_admission.pgy").read_text(encoding="utf-8")
    for phase, before in (("pre", "pre: true;"), ("invariant", "invariant: worker.calls >= 0;")):
        source = work / (phase + "-current-outcome.pgy")
        assert phase_template.count(before) == 1
        source.write_text(phase_template.replace(before, f"{phase}: IntentRunAccepted(outcome);"), encoding="utf-8")
        for origin in ("native", "public"):
            stem = f"{phase}.current-outcome.{origin}.admission"
            result = admit(stem, source, origin)
            record(stem, result.returncode == 1 and bool(result.stdout + result.stderr) and
                   b'"pgy.mir.v1"' not in result.stdout, exit=result.returncode)
    report = dict(native_sha256=hashlib.sha256(native.read_bytes()).hexdigest(),
                  driver_sha256=hashlib.sha256(driver.read_bytes()).hexdigest(),
                  backends=backends, checks=checks)
    (work / "report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    passed = sum(row["passed"] for row in checks)
    print(f"[intent-predicates] {passed} passed / {len(checks) - passed} failed; {work}")
    return int(passed != len(checks))


if __name__ == "__main__":
    sys.exit(main())
