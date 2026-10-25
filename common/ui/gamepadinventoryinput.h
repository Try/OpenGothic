#pragma once

#include "utils/keycodec.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

class GamepadInventoryInput final {
  public:
    enum class Mode : uint8_t { Browse, Quantity, SpellSlot };
    enum class Result : uint8_t { None, Changed, Confirm, Cancel };

    struct Selection {
      uint8_t state = 0;
      uint8_t panel = 0;
      size_t index = 0;
      size_t cls = 0;
      size_t count = 0;
      uint8_t slot = 255;
      bool equipped = false;

      bool operator==(const Selection& other) const {
        return state==other.state && panel==other.panel && index==other.index &&
               cls==other.cls && count==other.count && slot==other.slot && equipped==other.equipped;
        }
      };

    Mode mode() const { return current; }
    size_t quantity() const { return amount; }
    uint8_t slot() const { return spellSlot; }
    bool matches(const Selection& selection) const { return current!=Mode::Browse && selected==selection; }
    void reset() { current = Mode::Browse; }

    bool beginQuantity(const Selection& selection) {
      if(selection.count==0)
        return false;
      selected = selection;
      amount = 1;
      current = Mode::Quantity;
      return true;
      }

    void beginSpell(const Selection& selection) {
      selected = selection;
      spellSlot = selection.slot>=3 && selection.slot<=10 ? selection.slot : 3;
      current = Mode::SpellSlot;
      }

    Result process(KeyCodec::Action action) {
      if(current==Mode::Browse)
        return Result::None;
      if(action==KeyCodec::Escape)
        return Result::Cancel;
      if(action==KeyCodec::ActionGeneric)
        return Result::Confirm;
      if(current==Mode::Quantity) {
        switch(action) {
          case KeyCodec::Left:       amount -= std::min(amount-1,size_t(1)); break;
          case KeyCodec::Right:      amount += std::min(selected.count-amount,size_t(1)); break;
          case KeyCodec::Back:       amount -= std::min(amount-1,size_t(10)); break;
          case KeyCodec::Forward:    amount += std::min(selected.count-amount,size_t(10)); break;
          case KeyCodec::UiPrevious: amount = 1; break;
          case KeyCodec::UiNext:     amount = selected.count; break;
          default: return Result::None;
          }
        } else {
        switch(action) {
          case KeyCodec::Left:
          case KeyCodec::Forward:
          case KeyCodec::UiPrevious: spellSlot = spellSlot==3 ? 10 : uint8_t(spellSlot-1); break;
          case KeyCodec::Right:
          case KeyCodec::Back:
          case KeyCodec::UiNext:     spellSlot = spellSlot==10 ? 3 : uint8_t(spellSlot+1); break;
          default: return Result::None;
          }
        }
      return Result::Changed;
      }

  private:
    Mode current = Mode::Browse;
    Selection selected;
    size_t amount = 1;
    uint8_t spellSlot = 3;
  };
