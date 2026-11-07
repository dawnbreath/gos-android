# Plan and findings

## Approach

Reuse GoSRecomp's translator: the arm64 game code becomes C++, which is compiled
with the Android NDK for arm64 and linked with libil2cpp built from Unity
2020.3.30f1's sources into `libil2cpp.so`, loaded by Unity's own Android player
(`libunity.so`). Running the original arm64 Mach-O directly was rejected: the
libil2cpp inside it depends on macOS (mach thread APIs in the GC, Apple compact
unwind for exceptions).

## Findings (macOS v1.4.2)

- `GameAssembly.dylib` is universal (x86_64 + arm64); the arm64 slice is used.
- Registration tables: 136,275 methods (upstream `registration_tables.py`).
- Asset bundles: 7,585 files, 2.5 GB, all built for target platform 2
  (StandaloneOSX). Android needs the platform field patched; whether Unity
  accepts that is untested.
- Textures: in a 150-bundle sample, 154 of 264 Texture2D are BC7 (format 25),
  107 RGBA32. Mobile GPUs generally lack BC7: try Unity's runtime fallback
  first, transcode to ASTC if needed.
- Shaders: Metal only, also inside bundles. Upstream's blob parser/writer can be
  reused; a new Metal -> GLSL ES 3 backend is needed (GLES3 first, Vulkan later).

## Changes needed in the glue

- `recomp/runtime/*`: already has non-`_WIN32` paths; mostly reusable.
- `icall_marshal.cpp`: rearranges Apple AAPCS64 args for Win64. On Android the
  native ABI is AAPCS64 too, so this becomes a near pass-through (only Apple's
  packed stack arguments differ).
- `abi.py` / bridges: by-value structs are passed as opaque `Blob<size,align>`.
  Fine on Win64, wrong on AAPCS64 for HFAs (Vector3, Color...), which must go in
  v registers. Emit real float/double aggregate types for HFA classes.
- `libc_imports.cpp`: map to bionic instead of the MSVC CRT.
- Build: NDK clang, link `baselib.a` from the Unity Android module.

## Done so far

- `patches/0001`: HFA value types get a real `Hfa<float|double, N>` native type in
  the bridge generators (was an opaque byte blob).
- `patches/0002`: `recomp/android/aapcs_call.S`, a trampoline that calls native
  code with the guest's x0-x8/q0-q7 and writes x0/x1/q0-q3 back. Used by the
  generic native fallback and by the icall marshaller's AAPCS64 path (Apple's
  packed stack arguments are re-laid into 8-byte slots). Verified under qemu
  (`tests/run_aapcs_test.sh`).
- `recomp/runtime/*` compiles with NDK r19c for arm64 with no changes beyond these.
- `tools/msl2glsl.py`, `tools/convert_shaders_gles.py`: Metal -> GLSL ES 3.00.
  Each fragment variant is paired with its best vertex variant (and vice
  versa) into one GLES3 program listed under progVertex, as Unity's own GLES3
  shaders are. All 1,329 unique programs of the top-level data files pass
  glslangValidator, including the vertex/fragment link check
  (`tools/validate_gles.py`). Instancing variants (structured buffers) and
  render_target_array_index become stubs, as upstream does for D3D11.

## Steps

1. [x] Toolchain and game files verified (`scripts/setup_env.sh`)
2. [x] Analysis: Il2CppDumper (CodeRegistration 0x3ad2790), function table
   (212,294 + 78,963 __text = 291,257, `scripts/merge_ft_all.py`), type classes, LSDA
3. [x] Translation and NDK build of `libil2cpp.so` (411 MB stripped; link with LLD 18)
4. [~] Data: build settings and shaders done (29 bundles + top-level files); bundle platform and BC7 untested
5. [ ] APK packaging, on-device testing (logcat)
