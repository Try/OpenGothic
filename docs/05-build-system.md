# OpenGothic Engine - Build System & Dependencies

## Build System

**Type**: CMake 3.16+
**Language**: C++20
**File**: `CMakeLists.txt`

## Dependencies

### Core Libraries

| Library | Version | Purpose | Integration |
|---------|---------|---------|-------------|
| **ZenKit** | Latest | Gothic asset parsing | Submodule |
| **Tempest** | Latest | Graphics abstraction (Vulkan/DX12/Metal) | Submodule |
| **Bullet Physics** | 2.x | Physics simulation | Submodule (Bullet2 components) |
| **dmusic** | Custom | DirectMusic parsing | Submodule |
| **TinySoundFont** | Custom | SF2 synthesis | Submodule |
| **miniz** | 2.x | ZIP compression | Submodule |
| **edd-dbg** | Custom | Debug/crash reporting (Windows) | Submodule |

### System Dependencies

**Linux**:
```bash
# Ubuntu/Debian
sudo apt install git cmake g++ glslang-tools libvulkan-dev libasound2-dev libx11-dev libxcursor-dev

# Arch
sudo pacman -S git cmake gcc glslang vulkan-devel alsa-lib libx11 libxcursor

# Fedora
sudo dnf install git cmake gcc-c++ glslang vulkan-loader-devel alsa-lib-devel libX11-devel libXcursor-devel
```

**macOS**:
```bash
brew install glslang
```

**Windows**:
- Visual Studio 2019+ or Clang
- Vulkan SDK (optional, bundled drivers work)

## Build Configuration

### Compiler Settings

```cmake
set(CMAKE_CXX_STANDARD 20)
set(BUILD_SHARED_LIBS OFF)  # Static linking
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
```

### Platform-Specific Flags

**MSVC**:
```cmake
add_definitions(-D_USE_MATH_DEFINES)
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-D_ITERATOR_DEBUG_LEVEL=0)
add_definitions(-DNOMINMAX)
```

**GCC/Clang**:
```cmake
target_compile_options(Gothic2Notr PRIVATE
  -Wall -Wconversion -Wno-strict-aliasing -Werror
)
```

**UNIX**:
```cmake
target_link_options(Gothic2Notr PRIVATE -rdynamic)  # For backtraces
```

### Sanitizers (Debug Mode)

```cmake
if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
  add_compile_options(-fsanitize=address -fsanitize=leak)
  add_link_options(-fsanitize=address -fsanitize=leak)
endif()
```

## Build Instructions

### Linux

```bash
git clone --recurse-submodules https://github.com/Try/OpenGothic.git
cd OpenGothic
cmake -B build -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE:STRING=RelWithDebInfo
make -C build -j $(nproc)
```

### macOS

```bash
git clone --recurse-submodules https://github.com/Try/OpenGothic.git
cd OpenGothic
cmake -H. -Bbuild -DCMAKE_BUILD_TYPE:STRING=RelWithDebInfo
cmake --build ./build --target Gothic2Notr
```

### Windows

```bash
git clone --recurse-submodules https://github.com/Try/OpenGothic.git
cd OpenGothic
cmake -B build -DCMAKE_BUILD_TYPE:STRING=RelWithDebInfo
cmake --build build --config RelWithDebInfo
```

## Submodule Configuration

### Tempest Engine

```cmake
set(TEMPEST_BUILD_SHARED ON CACHE INTERNAL "")
add_subdirectory(lib/Tempest/Engine)
include_directories(lib/Tempest/Engine/include)
target_link_libraries(Gothic2Notr Tempest)
```

### ZenKit

```cmake
set(BUILD_SQUISH_WITH_SSE2 OFF CACHE INTERNAL "")
set(ZK_ENABLE_ASAN OFF CACHE INTERNAL "")
set(ZK_ENABLE_DEPRECATION OFF CACHE INTERNAL "")
add_subdirectory(lib/ZenKit)
target_link_libraries(Gothic2Notr zenkit)
```

### Bullet Physics

```cmake
set(BULLET2_MULTITHREADING ON CACHE INTERNAL "")
set(BUILD_BULLET2_DEMOS OFF CACHE INTERNAL "")
set(BUILD_UNIT_TESTS OFF CACHE INTERNAL "")
set(BUILD_BULLET3 OFF CACHE INTERNAL "")
add_subdirectory(lib/bullet3)
target_link_libraries(Gothic2Notr BulletDynamics BulletCollision LinearMath)
```

### dmusic

```cmake
set(DM_BUILD_STATIC ON CACHE INTERNAL "")
set(DM_ENABLE_ASAN OFF CACHE INTERNAL "")
add_subdirectory(lib/dmusic)
target_link_libraries(Gothic2Notr dmusic)
```

## Shader Compilation

**Subdirectory**: `shader/CMakeLists.txt`

**Process**:
1. Collect all `.vert`, `.frag`, `.comp`, `.mesh`, `.task` files
2. Compile to SPIR-V using `glslangValidator`
3. Package into `GothicShaders` library
4. Link with main executable

```cmake
add_subdirectory(shader)
target_link_libraries(Gothic2Notr GothicShaders)
```

## CI/CD

**Platform**: AppVeyor

**Configuration**: `.appveyor.yml`

**Builds**:
- Windows (MSVC)
- Linux (GCC)
- macOS (Clang)

**Artifacts**:
- Pre-compiled binaries
- Shader SPIR-V bundles
- Launch scripts

## Output Structure

```
build/opengothic/
├── Gothic2Notr(.exe)      - Main executable
├── Gothic2Notr.sh/.bat    - Launch script
├── Tempest.dll/.so        - Graphics engine (if shared)
└── shaders/*.spirv        - Compiled shaders
```

## Distribution

**Release Package**:
```
OpenGothic-v1.0/
├── Gothic2Notr.exe
├── Gothic2Notr.bat
├── Tempest.dll
├── shaders/
└── README.md
```

**Installation**:
1. Extract to any directory
2. Run `Gothic2Notr.exe` (auto-detects Gothic path)
3. Or use `-g` flag: `Gothic2Notr.exe -g "C:\Gothic2"`

## Key Features

- ✅ Cross-platform CMake
- ✅ Automated shader compilation
- ✅ Submodule management
- ✅ Debug sanitizers
- ✅ CI/CD automation
- ✅ Portable releases

---

**Next**: See `06-feature-matrix.md` for comprehensive feature comparison.
