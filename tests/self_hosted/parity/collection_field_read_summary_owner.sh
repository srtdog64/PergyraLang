#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here collection-field-read-summary "$PGY"
PYTHON_BIN="${PYTHON_BIN:-python3}"
cd "$ROOT_DIR"
mkdir -p .tmp
WORK="$(mktemp -d .tmp/collection-field-read-summary.XXXXXX)"
PYTHON_PLATFORM="$("$PYTHON_BIN" -c 'import sys; print(sys.platform)')"
PYTHON_ROOT="$ROOT_DIR"
PYTHON_PGY="$PGY"
PYTHON_WORK="$WORK"
if [[ "$PYTHON_PLATFORM" == win32 ]]; then
    PYTHON_ROOT="$(pgy_path_for_windows_tool "$ROOT_DIR")"
    PYTHON_PGY="$(pgy_path_for_windows_tool "$PGY")"
    PYTHON_WORK="$(pgy_path_for_windows_tool "$(cd "$WORK" && pwd)")"
fi
"$PYTHON_BIN" - "$PYTHON_ROOT" "$PYTHON_PGY" "$PYTHON_WORK" <<'PY'
import hashlib, os, pathlib, signal, subprocess, sys, time
root, native, work = map(pathlib.Path, sys.argv[1:])
probe = root / 'tests/self_hosted/fixtures/collection_field_read_summary_probe.pgy'
directory = root / 'tests/self_hosted/parity/fixture/collection_field_lifetime'
cases = {
 'direct_transitive': (6, {'DirectRead': ('rows', ['types','@types']), 'ForwardRead': ('rows',['types','@types']), 'SecondRead': ('rows',['types','@types'])}),
 'nested_path_prefix': (6, {'NestedRead': ('outer',['inner','@inner.types','@~inner']), 'ForwardNested': ('outer',['inner','@inner.types','@~inner'])}),
 'whole_transfer': (4, {'WholeRead': ('rows',['*','!']), 'ForwardWhole': ('rows',['*','!'])}),
 'field_mutation': (6, {'RetainField': ('rows',['types','@types','!']), 'ForwardRetain': ('rows',['types','@types','!'])})}
inputs = [directory / ('field_read_summary_' + name + '.pgy') for name in cases]
negatives = ['exclusive_inout_exit_before_writeback_negative.pgy', 'exclusive_inout_repeat_shared_alias_negative.pgy']
inputs += [directory/name for name in negatives]
def membership():
    return sorted(p for p in (root/'src').rglob('*') if p.is_file() and p.suffix in ('.pgy','.c','.h','.def'))
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
sources = membership()
guard_paths = sources + inputs + [probe, native, pathlib.Path(sys.executable), root/'Makefile', root/'tests/pgy_binary_path_helpers.sh', root/'tests/self_hosted/parity/collection_field_read_summary_owner.sh']
guards = {p: digest(p) for p in guard_paths}
(work/'inputs.sha256').write_text(''.join(f'{h}  {p}\n' for p,h in guards.items()), encoding='utf-8')
start = time.monotonic()
if sys.platform=='win32':
    import ctypes
    from ctypes import wintypes
    # Official Win32 SDK layouts:
    # https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_basic_limit_information
    # https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_extended_limit_information
    # https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-io_counters
    class BasicLimits(ctypes.Structure):
        _fields_=[('PerProcessUserTimeLimit',ctypes.c_int64),('PerJobUserTimeLimit',ctypes.c_int64),
                  ('LimitFlags',wintypes.DWORD),('MinimumWorkingSetSize',ctypes.c_size_t),
                  ('MaximumWorkingSetSize',ctypes.c_size_t),('ActiveProcessLimit',wintypes.DWORD),
                  ('Affinity',ctypes.c_size_t),('PriorityClass',wintypes.DWORD),('SchedulingClass',wintypes.DWORD)]
    class IoCounters(ctypes.Structure):
        _fields_=[(name,ctypes.c_uint64) for name in ('ReadOperationCount','WriteOperationCount',
                    'OtherOperationCount','ReadTransferCount','WriteTransferCount','OtherTransferCount')]
    class ExtendedLimits(ctypes.Structure):
        _fields_=[('BasicLimitInformation',BasicLimits),('IoInfo',IoCounters),
                  ('ProcessMemoryLimit',ctypes.c_size_t),('JobMemoryLimit',ctypes.c_size_t),
                  ('PeakProcessMemoryUsed',ctypes.c_size_t),('PeakJobMemoryUsed',ctypes.c_size_t)]
    kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    kernel.CreateJobObjectW.argtypes=[ctypes.c_void_p,wintypes.LPCWSTR]; kernel.CreateJobObjectW.restype=wintypes.HANDLE
    kernel.SetInformationJobObject.argtypes=[wintypes.HANDLE,ctypes.c_int,ctypes.c_void_p,wintypes.DWORD]; kernel.SetInformationJobObject.restype=wintypes.BOOL
    kernel.AssignProcessToJobObject.argtypes=[wintypes.HANDLE,wintypes.HANDLE]; kernel.AssignProcessToJobObject.restype=wintypes.BOOL
    kernel.TerminateJobObject.argtypes=[wintypes.HANDLE,wintypes.UINT]; kernel.TerminateJobObject.restype=wintypes.BOOL
    kernel.CloseHandle.argtypes=[wintypes.HANDLE]; kernel.CloseHandle.restype=wintypes.BOOL
    class OwnedJob:
        def __init__(self):
            self.handle=kernel.CreateJobObjectW(None,None)
            if not self.handle: raise ctypes.WinError(ctypes.get_last_error())
            limits=ExtendedLimits(); limits.BasicLimitInformation.LimitFlags=0x2000 # KILL_ON_JOB_CLOSE
            if not kernel.SetInformationJobObject(self.handle,9,ctypes.byref(limits),ctypes.sizeof(limits)):
                error=ctypes.WinError(ctypes.get_last_error()); self.close(); raise error
        def assign(self,process):
            if not kernel.AssignProcessToJobObject(self.handle,wintypes.HANDLE(int(process._handle))):
                raise ctypes.WinError(ctypes.get_last_error())
        def terminate(self):
            if not kernel.TerminateJobObject(self.handle,1): raise ctypes.WinError(ctypes.get_last_error())
        def close(self):
            if self.handle:
                handle=self.handle; self.handle=None
                if not kernel.CloseHandle(handle): raise ctypes.WinError(ctypes.get_last_error())
    launcher = "import sys; token=sys.stdin.buffer.read(1); token==b'G' or sys.exit(125); import subprocess; sys.exit(subprocess.Popen(sys.argv[1:],stdin=subprocess.DEVNULL).wait())"
def run(label, args, limit, expected_rc=0, compile_step=False):
    remaining = 300 - (time.monotonic()-start)
    if remaining <= 10: raise RuntimeError('focused 300-second budget exhausted (cleanup reserve)')
    job=OwnedJob() if sys.platform=='win32' else None
    command=[str(x) for x in args]
    try:
        # Isolated/no-site startup prevents cwd shadow imports and startup hooks.
        p = subprocess.Popen(([sys.executable,'-I','-S','-c',launcher]+command) if job else command,
                             cwd=root, stdin=subprocess.PIPE if job else subprocess.DEVNULL,
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                             start_new_session=(sys.platform!='win32'))
    except BaseException:
        if job: job.close()
        raise
    try:
        if job:
            try:
                # Child launch is impossible before assigning this exact process handle.
                job.assign(p)
                p.stdin.write(b'G'); p.stdin.flush(); p.stdin.close(); p.stdin=None
            except BaseException as setup_failure:
                # A failed assignment leaves only the blocked launcher, no children.
                if p.poll() is None: p.terminate()
                try:
                    stdout,stderr=p.communicate(timeout=min(5,max(0.01,300-(time.monotonic()-start))))
                except subprocess.TimeoutExpired as cleanup_failure:
                    raise RuntimeError(label+': blocked launcher cleanup failed') from cleanup_failure
                (work/(label+'.out')).write_bytes(stdout); (work/(label+'.err')).write_bytes(stderr)
                raise RuntimeError(label+': owned Job setup/handshake failed') from setup_failure
        try:
            stdout, stderr = p.communicate(timeout=min(limit,remaining-10))
        except subprocess.TimeoutExpired as failure:
            stdout, stderr = failure.output or b'', failure.stderr or b''
            cleanup_error = None
            try:
                if job:
                    # Handle authority survives the actual parent's or launcher's exit.
                    job.terminate()
                else:
                    try: os.killpg(p.pid, signal.SIGKILL)
                    except ProcessLookupError: pass
                drain_remaining = 300-(time.monotonic()-start)
                if drain_remaining <= 0: raise RuntimeError('no pipe-drain budget remains')
                stdout, stderr = p.communicate(timeout=min(5,drain_remaining))
            except (OSError, RuntimeError, subprocess.TimeoutExpired) as cleanup_failure:
                cleanup_error = str(cleanup_failure)
            finally:
                (work/(label+'.out')).write_bytes(stdout)
                (work/(label+'.err')).write_bytes(stderr)
                (work/(label+'.timeout.log')).write_text(
                    f'timeout launcher_pid={p.pid} launcher_rc={p.poll()} owned_job={job is not None} cleanup_error={cleanup_error}\n',encoding='utf-8')
            if cleanup_error: raise RuntimeError(label+': timeout cleanup failed: '+cleanup_error) from failure
            raise RuntimeError(label+': owned process tree timed out and was terminated') from failure
    finally:
        if job: job.close()
    (work/(label+'.out')).write_bytes(stdout); (work/(label+'.err')).write_bytes(stderr)
    if p.returncode != expected_rc or (stderr and not compile_step): raise RuntimeError(f'{label}: unexpected rc={p.returncode}/stderr')
    return stdout.decode('utf-8').replace('\r\n','\n')
def validate(text, name):
    lines = text.splitlines()
    count, expected = cases[name]
    if lines[:2] != ['BODY|', f'COUNT|{count}'] or sum(x.startswith('BODY|') for x in lines)!=1 or sum(x.startswith('COUNT|') for x in lines)!=1 or any(x.startswith('PROBE|') for x in lines): raise ValueError('body/count')
    keys = [x[4:] for x in lines if x.startswith('KEY|')]
    if len(keys)!=count or len(set(keys))!=count or keys!=sorted(keys): raise ValueError('keys/order')
    formals = {}; actual = []; allowed = {'BODY','COUNT','KEY','FORMAL','READ'}
    for line in lines:
        parts=line.split('|')
        if parts[0] not in allowed: raise ValueError('unknown row')
        if parts[0]=='FORMAL':
            if len(parts)!=5 or not parts[3].isdigit() or not parts[4].isdigit(): raise ValueError('formal shape')
            identity=tuple(parts[1:4])
            if identity in formals: raise ValueError('duplicate formal')
            formals[identity]=parts[4]
        if parts[0]=='READ':
            if len(parts)!=5: raise ValueError('read shape')
            actual.append(tuple(parts[1:]))
    wanted={(fn,param,'0',suffix) for fn,(param,suffixes) in expected.items() for suffix in suffixes}
    if set(formals)!={(fn,param,'0') for fn,(param,_) in expected.items()}: raise ValueError('formal inventory')
    if len(actual)!=count or len(set(actual))!=count or set(actual)!=wanted: raise ValueError('read set')
    projected=[]
    for fn,param,ordinal,suffix in actual:
        identity=(fn,param,ordinal)
        if identity not in formals: raise ValueError('missing formal')
        projected.append(formals[identity]+':'+suffix)
    if len(set(projected))!=count or set(projected)!=set(keys): raise ValueError('key/formal join')
def selftest(text,name):
    lines=text.splitlines(); reads=[i for i,l in enumerate(lines) if l.startswith('READ|')]; keys=[i for i,l in enumerate(lines) if l.startswith('KEY|')]
    mutations=[]
    mutations.append(lines[:reads[0]]+lines[reads[0]+1:])
    mutations.append(lines+[lines[reads[0]]])
    a=lines.copy(); a[keys[0]]='KEY|999999:foreign'; mutations.append(a)
    a=lines.copy(); a[reads[0]]=a[reads[0]].replace('|rows|','|foreign|'); mutations.append(a)
    a=lines.copy(); a[keys[0]],a[keys[1]]=a[keys[1]],a[keys[0]]; mutations.append(a)
    a=lines.copy(); a[1]='COUNT|0'; mutations.append(a)
    mutations.append(lines+['FORMAL|Foreign|rows|0|999999'])
    for ordinal,mutation in enumerate(mutations):
        candidate='\n'.join(mutation)+'\n'; (work/f'validator-negative-{ordinal}.out').write_text(candidate,encoding='utf-8')
        try: validate(candidate,name)
        except ValueError: continue
        raise RuntimeError(f'validator accepted planted fault {ordinal}')
try:
    outputs={}
    for backend in ('c','llvm'):
        executable=work/(backend+'-analyzer.exe')
        run(backend+'-compile',[native,'--native-pipeline',f'--backend={backend}','--opt=dev',probe,'-o',executable],120,compile_step=True)
        guards[executable]=digest(executable)
        for name in cases:
            # Execute the analyzer, never the source input program.
            relative=(directory/('field_read_summary_'+name+'.pgy')).relative_to(root)
            text=run(backend+'-'+name,[executable,relative],30)
            validate(text,name); outputs[(backend,name)]=text
        for name in negatives:
            text=run(backend+'-'+name,[executable,(directory/name).relative_to(root)],30,expected_rc=2)
            if text!='PROBE|body-invalid|borrow_boundary_escape\n': raise RuntimeError(name+': original refusal changed')
    for name in cases:
        if outputs[('c',name)]!=outputs[('llvm',name)]: raise RuntimeError(name+': backend projection mismatch')
    selftest(outputs[('c','direct_transitive')],'direct_transitive')
    (work/'results.log').write_text('PASS: 4 source projections + 2 original source refusals per backend, 7 validator negatives; KEY order and FORMAL/READ joins, not an external syntax-ID oracle or READ order proof\n',encoding='utf-8')
finally:
    drift=[str(p) for p,h in guards.items() if not p.exists() or digest(p)!=h]
    if membership()!=sources: drift.append('source membership drift')
    (work/'end-guards.log').write_text('PASS\n' if not drift else '\n'.join(drift),encoding='utf-8')
    if drift: raise RuntimeError('generation drift: '+','.join(drift))
print('[collection-field-read-summary] supporting fact parity PASS; receipt '+str(work))
PY
