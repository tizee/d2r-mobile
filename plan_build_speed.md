# Plan: faster dev IPA builds

## Measured (last build, 2026-10-16 23:23-23:26, after a 6-file source change)
| Step | Time | What it does |
|---|---|---|
| shell_build.sh: generate | 113 s | rsync tree to a fresh copy, gen_assets, romfs overlay, tables, stubs |
| shell_build.sh: xcodebuild | 17 s | only 11 CompileC + 1 Ld (Xcode incremental cache already works, DerivedData `$O/dd` is kept) |
| d2rm_pack.py | 48 s | UI assets again (10 s), overlay again (22 s), dylib, zip |
| Total | ~3 min | |

Root causes (not Xcode):
1. `shell_build.sh` does `rm -rf "$W"` and re-rsyncs the whole tree every run, so `Gen/assets`
   (has its own input fingerprint, dm/ios/run.sh:22) is lost and regenerated.
2. The romfs overlay (22 s) and UI assets are generated twice: once in the shell, once in pack.
3. The shell path exists for users without Xcode (stubs + dylib). For your own device we have
   Xcode and the game files, so the stub/dylib round trip is pure overhead.

## Change
A. New `dm/ios/tools/dev_ipa.sh` (own-device path, unsigned, for Sideloadly):
   - `DM_GEN_ONLY=1 run.sh build` in the real tree (keeps Gen/assets + DerivedData),
   - `xcodebuild ... CODE_SIGNING_ALLOWED=NO DM_FREE_ACCOUNT=1 build` (module images embedded
     as in run.sh, no stubs),
   - zip `Payload/DmIOS.app` -> `$DM_ROOT/build/D2RMobile-dev.ipa`.
   Target: < 60 s after a small source change.
B. `gen_romfs_overlay.py`: skip when an input fingerprint (RomFS manifest + overlay sources +
   tools) matches the last output (same scheme as gen_assets.py).
C. `shell_build.sh`: `rsync --delete` into the existing tree instead of `rm -rf` (keeps
   Gen/assets). Keeps the release shell path correct, only faster.

## Verify
- Clean run of dev_ipa.sh, then touch one host .c and re-run: log shows 1 CompileC, total < 60 s.
- IPA installs via Sideloadly and boots (boot.txt first line has the build number).
- shell_build.sh + d2rm_pack.py still produce an identical file list (shell_files.tsv diff empty).

## Open questions
- None.
