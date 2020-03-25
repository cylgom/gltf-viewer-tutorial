#version 330

layout (location = 0) in vec3 aPosition;

out vec3 vViewSpacePosition;

uniform mat4 uModelProjMatrix;
uniform mat4 uModelViewMatrix;

void main()
{
	vViewSpacePosition = aPosition;

	mat4 rotView = mat4(mat3(uModelViewMatrix));
	vec4 clipPos = uModelProjMatrix * rotView * vec4(vViewSpacePosition, 1.0);

	gl_Position = clipPos.xyww;
}
