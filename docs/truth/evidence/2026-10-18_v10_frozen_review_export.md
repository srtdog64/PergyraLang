# V10 immutable review export

Observed 2026-10-18 KST. Root created
`E:\PergyraValidation\readfile-result-20261018-v10` after the
[V10 root replay](2026-10-18_v10_root_replay.md). The previous V1–V9 export
directories were left untouched.

| Artifact | SHA-256 | Verified scope |
|---|---|---|
| `source-manifest.csv` | `06600568b8e1c86ca034a1ea21ec1134e0a6e6b08aeddf8a6ec2ae075a2a7896` | 270 frozen source/context files, all file hashes rechecked |
| `evidence-manifest.sha256` | `b47958dadfae6d8db8f6612e16ac20281b5326727f069a96d41d205d55a4e7a5` | 135 evidence, binary and replay files, `sha256sum -c` passed |
| `binaries/pgy` | `6b5b00baf5f21a28b351ce51273e1cea1f1be1fa9c56e34064465cb251c356d0` | same production native as the root replay |

The unrelated dirty `docs/01_intent_first_design.md` is absent from the source
manifest and remains uncommitted. The frozen source snapshot contains the
state card as it stood before export completion; current state lives in
`docs/truth/state/`. The full canonical program and CI are still red.

The independent `Pergyra 컴파일러 레드팀 검증` chat received the immutable path,
all three hashes, review scope and read-only constraint. Its verdict is
pending; this export alone is not approval or semantic closure.
