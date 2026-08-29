#include "property.h"

using namespace Property;

const Type Type::Template   = CoreType::Template;
const Type Type::String     = CoreType::String;

const Type Type::Texture    = CoreType::Texture;
const Type Type::Bool1      = {CoreType::Vec1,InputType::Bool};
const Type Type::Int1       = {CoreType::Vec1,InputType::Int};

const Type Type::iVec1      = {CoreType::Vec1,InputType::Int};
const Type Type::iVec2      = {CoreType::Vec2,InputType::Int};
const Type Type::iVec3      = {CoreType::Vec3,InputType::Int};
const Type Type::iVec4      = {CoreType::Vec4,InputType::Int};

const Type Type::Vec1       = CoreType::Vec1;
const Type Type::Vec2       = CoreType::Vec2;
const Type Type::Vec3       = CoreType::Vec3;
const Type Type::Vec4       = CoreType::Vec4;

const Type Type::Enum       = {CoreType::Vec1,InputType::Enum};
const Type Type::Color      = CoreType::Vec3;
const Type Type::Percentage = {CoreType::Vec1,InputType::Percentage};
