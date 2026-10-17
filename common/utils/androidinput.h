#pragma once

#ifdef OPENGOTHIC_TOUCH_UI

#include <Tempest/Sprite>
#include <array>
#include <functional>
#include <string>

#include "ui/mobilecontrolsmodel.h"

#ifdef __ANDROID__
struct AInputEvent;
#endif

namespace Tempest {
  class Font;
  class Painter;
  class PaintEvent;
  class TextureAtlas;
  class Window;
  }

namespace AndroidInput {

/// Order matches IconFiles in androidinput.cpp
enum Icon : int {
  NoIcon = -1,
  IconPause, IconStatus, IconJournal, IconMap, IconInventory, IconSneak, IconJump,
  IconAttack, IconAttackLeft, IconAttackRight, IconBlock, IconWeapon,
  IconTarget, IconInteract, IconContext, IconShoot, IconCast, IconSpells,
  IconUp, IconDown, IconConfirm, IconBack, IconTransfer, IconUse,
  IconJoystickBase, IconJoystickThumb, IconButton, IconButtonPressed, IconSlot, IconSlotSelected,
  IconBarEmpty, IconHealth, IconMana, IconOxygen, IconLungs,
  IconSettings, IconWalk, IconLeft, IconRight, IconKeyboard,
  IconCategoryAll, IconCategoryWeapons, IconCategoryArmor, IconCategoryJewelry, IconCategoryMagic,
  IconCategoryFood, IconCategoryPotion, IconCategoryDocuments, IconCategoryMisc,
  IconInvUse, IconInvDrop, IconInvTransfer, IconInvUp, IconInvDown, IconInvSlot, IconInvSlotSelected,
  IconSkip, IconTalk, IconTargetLock,
  IconCount
  };

/// Values of the [MOBILE] section of the ini file.
struct Preferences {
  int buttons     = 120; // percent
#ifdef __ANDROID__
  int resolution  = 75;  // percent of the display resolution
  int shadows     = 512; // shadow map size, 0 disables shadows
  int effects     = 0;   // SSAO
  int fps         = 30;
#else
  int resolution  = 100; // desktop keeps the full quality by default
  int shadows     = 2048;
  int effects     = 0;
  int fps         = 60;
#endif
  int deadzone    = 28;  // percent
  int sensitivity = 100; // percent
  int vibration   = 1;   // feedback on damage
  int gui         = 100; // scale of the interface, percent
  };

Preferences preferences();

class Controller {
  public:
    struct Hud {
      float health = 0;
      float mana   = 0;
      float oxygen = 1;
      int   hp = 0, hpMax = 0;
      int   mp = 0, mpMax = 0;
      bool  diving = false;
      bool  manaVisible = false;
      int   active = -1;
      Icon  actionIcon = IconInteract;
      int   inventoryCategory = 0;
      bool  inventoryTransfer = false;
      std::string useLabel;           // action of the selected item in the inventory
      };

    void  bind();
    void  unbind();
    bool  update(Tempest::Window& window, Mobile::Mode mode);
    void  paint(Tempest::PaintEvent& event);
    void  loadIcons(Tempest::TextureAtlas& atlas);
    void  cancel();
    void  inventoryLists(bool two) { controls.inventoryLists(two); }
    void  gui(float scale) { controls.gui(scale); }

    /// Pointer input: touches on Android, the mouse on desktop builds.
    void  pointerDown(int id, float x, float y);
    void  pointerMove(int id, float x, float y);
    void  pointerUp  (int id, float x, float y);
    void  physical(Mobile::Key key, bool down);

    void  availability(uint64_t keys, bool target, bool spells, bool movement) {
      controls.availability(keys,target,spells,movement);
      }

    const Tempest::Sprite&     icon(Icon id) const { return icons.at(size_t(id)); }
    const std::vector<Mobile::Button>& buttons() const { return controls.buttons(); }
    bool  settingsVisible() const { return controls.settingsVisible(); }

    Hud   hud;

    std::function<void(float,float)>     look;
    std::function<void(float,float,int)> menuTouch;
    std::function<void(float,float)>     menuTap;
    std::function<void(float)>           menuScroll;
    std::function<void(int)>             action;

  private:
#ifdef __ANDROID__
    int32_t onInput(AInputEvent* event);
#endif
    void    applyPreferences();
    void    dispatch(Tempest::Window& window, Mobile::Mode mode, const Mobile::Event& input);
    void    onSpecial(int id);

    int     iconFor(const Mobile::Button& b) const;
    void    paintButton(Tempest::Painter& p, Tempest::Font& font, size_t index);
    void    paintHotbarSlot(Tempest::Painter& p, Tempest::Font& font, size_t index);
    void    paintInventoryButton(Tempest::Painter& p, Tempest::Font& font, size_t index);
    void    paintStick(Tempest::Painter& p);
    void    paintSprite(Tempest::Painter& p, Icon id, const Mobile::Rect& r, float alpha) const;

    Mobile::Controls controls;
    bool             paused = false;

    std::array<Tempest::Sprite,IconCount> icons;
    std::array<float,IconCount>           iconPx{}; // side of the icon in pixels
    float                                 screenW = 0, screenH = 0;
    std::array<Mobile::Rect,IconCount>    ink;
  };

}

#endif
