# Per-key FX light-range override disables the preset's range flicker

**Confidence:** Medium (code-confirmed divergence; real-world impact is data-dependent — only bites flickering light presets that also receive a per-key `lightRange`). Fix is **DEFERRED** (not a one-liner — see reason).

## Original function + address (prose)
When an `oCVisualFX` applies a per-emitter-key light-range override it calls
`zCVobLight::SetRange` @`0x00608320` with the second argument (`bAdjustBase`) equal to `1`:

- `oCVisualFX::UpdateFXByEmitterKey` @`0x0048e29c` — for a non-zero key `lightRange` (field at
  `key+0xf8`) it does `zCVobLight::SetRange(light, key.lightRange, 1)`.
- `oCVisualFX::SetByScript` @`0x0048dbf1` — base-FX path, same `SetRange(..., 1)` shape after
  `zCVobLight::SetByPreset`.

`zCVobLight::SetRange(float range, int bAdjustBase)` @`0x00608320` does **not** touch the
range-animation state. It clamps `range` to `>= 0`, writes the current range (`this+0x144`), writes
`1/range` (`this+0x148`), and — only because `bAdjustBase==1` — writes the **base/reference** range
(`this+0x14c`) and re-fits the bbox. The `rangeAniScale`/`rangeAniFPS`/`rangeAniSmooth` flicker
machinery is left intact, so a preset whose range pulses (e.g. fire/magic lights) keeps pulsing —
just rescaled around the new base. The override **rescales** the flicker; it does not stop it.

The per-key override is also guarded by `lightRange != 0` (the Ghidra `NAN(x)==(x==0)` idiom),
i.e. any non-zero value, including negative (which `SetRange` then clamps to 0).

## OG file:line
- `game/graphics/effect.cpp:48-49`
  ```cpp
  if(key!=nullptr && key->lightRange>0)
    light.setRange(key->lightRange);
  ```
- `game/graphics/lightsource.cpp:79-83`
  ```cpp
  void LightSource::setRange(float r) {
    rgn            = r;
    curRgn         = r;
    rangeAniFPSInv = 0;   // <-- disables the range-flicker animation
    }
  ```

## Divergence
`Effect::setupLight` routes the per-key override through the **scalar** `LightSource::setRange(float)`
overload, which sets `rangeAniFPSInv = 0` and thereby **deletes the preset's range-flicker
animation**, turning a pulsing FX light into a steady one. The original
(`zCVobLight::SetRange(r, /*bAdjustBase=*/1)`) keeps the animation and only rescales its base range.

Net effect: any spell / fire / magic VisualFx whose light preset carries a `rangeAniScale` list and
that also supplies a per-key `lightRange` loses its range flicker in OpenGothic. Secondary, minor:
OG gates the override on `lightRange > 0` whereas the original uses `lightRange != 0` (negligible —
negative ranges do not occur in shipped FX scripts and the original clamps them to 0 anyway).

## Proposed patch — DEFERRED
A faithful fix is not surgical. To match `SetRange(r, bAdjustBase=1)`, OpenGothic must **rescale the
existing range animation** rather than replace it: keep `rangeAniScale`, `rangeAniFPSInv`,
`rangeSmooth`, and multiply each `rangeAniScale[i]` by `r / oldBase`. But OG bakes the base into
`rangeAniScale` at setup (`LightSource::setRange(list, base, …)` does `i *= base`, lightsource.cpp:91-93)
and does not retain `oldBase`, so the change spans `LightSource` (store base + add a
`rescaleRange(float)` that preserves the animation), `LightGroup::Light` (expose it), and
`Effect::setupLight`. That is beyond a high-confidence one-line edit, so it is deferred.

If only the trivial guard is desired (does **not** fix the flicker loss):
```cpp
// OLD (game/graphics/effect.cpp:48)
  if(key!=nullptr && key->lightRange>0)
// NEW
  // NOTE: in original-game oCVisualFX::UpdateFXByEmitterKey @0x0048e29c the per-key light-range
  // override is applied for any non-zero key.lightRange (key+0xf8 != 0), then zCVobLight::SetRange
  // @0x00608320 clamps negatives to 0.
  if(key!=nullptr && key->lightRange!=0)
```
