#version 330

in vec3 vViewSpacePosition;

uniform samplerCube uEnvironmentMap;
uniform float uRoughness;
uniform float uResolution;

out vec4 fColor;

const float PI = 3.14159265359f;

float RadicalInverse_VdC(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);

	return 0.00000000023283064365386963f * bits;
}

vec2 Hammersley(uint i, float Ni)
{
	return vec2((i * Ni), RadicalInverse_VdC(i));
}

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
    float a = roughness * roughness;
	
    float phi = 2.0f * PI * Xi.x;
    float cosTheta = sqrt((1.0f - Xi.y) / (1.0f + (((a * a) - 1.0f) * Xi.y)));
    float sinTheta = sqrt(1.0f - cosTheta*cosTheta);
	
    vec3 H;
	vec3 up;

    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

	if (abs(N.z) < 0.999f)
	{
		up = vec3(0.0f, 0.0f, 1.0f);
	}
	else
	{
		up = vec3(1.0f, 0.0f, 0.0f);
	}

    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);
    vec3 sampleVec = (tangent * H.x) + (bitangent * H.y) + (N * H.z);

    return normalize(sampleVec);
}

float DistributionGGX(float NdotH2, float a)
{
	float a2 = a * a;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom *= denom * PI;

	return a2 / denom;
}
  
void main()
{
    vec3 N = normalize(vViewSpacePosition);
    vec3 R = N;
    vec3 V = R;

    const uint samples = 1024u;
	const float Ni = 1.0f / samples;

    float totalWeight = 0.0f;
    vec3 prefilteredColor = vec3(0.0f);

    for (uint i = 0u; i < samples; ++i)
    {
        vec2 Xi = Hammersley(i, Ni);
        vec3 H = ImportanceSampleGGX(Xi, N, uRoughness);
        vec3 L = normalize(2.0f * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0f);
        float NdotH = max(dot(N, H), 0.0f);
        float HdotV = max(dot(H, V), 0.0f);

		float D = DistributionGGX(NdotH * NdotH, uRoughness * uRoughness);
		float pdf = ((D * NdotH) / (4.0f * HdotV)) + 0.0001f; 
		float saTexel = 4.0f * PI / (6.0f * uResolution * uResolution);
		float saSample = 1.0f / (float(samples) * pdf + 0.0001f);
		float mipLevel;

		if (uRoughness == 0.0f)
		{
			mipLevel = 0.0f;
		}
		else
		{
			mipLevel = 0.5f * log2(saSample / saTexel);
		}

        if (NdotL > 0.0f)
        {
            prefilteredColor += textureLod(uEnvironmentMap, L, mipLevel).rgb * NdotL;
            totalWeight += NdotL;
        }
    }

    prefilteredColor /= totalWeight;
    fColor = vec4(prefilteredColor, 1.0f);
}
