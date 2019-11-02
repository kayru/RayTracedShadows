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

	GfxOwn<GfxAccelerationStructure> m_blas;
	GfxOwn<GfxAccelerationStructure> m_tlas;

	// Pipeline and SBT

	GfxOwn<GfxRayTracingPipeline> m_rtPipeline;

	GfxOwn<GfxBuffer> m_sbtBuffer;

	u32 m_sbtRaygenOffset = 0;
	u32 m_sbtMissOffset = 0;
	u32 m_sbtMissStride = 0;
	u32 m_sbtHitOffset = 0;
	u32 m_sbtHitStride = 0;
private:


	void reset();

};

