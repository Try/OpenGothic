# GenerateAndroidBuild.cmake
# Generates a complete Android/Gradle build directory from templates
#
# Usage from CMakeLists.txt:
#   cmake -DGENERATE_ANDROID_BUILD=ON .

# Default configuration values
set(ANDROID_PROJECT_NAME "OpenGothic" CACHE STRING "Android project name")
set(ANDROID_NAMESPACE "com.example.opengothic" CACHE STRING "Android namespace")
set(ANDROID_APPLICATION_ID "com.example.opengothic" CACHE STRING "Android application ID")
set(ANDROID_LIB_NAME "opengothic" CACHE STRING "Native library name")
set(ANDROID_COMPILE_SDK "35" CACHE STRING "Android compile SDK version")
set(ANDROID_MIN_SDK "24" CACHE STRING "Android minimum SDK version")
set(ANDROID_TARGET_SDK "35" CACHE STRING "Android target SDK version")
set(ANDROID_VERSION_CODE "1" CACHE STRING "Android version code")
set(ANDROID_VERSION_NAME "1.0" CACHE STRING "Android version name")
set(ANDROID_GRADLE_PLUGIN_VERSION "8.7.3" CACHE STRING "Android Gradle Plugin version")
set(ANDROID_CMAKE_VERSION "3.22.1" CACHE STRING "CMake version for Android build")
set(ANDROID_ABI_LIST "\"arm64-v8a\", \"x86_64\"" CACHE STRING "Android ABI list")

function(generate_android_build)
    cmake_parse_arguments(ARG "" "OUTPUT_DIR;SOURCE_DIR" "" ${ARGN})

    if(NOT ARG_SOURCE_DIR)
        set(ARG_SOURCE_DIR "${CMAKE_SOURCE_DIR}")
    endif()
    if(NOT ARG_OUTPUT_DIR)
        set(ARG_OUTPUT_DIR "${CMAKE_BINARY_DIR}/android-build")
    endif()

    set(OPENGOTHIC_SOURCE_DIR "${ARG_SOURCE_DIR}")
    set(ANDROID_BUILD_DIR "${ARG_OUTPUT_DIR}")

    message(STATUS "Generating Android build directory: ${ANDROID_BUILD_DIR}")

    # Create directory structure
    file(MAKE_DIRECTORY "${ANDROID_BUILD_DIR}/app/src/main")
    file(MAKE_DIRECTORY "${ANDROID_BUILD_DIR}/gradle/wrapper")

    # settings.gradle
    file(WRITE "${ANDROID_BUILD_DIR}/settings.gradle"
"pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}
rootProject.name = \"${ANDROID_PROJECT_NAME}\"
include ':app'
")

    # build.gradle (root)
    file(WRITE "${ANDROID_BUILD_DIR}/build.gradle"
"plugins {
    id 'com.android.application' version '${ANDROID_GRADLE_PLUGIN_VERSION}' apply false
}
")

    # gradle.properties
    file(WRITE "${ANDROID_BUILD_DIR}/gradle.properties"
"org.gradle.jvmargs=-Xmx2048m -Dfile.encoding=UTF-8
android.useAndroidX=true
android.nonTransitiveRClass=true
")

    # gradle-wrapper.properties
    file(WRITE "${ANDROID_BUILD_DIR}/gradle/wrapper/gradle-wrapper.properties"
"distributionBase=GRADLE_USER_HOME
distributionPath=wrapper/dists
distributionUrl=https\\://services.gradle.org/distributions/gradle-8.9-bin.zip
zipStoreBase=GRADLE_USER_HOME
zipStorePath=wrapper/dists
")

    # app/build.gradle (from template - more complex)
    configure_file(
        "${OPENGOTHIC_SOURCE_DIR}/cmake/android/app.build.gradle.in"
        "${ANDROID_BUILD_DIR}/app/build.gradle"
        @ONLY
    )

    # AndroidManifest.xml (from template)
    configure_file(
        "${OPENGOTHIC_SOURCE_DIR}/AndroidManifest.xml.in"
        "${ANDROID_BUILD_DIR}/app/src/main/AndroidManifest.xml"
        @ONLY
    )

    # local.properties (SDK path)
    if(DEFINED ENV{ANDROID_HOME})
        set(SDK_DIR "$ENV{ANDROID_HOME}")
    elseif(DEFINED ENV{ANDROID_SDK_ROOT})
        set(SDK_DIR "$ENV{ANDROID_SDK_ROOT}")
    else()
        set(SDK_DIR "")
    endif()

    if(SDK_DIR)
        string(REPLACE "\\" "/" SDK_DIR "${SDK_DIR}")
        file(WRITE "${ANDROID_BUILD_DIR}/local.properties" "sdk.dir=${SDK_DIR}\n")
    else()
        file(WRITE "${ANDROID_BUILD_DIR}/local.properties" "# sdk.dir=/path/to/Android/Sdk\n")
    endif()

    message(STATUS "Done. Build: cd ${ANDROID_BUILD_DIR} && ./gradlew assembleRelease")
endfunction()
