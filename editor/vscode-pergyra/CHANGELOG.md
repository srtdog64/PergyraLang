# Changelog

## 0.3.1 (2026-07-23)

- Resolve workspace-relative compiler paths on Windows
- Prepend discovered MSYS2/LLVM runtime directories when launching `pgy.exe`
- Launch native executables without a command shell so paths remain stable
- Use the supported C backend flag for the editor build command

## 0.3.0 (2026-04-06)

- Run button (play icon) for `.pgy` files
- `Ctrl+Shift+R` keybinding to run current file
- Build command (`Pergyra: Build`)
- Auto-detect compiler from workspace `bin/pgy` or PATH
- Bracket auto-closing and comment toggle
- New keywords: `tobject`, `roster`, `intent`, `step`
- Snippets for all 16+ declaration types with structured comments
- Roster snippet for party container declarations

## 0.2.0 (2026-04-06)

- Run button and build command
- Language configuration (brackets, comments, indentation)
- Snippets for all declaration types

## 0.1.0 (2026-04-05)

- Initial release
- Basic syntax highlighting for Pergyra keywords
- File icon for `.pgy` files
