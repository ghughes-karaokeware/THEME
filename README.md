# Clarion Theme Extension

Permanent Codex development repository for the CompuHost V4 modern theme.

## Authoritative areas

- `dll/`: complete Visual C++ projects for `CHTheme` and `ButtonSubclass`.
- `clarion/`: current Clarion wrapper, include, and template sources.
- `docs/`: durable status, decisions, deployment procedure, hashes, and changelog.
- `recovery/`: historical manifests, focused diffs, and staged C++ milestones.

Existing date-based Codex workspaces remain preserved and are not modified by this repository.

## Required workflow

1. Read `docs/STATUS.md` before changing anything.
2. Commit a checkpoint before each focused bug fix.
3. Make and review the smallest source change.
4. Build Win32 Release and record the result.
5. Confirm target executables are closed before replacing DLLs.
6. Hash every deployed copy.
7. Update `STATUS.md`, `HASHES.md`, and `CHANGELOG.md`.
8. Commit the verified result.

