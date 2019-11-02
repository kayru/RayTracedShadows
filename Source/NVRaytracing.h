#pragma once

#include <Rush/GfxCommon.h>
#include <Rush/GfxDeviceVK.h>

#include <vector>

class NVRaytracing
{
public:

	~NVRaytracing() { reset(); }

	void createPipeline(const GfxShaderSource& rgen, const GfxShaderSource& rmiss);

	void build(GfxContext* ctx,
		GfxBuffer vertexBuffer, u32 vertexCount, GfxFormat positionFormat, u32 vertexStride,
		GfxBuffer indexBuffer, u32 indexCount, GfxFormat indexFormat);

	void dispatch(GfxContext* ctx,
		u32 width, u32 height,
		GfxBuffer constants,
		GfxSampler pointSampler,
		GfxTexture positionTexture,
		GfxTexture outputShadowMask);

	void reset();

	VkAccelerationStructureNV m_blas = VK_NULL_HANDLE;
	VkAccelerationStructureNV m_tlas = VK_NULL_HANDLE;

	VkDeviceMemory m_memory = VK_NULL_HANDLE;
	u32 m_blasMemoryOffset = 0;
	u32 m_tlasMemoryOffset = 0;

	VkBuffer m_scratchBuffer = VK_NULL_HANDLE;
	VkDeviceMemory m_scratchMemory = VK_NULL_HANDLE;
	u32 m_scratchBufferSize = 0;

	// Pipeline and SBT

	GfxOwn<GfxRayTracingPipeline> m_rtPipeline;

	GfxOwn<GfxBuffer> m_sbtBuffer;

	u32 m_sbtRaygenOffset = 0;
	u32 m_sbtMissOffset = 0;
	u32 m_sbtMissStride = 0;
	u32 m_sbtHitOffset = 0;
	u32 m_sbtHitStride = 0;
};

