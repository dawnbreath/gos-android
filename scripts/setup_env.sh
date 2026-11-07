#!/usr/bin/env bash
# Fetches the toolchain for the Android port into $TP (default ./third_party).
# Nothing here contains game data. Run from the repository root.
#
#   - GoSRecomp (upstream, MIT) at a pinned commit
#   - Unity 2020.3.30f1 Android Build Support (IL2CPP arm64 player, baselib.a)
#   - Unity 2020.3.30f1 Linux editor, only Editor/Data/il2cpp (libil2cpp sources)
#   - Android NDK r19c (the NDK generation Unity 2020.3 uses)
#   - 7-Zip 24.09 (Ubuntu package; 23.x fails on the .dmg's HFS+ compressed files)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TP="${TP:-$ROOT/third_party}"
mkdir -p "$TP" && cd "$TP"

UNITY_HASH=1fb1bf06830e
UNITY_VER=2020.3.30f1
GOSR_COMMIT=c344597ff97290c5c974d66bb203b10c631fcf97

if [ ! -d GoSRecomp ]; then
  git clone -q https://github.com/BLSNCVS/Castlevania-Grimoire-of-Souls-Recomp GoSRecomp
  git -C GoSRecomp checkout -q "$GOSR_COMMIT"
  for p in "$ROOT"/patches/*.patch; do git -C GoSRecomp apply "$p"; done
fi

if [ ! -x 7z24/usr/lib/7zip/7z ]; then
  curl -sSO http://archive.ubuntu.com/ubuntu/pool/universe/7/7zip/7zip_24.09+dfsg-7_amd64.deb
  dpkg-deb -x 7zip_24.09+dfsg-7_amd64.deb 7z24 && rm 7zip_24.09+dfsg-7_amd64.deb
fi
Z="$TP/7z24/usr/lib/7zip/7z"

if [ ! -d unity/android ]; then
  mkdir -p unity/android unity/pkg
  curl -sS -o unity/android.pkg \
    "https://download.unity3d.com/download_unity/$UNITY_HASH/MacEditorTargetInstaller/UnitySetup-Android-Support-for-Editor-$UNITY_VER.pkg"
  echo "6ebd69f2506cd17cddd67dc2cd584b0c  unity/android.pkg" | md5sum -c -
  "$Z" x -y -bso0 -ounity/pkg unity/android.pkg
  (cd unity/android && zcat ../pkg/TargetSupport.pkg.tmp/Payload | cpio -id --quiet)
  rm -rf unity/pkg unity/android.pkg unity/android/Variations/mono
fi

if [ ! -d unity/editor/Editor/Data/il2cpp ]; then
  mkdir -p unity/editor
  curl -sS "https://download.unity3d.com/download_unity/$UNITY_HASH/LinuxEditorInstaller/Unity.tar.xz" \
    | tar -xJf - -C unity/editor --wildcards "Editor/Data/il2cpp/*"
fi

if [ ! -d android-ndk-r19c ]; then
  curl -sSO https://dl.google.com/android/repository/android-ndk-r19c-linux-x86_64.zip
  unzip -q android-ndk-r19c-linux-x86_64.zip && rm android-ndk-r19c-linux-x86_64.zip
fi

echo "toolchain ready in $TP"
