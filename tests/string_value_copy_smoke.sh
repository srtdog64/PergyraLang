#!/usr/bin/env bash
# String leaf runtime acceptance only: no compiler normalization/automatic-drop
# or Option/liveness permission is inferred from these physical operations.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
cd "$ROOT_DIR"
mkdir -p .tmp
WORK="$(mktemp -d .tmp/string-value-copy.XXXXXX)"
"${PYTHON_BIN:-python3}" - "$ROOT_DIR" "$WORK" <<'PY'
import hashlib, os, pathlib, re, shlex, subprocess, sys, time

root, work = (pathlib.Path(p).resolve() for p in sys.argv[1:])
source = root / 'tests/string_value_copy_runtime.c'
inputs = [source, root / 'tests/string_value_copy_smoke.sh', root / 'tests/pgy_binary_path_helpers.sh']
runtime = sorted(p for p in (root / 'src/runtime').rglob('*') if p.suffix in ('.c', '.h', '.def'))
deadline = time.monotonic() + 300

def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def capture(paths, name):
    rows = [(p, digest(p)) for p in paths]
    (work / name).write_text(''.join(f'{h}  {p.as_posix()}\n' for p, h in rows), encoding='utf-8')
    return rows
def run(label, command, budget):
    remaining = deadline - time.monotonic()
    if remaining <= 0: raise RuntimeError('focused 300-second budget exhausted')
    result = subprocess.run([str(p) for p in command], cwd=root, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, timeout=min(budget, remaining))
    (work / (label + '.out')).write_bytes(result.stdout)
    (work / (label + '.err')).write_bytes(result.stderr)
    (work / (label + '.rc')).write_text(str(result.returncode), encoding='utf-8')
    return result

before = capture(runtime, 'runtime.start.sha256')
input_before = capture(inputs, 'inputs.start.sha256')
failures = []
try:
    cc = shlex.split(os.environ.get('CC', 'gcc'))
    flags = ['-std=gnu11', '-O2', '-Isrc']
    libraries = ['-pthread', '-lm'] + (['-lws2_32'] if os.name == 'nt' else [])
    suffix = '.exe' if os.name == 'nt' else ''
    inline = work / ('inline' + suffix)
    linked = work / ('linked' + suffix)
    cext = work / ('cext' + suffix)
    body, caller, cext_body = work / 'body.o', work / 'caller.o', work / 'cext-body.o'
    commands = [
        ('inline-compile', cc + flags + [source, '-o', inline] + libraries),
        ('body-compile', cc + flags + ['-DSTRING_VALUE_TEST_BODY', '-c', source, '-o', body]),
        ('caller-compile', cc + flags + ['-DSTRING_VALUE_TEST_CALLER', '-c', source, '-o', caller]),
        ('external-link', cc + [caller, body, '-o', linked] + libraries),
        ('cext-body-compile', cc + flags + ['-DSTRING_VALUE_TEST_CEXT_BODY', '-c', source, '-o', cext_body]),
        ('cext-external-link', cc + [caller, cext_body, '-o', cext] + libraries),
    ]
    for label, command in commands:
        result = run(label, command, 120)
        if result.returncode != 0: raise RuntimeError(label + ' failed')
    binaries = capture([inline, linked, cext, body, caller, cext_body], 'binary.start.sha256')
    # abort is the existing runtime panic contract, not arbitrary nonzero/crash.
    abort_statuses = {-6, 134, 3, -1073740791, 3221226505}
    faults = {
        'null-copy': ('internal-invariant', 'String result copy requires a nonnull source'),
        'null-retire': ('internal-invariant', 'String result retirement requires a nonnull owner'),
        'oom-copy': ('oom', 'allocation failed'),
    }
    for name, executable in [('inline', inline), ('linked', linked), ('cext', cext)]:
        result = run(name + '-normal', [executable], 30)
        actual = result.stdout.decode().replace('\r\n', '\n')
        if result.returncode != 0 or actual != 'String leaf independent system copies and exact retirement: PASS\n' or result.stderr:
            failures.append(name + ': independent copy/retirement control failed')
        for fault, (panic_class, reason) in faults.items():
            result = run(name + '-' + fault, [executable, fault], 30)
            error = result.stderr.decode(errors='replace').replace('\r\n', '\n')
            diagnostic = r'^\[PGY PANIC\] [^\n]+ class=' + re.escape(panic_class) + ' reason=' + re.escape(reason) + r'\n'
            if (result.returncode not in abort_statuses or result.stdout or re.search(diagnostic, error) is None
                or 'String leaf failure unexpectedly returned' in error):
                failures.append(name + '/' + fault + ': exact owned panic was not observed')
    if binaries != capture([inline, linked, cext, body, caller, cext_body], 'binary.end.sha256'):
        failures.append('binary/object byte guard changed')
except (RuntimeError, subprocess.TimeoutExpired, OSError) as error:
    failures.append(str(error))
finally:
    after = capture(runtime, 'runtime.end.sha256')
    after_inputs = capture(inputs, 'inputs.end.sha256')
    current = sorted(p for p in (root / 'src/runtime').rglob('*') if p.suffix in ('.c', '.h', '.def'))
    if before != after or input_before != after_inputs or runtime != current:
        failures.append('runtime/input bytes or membership changed')
if failures:
    raise SystemExit('\n'.join(failures) + '\nretained receipt: ' + str(work))
print('String leaf copy/retire: inline + linked LLVM runtime + linked C extern runtime, independent literal/empty/system/region copies, exact retirement and nine panic controls PASS')
print('receipt: ' + str(work))
PY
