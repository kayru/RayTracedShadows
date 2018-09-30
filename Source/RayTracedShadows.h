#pragma once

#include <Rush/GfxBitmapFont.h>
#include <Rush/GfxDevice.h>
#include <Rush/GfxPrimitiveBatch.h>
#include <Rush/GfxRef.h>
#include <Rush/MathTypes.h>
#include <Rush/Platform.h>
#include <Rush/UtilCamera.h>
#include <Rush/UtilCameraManipulator.h>
#include <Rush/UtilTimer.h>
#include <Rush/Window.h>

#include <stdio.h>
#include <memory>
#include <string>
#include <unordered_map>

#include "BaseApplication.h"
#include "BVHBuilder.h"
#include "MovingAverage.h"

class NVXRaytracing;

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

	void renderShadowMask();
	void renderShadowMaskNVX();

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

	GfxTechnique m_techniqueModel;
	GfxTechnique m_techniqueRayTracedShadows;
	GfxTechnique m_techniqueCombine;

	GfxTexture m_defaultWhiteTexture;

	GfxBuffer m_vertexBuffer;
	GfxBuffer m_indexBuffer;

	GfxBufferRef m_modelGlobalConstantBuffer;

	GfxBufferRef m_rayTracingConstantBuffer;

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

	std::unordered_map<std::string, GfxTextureRef> m_textures;
	std::unordered_map<u64, GfxRef<GfxBuffer>> m_materialConstantBuffers;

	GfxTextureRef m_shadowMask;
	GfxTextureRef m_gbufferDepth;
	GfxTextureRef m_gbufferNormal;
	GfxTextureRef m_gbufferPosition;
	GfxTextureRef m_gbufferBaseColor;

	struct MaterialConstants
	{
		Vec4 baseColor;
	};

	struct Material
	{
		GfxTextureRef albedoTexture;
		GfxBufferRef constantBuffer;
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

	GfxBufferRef m_bvhBuffer;

	Vec2 m_prevMousePos = Vec2(0.0f);

	NVXRaytracing* m_nvxRaytracing = nullptr;

};
