#!/bin/sh
# BOUCLIER 2050 - construction des paquets de release
#
#   tools/release/build.sh linux   <raylib>   -> dist/bouclier2050-linux-x64.tar.gz
#   tools/release/build.sh windows <raylib>   -> dist/bouclier2050-windows-x64.zip   (MinGW-w64, depuis Linux)
#   tools/release/build.sh macos   <raylib>   -> dist/bouclier2050-macos-arm64.zip   (sur un Mac Apple Silicon)
#   tools/release/build.sh android <raylib>   -> dist/bouclier2050-android-arm64.apk (SDK et NDK Android)
#
# <raylib> : clone de raylib 5.0 (git clone --depth 1 --branch 5.0 https://github.com/raysan5/raylib.git)
set -eu

TARGET=${1:?cible : linux, windows, macos ou android}
RAYLIB=$(cd "${2:?chemin du clone de raylib 5.0}" && pwd)
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
VERSION=${VERSION:-$(git -C "$ROOT" describe --tags --always 2>/dev/null || echo dev)}
DIST="$ROOT/dist"
WORK="$ROOT/build/release-$TARGET"
SIM="game world society war war_ai war_diplo war_logistics military ops resources intl command economy politics nuclear map_data save_fields events cabinet lang lang_en"
RL_SRC="rcore rshapes rtextures rtext rmodels raudio utils"
mkdir -p "$DIST" "$WORK"

sources() {
    for f in main ui_widgets ui_map ui_tabs_war ui_tabs_state ui_screens sound; do printf '%s ' "$ROOT/src/$f.c"; done
    for f in $SIM; do printf '%s ' "$ROOT/src/$f.c"; done
}

docs() {
    cp "$ROOT/README.md" "$1/LISEZMOI.md"
    [ -f "$ROOT/LICENSE" ] && cp "$ROOT/LICENSE" "$1/" || true
}

# raylib compile pour le bureau avec le compilateur $1 (bibliotheque statique dans $WORK)
build_raylib_desktop() {
    cc=$1; ar=$2; shift 2
    objs=""
    for f in $RL_SRC rglfw; do
        [ "$f" = rglfw ] && [ "$TARGET" = macos ] && extra="-x objective-c" || extra=""
        $cc -c -O2 -DPLATFORM_DESKTOP -DGRAPHICS_API_OPENGL_33 $extra "$@" -I"$RAYLIB/src/external/glfw/include" \
            "$RAYLIB/src/$f.c" -o "$WORK/rl_$f.o"
        objs="$objs $WORK/rl_$f.o"
    done
    rm -f "$WORK/libraylib.a"
    $ar rcs "$WORK/libraylib.a" $objs
}

case "$TARGET" in
linux)
    build_raylib_desktop "${CC:-gcc}" ar
    OUT="$WORK/bouclier2050"
    mkdir -p "$OUT"
    ${CC:-gcc} -std=c99 -O2 -I"$RAYLIB/src" $(sources) "$WORK/libraylib.a" -lGL -lm -lpthread -ldl -lrt -lX11 -o "$OUT/bouclier2050"
    docs "$OUT"
    tar -C "$WORK" -czf "$DIST/bouclier2050-$VERSION-linux-x64.tar.gz" bouclier2050
    ;;
windows)
    CCW=${CCW:-x86_64-w64-mingw32-gcc}
    build_raylib_desktop "$CCW" "${ARW:-x86_64-w64-mingw32-ar}"
    OUT="$WORK/bouclier2050"
    mkdir -p "$OUT"
    $CCW -std=c99 -O2 -I"$RAYLIB/src" $(sources) "$WORK/libraylib.a" -lopengl32 -lgdi32 -lwinmm -static -mwindows \
        -o "$OUT/bouclier2050.exe"
    docs "$OUT"
    (cd "$WORK" && rm -f "$DIST/bouclier2050-$VERSION-windows-x64.zip" && zip -qr "$DIST/bouclier2050-$VERSION-windows-x64.zip" bouclier2050)
    ;;
macos)
    build_raylib_desktop clang ar -arch arm64 -mmacosx-version-min=11.0
    APP="$WORK/Bouclier 2050.app"
    rm -rf "$APP"
    mkdir -p "$APP/Contents/MacOS"
    clang -std=c99 -O2 -arch arm64 -mmacosx-version-min=11.0 -I"$RAYLIB/src" $(sources) "$WORK/libraylib.a" \
        -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -framework CoreAudio -framework AudioToolbox \
        -o "$APP/Contents/MacOS/bouclier2050"
    cat > "$APP/Contents/Info.plist" << PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>CFBundleName</key><string>Bouclier 2050</string>
  <key>CFBundleExecutable</key><string>bouclier2050</string>
  <key>CFBundleIdentifier</key><string>fr.bouclier2050.game</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>$VERSION</string>
  <key>LSMinimumSystemVersion</key><string>11.0</string>
  <key>NSHighResolutionCapable</key><true/>
</dict></plist>
PLIST
    codesign --force --deep -s - "$APP" 2>/dev/null || true   # signature ad hoc (pas de notarisation)
    docs "$WORK"
    (cd "$WORK" && rm -f "$DIST/bouclier2050-$VERSION-macos-arm64.zip" && zip -qry "$DIST/bouclier2050-$VERSION-macos-arm64.zip" "Bouclier 2050.app" LISEZMOI.md)
    ;;
android)
    NDK=${ANDROID_NDK_HOME:?ANDROID_NDK_HOME requis}
    SDK=${ANDROID_SDK_ROOT:-${ANDROID_HOME:?ANDROID_SDK_ROOT requis}}
    API=${ANDROID_API:-29}
    BT=$(ls -d "$SDK"/build-tools/* | sort -V | tail -n 1)
    PLATFORM=$(ls -d "$SDK"/platforms/android-* | sort -V | tail -n 1)
    TC="$NDK/toolchains/llvm/prebuilt/linux-x86_64"
    CCA="$TC/bin/aarch64-linux-android$API-clang"
    # raylib pour Android (OpenGL ES 2) avec la glue native de NativeActivity.
    # Les NDK recents ont retire ALooper_pollAll : raylib 5.5 l'a remplace par ALooper_pollOnce.
    sed -i 's/ALooper_pollAll(/ALooper_pollOnce(/g' "$RAYLIB/src/platforms/rcore_android.c"
    objs=""
    for f in $RL_SRC; do
        $CCA -c -O2 -fPIC -DPLATFORM_ANDROID -DGRAPHICS_API_OPENGL_ES2 -I"$RAYLIB/src" -I"$NDK/sources/android/native_app_glue" \
            "$RAYLIB/src/$f.c" -o "$WORK/rl_$f.o"
        objs="$objs $WORK/rl_$f.o"
    done
    $CCA -c -O2 -fPIC "$NDK/sources/android/native_app_glue/android_native_app_glue.c" -o "$WORK/glue.o"
    LIB="$WORK/apk/lib/arm64-v8a"
    mkdir -p "$LIB"
    $CCA -std=c99 -O2 -fPIC -shared -DPLATFORM_ANDROID -I"$RAYLIB/src" -I"$NDK/sources/android/native_app_glue" $(sources) \
        $objs "$WORK/glue.o" -u ANativeActivity_onCreate -llog -landroid -lEGL -lGLESv2 -lOpenSLES -ldl -lm -o "$LIB/libmain.so"
    # paquet : manifeste, bibliotheque native, alignement et signature
    "$BT/aapt2" link -o "$WORK/base.apk" --manifest "$ROOT/tools/release/android/AndroidManifest.xml" -I "$PLATFORM/android.jar" \
        --min-sdk-version "$API" --target-sdk-version 34 --version-name "$VERSION" --version-code "${VERSION_CODE:-1}"
    (cd "$WORK/apk" && zip -qr "$WORK/base.apk" lib)
    "$BT/zipalign" -f -p 4 "$WORK/base.apk" "$WORK/aligned.apk"
    KS=${ANDROID_KEYSTORE:-$WORK/debug.keystore}
    if [ ! -f "$KS" ]; then
        keytool -genkeypair -keystore "$KS" -storepass android -keypass android -alias bouclier -keyalg RSA -keysize 2048 \
            -validity 10000 -dname "CN=Bouclier 2050" >/dev/null 2>&1
    fi
    "$BT/apksigner" sign --v4-signing-enabled false --ks "$KS" --ks-pass "pass:${ANDROID_KEYSTORE_PASS:-android}" \
        --out "$DIST/bouclier2050-$VERSION-android-arm64.apk" "$WORK/aligned.apk"
    ;;
*)
    echo "cible inconnue : $TARGET" >&2
    exit 1
    ;;
esac
ls -l "$DIST"
