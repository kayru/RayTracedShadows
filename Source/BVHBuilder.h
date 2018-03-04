#pragma once

#include <Rush/GfxDevice.h>
#include <Rush/GfxRef.h>
#include <Rush/MathTypes.h>

#include <vector>

struct BVHNode
{
	static const u32 LeafMask = 0x80000000;
	static const u32 InvalidMask = 0xFFFFFFFF;

	Vec3 bboxMin;
	u32 prim = InvalidMask;

	Vec3 bboxMax;
	u32 next = InvalidMask;

	bool isLeaf() const { return prim != InvalidMask; }
};

struct BVHPackedNode
{
	u32 a, b, c, d;
};

struct BVHBuilder
{
	std::vector<BVHNode> m_nodes;
	std::vector<BVHPackedNode> m_packedNodes;
	void build(const float* vertices, u32 stride, const u32* indices, u32 primCount);
};


