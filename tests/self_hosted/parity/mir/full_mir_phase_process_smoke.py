"""Falsify isolation with OOM, orphan children, timeout and cancellation."""
import importlib.util
import json
import os
from pathlib import Path
import resource
import signal
import subprocess
import sys
import tempfile
import time

owner_path = Path(__file__).with_name('full_mir_phase_process_owner.py')
spec = importlib.util.spec_from_file_location('full_mir_phase_process_owner', owner_path)
owner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(owner)
work = Path(tempfile.mkdtemp(prefix='pgy-full-mir-boundary-'))
original_limit = resource.getrlimit(resource.RLIMIT_AS)
sibling = subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(60)'], start_new_session=True)

def check(name, code, expected, reason, timeout=5):
    receipt = work / (name + '.json')
    status = owner.run_phase([sys.executable, '-c', code], receipt,
                             memory_bytes=64 * 1024**2, timeout_seconds=timeout)
    record = json.loads(receipt.read_text())
    assert status == expected and record['reason'] == reason, record
    assert record['process_group_drained'], record
    assert resource.getrlimit(resource.RLIMIT_AS) == original_limit
    assert sibling.poll() is None, 'unrelated validation process was terminated'
    return receipt

try:
    check('success', 'raise SystemExit(0)', 0, 'completed')
    check('kernel-limits', 'import resource; assert resource.getrlimit(resource.RLIMIT_AS)==(64*1024**2,)*2; assert resource.getrlimit(resource.RLIMIT_CORE)==(0,0)', 0, 'completed')
    check('cancel-at-launch', 'import os,signal,time; os.kill(os.getppid(),signal.SIGTERM); time.sleep(60)', 143, 'interrupted')
    check('memory', 'try:\n bytearray(256 * 1024**2)\nexcept MemoryError:\n raise SystemExit(71)\nraise SystemExit(99)', 71, 'child_failed')
    for name, sleep, expected, reason in [
            ('timeout', True, 124, 'timeout'),
            ('orphan', False, 125, 'residual_children')]:
        pid_file = work / (name + '.pids')
        code = ('import os,subprocess,sys,time\n'
                'child=subprocess.Popen([sys.executable,"-c","import time; time.sleep(60)"])\n'
                f'open({str(pid_file)!r},"w").write(str(os.getpid())+" "+str(child.pid))\n')
        if sleep:
            code += 'time.sleep(60)\n'
        check(name, code, expected, reason, timeout=2)
        assert pid_file.exists(), 'child process did not reach the cancellation boundary'
        assert all(not Path('/proc', pid).exists() for pid in pid_file.read_text().split())

    pid_file = work / 'interrupted.pids'
    receipt = work / 'interrupted.json'
    code = ('import os,subprocess,sys,time\n'
            'child=subprocess.Popen([sys.executable,"-c","import time; time.sleep(60)"])\n'
            f'open({str(pid_file)!r},"w").write(str(os.getpid())+" "+str(child.pid))\n'
            'time.sleep(60)\n')
    with (work / 'interrupted.err').open('wb') as stderr:
        controller = subprocess.Popen([sys.executable, str(owner_path), '--receipt', str(receipt),
                                       '--', sys.executable, '-c', code], stderr=stderr)
        until = time.monotonic() + 5
        while not pid_file.exists() and controller.poll() is None and time.monotonic() < until:
            time.sleep(0.01)
        assert pid_file.exists(), 'interruption control never started'
        controller.send_signal(signal.SIGTERM)
        assert controller.wait(timeout=7) == 143
    record = json.loads(receipt.read_text())
    assert record['reason'] == 'interrupted' and record['process_group_drained'], record
    assert all(not Path('/proc', pid).exists() for pid in pid_file.read_text().split())
    assert sibling.poll() is None
    pid_file = work / 'controller-killed.pid'
    receipt = work / 'controller-killed.json'
    code = f'import os,time; open({str(pid_file)!r},"w").write(str(os.getpid())); time.sleep(60)'
    controller = subprocess.Popen([sys.executable, str(owner_path), '--receipt', str(receipt),
                                   '--', sys.executable, '-c', code])
    until = time.monotonic() + 5
    while not pid_file.exists() and controller.poll() is None and time.monotonic() < until:
        time.sleep(0.01)
    assert pid_file.exists(), 'parent-death control never started'
    child_pid = int(pid_file.read_text())
    controller.kill()
    assert controller.wait(timeout=5) == -signal.SIGKILL
    until = time.monotonic() + 5
    reaped = 0
    while time.monotonic() < until:
        reaped, status = os.waitpid(child_pid, os.WNOHANG)
        if reaped:
            break
        time.sleep(0.01)
    assert reaped == child_pid and os.waitstatus_to_exitcode(status) == -signal.SIGKILL
    assert not Path('/proc', str(child_pid)).exists()
    check('after-failure', 'raise SystemExit(0)', 0, 'completed')
    owner.validate_receipt(work / 'success.json')
    for field, value in [('memory_limit_bytes', owner.MEMORY_BYTES + 1),
                         ('process_group_drained', False), ('exit_code', 124)]:
        changed = json.loads((work / 'success.json').read_text())
        changed[field] = value
        altered = work / ('invalid-' + field + '.json')
        altered.write_text(json.dumps(changed))
        try:
            owner.validate_receipt(altered)
        except ValueError as rejected:
            assert 'incomplete or failed' in str(rejected)
        else:
            raise AssertionError('a changed process boundary was admitted')
    previous = (work / 'success.json').read_bytes()
    try:
        owner.run_phase([sys.executable, '-c', 'raise SystemExit(99)'], work / 'success.json')
    except FileExistsError as rejected:
        assert rejected.filename == str(work / 'success.json')
    else:
        raise AssertionError('an earlier validation receipt was overwritten')
    assert (work / 'success.json').read_bytes() == previous
finally:
    sibling.terminate()
    sibling.wait(timeout=5)
print('[full-mir-process-boundary] PASS: bounded memory, descendant drain, timeout, cancellation, sibling isolation and receipt collision;', work)
