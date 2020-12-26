#pragma once

#include <Rush/GfxBitmapFont.h>
#include <Rush/GfxDevice.h>
#include <Rush/GfxPrimitiveBatch.h>
#include <Rush/MathTypes.h>
#include <Rush/Platform.h>
#include <Rush/UtilCamera.h>
#include <Rush/UtilTimer.h>
#include <Rush/Window.h>

#include <stdio.h>
#include <memory>
#include <string>
#include <unordered_map>

#include "BaseApplication.h"
#include "BVHBuilder.h"
#include "MovingAverage.h"

class NVRaytracing;

enum class ShadowRenderMode
{
	Compute,
	Hardware,
};

class RayTracedShadowsApp : public BaseApplication
{
public:

	RayTracedShadowsApp();
	~RayTracedShadowsApp();

	void update() override;

private:

	void createRenderTargets(Tuple2i size);

	enum Timestamp
	{
		Timestamp_Gbuffer,
		Timestamp_Shadows,
		Timestamp_Lighting,
	};

	void render();

	void renderGbuffer();

	struct RayTracingConstants
	{
		Vec4 cameraPosition;
		Vec4 cameraDirection;
		Vec4 lightDirection; // direction in XYZ, bias in W
		Vec4 renderTargetSize;
	};

	void renderShadowMaskCompute();
	void renderShadowMaskHardware();

	bool loadModel(const char* filename);
	GfxRef<GfxTexture> loadTexture(const std::string& filename);

	Timer m_timer;

	struct Stats
	{
		MovingAverage<double, 60> gpuGbuffer;
		MovingAverage<double, 60> gpuShadows;
		MovingAverage<double, 60> gpuTotal;
		MovingAverage<double, 60> cpuTotal;
		MovingAverage<double, 60> cpuUI;
		MovingAverage<double, 60> cpuModel;
	} m_stats;

	Camera m_camera;
	Camera m_interpolatedCamera;
	Camera m_lightCamera;

	CameraManipulator m_cameraMan;

	GfxOwn<GfxTechnique> m_techniqueModel;
	GfxOwn<GfxTechnique> m_techniqueRayTracedShadows;
	GfxOwn<GfxTechnique> m_techniqueCombine;

	GfxOwn<GfxTexture> m_defaultWhiteTexture;

	GfxOwn<GfxBuffer> m_vertexBuffer;
	GfxOwn<GfxBuffer> m_indexBuffer;

	GfxOwn<GfxBuffer> m_modelGlobalConstantBuffer;

	GfxOwn<GfxBuffer> m_rayTracingConstantBuffer;

	Mat4 m_matViewProj = Mat4::identity();
	Mat4 m_matViewProjInv = Mat4::identity();

	u32 m_indexCount = 0;
	u32 m_vertexCount = 0;

	struct ModelConstants
	{
		Mat4 matViewProj = Mat4::identity();
		Mat4 matWorld = Mat4::identity();
		Vec4 cameraPosition = Vec4(0.0f);
	};

	Mat4 m_worldTransform = Mat4::identity();

	Box3 m_boundingBox;

	struct Vertex
	{
		Vec3 position;
		Vec3 normal;
		Vec2 texcoord;
	};

	std::string m_statusString;
	bool m_valid = false;

	std::unordered_map<std::string, GfxRef<GfxTexture>> m_textures;
	std::unordered_map<u64, GfxRef<GfxBuffer>> m_materialConstantBuffers;

	GfxOwn<GfxTexture> m_shadowMask;
	GfxOwn<GfxTexture> m_gbufferDepth;
	GfxOwn<GfxTexture> m_gbufferNormal;
	GfxOwn<GfxTexture> m_gbufferPosition;
	GfxOwn<GfxTexture> m_gbufferBaseColor;

	struct MaterialConstants
	{
		Vec4 baseColor;
	};

	struct Material
	{
		GfxRef<GfxTexture> albedoTexture;
		GfxRef<GfxBuffer> constantBuffer;
	};

	std::vector<Material> m_materials;
	Material m_defaultMaterial;

	struct MeshSegment
	{
		u32 material = 0;
		u32 indexOffset = 0;
		u32 indexCount = 0;
	};

	std::vector<MeshSegment> m_segments;

	WindowEventListener m_windowEvents;

	float m_cameraScale = 1.0f;

	GfxOwn<GfxBuffer> m_bvhBuffer;

	Vec2 m_prevMousePos = Vec2(0.0f);

#if USE_NV_RAYTRACING
	NVRaytracing* m_nvRaytracing = nullptr;
	bool m_nvRaytracingDirty = false;
#endif // USE_NV_RAYTRACING

	ShadowRenderMode m_mode = ShadowRenderMode::Compute;

};
