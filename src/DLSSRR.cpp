#include "DLSSRR.h"

#include <filesystem>

DLSSRR::~DLSSRR()
{
	Shutdown();
}

bool DLSSRR::Initialize(ID3D12Device* a_device)
{
	if (initialized)
		return true;

	if (!a_device) {
		logger::error("[DLSS-RR] Cannot initialize NGX with null D3D12 device");
		return false;
	}

	device = a_device;

	std::wstring pluginsPath = std::filesystem::absolute(L"Data\\F4SE\\Plugins").wstring();
	std::wstring currentPath = std::filesystem::current_path().wstring();

	const wchar_t* pathList[] = { pluginsPath.c_str(), currentPath.c_str() };
	NVSDK_NGX_FeatureCommonInfo commonInfo{};
	commonInfo.PathListInfo.Path = pathList;
	commonInfo.PathListInfo.Length = _countof(pathList);

	NVSDK_NGX_Result res = NVSDK_NGX_D3D12_Init(
		0x1337,
		pluginsPath.c_str(),
		a_device,
		&commonInfo,
		NVSDK_NGX_Version_API
	);

	if (NVSDK_NGX_FAILED(res)) {
		logger::error("[DLSS-RR] NVSDK_NGX_D3D12_Init failed with code: 0x{:X}", (uint64_t)res);
		Shutdown();
		return false;
	}
	initialized = true;

	res = NVSDK_NGX_D3D12_GetCapabilityParameters(&ngxParameters);
	if (NVSDK_NGX_FAILED(res) || !ngxParameters) {
		logger::error("[DLSS-RR] GetCapabilityParameters failed with code: 0x{:X}", (uint64_t)res);
		Shutdown();
		return false;
	}

	int supported = 0;
	res = ngxParameters->Get("RayReconstruction.Available", &supported);
	if (NVSDK_NGX_SUCCEED(res)) {
		isSupported = supported != 0;
	} else {
		// Older NGX wrappers do not expose the RR capability key even though
		// the DLSSD feature library can create Ray Reconstruction. Let feature
		// creation perform the authoritative support check.
		logger::info("[DLSS-RR] RR capability key was not reported; deferring support check to feature creation");
		isSupported = true;
	}
	if (!isSupported) {
		logger::warn("[DLSS-RR] Ray Reconstruction is not supported by this GPU/driver");
		Shutdown();
		return false;
	}

	logger::info("[DLSS-RR] Initialized NGX successfully (Ray Reconstruction supported: {})", isSupported);
	initialized = true;
	return true;
}

bool DLSSRR::CreateFeature(ID3D12GraphicsCommandList* a_cmdList, uint32_t a_inWidth, uint32_t a_inHeight, uint32_t a_outWidth, uint32_t a_outHeight,
	NVSDK_NGX_RayReconstruction_Hint_Render_Preset a_preset)
{
	if (!initialized || !isSupported || !ngxParameters || !a_cmdList)
		return false;

	ReleaseFeature();

	renderWidth = a_inWidth;
	renderHeight = a_inHeight;
	targetWidth = a_outWidth;
	targetHeight = a_outHeight;

	NVSDK_NGX_DLSSD_Create_Params createParams{};
	createParams.InWidth = a_inWidth;
	createParams.InHeight = a_inHeight;
	createParams.InTargetWidth = a_outWidth;
	createParams.InTargetHeight = a_outHeight;
	createParams.InPerfQualityValue = NVSDK_NGX_PerfQuality_Value_Balanced;
	createParams.InFeatureCreateFlags = NVSDK_NGX_DLSS_Feature_Flags_IsHDR |
	                                    NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;
	createParams.InRoughnessMode = NVSDK_NGX_DLSS_Roughness_Mode_Packed;
	createParams.InUseHWDepth = NVSDK_NGX_DLSS_Depth_Type_HW;
	createParams.InDenoiseMode = NVSDK_NGX_DLSS_Denoise_Mode_DLUnified;

	// Apply the selected RR model preset to every performance mode.
	const uint32_t preset = static_cast<uint32_t>(a_preset);
	ngxParameters->Set(NVSDK_NGX_Parameter_RayReconstruction_Hint_Render_Preset_DLAA, preset);
	ngxParameters->Set(NVSDK_NGX_Parameter_RayReconstruction_Hint_Render_Preset_Quality, preset);
	ngxParameters->Set(NVSDK_NGX_Parameter_RayReconstruction_Hint_Render_Preset_Balanced, preset);
	ngxParameters->Set(NVSDK_NGX_Parameter_RayReconstruction_Hint_Render_Preset_Performance, preset);
	ngxParameters->Set(NVSDK_NGX_Parameter_RayReconstruction_Hint_Render_Preset_UltraPerformance, preset);
	ngxParameters->Set(NVSDK_NGX_Parameter_RayReconstruction_Hint_Render_Preset_UltraQuality, preset);

	NVSDK_NGX_Result res = NGX_D3D12_CREATE_DLSSD_EXT(
		a_cmdList,
		1,
		1,
		&featureHandle,
		ngxParameters,
		&createParams
	);

	if (NVSDK_NGX_FAILED(res)) {
		logger::error("[DLSS-RR] NGX_D3D12_CREATE_DLSSD_EXT failed with code: 0x{:X}", (uint64_t)res);
		featureHandle = nullptr;
		return false;
	}

	logger::info("[DLSS-RR] Created Ray Reconstruction feature ({}x{} -> {}x{})", a_inWidth, a_inHeight, a_outWidth, a_outHeight);
	return true;
}

bool DLSSRR::Evaluate(ID3D12GraphicsCommandList* a_cmdList, const NVSDK_NGX_D3D12_DLSSD_Eval_Params& a_params)
{
	if (!initialized || !isSupported || !featureHandle || !ngxParameters || !a_cmdList)
		return false;

	NVSDK_NGX_Result res = NGX_D3D12_EVALUATE_DLSSD_EXT(
		a_cmdList,
		featureHandle,
		ngxParameters,
		const_cast<NVSDK_NGX_D3D12_DLSSD_Eval_Params*>(&a_params)
	);

	if (NVSDK_NGX_FAILED(res)) {
		logger::warn("[DLSS-RR] NGX_D3D12_EVALUATE_DLSSD_EXT failed with code: 0x{:X}", (uint64_t)res);
		return false;
	}

	return true;
}

void DLSSRR::ReleaseFeature()
{
	if (featureHandle) {
		NVSDK_NGX_D3D12_ReleaseFeature(featureHandle);
		featureHandle = nullptr;
	}
}

void DLSSRR::Shutdown()
{
	ReleaseFeature();

	if (ngxParameters) {
		NVSDK_NGX_D3D12_DestroyParameters(ngxParameters);
		ngxParameters = nullptr;
	}

	if (initialized && device) {
		NVSDK_NGX_D3D12_Shutdown1(device);
		initialized = false;
	}

	isSupported = false;
	device = nullptr;
}
