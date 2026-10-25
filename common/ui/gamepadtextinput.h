#pragma once

#include "utils/keycodec.h"

#include <cstddef>
#include <string>

// The bitmap fonts and save names use single-byte game codepages, not UTF-8.
class GamepadTextInput final {
  public:
    enum class Result { Changed, Accept, Cancel };
    static constexpr size_t Columns = 10;
    static constexpr size_t Cells = 60;
    static constexpr size_t MaxBytes = 64;
    static constexpr size_t Language = 52, Case = 53, Erase = 54, Save = 55, Cancel = 56;

    explicit GamepadTextInput(bool russian = false):russian(russian) {}

    size_t selection() const { return selected; }
    bool enabled(size_t cell) const {
      return cell<(russian ? 33u : 26u) || (cell>=40 && cell<=Cancel);
      }

    void move(KeyCodec::Action action) {
      int step = 0;
      if(action==KeyCodec::Left) step = -1;
      if(action==KeyCodec::Right) step = 1;
      if(action==KeyCodec::Forward) step = -int(Columns);
      if(action==KeyCodec::Back) step = int(Columns);
      if(step==0)
        return;
      do {
        selected = size_t((int(selected)+int(Cells)+step)%int(Cells));
        } while(!enabled(selected));
      }

    std::string label(size_t cell, bool russianUi) const {
      if(!enabled(cell))
        return {};
      if(cell<33) {
        // Alphabetical order, including the separate CP1251 yo glyph.
        static constexpr char ruLower[] = "\xe0\xe1\xe2\xe3\xe4\xe5\xb8\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff";
        static constexpr char ruUpper[] = "\xc0\xc1\xc2\xc3\xc4\xc5\xa8\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf";
        return std::string(1,russian ? (upper ? ruUpper[cell] : ruLower[cell]) : char((upper ? 'A' : 'a')+cell));
        }
      if(cell<50)
        return std::string(1,char('0'+cell-40));
      switch(cell) {
        case 50: return ".";
        case 51: return russianUi ? "\xcf\xf0\xee\xe1\xe5\xeb" : "Space";
        case Language: return russian ? "EN" : "RU";
        case Case: return upper ? "abc" : "ABC";
        case Erase: return "<-";
        case Save: return russianUi ? "\xc3\xee\xf2\xee\xe2\xee" : "Save";
        case Cancel: return russianUi ? "\xce\xf2\xec\xe5\xed\xe0" : "Cancel";
        }
      return {};
      }

    Result activate(std::string& text) {
      switch(selected) {
        case Language: russian = !russian; break;
        case Case: upper = !upper; break;
        case Erase: if(!text.empty()) text.pop_back(); break;
        case Save: return Result::Accept;
        case Cancel: return Result::Cancel;
        default:
          if(text.size()<MaxBytes)
            text += selected==51 ? " " : label(selected,false);
          break;
        }
      return Result::Changed;
      }

  private:
    bool russian = false;
    bool upper = false;
    size_t selected = Save;
  };
