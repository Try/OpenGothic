#pragma once

#include <Tempest/Layout>

class StackLayout : public Tempest::Layout {
  public:
    explicit StackLayout(Tempest::Widget& fullScreen);

  private:
    void applyLayout() override;

    Tempest::Widget& fullScreen;
  };
