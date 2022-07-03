#pragma once

#include <vector>
#include <string>

class Serialize;

class QuestLog final {
  public:
    QuestLog();

    enum class Status : uint8_t {
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
      Status      status =Status::Running;

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
