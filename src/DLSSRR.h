#pragma once

#include <winrt/base.h>
#include <ngx/nvsdk_ngx.h>
#include <ngx/nvsdk_ngx_helpers.h>
#include <ngx/nvsdk_ngx_helpers_dlssd.h>

class DLSSRR
{
public:
	static DLSSRR* GetSingleton()
	{
		static DLSSRR singleton;
		return &singleton;
	}

	DLSSRR() = default;
	~DLSSRR();

	NVSDK_NGX_Parameter* ngxParameters = nullptr;
	NVSDK_NGX_Handle* featureHandle = nullptr;
	ID3D12Device* device = nullptr;

	bool initialized = false;
	bool isSupported = false;

	uint32_t renderWidth = 0;
	uint32_t renderHeight = 0;
	uint32_t targetWidth = 0;
	uint32_t targetHeight = 0;

	bool Initialize(ID3D12Device* a_device);
	bool CreateFeature(ID3D12GraphicsCommandList* a_cmdList, uint32_t a_inWidth, uint32_t a_inHeight, uint32_t a_outWidth, uint32_t a_outHeight,
		NVSDK_NGX_RayReconstruction_Hint_Render_Preset a_preset);
	bool Evaluate(ID3D12GraphicsCommandList* a_cmdList, const NVSDK_NGX_D3D12_DLSSD_Eval_Params& a_params);
	void ReleaseFeature();
	void Shutdown();
};
