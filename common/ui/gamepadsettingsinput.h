#pragma once

#include "utils/gamepadsettings.h"

#include <algorithm>
#include <utility>

// The editor owns a draft until its first terminal decision.
class GamepadSettingsInput final {
  public:
    static constexpr int FirstBinding = 6;
    static constexpr int Apply = FirstBinding+int(GamepadSettings::remappableButtons.size());
    static constexpr int Reset = Apply+1;
    static constexpr int Cancel = Apply+2;
    static constexpr int Rows = Cancel+1;

    explicit GamepadSettingsInput(GamepadSettings settings):draft(std::move(settings)) { draft.normalize(); }

    void select(int row) { if(!finished) selected = std::clamp(row,0,Rows-1); }
    void process(KeyCodec::Action action) {
      if(finished)
        return;
      if(action==KeyCodec::Escape) {
        finished = true;
        return;
        }
      if(action==KeyCodec::Forward || action==KeyCodec::Back) {
        select(selected+(action==KeyCodec::Forward ? -1 : 1));
        return;
        }
      if(action==KeyCodec::ActionGeneric && selected>=Apply) {
        if(selected==Reset)
          draft = GamepadSettings();
        else {
          accepted = selected==Apply;
          finished = true;
          }
        return;
        }
      const int step = action==KeyCodec::Left ? -1 : action==KeyCodec::Right ? 1 : 0;
      if(step==0)
        return;
      if(selected==0)
        draft.moveDeadZone = std::clamp(draft.moveDeadZone+step,5,70);
      else if(selected==1)
        draft.lookDeadZone = std::clamp(draft.lookDeadZone+step,0,50);
      else if(selected==2)
        draft.sensitivityX = std::clamp(draft.sensitivityX+step*5,25,300);
      else if(selected==3)
        draft.sensitivityY = std::clamp(draft.sensitivityY+step*5,25,300);
      else if(selected==4)
        draft.invertY = !draft.invertY;
      else if(selected==5)
        draft.hints = !draft.hints;
      else if(selected<Apply) {
        auto& binding = draft.bindings[size_t(GamepadSettings::remappableButtons[size_t(selected-FirstBinding)])];
        const auto& actions = GamepadSettings::allowedActions;
        const int index = int(std::find(actions.begin(),actions.end(),binding)-actions.begin());
        binding = actions[size_t((index+step+int(actions.size()))%int(actions.size()))];
        }
      }

    GamepadSettings draft;
    int selected = 0;
    bool finished = false;
    bool accepted = false;
  };
