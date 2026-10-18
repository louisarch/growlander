#!/usr/bin/env bash
set -euo pipefail

ANDROID_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$ANDROID_DIR/.." && pwd)"
JNI_DIR="$ANDROID_DIR/app/src/main/jni"
CACHE_DIR="$ANDROID_DIR/.bootstrap-cache"
mkdir -p "$JNI_DIR" "$CACHE_DIR" "$ANDROID_DIR/app/src/main/assets"

fetch_verified() {
    local url="$1" destination="$2" expected="$3"
    if [[ ! -f "$destination" ]] || ! echo "$expected  $destination" | sha256sum --check --status; then
        rm -f "$destination"
        curl --fail --location --retry 3 --output "$destination" "$url"
    fi
    echo "$expected  $destination" | sha256sum --check --status || {
        echo "SHA-256 mismatch: $destination" >&2
        rm -f "$destination"
        return 1
    }
}

SDL_ARCHIVE="$CACHE_DIR/SDL2-2.30.11.tar.gz"
TTF_ARCHIVE="$CACHE_DIR/SDL2_ttf-2.22.0.tar.gz"
fetch_verified "https://github.com/libsdl-org/SDL/releases/download/release-2.30.11/SDL2-2.30.11.tar.gz" \
    "$SDL_ARCHIVE" "8b8d4aef2038533da814965220f88f77d60dfa0f32685f80ead65e501337da7f"
fetch_verified "https://github.com/libsdl-org/SDL_ttf/releases/download/release-2.22.0/SDL2_ttf-2.22.0.tar.gz" \
    "$TTF_ARCHIVE" "d48cbd1ce475b9e178206bf3b72d56b66d84d44f64ac05803328396234d67723"

unpack_pinned() {
    local archive="$1" destination="$2" top="$3"
    if [[ ! -f "$destination/CMakeLists.txt" ]]; then
        rm -rf "$destination"
        mkdir -p "$destination"
        tar --extract --gzip --file "$archive" --directory "$destination" \
            --strip-components=1 --wildcards "$top/*"
    fi
}
unpack_pinned "$SDL_ARCHIVE" "$JNI_DIR/SDL" "SDL2-2.30.11"
unpack_pinned "$TTF_ARCHIVE" "$JNI_DIR/SDL_ttf" "SDL2_ttf-2.22.0"

JAVA_SOURCE="$JNI_DIR/SDL/android-project/app/src/main/java/org/libsdl/app"
if [[ ! -f "$JAVA_SOURCE/SDLActivity.java" ]]; then
    echo "SDLActivity.java is absent from the pinned SDL source archive." >&2
    exit 1
fi
mkdir -p "$ANDROID_DIR/app/src/main/java/org/libsdl/app"
cp -a "$JAVA_SOURCE/." "$ANDROID_DIR/app/src/main/java/org/libsdl/app/"
cp "$REPO_ROOT/assets/DejaVuSans.ttf" "$ANDROID_DIR/app/src/main/assets/DejaVuSans.ttf"
cp "$REPO_ROOT/assets/LICENSE-font.txt" "$ANDROID_DIR/app/src/main/assets/LICENSE-font.txt"
mkdir -p "$ANDROID_DIR/licenses"
cp "$JNI_DIR/SDL/LICENSE.txt" "$ANDROID_DIR/licenses/SDL2.txt"
cp "$JNI_DIR/SDL_ttf/LICENSE.txt" "$ANDROID_DIR/licenses/SDL2_ttf.txt"

printf 'Android SDL sources and font are ready. Next: ./gradlew assembleDebug\n'
