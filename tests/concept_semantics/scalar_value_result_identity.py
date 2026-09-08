"""Exact scalar formal storage, with refusal-only crossed MIR facts.

Only the fixed valid fixture executes. Optional third argument reuses the
shared admission/projection probe built by loop_statement_execution.py.
"""
import copy
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
    parent = root / ".tmp/concept_semantics/scalar_value_result"
    parent.mkdir(parents=True, exist_ok=True)
    work = pathlib.Path(tempfile.mkdtemp(prefix="run.", dir=parent))
    source = root / "tests/self_hosted/parity/fixture/scalar_value_result_identity.pgy"
    probe_source = root / "tests/self_hosted/parity/fixture/identity_cell_receiver_probe.pgy"
    probe = pathlib.Path(sys.argv[3]).resolve() if len(sys.argv) == 4 else work / "probe.exe"
    env = dict(os.environ, PGY_SELF_DRIVER_BIN=str(driver), PGY_DEBUG_PIPELINE_TIMING="1")
    env.pop("PGY_NATIVE_PIPELINE", None)
    checks = []
    expected = "inner\n9\n7\n5\n42\n9\n7\n"

    def relative(path):
        return path.relative_to(root).as_posix()

    def run(stem, command, timeout=45):
        result = subprocess.run([str(p) for p in command], cwd=root, env=env,
                                capture_output=True, timeout=timeout)
        (work / (stem + ".out")).write_bytes(result.stdout)
        (work / (stem + ".err")).write_bytes(result.stderr)
        return result

    def check(stem, passed):
        checks.append({"case": stem, "passed": passed})
        print(f"[scalar-value-result] {'PASS' if passed else 'FAIL'}: {stem}", flush=True)

    def reject_mutant(stem, document):
        path = work / (stem + ".json")
        path.write_text(json.dumps(document, separators=(",", ":")), encoding="utf-8")
        refused = run(stem, [probe, relative(path)])
        diagnostic = refused.stdout + refused.stderr
        check(stem, refused.returncode == 1 and bool(diagnostic.strip()) and
              b"receiver GraphPlan admitted" not in diagnostic and
              b"empty or unreadable" not in diagnostic)

    if len(sys.argv) != 4:
        built = run("probe-build", [native, "--native-pipeline", "--backend=c", "--opt=dev",
                                    relative(probe_source), "-o", relative(probe)], 120)
        if built.returncode:
            print(built.stdout.decode(errors="replace") + built.stderr.decode(errors="replace"))
            return 1
    for origin in ("native", "public"):
        command = ([native, "--native-pipeline", "--mir-json", relative(source)] if origin == "native"
                   else [driver, "--emit-mir-json-verified", relative(source)])
        admitted = run(origin + ".source-admission", command)
        try:
            document = json.loads(admitted.stdout)
            passed = admitted.returncode == 0 and document.get("schema") == "pgy.mir.v1"
        except (ValueError, AttributeError):
            passed = False
        check(origin + ".source-admission", passed)
        if not passed:
            continue
        mir = work / (origin + ".mir.json")
        mir.write_bytes(admitted.stdout)
        sealed = run(origin + ".shared-admission", [probe, relative(mir)])
        shared_ready = sealed.returncode == 0 and b"receiver GraphPlan admitted" in sealed.stdout
        check(origin + ".shared-admission", shared_ready)
        for backend in ("c", "llvm"):
            stem = origin + "." + backend
            exe = work / (stem + ".exe")
            command = [native, relative(source), "--backend=" + backend, "--opt=dev", "-o", relative(exe)]
            if origin == "native":
                command.append("--native-pipeline")
            built = run(stem + ".compile", command)
            passed = built.returncode == 0 and (origin == "native" or
                     b"[pipeline timing]" not in built.stdout + built.stderr)
            if passed:
                executed = run(stem + ".run", [exe], 10)
                passed = executed.returncode == 0 and not executed.stderr and \
                    executed.stdout.decode().replace("\r\n", "\n") == expected
            check(stem + ".execution", passed)
        for backend in ("c", "llvm"):
            stem = origin + ".shared." + backend
            passed = shared_ready
            if passed:
                projected = run(stem + ".project", [probe, relative(mir), backend])
                passed = projected.returncode == 0
                if passed:
                    code = work / (stem + (".c" if backend == "c" else ".ll"))
                    code.write_bytes(projected.stdout)
                    exe = work / (stem + ".exe")
                    command = (["gcc", "-std=c11", "-O0", "-fwrapv", "-fno-strict-aliasing",
                                relative(code), "-Isrc", "-Isrc/runtime", "-pthread", "-o", relative(exe)]
                               if backend == "c" else ["clang", relative(code), "-o", relative(exe)])
                    built = run(stem + ".compile", command)
                    passed = built.returncode == 0
                    if passed:
                        executed = run(stem + ".run", [exe], 10)
                        passed = executed.returncode == 0 and not executed.stderr and \
                            executed.stdout.decode().replace("\r\n", "\n") == expected
            check(stem + ".execution", passed)
        if origin == "native" and shared_ready:
            # Native formal entry uses must agree with the carried declaration,
            # not merely a same-spelled local or another parameter's SSA name.
            for name, uses in (("wrong-formal-use", ["other.0"]),
                               ("shadow-version-use", ["n.1"]),
                               ("duplicate-entry-use", ["n.0", "n.0"])):
                mutant = copy.deepcopy(document)
                routine = next(r for r in mutant["routines"] if r["name"] == "Touch")
                instruction = next(i for b in routine["blocks"] for i in b["instructions"]
                                   if i["kind"] == "return")
                instruction["uses"] = uses
                reject_mutant("native." + name, mutant)
            mutant = copy.deepcopy(document)
            routine = next(r for r in mutant["routines"] if r["name"] == "TouchInt")
            instruction = next(i for b in routine["blocks"] for i in b["instructions"]
                               if i["kind"] == "return")
            instruction["uses"] = ["n.1"]
            reject_mutant("native.same-type-shadow-use", mutant)
        if not shared_ready:
            continue
        # Both wires carry explicit formal-vs-local identities. Mutants enter
        # only the common admission probe (no target argument), never projection.
        def touch(doc):
            return next(r for r in doc["routines"] if r["name"] == "Touch")

        def shadow(doc):
            return next(i for b in touch(doc)["blocks"] for i in b["instructions"]
                        if i["source_type"] == "AST_LET_DECL")

        mutations = [
            ("missing-primary-ref", lambda d: shadow(d).pop("local_ref")),
            ("missing-expression-refs", lambda d: shadow(d).pop("expr0_local_refs")),
            ("shadow-as-formal", lambda d: shadow(d).__setitem__("local_ref",
                f'parameter:{touch(d)["source_syntax_id"]}:1')),
            ("foreign-formal-owner", lambda d: shadow(d).__setitem__("local_ref", "parameter:987654:0")),
            ("zero-formal-id", lambda d: touch(d)["params"][0].__setitem__("source_syntax_id", 0)),
            ("duplicate-formal-id", lambda d: touch(d)["params"][1].__setitem__("source_syntax_id",
                touch(d)["params"][0]["source_syntax_id"])),
            ("readonly-carriage", lambda d: touch(d)["params"][0].__setitem__("carriage", "readonly-ref")),
            ("owner-carriage", lambda d: touch(d)["params"][0].__setitem__("carriage", "owner-handle")),
            ("indirect-pass", lambda d: touch(d)["params"][0].__setitem__("pass", "indirect")),
            ("required-scalar-layout", lambda d: touch(d)["params"][0].__setitem__("abi_layout_required", True)),
            ("missing-source-local", lambda d: touch(d).__setitem__("source_locals", [])),
        ]
        for name, mutate in mutations:
            mutant = copy.deepcopy(document)
            mutate(mutant)
            reject_mutant(origin + "." + name, mutant)
    failures = sum(not c["passed"] for c in checks)
    hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest()
              for p in (native, driver, source, probe_source, probe, pathlib.Path(__file__))}
    (work / "report.json").write_text(json.dumps({"hashes": hashes, "checks": checks,
                                                "failures": failures}, indent=2), encoding="utf-8")
    print(f"[scalar-value-result] {len(checks)} checks / {failures} failures; evidence: {work}")
    return int(failures != 0)


if __name__ == "__main__":
    raise SystemExit(main())
