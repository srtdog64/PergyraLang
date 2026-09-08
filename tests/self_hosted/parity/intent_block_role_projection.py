"""Producer-owned Intent CFG roles through the existing MIR-to-C consumer.

Valid typed sources alone are compiled and run. Mutated MIR is admission-only,
must produce a controlled diagnostic, and must not publish a C artifact.
This is not coverage of the separate direct GraphPlan C/LLVM entrypoint.
"""
import argparse
import copy
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
    base = root / ".tmp/self_hosted/intent_block_roles"
    base.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="run.", dir=base))
    print(f"[intent-block-roles] evidence: {work}", flush=True)

    def compiler_path(path):
        return path.relative_to(root).as_posix()

    def invoke(command, label, output=None, expected=0):
        stdout = output if output is not None else work / f"{label}.out"
        stderr = work / f"{label}.err"
        with stdout.open("wb") as out, stderr.open("wb") as err:
            result = subprocess.run(command, cwd=root, stdout=out, stderr=err, timeout=60)
        if result.returncode != expected:
            raise RuntimeError(f"{label}: exit {result.returncode}, expected {expected}; see {stdout} / {stderr}")
        return stdout.read_text(encoding="utf-8", errors="replace") + stderr.read_text(encoding="utf-8", errors="replace")

    # Reuse the established transition gate's independent expected observation.
    gate = (root / "tests/self_hosted/parity/intent_typed_outcome_compensation_owner.sh").read_text(encoding="utf-8")
    expected_match = re.search(r"<<'EXPECTED'\n(.*?)\nEXPECTED", gate, re.S)
    if expected_match is None:
        raise RuntimeError("typed transition gate's expected observation is missing")
    expected = expected_match.group(1) + "\n"
    fixture = "tests/self_hosted/parity/fixture/intent_typed_outcome_compensation.pgy"
    checks = 0
    for producer in ("native", "self"):
        mir = work / f"{producer}.mir.json"
        if producer == "native":
            invoke([args.native, "--native-pipeline", "--mir-json", fixture], f"{producer}-mir", output=mir)
        else:
            invoke([args.driver, "--emit-mir-json-verified", fixture, "-o", compiler_path(mir)], f"{producer}-mir")
        document = json.loads(mir.read_text(encoding="utf-8-sig"))
        intent = next(r for r in document["routines"] if r["kind"] == "intent")
        roots = {name: [b for b in intent["blocks"] if b.get("intent_block_role") == name]
                 for name in ("entry", "cleanup", "rollback", "invalidation")}
        assert all(len(rows) == 1 for rows in roots.values())
        mirrors = [b for b in intent["blocks"] if b.get("intent_block_role") == "mirror"]
        assert bool(mirrors) == (producer == "native")
        legacy_statements = [i for b in intent["blocks"] if b.get("intent_block_role") != "execution"
                             for i in b["instructions"] if i.get("name") == "stmt"]
        assert bool(legacy_statements) == (producer == "native")
        rollback = roots["rollback"][0]
        compensation = [r for r in rollback["instructions"] if r["name"] == "CompensateIntentStep"]
        assert len(compensation) == 2
        c = work / f"{producer}.c"
        exe = work / f"{producer}.exe"
        invoke([args.driver, "--mir-json", compiler_path(mir), "-o", compiler_path(c)], f"{producer}-projection")
        assert c.is_file() and c.stat().st_size > 0
        invoke([args.cc, "-x", "c", "-std=c11", "-O0", "-fwrapv", "-fno-strict-aliasing",
                "-I" + str(root / "src"), "-I" + str(root / "src/runtime"), "-pthread",
                str(c), "-o", str(exe)], f"{producer}-compile")
        run = work / f"{producer}.run"
        invoke([str(exe)], f"{producer}-run", output=run)
        assert run.read_text(encoding="utf-8") == expected
        assert (work / f"{producer}-run.err").stat().st_size == 0
        print(f"[intent-block-roles] {producer} MIR -> self C: 32 transition observations PASS", flush=True)

        def block(d, role):
            r = next(r for r in d["routines"] if r["kind"] == "intent")
            return next(b for b in r["blocks"] if b.get("intent_block_role") == role)

        def cross_cleanup(d):
            left, right = block(d, "rollback"), block(d, "invalidation")
            left["intent_block_role"], right["intent_block_role"] = "invalidation", "rollback"

        def remove_compensation(d):
            rows = block(d, "rollback")["instructions"]
            rows.remove(next(r for r in rows if r["name"] == "CompensateIntentStep"))

        def cross_compensation(d):
            row = next(r for r in block(d, "rollback")["instructions"] if r["name"] == "CompensateIntentStep")
            row["arg1"] = "UnknownCompensation"

        cases = [
            ("missing-role", lambda d: block(d, "entry").pop("intent_block_role"), None),
            ("mistyped-role", lambda d: block(d, "entry").update(intent_block_role=0), None),
            ("duplicate-root", lambda d: block(d, "cleanup").update(intent_block_role="entry"), None),
            ("crossed-cleanup", cross_cleanup, None),
            ("cyclic-body", lambda d: block(d, "entry").update(succ_true=block(d, "entry")["id"]), None),
            ("execution-as-mirror", lambda d: block(d, "execution").update(intent_block_role="mirror"), None),
            ("missing-rollback-compensation", remove_compensation, "rollback compensation identity is missing"),
            ("crossed-rollback-compensation", cross_compensation, "rollback compensation identity is invalid"),
        ]
        if mirrors:
            cases.extend([
                ("reachable-mirror", lambda d: block(d, "mirror").update(reachable=True), None),
                ("mirror-as-body", lambda d: block(d, "mirror").update(intent_block_role="body"), None),
            ])

            def cross_mirror(d):
                r = next(r for r in d["routines"] if r["kind"] == "intent")
                rows = [i for b in r["blocks"] for i in b["instructions"]
                        if i.get("name") == "stmt" and i.get("source_type") == "AST_CALL"]
                left = next(i for i in rows if i.get("expr0") == "actor.UndoA()")
                right = next(i for i in rows if i.get("expr0") == "actor.UndoB()")
                left["expr0"], left["expr0_graph"] = right["expr0"], copy.deepcopy(right["expr0_graph"])

            cases.append(("crossed-mirror-identity", cross_mirror, "legacy graph mirror multiset is inconsistent"))

        for name, mutate, required in cases:
            changed = copy.deepcopy(document)
            mutate(changed)
            bad = work / f"{producer}-{name}.mir.json"
            bad.write_text(json.dumps(changed, separators=(",", ":")) + "\n", encoding="utf-8")
            artifact = work / f"{producer}-{name}.c"
            assert not artifact.exists()
            diagnostic = invoke([args.driver, "--mir-json", compiler_path(bad), "-o", compiler_path(artifact)],
                                f"{producer}-{name}", expected=1)
            assert not artifact.exists()
            assert "MIR-LOWER ERROR:" in diagnostic or "CODEGEN ERROR:" in diagnostic
            if required is not None:
                assert required in diagnostic, (producer, name, diagnostic)
            checks += 1
    print(f"[intent-block-roles] both producers: 64 observations, {checks} no-artifact refusals PASS", flush=True)


if __name__ == "__main__":
    main()
