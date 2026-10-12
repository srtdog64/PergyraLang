#!/usr/bin/env bash
# Explicit nested Clone parity and production exported Clone/SliceCopy backing.
# Compiled nested SliceCopy and automatic nested retirement are not asserted.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here nested-array-copy "$PGY"
TRANSPORT="$(pgy_select_optional_exe_binary "${NESTED_ARRAY_CLONE_UNIT:-$(dirname "$PGY")/llvm_nested_array_clone_transport_unit}")"
pgy_require_runnable_binary_here nested-array-clone-transport "$TRANSPORT"
cd "$ROOT_DIR"
mkdir -p .tmp
WORK="$(mktemp -d .tmp/nested-array-copy.XXXXXX)"
"${PYTHON_BIN:-python3}" - "$ROOT_DIR" "$PGY" "$TRANSPORT" "$WORK" <<'PY'
import hashlib, os, pathlib, re, shlex, subprocess, sys, time

root, native, transport, work = (pathlib.Path(p).resolve() for p in sys.argv[1:])
unit_source = root / 'tests/nested_array_copy_runtime.c'
source = root / 'tests/concept_semantics/hashmap/nested_array_copy_independence.pgy'
inputs = [unit_source, source, root / 'tests/nested_array_copy_smoke.sh',
          root / 'tests/compiler/llvm_nested_array_clone_transport_unit.c']
sources = sorted(p for p in (root / 'src').rglob('*') if p.suffix in ('.c', '.h', '.def'))
sources.append(root / 'Makefile')
deadline = time.monotonic() + 300

def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def capture(paths, name):
    rows = [(p, digest(p)) for p in paths]
    (work / name).write_text(''.join(f'{h}  {p.as_posix()}\n' for p, h in rows), encoding='utf-8')
    return rows

before = capture(sources, 'source.start.sha256')
input_before = capture(inputs, 'inputs.start.sha256')
native_before = capture([native, transport], 'native.start.sha256')
failures = []

def run(label, command, limit):
    remaining = deadline - time.monotonic()
    if remaining <= 0:
        raise RuntimeError('focused 300-second budget exhausted')
    result = subprocess.run([str(x) for x in command], cwd=root,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            timeout=min(limit, remaining))
    (work / (label + '.out')).write_bytes(result.stdout)
    (work / (label + '.err')).write_bytes(result.stderr)
    (work / (label + '.rc')).write_text(str(result.returncode), encoding='utf-8')
    return result

try:
    result = run('transport-unit', [transport], 30)
    if (result.returncode != 0 or result.stderr or
        result.stdout.decode().replace('\r\n', '\n') != 'LLVM NESTED CLONE TRANSPORT: 286 checks, 0 failures\n'):
        raise RuntimeError('nested Clone transport admission control failed')
    runtime = work / ('runtime.exe' if os.name == 'nt' else 'runtime')
    flags = ['-std=gnu11', '-O2', '-Isrc', '-pthread']
    libraries = ['-lm'] + (['-lws2_32'] if os.name == 'nt' else [])
    cc = shlex.split(os.environ.get('CC', 'gcc'))
    result = run('runtime-compile', cc + flags + [unit_source, '-o', runtime] + libraries, 120)
    if result.returncode != 0 or not runtime.is_file():
        raise RuntimeError('production runtime probe did not compile')
    runtime_before = digest(runtime)
    result = run('runtime-normal', [runtime], 30)
    if result.returncode != 0 or result.stdout.decode().replace('\r\n', '\n') != 'nested array Clone/SliceCopy independent backing: PASS\n' or result.stderr:
        raise RuntimeError('production nested copy pointer/lifetime control failed')
    # The owned panic contract aborts. Accept only platform abort status AND
    # its exact OOM diagnostic, never a bare crash/nonzero/missing output.
    abort_statuses = {-6, 134, 3, -1073740791, 3221226505}
    for family, offsets in [('int', (1, 2)), ('string', (1, 2, 3, 4))]:
        for operation in ('clone', 'slice'):
            for offset in offsets:
                label = f'oom-{family}-{operation}-{offset}'
                result = run(label, [runtime, f'oom-{family}-{operation}', str(offset)], 30)
                message = result.stderr.decode(errors='replace').replace('\r\n', '\n')
                if (result.returncode not in abort_statuses or result.stdout or
                    re.search(r'^\[PGY PANIC\] [^\n]+ class=oom reason=allocation failed\n', message) is None or
                    'injected allocation failure returned a copy' in message):
                    failures.append(label + ': expected owned allocation-failure panic')
    for family in ('int', 'string'):
        for fault in ('null', 'alias', 'source-null'):
            label = f'twin-{fault}-{family}'
            result = run(label, [runtime, label], 30)
            message = result.stderr.decode(errors='replace').replace('\r\n', '\n')
            if (result.returncode not in abort_statuses or result.stdout or
                re.search(r'^\[PGY PANIC\] [^\n]+ class=internal-invariant reason=nested array clone into requires distinct nonnull source and output\n', message) is None or
                'invalid clone-into boundary returned a copy' in message):
                failures.append(label + ': expected owned distinct-output boundary panic')
    expected = '99\n7\n8\nchanged\nalpha\nbeta\n0\n0\n'
    for backend in ('c', 'llvm'):
        executable = work / ('clone-' + backend + ('.exe' if os.name == 'nt' else ''))
        result = run(backend + '-compile', [native, '--native-pipeline', '--backend=' + backend,
                                          '--opt=dev', source, '-o', executable], 90)
        if result.returncode != 0 or not executable.is_file():
            failures.append(backend + ': safe explicit Clone source failed to compile')
            continue
        binary_before = digest(executable)
        result = run(backend + '-run', [executable], 30)
        actual = result.stdout.decode().replace('\r\n', '\n')
        if result.returncode != 0 or actual != expected or result.stderr:
            failures.append(backend + ': explicit Clone independence stdout mismatch')
        if digest(executable) != binary_before:
            failures.append(backend + ': generated executable changed')
    if digest(runtime) != runtime_before:
        failures.append('runtime probe binary changed')
except (RuntimeError, subprocess.TimeoutExpired, OSError) as error:
    failures.append(str(error))
finally:
    after = capture(sources, 'source.end.sha256')
    after_inputs = capture(inputs, 'inputs.end.sha256')
    after_native = capture([native, transport], 'native.end.sha256')
    current_members = sorted(p for p in (root / 'src').rglob('*') if p.suffix in ('.c', '.h', '.def')) + [root / 'Makefile']
    if before != after or input_before != after_inputs or native_before != after_native or sources != current_members:
        failures.append('source/input/native membership or byte guard changed')
if failures:
    raise SystemExit('\n'.join(failures) + '\nretained receipt: ' + str(work))
print('nested-array-copy: Clone/SliceCopy/fixed-pointer twin retirement, 12 OOM and 6 twin-boundary refusals, C/LLVM Clone parity PASS')
print('receipt: ' + str(work))
PY
