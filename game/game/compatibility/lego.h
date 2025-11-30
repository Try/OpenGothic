#pragma once

#include <zenkit/DaedalusVm.hh>

#include "scriptplugin.h"
#include "mem32instances.h"
#include "ikarus.h"

class GameScript;

class LeGo : public ScriptPlugin {
  public:
    LeGo(GameScript& owner, Ikarus& ikarus, zenkit::DaedalusVm& vm);

    static bool isRequired(zenkit::DaedalusVm& vm);

  private:
    void tick(uint64_t dt) override;
    void eventPlayAni(std::string_view ani) override;

    using ptr32_t = Compatibility::ptr32_t;
    using zString = Compatibility::zString;

    std::string SB_toString();

    // ## UI
    struct zCView;
    struct oCViewStatusBar;

    void        zCView__zCView(ptr32_t ptr, int x1, int y1, int x2, int y2);
    void        zCView__Open(ptr32_t ptr);
    void        zCView__Close(ptr32_t ptr);
    void        zCView__Top(ptr32_t ptr);
    void        zCView__SetSize(ptr32_t ptr, int w, int h);
    void        zCView__Move(ptr32_t ptr, int x, int y);
    void        zCView__InsertBack(ptr32_t ptr, std::string_view img);

    // ## Textures
    int         zCTexture__Load(std::string_view img, int flag);

    // ## Font
    struct zCFontMan;
    int         zCFontMan__Load(ptr32_t ptr, std::string_view font);
    int         zCFontMan__GetFont(ptr32_t ptr, int handle);

    GameScript&         owner;
    Ikarus&             ikarus;
    zenkit::DaedalusVm& vm;

    ptr32_t             fontMan_Ptr = 0;
  };

