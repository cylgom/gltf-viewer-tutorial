#version 330

in vec3 vViewSpaceNormal;
in vec3 vViewSpacePosition;
in vec2 vTexCoords;

uniform vec3 uLightDirection;
uniform vec3 uLightIntensity;

uniform vec4 uBaseColorFactor;
uniform sampler2D uBaseColorTexture;

uniform float uMetallicFactor;
uniform float uRoughnessFactor;
uniform sampler2D uMetallicRoughnessTexture;

out vec3 fColor;

// Constants
const float GAMMA = 2.2;
const float INV_GAMMA = 1. / GAMMA;
const float M_PI = 3.141592653589793;
const float M_1_PI = 1.0 / M_PI;

// We need some simple tone mapping functions
// Basic gamma = 2.2 implementation
// Stolen here: https://github.com/KhronosGroup/glTF-Sample-Viewer/blob/master/src/shaders/tonemapping.glsl

// linear to sRGB approximation
// see http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
vec3 LINEARtoSRGB(vec3 color)
{
  return pow(color, vec3(INV_GAMMA));
}

// sRGB to linear approximation
// see http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
vec4 SRGBtoLINEAR(vec4 srgbIn)
{
  return vec4(pow(srgbIn.xyz, vec3(GAMMA)), srgbIn.w);
}

void main()
{
  // constants
  vec3 dielectricSpecular = vec3(0.04, 0.04, 0.04);
  vec3 black = vec3(0, 0, 0);
  vec3 N = normalize(vViewSpaceNormal);
  vec3 L = uLightDirection;
  vec3 V = normalize(-vViewSpacePosition);
  vec3 H = normalize(L+V);

  // dot products
  float NdotL = clamp(dot(N, L), 0, 1);
  float VdotH = clamp(dot(V, H), 0, 1);
  float NdotV = clamp(dot(N, V), 0, 1);
  float NdotH = clamp(dot(N, H), 0, 1);

  // metallic/roughness texture
  vec4 mrSample = texture2D(uMetallicRoughnessTexture, vTexCoords);
  float roughness = mrSample.g * uRoughnessFactor;
  float metallic = mrSample.b * uMetallicFactor;

  // color texture
  vec4 baseColorFromTexture =
	  SRGBtoLINEAR(texture(uBaseColorTexture, vTexCoords));
  vec4 baseColor =
	  baseColorFromTexture * uBaseColorFactor;

  // alpha squared == roughness to the 4
  float a_sq =
	roughness
	* roughness
	* roughness
	* roughness;

  // compute Vis
  float Vis_sqrt_a =
    sqrt(NdotV * NdotV * (1 - a_sq) + a_sq);
  float Vis_sqrt_b =
    sqrt(NdotL * NdotL * (1 - a_sq) + a_sq);
  float Vis_denom =
    (NdotL * Vis_sqrt_a) + (NdotV * Vis_sqrt_b);

  float Vis;

  if (Vis_denom <= 0)
  {
  	Vis = 0;
  }
  else
  {
  	Vis = 0.5 / Vis_denom;
  }

  // V.H to the 5
  float VdotH_p5 = (1 - VdotH);
  VdotH_p5 *= VdotH_p5 * VdotH_p5 * VdotH_p5 * VdotH_p5;

  // variables
  vec3 F0 = mix(dielectricSpecular, baseColor.rgb, metallic);
  vec3 F = F0 + (1 - F0) * VdotH_p5;
  float D = a_sq * M_1_PI * pow((NdotH * NdotH) * (a_sq - 1) + 1, -2);

  // diffuse
  vec3 diffuse = mix(
	  baseColor.rgb * (1 - dielectricSpecular.r),
	  black,
	  metallic) * M_1_PI;

  // components
  vec3 f_diffuse = (1 - F) * diffuse;
  vec3 f_specular = F * Vis * D;

  // done
  fColor = LINEARtoSRGB((f_diffuse + f_specular) * uLightIntensity * NdotL);
}
