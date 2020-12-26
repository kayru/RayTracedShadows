#pragma once

#include <Rush/GfxCommon.h>

#include <vector>

class VkRaytracing
{
public:

	~VkRaytracing() { reset(); }

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

	GfxOwn<GfxAccelerationStructure> m_blas;
	GfxOwn<GfxAccelerationStructure> m_tlas;

	// Pipeline and SBT

	GfxOwn<GfxRayTracingPipeline> m_pipeline;
	GfxOwn<GfxBuffer> m_sbtBuffer;

private:

	void reset();

};

