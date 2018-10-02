#version 460
#extension GL_NVX_raytracing : enable

layout(location = 0) rayPayloadInNVX uint payload;

void main()
{
	payload = 1;
}
