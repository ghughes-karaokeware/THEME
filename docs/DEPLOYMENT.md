# Build and Deployment

## C++ DLL changes

1. Confirm the intended diff.
2. Build `dll/CHTheme.vcxproj` or `dll/ButtonSubclass.vcxproj` as Win32 Release.
3. Require zero new errors and warnings.
4. Confirm CompuHost and relevant test executables are closed.
5. Back up deployed DLLs.
6. Copy the Release DLL to the Clarion accessory bin and applicable test/project folders.
7. Verify all copies by SHA-256.

## Clarion source changes

1. Review the focused `.clw`, `.inc`, or `.tpl` diff.
2. Back up installed copies.
3. Synchronize matching source/include/template files.
4. Regenerate only when template output requires it.
5. Compile Test_CDG and CompuHost V4 Win32 Release.
6. Perform the relevant interactive tests.
7. Record hashes and results in `STATUS.md`, `HASHES.md`, and `CHANGELOG.md`.

## Known live locations

- Clarion accessory bin: `F:\SoftVelocity\Clarion 10\accessory\bin`
- Clarion library source: `F:\SoftVelocity\Clarion 10\accessory\libsrc\win`
- Clarion accessory template: `F:\SoftVelocity\Clarion 10\accessory\template\win`
- Clarion template: `F:\SoftVelocity\Clarion 10\template\win`
- CompuHost V4: `F:\Invicion Software Code\Clarion 10 Projects\CompuHost V4`
- Test_App: `F:\Invicion Software Code\Clarion 10 Projects\Test_App\Test_App`
- Test_CDG: `F:\Invicion Software Code\Clarion 10 Projects\Test_App\Test_CDG`

