# gos-android

Work in progress: an Android (arm64) port of Castlevania: Grimoire of Souls,
built on [GoSRecomp](https://github.com/BLSNCVS/Castlevania-Grimoire-of-Souls-Recomp)
(MIT). Target device for testing: Galaxy Z Fold8 (Snapdragon 8 Elite Gen 5, Adreno 840).

**This repository contains no game code or data and must never contain any.**
You need your own macOS v1.4.2 `.dmg`. Anything extracted or generated from it
stays in `work/` (ignored by git).

## Status

See [docs/PLAN.md](docs/PLAN.md).

## Layout

```
scripts/setup_env.sh    fetches the toolchain (GoSRecomp, Unity Android player, libil2cpp, NDK r19c, 7-Zip 24)
scripts/thin_arm64.py   extracts the arm64 slice of a universal Mach-O
docs/PLAN.md            findings and roadmap
```
