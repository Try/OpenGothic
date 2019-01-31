#pragma once

#include <vector>
#include <string>

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

    Quest& add      (const std::string& name,Section s);
    void   setStatus(const std::string& name,Status  s);
    void   addEntry (const std::string& name,const std::string& entry);

  private:
    Quest* find(const std::string& name);

    std::vector<Quest> quests;
  };
