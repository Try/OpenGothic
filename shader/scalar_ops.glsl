#ifndef SCALAR_OPS_GLSL
#define SCALAR_OPS_GLSL

#if (VULKAN >= 110) && defined(GL_KHR_shader_subgroup_basic)
#extension GL_KHR_shader_subgroup_basic : enable
#endif

#if (VULKAN >= 110) && defined(GL_KHR_shader_subgroup_vote)
#extension GL_KHR_shader_subgroup_vote : enable
#endif

shared bool workgroup_bool;

bool workgroupAny(bool b) {
#if (VULKAN >= 110) && defined(GL_KHR_shader_subgroup_vote)
  const uint wgSize = gl_WorkGroupSize.x*gl_WorkGroupSize.y*gl_WorkGroupSize.z;
  if(wgSize<=gl_SubgroupSize)
    return subgroupAny(b);
#endif
  workgroup_bool = false;
  barrier();
  if(b)
    workgroup_bool = true;
  memoryBarrierShared();
  barrier();
  return workgroup_bool;
  }

bool workgroupAll(bool b) {
#if (VULKAN >= 110) && defined(GL_KHR_shader_subgroup_vote)
  const uint wgSize = gl_WorkGroupSize.x*gl_WorkGroupSize.y*gl_WorkGroupSize.z;
  if(wgSize<=gl_SubgroupSize)
    return subgroupAll(b);
#endif
  workgroup_bool = true;
  barrier();
  if(!b)
    workgroup_bool = false;
  memoryBarrierShared();
  barrier();
  return workgroup_bool;
  }

#endif
