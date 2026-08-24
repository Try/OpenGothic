#pragma once

#include <Tempest/Panel>

class ToolWindow : public Tempest::Widget {
  public:
    enum Tool : uint8_t {
      T_ProjectTree = 0,
      T_VobTree,
      T_VobProp,
      T_Count
      };
    ToolWindow(Tool t);

    std::string_view name() const { return tname; }

    void   invalidate();
    bool   hasContent();

    Tool   tool()  const { return tId;  }
    size_t order() const { return ord; }
    void   setOrder(size_t ord) { this->ord = ord; }

  private:
    void   setName(const char* name);
    void   setName(Tool preset);

    std::string tname;
    Tool        tId = Tool::T_Count;
    size_t      ord = 0;
  };

