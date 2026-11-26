# OpenGothic contributor instructions

## Contributions

- Read [CONTRIBUTING.md](CONTRIBUTING.md) before editing, including the [C++ formatting rules and example](CONTRIBUTING.md#c-formatting).
- Keep patches small and focused on a single bug or feature.
- Prefer private nested types for helper classes used by a single parent class.
- In UI code, include the headers for the UI controls you use directly.
- Write compact, readable code. Reserve comments for deliberate quirks and workarounds, such as reproducing an original game bug for compatibility or handling a graphics driver issue. Explain why the code is needed.
- Do not reverse-engineer the original Gothic executable or its dynamically loaded libraries. Do not inspect the binary data of the executable or those libraries.
- Testing and instrumenting the original game through mods is welcome, including using zSpy and adding `PrintDebug` calls to Daedalus scripts.

## Using clang-format

- Use clang-format 21 or newer on new and edited C++ code. From the repository root, preview the affected lines with `clang-format --style=file --lines=<start>:<end> path/to/file.cpp`. The root [`.clang-format`](.clang-format) aligns related declarations and uses Whitesmiths to indent closing braces. It skips lines containing an opening brace to preserve its placement.
- Review the preview before applying it. Keep formatting changes within the code you edit, since a line range can affect surrounding lines. Check access labels, class members, and method bodies against the [C++ formatting rules](CONTRIBUTING.md#c-formatting), and correct any indentation drift by hand.
- Use braces when an `if` condition spans multiple lines. If any branch of an `if`/`else if`/`else` chain uses braces, use braces on every branch.
- Align a standalone scope's closing brace with its opening brace. Correct this by hand if clang-format indents the closing brace.

## Exe isolation

- Keep agent executables separate from user builds. Use distinct executable names, such as `Spacer-codex.exe` for agent builds and `Spacer.exe` for user builds.
