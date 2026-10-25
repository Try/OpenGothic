cmake_minimum_required(VERSION 3.16)

project(OpenGothic LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(BUILD_SHARED_LIBS OFF)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/opengothic)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/opengothic)
set(CMAKE_DEBUG_POSTFIX "")
set(CMAKE_RELWITHDEBINFO_POSTFIX "")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_SKIP_RPATH ON)

add_library(${PROJECT_NAME} STATIC)
if(ANDROID)
  add_library(Gothic2Notr SHARED)
  add_library(Spacer SHARED)
else()
  add_executable(Gothic2Notr)
  add_executable(Spacer)
endif()

if(MSVC)
  add_definitions(-D_USE_MATH_DEFINES)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
  add_definitions(-D_ITERATOR_DEBUG_LEVEL=0)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4800")
endif()

if(NOT MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wconversion -Wno-strict-aliasing -Werror)
  if(CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "8.0")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wno-class-memaccess)
  endif()
  if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "7.1" AND NOT APPLE AND NOT ${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wno-format-truncation)
  endif()
  if(UNIX)
    # -rdynamic is needed to allow obtaining meaningful backtraces
    target_link_options(${PROJECT_NAME} PRIVATE -rdynamic)
  endif()
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU") # gcc
    # required by pthread_setname_np()
    add_definitions(-D_GNU_SOURCE)
  endif()
endif()

if(WIN32)
  add_definitions(-DNOMINMAX)
  if(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup" )
  elseif(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows -lpthread")
  else()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows -lstdc++fs")
  endif()
elseif(IOS)
  enable_language(OBJCXX)
  set(CMAKE_OSX_DEPLOYMENT_TARGET 15.0)
elseif(APPLE)
  enable_language(OBJCXX)
endif()

if(IOS)
  file(GLOB_RECURSE ObjCSOURCES "common/*.mm")
elseif(APPLE)
  target_link_libraries(${PROJECT_NAME} "-framework AppKit")
  file(GLOB_RECURSE ObjCSOURCES "common/*.mm")
endif()

## OpenGothic

# sources
file(GLOB_RECURSE OPENGOTHIC_SOURCES "common/*.h" "common/*.cpp")
file(GLOB_RECURSE SPACER_SOURCES "spacer/*.h" "spacer/*.cpp")

target_sources(${PROJECT_NAME} PRIVATE ${OPENGOTHIC_SOURCES} ${ObjCSOURCES})

# shaders
add_subdirectory(shader)
target_link_libraries(${PROJECT_NAME} GothicShaders)

# editor icons and such
add_subdirectory(spacer/assets)

include_directories("common")

## submodule dependencies

# edd-dbg
if(WIN32)
  add_subdirectory(lib/edd-dbg)
  include_directories(lib/edd-dbg/include)
  target_link_libraries(${PROJECT_NAME} edd_dbg)
endif()

if(WIN32)
  target_link_libraries(${PROJECT_NAME} shlwapi DbgHelp)
elseif(ANDROID)
  target_link_libraries(${PROJECT_NAME} dl)
elseif(UNIX)
  target_link_libraries(${PROJECT_NAME} -lpthread -ldl)
endif()

# ZenKit
set(BUILD_SQUISH_WITH_SSE2     OFF CACHE INTERNAL "")
set(ZK_ENABLE_ASAN             OFF CACHE INTERNAL "")
set(ZK_ENABLE_DEPRECATION      OFF CACHE INTERNAL "")

add_subdirectory(lib/ZenKit)
target_link_libraries(${PROJECT_NAME} zenkit)

# dmusic
set(DM_BUILD_STATIC            ON  CACHE INTERNAL "")
set(DM_ENABLE_ASAN             OFF CACHE INTERNAL "")

add_subdirectory(lib/dmusic)
target_link_libraries(${PROJECT_NAME} dmusic)

# TinySoundFont
add_definitions(-DTSF_NO_STDIO)
include_directories(lib/TinySoundFont)

# Rapidjson
include_directories(lib/rapidjson/include)

# Tempest
if(WIN32)
  set(TEMPEST_BUILD_SHARED ON  CACHE INTERNAL "")
else()
  set(TEMPEST_BUILD_SHARED OFF CACHE INTERNAL "")
endif()
add_subdirectory(lib/Tempest/Engine)
include_directories(lib/Tempest/Engine/include)
target_link_libraries(${PROJECT_NAME} Tempest)

# zip
include_directories(lib/miniz)
add_definitions(-DMINIZ_NO_STDIO)
add_library(miniz STATIC EXCLUDE_FROM_ALL "lib/miniz/miniz.h" "lib/miniz/miniz.c")
target_link_libraries(${PROJECT_NAME} miniz)

# bullet physics
set(BULLET2_MULTITHREADING       ON  CACHE INTERNAL "")
set(USE_MSVC_RUNTIME_LIBRARY_DLL ON  CACHE INTERNAL "")
set(USE_GRAPHICAL_BENCHMARK      OFF CACHE INTERNAL "")
set(BUILD_BULLET2_DEMOS          OFF CACHE INTERNAL "") # No samples
set(BUILD_OPENGL3_DEMOS          OFF CACHE INTERNAL "") # No samples
set(BUILD_UNIT_TESTS             OFF CACHE INTERNAL "") # No tests
set(BUILD_CPU_DEMOS              OFF CACHE INTERNAL "") # No samples
set(BUILD_EXTRAS                 OFF CACHE INTERNAL "") # No bugs
set(GLFW_BUILD_EXAMPLES          OFF CACHE INTERNAL "")
set(GLFW_BUILD_TESTS             OFF CACHE INTERNAL "")
set(GLFW_BUILD_DOCS              OFF CACHE INTERNAL "")
set(BUILD_BULLET3                OFF CACHE INTERNAL "") # Can use bullet2, bullet3 wants to build examples...
add_subdirectory(lib/bullet3)
if(NOT MSVC)
  target_compile_options(BulletDynamics         PRIVATE -Wno-argument-outside-range)
  target_compile_options(BulletInverseDynamics  PRIVATE -Wno-argument-outside-range)
  target_compile_options(BulletSoftBody         PRIVATE -Wno-argument-outside-range)
  target_compile_options(Bullet3Common          PRIVATE -Wno-argument-outside-range)
  target_compile_options(BulletCollision        PRIVATE -Wno-argument-outside-range)
  target_compile_options(LinearMath             PRIVATE -Wno-argument-outside-range)
endif()
if(NOT MSVC AND NOT APPLE AND NOT ${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
  target_compile_options(BulletDynamics         PRIVATE -Wno-stringop-overflow)
  target_compile_options(BulletInverseDynamics  PRIVATE -Wno-stringop-overflow)
  target_compile_options(BulletSoftBody         PRIVATE -Wno-stringop-overflow)
  target_compile_options(Bullet3Common          PRIVATE -Wno-stringop-overflow)
  target_compile_options(BulletCollision        PRIVATE -Wno-stringop-overflow)
  target_compile_options(LinearMath             PRIVATE -Wno-stringop-overflow)
endif()

include_directories(lib/bullet3/src)
target_link_libraries(${PROJECT_NAME} BulletDynamics BulletCollision LinearMath)

# script for launching in binary directory
if(WIN32)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/scripts/Gothic2Notr.bat
        ${CMAKE_CURRENT_BINARY_DIR}/opengothic/Gothic2Notr.bat)
endif()

# in debug mode, enable sanitizers
if(${CMAKE_BUILD_TYPE} MATCHES "Debug" AND NOT ANDROID)
  add_compile_options(-fsanitize=address)
  add_compile_options(-fsanitize=leak)
  add_link_options   (-fsanitize=address)
  add_link_options   (-fsanitize=leak)
endif()

# the game
if(IOS)
  set_directory_properties("${CMAKE_SOURCE_DIR}" PROPERTIES
    PROPERTY XCODE_STARTUP_PROJECT "Gothic2Notr")
  set_target_properties(Gothic2Notr PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "Info.plist.in"
    MACOSX_BUNDLE_GUI_IDENTIFIER "opengothic.gothic2"
    MACOSX_BUNDLE_BUNDLE_VERSION 1
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "iPhone Developer")
endif()

target_sources(Gothic2Notr PRIVATE game/main.cpp)
if(WIN32)
  target_sources(Gothic2Notr PRIVATE icon.rc)
endif()
target_link_libraries(Gothic2Notr PRIVATE OpenGothic)

# editor
target_sources(Spacer PRIVATE ${SPACER_SOURCES} spacer/main.cpp)
target_include_directories(Spacer PRIVATE "spacer")
target_link_libraries(Spacer PRIVATE OpenGothic SpacerAssets)

# Optional controller diagnostics and portable regression tests (no game assets).
option(OPENGOTHIC_GAMEPAD_TOOLS "Build gamepad diagnostics and tests" OFF)
if(OPENGOTHIC_GAMEPAD_TOOLS)
  enable_testing()
  set(GAMEPAD_GTEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/Tempest/Tests/googletest/googletest")
  add_executable(GamepadStateTests
    lib/Tempest/Tests/tests/gamepad_test.cpp
    lib/Tempest/Engine/system/gamepadstate.cpp
    "${GAMEPAD_GTEST_DIR}/src/gtest-all.cc"
    "${GAMEPAD_GTEST_DIR}/src/gtest_main.cc")
  target_include_directories(GamepadStateTests PRIVATE
    lib/Tempest/Engine lib/Tempest/Engine/include
    "${GAMEPAD_GTEST_DIR}" "${GAMEPAD_GTEST_DIR}/include")
  find_package(Threads REQUIRED)
  target_link_libraries(GamepadStateTests PRIVATE Threads::Threads)
  add_test(NAME gamepad-state COMMAND GamepadStateTests)

  add_executable(GamepadInputTests
    tests/gamepadinput_test.cpp
    tests/gamepadsettings_test.cpp
    tests/gamepadsettingsmenu_test.cpp
    tests/gamepadtextinput_test.cpp
    tests/gamepadplayerinput_test.cpp
    tests/actioninput_test.cpp
    tests/gamepadinventoryinput_test.cpp
    tests/gamepadmenuinput_test.cpp
    tests/menuinput_test.cpp
    tests/string_frm_test.cpp
    common/utils/gamepadinput.cpp
    common/utils/gamepadsettings.cpp
    "${GAMEPAD_GTEST_DIR}/src/gtest-all.cc"
    "${GAMEPAD_GTEST_DIR}/src/gtest_main.cc")
  target_include_directories(GamepadInputTests PRIVATE
    common lib/Tempest/Engine/include
    "${GAMEPAD_GTEST_DIR}" "${GAMEPAD_GTEST_DIR}/include")
  target_link_libraries(GamepadInputTests PRIVATE Threads::Threads)
  add_test(NAME gamepad-input COMMAND GamepadInputTests)

  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    # Compile the real input methods against a small world boundary fixture.
    # Extract from the source so the regression cannot drift into a copied
    # routing implementation; changing that source regenerates this include.
    set(PLAYER_CONTROL_INPUT_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/common/game/playercontrol.cpp")
    set(PLAYER_CONTROL_NPC_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/common/world/objects/npc.cpp")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
      "${PLAYER_CONTROL_INPUT_SOURCE}" "${PLAYER_CONTROL_NPC_SOURCE}")
    file(READ "${PLAYER_CONTROL_INPUT_SOURCE}" PLAYER_CONTROL_INPUT_TEXT)
    file(READ "${PLAYER_CONTROL_NPC_SOURCE}" PLAYER_CONTROL_NPC_TEXT)
    set(PLAYER_CONTROL_INPUT_BODIES "")
    set(PLAYER_CONTROL_BOW_BODIES "")
    foreach(PART IN ITEMS input reset movement bow)
      set(METHOD_SOURCE "${PLAYER_CONTROL_INPUT_TEXT}")
      if(PART STREQUAL "input")
        set(BEGIN_MARKER "void PlayerControl::onKeyPressed(")
        set(END_MARKER "bool PlayerControl::isPressed(")
      elseif(PART STREQUAL "reset")
        set(BEGIN_MARKER "void PlayerControl::clearInput(")
        set(END_MARKER "void PlayerControl::marvinF8(")
      elseif(PART STREQUAL "movement")
        set(BEGIN_MARKER "void PlayerControl::implMove(")
        set(END_MARKER "void PlayerControl::implMoveMobsi(")
      else()
        set(METHOD_SOURCE "${PLAYER_CONTROL_NPC_TEXT}")
        set(BEGIN_MARKER "bool Npc::aimBow(")
        set(END_MARKER "bool Npc::isEnemy(")
      endif()
      string(FIND "${METHOD_SOURCE}" "${BEGIN_MARKER}" BEGIN_OFFSET)
      string(FIND "${METHOD_SOURCE}" "${END_MARKER}" END_OFFSET)
      if(BEGIN_OFFSET LESS 0 OR END_OFFSET LESS BEGIN_OFFSET)
        message(FATAL_ERROR "Cannot locate PlayerControl ${PART} methods for regression fixture")
      endif()
      math(EXPR METHOD_LENGTH "${END_OFFSET} - ${BEGIN_OFFSET}")
      string(SUBSTRING "${METHOD_SOURCE}" ${BEGIN_OFFSET} ${METHOD_LENGTH} METHOD_TEXT)
      if(PART STREQUAL "input" OR PART STREQUAL "reset")
        string(APPEND PLAYER_CONTROL_INPUT_BODIES "${METHOD_TEXT}\n")
      endif()
      string(APPEND PLAYER_CONTROL_BOW_BODIES "${METHOD_TEXT}\n")
    endforeach()
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/playercontrol-input-bodies.inc" "${PLAYER_CONTROL_INPUT_BODIES}")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/playercontrol-bow-bodies.inc" "${PLAYER_CONTROL_BOW_BODIES}")
    add_executable(PlayerControlInputTests
      tests/playercontrolinput_test.cpp
      "${GAMEPAD_GTEST_DIR}/src/gtest-all.cc"
      "${GAMEPAD_GTEST_DIR}/src/gtest_main.cc")
    target_include_directories(PlayerControlInputTests PRIVATE
      "${CMAKE_CURRENT_BINARY_DIR}/generated"
      "${GAMEPAD_GTEST_DIR}" "${GAMEPAD_GTEST_DIR}/include")
    target_compile_options(PlayerControlInputTests PRIVATE -fno-access-control)
    target_link_libraries(PlayerControlInputTests PRIVATE Threads::Threads)
    add_test(NAME gamepad-player-control COMMAND PlayerControlInputTests)

    add_executable(PlayerControlBowTests
      tests/playercontrolbow_test.cpp
      "${GAMEPAD_GTEST_DIR}/src/gtest-all.cc"
      "${GAMEPAD_GTEST_DIR}/src/gtest_main.cc")
    target_include_directories(PlayerControlBowTests PRIVATE
      "${CMAKE_CURRENT_BINARY_DIR}/generated"
      "${GAMEPAD_GTEST_DIR}" "${GAMEPAD_GTEST_DIR}/include")
    target_compile_options(PlayerControlBowTests PRIVATE -fno-access-control)
    target_link_libraries(PlayerControlBowTests PRIVATE Threads::Threads)
    add_test(NAME gamepad-player-bow COMMAND PlayerControlBowTests)

    # Include the actual private dialogs and exercise close/queued input before
    # Dialog::exec unwinds. Access relaxation applies only to this test target.
    add_executable(GamepadDialogTests
      tests/gamepaddialog_test.cpp
      tests/gamepadsettingsdialog_test.cpp
      tests/gthfont_test.cpp
      tests/gamepadmodalredraw_test.cpp
      "${GAMEPAD_GTEST_DIR}/src/gtest-all.cc")
    target_include_directories(GamepadDialogTests PRIVATE
      "${GAMEPAD_GTEST_DIR}" "${GAMEPAD_GTEST_DIR}/include")
    target_compile_options(GamepadDialogTests PRIVATE -fno-access-control)
    target_link_libraries(GamepadDialogTests PRIVATE OpenGothic Threads::Threads)
    add_test(NAME gamepad-dialog-lifecycle COMMAND GamepadDialogTests)
    set_tests_properties(gamepad-dialog-lifecycle PROPERTIES TIMEOUT 20)
  endif()

  # Exercise the actual upload thread with deterministic scheduling, without
  # constructing a GPU device. Normal builds omit the scheduling hook.
  add_executable(InstanceStorageTests
    tests/instancestorage_test.cpp
    common/graphics/instancestorage.cpp
    "${GAMEPAD_GTEST_DIR}/src/gtest-all.cc"
    "${GAMEPAD_GTEST_DIR}/src/gtest_main.cc")
  target_compile_definitions(InstanceStorageTests PRIVATE OPENGOTHIC_INSTANCE_STORAGE_TEST_HOOK)
  target_include_directories(InstanceStorageTests PRIVATE
    "${GAMEPAD_GTEST_DIR}" "${GAMEPAD_GTEST_DIR}/include")
  target_link_libraries(InstanceStorageTests PRIVATE OpenGothic Threads::Threads)
  add_test(NAME instance-upload COMMAND InstanceStorageTests)
  set_tests_properties(instance-upload PROPERTIES TIMEOUT 20)

  if(APPLE AND NOT IOS)
    # Exercise synchronous menu dialogs entered after a real native/GCD frame
    # callback returns. The watchdog bounds regressions in main-queue liveness.
    add_executable(GamepadMenuLoopTests
      tests/gamepadmenuloop_test.mm
      tests/gamepadmenuloop_native.mm)
    target_compile_options(GamepadMenuLoopTests PRIVATE -fno-access-control)
    target_link_libraries(GamepadMenuLoopTests PRIVATE OpenGothic
      "-framework AppKit" "-framework CoreFoundation")
    add_test(NAME gamepad-menu-native-loop COMMAND GamepadMenuLoopTests)
    set_tests_properties(gamepad-menu-native-loop PROPERTIES TIMEOUT 20)

    # This fixture includes the real backend and exercises saved ObjC blocks
    # after their registration scope and backend lifetime have both ended.
    add_executable(MacOSGamepadTests
      lib/Tempest/Tests/tests/macosgamepad_test.mm
      lib/Tempest/Engine/system/gamepadstate.cpp)
    target_include_directories(MacOSGamepadTests PRIVATE lib/Tempest/Engine/include)
    target_link_libraries(MacOSGamepadTests PRIVATE
      "-framework AppKit" "-framework GameController")
    target_compile_options(MacOSGamepadTests PRIVATE -Wall -Wextra -Werror)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      target_compile_options(MacOSGamepadTests PRIVATE -fsanitize=address -fno-omit-frame-pointer)
      target_link_options(MacOSGamepadTests PRIVATE -fsanitize=address)
    endif()
    add_test(NAME gamepad-macos-lifetime COMMAND MacOSGamepadTests)
    set_tests_properties(gamepad-macos-lifetime PROPERTIES
      ENVIRONMENT "ASAN_OPTIONS=detect_stack_use_after_return=1")
  endif()

  if(NOT IOS AND NOT ANDROID)
    add_executable(TempestGamepadProbe scripts/tempest-gamepad-probe.cpp)
    target_link_libraries(TempestGamepadProbe PRIVATE Tempest)
    if(NOT MSVC)
      target_compile_options(TempestGamepadProbe PRIVATE -Wall -Wextra -Werror)
    endif()
  endif()
endif()

if(ANDROID)
  add_android_apk(OpenGothic-apk
    PACKAGE_NAME org.opengothic.app
    CODE Gothic2Notr
    MANIFEST android/AndroidManifest.xml)
endif()

# installation
install(TARGETS Gothic2Notr Spacer DESTINATION bin)
