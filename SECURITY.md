# Security Policy

## Supported Versions

Pergyra is pre-1.0. Only the `main` branch is currently receiving security updates.
Once a stable beta is tagged, this table will list the version lines that receive backports.

| Version | Supported          |
| ------- | ------------------ |
| `main`  | :white_check_mark: |
| < 0.9   | :x:                |

## Reporting a Vulnerability

If you believe you have found a security vulnerability in Pergyra (the compiler, runtime, or standard library), please report it privately — do not open a public GitHub issue.

**Preferred channel:**
- Email: `doogwoo@gmail.com` with subject line starting with `[pergyra-security]`

**Please include:**
- A description of the vulnerability
- Steps to reproduce (ideally a minimal `.pgy` file or C test case)
- Affected commit SHA or version
- Impact assessment (RCE? memory disclosure? denial of service? type-safety breach?)

**What to expect:**
- Acknowledgment within **7 days**
- Initial triage and severity classification within **14 days**
- For confirmed vulnerabilities: a fix plan with a target disclosure window

## Scope

### In scope
- **Compiler**: stack/heap memory safety, command injection, path traversal in the build driver
- **Runtime** (`src/runtime/`): memory unsafety in slot/channel/allocator code, fiber scheduler races, buffer overflows in string/collection runtime
- **Generated code**: codegen emitting invalid C/LLVM IR that compiles to an exploitable binary
- **Standard library** (`stdlib/`): I/O, `storage`, `http` modules that can be coerced into unsafe behavior
- **Secure slots / authority / token** system: integrity or authentication bypass in `SecureSlot`, `Token<T>`, capability forwarding

### Out of scope
- Vulnerabilities requiring attacker-controlled compiler flags (e.g. supplying malicious `PGY_CC`)
- Issues only affecting experimental surfaces (marked `experimental` or `v2`) — quantum surface, WASM target, in-progress DAG evaluator
- Third-party dependencies (LLVM, libc, pthread) unless Pergyra explicitly mis-uses them
- Denial of service by submitting extremely large or deeply nested `.pgy` source files
- Social engineering, local-physical attacks

## Security Practices

- **No shell invocation**: the compiler driver uses `_spawnvp` (Windows) / `execvp` (POSIX) with argv arrays, never `system()`. Command-injection-proof by construction.
- **Path safety**: all user-supplied paths go through `pgy_path_is_safe` (`src/compiler/path_utils.c`) before being handed to the platform exec APIs.
- **Fail closed**: `semantic_error` on contract violations; `Unwrap` on failed `Result`/`Option` aborts with structured provenance rather than returning undefined values.
- **AES-256 real implementation**: `SecureSlot` capability tokens use FIPS 197 AES-256-CTR + HMAC-SHA256, not a placeholder XOR scrambler.
- **No unsafe FFI by default**: `extern` is parsed but constrained; there is no `unsafe` block that bypasses boundary checks without explicit opt-in.

## Advisory Format

Confirmed advisories will be published at:
- `docs/security-advisories/` in this repository
- The `CHANGELOG.md` entry for the release that contains the fix

Advisories follow the [GitHub Security Advisory](https://docs.github.com/en/code-security/security-advisories) schema where possible and will include:
- CVE ID (if assigned)
- CVSS v3.1 score
- Affected versions
- Patched versions
- Reporter credit (with permission)
- Mitigation for users who cannot upgrade immediately
