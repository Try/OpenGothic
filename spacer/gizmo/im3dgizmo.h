#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/Vec>

#include <cstdint>
#include <memory>

class Camera;

namespace Tempest {
class Attachment;
class CommandBuffer;
template<class T>
class Encoder;
}

class Im3dGizmo final {
  public:
    enum class Mode : uint8_t {
      Translation,
      Rotation,
      Scale,
      };

    Im3dGizmo();
    ~Im3dGizmo();

    Im3dGizmo(const Im3dGizmo&) = delete;
    Im3dGizmo& operator=(const Im3dGizmo&) = delete;

    void setMode(Mode mode);
    Mode mode() const;

    bool isHovered() const;
    bool isActive() const;

    // Builds im3d's frame and copies its transient draw lists for the native Tempest pass.
    bool prepare(const Camera& camera,
                 const Tempest::Vec3& cursorRayOrigin, const Tempest::Vec3& cursorRayDirection,
                 bool mouseDown, int width, int height, Tempest::Matrix4x4* transform);

    // Records the gizmo directly into the editor scene framebuffer.
    void render(Tempest::Encoder<Tempest::CommandBuffer>& cmd,
                Tempest::Attachment& target, uint8_t frameId);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    Mode currentMode = Mode::Translation;
  };
