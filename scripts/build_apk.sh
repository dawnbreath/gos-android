#!/usr/bin/env bash
# Assembles a debug-signed APK from the Unity Android player, libil2cpp.so and
# the user's own game files. The APK contains game data: keep it to yourself.
#
# Environment:
#   TP        third_party (setup_env.sh), plus $TP/sdk/bt/<ver> (build-tools) and $TP/sdk/pf/<ver> (platform)
#   APP       ".../Castlevania Grimoire of Souls.app/Contents" extracted from the .dmg
#   GAME_ARM64  arm64 slice of GameAssembly.dylib (scripts/thin_arm64.py)
#   LIBIL2CPP   libil2cpp.so from build_android.sh
#   PLUGINS     directory with the plugin stand-ins (build_android.sh, stage plugins)
#   DATA_OVERRIDE  optional dir laid out like Data whose files replace the originals
#                  (tools/convert_shaders_gles.py output; StreamingAssets/ included)
#   NO_STREAMING  set to leave StreamingAssets out (they go to /sdcard/GoSRecomp: make_data_parts.py)
#   OUT       output directory
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TP="${TP:-$ROOT/third_party}"
UA="$TP/unity/android"
BT="$(ls -d "$TP"/sdk/bt/*/ | head -1)"
ANDROID_JAR="$(ls "$TP"/sdk/pf/*/android.jar | head -1)"
DATA="$APP/Resources/Data"
STAGE="$OUT/stage"
rm -rf "$STAGE" && mkdir -p "$STAGE"/{lib/arm64-v8a,assets/bin/Data/Managed,dex,classes}

echo "== native libraries"
cp "$UA/Variations/il2cpp/Release/Libs/arm64-v8a/libmain.so" "$UA/Variations/il2cpp/Release/Libs/arm64-v8a/libunity.so" "$STAGE/lib/arm64-v8a/"
cp "$LIBIL2CPP" "$STAGE/lib/arm64-v8a/libil2cpp.so"
cp "$GAME_ARM64" "$STAGE/lib/arm64-v8a/libgosgame.so"   # loaded as data by libil2cpp.so
[ -n "${PLUGINS:-}" ] && cp "$PLUGINS"/lib*.so "$STAGE/lib/arm64-v8a/"   # build_android.sh plugin stand-ins

echo "== player data"
D="$STAGE/assets/bin/Data"
for f in "$DATA"/*; do
  n="$(basename "$f")"
  case "$n" in StreamingAssets|il2cpp_data|Resources) continue ;; esac
  cp -r "$f" "$D/"
done
mkdir -p "$D/Resources"
cp "$DATA/Resources/unity_builtin_extra" "$D/Resources/"
cp "$DATA/Resources/unity_builtin_extra" "$D/"
cp "$UA/Data/unity default resources" "$D/"
cp -r "$DATA/il2cpp_data/Metadata" "$D/Managed/"
[ -d "$DATA/il2cpp_data/Resources" ] && cp -r "$DATA/il2cpp_data/Resources" "$D/Managed/"
printf '<?xml version="1.0" encoding="utf-8"?>\n<resources>\n  <bool name="useObb">False</bool>\n</resources>\n' > "$D/settings.xml"
python3 -I "$ROOT/scripts/patch_gfx_api.py" "$DATA/globalgamemanagers" "$D/globalgamemanagers"
if [ -n "${DATA_OVERRIDE:-}" ]; then  # converted files, laid out like Data
  (cd "$DATA_OVERRIDE" && find . -type f ! -path './StreamingAssets/*' -exec cp --parents {} "$D/" \;)
  [ -f "$DATA_OVERRIDE/Resources/unity_builtin_extra" ] && cp "$DATA_OVERRIDE/Resources/unity_builtin_extra" "$D/"
fi

if [ -z "${NO_STREAMING:-}" ]; then
  echo "== streaming assets"
  cp -r "$DATA/StreamingAssets"/. "$STAGE/assets/"
  if [ -n "${DATA_OVERRIDE:-}" ] && [ -d "$DATA_OVERRIDE/StreamingAssets" ]; then
    cp -r "$DATA_OVERRIDE/StreamingAssets"/. "$STAGE/assets/"
  fi
fi

echo "== resources and manifest"
"$BT/aapt2" compile --dir "$UA/Apk/res" -o "$OUT/res.zip"
"$BT/aapt2" link -o "$OUT/base.apk" -I "$ANDROID_JAR" --manifest "$ROOT/android/AndroidManifest.xml" "$OUT/res.zip" --auto-add-overlay

echo "== dex"
javac -nowarn -source 8 -target 8 -cp "$ANDROID_JAR:$UA/Variations/il2cpp/Release/Classes/classes.jar" \
  -d "$STAGE/classes" "$UA/Source/com/unity3d/player/UnityPlayerActivity.java" 2>&1 | grep -v "^warning\|^Note" || true
"$BT/d8" --min-api 24 --lib "$ANDROID_JAR" --classpath "$UA/Variations/il2cpp/Release/Classes/classes.jar" \
  --output "$STAGE/dex" $(find "$STAGE/classes" -name '*.class')
cp "$UA/Variations/il2cpp/Release/Classes/classes.dex" "$STAGE/classes.dex"
cp "$STAGE/dex/classes.dex" "$STAGE/classes2.dex"

echo "== package"
cp "$OUT/base.apk" "$OUT/unsigned.apk"
(cd "$STAGE" && zip -q -r -X "$OUT/unsigned.apk" classes.dex classes2.dex lib \
  && zip -q -r -X -0 "$OUT/unsigned.apk" assets)
"$BT/zipalign" -f -p 4 "$OUT/unsigned.apk" "$OUT/aligned.apk"
KS="$OUT/debug.keystore"
[ -f "$KS" ] || keytool -genkeypair -keystore "$KS" -storepass android -keypass android -alias debug \
  -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=GoSRecomp Debug" >/dev/null 2>&1
"$BT/apksigner" sign --ks "$KS" --ks-pass pass:android --key-pass pass:android --out "$OUT/gos-android.apk" "$OUT/aligned.apk"
rm -f "$OUT/unsigned.apk" "$OUT/aligned.apk" "$OUT/base.apk" "$OUT/res.zip"
ls -la "$OUT/gos-android.apk"
