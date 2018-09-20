#version 450

layout (binding = 0) uniform Constants
{
	vec4 cameraPosition;
	vec4 cameraDirection;
	vec4 lightDirection;
	vec4 renderTargetSize;
};

layout (binding = 1) uniform sampler defaultSampler;
layout (binding = 2) uniform texture2D gbufferBaseColorTexture;
layout (binding = 3) uniform texture2D gbufferNormalTexture;
layout (binding = 4) uniform texture2D shadowMaskTexture;

layout (location = 0) out vec4 fragColor;

void main()
{
	vec2 texcoord = gl_FragCoord.xy * renderTargetSize.zw;

	vec3 worldNormal = texture(sampler2D(gbufferNormalTexture, defaultSampler), texcoord).xyz;
	vec3 baseColor = texture(sampler2D(gbufferBaseColorTexture, defaultSampler), texcoord).xyz;
	float shadowMask = texture(sampler2D(shadowMaskTexture, defaultSampler), texcoord).x;

	vec4 result;

	float directLight = 1.25 * max(0.0, dot(worldNormal, lightDirection.xyz)) * shadowMask;
	float ambientLight = 0.15 + 0.05 * (1.0 - max(0.0f, dot(worldNormal, -cameraDirection.xyz)));

	result.xyz = baseColor * vec3(directLight + ambientLight);
	result.w = 1.0;

	if (worldNormal.rgb == vec3(0.0))
		discard;

	fragColor = result;
}
