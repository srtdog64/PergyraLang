import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

root = Path(os.environ['PGY_TEST_ROOT']).resolve()
driver = Path(os.environ.get('PGY_SELF_DRIVER_BIN', root / 'bin/pgy-self-driver')).resolve()
native = Path(os.environ.get('PGY_BIN', root / 'bin/pgy')).resolve()
clang = shutil.which(os.environ.get('CLANG_BIN', 'clang'))
if not driver.is_file() or not native.is_file() or not clang:
    raise SystemExit('collection-replacement: native compiler, installed driver and clang are required')
(root / '.tmp/self_hosted').mkdir(exist_ok=True, parents=True)
work = Path(tempfile.mkdtemp(prefix='collection-replacement.', dir=root / '.tmp/self_hosted'))

def run(name, args, *, env=None, timeout=60):
    result = subprocess.run([str(a) for a in args], cwd=root, env=env,
                            capture_output=True, timeout=timeout)
    (work / (name + '.out')).write_bytes(result.stdout)
    (work / (name + '.err')).write_bytes(result.stderr)
    return result

def require(ok, message):
    if not ok:
        raise SystemExit(f'collection-replacement: {message}; evidence={work}')

cases = [
    ('join', 'tests/self_hosted/parity/fixture/loop_local_collection_cleanup_join_shadow_restored.pgy', b''),
    ('exits', 'tests/cases/collection_replacement/owned_string_exits.pgy', b'5\n0\nalpha\nalpha\nalpha\n3\n'),
]
flags = [clang, '-O0', '-g', '-fwrapv', '-fno-strict-aliasing',
         '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-pthread',
         '-I' + str(root / 'src/runtime'), '-I' + str(root / 'src')]
env = os.environ.copy()
env['ASAN_OPTIONS'] = 'detect_leaks=1'
runtime = work / 'runtime.o'
compiled = run('runtime', flags + ['-DPGY_LLVM_ENABLED', '-c', root / 'src/runtime/pgy_runtime_lib.c', '-o', runtime])
require(compiled.returncode == 0, 'LLVM runtime build failed')
summary = {'driver_sha256': hashlib.sha256(driver.read_bytes()).hexdigest(),
           'native_sha256': hashlib.sha256(native.read_bytes()).hexdigest(), 'runs': []}

proof_source = 'tests/self_hosted/parity/fixture/collection_replacement_mode_proof.pgy'
for backend in ['c', 'llvm']:
    exe = work / ('mode-proof.' + backend + '.exe')
    built = run(backend + '.mode-proof.build', [native, '--native-pipeline', proof_source,
                '--backend=' + backend, '--opt=dev', '-o', exe.relative_to(root)])
    require(built.returncode == 0, backend + ' replacement proof did not compile')
    executed = run(backend + '.mode-proof.run', [exe], timeout=10)
    require(executed.returncode == 0 and executed.stderr == b'' and
            executed.stdout == b'true\ntrue\nfalse\nfalse\nfalse\ntrue\ntrue\n',
            backend + ' missing/forged replacement authority was accepted')

mir = work / 'exits.mir.json'
emitted = run('exits.mir', [driver, '--emit-mir-json-verified', cases[1][1], '-o', mir.relative_to(root)])
require(emitted.returncode == 0 and mir.exists(), 'exit-control typed MIR production failed')
original = json.loads(mir.read_text())
mutations = []
for mutation in ['missing-borrowed-owner', 'unsupported-entrypoint']:
    changed = json.loads(json.dumps(original))
    for routine in changed['routines']:
        if mutation == 'missing-borrowed-owner':
            if routine['name'] == 'Repeated':
                require(any(f['origin'] == 'borrowed-literal' for f in routine['collection_ownership_facts']),
                        'exit control has no borrowed-literal evidence')
                routine['collection_ownership_facts'] = []
                routine['collection_ownership_fact_count'] = 0
        elif routine['name'] == 'Main':
            routine['return'] = 'Bool'
    target = work / (mutation + '.mir.json')
    target.write_text(json.dumps(changed, separators=(',', ':')))
    mutations.append((mutation, target))

for backend, extension in [('c', 'c'), ('llvm', 'll')]:
    extra = [] if backend == 'c' else [runtime]
    for name, source, expected in cases:
        stem = name + '.' + backend
        artifact = work / (name + '.' + extension)
        mode = ['--emit-c-artifact-verified', source] if backend == 'c' else [
            '--emit-source-llvm-ir-verified', source, '-o']
        emitted = run(stem + '.emit', [driver, *mode, artifact.relative_to(root)])
        require(emitted.returncode == 0 and artifact.is_file(), stem + ' emission failed')
        exe = work / (stem + '.exe')
        observer = [root / 'tests/cases/collection_replacement/observe_alloc.c',
                    '-Wl,--wrap=malloc,--wrap=realloc,--wrap=free']
        if name == 'exits':
            observer += ['-DPGY_EXPECT_ALLOCATIONS=4', '-DPGY_EXPECT_BYTES=128', '-DPGY_EXPECT_PEAK=2']
        built = run(stem + '.build', flags + [artifact, *extra, *observer, '-lm', '-o', exe])
        require(built.returncode == 0, stem + ' artifact did not compile')
        executed = run(stem + '.run', [exe], env=env, timeout=10)
        require(executed.returncode == 0 and executed.stdout == expected,
                stem + ' runtime output or sanitizer failure')
        expected_memory = b'ownership: allocations=6 frees=6 live=0 freed_bytes=72 peak=3\n' if name == 'join' else b'ownership: allocations=4 frees=4 live=0 freed_bytes=128 peak=2\n'
        require(executed.stderr == expected_memory, stem + ' physical release observation drifted')
        summary['runs'].append({'input': name, 'backend': backend,
                                'artifact_sha256': hashlib.sha256(artifact.read_bytes()).hexdigest(),
                                'memory': executed.stderr.decode().strip()})

        # Mutate emitted artifacts, not compiler source spelling. Removing the
        # actual release calls must let the independent sanitizer find a leak.
        if name == 'join':
            text = artifact.read_text()
            pattern = r'(?m)^\s*pgy_as_drop_owned\(&pgy_local_\d+\);[^\S\n]*$' if backend == 'c' else r'(?m)^\s*call void @pgy_as_drop_owned\(ptr %pgy\.local\.\d+\)[^\S\n]*$'
            suppressed, changed = re.subn(pattern, '', text)
            require(changed > 0, stem + ' has no release for the leak control')
            mutant = work / ('suppressed.' + extension)
            mutant.write_text(suppressed)
            mutant_exe = work / ('suppressed.' + backend + '.exe')
            built = run(stem + '.suppressed.build', flags + [mutant, *extra, '-lm', '-o', mutant_exe])
            require(built.returncode == 0, stem + ' leak control did not compile')
            leaked = run(stem + '.suppressed.run', [mutant_exe], env=env, timeout=10)
            require(leaked.returncode != 0 and b'LeakSanitizer: detected memory leaks' in leaked.stderr,
                    stem + ' sanitizer failed to detect the suppressed cleanup')
        elif backend == 'c':
            text = artifact.read_text()
            pattern = r'(?m)^(    pgy_as pgy_replacement_\d+ = [^\n]+;\n)(    pgy_as_drop_storage\(&pgy_local_\d+\);\n)'
            reordered, changed = re.subn(pattern, r'\2\1', text)
            require(changed > 0, 'no fresh RHS/release pair for the ordering control')
            mutant = work / 'premature.c'
            mutant.write_text(reordered)
            mutant_exe = work / 'premature.c.exe'
            built = run('premature.build', flags + [mutant, *observer, '-lm', '-o', mutant_exe])
            require(built.returncode == 0, 'ordering control did not compile')
            premature = run('premature.run', [mutant_exe], env=env, timeout=10)
            require(premature.returncode != 0 and b'peak=1' in premature.stderr,
                    'physical observer missed cleanup before RHS evaluation')

    refused = work / ('retaining.' + extension)
    source = 'tests/self_hosted/parity/fixture/loop_local_collection_cleanup_join_shadow_invalid.pgy'
    mode = ['--emit-c-artifact-verified', source] if backend == 'c' else ['--emit-source-llvm-ir-verified', source, '-o']
    rejected = run(backend + '.retaining', [driver, *mode, refused.relative_to(root)])
    require(rejected.returncode != 0 and not refused.exists() and
            b'borrow_boundary_escape' in rejected.stdout + rejected.stderr,
            backend + ' retained storage was admitted or failed for another reason')
    unknown = work / ('unknown-replacement.' + extension)
    source = 'tests/cases/collection_replacement/owned_string_rhs.pgy'
    mode = ['--emit-c-artifact-verified', source] if backend == 'c' else ['--emit-source-llvm-ir-verified', source, '-o']
    rejected = run(backend + '.unknown-replacement', [driver, *mode, unknown.relative_to(root)])
    require(rejected.returncode != 0 and not unknown.exists() and
            b'replacement requires its admitted ownership row' in rejected.stdout + rejected.stderr,
            backend + ' guessed ownership for an unproved replacement')
    for mutation, source in mutations:
        artifact = work / (mutation + '.' + extension)
        rejected = run(backend + '.' + mutation,
                       [driver, '--mir-json-backend=' + backend, source.relative_to(root), '-o', artifact.relative_to(root)])
        require(rejected.returncode != 0 and not artifact.exists() and
                (b'CODEGEN ERROR:' in rejected.stdout + rejected.stderr or
                 b'MIR-LOWER ERROR:' in rejected.stdout + rejected.stderr),
                backend + ' accepted ' + mutation)

(work / 'result.json').write_text(json.dumps(summary, indent=2) + '\n')
print(f'[collection-replacement] PASS: installed C/LLVM, RHS-before-release, exits, 6 frees/0 live each, leak controls and retaining refusals; evidence={work}')
