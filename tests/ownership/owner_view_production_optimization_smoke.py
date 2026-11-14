#!/usr/bin/env python3
"""Behavioral controls for production owner/View optimization boundaries."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--driver', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--route', choices=('native', 'installed'), required=True)
    args = parser.parse_args()
    root, driver, out = args.root.resolve(), args.driver.resolve(), args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    env = os.environ | {'PGY_CC': 'clang-18', 'PGY_LLVM_CLANG': 'clang-18',
                        'PGY_NATIVE_PIPELINE': '1' if args.route == 'native' else '0'}
    if args.route == 'installed':
        env['PGY_SELF_DRIVER_BIN'] = str(driver.with_name('pgy-self-driver'))
    rows = []

    def run(label, argv):
        process = subprocess.Popen([str(x) for x in argv], cwd=root, env=env,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   start_new_session=True)
        try:
            stdout, stderr = process.communicate(timeout=60)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.communicate()
            raise RuntimeError(label + ': timeout')
        (out / (label + '.out')).write_bytes(stdout)
        (out / (label + '.err')).write_bytes(stderr)
        return process.returncode, stdout.decode().replace('\r\n', '\n'), stderr.decode()

    patterns = 'tests/cases/raw_owner_patterns/'
    positives = {
        patterns + 'constant_literal_lifetime.pgy': '14\n5\n7\n8\n15\n6\n2147483647\n',
        patterns + 'proven_divisor.pgy': '11\n22\n2\n516\n-4\n1\n',
        'tests/cases/function_param_flow_summary/array_result_owned_clone_deepdrop_valid.pgy': '1\n',
        'tests/cases/raw_view_ownership/scalar_arrays.pgy': '3\n3\n3.500000\n3.500000\ntrue\n',
        'tests/cases/backend_compare/namespace_export_import/main.pgy': '7\n',
    }
    panics = {
        patterns + 'derived_divide_zero.pgy': 'class=divide-by-zero',
        patterns + 'derived_divide_overflow.pgy': 'class=arithmetic-overflow',
    }
    for source, expected in (positives | panics).items():
        for backend in ('c', 'llvm'):
            for profile in ('dev', 'release'):
                label = Path(source).stem + '-' + backend + '-' + profile
                binary = out / label
                argv = [driver, source, '--backend=' + backend, '--opt=' + profile, '-o', binary]
                if args.route == 'native':
                    argv.insert(1, '--native-pipeline')
                status, _, stderr = run(label + '-compile', argv)
                assert status == 0, (label, 'compile', status, stderr)
                status, stdout, stderr = run(label + '-run', [binary])
                if source in panics:
                    assert status != 0 and expected in stderr, (label, status, stderr)
                else:
                    assert status == 0 and stdout == expected and not stderr, (label, status, stdout, stderr)
                rows.append({'case': source, 'backend': backend, 'profile': profile,
                             'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest()})
    for source in (patterns + 'live_view_growth_negative.pgy',
                   'tests/cases/raw_view_ownership/release_before_read_invalid.pgy'):
        label = Path(source).stem
        argv = [driver, source, '--backend=c', '-o', out / label]
        if args.route == 'native':
            argv.insert(1, '--native-pipeline')
        status, _, stderr = run(label, argv)
        assert status != 0 and not (out / label).exists(), (label, status, stderr)
        assert 'borrow' in stderr.lower() or 'released' in stderr.lower(), (label, stderr)
        rows.append({'case': source, 'refused': True})
    inputs = {str(driver): hashlib.sha256(driver.read_bytes()).hexdigest()}
    for row in rows:
        source = root / row['case']
        inputs[str(source)] = hashlib.sha256(source.read_bytes()).hexdigest()
    (out / 'receipt.json').write_text(json.dumps({'route': args.route, 'inputs': inputs, 'checks': rows}, indent=2) + '\n')
    print(f'owner/View behavior: {len(rows)} checks PASS ({args.route})')


if __name__ == '__main__':
    import resource
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    main()
