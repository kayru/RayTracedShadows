#include "BVHBuilder.h"

#include <algorithm>
#include <xmmintrin.h>

namespace
{
struct TempNode : BVHNode
{
	u32 visitOrder = InvalidMask;
	u32 parent = InvalidMask;

	u32 left;
	u32 right;

	Vec3 bboxCenter;

	float primArea = 0.0f;

	float surfaceAreaLeft = 0.0f;
	float surfaceAreaRight = 0.0f;
};

inline float bboxSurfaceArea(const Vec3& bboxMin, const Vec3& bboxMax)
{
	Vec3 extents = bboxMax - bboxMin;
	return (extents.x * extents.y + extents.y * extents.z + extents.z * extents.x) * 2.0f;
}

inline float bboxSurfaceArea(const Box3& bbox)
{
	return bboxSurfaceArea(bbox.m_min, bbox.m_max);
}

inline void setBounds(BVHNode& node, const Vec3& min, const Vec3& max)
{
	node.bboxMin[0] = min.x;
	node.bboxMin[1] = min.y;
	node.bboxMin[2] = min.z;

	node.bboxMax[0] = max.x;
	node.bboxMax[1] = max.y;
	node.bboxMax[2] = max.z;
}

inline Vec3 extractVec3(__m128 v)
{
	alignas(16) float temp[4];
	_mm_store_ps(temp, v);
	return Vec3(temp);
}

Box3 calculateBounds(std::vector<TempNode>& nodes, u32 begin, u32 end)
{
	Box3 bounds;
	if (begin == end)
	{
		bounds.m_min = Vec3(0.0f);
		bounds.m_max = Vec3(0.0f);
	}
	else
	{
		__m128 bboxMin = _mm_set1_ps(FLT_MAX);
		__m128 bboxMax = _mm_set1_ps(-FLT_MAX);
		for (u32 i = begin; i < end; ++i)
		{
			__m128 nodeBoundsMin = _mm_loadu_ps(&nodes[i].bboxMin.x);
			__m128 nodeBoundsMax = _mm_loadu_ps(&nodes[i].bboxMax.x);
			bboxMin = _mm_min_ps(bboxMin, nodeBoundsMin);
			bboxMax = _mm_max_ps(bboxMax, nodeBoundsMax);
		}
		bounds.m_min = extractVec3(bboxMin);
		bounds.m_max = extractVec3(bboxMax);
	}
	return bounds;
}

u32 split(std::vector<TempNode>& nodes, u32 begin, u32 end, const Box3& nodeBounds)
{
	u32 count = end - begin;
	u32 bestSplit = begin;

	if (count <= 1000000)
	{
		u32 bestAxis = 0;
		u32 globalBestSplit = begin;
		float globalBestCost = FLT_MAX;

		for (u32 axis = 0; axis < 3; ++axis)
		{
			std::sort(nodes.begin() + begin, nodes.begin() + end,
				[&](const TempNode& a, const TempNode& b)
			{
				return a.bboxCenter[axis] < b.bboxCenter[axis];
			});

			Box3 boundsLeft;
			boundsLeft.expandInit();

			Box3 boundsRight;
			boundsRight.expandInit();

			for (u32 indexLeft = 0; indexLeft < count; ++indexLeft)
			{
				u32 indexRight = count - indexLeft - 1;

				boundsLeft.expand(nodes[begin + indexLeft].bboxMin);
				boundsLeft.expand(nodes[begin + indexLeft].bboxMax);

				boundsRight.expand(nodes[begin + indexRight].bboxMin);
				boundsRight.expand(nodes[begin + indexRight].bboxMax);

				float surfaceAreaLeft = bboxSurfaceArea(boundsLeft);
				float surfaceAreaRight = bboxSurfaceArea(boundsRight);

				nodes[begin + indexLeft].surfaceAreaLeft = surfaceAreaLeft;
				nodes[begin + indexRight].surfaceAreaRight = surfaceAreaRight;
			}

			float bestCost = FLT_MAX;
			for (u32 mid = begin + 1; mid < end; ++mid)
			{
				float surfaceAreaLeft = nodes[mid - 1].surfaceAreaLeft;
				float surfaceAreaRight = nodes[mid].surfaceAreaRight;

				u32 countLeft = mid - begin;
				u32 countRight = end - mid;

				float costLeft = surfaceAreaLeft * (float)countLeft;
				float costRight = surfaceAreaRight * (float)countRight;

				float cost = costLeft + costRight;
				if (cost < bestCost)
				{
					bestSplit = mid;
					bestCost = cost;
				}
			}

			if (bestCost < globalBestCost)
			{
				globalBestSplit = bestSplit;
				globalBestCost = bestCost;
				bestAxis = axis;
			}
		}

		std::sort(nodes.begin() + begin, nodes.begin() + end,
			[&](const TempNode& a, const TempNode& b)
		{
			return a.bboxCenter[bestAxis] < b.bboxCenter[bestAxis];
		});

		return globalBestSplit;
	}
	else
	{
		Vec3 extents = nodeBounds.dimensions();
		int majorAxis = (int)std::distance(extents.begin(), std::max_element(extents.begin(), extents.end()));

		std::sort(nodes.begin() + begin, nodes.begin() + end,
			[&](const TempNode& a, const TempNode& b)
		{
			return a.bboxCenter[majorAxis] < b.bboxCenter[majorAxis];
		});

		float splitPos = (nodeBounds.m_min[majorAxis] + nodeBounds.m_max[majorAxis]) * 0.5f;
		for (u32 mid = begin + 1; mid < end; ++mid)
		{
			if (nodes[mid].bboxCenter[majorAxis] >= splitPos)
			{
				return mid;
			}
		}

		return end - 1;
	};
}

u32 buildInternal(std::vector<TempNode>& nodes, u32 begin, u32 end)
{
	u32 count = end - begin;

	if (count == 1)
	{
		return begin;
	}

	Box3 bounds = calculateBounds(nodes, begin, end);

	u32 mid = split(nodes, begin, end, bounds);

	u32 nodeId = (u32)nodes.size();
	nodes.push_back(TempNode());

	TempNode node;

	node.left = buildInternal(nodes, begin, mid);
	node.right = buildInternal(nodes, mid, end);

	float surfaceAreaLeft = bboxSurfaceArea(nodes[node.left].bboxMin, nodes[node.left].bboxMax);
	float surfaceAreaRight = bboxSurfaceArea(nodes[node.right].bboxMin, nodes[node.right].bboxMax);

	if (surfaceAreaRight > surfaceAreaLeft)
	{
		std::swap(node.left, node.right);
	}

	setBounds(node, bounds.m_min, bounds.m_max);
	node.bboxCenter = bounds.center();
	node.prim = BVHNode::InvalidMask;

	nodes[node.left].parent = nodeId;
	nodes[node.right].parent = nodeId;

	nodes[nodeId] = node;

	return nodeId;
}

void setDepthFirstVisitOrder(std::vector<TempNode>& nodes, u32 nodeId, u32 nextId, u32& order)
{
	TempNode& node = nodes[nodeId];

	node.visitOrder = order++;
	node.next = nextId;

	if (node.left != BVHNode::InvalidMask)
	{
		setDepthFirstVisitOrder(nodes, node.left, node.right, order);
	}

	if (node.right != BVHNode::InvalidMask)
	{
		setDepthFirstVisitOrder(nodes, node.right, nextId, order);
	}
}

void setDepthFirstVisitOrder(std::vector<TempNode>& nodes, u32 root)
{
	u32 order = 0;
	setDepthFirstVisitOrder(nodes, root, BVHNode::InvalidMask, order);
}

}

void BVHBuilder::build(const float* vertices, u32 stride, const u32* indices, u32 primCount)
{
	auto getVertex = [vertices, stride](u32 vertexId)
	{
		return Vec3(vertices + stride*vertexId);
	};

	m_nodes.clear();
	m_nodes.reserve(primCount * 2 - 1);

	std::vector<TempNode> tempNodes;
	tempNodes.reserve(primCount * 2 - 1);

	for (u32 primId = 0; primId < primCount; ++primId)
	{
		TempNode node;
		Box3 box;
		box.expandInit();

		Vec3 v0 = getVertex(indices[primId * 3 + 0]);
		Vec3 v1 = getVertex(indices[primId * 3 + 1]);
		Vec3 v2 = getVertex(indices[primId * 3 + 2]);

		box.expand(v0);
		box.expand(v1);
		box.expand(v2);

		node.primArea = Triangle::calculateArea(v0, v1, v2);

		if (node.primArea > 1e-4) // filter out degenerate prims
		{
			setBounds(node, box.m_min, box.m_max);
			node.bboxCenter = box.center();
			node.prim = primId;
			node.left = BVHNode::InvalidMask;
			node.right = BVHNode::InvalidMask;
			tempNodes.push_back(node);
		}
	}

	const u32 rootIndex = buildInternal(tempNodes, 0, (u32)tempNodes.size());

	setDepthFirstVisitOrder(tempNodes, rootIndex);

	m_nodes.resize(tempNodes.size());

	for (u32 oldIndex = 0; oldIndex < (u32)tempNodes.size(); ++oldIndex)
	{
		const TempNode& oldNode = tempNodes[oldIndex];

		BVHNode& newNode = m_nodes[oldNode.visitOrder];

		Vec3 bboxMin(oldNode.bboxMin);
		Vec3 bboxMax(oldNode.bboxMax);
		setBounds(newNode, bboxMin, bboxMax);

		newNode.prim = oldNode.prim;;
		newNode.next = oldNode.next == BVHNode::InvalidMask
			? BVHNode::InvalidMask
			: tempNodes[oldNode.next].visitOrder;
	}

	m_packedNodes.reserve(m_nodes.size() + primCount);

	for (u32 i = 0; i < (u32)tempNodes.size(); ++i)
	{
		const BVHNode& node = m_nodes[i];

		if (node.isLeaf())
		{
			struct BVHPrimitiveNode
			{
				Vec3 edge0;
				u32 prim;
				Vec3 edge1;
				u32 next;
			};

			BVHPrimitiveNode packedNode;

			Vec3 v0 = getVertex(indices[node.prim * 3 + 0]);
			Vec3 v1 = getVertex(indices[node.prim * 3 + 1]);
			Vec3 v2 = getVertex(indices[node.prim * 3 + 2]);

			packedNode.edge0 = v1 - v0;
			packedNode.prim = node.prim + (u32)tempNodes.size() * 2;

			packedNode.edge1 = v2 - v0;
			packedNode.next = node.next;

			BVHPackedNode data0, data1;
			memcpy(&data0, &packedNode.edge0, sizeof(BVHPackedNode));
			memcpy(&data1, &packedNode.edge1, sizeof(BVHPackedNode));

			m_packedNodes.push_back(data0);
			m_packedNodes.push_back(data1);
		}
		else
		{
			BVHNode packedNode;

			packedNode.bboxMin = node.bboxMin;
			packedNode.prim = node.prim;
			packedNode.bboxMax = node.bboxMax;
			packedNode.next = node.next;

			BVHPackedNode data0, data1;
			memcpy(&data0, &packedNode.bboxMin, sizeof(BVHPackedNode));
			memcpy(&data1, &packedNode.bboxMax, sizeof(BVHPackedNode));

			m_packedNodes.push_back(data0);
			m_packedNodes.push_back(data1);
		}
	}

	for (u32 primId = 0; primId < primCount; ++primId)
	{
		Vec3 v0 = getVertex(indices[primId * 3 + 0]);
		BVHPackedNode data;
		memcpy(&data, &v0, sizeof(BVHPackedNode));
		m_packedNodes.push_back(data);
	}
}
