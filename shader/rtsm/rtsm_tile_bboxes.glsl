const uint NumSubTiles  = 4*4;
const uint NumSubTilesX = 4;

// 5'576 bytes
shared uvec4 rayTileBbox[(1+NumSubTiles)*MaxSlices];
shared uint  rayDepthMin[(1+NumSubTiles)*MaxSlices];
shared uint  numSlices  [(1+NumSubTiles)];
shared uint  numSubtiles;

// visibility
bool isAabbVisible(const vec4 aabb, const float depthMax, const uint id) {
  const uint num = numSlices[id];
  const uint tid = id*MaxSlices;

  for(uint i=0; i<num; ++i) {
    vec4  rbb  = uintBitsToFloat(rayTileBbox[tid + i]);
    float rayd = uintBitsToFloat(rayDepthMin[tid + i]);
    if(rayd > depthMax)
      continue;
    if(!bboxIntersect(aabb, rbb))
      continue;
    return true;
    }
  return false;
  }

// ray related
// FIXME: normal bias
vec3 rayOrigin(ivec2 frag, float depth) {
  const vec2 fragCoord = ((frag.xy+0.5)*scene.screenResInv)*2.0 - vec2(1.0);
  const vec4 scr       = vec4(fragCoord.x, fragCoord.y, depth, 1.0);

  const vec4 shPos = scene.viewProject2VirtualShadow * scr;
  return shPos.xyz/shPos.w;
  }

uint depthSlice(const float z) {
  float dZ   = linearDepth(z,      scene.clipInfo);
  float d0   = linearDepth(0,      scene.clipInfo);
  float d1   = linearDepth(0.9999, scene.clipInfo);
  float d    = (dZ-d0)/(d1-d0);

  return min(uint(d*MaxSlices), MaxSlices-1);
  }

// fragments/tiles
void processFragment(const ivec2 fragCoord, const ivec2 subTile) {
  const float lamb   = texelFetch(outputImage, fragCoord, 0).x;
  const bool  actRay = (lamb>0);
  if(!actRay)
    return;

  const float z = texelFetch(depth, fragCoord, 0).x;
  if(z==1.0)
    return;

  const uint slice = depthSlice(z);
  const vec3 ray   = rayOrigin(fragCoord, z);
  const uint bin   = (1 + subTile.x + subTile.y*NumSubTilesX);
  const uint tid   = bin*MaxSlices + slice;

  atomicMin(rayTileBbox[tid].x, floatToOrderedUint(ray.x));
  atomicMin(rayTileBbox[tid].y, floatToOrderedUint(ray.y));
  atomicMax(rayTileBbox[tid].z, floatToOrderedUint(ray.x));
  atomicMax(rayTileBbox[tid].w, floatToOrderedUint(ray.y));
  atomicMin(rayDepthMin[tid],   floatToOrderedUint(ray.z));
  atomicOr (numSlices  [bin], 1u << slice);
  }

void tileBboxes(const ivec2 tileId, const uint tileSz) {
  const uint laneID    = gl_LocalInvocationIndex;
  const uint subTileSz = tileSz/NumSubTilesX;

  for(uint i=laneID; i<rayTileBbox.length(); i+=NumThreads) {
    rayTileBbox[i] = uvec4(0xFFFFFFFF, 0xFFFFFFFF, 0, 0);
    rayDepthMin[i] = 0xFFFFFFFF;
    }
  if(laneID<numSlices.length())
    numSlices[laneID] = 0;
  numSubtiles = 0;
  barrier();

  const uvec2 srcSz = textureSize(depth,0);
  const ivec2 at0   = ivec2(tileId) * ivec2(tileSz);
  const ivec2 xy0   = ivec2(gl_LocalInvocationID.xy);

  for(int x=xy0.x; x<tileSz; x+=int(gl_WorkGroupSize.x)) {
    for(int y=xy0.y; y<tileSz; y+=int(gl_WorkGroupSize.y)) {
      ivec2 at = at0 + ivec2(x,y);
      if(any(greaterThanEqual(at,srcSz)))
        continue;
      processFragment(at, ivec2(x,y)/int(subTileSz));
      }
    }
  barrier();

  for(uint i=MaxSlices+laneID; i<rayTileBbox.length(); i+=NumThreads) {
    const uint slice = i % MaxSlices;
    atomicMin(rayTileBbox[slice].x, rayTileBbox[i].x);
    atomicMin(rayTileBbox[slice].y, rayTileBbox[i].y);
    atomicMax(rayTileBbox[slice].z, rayTileBbox[i].z);
    atomicMax(rayTileBbox[slice].w, rayTileBbox[i].w);
    atomicMin(rayDepthMin[slice],   rayDepthMin[i]);
    }
  if(laneID<NumSubTiles)
    atomicOr(numSlices[0], numSlices[laneID+1]);
  barrier();

  vec4 rbb; float rayD;
  if(laneID<rayTileBbox.length()) {
    rbb  = orderedUintToFloat(rayTileBbox[laneID]);
    rayD = orderedUintToFloat(rayDepthMin[laneID]);
    }
  barrier();

  const uint srcBin   = laneID / MaxSlices;
  const uint srcSlice = laneID % MaxSlices;
  if(laneID<rayTileBbox.length() && (numSlices[srcBin] & (1u << srcSlice))!=0) {
    const uint i = bitCount(numSlices[srcBin] & ((1u << srcSlice)-1u));
    rayTileBbox[srcBin*MaxSlices + i] = floatBitsToUint(rbb);
    rayDepthMin[srcBin*MaxSlices + i] = floatBitsToUint(rayD);
    }
  barrier();

  if(laneID<numSlices.length()) {
    const uint num = bitCount(numSlices[laneID]);
    numSlices[laneID] = num;
    if(num>0)
      atomicOr(numSubtiles, 1u<<laneID);
    }
  }
