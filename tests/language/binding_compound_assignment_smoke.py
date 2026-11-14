#!/usr/bin/env python3
"""Execute binding permission and compound-place contracts on C and LLVM."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pgy', type=Path)
    parser.add_argument('--route', choices=['native', 'self-host'], default='native')
    parser.add_argument('--driver', type=Path)
    parser.add_argument('--backends', nargs='+', choices=['c', 'llvm'], default=['c', 'llvm'])
    parser.add_argument('--output', type=Path)
    parser.add_argument('--only', nargs='+')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    compiler = (args.pgy or root/'bin/pgy').resolve()
    driver = args.driver.resolve() if args.driver else None
    identities = {str(p): digest(p) for p in [compiler] + ([driver] if driver else [])}
    output = args.output.resolve() if args.output else Path(tempfile.mkdtemp(prefix='pgy-bindings-'))
    output.mkdir(parents=True, exist_ok=True)
    cases = json.loads((root/'tests/language/binding_compound_assignment_cases.json').read_text())
    if args.only:
        unknown = set(args.only) - {c['name'] for c in cases}
        if unknown:
            parser.error('unknown cases: ' + ', '.join(sorted(unknown)))
        cases = [c for c in cases if c['name'] in args.only]
    env = dict(os.environ)
    env.pop('PGY_NATIVE_PIPELINE', None)
    if driver:
        env['PGY_SELF_DRIVER_BIN'] = str(driver)
    results = []
    for case in cases:
        path = output/(case['name']+'.pgy')
        path.write_text(case['source'], encoding='utf-8')
        for backend in args.backends:
            executable = output/(case['name']+'-'+backend+'.exe')
            if executable.exists():
                raise RuntimeError('output already contains an executable: ' + str(executable))
            command = [str(compiler)]
            if args.route == 'native':
                command.append('--native-pipeline')
            command += ['--error-format=json', '--backend='+backend, str(path), '-o', str(executable)]
            row = {'name': case['name'], 'backend': backend, 'command': command,
                   'source_sha256': digest(path), 'pass': False}
            started = time.monotonic()
            try:
                compiled = subprocess.run(command, cwd=root, env=env, capture_output=True, text=True, timeout=40)
                log = compiled.stdout + compiled.stderr
                (output/(case['name']+'-'+backend+'.compile.log')).write_text(log, encoding='utf-8')
                row['compile'] = compiled.returncode
                expected = case['expected']
                if expected.startswith('REFUSE:'):
                    marker = expected.split(':', 1)[1]
                    # A linker/codegen failure or crash is not a semantic refusal.
                    diagnostics = []
                    for line in log.splitlines():
                        if line.startswith('['):
                            try:
                                parsed = json.loads(line)
                            except json.JSONDecodeError:
                                continue
                            if isinstance(parsed, list):
                                diagnostics.extend(parsed)
                    refused = any(isinstance(d, dict) and d.get('severity') == 'error'
                                  and d.get('stage') == 'semantic' for d in diagnostics)
                    row['pass'] = (compiled.returncode == 1 and refused
                                   and marker.lower() in log.lower() and not executable.exists())
                elif compiled.returncode == 0:
                    run = subprocess.run([str(executable)], cwd=root, env=env,
                                         capture_output=True, text=True, timeout=5)
                    row.update(run=run.returncode, stdout=run.stdout, stderr=run.stderr)
                    row['pass'] = (run.returncode != 0 and 'divide-by-zero' in run.stderr
                                   if expected.startswith('PANIC:')
                                   else run.returncode == 0 and run.stdout == expected)
            except subprocess.TimeoutExpired as error:
                row['timeout'] = str(error.cmd)
            row['seconds'] = time.monotonic() - started
            results.append(row)
            print(('PASS' if row['pass'] else 'FAIL') + ' ' + case['name'] + '/' + backend, flush=True)
            (output/'result.json').write_text(json.dumps({'binaries': identities, 'route': args.route,
                                                        'results': results}, indent=2), encoding='utf-8')
    if any(digest(Path(p)) != value for p, value in identities.items()):
        raise RuntimeError('compiler changed while the gate ran')
    passed = sum(r['pass'] for r in results)
    print(f'{passed}/{len(results)} passed; evidence: {output}')
    return 0 if results and passed == len(results) else 1


if __name__ == '__main__':
    raise SystemExit(main())
