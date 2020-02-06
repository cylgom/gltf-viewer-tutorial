#version 330

in vec3 vViewSpacePosition;
in vec3 vViewSpaceNormal;
in vec2 vTexCoords;

uniform vec3 uLightDirection;
uniform vec3 uLightRadiance;

out vec3 fColor;

void main()
{
	vec3 viewSpaceNormal = normalize(vViewSpaceNormal);

	fColor = vec3(1.0f/3.14f) * uLightRadiance * dot(viewSpaceNormal, uLightDirection);
}
