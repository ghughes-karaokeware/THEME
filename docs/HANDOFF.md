# Continuation Handoff

Start with `STATUS.md`, `DECISIONS.md`, `DEPLOYMENT.md`, and `HASHES.md`.

The current stable DLL hash is recorded in `HASHES.md`. The latest work was Clarion-source-only and did not rebuild `CHTheme.dll`.

Do not reintroduce sheet-driven hiding of ordinary TAB controls. Clarion is authoritative for TAB contents. Native slider overlays remain responsible for following their assigned REGION and Clarion ancestor visibility.

Before each new bug:

1. Verify `git status`.
2. Commit a checkpoint if the tree is not already clean.
3. Diagnose before modifying.
4. Review the focused diff.
5. Build and test proportionally.
6. Update durable documentation and commit the verified result.

