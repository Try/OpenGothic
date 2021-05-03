#pragma once

#include <Tempest/Layout>

class StackLayout : public Tempest::Layout {
  public:
    StackLayout();

  private:
    void applyLayout() override;
  };
