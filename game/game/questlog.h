#pragma once

#include <vector>
#include <string>
#include <cstdint>

class Serialize;

class QuestLog final {
  public:
    QuestLog();

    enum class Status : uint8_t {
      // NOTE: in original-game Log_CreateTopic @0x006e3ca0 a new oCLogTopic is initialized with
      // status 0 (field +0x18), which oCMenu_Log::SetLogTopics @0x0047bf90 maps to NO mission tab
      // until Log_SetTopicStatus sets RUNNING/SUCCESS/FAILED. OpenGothic defaulted to Running, so a
      // mission topic created without an explicit status was wrongly shown as a current quest.
      None     = 0,
      Running  = 1,
      Success  = 2,
      Failed   = 3,
      Obsolete = 4
      };

    enum Section : uint8_t {
      Mission = 0,
      Note    = 1
      };

    struct Quest {
      std::string name;
      Section     section=Section::Mission;
      Status      status =Status::None;

      std::vector<std::string> entry;
      };

    Quest& add      (std::string_view name, Section s);
    void   setStatus(std::string_view name, Status  s);
    void   addEntry (std::string_view name, std::string_view entry);

    void   save(Serialize &fout);
    void   load(Serialize &fin);

    size_t questCount() const { return quests.size(); }
    auto   quest(size_t i) const -> const Quest& { return quests[i]; }


  private:
    Quest* find(std::string_view name);

    std::vector<Quest> quests;
  };
