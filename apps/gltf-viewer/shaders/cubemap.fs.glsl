#version 330

in vec3 vViewSpacePosition;

uniform sampler2D uEquirectangularMap;

out vec4 fColor;

vec2 SampleSphericalMap(vec3 v)
{
	const vec2 invAtan = vec2(0.1591, 0.3183);

    return vec2(atan(v.z, v.x), asin(v.y)) * invAtan + 0.5;
}

void main()
{		
    vec2 uv = SampleSphericalMap(normalize(vViewSpacePosition));
    vec3 color = texture(uEquirectangularMap, uv).rgb;
    fColor = vec4(color, 1.0);
}
