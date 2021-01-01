#version 460
#extension GL_EXT_ray_tracing : enable

layout(location = 0) rayPayloadInEXT uint payload;

void main()
{
	payload = 1;
}
