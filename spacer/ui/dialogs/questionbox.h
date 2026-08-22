#pragma once

#include <Tempest/Dialog>

class QuestionBox : public Tempest::Dialog {
  public:
    QuestionBox();

    enum Ret : uint8_t {
      R_Cancel = 1 << 0,
      R_OK     = 1 << 1,
      R_No     = 1 << 2,
      R_Yes    = 1 << 3,
      };

    friend Ret operator | (Ret a, Ret b) {
      return Ret(uint8_t(a)|uint8_t(b));
      }

    friend Ret operator & (Ret a, Ret b) {
      return Ret(uint8_t(a)&uint8_t(b));
      }

    static Ret ask(const std::string& title, Ret buttons);
    static Ret ask(const char*        title, Ret buttons);

    void       setTitle(const char* t);

  private:
    struct Title;
    template<Ret r>
    void       onButton();

    Ret              ret    = R_Cancel;
    Title*           title  = nullptr;
    Tempest::Button* yes    = nullptr;
    Tempest::Button* no     = nullptr;
    Tempest::Button* ok     = nullptr;
    Tempest::Button* cancel = nullptr;
  };

