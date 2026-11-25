# Contributing to OpenGothic

OpenGothic development focuses on Gothic II: Night of the Raven. Fixes for Gothic I and mods are welcome. When reporting a bug or testing a change, specify which game and mod versions you used.

## Project layout

| Path | Contents |
| --- | --- |
| `game/` | Game executable entry point for `Gothic2Notr`. |
| `common/` | Shared engine code, including gameplay, world handling, rendering, physics, audio, and UI. |
| `shader/` | GLSL shaders and shader build rules. |
| `spacer/` | `Spacer` editor, including its UI, project handling, and assets. |
| `lib/` | Dependencies, including Tempest for graphics and platform support, Bullet for physics, and ZenKit for Gothic data and Daedalus. See [`.gitmodules`](.gitmodules) for the submodule list. |

## Build and verify

Initialize dependencies in an existing checkout, then configure and build from the repository root:

```sh
git submodule update --init --recursive
cmake -S . -B build/dev -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/dev --config RelWithDebInfo --target Gothic2Notr Spacer --parallel
```

See the [README build instructions](README.md#build-instructions) for platform prerequisites. For the Windows setup used in CI, see the [build workflow](.github/workflows/build.yml), which uses MSYS2 MINGW64, Ninja, glslang, and the Vulkan SDK. Use `-G Ninja` on the configure command when following that setup.

Compilation does not require the original Gothic game files. To verify gameplay in OpenGothic, use a local Gothic installation and pass its path with `-g` if needed. See the [command-line arguments](README.md#command-line-arguments) for options such as selecting a world, loading a save, or forcing DirectX 12 on Windows.

Build the affected target and build both targets for changes to shared engine code. For behavior changes, reproduce the issue and check the fix in the relevant game or editor scenario. In the pull request, record the build and runtime checks you ran and any checks you could not run.

## Code guidelines

- Keep patches small and focused on a single bug or feature. Submit unrelated cleanup and formatting changes separately.
- Write compact, readable code. Reserve comments for deliberate quirks and workarounds, such as reproducing an original game bug for compatibility or handling a graphics driver issue. Explain why the code is needed.
- Declare data members `const` when they are initialized by the constructor and never reassigned.
- Always initialize variables of non-class types. Initialize local variables at their declaration and data members at their declaration or in the constructor initializer list.

## C++ formatting

Use [`.clang-format`](.clang-format) with clang-format 21 or newer. To preview formatting, run `clang-format --style=file path/to/file.cpp` from the repository root. Review the output and keep formatting changes within the code you edit.

- Use OpenGothic's Ratliff-style-like formatting for new and edited code. Use two spaces per indentation level. Put the opening brace on the same line as the declaration or control statement. Indent the closing brace to the same level as the block body, including for nested blocks.
- In classes, indent access labels (`public:`, `protected:`, `private:`) one level and member declarations two levels from the `class` line.
- Format groups of related variable declarations as a table. Use spaces to align variable names and, when several declarations use `=`, align the `=` signs. Apply this to local variables and data members. Keep alignment within each group.
- The formatter uses Whitesmiths indentation and skips lines containing an opening brace to preserve its placement. Check class member and method body indentation after formatting and adjust it by hand where needed.

```cpp
#include "header.h"

#include <Tempest/Widget>
#include <string>

#include "resources.h"

using namespace Tempest;

void updateViewport() {
  const int  width   = viewport.width();
  const int  height  = viewport.height();
  const bool resized = width != oldWidth || height != oldHeight;

  if(resized) {
    resize(width, height);
    }
  }
```

## Reverse engineering and original-game testing

Do not reverse-engineer the original Gothic executable or dynamicaly loaded libraries.

Do not peek into original Gothic executable or dynamicaly loaded libraries binary data.

Testing and instrumenting the original game through mods is welcome. Use zSpy, add `PrintDebug` calls to Daedalus scripts, or create a small test mod. Include reproduction steps and observations when they explain a compatibility change.

## Issues and feature requests

Use the [issue templates](https://github.com/Try/OpenGothic/issues/new/choose) to report bugs, mod issues, and feature requests. For bugs, include the OpenGothic build or commit, platform, steps to reproduce, and expected and actual behavior. For rendering issues, include the GPU and graphics backend. Attach a save, `log.txt`, `crash.log`, or screenshots if they help reproduce the issue.

The wiki also has a [feature list](https://github.com/Try/OpenGothic/wiki/Feature-list) and [Daedalus script API notes](https://github.com/Try/OpenGothic/wiki/Daedalus-scripts).
