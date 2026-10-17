# Android

An ARM64 APK using Tempest's Android backend. Requires Android 7 (API 24) and Vulkan 1.1. Android window destruction closes the application, including when backgrounding or locking destroys its surface, so save the game before switching applications.

The application has a launcher (`android/java`) that imports the game files and starts the native `GameActivity`. Controls are drawn and handled by the engine, see `common/utils/androidinput.cpp`.

## Build

Install JDK 17, Gradle 8.9, CMake 3.22.1 or newer, Ninja and the Android SDK with SDK 35, build-tools 35.0.0, NDK 27.0.12077973 and SDK CMake 3.22.1. Set `JAVA_HOME`, `ANDROID_HOME` and `VULKAN_SDK`. Put Gradle, Ninja and the host Vulkan SDK's `glslangValidator` on `PATH`. Vulkan headers come from the host SDK; Android links the NDK's Vulkan loader.

Clone with `git clone --recursive https://github.com/Try/OpenGothic.git`, or run `git submodule update --init --recursive` in an existing checkout. Replace `/path/to/ndk` below with the NDK installation directory. The same commands work in PowerShell and a Linux shell.

```sh
cmake -S . -B build/android -G Ninja -DCMAKE_TOOLCHAIN_FILE=/path/to/ndk/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 -DCMAKE_BUILD_TYPE=Release
cmake --build build/android --target OpenGothic-apk
```

APK: `build/android/OpenGothic-apk/build/outputs/apk/release/OpenGothic-apk-release.apk`. Use `-DCMAKE_BUILD_TYPE=Debug` for a debuggable APK under `outputs/apk/debug/OpenGothic-apk-debug.apk`. Desktop builds do not invoke the APK helper or require Android tools.

`android/build-apk.sh` builds a signed APK with the tools of the Android SDK only, without Gradle. See the comment in the script for its options.

## Game files

Use a legally owned Gothic II: Night of the Raven installation. No game files are included in the APK.

Make a ZIP of the installation: `Data`, `_work` and `System` have to be at the root of the archive, or in a single folder in it. Choose **Import game files** in the launcher; the files are copied to the app-specific storage, no storage permissions are needed.

Alternatively, copy the installation root with adb. `Data`, `_work` and `System` must be directly inside `Gothic2`.

```sh
adb install -r build/android/OpenGothic-apk/build/outputs/apk/release/OpenGothic-apk-release.apk
adb shell mkdir -p /sdcard/Android/data/org.opengothic.mobile/files/Gothic2
adb push "/path/to/Gothic II/." /sdcard/Android/data/org.opengothic.mobile/files/Gothic2/
adb logcat -s app AndroidRuntime DEBUG
```

Logs, saves and writable settings use the app's internal files directory. Uninstalling removes both directories. Ray tracing and mesh shading are disabled for this Android build.

With a debug APK, read the file log using `adb shell run-as org.opengothic.mobile cat files/log.txt`.

## Touch controls

- Left stick moves the hero, dragging the right side of the screen rotates the camera.
- Actions that are not possible at the moment (attack without a weapon, spells without mana, sneaking before it is learned, ...) are hidden or dimmed.
- Menus are scrolled with swipes. A selection is confirmed only with an explicit button, never by a gesture.
- Inventory has item categories, direct selection and use/drop/transfer buttons.
- The gear in the main menu opens mobile settings: button size, 3D resolution, shadows, SSAO, FPS limit, joystick deadzone and camera sensitivity. They are stored in the `[MOBILE]` section of the ini file; the defaults are 75% resolution, 512 shadow map, no SSAO and 30 FPS.

The state machine of the controls has no Android dependencies and is tested on the host:

```sh
c++ -std=c++17 -I common android/tests/controls_test.cpp common/ui/mobilecontrolsmodel.cpp -o controls_test && ./controls_test
```

## Icons

The icons of the controls are 128x128 RGBA PNG files in `android/assets/touch-icons`. They are embedded into the binary by `android/assets/CMakeLists.txt` and decoded at startup. `IconFiles` in `common/utils/androidinput.cpp` lists them in the order of `AndroidInput::Icon`.
