#pragma once

#include <Tempest/UiOverlay>
#include <Tempest/Widget>
#include <Tempest/Window>

#include <cstdint>

class DropOverEvent : public Tempest::Event {
  public:
    DropOverEvent(Tempest::Widget* dropable):dropable(dropable){}

    Tempest::Widget& drop();
    void   setDropLocation(size_t pos);
    size_t dropLocation() const;

    void setPosition(const Tempest::Point& p);
    const Tempest::Point& pos() const;

  private:
    size_t           dpos = 0;
    Tempest::Widget* dropable;
    Tempest::Point   mpos;
  };

class DropReciver {
  public:
    virtual ~DropReciver(){}
    virtual void moveDropOver ( DropOverEvent& ev ) = 0;
    virtual void moveDropLeave( DropOverEvent& ev ){ ev.ignore(); }
    virtual void dropDone     ( DropOverEvent& ev ){ ev.ignore(); }
  };

class DragDrop {
  public:
    DragDrop();
    ~DragDrop();

    void begin(Tempest::MouseEvent &e, Tempest::Widget &w);
    void drag (Tempest::MouseEvent &e);
    bool end  (Tempest::MouseEvent &e);

          Tempest::Widget* dragable();
    const Tempest::Widget* dragable() const;

    void             setDrop(Tempest::Widget* owner, size_t place);
    Tempest::Widget* drop() const;

    bool             isDrag() const { return state==Drag; }

  private:
    struct Overlay : Tempest::UiOverlay {
      };
    enum State:uint8_t {
      PreDrag,
      Drag
      };

    Tempest::Widget* solveDrop    (DropOverEvent& ev, const Tempest::Point& p);
    Tempest::Widget* implSolveDrop(DropOverEvent& ev, Tempest::Widget* wx, const Tempest::Point& p);

    std::unique_ptr<Overlay> overlay;
    Tempest::Widget*         drItem  = nullptr;

    Tempest::Point           dpos;

    Tempest::Widget*         mOwner   = nullptr;
    size_t                   mOwnerAt = 0;

    Tempest::Widget*         mDrop    = nullptr;
    size_t                   mDropAt  = 0;

    State                    state    = PreDrag;
  };
