#!/usr/bin/env python3
"""Run unchanged owner/View workloads through an installed compiler command.

The C-compiler wrapper supplies only the fixture's foreign benchmark support.
It never edits generated C/IR or changes the driver's optimization policy.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import random
import signal
import statistics
import subprocess
import time


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def buffer_answers():
    answers = []
    for seed in range(251):
        values = [(seed + i * 7) % 1024 for i in range(64)]
        total = 0
        for turn in range(96):
            j = 8 + (turn * 13 + seed) % 48
            values[j] = (values[j] + turn + seed) % 1024
            total += values[j]
        answers.append(total)
    return answers


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--driver', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--route', choices=('native', 'installed'), required=True)
    parser.add_argument('--cases', nargs='+', default=['fixed_owner', 'fixed_view'])
    parser.add_argument('--repetitions', type=int, default=9)
    parser.add_argument('--observe', action='store_true')
    args = parser.parse_args()
    root, driver, out = args.root.resolve(), args.driver.resolve(), args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    for directory in ('logs', 'bin'):
        (out / directory).mkdir()
    fixture = root / 'tests/cases/raw_owner_patterns'
    pinned_paths = [driver, Path(__file__).resolve(),
                    fixture / 'bench_support.c', root / 'tests/ownership/raw_owner_pattern_trials.py']
    pinned_paths += [fixture / (case + '.pgy') for case in args.cases]
    pinned_paths += [fixture / ('raw_' + case + '.c') for case in
                     sorted({'buffer' if case in ('fixed_owner', 'fixed_view') else case
                             for case in args.cases})]
    if args.route == 'installed':
        pinned_paths += [driver.with_name('pgy-self-driver'),
                         driver.with_name('pgy-self-driver.machine-layer-manifest.json')]
    sources = {str(path): digest(path) for path in pinned_paths}
    (out / 'inputs.json').write_text(json.dumps(sources, indent=2) + '\n')

    def verify_inputs():
        changed = [path for path, expected in sources.items() if digest(path) != expected]
        if changed:
            raise RuntimeError('pinned input changed during validation: ' + ', '.join(changed))

    spec = importlib.util.spec_from_file_location('pattern_trials', root / 'tests/ownership/raw_owner_pattern_trials.py')
    patterns = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(patterns)
    cpu = 15 if 15 in os.sched_getaffinity(0) else min(os.sched_getaffinity(0))
    os.sched_setaffinity(0, {cpu})
    commands = []

    def run(name, argv, extra=None, expected_status=0):
        env = os.environ | {'PGY_CC': str(out / 'cc'), 'PGY_LLVM_CLANG': str(out / 'cc'),
                            'PGY_NATIVE_PIPELINE': '1' if args.route == 'native' else '0'}
        if args.route == 'installed':
            env['PGY_SELF_DRIVER_BIN'] = str(driver.with_name('pgy-self-driver'))
        if extra:
            env.update(extra)
        started = time.monotonic()
        process = subprocess.Popen([str(a) for a in argv], cwd=root, env=env,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   start_new_session=True)
        try:
            stdout, stderr = process.communicate(timeout=300)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.communicate()
            raise RuntimeError(f'{name}: 300 second budget exceeded')
        serial = len(commands)
        (out / 'logs' / f'{serial:04d}-{name}.out').write_bytes(stdout)
        (out / 'logs' / f'{serial:04d}-{name}.err').write_bytes(stderr)
        commands.append({'name': name, 'argv': [str(a) for a in argv], 'extra_env': extra,
                         'status': process.returncode, 'seconds': time.monotonic() - started})
        with (out / 'commands.jsonl').open('a') as receipt:
            receipt.write(json.dumps(commands[-1]) + '\n')
        if process.returncode != expected_status:
            raise RuntimeError(f'{name}: {process.returncode}\n'
                               f'{stdout.decode(errors="replace")[-2000:]}\n'
                               f'{stderr.decode(errors="replace")[-2000:]}')
        return stdout.decode()

    common = ['clang-18', '-O3', '-g0', '-fwrapv', '-fno-strict-aliasing', '-pthread']
    support = out / 'support.o'
    run('support', common + (['-DPATTERN_OBSERVE'] if args.observe else []) +
        ['-c', fixture / 'bench_support.c', '-o', support])
    run('support-bitcode', common + (['-DPATTERN_OBSERVE'] if args.observe else []) +
        ['-emit-llvm', '-c', fixture / 'bench_support.c', '-o', out / 'support.bc'])
    wrapper = ('#!/usr/bin/env python3\nimport os, sys\nargs=sys.argv[1:]\n'
               'if not any(x in args for x in ("-c", "-E", "--version", "-v")):\n'
               f'    args += ["-x", "none", {str(support)!r}]\n')
    if args.observe:
        wrapper += ('    args += ["-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free"]\n')
    wrapper += 'os.execvp("clang-18", ["clang-18", *args])\n'
    (out / 'cc').write_text(wrapper)
    (out / 'cc').chmod(0o755)
    binaries, answers, build_failures = {}, {}, []
    for case in args.cases:
        is_buffer = case in ('fixed_owner', 'fixed_view')
        answers[case] = buffer_answers() if is_buffer else patterns.answers(case)
        source = fixture / (case + '.pgy')
        for backend in ('c', 'llvm'):
            binary = out / 'bin' / (case + '-' + backend)
            command = [driver, source, '--backend=' + backend, '--opt=release', '-o', binary]
            if args.route == 'native':
                command.insert(1, '--native-pipeline')
            try:
                run(case + '-' + backend + '-build', command)
            except RuntimeError as error:
                build_failures.append({'case': case, 'backend': backend, 'error': str(error)})
            else:
                binaries[(case, backend)] = binary
        control = 'buffer' if is_buffer else case
        if (control, 'raw') not in binaries:
            raw = fixture / ('raw_' + control + '.c')
            for mode, flags in (('raw', []), ('raw-whole', []),
                                ('raw-native', ['-march=native'])):
                binary = out / 'bin' / (control + '-' + mode)
                inputs = [raw, fixture / 'bench_support.c']
                if mode != 'raw':
                    bc = out / (control + '-' + mode + '.bc')
                    linked = out / (control + '-' + mode + '-linked.bc')
                    closed = out / (control + '-' + mode + '-closed.bc')
                    run('raw-bitcode', common + flags + ['-emit-llvm', '-c', raw, '-o', bc])
                    run('raw-link', ['llvm-link-18', bc, out / 'support.bc', '-o', linked])
                    public = 'main,__wrap_malloc,__wrap_calloc,__wrap_realloc,__wrap_free'
                    run('raw-visibility', ['opt-18', '-passes=internalize',
                        '-internalize-public-api-list=' + public, linked, '-o', closed])
                    inputs = [closed]
                run(control + '-' + mode + '-build', common + flags + inputs +
                    (['-DPATTERN_OBSERVE', '-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free']
                     if args.observe else []) + ['-lm', '-o', binary])
                binaries[(control, mode)] = binary
        print(case + ': ' + str(sum((case, backend) in binaries for backend in ('c', 'llvm'))) +
              '/2 actual driver binaries built', flush=True)
    verify_inputs()
    (out / 'admission.json').write_text(json.dumps({'route': args.route,
        'accepted': [{'case': case, 'backend': mode, 'binary_sha256': digest(binary)}
                     for (case, mode), binary in binaries.items() if mode in ('c', 'llvm')],
        'failures': build_failures}, indent=2) + '\n')
    if build_failures:
        raise RuntimeError(f'{len(build_failures)} driver admission failures; see {out / "admission.json"}')
    if 'buffer' in {case for case, _ in binaries}:
        answers['buffer'] = buffer_answers()

    def check(case, text, count, seed=0):
        value = json.loads(text)
        if case in ('buffer', 'fixed_owner', 'fixed_view'):
            expected = (sum(answers[case]) * (count // 251) + sum(answers[case][:count % 251])) % 1000003
        else:
            expected = sum(answers[case][(seed + i) % 251] for i in range(count)) % 1000003
        assert value['checksum'] == expected, (case, value, expected)
        return value

    preflight = []
    for (case, mode), binary in binaries.items():
        for seed in range(251):
            is_buffer = case in ('buffer', 'fixed_owner', 'fixed_view')
            count = seed + 1 if is_buffer else 1
            check(case, run('oracle', [binary], {'PGY_PATTERN_ITERATIONS': str(count),
                  'PGY_PATTERN_SEED': str(0 if is_buffer else seed)}), count, seed)
        observed = check(case, run('retirement', [binary],
                         {'PGY_PATTERN_ITERATIONS': '503', 'PGY_PATTERN_SEED': '0'}), 503)
        if args.observe:
            assert observed['allocations'] == observed['frees'] and observed['live_blocks'] == 0, (case, mode, observed)
        preflight.append({'case': case, 'mode': mode, 'binary_sha256': digest(binary), 'observation': observed})
    print('all answers and requested retirement observations PASS', flush=True)
    timing, over_budget = {}, []
    if not args.observe:
        order = list(binaries)
        rng = random.Random(7212607)
        samples = {key: [] for key in order}
        for repetition in range(args.repetitions):
            rng.shuffle(order)
            for case, mode in order:
                count = 200000 if case in ('buffer', 'fixed_owner', 'fixed_view') else 120000
                value = check(case, run('timing', [binaries[(case, mode)]],
                              {'PGY_PATTERN_ITERATIONS': str(count), 'PGY_PATTERN_SEED': '0'}), count)
                samples[(case, mode)].append(value['elapsed_ns'])
            print(f'timing {repetition + 1}/{args.repetitions} PASS', flush=True)
        timing = {case + '/' + mode: {'median_ns': statistics.median(rows), 'samples_ns': rows}
                  for (case, mode), rows in samples.items()}
        for case in args.cases:
            control = 'buffer' if case in ('fixed_owner', 'fixed_view') else case
            fastest = min(timing[control + '/' + mode]['median_ns'] for mode in ('raw', 'raw-whole', 'raw-native'))
            for mode in ('c', 'llvm'):
                ratio = timing[case + '/' + mode]['median_ns'] / fastest
                if ratio > 1.3:
                    over_budget.append(case + '/' + mode)
                print(case, mode, 'ratio', ratio, 'PASS_1.3', ratio <= 1.3, flush=True)
    verify_inputs()
    (out / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
    (out / 'results.json').write_text(json.dumps({'cpu': cpu, 'route': args.route, 'observe': args.observe,
        'preflight': preflight, 'timing': timing}, indent=2) + '\n')
    if over_budget:
        raise RuntimeError('1.3x performance gate failed: ' + ', '.join(over_budget))


if __name__ == '__main__':
    import resource
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    main()
