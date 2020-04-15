#include "abstractobjectsbucket.h"

void AbstractObjectsBucket::Item::setObjMatrix(const Tempest::Matrix4x4 &mt) {
  owner->markAsChanged();
  owner->setObjMatrix(id,mt);
  }

void AbstractObjectsBucket::Item::setSkeleton(const Skeleton *sk) {
  owner->markAsChanged();
  owner->setSkeleton(id,sk);
  }

void AbstractObjectsBucket::Item::setPose(const Pose &p) {
  owner->markAsChanged();
  owner->setSkeleton(id,p);
  }

const Tempest::Texture2d &AbstractObjectsBucket::Item::texture() const {
  return owner->texture();
  }

void AbstractObjectsBucket::Item::draw(Tempest::Encoder<Tempest::CommandBuffer> &cmd,const Tempest::RenderPipeline &pipeline, uint32_t imgId) const {
  owner->draw(id,cmd,pipeline,imgId);
  }

AbstractObjectsBucket::AbstractObjectsBucket() {
  }
