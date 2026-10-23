# Project truth

This folder is the single place for the project's current state and its
decisions. Every other document links here and does not restate what is
here. Compiler semantics stay where they are: the SoT registry, the owner
documents and the executable gates. When this folder and an executable
gate disagree, the gate wins and this folder is corrected.

## Layout

| Folder | What goes in | How it changes | Size |
|---|---|---|---|
| `state/` | what is true now: SHAs, the blocker, gate results, measurements | overwrite in place; never append history | each file at most 60 lines |
| `decisions/` | one decision per file, `YYYY-MM-DD_<name>.md` | never rewritten; a later decision names the one it replaces | short |
| `evidence/` | dated records a claim cites (runs, receipts, measurements) | add a new file; never edit an old one | any |
| `lanes/` | one file per agent lane: what it owns and what it may edit | the lane edits only its own file | at most 40 lines |

## Rules

- One fact, one place. If a fact appears in two files, delete it from one
  and link.
- A `state/` line names its revision or date. A value without one is not a
  state.
- Facades hold links, not facts. A facade is
  `docs/current_work_handoff.md`'s top card, a directive's status line, or
  any index page. If a facade needs a fact, add it to `state/` and link it.
- A lane edits its own lane file and the `state/` files whose owner column
  names it. Writing to another lane's file is a handoff, and the user says
  when that happens.
- Large legacy documents (the long handoff archive, the 2026-10-16
  integration directive, completion logs) are frozen evidence. Do not
  append to them. Copy a fact out into `state/` or `decisions/` when it
  becomes current again.

## Files

- [state/main.md](state/main.md): published and local main, last CI
- [state/blocker.md](state/blocker.md): the one current blocker and its
  next step
- [state/gates.md](state/gates.md): the progress gates and their last result
- [state/native_checker.md](state/native_checker.md): the native checker's refusals on the canonical input, by front
- [state/measurements.md](state/measurements.md): memory and time
- [decisions/](decisions/): dated decisions, including the open ones
- [evidence/](evidence/): records that state lines cite
- [lanes/](lanes/): who owns what
