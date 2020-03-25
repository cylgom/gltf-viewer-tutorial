#version 330

layout(location = 0) in vec3 aPosition;

out vec3 vViewSpacePosition;

uniform mat4 uModelViewMatrix;
uniform mat4 uModelProjMatrix;

void main()
{
    vViewSpacePosition = aPosition;
    gl_Position = uModelProjMatrix * uModelViewMatrix * vec4(aPosition, 1);
}
