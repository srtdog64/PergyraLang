"""Action declaration admission parity. Invalid inputs are never executed."""
import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile


def main() -> int:
    root = pathlib.Path(__file__).resolve().parents[2]
    native, driver = (pathlib.Path(arg).resolve() for arg in sys.argv[1:3])
    parent = root / ".tmp/concept_semantics/action_authority"
    parent.mkdir(parents=True, exist_ok=True)
    work = pathlib.Path(tempfile.mkdtemp(prefix="run.", dir=parent))
    original = (root / "tests/concept_semantics/word_deletion/cases/06_action_func/orig_within.pgy").read_text(encoding="utf-8")
    positive = (root / "tests/concept_semantics/authority_effect/action_authority_valid.pgy").read_text(encoding="utf-8")
    parameter = positive.replace("Bump(self)", "Bump(self, ref actor: Counter)")
    other = "subject Other { let value: Int; }\n"
    ability_valid = (root / "tests/concept_semantics/word_deletion/cases/21_ability_role/orig.pgy").read_text(encoding="utf-8")
    ability_invalid = (root / "tests/concept_semantics/word_deletion/cases/21_ability_role/neg_orig.pgy").read_text(encoding="utf-8")
    generic = ability_valid.replace("ability Ready {", "ability Ready<T> {", 1).replace("requires Ready", "requires Ready<Int>").replace("impl ability Ready {", "impl ability Ready<Int> {")
    defaulted = generic.replace("ability Ready<T>", "ability Ready<T = Int>")
    paired = generic.replace("ability Ready<T>", "ability Ready<T, U>").replace("Ready<Int>", "Ready<Int, String>")
    dependent = paired.replace("ability Ready<T, U>", "ability Ready<T = Int, U = T>").replace("Ready<Int, String>", "Ready<Int, Int>")
    cases = [
        ("explicit-authority", positive, True, ""),
        ("within-only", original.replace(" authorized by self", ""), True, ""),
        ("without-within", original.replace(" within ArenaZone", ""), True, ""),
        ("subject-parameter", parameter.replace("authorized by self", "authorized by actor"), True, ""),
        ("two-bindings-one-type", parameter.replace("authorized by self", "authorized by self, actor"), True, ""),
        ("no-explicit-authority", original, False, "semantic"),
        ("unknown-zone", original.replace("within ArenaZone", "within MissingZone"), False, "semantic"),
        ("missing-owner-slot", other + original.replace("subject slot c: Counter", "subject slot c: Other"), False, "semantic"),
        ("wrong-binding", positive.replace("authorized by self", "authorized by absent"), False, "semantic"),
        ("non-subject-binding", positive.replace("Bump(self)", "Bump(self, amount: Int)").replace("authorized by self", "authorized by amount"), False, "semantic"),
        ("wrong-authority-type", other + original.replace("subject slot c: Counter", "subject slot c: Counter\nsubject slot other: Other\nauthority other"), False, "semantic"),
        ("authority-other-zone", original + "\nzone OtherZone { subject slot c: Counter\nauthority c }\n", False, "semantic"),
        ("func-within", original.replace("action Bump", "func Bump"), False, "'within' clause is only valid on 'action' declarations"),
        ("duplicate-within", positive.replace("within ArenaZone", "within ArenaZone within ArenaZone"), False, "within clause is duplicated"),
        ("role-implemented", ability_valid, True, ""),
        ("role-missing", ability_invalid, False, "ability"),
        ("role-unrelated-subject", other + ability_valid.replace("for Worker", "for Other"), False, "ability"),
        ("unknown-required-ability", ability_valid.replace("requires Ready", "requires Missing"), False, "ability"),
        ("generic-role", generic, True, ""),
        ("generic-role-wrong-actual", generic.replace("requires Ready<Int>", "requires Ready<String>"), False, "ability"),
        ("generic-required-arity", generic.replace("requires Ready<Int>", "requires Ready"), False, "ability"),
        ("default-requirement", defaulted.replace("requires Ready<Int>", "requires Ready"), True, ""),
        ("default-implementation", defaulted.replace("impl ability Ready<Int>", "impl ability Ready"), True, ""),
        ("ordered-type-pair", paired, True, ""),
        ("crossed-type-pair", paired.replace("requires Ready<Int, String>", "requires Ready<String, Int>"), False, "ability"),
        # Native defaults must be concrete; a previous formal is not a default.
        ("dependent-default", dependent.replace("requires Ready<Int, Int>", "requires Ready"), False, "ability"),
    ]
    results = []
    for name, content, accepted, public_reason in cases:
        source = work / f"{name}.pgy"
        source.write_text(content, encoding="utf-8")
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
                expected = {"semantic": "PGY_SEM_ACTION_CONTRACT_INVALID", "ability": "PGY_SEM_ABILITY_CONTRACT_INVALID"}.get(public_reason, "PGY_PARSE_SYNTAX")
                if origin == "public":
                    expected = {"semantic": "Code: action_contract_invalid", "ability": "Code: action_ability_unsatisfied"}.get(public_reason, public_reason)
                ok = result.returncode == 1 and expected in message and '"pgy.mir.v1"' not in result.stdout
            results.append({"case": name, "origin": origin, "passed": ok, "exit": result.returncode})
            print(f"[action-authority] {'PASS' if ok else 'FAIL'}: {name} {origin}", flush=True)
    report = {"native_sha256": hashlib.sha256(native.read_bytes()).hexdigest(),
              "driver_sha256": hashlib.sha256(driver.read_bytes()).hexdigest(), "results": results}
    (work / "report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    failures = sum(not row["passed"] for row in results)
    print(f"[action-authority] {len(results)} checks / {failures} failures; evidence: {work}")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
