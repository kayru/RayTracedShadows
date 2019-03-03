#version 450

layout (binding = 0) uniform Global
{
	mat4 g_matViewProj;
	mat4 g_matWorld;
	vec4 g_cameraPosition;
};

layout (binding = 1) uniform Material
{
	vec4 g_baseColor;
};

layout (binding = 2) uniform sampler defaultSampler;
layout (binding = 3) uniform texture2D albedoTexture;

layout (location = 0) in vec2 v_tex0;
layout (location = 1) in vec3 v_nor0;
layout (location = 2) in vec3 v_worldPos;

layout (location = 0) out vec4 fragColor0;
layout (location = 1) out vec4 fragColor1;
layout (location = 2) out vec4 fragColor2;

void main()
{
	vec4 outBaseColor = vec4(0.0);
	vec4 outNormal = vec4(0.0);
	vec4 outCameraRelativePosition = vec4(0.0);

	outBaseColor = g_baseColor * texture(sampler2D(albedoTexture, defaultSampler), v_tex0);
	outNormal.xyz = normalize(v_nor0);

	outCameraRelativePosition.xyz = v_worldPos.xyz - g_cameraPosition.xyz;

	fragColor0 = outBaseColor;
	fragColor1 = gl_FrontFacing ? outNormal : -outNormal;
	fragColor2 = outCameraRelativePosition;
}
