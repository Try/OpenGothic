#!/usr/bin/env bash
# Builds and signs the APK with the tools of the Android SDK only, no Gradle or Maven downloads.
#
#   ANDROID_HOME, JAVA_HOME (JDK 17)   required
#   ANDROID_NDK_ROOT                   default: $ANDROID_HOME/ndk/27.0.12077973
#   BUILD_DIR                          default: build/android
#   BUILD_JOBS                         default: 4
#   PACKAGE_ONLY=1                     skip the native build and package the existing library
#   KEYSTORE                           default: a test keystore generated in BUILD_DIR
#   VERSION_CODE, VERSION_NAME         recorded in the manifest, VERSION_CODE has to grow with every update
set -euo pipefail

repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
: "${ANDROID_HOME:?Set ANDROID_HOME to your Android SDK}"
: "${JAVA_HOME:?Set JAVA_HOME to your JDK 17}"
export PATH="$JAVA_HOME/bin:$PATH"

ndk_dir="${ANDROID_NDK_ROOT:-$ANDROID_HOME/ndk/27.0.12077973}"
tools_dir="$ANDROID_HOME/build-tools/35.0.0"
android_jar="$ANDROID_HOME/platforms/android-35/android.jar"
cmake_dir="$ANDROID_HOME/cmake/3.22.1/bin"
build_dir="${BUILD_DIR:-$repo_dir/build/android}"
jobs="${BUILD_JOBS:-4}"
package_name="org.opengothic.mobile"

package_dir="$(mktemp -d)"
trap 'rm -rf "$package_dir"' EXIT
export PATH="$cmake_dir:$PATH"

if [[ "${PACKAGE_ONLY:-0}" != 1 ]]; then
  cmake -S "$repo_dir" -B "$build_dir" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$ndk_dir/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 -DCMAKE_BUILD_TYPE=Release
  cmake --build "$build_dir" --target shader fonts --parallel "$jobs"
  python3 "$repo_dir/android/embed-resources.py" "$repo_dir" "$build_dir"
  cmake --build "$build_dir" --target Gothic2Notr --parallel "$jobs"
fi

mkdir -p "$package_dir/classes" "$package_dir/dex" "$package_dir/lib/arm64-v8a"

# Package, version and sdk levels are normally supplied by Gradle
python3 - "$repo_dir/android/AndroidManifest.xml" "$package_dir/AndroidManifest.xml" \
  "$package_name" "${VERSION_CODE:-8}" "${VERSION_NAME:-0.8}" <<'PY'
import sys
import xml.etree.ElementTree as ET

src, dst, package, code, name = sys.argv[1:]
ns = '{http://schemas.android.com/apk/res/android}'
ET.register_namespace('android', 'http://schemas.android.com/apk/res/android')
tree = ET.parse(src)
root = tree.getroot()
root.set('package', package)
root.set(ns + 'versionCode', code)
root.set(ns + 'versionName', name)
ET.SubElement(root, 'uses-sdk', {ns + 'minSdkVersion': '24', ns + 'targetSdkVersion': '35'})
tree.write(dst, encoding='utf-8', xml_declaration=True)
PY

"$tools_dir/aapt2" link -I "$android_jar" --manifest "$package_dir/AndroidManifest.xml" -o "$package_dir/base.apk"

javac -source 8 -target 8 -bootclasspath "$android_jar:$tools_dir/core-lambda-stubs.jar" \
  -d "$package_dir/classes" "$repo_dir"/android/java/org/opengothic/app/*.java
jar cf "$package_dir/classes.jar" -C "$package_dir/classes" .
"$tools_dir/d8" --release --min-api 24 --lib "$android_jar" --output "$package_dir/dex" "$package_dir/classes.jar"

cp "$build_dir/opengothic/libGothic2Notr.so" "$package_dir/lib/arm64-v8a/"
"$ndk_dir/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip" --strip-unneeded "$package_dir/lib/arm64-v8a/libGothic2Notr.so"

python3 - "$package_dir" <<'PY'
import sys
import zipfile
from pathlib import Path

root = Path(sys.argv[1])
with zipfile.ZipFile(root / 'base.apk', 'a', compression=zipfile.ZIP_DEFLATED) as z:
  for f in sorted((root / 'dex').glob('*.dex')):
    z.write(f, f.name)
  for f in sorted((root / 'lib').rglob('*.so')):
    z.write(f, f.relative_to(root).as_posix())
PY

# 16 KiB page alignment is required by recent Android versions
"$tools_dir/zipalign" -f -P 16 4 "$package_dir/base.apk" "$package_dir/aligned.apk"

keystore="${KEYSTORE:-$build_dir/mobile-test.keystore}"
if [[ ! -f "$keystore" ]]; then
  keytool -genkeypair -keystore "$keystore" -storepass android -keypass android -alias androiddebugkey \
    -keyalg RSA -keysize 2048 -validity 10000 -dname 'CN=OpenGothic Local Test'
fi
"$tools_dir/apksigner" sign --ks "$keystore" --ks-key-alias androiddebugkey --ks-pass pass:android \
  --key-pass pass:android --out "$package_dir/final.apk" "$package_dir/aligned.apk"
"$tools_dir/apksigner" verify --verbose "$package_dir/final.apk"
"$tools_dir/zipalign" -c -P 16 4 "$package_dir/final.apk"

cp "$package_dir/final.apk" "$build_dir/OpenGothic-arm64.apk"
echo "APK: $build_dir/OpenGothic-arm64.apk"
