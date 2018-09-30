#include "NVXRaytracing.h"
#include <Rush/MathCommon.h>
#include <Rush/UtilStaticArray.h>

#define V(x)                                                                                                           \
	{                                                                                                                  \
		auto s = x;                                                                                                    \
		RUSH_UNUSED(s);                                                                                                \
		RUSH_ASSERT_MSG(s == VK_SUCCESS, #x " call failed");                                                           \
	}

void NVXRaytracing::createPipeline(const GfxShaderSource& rgen, const GfxShaderSource& rmiss)
{
	GfxDevice* device = Platform_GetGfxDevice();
	VkDevice vulkanDevice = device->m_vulkanDevice;

	{
		VkShaderModuleCreateInfo moduleCreateInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		moduleCreateInfo.codeSize = rgen.size();
		moduleCreateInfo.pCode = (const u32*)rgen.data();
		V(vkCreateShaderModule(vulkanDevice, &moduleCreateInfo, nullptr, &m_rayGenShader));
	}

	{
		VkShaderModuleCreateInfo moduleCreateInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		moduleCreateInfo.codeSize = rmiss.size();
		moduleCreateInfo.pCode = (const u32*)rmiss.data();
		V(vkCreateShaderModule(vulkanDevice, &moduleCreateInfo, nullptr, &m_rayMissShader));
	}

	// create descriptor set layout

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };

	StaticArray<VkDescriptorSetLayoutBinding, 5> bindings; // TODO: configurable bindings based on GfxShaderBindingDesc

	{
		VkDescriptorSetLayoutBinding item = {};
		item.binding = 0;
		item.descriptorCount = 1;
		item.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		item.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NVX;
		bindings.pushBack(item);
	}

	{
		VkDescriptorSetLayoutBinding item = {};
		item.binding = 1;
		item.descriptorCount = 1;
		item.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		item.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NVX;
		bindings.pushBack(item);
	}

	{
		VkDescriptorSetLayoutBinding item = {};
		item.binding = 2;
		item.descriptorCount = 1;
		item.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		item.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NVX;
		bindings.pushBack(item);
	}

	{
		VkDescriptorSetLayoutBinding item = {};
		item.binding = 3;
		item.descriptorCount = 1;
		item.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		item.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NVX;
		bindings.pushBack(item);
	}

	{
		VkDescriptorSetLayoutBinding item = {};
		item.binding = 4;
		item.descriptorCount = 1;
		item.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NVX;
		item.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NVX;
		bindings.pushBack(item);
	}

	descriptorSetLayoutCreateInfo.bindingCount = u32(bindings.currentSize);
	descriptorSetLayoutCreateInfo.pBindings = bindings.data;
	V(vkCreateDescriptorSetLayout(vulkanDevice, &descriptorSetLayoutCreateInfo, nullptr, &m_descriptorSetLayout));

	// create pipeline layout

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &m_descriptorSetLayout;

	V(vkCreatePipelineLayout(vulkanDevice, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout));

	// create pipeline

	VkRaytracingPipelineCreateInfoNVX createInfo = { VK_STRUCTURE_TYPE_RAYTRACING_PIPELINE_CREATE_INFO_NVX };

	VkPipelineShaderStageCreateInfo shaderStages[2] = {};

	shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[0].stage = VK_SHADER_STAGE_RAYGEN_BIT_NVX;
	shaderStages[0].module = m_rayGenShader;
	shaderStages[0].pName = "main";

	shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[1].stage = VK_SHADER_STAGE_MISS_BIT_NVX;
	shaderStages[1].module = m_rayMissShader;
	shaderStages[1].pName = "main";

	createInfo.stageCount = 2;
	createInfo.pStages = shaderStages;

	u32 groupNumbers[2] = { 0, 1 };
	createInfo.pGroupNumbers = groupNumbers;
	createInfo.maxRecursionDepth = 1;

	createInfo.layout = m_pipelineLayout;

	V(vkCreateRaytracingPipelinesNVX(vulkanDevice, 
		device->m_pipelineCache,
		1, &createInfo, nullptr, &m_pipeline));

	m_shaderHandleSize = device->m_nvxRaytracingProps.shaderHeaderSize;
	m_shaderHandles.resize(m_shaderHandleSize * 2);

	vkGetRaytracingShaderHandlesNVX(vulkanDevice, m_pipeline,
		0, 2, m_shaderHandleSize, m_shaderHandles.data());

	m_sbtMissStride = m_shaderHandleSize;
	m_sbtHitStride = m_shaderHandleSize;

	m_sbtRaygenOffset = 0 * m_shaderHandleSize;
	m_sbtMissOffset = 1 * m_shaderHandleSize;

	GfxBufferDesc instanceBufferDesc;
	instanceBufferDesc.flags = GfxBufferFlags::None;
	instanceBufferDesc.count = 2;
	instanceBufferDesc.stride = m_shaderHandleSize;

	m_sbtBuffer = Gfx_CreateBuffer(instanceBufferDesc, m_shaderHandles.data());
}

void NVXRaytracing::build(GfxContext * ctx,
	GfxBuffer vertexBuffer, u32 vertexCount, GfxFormat positionFormat, u32 vertexStride,
	GfxBuffer indexBuffer, u32 indexCount, GfxFormat indexFormat)
{
	RUSH_ASSERT(indexFormat == GfxFormat_R32_Uint || indexFormat == GfxFormat_R16_Uint);

	GfxDevice* device = Platform_GetGfxDevice();
	VkDevice vulkanDevice = device->m_vulkanDevice;

	const BufferVK& vertexBufferVK = device->m_buffers[vertexBuffer];
	const BufferVK& indexBufferVK = device->m_buffers[indexBuffer];

	VkGeometryTrianglesNVX triangles = { VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NVX };
	triangles.vertexData = vertexBufferVK.info.buffer;
	triangles.vertexOffset = vertexBufferVK.info.offset;
	triangles.vertexCount = vertexCount;
	triangles.vertexStride = vertexStride;
	triangles.vertexFormat = Gfx_vkConvertFormat(positionFormat);
	triangles.indexData = indexBufferVK.info.buffer;
	triangles.indexOffset = indexBufferVK.info.offset;
	triangles.indexCount = indexCount;
	triangles.indexType = indexFormat == GfxFormat_R32_Uint ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;

	VkGeometryNVX geometry = { VK_STRUCTURE_TYPE_GEOMETRY_NVX };
	geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_NVX;	
	geometry.geometry.triangles = triangles;
	geometry.flags = VK_GEOMETRY_OPAQUE_BIT_NVX;

	VkAccelerationStructureCreateInfoNVX blasCreateInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NVX };
	blasCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NVX;
	blasCreateInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NVX;
	blasCreateInfo.geometryCount = 1;
	blasCreateInfo.pGeometries = &geometry;

	VkAccelerationStructureCreateInfoNVX tlasCreateInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NVX };
	tlasCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NVX;
	tlasCreateInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NVX;
	tlasCreateInfo.instanceCount = 1;

	V(vkCreateAccelerationStructureNVX(vulkanDevice, &blasCreateInfo, nullptr, &m_blas));
	V(vkCreateAccelerationStructureNVX(vulkanDevice, &tlasCreateInfo, nullptr, &m_tlas));

	VkMemoryRequirements2KHR blasMemoryReq = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
	VkMemoryRequirements2KHR tlasMemoryReq = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
	VkMemoryRequirements2KHR blasScratchMemoryReq = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
	VkMemoryRequirements2KHR tlasScratchMemoryReq = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };

	{
		VkAccelerationStructureMemoryRequirementsInfoNVX memoryReqInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NVX };
		memoryReqInfo.accelerationStructure = m_blas;
		vkGetAccelerationStructureMemoryRequirementsNVX(vulkanDevice, &memoryReqInfo, &blasMemoryReq);
		vkGetAccelerationStructureScratchMemoryRequirementsNVX(vulkanDevice, &memoryReqInfo, &blasScratchMemoryReq);
	}
	
	{
		VkAccelerationStructureMemoryRequirementsInfoNVX memoryReqInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NVX };
		memoryReqInfo.accelerationStructure = m_tlas;
		vkGetAccelerationStructureMemoryRequirementsNVX(vulkanDevice, &memoryReqInfo, &tlasMemoryReq);
		vkGetAccelerationStructureScratchMemoryRequirementsNVX(vulkanDevice, &memoryReqInfo, &tlasScratchMemoryReq);
	}

	RUSH_ASSERT(blasScratchMemoryReq.memoryRequirements.memoryTypeBits == tlasScratchMemoryReq.memoryRequirements.memoryTypeBits);
	RUSH_ASSERT(blasMemoryReq.memoryRequirements.memoryTypeBits == tlasMemoryReq.memoryRequirements.memoryTypeBits);

	m_scratchBufferSize = u32(max(blasScratchMemoryReq.memoryRequirements.size, tlasScratchMemoryReq.memoryRequirements.size));
	
	m_blasMemoryOffset = 0;
	m_tlasMemoryOffset = u32(alignCeiling(blasMemoryReq.memoryRequirements.size, tlasMemoryReq.memoryRequirements.alignment));

	{
		u64 totalASMemorySize = m_tlasMemoryOffset + blasMemoryReq.memoryRequirements.size;
		VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		allocInfo.memoryTypeIndex = device->memoryTypeFromProperties(
			blasMemoryReq.memoryRequirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		allocInfo.allocationSize = totalASMemorySize;
		V(vkAllocateMemory(vulkanDevice, &allocInfo, nullptr, &m_memory));
	}

	{
		VkBindAccelerationStructureMemoryInfoNVX bindInfos[2] = {};
		for (auto& it : bindInfos) it.sType = VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NVX;

		bindInfos[0].accelerationStructure = m_blas;
		bindInfos[0].memory = m_memory;
		bindInfos[0].memoryOffset = m_blasMemoryOffset;

		bindInfos[1].accelerationStructure = m_tlas;
		bindInfos[1].memory = m_memory;
		bindInfos[1].memoryOffset = m_tlasMemoryOffset;

		V(vkBindAccelerationStructureMemoryNVX(vulkanDevice, 2, bindInfos));
	}

	{
		VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferCreateInfo.usage = VK_BUFFER_USAGE_RAYTRACING_BIT_NVX;
		bufferCreateInfo.size = m_scratchBufferSize;

		V(vkCreateBuffer(vulkanDevice, &bufferCreateInfo, nullptr, &m_scratchBuffer));

		VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		allocInfo.allocationSize = m_scratchBufferSize;
		allocInfo.memoryTypeIndex = device->memoryTypeFromProperties(
			blasScratchMemoryReq.memoryRequirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		V(vkAllocateMemory(vulkanDevice, &allocInfo, nullptr, &m_scratchMemory));
		V(vkBindBufferMemory(vulkanDevice, m_scratchBuffer, m_scratchMemory, 0));
	}

	// build bottom level acceleration structure

	vkCmdBuildAccelerationStructureNVX(ctx->m_commandBuffer,
		blasCreateInfo.type,
		0, VK_NULL_HANDLE, 0,
		1, &geometry, blasCreateInfo.flags, VK_FALSE,
		m_blas, VK_NULL_HANDLE,
		m_scratchBuffer, m_scratchBufferSize);

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
	instanceData.transform[4] = 1;
	instanceData.transform[8] = 1;
	vkGetAccelerationStructureHandleNVX(vulkanDevice, m_blas, 8, &instanceData.accelerationStructureHandle);

	GfxBufferDesc instanceBufferDesc;
	instanceBufferDesc.flags = GfxBufferFlags::Transient;
	instanceBufferDesc.count = 1;
	instanceBufferDesc.stride = sizeof(instanceData);

	GfxBuffer instanceBuffer = Gfx_CreateBuffer(instanceBufferDesc, &instanceData);
	BufferVK& instanceBufferVK = device->m_buffers[instanceBuffer];

	VkBuffer instanceDataBuffer = VK_NULL_HANDLE;
	vkCmdBuildAccelerationStructureNVX(ctx->m_commandBuffer,
		tlasCreateInfo.type, 
		1, instanceBufferVK.info.buffer, instanceBufferVK.info.offset,
		0, nullptr, tlasCreateInfo.flags, VK_FALSE,
		m_tlas, VK_NULL_HANDLE,
		m_scratchBuffer, m_scratchBufferSize);

	Gfx_vkFullPipelineBarrier(ctx);
}

void NVXRaytracing::dispatch(GfxContext* ctx,
	u32 width, u32 height,
	GfxBuffer constants,
	GfxSampler pointSampler,
	GfxTexture positionTexture,
	GfxTexture outputShadowMask)
{
	ctx->flushBarriers();

	GfxDevice* device = Platform_GetGfxDevice();
	VkDevice vulkanDevice = device->m_vulkanDevice;

	VkDescriptorSetAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	allocInfo.descriptorPool = device->m_currentFrame->descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &m_descriptorSetLayout;

	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	V(vkAllocateDescriptorSets(vulkanDevice, &allocInfo, &descriptorSet));

	StaticArray<VkWriteDescriptorSet, 5> descriptors;

	{
		VkWriteDescriptorSet item = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		item.dstSet = descriptorSet;
		item.dstBinding = 0;
		item.descriptorCount = 1;
		item.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
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
	outputImageInfo.imageView = device->m_textures[positionTexture].imageView;
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

	VkDescriptorAccelerationStructureInfoNVX tlasInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_ACCELERATION_STRUCTURE_INFO_NVX };
	tlasInfo.accelerationStructureCount = 1;
	tlasInfo.pAccelerationStructures = &m_tlas;

	{

		VkWriteDescriptorSet item = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		item.dstSet = descriptorSet;
		item.dstBinding = 4;
		item.descriptorCount = 1;
		item.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NVX;
		item.pNext = &tlasInfo;
		descriptors.pushBack(item);
	}

	vkUpdateDescriptorSets(vulkanDevice,
		u32(descriptors.currentSize),
		descriptors.data, 
		0, nullptr);

	vkCmdBindPipeline(ctx->m_commandBuffer,
		VK_PIPELINE_BIND_POINT_RAYTRACING_NVX,
		m_pipeline);

	vkCmdBindDescriptorSets(ctx->m_commandBuffer,
		VK_PIPELINE_BIND_POINT_RAYTRACING_NVX,
		m_pipelineLayout, 0, 1, &descriptorSet,
		0, nullptr);

	const BufferVK& sbtBufferVK = device->m_buffers[m_sbtBuffer];
	VkBuffer sbtBuffer = sbtBufferVK.info.buffer;

	vkCmdTraceRaysNVX(ctx->m_commandBuffer,
		sbtBuffer, sbtBufferVK.info.offset + m_sbtRaygenOffset,
		sbtBuffer, sbtBufferVK.info.offset + m_sbtMissOffset, m_sbtMissStride,
		sbtBuffer, sbtBufferVK.info.offset + m_sbtHitOffset, m_sbtHitStride,
		width, height);

	ctx->m_dirtyState |= GfxContext::DirtyStateFlag_Pipeline;
}

void NVXRaytracing::reset()
{
	GfxDevice* device = Platform_GetGfxDevice();
	VkDevice vulkanDevice = device->m_vulkanDevice;

	Gfx_DestroyBuffer(m_sbtBuffer);

	// TODO: Enqueue destruction

	vkDestroyShaderModule(vulkanDevice, m_rayGenShader, nullptr);
	vkDestroyShaderModule(vulkanDevice, m_rayMissShader, nullptr);

	vkDestroyDescriptorSetLayout(vulkanDevice, m_descriptorSetLayout, nullptr);
	vkDestroyPipelineLayout(vulkanDevice, m_pipelineLayout, nullptr);

	vkDestroyPipeline(vulkanDevice, m_pipeline, nullptr);

	vkFreeMemory(vulkanDevice, m_memory, nullptr);
	vkFreeMemory(vulkanDevice, m_scratchMemory, nullptr);

	vkDestroyBuffer(vulkanDevice, m_scratchBuffer, nullptr);

	vkDestroyAccelerationStructureNVX(vulkanDevice, m_blas, nullptr);
	vkDestroyAccelerationStructureNVX(vulkanDevice, m_tlas, nullptr);
}

