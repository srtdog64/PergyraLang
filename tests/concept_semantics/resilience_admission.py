"""Compare resilience refusal at source admission; no input is executed."""
import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile


def main() -> int:
    root = pathlib.Path(__file__).resolve().parents[2]
    native, driver = (pathlib.Path(arg).resolve() for arg in sys.argv[1:3])
    parent = root / ".tmp/concept_semantics/resilience"
    parent.mkdir(parents=True, exist_ok=True)
    work = pathlib.Path(tempfile.mkdtemp(prefix="run.", dir=parent))
    template = (root / "tests/concept_semantics/word_deletion/cases/28_intent_header_policies/retry.pgy").read_text(encoding="utf-8")
    assert template.count("with retry(3)") == 1
    results = []

    def admit(source, origin):
        command = ([str(native), "--native-pipeline", "--mir-json", "--error-format=json"]
                   if origin == "native" else [str(driver), "--emit-mir-json-verified"])
        return subprocess.run(command + [source], cwd=root, capture_output=True,
                              text=True, encoding="utf-8", errors="replace", timeout=45)

    for origin in ("native", "public"):
        control = admit("tests/concept_semantics/intent/single_step_exact.pgy", origin)
        ok = control.returncode == 0 and json.loads(control.stdout).get("schema") == "pgy.mir.v1"
        results.append({"case": "no-retry-valid", "origin": origin, "passed": ok})
        print(f"[resilience-admission] {'PASS' if ok else 'FAIL'}: no-retry-valid {origin}")

    policy_source = "tests/concept_semantics/word_deletion/cases/28_intent_header_policies/orig.pgy"
    policy_template = (root / policy_source).read_text(encoding="utf-8")
    for origin in ("native", "public"):
        control = admit(policy_source, origin)
        (work / f"explicit-full.{origin}.out").write_text(control.stdout, encoding="utf-8")
        (work / f"explicit-full.{origin}.err").write_text(control.stderr, encoding="utf-8")
        try:
            ok = control.returncode == 0 and json.loads(control.stdout).get("schema") == "pgy.mir.v1"
        except (ValueError, AttributeError):
            ok = False
        results.append({"case": "explicit-full-valid", "origin": origin, "passed": ok})
        print(f"[resilience-admission] {'PASS' if ok else 'FAIL'}: explicit-full-valid {origin}")

    for name, old, new, reason in (
        ("duplicate-mode", "exclusive;", "exclusive; concurrent;", "duplicate Intent execution mode"),
        ("duplicate-full", "rollback: full;", "rollback: full; rollback: full;", "duplicate Intent rollback policy"),
        ("duplicate-priority", "priority: 3;", "priority: 3; priority: 4;", "invalid Intent policy clause"),
        ("invalid-rollback", "rollback: full;", "rollback: mystery;", "rollback currently requires full"),
    ):
        source = work / f"{name}.pgy"
        source.write_text(policy_template.replace(old, new), encoding="utf-8")
        for origin in ("native", "public"):
            result = admit(source.relative_to(root).as_posix(), origin)
            message = result.stdout + result.stderr
            (work / f"{name}.{origin}.diagnostic").write_text(message, encoding="utf-8")
            expected = "PGY_PARSE_SYNTAX" if origin == "native" else reason
            ok = result.returncode == 1 and expected in message and '"pgy.mir.v1"' not in result.stdout
            results.append({"case": name, "origin": origin, "passed": ok, "exit": result.returncode})
            print(f"[resilience-admission] {'PASS' if ok else 'FAIL'}: {name} {origin} diagnostic, no MIR")

    for name, modifier, native_code, public_reason in (
        ("one", "retry(1)", "PGY_SEM_INTENT_STEP_INVALID", "Code: intent_retry_unavailable"),
        ("max-int", "retry(2147483647)", "PGY_SEM_INTENT_STEP_INVALID", "Code: intent_retry_unavailable"),
        ("zero", "retry(0)", "PGY_PARSE_SYNTAX", "positive integer attempt count"),
        ("fraction", "retry(1.5)", "PGY_PARSE_SYNTAX", "positive integer attempt count"),
        ("overflow", "retry(2147483648)", "PGY_PARSE_SYNTAX", "positive integer attempt count"),
        ("timeout", "timeout(1)", "PGY_PARSE_SYNTAX", "reserved but not implemented"),
        ("backoff", "backoff(1)", "PGY_PARSE_SYNTAX", "reserved but not implemented"),
    ):
        source = work / f"{name}.pgy"
        source.write_text(template.replace("with retry(3)", f"with {modifier}"), encoding="utf-8")
        for origin in ("native", "public"):
            result = admit(source.relative_to(root).as_posix(), origin)
            message = result.stdout + result.stderr
            (work / f"{name}.{origin}.diagnostic").write_text(message, encoding="utf-8")
            expected = native_code if origin == "native" else public_reason
            ok = (result.returncode == 1 and expected in message and
                  '"pgy.mir.v1"' not in result.stdout)
            results.append({"case": name, "origin": origin, "passed": ok, "exit": result.returncode})
            print(f"[resilience-admission] {'PASS' if ok else 'FAIL'}: {name} {origin} diagnostic, no MIR")
    report = {"native_sha256": hashlib.sha256(native.read_bytes()).hexdigest(),
              "driver_sha256": hashlib.sha256(driver.read_bytes()).hexdigest(), "results": results}
    (work / "report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"[resilience-admission] {len(results)} checks; evidence: {work}")
    return 0 if all(row["passed"] for row in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
