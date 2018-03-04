#version 450

layout (binding = 0) uniform Constants
{
	vec4 cameraPosition;
	vec4 cameraDirection;
	vec4 lightDirection;
	vec4 renderTargetSize;
};

layout (binding = 1) uniform sampler2D gbufferBaseColorSampler;
layout (binding = 2) uniform sampler2D gbufferNormalSampler;
layout (binding = 3) uniform sampler2D shadowMaskSampler;

layout (location = 0) out vec4 fragColor;

void main()
{
	vec2 texcoord = gl_FragCoord.xy * renderTargetSize.zw;

	vec3 worldNormal = texture(gbufferNormalSampler, texcoord).xyz;
	vec3 baseColor = texture(gbufferBaseColorSampler, texcoord).xyz;
	float shadowMask = texture(shadowMaskSampler, texcoord).x;

	vec4 result;

	float directLight = 1.25 * max(0.0, dot(worldNormal, lightDirection.xyz)) * shadowMask;
	float ambientLight = 0.15 + 0.05 * (1.0 - max(0.0f, dot(worldNormal, -cameraDirection.xyz)));

	result.xyz = baseColor * vec3(directLight + ambientLight);
	result.w = 1.0;

	if (worldNormal.rgb == vec3(0.0))
		discard;

	fragColor = result;
}
