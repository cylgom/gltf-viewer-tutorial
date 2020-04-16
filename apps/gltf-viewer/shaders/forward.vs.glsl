#version 330

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;

out vec2 vTexCoords;
out vec3 vWorldSpaceNormal;
out vec3 vWorldSpacePosition;

uniform mat4 uNormalMatrix;
uniform mat4 uModelViewProjMatrix;
uniform mat4 uModelViewMatrix;
uniform mat4 uModelMatrix;

void main()
{
	vTexCoords = aTexCoords;
    vWorldSpacePosition = vec3(uModelMatrix * vec4(aPosition, 1));
	vWorldSpaceNormal = normalize(vec3(uModelMatrix * vec4(aNormal, 1)));
    gl_Position =  uModelViewProjMatrix * vec4(aPosition, 1);
}
