#pragma once

#include <Tempest/LineEdit>

class NumberEdit : public Tempest::LineEdit {
  public:
    NumberEdit();

    Tempest::Signal<void(double,bool)> onValueModifyed;

    void   setValue(double v);
    double value() const;

    void   enableFloats(bool e);
    void   setFloatDigits(uint8_t d);

  protected:
    void keyDownEvent  (Tempest::KeyEvent &e) override;
    void keyRepeatEvent(Tempest::KeyEvent &e) override;
    void focusEvent    (Tempest::FocusEvent& e) override;

  private:
    bool checkInput(Tempest::KeyEvent &e) const;

    void commitChange();
    void postAccept(Tempest::KeyEvent&);

    double  val    = 0;
    uint8_t digits = 6;
    bool    flt   = false;
  };

