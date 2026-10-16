# Sprawdzenie clang -Wconversion -Werror (jak macOS w CI) dla podanych plików .cpp.
# Flagi z build/compile_commands.json. Użycie: python3 dev/clangcheck.py common/plik.cpp ...
import json, shlex, subprocess, sys, os
R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
db = {os.path.realpath(os.path.join(e['directory'], e['file'])): e
      for e in json.load(open(R + '/build/compile_commands.json'))}
bad = 0
for f in sys.argv[1:]:
    e = db.get(os.path.realpath(os.path.join(R, f)))
    if not e:
        print('pominięto (nie w buildzie):', f); continue
    a = shlex.split(e['command'])[1:]
    out, skip = [], False
    for x in a:
        if skip: skip = False; continue
        if x in ('-o', '-c'): skip = True; continue
        if x.startswith('-O') or x == '-g': continue
        out.append(x)
    r = subprocess.run(['clang++', '-fsyntax-only', '-Wconversion', '-Werror',
                        '-Wno-unknown-warning-option', *out, e['file']],
                       cwd=e['directory'], capture_output=True, text=True)
    errs = [l for l in r.stderr.splitlines() if 'error' in l]
    if r.returncode:
        bad += 1; print('BŁĄD', f); print('\n'.join(errs[:8]))
print('clang: OK' if not bad else f'clang: {bad} plik(ów) z błędami')
sys.exit(1 if bad else 0)
