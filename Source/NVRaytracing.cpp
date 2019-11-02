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
	sbtBufferDesc.count = pipeline.shaderHandles.size() / shaderHandleSize;
	sbtBufferDesc.stride = shaderHandleSize;

	m_sbtBuffer = Gfx_CreateBuffer(sbtBufferDesc, pipeline.shaderHandles.data());
}

void NVRaytracing::build(GfxContext * ctx,
	GfxBuffer vertexBuffer, u32 vertexCount, GfxFormat positionFormat, u32 vertexStride,
	GfxBuffer indexBuffer, u32 indexCount, GfxFormat indexFormat)
{
	RUSH_ASSERT(indexFormat == GfxFormat_R32_Uint || indexFormat == GfxFormat_R16_Uint);
	RUSH_ASSERT(ctx->m_isActive);

	GfxDevice* device = Platform_GetGfxDevice();
	VkDevice vulkanDevice = device->m_vulkanDevice;

	const BufferVK& vertexBufferVK = device->m_buffers[vertexBuffer];
	const BufferVK& indexBufferVK = device->m_buffers[indexBuffer];

	VkGeometryTrianglesNV triangles = { VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV };
	triangles.vertexData = vertexBufferVK.info.buffer;
	triangles.vertexOffset = vertexBufferVK.info.offset;
	triangles.vertexCount = vertexCount;
	triangles.vertexStride = vertexStride;
	triangles.vertexFormat = Gfx_vkConvertFormat(positionFormat);
	triangles.indexData = indexBufferVK.info.buffer;
	triangles.indexOffset = indexBufferVK.info.offset;
	triangles.indexCount = indexCount;
	triangles.indexType = indexFormat == GfxFormat_R32_Uint ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;

	VkGeometryNV geometry = { VK_STRUCTURE_TYPE_GEOMETRY_NV };
	geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_NV;	
	geometry.geometry.triangles = triangles;
	geometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV;
	geometry.flags = VK_GEOMETRY_OPAQUE_BIT_NV;

	VkAccelerationStructureInfoNV blasInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV };
	blasInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
	blasInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NV;
	blasInfo.geometryCount = 1;
	blasInfo.pGeometries = &geometry;

	VkAccelerationStructureCreateInfoNV blasCreateInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV };
	blasCreateInfo.info = blasInfo;

	VkAccelerationStructureInfoNV tlasInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV };
	tlasInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;
	tlasInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NV;
	tlasInfo.instanceCount = 1;

	VkAccelerationStructureCreateInfoNV tlasCreateInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV };
	tlasCreateInfo.info = tlasInfo;

	V(vkCreateAccelerationStructureNV(vulkanDevice, &blasCreateInfo, nullptr, &m_blas));
	V(vkCreateAccelerationStructureNV(vulkanDevice, &tlasCreateInfo, nullptr, &m_tlas));

	VkMemoryRequirements2KHR blasMemoryReq = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
	VkMemoryRequirements2KHR tlasMemoryReq = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
	VkMemoryRequirements2KHR blasScratchMemoryReq = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
	VkMemoryRequirements2KHR tlasScratchMemoryReq = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };

	{
		VkAccelerationStructureMemoryRequirementsInfoNV memoryReqInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV };
		memoryReqInfo.accelerationStructure = m_blas;
		memoryReqInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;
		vkGetAccelerationStructureMemoryRequirementsNV(vulkanDevice, &memoryReqInfo, &blasMemoryReq);

		memoryReqInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_NV;
		vkGetAccelerationStructureMemoryRequirementsNV(vulkanDevice, &memoryReqInfo, &blasScratchMemoryReq);
	}

	{
		VkAccelerationStructureMemoryRequirementsInfoNV memoryReqInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV };
		memoryReqInfo.accelerationStructure = m_tlas;
		memoryReqInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;

		vkGetAccelerationStructureMemoryRequirementsNV(vulkanDevice, &memoryReqInfo, &tlasMemoryReq);

		memoryReqInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_NV;
		vkGetAccelerationStructureMemoryRequirementsNV(vulkanDevice, &memoryReqInfo, &tlasScratchMemoryReq);
	}

	RUSH_ASSERT(blasScratchMemoryReq.memoryRequirements.memoryTypeBits == tlasScratchMemoryReq.memoryRequirements.memoryTypeBits);
	RUSH_ASSERT(blasMemoryReq.memoryRequirements.memoryTypeBits == tlasMemoryReq.memoryRequirements.memoryTypeBits);

	m_scratchBufferSize = u32(max(blasScratchMemoryReq.memoryRequirements.size, tlasScratchMemoryReq.memoryRequirements.size));
	
	m_blasMemoryOffset = 0;
	m_tlasMemoryOffset = u32(alignCeiling(blasMemoryReq.memoryRequirements.size, tlasMemoryReq.memoryRequirements.alignment));

	{
		u64 totalASMemorySize = m_tlasMemoryOffset + tlasMemoryReq.memoryRequirements.size;
		VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		allocInfo.memoryTypeIndex = device->memoryTypeFromProperties(
			blasMemoryReq.memoryRequirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		allocInfo.allocationSize = totalASMemorySize;
		V(vkAllocateMemory(vulkanDevice, &allocInfo, nullptr, &m_memory));
	}

	{
		VkBindAccelerationStructureMemoryInfoNV bindInfos[2] = {};
		for (auto& it : bindInfos) it.sType = VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV;

		bindInfos[0].accelerationStructure = m_blas;
		bindInfos[0].memory = m_memory;
		bindInfos[0].memoryOffset = m_blasMemoryOffset;

		bindInfos[1].accelerationStructure = m_tlas;
		bindInfos[1].memory = m_memory;
		bindInfos[1].memoryOffset = m_tlasMemoryOffset;

		V(vkBindAccelerationStructureMemoryNV(vulkanDevice, 2, bindInfos));
	}

	{
		VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferCreateInfo.usage = VK_BUFFER_USAGE_RAY_TRACING_BIT_NV;
		bufferCreateInfo.size = m_scratchBufferSize;

		V(vkCreateBuffer(vulkanDevice, &bufferCreateInfo, nullptr, &m_scratchBuffer));

		VkMemoryRequirements scratchBufferRequirements = {};
		vkGetBufferMemoryRequirements(vulkanDevice, m_scratchBuffer, &scratchBufferRequirements);

		VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		allocInfo.allocationSize = max(m_scratchBufferSize, scratchBufferRequirements.size);
		allocInfo.memoryTypeIndex = device->memoryTypeFromProperties(
			blasScratchMemoryReq.memoryRequirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		V(vkAllocateMemory(vulkanDevice, &allocInfo, nullptr, &m_scratchMemory));
		V(vkBindBufferMemory(vulkanDevice, m_scratchBuffer, m_scratchMemory, 0));
	}

	// build bottom level acceleration structure

	vkCmdBuildAccelerationStructureNV(ctx->m_commandBuffer,
		&blasInfo,
		VK_NULL_HANDLE, 0, // instance data, instance offset
		VK_FALSE, // update
		m_blas, VK_NULL_HANDLE, // dest, source
		m_scratchBuffer, 0);

	Gfx_vkFullPipelineBarrier(ctx);

	// build top level acceleration structure

	struct InstanceDesc
	{
		float transform[12];
		u32 instanceID : 24;
		u32 instanceMask : 8;
		u32 instanceContributionToHitGroupIndex : 24;
		u32 flags : 8;
		u64 accelerationStructureHandle;
	};

	InstanceDesc instanceData = {};
	instanceData.transform[0] = 1;
	instanceData.transform[5] = 1;
	instanceData.transform[10] = 1;
	instanceData.instanceID = 0;
	instanceData.instanceMask = 0xFF;
	instanceData.instanceContributionToHitGroupIndex = 0;
	instanceData.flags = 0;

	vkGetAccelerationStructureHandleNV(vulkanDevice, m_blas, 8, &instanceData.accelerationStructureHandle);

	GfxBufferDesc instanceBufferDesc;
	instanceBufferDesc.flags = GfxBufferFlags::Transient;
	instanceBufferDesc.count = 1;
	instanceBufferDesc.stride = sizeof(instanceData);

	GfxOwn<GfxBuffer> instanceBuffer = Gfx_CreateBuffer(instanceBufferDesc, &instanceData);
	BufferVK& instanceBufferVK = device->m_buffers[instanceBuffer.get()];

	VkBuffer instanceDataBuffer = VK_NULL_HANDLE;
	vkCmdBuildAccelerationStructureNV(ctx->m_commandBuffer,
		&tlasInfo,
		instanceBufferVK.info.buffer, instanceBufferVK.info.offset,
		VK_FALSE, // update
		m_tlas, VK_NULL_HANDLE, // dest, source
		m_scratchBuffer, 0);

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

	VkWriteDescriptorSetAccelerationStructureNV tlasInfo = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV };
	tlasInfo.accelerationStructureCount = 1;
	tlasInfo.pAccelerationStructures = &m_tlas;

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

	ctx->m_dirtyState |= GfxContext::DirtyStateFlag_Pipeline;
}

void NVRaytracing::reset()
{
	GfxDevice* device = Platform_GetGfxDevice();
	VkDevice vulkanDevice = device->m_vulkanDevice;

	m_sbtBuffer.reset();

	// TODO: Enqueue destruction to avoid wait-for-idle

	vkDeviceWaitIdle(vulkanDevice);

	vkFreeMemory(vulkanDevice, m_memory, nullptr);
	vkFreeMemory(vulkanDevice, m_scratchMemory, nullptr);

	vkDestroyBuffer(vulkanDevice, m_scratchBuffer, nullptr);

	vkDestroyAccelerationStructureNV(vulkanDevice, m_blas, nullptr);
	vkDestroyAccelerationStructureNV(vulkanDevice, m_tlas, nullptr);
}

