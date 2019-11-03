#include "NVRaytracing.h"
#include <Rush/GfxDeviceVK.h>
#include <Rush/MathCommon.h>
#include <Rush/UtilArray.h>

#define V(x)                                                                                                           \
	{                                                                                                                  \
		auto s = x;                                                                                                    \
		RUSH_UNUSED(s);                                                                                                \
		RUSH_ASSERT_MSG(s == VK_SUCCESS, #x " call failed");                                                           \
	}

void NVRaytracing::createPipeline(const GfxShaderSource& rgen, const GfxShaderSource& rmiss)
{
	GfxDevice* device = Platform_GetGfxDevice();
	VkDevice vulkanDevice = device->m_vulkanDevice;

	GfxRayTracingPipelineDesc desc;
	desc.rayGen = rgen;
	desc.miss = rmiss;
	desc.bindings.constantBuffers = 1;
	desc.bindings.samplers = 1;
	desc.bindings.textures = 1;
	desc.bindings.rwImages = 1;
	desc.bindings.accelerationStructures = 1;

	m_pipeline = Gfx_CreateRayTracingPipeline(desc);

	// TODO: get the native pipeline out of the device while abstraction layer is WIP
	RayTracingPipelineVK& pipeline = device->m_rayTracingPipelines[m_pipeline.get()];

	const u32 shaderHandleSize = Gfx_GetCapability().rtShaderHandleSize;

	// TODO: SBT should take into account alignment requirements
	GfxBufferDesc sbtBufferDesc;
	sbtBufferDesc.flags = GfxBufferFlags::None;
	sbtBufferDesc.count = u32(pipeline.shaderHandles.size() / shaderHandleSize);
	sbtBufferDesc.stride = shaderHandleSize;

	m_sbtBuffer = Gfx_CreateBuffer(sbtBufferDesc, pipeline.shaderHandles.data());
}

void NVRaytracing::build(GfxContext* ctx,
	GfxBuffer vertexBuffer, u32 vertexCount, GfxFormat positionFormat, u32 vertexStride,
	GfxBuffer indexBuffer, u32 indexCount, GfxFormat indexFormat)
{
	GfxRayTracingGeometryDesc geometryDesc;
	geometryDesc.vertexBuffer = vertexBuffer;
	geometryDesc.vertexCount = vertexCount;
	geometryDesc.vertexFormat = positionFormat;
	geometryDesc.vertexStride = vertexStride;
	geometryDesc.indexBuffer = indexBuffer;
	geometryDesc.indexCount = indexCount;
	geometryDesc.indexFormat = indexFormat;
	geometryDesc.isOpaque = true;

	GfxAccelerationStructureDesc blasDesc;
	blasDesc.type = GfxAccelerationStructureType::BottomLevel;
	blasDesc.geometries = &geometryDesc;
	blasDesc.geometyCount = 1;

	m_blas = Gfx_CreateAccelerationStructure(blasDesc);

	GfxAccelerationStructureDesc tlasDesc;
	tlasDesc.type = GfxAccelerationStructureType::TopLevel;
	tlasDesc.instanceCount = 1;

	m_tlas = Gfx_CreateAccelerationStructure(tlasDesc);

	GfxOwn<GfxBuffer> instanceBuffer = Gfx_CreateBuffer(GfxBufferDesc(GfxBufferFlags::Transient, 0, 0));
	{
		auto instanceData = Gfx_BeginUpdateBuffer<GfxRayTracingInstanceDesc>(ctx, instanceBuffer.get(), tlasDesc.instanceCount);
		instanceData[0].init();
		instanceData[0].accelerationStructureHandle = Gfx_GetAccelerationStructureHandle(m_blas);
		Gfx_EndUpdateBuffer(ctx, instanceBuffer);
	}

	Gfx_BuildAccelerationStructure(ctx, m_blas);
	Gfx_vkFullPipelineBarrier(ctx);

	Gfx_BuildAccelerationStructure(ctx, m_tlas, instanceBuffer);
	Gfx_vkFullPipelineBarrier(ctx);
}

void NVRaytracing::dispatch(GfxContext* ctx,
	u32 width, u32 height,
	GfxBuffer constants,
	GfxSampler pointSampler,
	GfxTexture positionTexture,
	GfxTexture outputShadowMask)
{
	Gfx_SetConstantBuffer(ctx, 0, constants);
	Gfx_SetSampler(ctx, 0, pointSampler);
	Gfx_SetTexture(ctx, 0, positionTexture);
	Gfx_SetStorageImage(ctx, 0, outputShadowMask);
	Gfx_TraceRays(ctx, m_pipeline, m_tlas, m_sbtBuffer, width, height, 1);
}

void NVRaytracing::reset()
{
	// TODO: Enqueue destruction to avoid wait-for-idle
	Gfx_Finish();

	m_pipeline.reset();
	m_tlas.reset();
	m_blas.reset();
	m_sbtBuffer.reset();
}

