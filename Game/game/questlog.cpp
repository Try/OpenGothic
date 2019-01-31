#include "questlog.h"

QuestLog::QuestLog() {
  }

QuestLog::Quest &QuestLog::add(const std::string &name, Section s) {
  if(auto m = find(name))
    return *m;
  Quest q;
  q.name    = name;
  q.section = s;
  quests.emplace_back(q);
  return quests.back();
  }

void QuestLog::setStatus(const std::string &name, QuestLog::Status s) {
  auto m = find(name);
  if(m==nullptr && s==Status::Obsolete)
    return;
  auto& q  = add(name,Mission);
  q.status = s;
  }

void QuestLog::addEntry(const std::string &name, const std::string &entry) {
  if(auto m = find(name))
    m->entry.emplace_back(entry);
  }

QuestLog::Quest *QuestLog::find(const std::string &name) {
  for(auto& i:quests)
    if(i.name==name)
      return &i;
  return nullptr;
  }
