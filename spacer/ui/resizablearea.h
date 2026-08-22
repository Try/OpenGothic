#pragma once

#include <Tempest/Widget>
#include <map>

class ResizableArea : public Tempest::Widget {
  public:
    ResizableArea(Tempest::Orientation ori);

    struct State {
      std::map<const Tempest::Widget*,float> val;
      float                                  visSum   = 0;
      int                                    visCount = 0;
      };

    void                 setOrientation(Tempest::Orientation ori);
    Tempest::Orientation orientation() const;

    void                 setWeights(std::initializer_list<float> w);
    void                 setWeights(const std::vector<float>& w);
    void                 setWeights(const State& w);
    State                weightVec() const { return weight; };
    std::vector<float>   weights() const;

    Tempest::Signal<void()> onResizeFinished;

  protected:
    void paintEvent(Tempest::PaintEvent &event) override;

    void mouseMoveEvent(Tempest::MouseEvent &event) override;
    void mouseDownEvent(Tempest::MouseEvent &event) override;
    void mouseDragEvent(Tempest::MouseEvent &event) override;
    void mouseUpEvent  (Tempest::MouseEvent &event) override;
    void resizeEvent(Tempest::SizeEvent &event) override;

  private:
    using Widget::setLayout;
    struct Layout;

    void relayout();
    void initWeightVec  (State& w);
    void updateWeightVec(State& w);
    void emplace(Widget& w, int at, int sz);

    struct Drag {
      Tempest::Widget* prev    = nullptr;
      Tempest::Widget* curr = nullptr;
      } dr;
    struct LayLock {
      bool v = false;
      void lock()   { v = true;  }
      void unlock() { v = false; }
      };

    Tempest::Orientation ori     = Tempest::Horizontal;
    int                  x0      = 0;
    Tempest::Rect        r0, r1;

    State                weight;
    LayLock              lockLayout;
  };

