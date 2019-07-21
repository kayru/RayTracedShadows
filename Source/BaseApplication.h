#pragma once

#include <Rush/GfxDevice.h>
#include <Rush/Platform.h>

class ShaderCompiler;

namespace Rush
{
class PrimitiveBatch;
class BitmapFontRenderer;
}

class BaseApplication : public Application
{
	RUSH_DISALLOW_COPY_AND_ASSIGN(BaseApplication);

public:
	BaseApplication();
	~BaseApplication();

protected:
	struct DepthStencilStates
	{
		GfxOwn<GfxDepthStencilState> testLessEqual;
		GfxOwn<GfxDepthStencilState> writeLessEqual;
		GfxOwn<GfxDepthStencilState> writeAlways;
		GfxOwn<GfxDepthStencilState> disable;
	} m_depthStencilStates;

	struct SamplerStates
	{
		GfxOwn<GfxSampler> pointClamp;
		GfxOwn<GfxSampler> linearClamp;
		GfxOwn<GfxSampler> linearWrap;
		GfxOwn<GfxSampler> anisotropicWrap;
	} m_samplerStates;

	struct BlendStates
	{
		GfxOwn<GfxBlendState> lerp;
		GfxOwn<GfxBlendState> opaque;
		GfxOwn<GfxBlendState> additive;
	} m_blendStates;

	GfxDevice*          m_dev;
	GfxContext*         m_ctx;
	Window*             m_window;
	PrimitiveBatch*     m_prim;
	BitmapFontRenderer* m_font;
};
