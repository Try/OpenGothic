#include "abstractobjectsbucket.h"

void AbstractObjectsBucket::Item::setObjMatrix(const Tempest::Matrix4x4 &mt) {
  owner->setObjMatrix(id,mt);
  }

void AbstractObjectsBucket::Item::setPose(const Pose &p) {
  owner->setSkeleton(id,p);
  }

void AbstractObjectsBucket::Item::setBounds(const Bounds& bbox) {
  owner->setBounds(id,bbox);
  }

void AbstractObjectsBucket::Item::draw(Tempest::Encoder<Tempest::CommandBuffer>& p, uint8_t fId) const {
  owner->draw(id,p,fId);
  }

AbstractObjectsBucket::AbstractObjectsBucket() {
  }
