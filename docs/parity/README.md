# OpenGothic parity map

A living map between the **original Gothic II engine** (`Gothic2.exe`, ZenGin) and
**OpenGothic's** reimplementation, cross-linked to the GitHub issue tracker, plus a
repeatable loop for driving 1:1 parity.

OpenGothic is a *clean-room reimplementation*, not a decompilation: of the ~32.6k
functions in `Gothic2.exe`, only ~9.4k live in ZenGin gameplay/engine classes
(`oC*`/`zC*`/`oG*`); the rest are CRT/STL, Direct3D7, Miles Sound System and Bink —
subsystems OpenGothic *replaces* (Tempest / ZenKit / dmusic). So the map is
**class/subsystem-level**, not exact-function-level.

## Files

| File | What |
|---|---|
| `name_map.json` | **Hand-edited seed** — the iteration surface. ZenGin class → OpenGothic class(es), grouped by subsystem, with the GitHub issue buckets each subsystem owns. |
| `PARITY_MAP.md` | **Generated.** Per subsystem: linked issues + a table (ZenGin class, binary fn count, OG class/methods/files, status) + an "unmapped ZenGin classes" backlog. |
| `parity_map.json` | **Generated.** Structured form for tooling/automation. |
| `findings/issue-<n>.md` | Per-issue parity analyses (decompiled original behavior vs OG, with a proposed fix). |

Bulky raw dumps live outside the repo in `~/gothic-re/out/`
(`functions.json`, `opengothic_classes.json`, `issues.json`).

## Regenerate

The generators live in the RE workspace `~/gothic-re/scripts/`. Run in order
(the function dump needs the Ghidra project lock, so stop `wde` first):

```sh
# 1. binary function inventory (Ghidra headless; ~30s; needs the project unlocked)
~/gothic-re/wde stop
~/wc3-re/ghidra_11.3.2_PUBLIC/support/analyzeHeadless ~/gothic-re/proj gothic \
  -process "Gothic2.exe" -noanalysis -scriptPath ~/gothic-re/scripts \
  -postScript dump_functions.py

# 2. OpenGothic class surface + GitHub issues (independent)
python3 ~/gothic-re/scripts/dump_opengothic.py
python3 ~/gothic-re/scripts/fetch_issues.py        # public REST API; uses gh if authed

# 3. join into the curated map (re-run this alone after editing name_map.json)
python3 ~/gothic-re/scripts/build_parity_map.py
```

`build_parity_map.py` is idempotent: edit `name_map.json` (map a backlog class,
split a subsystem, retarget issue buckets) and re-run step 3 to refine the map.

## The parity loop (per issue)

1. Find the issue's subsystem in `PARITY_MAP.md`; note the OG class+files and the
   paired ZenGin class.
2. `~/gothic-re/wde start`, then decompile the original routine(s):
   `~/gothic-re/wde dec <addr>` / `xrefs` / `strings`. List a class's methods with
   `python3 -c "import json;[print(f['entry'],f['name']) for f in json.load(open('$HOME/gothic-re/out/functions.json')) if (f.get('namespace') or '')=='oCNpc']"`.
3. Read the OpenGothic code, identify the divergence, and reimplement the
   *understood behavior*. Cite it: `// NOTE: in original-game <fn> @<addr> ...`.
4. Capture the analysis in `findings/issue-<n>.md`. `~/gothic-re/wde stop` when done.

**Clean-room rule (hard):** read the decompiler to *understand* behavior, then
reimplement. **Never paste decompiled code** into OpenGothic or these docs.

`[G1]`-tagged issues are Gothic 1 and are **not** analyzable against our Gothic 2
binary — they're flagged in the map but need `Gothic1.exe` imported separately.

Full RE-toolchain setup: `~/gothic-re/SETUP.md`.
