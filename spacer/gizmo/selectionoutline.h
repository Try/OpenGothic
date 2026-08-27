#pragma once

#include <cstdint>
#include <memory>

class Camera;
class WorldEdit;

namespace Tempest {
class Attachment;
class CommandBuffer;
class ZBuffer;
template<class T>
class Encoder;
}

class SelectionOutline final {
  public:
    SelectionOutline();
    ~SelectionOutline();

    SelectionOutline(const SelectionOutline&) = delete;
    SelectionOutline& operator=(const SelectionOutline&) = delete;

    void render(Tempest::Encoder<Tempest::CommandBuffer>& cmd,
                Tempest::Attachment& target, Tempest::ZBuffer& sceneDepth,
                const Camera& camera, const WorldEdit& world);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
  };
