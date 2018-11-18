#version 460
#extension GL_NV_ray_tracing : enable

layout(location = 0) rayPayloadInNV uint payload;

void main()
{
	payload = 1;
}
