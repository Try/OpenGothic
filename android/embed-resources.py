#!/usr/bin/env python3
"""Embeds shaders and fonts into C++ sources, for the Android build of build-apk.sh.

Usage: embed-resources.py <repo> <build>

The data is written in short lines to a temporary file, which replaces the result at once.
Very long generated files are not written reliably by CMake file(APPEND) on some filesystems.
"""
from pathlib import Path
import os
import sys


def embed(files, output, struct, font=False):
  files = sorted(files)
  if not files:
    raise RuntimeError('No resources to embed')
  output.mkdir(parents=True, exist_ok=True)

  stem  = 'builtin_fonts' if font else 'shader'
  names = [p.name.replace('.', '_').replace('-', '_') for p in files]
  if len(set(names)) != len(names):
    raise RuntimeError('Duplicate resource identifier')

  header = [
    '#pragma once',
    '#include <cstdint>',
    '#include <cstring>',
    '#include <string>',
    '#include <string_view>',
    '#include <stdexcept>',
    f'struct {struct} {{',
    '  const char* data;',
    '  size_t len;',
    ]
  header.extend(f'  static const {struct} {name};' for name in names)

  arg = 'const char* name' if font else 'std::string_view name'
  header.append(f'  static {struct} get({arg}) {{')
  for p, name in zip(files, names):
    condition = f'std::strcmp(name,"{p.name}")==0' if font else f'name=="{p.name}"'
    header.append(f'    if({condition}) return {name};')
  header.extend(['    throw std::runtime_error("embedded resource missing");', '  }', '};', ''])

  source = output / (stem + '.cpp.tmp')
  with source.open('w') as out:
    out.write(f'#include "{stem}.h"\n')
    for p, name in zip(files, names):
      data = p.read_bytes()
      if not data:
        raise RuntimeError(f'Empty resource: {p}')
      out.write(f'alignas(uint32_t) static const uint8_t SRC_{name}[] = {{\n')
      for pos in range(0, len(data), 32):
        out.write(','.join(f'0x{x:02x}' for x in data[pos:pos + 32]) + ',\n')
      out.write('};\n')
      out.write(f'const {struct} {struct}::{name} = '
                f'{{reinterpret_cast<const char*>(SRC_{name}),sizeof(SRC_{name})}};\n')
    out.flush()
    os.fsync(out.fileno())

  header_tmp = output / (stem + '.h.tmp')
  header_tmp.write_text('\n'.join(header))
  os.replace(header_tmp, output / (stem + '.h'))
  os.replace(source, output / (stem + '.cpp'))
  print(f'Embedded {len(files)} resources in {output / stem}')


if __name__ == '__main__':
  repo  = Path(sys.argv[1])
  build = Path(sys.argv[2])
  embed((build / 'shader/sprv').glob('*.sprv'), build / 'shader/sprv', 'GothicShader')
  embed((repo / 'lib/Tempest/Engine/fonts').glob('*.ttf'), build / 'lib/Tempest/Engine/fonts', 'AppFonts', True)
