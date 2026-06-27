# Armor body mesh ignores C_ITEM.visual_skin texture variant

**Confidence:** Medium-High (divergence is certain; the field is provably unused in OpenGothic. The exact surgical fix is a faithful approximation of a node-grouping the original keeps but OpenGothic flattened.)

## Original function + address

`oCNpc::SetAdditionalVisuals` (entry `0x738350`) is the C++ target of the
`Mdl_SetVisualBody` external handler (`0x6f99e0`, source tag
`oGameExternal.cpp`). It stores the body/head mesh names and the
bodyTex/skinColor/headTex/teethTex indices into the NPC, optionally creates and
equips the `armorInst` item, then calls `oCNpc::InitModel` (entry `0x738480`,
source tag `oNpc.cpp`).

`InitModel` builds the body model. When a torso-slot item is equipped
(`GetSlotItem(NPC_NODE_TORSO)`), it takes the armor's `visual_change` string as
the body mesh name, then applies three mesh-lib texture channels:

- mesh-lib node **"BODY"**, channel 0 = the NPC's body-texture index
  (the bit-packed field written by `SetAdditionalVisuals`, i.e. `bodyTexNr`).
- mesh-lib node **"BODY"**, channel 1 = the NPC's skin color (`bodyTexColor`).
- mesh-lib node **"ARMOR"**, channel 0 = the equipped item's `visual_skin`
  field (read from the item at offset `+0x248`, the int sitting just past the
  item's `visual_change` string).

So the armor's own geometry ("ARMOR" node) is textured with the item's
`visual_skin` variant, while only the exposed-skin geometry ("BODY" node) uses
the wearer's body-texture/skin-color indices. `visual_skin` is the documented
C_ITEM knob that selects an armor's texture variant (e.g. the different-colored
militia/mercenary armor tiers) independently of the wearer's skin tone.

## OpenGothic file:line

`game/world/objects/npc.cpp:913` (`Npc::updateArmor`, armor branch).

## Divergence

OpenGothic never reads `IItem::visual_skin` for rendering — a repo-wide grep
finds it only in `game/world/objects/item.cpp:55,125` (serialization). In
`Npc::updateArmor`, the equipped-armor mesh is built with:

```
w.addView(asc, vColor, 0, bdColor)   // texVar = vColor (NPC body-tex index)
```

`vColor` is the NPC's body-texture number (`Mdl_SetVisualBody` `bodyTexNr`),
which the original applies only to the "BODY" (skin) node — never to the armor
geometry. The original applies the item's `visual_skin` to the "ARMOR" node.
Because OpenGothic's `addView` substitutes a single `texVar` into every
`V`/`C`-named submesh texture (`MeshObjects::implGet`/`solveTex`), every
multi-variant armor renders at the wearer's body-tex index (usually 0) instead
of the variant chosen by `visual_skin`. Armors that ship multiple texture
variants therefore all look like variant 0.

## Proposed patch

Apply the equipped armor's `visual_skin` as the armor-mesh texture variant
(matching the original's "ARMOR" node channel-0 source). `IItem::visual_skin`
is grep-verified to exist (`lib/ZenKit/include/zenkit/addon/daedalus.hh:300`,
used at `game/world/objects/item.cpp:55`).

`game/world/objects/npc.cpp` — `Npc::updateArmor`, armor branch:

OLD:
```cpp
    if(flag & ITM_CAT_ARMOR){
      auto& asc   = itData.visual_change;
      auto  vbody = asc.empty() ? MeshObjects::Mesh() : w.addView(asc,vColor,0,bdColor);
      visual.setArmor(*this,std::move(vbody));
      }
```

NEW:
```cpp
    if(flag & ITM_CAT_ARMOR){
      // NOTE: in original-game oCNpc::InitModel @0x738480 the equipped armor's
      // mesh-lib "ARMOR" node is textured with the item's visual_skin variant
      // (oCItem+0x248), not the wearer's body-texture index (vColor).
      auto& asc   = itData.visual_change;
      auto  vbody = asc.empty() ? MeshObjects::Mesh() : w.addView(asc,itData.visual_skin,0,bdColor);
      visual.setArmor(*this,std::move(vbody));
      }
```

Caveat (does not block the fix): the original keeps two distinct mesh-lib nodes
("BODY" = wearer body-tex, "ARMOR" = `visual_skin`) that OpenGothic's flattened
single-`texVar` mesh cannot separate. Using `visual_skin` for the whole armor
mesh is strictly closer to the original than ignoring the field, and matches how
multi-variant armor ASCs are authored (the variant index drives all of the
armor's `V`/`C` textures). Exposed-skin submeshes inside an armor ASC, if any,
would shift from `vColor` to `visual_skin`; this is the only fidelity gap and is
negligible for stock content where `visual_skin` is the intended armor variant.
