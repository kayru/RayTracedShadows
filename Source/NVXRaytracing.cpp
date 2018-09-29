#include "NVXRaytracing.h"
#include <Rush/MathCommon.h>

#define V(x)                                                                                                           \
	{                                                                                                                  \
		auto s = x;                                                                                                    \
		RUSH_UNUSED(s);                                                                                                \
		RUSH_ASSERT_MSG(s == VK_SUCCESS, #x " call failed");                                                           \
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

void NVXRaytracing::reset()
{
	GfxDevice* device = Platform_GetGfxDevice();
	VkDevice vulkanDevice = device->m_vulkanDevice;

	// TODO: Enqueue destruction

	vkFreeMemory(vulkanDevice, m_memory, nullptr);
	vkFreeMemory(vulkanDevice, m_scratchMemory, nullptr);

	vkDestroyBuffer(vulkanDevice, m_scratchBuffer, nullptr);

	vkDestroyAccelerationStructureNVX(vulkanDevice, m_blas, nullptr);
	vkDestroyAccelerationStructureNVX(vulkanDevice, m_tlas, nullptr);
}

