#include "BaseApplication.h"

#include <Rush/GfxBitmapFont.h>
#include <Rush/GfxPrimitiveBatch.h>
#include <Rush/Window.h>

BaseApplication::BaseApplication()
: m_dev(Platform_GetGfxDevice()), m_ctx(Platform_GetGfxContext()), m_window(Platform_GetWindow())
{
	m_window->retain();
	Gfx_Retain(m_dev);
	Gfx_Retain(m_ctx);

	m_prim = new PrimitiveBatch();
	m_font = new BitmapFontRenderer(BitmapFontRenderer::createEmbeddedFont(true, 0, 1));

	// Depth stencil states

	{
		GfxDepthStencilDesc desc;
		desc.enable      = false;
		desc.writeEnable = false;
		desc.compareFunc = GfxCompareFunc::Always;
		m_depthStencilStates.disable = Gfx_CreateDepthStencilState(desc);
	}

	{
		GfxDepthStencilDesc desc;
		desc.enable      = true;
		desc.writeEnable = true;
		desc.compareFunc = GfxCompareFunc::LessEqual;
		m_depthStencilStates.writeLessEqual = Gfx_CreateDepthStencilState(desc);
	}

	{
		GfxDepthStencilDesc desc;
		desc.enable      = true;
		desc.writeEnable = true;
		desc.compareFunc = GfxCompareFunc::Always;
		m_depthStencilStates.writeAlways = Gfx_CreateDepthStencilState(desc);
	}

	{
		GfxDepthStencilDesc desc;
		desc.enable      = true;
		desc.writeEnable = false;
		desc.compareFunc = GfxCompareFunc::LessEqual;
		m_depthStencilStates.testLessEqual = Gfx_CreateDepthStencilState(desc);
	}

	// Blend states

	{
		GfxBlendStateDesc desc = GfxBlendStateDesc::makeOpaque();
		m_blendStates.opaque = Gfx_CreateBlendState(desc);
	}

	{
		GfxBlendStateDesc desc = GfxBlendStateDesc::makeLerp();
		m_blendStates.lerp = Gfx_CreateBlendState(desc);
	}

	{
		GfxBlendStateDesc desc = GfxBlendStateDesc::makeAdditive();
		m_blendStates.additive = Gfx_CreateBlendState(desc);
	}

	// Sampler states

	{
		GfxSamplerDesc desc = GfxSamplerDesc::makePoint();
		desc.wrapU          = GfxTextureWrap::Clamp;
		desc.wrapV          = GfxTextureWrap::Clamp;
		desc.wrapW          = GfxTextureWrap::Clamp;
		m_samplerStates.pointClamp = Gfx_CreateSamplerState(desc);
	}

	{
		GfxSamplerDesc desc = GfxSamplerDesc::makeLinear();
		desc.wrapU          = GfxTextureWrap::Clamp;
		desc.wrapV          = GfxTextureWrap::Clamp;
		desc.wrapW          = GfxTextureWrap::Clamp;
		m_samplerStates.linearClamp = Gfx_CreateSamplerState(desc);
	}

	{
		GfxSamplerDesc desc = GfxSamplerDesc::makeLinear();
		desc.wrapU          = GfxTextureWrap::Wrap;
		desc.wrapV          = GfxTextureWrap::Wrap;
		desc.wrapW          = GfxTextureWrap::Wrap;
		m_samplerStates.linearWrap = Gfx_CreateSamplerState(desc);
	}

	{
		GfxSamplerDesc desc = GfxSamplerDesc::makeLinear();
		desc.wrapU          = GfxTextureWrap::Wrap;
		desc.wrapV          = GfxTextureWrap::Wrap;
		desc.wrapW          = GfxTextureWrap::Wrap;
		desc.anisotropy     = 4.0f;
		m_samplerStates.anisotropicWrap = Gfx_CreateSamplerState(desc);
	}
}

BaseApplication::~BaseApplication()
{
	delete m_font;
	delete m_prim;

	Gfx_Release(m_ctx);
	Gfx_Release(m_dev);
	m_window->release();
}
