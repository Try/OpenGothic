#include "abstractobjectsbucket.h"

void AbstractObjectsBucket::Item::setObjMatrix(const Tempest::Matrix4x4 &mt) {
  owner->setObjMatrix(id,mt);
  }

void AbstractObjectsBucket::Item::setSkeleton(const Skeleton *sk) {
  owner->setSkeleton(id,sk);
  }

void AbstractObjectsBucket::Item::setPose(const Pose &p) {
  owner->setSkeleton(id,p);
  }

void AbstractObjectsBucket::Item::setBounds(const Bounds& bbox) {
  owner->setBounds(id,bbox);
  }

const Tempest::Texture2d &AbstractObjectsBucket::Item::texture() const {
  return owner->texture();
  }

void AbstractObjectsBucket::Item::draw(Painter3d& p, uint32_t imgId) const {
  owner->draw(id,p,imgId);
  }

AbstractObjectsBucket::AbstractObjectsBucket() {
  }
