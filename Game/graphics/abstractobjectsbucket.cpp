#include "abstractobjectsbucket.h"

void AbstractObjectsBucket::Item::setObjMatrix(const Tempest::Matrix4x4 &mt) {
  owner->markAsChanged();
  owner->setObjMatrix(id,mt);
  }

void AbstractObjectsBucket::Item::setSkeleton(const Skeleton *sk) {
  owner->markAsChanged();
  owner->setSkeleton(id,sk);
  }

void AbstractObjectsBucket::Item::setSkeleton(const Pose &p) {
  owner->markAsChanged();
  owner->setSkeleton(id,p);
  }

AbstractObjectsBucket::AbstractObjectsBucket() {
  }
