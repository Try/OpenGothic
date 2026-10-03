# Android

An ARM64 NativeActivity APK using Tempest's Android backend and generated Gradle project. This is the basic platform integration; touch controls, controller input and game-file importing are not included yet. Android window destruction closes the application, including when backgrounding or locking destroys its surface.

## Build

Install JDK 17, Gradle 8.9, CMake 3.22.1 or newer, Ninja and the Android SDK with SDK 35, build-tools 35.0.0, NDK 27.0.12077973 and SDK CMake 3.22.1. Set `JAVA_HOME`, `ANDROID_HOME` and `VULKAN_SDK`. Put Gradle, Ninja and the host Vulkan SDK's `glslangValidator` on `PATH`. Vulkan headers come from the host SDK; Android links the NDK's Vulkan loader.

Clone with `git clone --recursive https://github.com/Try/OpenGothic.git`, or run `git submodule update --init --recursive` in an existing checkout. Replace `/path/to/ndk` below with the NDK installation directory. The same commands work in PowerShell and a Linux shell.

```sh
cmake -S . -B build/android -G Ninja -DCMAKE_TOOLCHAIN_FILE=/path/to/ndk/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 -DCMAKE_BUILD_TYPE=Release
cmake --build build/android --target OpenGothic-apk
```

APK: `build/android/OpenGothic-apk/build/outputs/apk/release/OpenGothic-apk-release.apk`. Use `-DCMAKE_BUILD_TYPE=Debug` for a debuggable APK under `outputs/apk/debug/OpenGothic-apk-debug.apk`. Desktop builds do not invoke the APK helper or require Android tools.

## Install and game files

Use a legally owned Gothic II: Night of the Raven installation. No game files are included in the APK. Copy the installation root, not only its `Data` directory: `Data`, `_work` and `System` must be directly inside `Gothic2`.

```sh
adb install -r build/android/OpenGothic-apk/build/outputs/apk/release/OpenGothic-apk-release.apk
adb shell mkdir -p /sdcard/Android/data/org.opengothic.app/files/Gothic2
adb push "/path/to/Gothic II/." /sdcard/Android/data/org.opengothic.app/files/Gothic2/
adb shell am start -n org.opengothic.app/.GothicActivity
adb logcat -s OpenGothic AndroidRuntime DEBUG
```

Game files use app-specific external storage, without storage permissions. Logs, saves and writable settings use the app's internal files directory. Uninstalling removes both directories. Ray tracing and mesh shading are disabled for this initial Android build.

With a debug APK, read the file log using `adb shell run-as org.opengothic.app cat files/log.txt`.
