#!/usr/bin/env python3
"""Existing C runtime manager: per-operation checks vs scoped pin vs raw load.

This is runtime evidence, not a claim that source Slot<T> has manager-handle ABI.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import statistics
import sys

from raw_owner_pattern_trials import Trial, digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    baseline, output = args.baseline.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    for name in ('logs', 'bin'):
        (output / name).mkdir()
    (output / 'src').symlink_to(baseline / 'src', target_is_directory=True)
    shutil.copytree(Path(__file__).resolve().parents[1] / 'cases/raw_owner_patterns', output / 'fixtures')
    cpu = json.loads((baseline / 'environment.json').read_text())['cpu']
    os.sched_setaffinity(0, {cpu})
    record = {'baseline_manifest': digest(baseline / 'inputs.json'), 'harness': digest(Path(__file__)),
              'process_owner': digest(Path(__file__).with_name('raw_owner_pattern_trials.py')),
              'fixtures': {str(p.relative_to(output)): digest(p) for p in (output / 'fixtures').iterdir() if p.is_file()},
              'source_abi': 'C SlotManager / PgyPinnedView, not canonical Slot<T>', 'cpu': cpu}
    (output / 'inputs.json').write_text(json.dumps(record, indent=2) + '\n')
    trial = Trial(output)
    names = ('slot_manager', 'slot_manager_core_ops', 'slot_manager_storage', 'slot_manager_pin',
             'slot_manager_query_lock', 'slot_manager_secure_ops', 'slot_type_utils', 'slot_pool',
             'slot_pool_linked_list', 'slot_pool_perf', 'slot_security', 'slot_security_fingerprint',
             'slot_security_crypto', 'slot_security_memory', 'slot_security_platform', 'slot_security_sealed_payload',
             'slot_manager_security_stats', 'slot_manager_scope')
    runtime = ['src/runtime/' + name + '.c' for name in names]
    common = ['clang-18', '-g0', '-pthread', '-Isrc/runtime', '-fwrapv', '-fno-strict-aliasing']
    san = ['-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-DPATTERN_OBSERVE',
           '-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free']
    n = 1000000
    env = os.environ | {'PGY_PATTERN_ITERATIONS': str(n), 'ASAN_OPTIONS': 'detect_leaks=1:halt_on_error=1',
                       'UBSAN_OPTIONS': 'halt_on_error=1'}
    value, oracle = 0, 0
    for i in range(n):
        value = (value + i % 17 + 1) % 1024
        oracle = (oracle + value) % 1000003
    rows, release = [], {}
    for config, flags in (('raw-c', ['-DPATTERN_RAW_SLOT']), ('pin-once', ['-DPATTERN_PIN_ONCE']),
                          ('checked-each-access', [])):
        for mode, options in (('san', san), ('release', ['-O2'])):
            name = config + '-' + mode
            command = common + flags + options + ['fixtures/slot_view_runtime.c', 'fixtures/bench_support.c']
            if config != 'raw-c':
                command += runtime
            command += ['-lcrypto', '-lm', '-o', 'bin/' + name]
            status, _, error = trial.run(name + '-build', command)
            if status:
                raise RuntimeError(error[-3500:])
            status, text, error = trial.run(name, [str(output / 'bin' / name)], env)
            actual = json.loads(text) if status == 0 else None
            if actual is None or actual['checksum'] != oracle:
                raise RuntimeError(name + text + error[-3500:])
            if mode == 'san':
                if actual['live_blocks'] or actual['allocations'] != actual['frees']:
                    raise RuntimeError('runtime owner did not retire allocations: ' + text)
                rows.append({'config': config, **actual})
            else:
                release[config] = str(output / 'bin' / name)
        print(config + ': payload, lifetime and reached manager controls passed.', flush=True)
    samples = {key: [] for key in release}
    for _ in range(7):
        for key, executable in release.items():
            status, text, error = trial.run('timing', [executable], env)
            actual = json.loads(text) if status == 0 else None
            if actual is None or actual['checksum'] != oracle:
                raise RuntimeError(key + text + error)
            samples[key].append(actual['elapsed_ns'])
    (output / 'observations.json').write_text(json.dumps(rows, indent=2) + '\n')
    timing = {key: {'iterations': n, 'samples_ns': values, 'median_ns': statistics.median(values),
        'min_ns': min(values), 'max_ns': max(values)} for key, values in samples.items()}
    (output / 'timing.json').write_text(json.dumps(timing, indent=2) + '\n')
    for path, sha in record['fixtures'].items():
        if digest(output / path) != sha:
            raise RuntimeError('frozen slot fixture changed: ' + path)
    print('PASS: existing manager-handle generation, foreign root, lease and stale-handle controls.', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
