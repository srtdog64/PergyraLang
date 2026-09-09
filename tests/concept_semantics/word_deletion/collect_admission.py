"""Observe both source-to-MIR routes without compiling or running any input.

Usage: python3 collect_admission.py LAUNCHER DRIVER
       python3 collect_admission.py --self-test

This manual census is not a language-equivalence gate. A refusal may happen
during parsing, semantic admission or MIR lowering; matching refusals need not
have the same reason. Runtime observations in run_matrix.py are a separate claim.
Exit 0 means a complete equal-status census, 1 a status difference, 2 an
operational/protocol failure. None of these proves equivalent semantics.
"""
import collections
import datetime
import hashlib
import json
import os
import pathlib
import subprocess
import sys
import tempfile
import time


def classify(status, stdout, stderr, public):
    if public and "[pipeline timing]" in stdout + stderr:
        return "unexpected-native-route"
    try:
        document = json.loads(stdout)
    except (ValueError, TypeError):
        document = None
    has_mir = isinstance(document, dict) and document.get("schema") == "pgy.mir.v1"
    if status == 0:
        return "admitted" if has_mir else "success-without-mir"
    if has_mir:
        return "mir-on-failure"
    if status != 1:
        return "process-failure"
    return "refused" if (stdout + stderr).strip() else "silent-failure"


def self_test():
    mir = '{"schema":"pgy.mir.v1"}'
    controls = [
        ((0, mir, "", False), "admitted"),
        ((0, mir, "[pipeline timing] native", True), "unexpected-native-route"),
        ((1, "", "diagnostic", True), "refused"),
        ((1, "Code: owned_refusal", "", True), "refused"),
        ((0, "", "", False), "success-without-mir"),
        ((0, "not JSON", "", False), "success-without-mir"),
        ((1, mir, "diagnostic", False), "mir-on-failure"),
        ((1, "", "", False), "silent-failure"),
        ((124, "", "timeout", False), "process-failure"),
    ]
    for arguments, expected in controls:
        actual = classify(*arguments)
        if actual != expected:
            raise AssertionError((expected, actual))
    print(f"[word-admission] {len(controls)} observation-classifier controls passed")
    return 0


def main():
    if sys.argv[1:] == ["--self-test"]:
        return self_test()
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    root = pathlib.Path(__file__).resolve().parents[3]
    launcher, driver = (pathlib.Path(arg).resolve() for arg in sys.argv[1:])
    if not launcher.is_file() or not driver.is_file():
        print("[word-admission] both exact compiler binaries are required", file=sys.stderr)
        return 2
    binaries = {"launcher": launcher, "driver": driver}
    hashes = {name: hashlib.sha256(path.read_bytes()).hexdigest()
              for name, path in binaries.items()}
    parent = root / ".tmp/concept_semantics/word_admission"
    parent.mkdir(parents=True, exist_ok=True)
    work = pathlib.Path(tempfile.mkdtemp(prefix="run.", dir=parent))
    env = dict(os.environ, PGY_SELF_DRIVER_BIN=str(driver), PGY_DEBUG_PIPELINE_TIMING="1")
    env.pop("PGY_NATIVE_PIPELINE", None)
    sources = sorted((pathlib.Path(__file__).parent / "cases").glob("*/*.pgy"))
    report = {
        "schema": "pgy.word-admission-observation.v1",
        "observed_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
        "dirty_state": subprocess.check_output(["git", "status", "--porcelain"], cwd=root, text=True),
        "binaries": {name: {"path": str(path), "sha256": hashes[name]}
                     for name, path in binaries.items()},
        "programs_expected": len(sources), "executed_programs": 0, "rows": [],
    }
    deadline = time.monotonic() + 300
    for source in sources:
        row = {"source": source.relative_to(root).as_posix(),
               "sha256": hashlib.sha256(source.read_bytes()).hexdigest()}
        for origin in ("native", "public"):
            command = [str(launcher), "--mir-json", row["source"]]
            if origin == "native":
                command.append("--native-pipeline")
            observation = {"command": command}
            stem = source.parent.name + "__" + source.stem + "." + origin
            remaining = deadline - time.monotonic()
            try:
                if remaining <= 0:
                    observation["status"] = "census-budget-exhausted"
                else:
                    result = subprocess.run(command, cwd=root, env=env, capture_output=True,
                                            timeout=min(30, remaining), text=True,
                                            encoding="utf-8", errors="replace")
                    (work / (stem + ".out")).write_text(result.stdout, encoding="utf-8")
                    (work / (stem + ".err")).write_text(result.stderr, encoding="utf-8")
                    observation.update(exit_code=result.returncode,
                                       status=classify(result.returncode, result.stdout,
                                                       result.stderr, origin == "public"))
                    if observation["status"] != "admitted":
                        observation["diagnostic_tail"] = (result.stdout + result.stderr)[-2000:]
            except subprocess.TimeoutExpired:
                observation["status"] = "timeout"
            except OSError as error:
                observation.update(status="launch-failure", diagnostic_tail=str(error))
            row[origin] = observation
        report["rows"].append(row)
        print(f"[word-admission] {source.parent.name}/{source.name}: "
              f"{row['native']['status']} / {row['public']['status']}", flush=True)
    pairs = collections.Counter(row["native"]["status"] + "/" + row["public"]["status"]
                                for row in report["rows"])
    stable = all(hashlib.sha256(path.read_bytes()).hexdigest() == hashes[name]
                 for name, path in binaries.items())
    operational = sum(observation["status"] not in ("admitted", "refused")
                      for row in report["rows"] for observation in (row["native"], row["public"]))
    differences = sum(row["native"]["status"] != row["public"]["status"] for row in report["rows"])
    report.update(status_pairs=dict(pairs), operational_failures=operational,
                  binaries_unchanged=stable, status_differences=differences)
    (work / "report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"[word-admission] {len(sources)} programs; {differences} status differences; "
          f"{operational} operational failures; {work}", flush=True)
    return 2 if operational or not stable or not sources else int(differences != 0)


if __name__ == "__main__":
    sys.exit(main())
