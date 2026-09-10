"""Execute both routes and report programs whose observable results differ.

Usage: python3 collect_runtime_divergence.py LAUNCHER DRIVER
       python3 collect_runtime_divergence.py --self-test

This is the executing complement of collect_admission.py. That census stops at
source-to-MIR admission, so a program both routes admit is counted as agreement
even when the two generated programs print different values. This tool builds
and runs each program on both routes and compares the observable result.

It is a manual observation, not a language-equivalence gate. Equal stdout does
not establish equal semantics: it does not compare diagnostics, effects,
allocation, failure phase order or timing, and one input per shape is not a
proof about the shape. A divergence, however, is a positive finding: the same
source produced different observable behaviour through the two production
routes.

Neither route is treated as the oracle here. Recording which one is correct
belongs to the owning semantic contract, not to this collector.

Exit 0 means every program that both routes built produced the same observable
result, 1 means at least one such program diverged, 2 an operational failure.
"""
import collections
import datetime
import hashlib
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import time

BUILD_TIMEOUT = int(os.environ.get("PGY_WORD_BUILD_TIMEOUT", "300"))
RUN_TIMEOUT = int(os.environ.get("PGY_WORD_RUN_TIMEOUT", "60"))
TOTAL_BUDGET = int(os.environ.get("PGY_WORD_TOTAL_BUDGET", "5400"))


def classify_pair(native, public):
    """Bucket one program from its two route observations."""
    statuses = (native["status"], public["status"])
    if any(s not in ("built", "refused") for s in statuses):
        return "operational"
    if statuses == ("refused", "refused"):
        return "agree-refuse"
    if statuses != ("built", "built"):
        return "build-differs"
    same = (native["stdout"] == public["stdout"]
            and native["run_exit"] == public["run_exit"])
    return "agree-run" if same else "diverge-run"


def self_test():
    def built(stdout, run_exit=0):
        return {"status": "built", "stdout": stdout, "run_exit": run_exit}

    refused = {"status": "refused"}
    controls = [
        ((built("90"), built("90")), "agree-run"),
        ((built("90"), built("100")), "diverge-run"),
        ((built("ok", 0), built("ok", 1)), "diverge-run"),
        ((refused, refused), "agree-refuse"),
        ((built("x"), refused), "build-differs"),
        ((refused, built("x")), "build-differs"),
        ((({"status": "timeout"}), refused), "operational"),
        ((built("x"), {"status": "run-failure"}), "operational"),
    ]
    for arguments, expected in controls:
        actual = classify_pair(*arguments)
        if actual != expected:
            raise AssertionError((expected, actual))
    print(f"[word-runtime] {len(controls)} pair-classifier controls passed")
    return 0


def observe(launcher, source_relative, work, stem, root, env, native, deadline):
    """Build one program on one route, then run it if the build published one."""
    executable = work / (stem + (".exe" if os.name == "nt" else ""))
    command = [str(launcher)]
    if native:
        command.append("--native-pipeline")
    command += [source_relative, "--backend=c", "-o", str(executable)]
    observation = {"command": command}
    remaining = deadline - time.monotonic()
    if remaining <= 0:
        observation["status"] = "budget-exhausted"
        return observation
    try:
        build = subprocess.run(command, cwd=root, env=env, capture_output=True,
                               timeout=min(BUILD_TIMEOUT, remaining), text=True,
                               encoding="utf-8", errors="replace")
    except subprocess.TimeoutExpired:
        observation["status"] = "build-timeout"
        return observation
    except OSError as error:
        observation.update(status="launch-failure", diagnostic_tail=str(error))
        return observation
    observation["build_exit"] = build.returncode
    if build.returncode != 0 or not executable.is_file():
        if build.returncode == 0:
            observation["status"] = "success-without-artifact"
        else:
            observation["status"] = "refused"
        observation["diagnostic_tail"] = (build.stdout + build.stderr)[-2000:]
        return observation
    try:
        run = subprocess.run([str(executable)], cwd=work, capture_output=True,
                             timeout=RUN_TIMEOUT, text=True,
                             encoding="utf-8", errors="replace")
    except subprocess.TimeoutExpired:
        observation["status"] = "run-timeout"
        return observation
    except OSError as error:
        observation.update(status="run-failure", diagnostic_tail=str(error))
        return observation
    observation.update(status="built", run_exit=run.returncode,
                       stdout=run.stdout.replace("\r\n", "\n").strip(),
                       stderr=run.stderr.replace("\r\n", "\n").strip()[:500])
    return observation


def main():
    if sys.argv[1:] == ["--self-test"]:
        return self_test()
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    root = pathlib.Path(__file__).resolve().parents[3]
    launcher, driver = (pathlib.Path(arg).resolve() for arg in sys.argv[1:])
    if not launcher.is_file() or not driver.is_file():
        print("[word-runtime] both exact compiler binaries are required", file=sys.stderr)
        return 2
    binaries = {"launcher": launcher, "driver": driver}
    hashes = {name: hashlib.sha256(path.read_bytes()).hexdigest()
              for name, path in binaries.items()}
    parent = root / ".tmp/concept_semantics/word_runtime"
    parent.mkdir(parents=True, exist_ok=True)
    work = pathlib.Path(tempfile.mkdtemp(prefix="run.", dir=parent))
    env = dict(os.environ, PGY_SELF_DRIVER_BIN=str(driver))
    env.pop("PGY_NATIVE_PIPELINE", None)
    sources = sorted((pathlib.Path(__file__).parent / "cases").glob("*/*.pgy"))
    report = {
        "schema": "pgy.word-runtime-observation.v1",
        "observed_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
        "dirty_state": subprocess.check_output(["git", "status", "--porcelain"], cwd=root, text=True),
        "binaries": {name: {"path": str(path), "sha256": hashes[name]}
                     for name, path in binaries.items()},
        "timeouts": {"build_s": BUILD_TIMEOUT, "run_s": RUN_TIMEOUT, "total_s": TOTAL_BUDGET},
        "programs_expected": len(sources), "rows": [],
    }
    deadline = time.monotonic() + TOTAL_BUDGET
    for source in sources:
        relative = source.relative_to(root).as_posix()
        row = {"source": relative,
               "sha256": hashlib.sha256(source.read_bytes()).hexdigest()}
        for origin in (True, False):
            name = "native" if origin else "public"
            stem = source.parent.name + "__" + source.stem + "." + name
            row[name] = observe(launcher, relative, work, stem, root, env, origin, deadline)
        row["verdict"] = classify_pair(row["native"], row["public"])
        report["rows"].append(row)
        detail = ""
        if row["verdict"] == "diverge-run":
            detail = (f"  native={row['native']['stdout']!r}"
                      f" public={row['public']['stdout']!r}")
        print(f"[word-runtime] {source.parent.name}/{source.name}: "
              f"{row['verdict']}{detail}", flush=True)
        for name in ("native", "public"):
            executable = work / (source.parent.name + "__" + source.stem + "." + name
                                 + (".exe" if os.name == "nt" else ""))
            if executable.is_file():
                executable.unlink()
    verdicts = collections.Counter(row["verdict"] for row in report["rows"])
    stable = all(hashlib.sha256(path.read_bytes()).hexdigest() == hashes[name]
                 for name, path in binaries.items())
    diverged = [row["source"] for row in report["rows"] if row["verdict"] == "diverge-run"]
    operational = verdicts.get("operational", 0)
    report.update(verdicts=dict(verdicts), diverging_sources=diverged,
                  operational_failures=operational, binaries_unchanged=stable)
    (work / "report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    for source in diverged:
        print(f"[word-runtime] DIVERGENCE {source}", flush=True)
    print(f"[word-runtime] {len(sources)} programs; {len(diverged)} runtime divergences; "
          f"{operational} operational failures; {work}", flush=True)
    if operational or not stable or not sources:
        return 2
    return int(bool(diverged))


if __name__ == "__main__":
    sys.exit(main())
