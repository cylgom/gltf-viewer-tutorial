#version 330

in vec3 vViewSpacePosition;
  
uniform samplerCube uEnvironmentMap;

out vec4 fColor;
  
void main()
{
    vec3 envColor = texture(uEnvironmentMap, vViewSpacePosition).rgb;
    
    envColor = envColor / (envColor + vec3(1.0));
    envColor = pow(envColor, vec3(1.0/2.2)); 
  
    fColor = vec4(envColor, 1.0);
}
