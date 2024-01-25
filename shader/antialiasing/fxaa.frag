#version 450

#extension GL_GOOGLE_include_directive : enable

#define FXAA_GLSL_130 1

#if FXAA_QUALITY_SETTING == 0
	#define FXAA_QUALITY_PRESET 10
	#define FXAA_PC_CONSOLE 1
#elif FXAA_QUALITY_SETTING == 1
	#define FXAA_QUALITY_PRESET 10
	#define FXAA_PC 1
#elif FXAA_QUALITY_SETTING == 2
	#define FXAA_QUALITY_PRESET 15
	#define FXAA_PC 1
#elif FXAA_QUALITY_SETTING == 3
	#define FXAA_QUALITY_PRESET 29
	#define FXAA_PC 1
#elif FXAA_QUALITY_SETTING == 4
	#define FXAA_QUALITY_PRESET 39
	#define FXAA_PC 1
#endif

#include "Fxaa3_11.h"

layout(push_constant, std140) uniform PushConstantsFxaa {
  float fxaaInverseSharpnessCoeff;
  float fxaaQualitySubpix;
  float fxaaQualityEdgeThreshold;
  float fxaaQualityEdgeThresholdMin;
  float fxaaConsoleEdgeSharpness;
  float fxaaConsoleEdgeThreshold;
  float fxaaConsoleEdgeThresholdMin;
  } pushConstantsFxaa;

layout(binding  = 0) uniform sampler2D aliasedInput;

layout(location = 0) in  vec2 uv;
layout(location = 0) out vec4 outColor;

void main() {
  vec2 screenSize = textureSize(aliasedInput, 0);
  vec2 screenSizeInv = 1.f / screenSize;

  vec4 fxaaConsolePosPos = vec4(uv.xy - screenSizeInv, uv + screenSizeInv);

  const vec4 fxaaConsoleRcpFrameOpt = vec4(
	-pushConstantsFxaa.fxaaInverseSharpnessCoeff * screenSizeInv.x,
	-pushConstantsFxaa.fxaaInverseSharpnessCoeff * screenSizeInv.y,
	pushConstantsFxaa.fxaaInverseSharpnessCoeff * screenSizeInv.x,
	pushConstantsFxaa.fxaaInverseSharpnessCoeff * screenSizeInv.y
	);

  const vec4 fxaaConsoleRcpFrameOpt2 = vec4(
	-2.f * screenSizeInv.x,  
    -2.f * screenSizeInv.y,
    2.f * screenSizeInv.x,  
    2.f * screenSizeInv.y
	);

  const vec4 fxaaConsole360RcpFrameOpt2 = vec4(
    8.f * screenSizeInv.x,  
    8.f * screenSizeInv.y,
    -4.f * screenSizeInv.x,  
    -4.f * screenSizeInv.y
	);

  const vec4 fxaaConsole360ConstDir = vec4(0.f, 0.f, 0.f, 0.f);

  outColor = FxaaPixelShader(
	uv, 
	fxaaConsolePosPos,
	aliasedInput,
	aliasedInput,
	aliasedInput,
	screenSizeInv,
	fxaaConsoleRcpFrameOpt,
	fxaaConsoleRcpFrameOpt2,
	fxaaConsole360RcpFrameOpt2,
	pushConstantsFxaa.fxaaQualitySubpix,
	pushConstantsFxaa.fxaaQualityEdgeThreshold,
	pushConstantsFxaa.fxaaQualityEdgeThresholdMin,
	pushConstantsFxaa.fxaaConsoleEdgeSharpness,
	pushConstantsFxaa.fxaaConsoleEdgeThreshold,
	pushConstantsFxaa.fxaaConsoleEdgeThresholdMin,
	fxaaConsole360ConstDir);
  }