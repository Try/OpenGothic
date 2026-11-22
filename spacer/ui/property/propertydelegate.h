#pragma once

#include <Tempest/ListDelegate>
#include <unordered_set>

#include "objects/vobreflection.h"
#include "objects/worldedit.h"

class PropertyDelegate : public Tempest::ListDelegate {
  public:
    PropertyDelegate();

    void             setVob(WorldEdit::Vob* vob);
    void             update();

    std::function<void(std::unique_ptr<Command::Action<WorldEdit>>&,bool)> onChanged;

    size_t           size() const override;
    Tempest::Widget* createView(size_t position) override;
    Tempest::Widget* update(Tempest::Widget* widget, size_t position) override;

  private:
    struct TypeHeader;
    struct Index {
      const VobReflection::Class* cls;
      const VobReflection::Field* field = nullptr;
      };

    void onProperty(size_t id, const Variant& v, bool commit);
    void onCategory(zenkit::VirtualObjectType cat);

    void setupUI(const VobReflection::Class& cls);
    bool isClosed(zenkit::VirtualObjectType type) const;

    std::unordered_set<zenkit::VirtualObjectType> closed;
    std::vector<Index>                            index;
    WorldEdit::Vob*                               vob = nullptr;
  };
