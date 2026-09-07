#pragma once

#include <vector>
#include <memory>

namespace Command {

template<class Subject>
class Action;

template<class Subject>
class UndoStack final {
  public:
    UndoStack() = default;
    virtual ~UndoStack()=default;

    void push(Subject& subj, Action<Subject>* cmd, bool commit=true) {
      bool merged = false;
      stk.push_back(std::unique_ptr<Action<Subject>>(cmd));
      try {
        stk.back()->redo(subj);
        if(stk.size()>1 && !barrier) {
          if(stk[stk.size()-2]->merge(*stk.back())) {
            merged = true;
            stk.pop_back();
            }
          }
        undoStk.clear();
        barrier = commit;
        }
      catch(...){
        stk.pop_back();
        throw;
        }
      if(merged)
        return;
      if(savStk<0)
        savStk = std::numeric_limits<int64_t>::max();
      if(savStk!=std::numeric_limits<int64_t>::max())
        savStk++;
      }

    void undo(Subject& subj)  {
      if(stk.size()==0)
        return;

      stk.back()->undo(subj);
      auto ptr = std::move(stk.back());
      try {
        undoStk.push_back(std::move(ptr));
        stk.pop_back();
        }
      catch(...){
        // undoStk not consistent anymore
        undoStk.clear();
        // abandon action in this case: cannot return it to stk, because it creates recursive 'fail to fail' scenario
        throw;
        }
      if(savStk!=std::numeric_limits<int64_t>::max())
        savStk--;
      }

    void redo(Subject& subj)  {
      if(undoStk.size()==0)
        return;

      stk.push_back(nullptr);
      try {
        undoStk.back()->redo(subj);
        }
      catch(...) {
        stk.pop_back();
        throw;
        }
      stk.back() = std::move(undoStk.back());
      undoStk.pop_back();
      if(savStk!=std::numeric_limits<int64_t>::max())
        savStk++;
      }

    void setSaveMarker() {
      savStk = 0;
      }

    bool hasUnsavedChanges() const {
      return savStk!=0;
      }

  private:
    std::vector<std::unique_ptr<Action<Subject>>> stk, undoStk;

    int64_t savStk  = 0;
    bool    barrier = false;
  };

template<class Subject>
class Action {
  public:
    Action() = default;
    virtual ~Action() = default;

    virtual void redo(Subject& subj)=0;
    virtual void undo(Subject& subj)=0;
    virtual bool merge(const Action& prev) { (void)prev; return false; }
  };

}

