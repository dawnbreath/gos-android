#!/usr/bin/env bash
# Builds libil2cpp.so for Android arm64 with NDK r19c: Unity's libil2cpp +
# Boehm GC + zlib, the recompiled game code, the bridges and the runtime glue.
# Android counterpart of upstream recomp/winbuild/build_gameassembly.sh.
#
# Environment:
#   TP           third_party dir from setup_env.sh (default ./third_party)
#   RECOMP_OUT   translator output (registration-derived functions)
#   GAP_OUT      translator output (--extra-table helpers)
#   BRIDGES_OUT  gen_bridges.py output
#   BUILD_DIR    objects and the .so
#   STAGES       space-separated subset of: il2cpp glue server gc runtime gen gap link plugins (default: all)
set -euo pipefail
ulimit -s unlimited  # the largest recompiled functions need a deep compiler stack

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TP="${TP:-$ROOT/third_party}"
U="$TP/GoSRecomp"
L="$TP/unity/editor/Editor/Data/il2cpp"
A="$TP/unity/android/Variations/il2cpp/Release/StaticLibs/arm64-v8a"
NDK_BIN="$TP/android-ndk-r19c/toolchains/llvm/prebuilt/linux-x86_64/bin"
CC="$NDK_BIN/aarch64-linux-android24-clang"
CXX="$NDK_BIN/aarch64-linux-android24-clang++"
JOBS="${JOBS:-4}"
# NDK r19c clang can spend hours optimizing a few giant recompiled functions: after
# TIMEOUT a file is rebuilt at -O0 (seconds)
TIMEOUT="${TIMEOUT:-900}"
STAGES="${STAGES:-il2cpp glue server gc runtime gen gap link plugins}"
RUNTIME="$U/recomp/runtime"
WB="$U/recomp/winbuild"

SYS="-fPIC -fno-strict-aliasing -ffunction-sections -fdata-sections -Wno-everything -DNDEBUG"
IL2CPP_FLAGS="$SYS -O2 -std=c++14 -DGC_NOT_DLL -DRUNTIME_IL2CPP -DHAVE_BDWGC_GC \
  -DBASELIB_INLINE_NAMESPACE=il2cpp_baselib \
  -I$L/libil2cpp -I$L/libil2cpp/pch -I$L/external/bdwgc/include -I$L/external/baselib/Include \
  -I$L/external/baselib/Platforms/Android/Include -I$L/external/xxHash -I$L/external/zlib \
  -I$L/external/mono -I$L/external/google"
GC_FLAGS="$SYS -O2 -I$L/external/bdwgc/include -I$L/external/bdwgc/libatomic_ops/src -DGC_NOT_DLL \
  -DALL_INTERIOR_POINTERS=1 -DGC_GCJ_SUPPORT=1 -DJAVA_FINALIZATION=1 -DNO_EXECUTE_PERMISSION=1 \
  -DGC_NO_THREADS_DISCOVERY=1 -DIGNORE_DYNAMIC_LOADING=1 -DGC_DONT_REGISTER_MAIN_STATIC_DATA=1 \
  -DGC_VERSION_MAJOR=7 -DGC_VERSION_MINOR=7 -DGC_VERSION_MICRO=0 -DGC_THREADS=1 -DUSE_MMAP=1 -DUSE_MUNMAP=1"
GEN_FLAGS="$SYS -O1 -std=c++17 -I$RUNTIME -I${RECOMP_OUT:-.} -I${BRIDGES_OUT:-.}"
GAP_FLAGS="$SYS -O1 -std=c++17 -I$RUNTIME -I${GAP_OUT:-.}"
GLUE_FLAGS="$SYS -O2 -std=c++17 -I$RUNTIME"
IMPORT_FLAGS="${IL2CPP_FLAGS/-std=c++14/-std=c++17} -I$RUNTIME -I$WB -I$L/libil2cpp/icalls"

mkdir -p "$BUILD_DIR"/{il2cpp,gc,zlib,gen,gap,glue,server}
compile() {  # compiler, flags, output dir, sources...
    local cc="$1" flags="$2" out="$3"; shift 3
    printf '%s\n' "$@" | xargs -P "$JOBS" -I{} sh -c \
        'o="$0/$(echo {} | tr "/" "_" | sed "s/\.[a-zA-Z]*$/.o/")"; [ -s "$o" ] || timeout '"$TIMEOUT"' '"$cc $flags"' -c {} -o "$o" >"$o.log" 2>&1 \
         || { echo "retry -O0 {}"; '"$cc $flags"' -O0 -c {} -o "$o" >>"$o.log" 2>&1; } || echo "FAIL {}"' "$out"
}
has() { [[ " $STAGES " == *" $1 "* ]]; }

if has il2cpp; then
  echo "== libil2cpp"
  mapfile -t SRC < <(find "$L/libil2cpp" -name '*.cpp' | grep -vE '/os/(Win32|WinRT|Emscripten|Lumin|WindowsGames|OSX)/|/debugger/|/metadata/FieldLayout\.cpp$')
  compile "$CXX" "$IL2CPP_FLAGS" "$BUILD_DIR/il2cpp" "${SRC[@]}" "$WB/field_layout_clang_abi.cpp"
fi
if has glue; then
  echo "== import handlers"
  compile "$CXX" "$IMPORT_FLAGS" "$BUILD_DIR/glue" "$WB/runtime_imports.cpp" "$WB/icall_marshal.cpp" \
      "$WB/icall_imports.cpp" "$WB/libc_imports.cpp" "$WB/battle_hooks.cpp" \
      "$ROOT/src/codegen_registration_android.cpp"
  compile "$CXX" "$GLUE_FLAGS" "$BUILD_DIR/glue" "$ROOT/src/android_glue.cpp"  # no winbuild/unistd.h shim
fi
if has server; then
  echo "== offline server"
  compile "$CXX" "$IMPORT_FLAGS -I$U/recomp/server" "$BUILD_DIR/server" "$U/recomp/server/local_server.cpp" "$U/recomp/server/msgpack.cpp"
fi
if has gc; then
  echo "== Boehm GC and zlib"
  compile "$CC" "$GC_FLAGS" "$BUILD_DIR/gc" "$L/external/bdwgc/extra/gc.c"
  compile "$CC" "$GC_FLAGS -I$L/external/zlib" "$BUILD_DIR/zlib" "$L"/external/zlib/*.c
fi
if has runtime; then
  echo "== runtime"
  compile "$CXX" "$GLUE_FLAGS" "$BUILD_DIR/glue" "$RUNTIME/macho_image.cpp" "$RUNTIME/recomp_runtime.cpp" \
      "$RUNTIME/recomp_bridge.cpp" "$RUNTIME/recomp_eh.cpp" "$WB/image_symbols.cpp"
  compile "$CC" "" "$BUILD_DIR/glue" "$U/recomp/android/aapcs_call.S"
fi
if has gen; then
  echo "== recompiled game code and bridges"
  compile "$CXX" "$GEN_FLAGS" "$BUILD_DIR/gen" "$RECOMP_OUT"/*.cpp "$BRIDGES_OUT"/*.cpp
fi
if has gap; then
  compile "$CXX" "$GAP_FLAGS" "$BUILD_DIR/gap" "$GAP_OUT"/*.cpp
fi
if has link; then
  echo "== link"
  # lld: the text exceeds the 128 MB reach of arm64 branches; lld adds range-extension thunks
  "$CXX" -fuse-ld=lld -shared -o "$BUILD_DIR/libil2cpp.so" -Wl,--gc-sections -Wl,-soname,libil2cpp.so \
      "$BUILD_DIR"/il2cpp/*.o "$BUILD_DIR"/gc/*.o "$BUILD_DIR"/zlib/*.o "$BUILD_DIR"/glue/*.o \
      "$BUILD_DIR"/server/*.o "$BUILD_DIR"/gen/*.o "$BUILD_DIR"/gap/*.o \
      "$A/baselib.a" -static-libstdc++ -llog -ldl -lm -lz
  echo "built $BUILD_DIR/libil2cpp.so"
fi
if has plugins; then
  echo "== plugin stand-ins"
  mkdir -p "$BUILD_DIR/plugins"
  for name in FPBundleWrapper GameCenterAccessPointBundleWrapper iCloudBundleWrapper GCControllerBundleWrapper \
              SymbolImageBundleWrapper SystemLangBundle SPFXUnityPlugin; do
    "$CXX" -shared -fPIC -O2 -std=c++17 -fvisibility=hidden -DGOSR_PLUGIN_$name -static-libstdc++ \
      -o "$BUILD_DIR/plugins/lib$name.so" "$ROOT/src/plugins/android_plugins.cpp"
  done
  ls "$BUILD_DIR/plugins"
fi
