#pragma once

#include <Rush/GfxDevice.h>
#include <Rush/GfxRef.h>
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
		GfxDepthStencilStateRef testLessEqual;
		GfxDepthStencilStateRef writeLessEqual;
		GfxDepthStencilStateRef writeAlways;
		GfxDepthStencilStateRef disable;
	} m_depthStencilStates;

	struct SamplerStates
	{
		GfxSamplerRef pointClamp;
		GfxSamplerRef linearClamp;
		GfxSamplerRef linearWrap;
		GfxSamplerRef anisotropicWrap;
	} m_samplerStates;

	struct BlendStates
	{
		GfxBlendStateRef lerp;
		GfxBlendStateRef opaque;
		GfxBlendStateRef additive;
	} m_blendStates;

	GfxDevice*          m_dev;
	GfxContext*         m_ctx;
	Window*             m_window;
	PrimitiveBatch*     m_prim;
	BitmapFontRenderer* m_font;
};
