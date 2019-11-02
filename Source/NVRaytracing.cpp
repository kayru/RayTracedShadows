#include "NVRaytracing.h"
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

	m_rtPipeline = Gfx_CreateRayTracingPipeline(desc);

	// TODO: get the native pipeline out of the device while abstraction layer is WIP
	RayTracingPipelineVK& pipeline = device->m_rayTracingPipelines[m_rtPipeline.get()];

	const u32 shaderHandleSize = device->m_nvRayTracingProps.shaderGroupHandleSize;

	m_sbtMissStride = shaderHandleSize;
	m_sbtHitStride = shaderHandleSize;

	// TODO: add API to get handles for specific shaders
	m_sbtRaygenOffset = 0 * shaderHandleSize;
	m_sbtMissOffset = 1 * shaderHandleSize;

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
	ctx->flushBarriers();

	GfxDevice* device = Platform_GetGfxDevice();
	VkDevice vulkanDevice = device->m_vulkanDevice;

	// TODO: get the native pipeline out of the device while abstraction layer is WIP
	RayTracingPipelineVK& pipeline = device->m_rayTracingPipelines[m_rtPipeline.get()];

	VkDescriptorSetAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	allocInfo.descriptorPool = device->m_currentFrame->currentDescriptorPool;
	allocInfo.descriptorSetCount = u32(pipeline.setLayouts.size());
	allocInfo.pSetLayouts = pipeline.setLayouts.data;

	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	V(vkAllocateDescriptorSets(vulkanDevice, &allocInfo, &descriptorSet)); // TODO: handle descriptor pool OOM

	StaticArray<VkWriteDescriptorSet, 5> descriptors;

	{
		VkWriteDescriptorSet item = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		item.dstSet = descriptorSet;
		item.dstBinding = 0;
		item.descriptorCount = 1;
		item.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		item.pBufferInfo = &device->m_buffers[constants].info;
		descriptors.pushBack(item);
	}

	VkDescriptorImageInfo samplerInfo = {};
	samplerInfo.sampler = device->m_samplers[pointSampler].native;

	{
		
		VkWriteDescriptorSet item = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		item.dstSet = descriptorSet;
		item.dstBinding = 1;
		item.descriptorCount = 1;
		item.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		item.pImageInfo = &samplerInfo;
		descriptors.pushBack(item);
	}

	VkDescriptorImageInfo positionImageInfo = {};
	positionImageInfo.imageView = device->m_textures[positionTexture].imageView;
	positionImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	{

		VkWriteDescriptorSet item = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		item.dstSet = descriptorSet;
		item.dstBinding = 2;
		item.descriptorCount = 1;
		item.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		item.pImageInfo = &positionImageInfo;
		descriptors.pushBack(item);
	}

	VkDescriptorImageInfo outputImageInfo = {};
	outputImageInfo.imageView = device->m_textures[outputShadowMask].imageView;
	outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	{

		VkWriteDescriptorSet item = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		item.dstSet = descriptorSet;
		item.dstBinding = 3;
		item.descriptorCount = 1;
		item.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		item.pImageInfo = &outputImageInfo;
		descriptors.pushBack(item);
	}

	AccelerationStructureVK& tlas = device->m_accelerationStructures[m_tlas.get()];

	VkWriteDescriptorSetAccelerationStructureNV tlasInfo = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV };
	tlasInfo.accelerationStructureCount = 1;
	tlasInfo.pAccelerationStructures = &tlas.native;

	{

		VkWriteDescriptorSet item = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		item.dstSet = descriptorSet;
		item.dstBinding = 4;
		item.descriptorCount = 1;
		item.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
		item.pNext = &tlasInfo;
		descriptors.pushBack(item);
	}

	vkUpdateDescriptorSets(vulkanDevice,
		u32(descriptors.currentSize),
		descriptors.data, 
		0, nullptr);

	vkCmdBindPipeline(ctx->m_commandBuffer,
		VK_PIPELINE_BIND_POINT_RAY_TRACING_NV,
		pipeline.pipeline);

	u32 dynamicOffsets[1] = { 0 };
	vkCmdBindDescriptorSets(ctx->m_commandBuffer,
		VK_PIPELINE_BIND_POINT_RAY_TRACING_NV,
		pipeline.pipelineLayout, 0, 1, &descriptorSet,
		RUSH_COUNTOF(dynamicOffsets), dynamicOffsets);

	const BufferVK& sbtBufferVK = device->m_buffers[m_sbtBuffer.get()];
	VkBuffer sbtBuffer = sbtBufferVK.info.buffer;

	vkCmdTraceRaysNV(ctx->m_commandBuffer,
		sbtBuffer, sbtBufferVK.info.offset + m_sbtRaygenOffset,
		sbtBuffer, sbtBufferVK.info.offset + m_sbtMissOffset, m_sbtMissStride,
		sbtBuffer, sbtBufferVK.info.offset + m_sbtHitOffset, m_sbtHitStride,
		VK_NULL_HANDLE, 0, 0, // callable shader table
		width, height, 1);

	ctx->m_dirtyState |= GfxContext::DirtyStateFlag_Pipeline | GfxContext::DirtyStateFlag_Descriptors | GfxContext::DirtyStateFlag_DescriptorSet;
}

void NVRaytracing::reset()
{
	// TODO: Enqueue destruction to avoid wait-for-idle
	Gfx_Finish();

	m_rtPipeline.reset();
	m_tlas.reset();
	m_blas.reset();
	m_sbtBuffer.reset();
}

