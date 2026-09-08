"""Legacy completion expression identity through both MIR producers to self C.

Only valid source controls and the owner validator are executed. Modified MIR
is refusal-only and may not publish C; the validator never emits altered facts.
This gate does not cover the separate direct GraphPlan emitter.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", required=True)
    parser.add_argument("--driver", required=True)
    parser.add_argument("--cc", default="gcc")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    parent = root / ".tmp/self_hosted/intent_completion"
    parent.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="run.", dir=parent))
    print(f"[intent-completion] evidence: {work}", flush=True)
    checks = []

    def relative(path):
        return path.relative_to(root).as_posix()

    def invoke(command, label, expected=0):
        result = subprocess.run(command, cwd=root, capture_output=True, timeout=60)
        (work / f"{label}.out").write_bytes(result.stdout)
        (work / f"{label}.err").write_bytes(result.stderr)
        if result.returncode != expected:
            raise RuntimeError(f"{label}: exit {result.returncode}, expected {expected}; see {work}")
        return result

    def check(label, condition):
        checks.append(dict(case=label, passed=bool(condition)))
        if not condition:
            raise RuntimeError(f"{label}: failed; see {work}")
        print(f"[intent-completion] PASS: {label}", flush=True)

    def completion(document):
        intent = next(r for r in document["routines"] if r["kind"] == "intent")
        found = [(b["instructions"], i) for b in intent["blocks"] for i in b["instructions"]
                 if i.get("name") == "IntentCheck" and i.get("arg0") == "success"]
        assert len(found) == 1
        return found[0]

    probe = work / "intent-step-plan-probe.exe"
    invoke([args.native, "--native-pipeline",
            "tests/self_hosted/parity/fixture/intent_step_plan_probe.pgy",
            "--backend=c", "-o", relative(probe)], "step-plan-probe-build")
    nested_gate = (root / "tests/self_hosted/parity/direct_mir_nested_intent_program_llvm_owner.sh").read_text(encoding="utf-8")
    nested_match = re.search(r"<<'EXPECTED'\n(.*?)\nEXPECTED", nested_gate, re.S)
    if nested_match is None:
        raise RuntimeError("nested Intent independent expected observation is missing")
    nested_expected = nested_match.group(1) + "\n"

    for producer in ("native", "self"):
        for case, expected in (("false", "false\n1\n1\n"),
                               ("observed", "completion\n1\ntrue\n1\n1\n"),
                               ("repeated", "completion\n2\ntrue\n2\n2\n"),
                               ("placement_exact", "true\n1\n50\n1\n"),
                               ("placement_unique", "true\n1\n50\n1\n"),
                               ("default", "true\n1\n1\n")):
            label = f"{producer}-{case}"
            source = f"tests/concept_semantics/intent_predicates/header_success_{case}.pgy"
            mir = work / f"{label}.mir.json"
            command = ([args.native, "--native-pipeline", "--mir-json", source] if producer == "native"
                       else [args.driver, "--emit-mir-json-verified", source])
            result = invoke(command, label + "-produce")
            mir.write_bytes(result.stdout)
            document = json.loads(result.stdout)
            _, carrier = completion(document)
            check(label + "-purpose", carrier["arg1"] == carrier["slot_anchor"] == "Complete"
                  and carrier["expr0_graph"] is not None)
            intent = next(r for r in document["routines"] if r["kind"] == "intent")
            instructions = [i for b in intent["blocks"] for i in b["instructions"]]
            check(label + "-no-statement-mirror", not any(
                i.get("name") == "stmt" and i.get("expr0_graph") for i in instructions))
            if case == "repeated":
                calls = [i for i in instructions if i.get("name") == "IntentEval" and i.get("arg0") == "on"]
                check(label + "-distinct-occurrences", len(calls) == 2
                      and {i["slot_anchor"] for i in calls} == {"First", "Second"}
                      and calls[0]["expr0"] == calls[1]["expr0"])
                for mode in ("control", "purpose", "phase", "target"):
                    result = invoke([probe, relative(mir), mode], label + "-plan-" + mode,
                                    expected=0 if mode == "control" else 1)
                    message = (result.stdout + result.stderr).decode(errors="replace")
                    expected_message = {
                        "control": "Intent step plan verified: 2 occurrences",
                        "purpose": "MIR Intent step plan owner inputs are inconsistent",
                        "phase": "MIR Intent step plan owner inputs are inconsistent",
                        "target": "MIR intent call target syntax identity is inconsistent",
                    }[mode]
                    check(label + "-plan-" + mode, expected_message in message)
            c = work / f"{label}.c"
            exe = work / f"{label}.exe"
            invoke([args.driver, "--mir-json", relative(mir), "-o", relative(c)], label + "-project")
            check(label + "-artifact", c.is_file() and c.stat().st_size > 0)
            invoke([args.cc, "-x", "c", "-std=c11", "-O0", "-fwrapv", "-fno-strict-aliasing",
                    "-I" + str(root / "src"), "-I" + str(root / "src/runtime"), "-pthread",
                    str(c), "-o", str(exe)], label + "-compile")
            result = invoke([str(exe)], label + "-run")
            check(label + "-observation", result.stdout.decode().replace("\r\n", "\n") == expected
                  and not result.stderr)
            if case != "observed":
                continue
            for mutation in ("missing", "duplicate", "foreign-purpose", "wrong-slot", "missing-graph", "result-type", "entry-statement-mirror", "on-target-phase", "placement-slot-kind", "on-statement-mirror", "on-mirror-without-carrier"):
                bad = copy.deepcopy(document)
                rows, row = completion(bad)
                if mutation == "missing":
                    rows.remove(row)
                elif mutation == "duplicate":
                    rows.insert(rows.index(row), copy.deepcopy(row))
                elif mutation == "foreign-purpose":
                    row["arg1"] = row["slot_anchor"] = "Foreign"
                elif mutation == "wrong-slot":
                    row["slot_anchor"] = "Foreign"
                elif mutation == "missing-graph":
                    row["expr0_graph"] = None
                elif mutation == "result-type":
                    row["result"], row["abi_type_name"] = "illegal", "Bool"
                elif mutation == "entry-statement-mirror":
                    mirror = copy.deepcopy(row)
                    mirror.update(name="stmt", arg0=None, arg1=None, slot_anchor=None)
                    rows.insert(rows.index(row), mirror)
                elif mutation == "on-target-phase":
                    intent = next(r for r in bad["routines"] if r["kind"] == "intent")
                    on = next(i for b in intent["blocks"] for i in b["instructions"]
                              if i.get("name") == "IntentEval" and i.get("arg0") == "on")
                    on["arg0"] = "intent"
                elif mutation == "placement-slot-kind":
                    zone = next(d for d in bad["decls"] if d["name"] == "HeaderZone")
                    slot = next(f for f in zone["fields"] if f["name"] == "worker")
                    slot["field_kind"] = "field"
                else:
                    intent = next(r for r in bad["routines"] if r["kind"] == "intent")
                    body, on = next((b["instructions"], i) for b in intent["blocks"] for i in b["instructions"]
                                    if i.get("name") == "IntentEval" and i.get("arg0") == "on")
                    mirror = copy.deepcopy(on)
                    mirror.update(id=max(i["id"] for b in intent["blocks"] for i in b["instructions"]) + 1,
                                  name="stmt", arg0=None, arg1=None, slot_anchor=None)
                    body.insert(body.index(on), mirror)
                    if mutation == "on-mirror-without-carrier":
                        body.remove(on)
                negative = label + "-" + mutation
                bad_path = work / f"{negative}.mir.json"
                artifact = work / f"{negative}.c"
                bad_path.write_text(json.dumps(bad), encoding="utf-8")
                result = invoke([args.driver, "--mir-json", relative(bad_path), "-o", relative(artifact)],
                                negative, expected=1)
                diagnostic = (result.stdout + result.stderr).decode(errors="replace")
                expected_message = {
                    "missing": "MIR intent completion expression is missing",
                    "duplicate": "MIR intent completion purpose or cardinality is invalid",
                    "foreign-purpose": "MIR intent completion purpose or cardinality is invalid",
                    "wrong-slot": "MIR intent completion purpose or cardinality is invalid",
                    "missing-graph": "MIR intent phase carrier expression graph is missing",
                    "result-type": "MIR intent check result/type shape is invalid",
                    "entry-statement-mirror": "MIR legacy intent executable mirror is retired",
                    "on-target-phase": "MIR intent evaluation phase disagrees with its target kind",
                    "placement-slot-kind": "field kind is invalid for nominal declaration: HeaderZone.worker",
                    "on-statement-mirror": "MIR legacy intent executable mirror is retired",
                    "on-mirror-without-carrier": "MIR legacy intent executable mirror is retired",
                }[mutation]
                check(negative, expected_message in diagnostic and not artifact.exists())
        # Exercise the plan's direct nested-target branch, not only member calls.
        label = f"{producer}-nested"
        source = "tests/self_hosted/parity/fixture/intent_priority_nested_observability.pgy"
        command = ([args.native, "--native-pipeline", "--mir-json", source] if producer == "native"
                   else [args.driver, "--emit-mir-json-verified", source])
        mir = work / f"{label}.mir.json"
        mir.write_bytes(invoke(command, label + "-produce").stdout)
        nested_document = json.loads(mir.read_bytes())
        outer = next(r for r in nested_document["routines"] if r["name"] == "OuterPriority")
        nested_evals = [i for b in outer["blocks"] for i in b["instructions"]
                        if i.get("name") == "IntentEval" and i.get("arg1") == "inner"]
        check(label + "-target-phase", len(nested_evals) == 1 and nested_evals[0]["arg0"] == "intent")
        artifact = work / f"{label}.c"
        executable = work / f"{label}.exe"
        invoke([args.driver, "--mir-json", relative(mir), "-o", relative(artifact)], label + "-project")
        check(label + "-artifact", artifact.is_file() and artifact.stat().st_size > 0)
        invoke([args.cc, "-x", "c", "-std=c11", "-O0", "-fwrapv", "-fno-strict-aliasing",
                "-I" + str(root / "src"), "-I" + str(root / "src/runtime"), "-pthread",
                str(artifact), "-o", str(executable)], label + "-compile")
        result = invoke([executable], label + "-run")
        check(label + "-observation",
              result.stdout.decode().replace("\r\n", "\n") == nested_expected and not result.stderr)
        nested_evals[0]["arg0"] = "on"
        crossed = work / f"{label}-crossed-phase.mir.json"
        refused = work / f"{label}-crossed-phase.c"
        crossed.write_text(json.dumps(nested_document), encoding="utf-8")
        result = invoke([args.driver, "--mir-json", relative(crossed), "-o", relative(refused)],
                        label + "-crossed-phase", expected=1)
        diagnostic = (result.stdout + result.stderr).decode(errors="replace")
        check(label + "-crossed-phase", "MIR intent evaluation phase disagrees with its target kind" in diagnostic
              and not refused.exists())
    report = dict(checks=checks, native_sha256=hashlib.sha256(Path(args.native).read_bytes()).hexdigest(),
                  driver_sha256=hashlib.sha256(Path(args.driver).read_bytes()).hexdigest())
    (work / "report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"[intent-completion] {len(checks)} checks PASS", flush=True)


if __name__ == "__main__":
    main()
