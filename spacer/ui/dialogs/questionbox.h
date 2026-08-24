#pragma once

#include <Tempest/Dialog>
#include <cstdint>

class QuestionBox : public Tempest::Dialog {
  public:
    QuestionBox();

    enum Ret : uint8_t {
      Cancel = 1 << 0,
      OK     = 1 << 1,
      No     = 1 << 2,
      Yes    = 1 << 3,
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

    Ret              ret    = Cancel;
    Title*           title  = nullptr;
    Tempest::Button* yes    = nullptr;
    Tempest::Button* no     = nullptr;
    Tempest::Button* ok     = nullptr;
    Tempest::Button* cancel = nullptr;
  };

