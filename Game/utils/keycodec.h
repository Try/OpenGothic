#pragma once

#include <Tempest/Event>
#include <string>

class KeyCodec final {
  public:
    KeyCodec()=delete;

    static void getKeysStr(const std::string& keys, char buf[], size_t bufSz);

  private:
    static int  fetch(const std::string& keys,size_t s,size_t e);
    static bool keyToStr(int32_t k, char* buf, size_t bufSz);
    static void keyToStr(Tempest::Event::KeyType k, char* buf, size_t bufSz);
    static void keyToStr(Tempest::Event::MouseButton k, char* buf, size_t bufSz);

    struct K_Key {
      Tempest::Event::KeyType k=Tempest::Event::K_NoKey;
      int32_t                 code=0;
      };
    struct M_Key {
      Tempest::Event::MouseButton k=Tempest::Event::ButtonNone;
      int32_t                     code=0;
      };
  };

