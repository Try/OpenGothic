#include "questlog.h"
#include "serialize.h"

QuestLog::QuestLog() {
  }

QuestLog::Quest &QuestLog::add(std::string_view name, Section s) {
  if(auto m = find(name))
    return *m;
  Quest q;
  q.name    = name;
  q.section = s;
  quests.emplace_back(q);
  return quests.back();
  }

void QuestLog::setStatus(std::string_view name, QuestLog::Status s) {
  // NOTE: in original-game Log_SetTopicStatus (Gothic2.exe 0x006e3f10) only updates the status
  // of an EXISTING topic and creates nothing when the name isn't found. OpenGothic add()-ed a
  // missing topic as a Mission, spawning an empty phantom quest that also persisted into saves.
  auto m = find(name);
  if(m==nullptr)
    return;
  m->status = s;
  }

void QuestLog::addEntry(std::string_view name, std::string_view entry) {
  // NOTE: in original-game oCLogTopic::AddEntry @0x00663870 (from the log_addentry external
  // @0x006e3df0) suppresses the insert when an entry with byte-identical text already exists in
  // the topic; only unique entries are appended. OpenGothic appended unconditionally, so a
  // re-issued Log_AddEntry on a revisited dialogue branch produced duplicate journal lines.
  auto m = find(name);
  if(m==nullptr)
    return;
  for(auto& e:m->entry)
    if(e==entry)
      return;
  m->entry.emplace_back(entry);
  }

QuestLog::Quest *QuestLog::find(std::string_view name) {
  for(auto& i:quests)
    if(i.name==name)
      return &i;
  return nullptr;
  }

void QuestLog::save(Serialize &fout) {
  uint32_t sz=uint32_t(quests.size());
  fout.write(sz);
  for(auto& i:quests){
    fout.write(i.name,uint8_t(i.section),uint8_t(i.status),i.entry);
    }
  }

void QuestLog::load(Serialize &fin) {
  uint32_t sz=0;
  fin.read(sz);
  quests.resize(sz);

  for(auto& i:quests){
    fin.read(i.name,reinterpret_cast<uint8_t&>(i.section),reinterpret_cast<uint8_t&>(i.status),i.entry);
    }
  }
