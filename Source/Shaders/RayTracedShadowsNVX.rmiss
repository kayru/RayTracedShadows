#version 460
#extension GL_NVX_raytracing : enable

layout(location = 0) rayPayloadNVX uint payload;

void main()
{
	payload = 1;
}
