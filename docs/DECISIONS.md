# Decisions

## Visibility ownership

- Clarion owns SHEET selection and ordinary TAB-content visibility.
- `CHModernSheet` may synchronize the native strip highlight and Clarion selection, but must not manually hide or show ordinary TAB contents.
- Native Win32 slider overlays are window children and therefore must evaluate their assigned REGION and relevant Clarion ancestors.

## Initial selection

- A zero `SHEET{PROP:ChoiceFEQ}` remains untouched.
- Application-restored selections are authoritative and must be mirrored to the custom strip.
- Only a genuine custom-strip notification may drive Clarion selection from the strip.

## Recovery

- Conversation history is not the authoritative project state.
- Git commits plus the files in `docs/` are the durable continuation record.
- Date-based Codex workspaces and recovery packages remain historical evidence.

