#version 330

in vec2 vTexCoords;
in vec3 vViewSpacePosition;

out vec2 fColor;

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
    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);
	
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

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float k = (roughness * roughness) * 0.5f;

    return NdotV / (NdotV * (1.0f - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);

    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec2 IntegrateBRDF(float NdotV, float roughness)
{
    vec3 V;
    V.x = sqrt(1.0f - NdotV * NdotV);
    V.y = 0.0f;
    V.z = NdotV;

    float A = 0.0f;
    float B = 0.0f;

    vec3 N = vec3(0.0f, 0.0f, 1.0f);
    const uint samples = 1024u;
	const float invSamples = 1.0f / samples;

    for (uint i = 0u; i < samples; ++i)
    {
        vec2 Xi = Hammersley(i, samples);
        vec3 H  = ImportanceSampleGGX(Xi, N, roughness);
        vec3 L  = normalize(2.0f * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.0f);
        float NdotH = max(H.z, 0.0f);
        float VdotH = max(dot(V, H), 0.0f);

        if (NdotL > 0.0f)
        {
            float G = GeometrySmith(N, V, L, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc = pow(1.0f - VdotH, 5.0f);

            A += (1.0f - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }

    A *= invSamples;
    B *= invSamples;

    return vec2(A, B);
}

void main() 
{
    vec2 integratedBRDF = IntegrateBRDF(vTexCoords.x, vTexCoords.y);
	fColor = integratedBRDF;
}
