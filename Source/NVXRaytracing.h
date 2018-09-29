#pragma once

#include <Rush/GfxCommon.h>
#include <Rush/GfxDeviceVK.h>

class NVXRaytracing
{
public:

	~NVXRaytracing() { reset(); }

	void build(GfxContext* ctx,
		GfxBuffer vertexBuffer, u32 vertexCount, GfxFormat positionFormat, u32 vertexStride,
		GfxBuffer indexBuffer, u32 indexCount, GfxFormat indexFormat);

	void reset();

	VkAccelerationStructureNVX m_blas = VK_NULL_HANDLE;
	VkAccelerationStructureNVX m_tlas = VK_NULL_HANDLE;

	VkDeviceMemory m_memory = VK_NULL_HANDLE;
	u32 m_blasMemoryOffset = 0;
	u32 m_tlasMemoryOffset = 0;

	VkBuffer m_scratchBuffer = VK_NULL_HANDLE;
	VkDeviceMemory m_scratchMemory = VK_NULL_HANDLE;
	u32 m_scratchBufferSize = 0;
};

