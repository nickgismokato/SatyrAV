#include "render/D3D11Interop.hpp"
#include "core/Logger.hpp"
#include <SDL3/SDL.h>

extern "C"{
#include <libavutil/hwcontext.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
}

// Windows headers last, so their macros cannot rewrite anything above.
#ifdef _WIN32
#include <d3d11.h>
#include <d3d10.h>
extern "C"{
#include <libavutil/hwcontext_d3d11va.h>
}
#endif

namespace SatyrAV{
namespace D3D11Interop{

AVBufferRef* CreateSharedDeviceCtx(SDL_Renderer* renderer){
#ifdef _WIN32
	if(!renderer) return nullptr;

	auto* device = (ID3D11Device*)SDL_GetPointerProperty(
		SDL_GetRendererProperties(renderer),
		SDL_PROP_RENDERER_D3D11_DEVICE_POINTER, nullptr);
	if(!device){
		Logger::Instance().Infof(
			"D3D11Interop: slave renderer is '%s', not direct3d11 — decoder will use its own device",
			SDL_GetRendererName(renderer));
		return nullptr;
	}

	// The decoder works on the decode thread while SDL draws on the main
	// thread, both through this one device. That is only defined behaviour
	// once the device is multithread-protected.
	//
	// SDL creates its device single-threaded unless
	// SDL_HINT_RENDER_DIRECT3D_THREADSAFE was set before the renderer was
	// created (Renderer.cpp does this). A single-threaded device does not
	// expose this interface at all, so a failure here means the hint did not
	// take effect — and sharing must not go ahead.
	ID3D10Multithread* mt = nullptr;
	if(FAILED(device->QueryInterface(__uuidof(ID3D10Multithread), (void**)&mt)) || !mt){
		Logger::Instance().Warnf(
			"D3D11Interop: device is single-threaded (no multithread interface) — "
			"decoder will use its own device");
		return nullptr;
	}
	mt->SetMultithreadProtected(TRUE);
	const bool isProtected = mt->GetMultithreadProtected() != FALSE;
	mt->Release();
	if(!isProtected){
		Logger::Instance().Warnf(
			"D3D11Interop: device refused multithread protection — decoder will use its own device");
		return nullptr;
	}

	AVBufferRef* hw = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
	if(!hw) return nullptr;

	auto* deviceCtx = (AVHWDeviceContext*)hw->data;
	auto* d3d = (AVD3D11VADeviceContext*)deviceCtx->hwctx;
	d3d->device = device;
	device->AddRef(); // FFmpeg releases it when the context is freed

	if(av_hwdevice_ctx_init(hw) < 0){
		Logger::Instance().Errorf("D3D11Interop: av_hwdevice_ctx_init failed on the shared device");
		av_buffer_unref(&hw);
		return nullptr;
	}

	Logger::Instance().Infof("D3D11Interop: hardware decoder is sharing the slave renderer's D3D11 device");
	return hw;
#else
	(void)renderer;
	return nullptr;
#endif
}

bool CopyFrameToTexture(SDL_Renderer* renderer, AVBufferRef* deviceCtx,
	const AVFrame* frame, SDL_Texture** texture){
#ifdef _WIN32
	if(!renderer || !deviceCtx || !frame || !texture) return false;
	if(frame->format != AV_PIX_FMT_D3D11) return false;

	// For AV_PIX_FMT_D3D11, data[0] is the decoder's texture array and
	// data[1] is the index of this frame's slice within it.
	auto* src = (ID3D11Texture2D*)frame->data[0];
	const UINT slice = (UINT)(uintptr_t)frame->data[1];
	if(!src) return false;

	D3D11_TEXTURE2D_DESC srcDesc;
	src->GetDesc(&srcDesc);
	// 8-bit only. 10-bit sources decode to P010 and are kept on the
	// read-back path, which is decided before decoding starts — so reaching
	// here with anything else means the driver did something unexpected.
	if(srcDesc.Format != DXGI_FORMAT_NV12) return false;

	bool create = (*texture == nullptr);
	if(*texture){
		float tw = 0.0f, th = 0.0f;
		SDL_GetTextureSize(*texture, &tw, &th);
		if((UINT)tw != srcDesc.Width || (UINT)th != srcDesc.Height) create = true;
	}
	if(create){
		if(*texture){ SDL_DestroyTexture(*texture); *texture = nullptr; }
		SDL_PropertiesID props = SDL_CreateProperties();
		SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER, SDL_PIXELFORMAT_NV12);
		SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER, SDL_TEXTUREACCESS_STATIC);
		SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER, srcDesc.Width);
		SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER, srcDesc.Height);
		*texture = SDL_CreateTextureWithProperties(renderer, props);
		SDL_DestroyProperties(props);
		if(!*texture){
			Logger::Instance().Errorf("D3D11Interop: NV12 texture %ux%u creation failed: %s",
				srcDesc.Width, srcDesc.Height, SDL_GetError());
			return false;
		}
		// Same blend mode the read-back path gives its video texture, so the
		// fade and transparency modifiers behave identically on both.
		SDL_SetTextureBlendMode(*texture, SDL_BLENDMODE_BLEND);
	}

	auto* dst = (ID3D11Texture2D*)SDL_GetPointerProperty(
		SDL_GetTextureProperties(*texture), SDL_PROP_TEXTURE_D3D11_TEXTURE_POINTER, nullptr);
	if(!dst) return false;

	// SDL batches draw calls. Anything already queued that samples this
	// texture must reach the GPU before its contents change underneath it.
	SDL_FlushRenderer(renderer);

	// FFmpeg's own lock, the one the decoder takes around its use of the
	// device context on the decode thread.
	auto* d3d = (AVD3D11VADeviceContext*)((AVHWDeviceContext*)deviceCtx->data)->hwctx;
	d3d->lock(d3d->lock_ctx);
	d3d->device_context->CopySubresourceRegion(dst, 0, 0, 0, 0, src, slice, nullptr);
	d3d->unlock(d3d->lock_ctx);
	return true;
#else
	(void)renderer; (void)deviceCtx; (void)frame; (void)texture;
	return false;
#endif
}

} // namespace D3D11Interop
} // namespace SatyrAV
