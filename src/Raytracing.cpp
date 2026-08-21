#include "Raytracing.h"

#include <d3dcompiler.h>

#include "DX12SwapChain.h"
#include "DirectXMath.h"

#include "ShaderUtils.h"

// Microsoft Pix
#include <filesystem>
#include <shlobj.h>
#include <KnownFolders.h>

enum class RenderTarget
{
	kFrameBuffer = 0,

	kRefractionNormal = 1,
	
	kMainPreAlpha = 2,
	kMain = 3,
	kMainTemp = 4,

	kSSRRaw = 7,
	kSSRBlurred = 8,
	kSSRBlurredExtra = 9,

	kMainVerticalBlur = 14,
	kMainHorizontalBlur = 15,

	kSSRDirection = 10,
	kSSRMask = 11,

	kUI = 17,
	kUITemp = 18,

	kGbufferNormal = 20,
	kGbufferNormalSwap = 21,
	kGbufferAlbedo = 22,
	kGbufferEmissive = 23,
	kGbufferMaterial = 24, //  Glossiness, Specular, Backlighting, SSS

	kSSAO = 28,

	kTAAAccumulation = 26,
	kTAAAccumulationSwap = 27,

	kMotionVectors = 29,

	kUIDownscaled = 36,
	kUIDownscaledComposite = 37,

	kMainDepthMips = 39,

	kUnkMask = 57,

	kSSAOTemp = 48,
	kSSAOTemp2 = 49,
	kSSAOTemp3 = 50,

	kDiffuseBuffer = 58,
	kSpecularBuffer = 59,

	kDownscaledHDR = 64,
	kDownscaledHDRLuminance2 = 65,
	kDownscaledHDRLuminance3 = 66,
	kDownscaledHDRLuminance4 = 67,
	kDownscaledHDRLuminance5Adaptation = 68,
	kDownscaledHDRLuminance6AdaptationSwap = 69,
	kDownscaledHDRLuminance6 = 70,

	kCount = 101
};

enum class DepthStencilTarget
{
	kMainOtherOther = 0,
	kMainOther = 1,
	kMain = 2,
	kMainCopy = 3,
	kMainCopyCopy = 4,

	kShadowMap = 8,

	kCount = 13
};

void Raytracing::LoadSettings()
{
	logger::info("[Raytracing] Loading settings");

	CSimpleIniA ini;
	ini.SetUnicode();
	ini.LoadFile("Data\\F4SE\\Plugins\\FrameGeneration.ini");

	settings.frameGenerationMode = ini.GetBoolValue("Settings", "bFrameGenerationMode", true);
	settings.frameLimitMode = ini.GetBoolValue("Settings", "bFrameLimitMode", true);
	settings.captureHotkey = static_cast<uint32_t>(ini.GetLongValue("Settings", "uCaptureHotkey", VK_F11));

	logger::info("[Raytracing] bFrameGenerationMode: {}", settings.frameGenerationMode);
	logger::info("[Raytracing] bFrameLimitMode: {}", settings.frameLimitMode);
	logger::info("[Raytracing] uCaptureHotkey: 0x{:X}", settings.captureHotkey);
}

void Raytracing::InitializePIX()
{
	auto getLatestWinPixGpuCapturerPath = [] {
		LPWSTR programFilesPath = nullptr;
		SHGetKnownFolderPath(FOLDERID_ProgramFiles, KF_FLAG_DEFAULT, NULL, &programFilesPath);

		std::filesystem::path pixInstallationPath = programFilesPath;
		pixInstallationPath /= "Microsoft PIX";

		std::wstring newestVersionFound;

		for (auto const& directory_entry : std::filesystem::directory_iterator(pixInstallationPath)) {
			if (directory_entry.is_directory()) {
				if (newestVersionFound.empty() || newestVersionFound < directory_entry.path().filename().c_str()) {
					newestVersionFound = directory_entry.path().filename().c_str();
				}
			}
		}

		if (newestVersionFound.empty()) {
			logger::warn("[DX12Interop] PIX installation not found");
		}

		return std::wstring{ pixInstallationPath / newestVersionFound / L"WinPixGpuCapturer.dll" };
	};

	try {
		auto module = GetModuleHandleW(L"WinPixGpuCapturer.dll");

		// Check to see if a copy of WinPixGpuCapturer.dll has already been injected into the application.
		// This may happen if the application is launched through the PIX UI.
		if (module == 0) {
			auto pixGPUCapturerPath = getLatestWinPixGpuCapturerPath();

			if (pixGPUCapturerPath.empty()) {
				logger::warn("[DX12Interop] PIX capture is enabled but binaries where not found.");
				return;
			}
			else {
				module = LoadLibraryW(pixGPUCapturerPath.c_str());

				if (module == 0) {
					logger::warn("[DX12Interop] Failed to load PIX from path.");
					return;
				}
			}
		}

		DXGIGetDebugInterface1(0, IID_PPV_ARGS(&ga));

		logger::info("[Raytracing] PIX Initialized");
	}
	catch (const std::exception& e) {
		logger::error("[DX12Interop] Failed to load PIX with exception: {}", e.what());
	}
	catch (...) {
		logger::error("[DX12Interop] Failed to load PIX with unknown exception.");
	}
}

void Raytracing::CreateD3D12Device(IDXGIAdapter* a_adapter, ID3D11Device* a_d3d11Device, ID3D11DeviceContext* a_d3d11Context)
{
	const bool enableDebug = false;
	if (enableDebug) {
		winrt::com_ptr<ID3D12Debug3> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
			debugController->EnableDebugLayer();
			debugController->SetEnableGPUBasedValidation(TRUE);
		}
		else {
			logger::critical("[Raytracing] Debug layer creation failed");
		}

		winrt::com_ptr<ID3D12DeviceRemovedExtendedDataSettings1> pDredSettings;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pDredSettings)))) {
			pDredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
			pDredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
		}
	}
	else {
		InitializePIX();
	}

	DX::ThrowIfFailed(D3D12CreateDevice(a_adapter, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&d3d12Device)));

	if (enableDebug) {
		winrt::com_ptr<ID3D12InfoQueue> infoQueue;
		if (SUCCEEDED(d3d12Device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, FALSE);
		}
		else {
			logger::critical("[Raytracing] Debug break creation failed");
		}
	}

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	queueDesc.NodeMask = 0;
	DX::ThrowIfFailed(d3d12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)));

	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	DX::ThrowIfFailed(d3d12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&computeCommandQueue)));

	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
	DX::ThrowIfFailed(d3d12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&copyCommandQueue)));

	DX::ThrowIfFailed(a_d3d11Device->QueryInterface(IID_PPV_ARGS(&d3d11Device)));
	DX::ThrowIfFailed(a_d3d11Context->QueryInterface(IID_PPV_ARGS(&d3d11Context)));

	// Create Interop
	{
		HANDLE sharedFenceHandle;
		DX::ThrowIfFailed(d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&d3d12Fence)));
		DX::ThrowIfFailed(d3d12Device->CreateSharedHandle(d3d12Fence.get(), nullptr, GENERIC_ALL, nullptr, &sharedFenceHandle));
		DX::ThrowIfFailed(d3d11Device->OpenSharedFence(sharedFenceHandle, IID_PPV_ARGS(&d3d11Fence)));
		CloseHandle(sharedFenceHandle);

		fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (!fenceEvent)
			DX::ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}

	InitializeCERaytracing(d3d11Device.get(), d3d12Device.get(), commandQueue.get(), computeCommandQueue.get(), copyCommandQueue.get());
}

void Raytracing::InitializeCERaytracing(ID3D11Device5* a_d3d11Device, ID3D12Device5* a_d3d12Device, ID3D12CommandQueue* a_commandQueue, ID3D12CommandQueue* a_computeCommandQueue, ID3D12CommandQueue* a_copyCommandQueue)
{
	if (forcedDisabled)
		return;

	if (initialized)
		return;

	logger::error("[Raytracing] Initializing Creation Engine Ray Tracing...");

	bool result = creationEngineRaytracing->InitializeRenderer(a_d3d11Device, a_d3d12Device, a_commandQueue, a_computeCommandQueue, a_copyCommandQueue);

	if (!result) {
		//settings.CreationEngineRaytracingSettings.Enabled = false;
		initialized = false;
		forcedDisabled = true;
		disableReason = DisableReason::InitFailed;

		logger::error("[Raytracing] Failed to initialize Creation Engine Ray Tracing.");
		return;
	}

	initialized = true;

	logger::info("[Raytracing] Successfully initialized Creation Engine ray tracing.");

	// Set Resolution
	auto state = RE::BSGraphics::State::GetSingleton();
	creationEngineRaytracing->SetResolution(state.screenWidth, state.screenHeight);
}

void Raytracing::PostPostLoad()
{
	creationEngineRaytracing = std::make_unique<CreationEngineRaytracing>();

	highFPSPhysicsFixLoaded = GetModuleHandleA("Data\\F4SE\\Plugins\\HighFPSPhysicsFix.dll") != nullptr;

	if (highFPSPhysicsFixLoaded)
		logger::info("[Raytracing] HighFPSPhysicsFix.dll is loaded");
	else
		logger::info("[Raytracing] HighFPSPhysicsFix.dll is not loaded");

	Hooks::Install();
}

void Raytracing::ShareTexture(ID3D11Texture2D* d3d11Texture, ID3D12Resource** d3d12Resource, bool nt, uint accessFlags) const
{
	D3D11_TEXTURE2D_DESC desc;
	d3d11Texture->GetDesc(&desc);

	IDXGIResource1* dxgiResource;
	DX::ThrowIfFailed(d3d11Texture->QueryInterface(IID_PPV_ARGS(&dxgiResource)));

	HANDLE sharedHandle = nullptr;

	if (nt)
		DX::ThrowIfFailed(dxgiResource->CreateSharedHandle(nullptr, accessFlags, nullptr, &sharedHandle));
	else
		DX::ThrowIfFailed(dxgiResource->GetSharedHandle(&sharedHandle));

	DX::ThrowIfFailed(d3d12Device->OpenSharedHandle(sharedHandle, IID_PPV_ARGS(d3d12Resource)));

	// Only close handle if it was created here
	if (nt)
		CloseHandle(sharedHandle);
}

void Raytracing::SetupResources()
{
	auto rendererData = RE::BSGraphics::RendererData::GetSingleton();

	auto& main = rendererData->renderTargets[(uint)RenderTarget::kMain];
	D3D11_TEXTURE2D_DESC mainDesc;
	reinterpret_cast<ID3D11Texture2D*>(main.texture)->GetDesc(&mainDesc);

	// Create Samplers
	{
		D3D11_SAMPLER_DESC samplerDesc = {
			.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
			.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
			.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
			.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
			.MaxAnisotropy = 1,
			.MinLOD = 0,
			.MaxLOD = D3D11_FLOAT32_MAX
		};
		DX::ThrowIfFailed(d3d11Device->CreateSamplerState(&samplerDesc, samplerState.put()));
	}

	// Sky Hemisphere
	{
		auto& reflections = rendererData->cubeMapRenderTargets[0];

		D3D11_TEXTURE2D_DESC desc;
		reinterpret_cast<ID3D11Texture2D*>(reflections.texture)->GetDesc(&desc);
		skyHemiSize = desc.Width * 2u;

		D3D11_TEXTURE2D_DESC texDesc{};
		texDesc.Width = skyHemiSize;
		texDesc.Height = skyHemiSize;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

		skyHemisphere = std::make_unique<WrappedResource>(texDesc, d3d11Device.get(), d3d12Device.get());
		DX::ThrowIfFailed(skyHemisphere->resource->SetName(L"Sky Hemisphere"));

		creationEngineRaytracing->SetSkyHemisphere(skyHemisphere->resource.get());
	}

	// Water FlowMap
	{
		D3D11_TEXTURE2D_DESC texDesc{};
		texDesc.Width = 320;
		texDesc.Height = 320;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

		waterFlowMap = std::make_unique<WrappedResource>(texDesc, d3d11Device.get(), d3d12Device.get());
		DX::ThrowIfFailed(waterFlowMap->resource->SetName(L"Water FlowMap"));

		creationEngineRaytracing->SetWaterFlowMap(waterFlowMap->resource.get());
	}

	// Gbuffer Textures
	{
		auto& albedo = rendererData->renderTargets[(uint)RenderTarget::kGbufferAlbedo];
		ShareTexture(reinterpret_cast<ID3D11Texture2D*>(albedo.texture), albedoTexture.put());

		auto& material = rendererData->renderTargets[(uint)RenderTarget::kGbufferMaterial];
		ShareTexture(reinterpret_cast<ID3D11Texture2D*>(material.texture), gnmaoTexture.put());
	}

	{
		D3D11_TEXTURE2D_DESC texDesc{};
		texDesc.Width = mainDesc.Width;
		texDesc.Height = mainDesc.Height;
		texDesc.MipLevels = 1;
		texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		texDesc.ArraySize = 1;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

		normalRoughnessTexture = std::make_unique<WrappedResource>(texDesc, d3d11Device.get(), d3d12Device.get());
	}

	auto& certSettings = settings.cert;

	certSettings.Enabled = true;
	certSettings.GeneralSettings.Mode = CreationEngineRaytracing::Mode::PathTracing;
	certSettings.GeneralSettings.Denoiser = CreationEngineRaytracing::Denoiser::NRD_Reblur;
	certSettings.DebugSettings.Timings = CreationEngineRaytracing::TimingMode::Extended;
	certSettings.DebugSettings.Markers = true;

	creationEngineRaytracing->Initialize(certSettings);

	creationEngineRaytracing->SetSharedTextures(albedoTexture.get(), normalRoughnessTexture->resource.get(), gnmaoTexture.get());

	CompileShaders();
}

void Raytracing::CompileShaders()
{
	const auto skyHemiSizeStr = std::to_string(skyHemiSize);
	if (auto rawPtr = reinterpret_cast<ID3D11ComputeShader*>(ShaderUtils::CompileShader(L"Data\\Shaders\\CubeToHemiCS.hlsl", { { "RESOLUTION", skyHemiSizeStr.c_str() } }, "cs_5_0")); rawPtr)
		cubeToHemiCS.attach(rawPtr);
}

void Raytracing::CreateFrameGenerationResources()
{
	logger::info("[Frame Generation] Creating resources");
	
	setupBuffers = true;

	auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
	auto& main = rendererData->renderTargets[(uint)RenderTarget::kMain];

	for (int index = 0; index < 2; index++) {
		D3D11_TEXTURE2D_DESC texDesc{};
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

		reinterpret_cast<ID3D11Texture2D*>(main.texture)->GetDesc(&texDesc);
		reinterpret_cast<ID3D11ShaderResourceView*>(main.srView)->GetDesc(&srvDesc);
		reinterpret_cast<ID3D11RenderTargetView*>(main.rtView)->GetDesc(&rtvDesc);

		texDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;

		uavDesc.Format = texDesc.Format;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;

		texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;

		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.Format = texDesc.Format;
		rtvDesc.Format = texDesc.Format;
		uavDesc.Format = texDesc.Format;

		HUDLessBufferShared[index] = new Texture2D(texDesc);
		HUDLessBufferShared[index]->CreateSRV(srvDesc);
		HUDLessBufferShared[index]->CreateRTV(rtvDesc);
		HUDLessBufferShared[index]->CreateUAV(uavDesc);

		texDesc.Format = DXGI_FORMAT_R32_FLOAT;
		srvDesc.Format = texDesc.Format;
		rtvDesc.Format = texDesc.Format;
		uavDesc.Format = texDesc.Format;

		depthBufferShared[index] = new Texture2D(texDesc);
		depthBufferShared[index]->CreateSRV(srvDesc);
		depthBufferShared[index]->CreateRTV(rtvDesc);
		depthBufferShared[index]->CreateUAV(uavDesc);

		auto& motionVector = rendererData->renderTargets[(uint)RenderTarget::kMotionVectors];
		D3D11_TEXTURE2D_DESC texDescMotionVector{};
		reinterpret_cast<ID3D11Texture2D*>(motionVector.texture)->GetDesc(&texDescMotionVector);

		texDesc.Format = texDescMotionVector.Format;
		srvDesc.Format = texDesc.Format;
		rtvDesc.Format = texDesc.Format;
		uavDesc.Format = texDesc.Format;

		motionVectorBufferShared[index] = new Texture2D(texDesc);
		motionVectorBufferShared[index]->CreateSRV(srvDesc);
		motionVectorBufferShared[index]->CreateRTV(rtvDesc);
		motionVectorBufferShared[index]->CreateUAV(uavDesc);

		auto dx12SwapChain = DX12SwapChain::GetSingleton();

		{
			IDXGIResource1* dxgiResource = nullptr;
			DX::ThrowIfFailed(HUDLessBufferShared[index]->resource->QueryInterface(IID_PPV_ARGS(&dxgiResource)));

			if (dx12SwapChain->swapChain) {
				HANDLE sharedHandle = nullptr;
				DX::ThrowIfFailed(dxgiResource->CreateSharedHandle(
					nullptr,
					DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
					nullptr,
					&sharedHandle));

				DX::ThrowIfFailed(dx12SwapChain->d3d12Device->OpenSharedHandle(
					sharedHandle,
					IID_PPV_ARGS(&HUDLessBufferShared12[index])));

				CloseHandle(sharedHandle);
			}
		}

		{
			IDXGIResource1* dxgiResource = nullptr;
			DX::ThrowIfFailed(depthBufferShared[index]->resource->QueryInterface(IID_PPV_ARGS(&dxgiResource)));

			if (dx12SwapChain->swapChain) {
				HANDLE sharedHandle = nullptr;
				DX::ThrowIfFailed(dxgiResource->CreateSharedHandle(
					nullptr,
					DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
					nullptr,
					&sharedHandle));

				DX::ThrowIfFailed(dx12SwapChain->d3d12Device->OpenSharedHandle(
					sharedHandle,
					IID_PPV_ARGS(&depthBufferShared12[index])));

				CloseHandle(sharedHandle);
			}
		}

		{
			IDXGIResource1* dxgiResource = nullptr;
			DX::ThrowIfFailed(motionVectorBufferShared[index]->resource->QueryInterface(IID_PPV_ARGS(&dxgiResource)));

			if (dx12SwapChain->swapChain) {
				HANDLE sharedHandle = nullptr;
				DX::ThrowIfFailed(dxgiResource->CreateSharedHandle(
					nullptr,
					DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
					nullptr,
					&sharedHandle));

				DX::ThrowIfFailed(dx12SwapChain->d3d12Device->OpenSharedHandle(
					sharedHandle,
					IID_PPV_ARGS(&motionVectorBufferShared12[index])));

				CloseHandle(sharedHandle);
			}
		}
	}

	copyDepthToSharedBufferCS = (ID3D11ComputeShader*)ShaderUtils::CompileShader(L"Data\\F4SE\\Plugins\\FrameGeneration\\CopyDepthToSharedBufferCS.hlsl", {}, "cs_5_0");
	generateSharedBuffersCS = (ID3D11ComputeShader*)ShaderUtils::CompileShader(L"Data\\F4SE\\Plugins\\FrameGeneration\\GenerateSharedBuffersCS.hlsl", {}, "cs_5_0");
}

void Raytracing::PreAlpha()
{
	auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
	auto context = reinterpret_cast<ID3D11DeviceContext*>(rendererData->context);
	
	auto& colorMain = rendererData->renderTargets[(uint)RenderTarget::kMain];
	auto& colorPostAlpha = rendererData->renderTargets[(uint)RenderTarget::kMainTemp];

	context->CopyResource(reinterpret_cast<ID3D11Texture2D*>(colorMain.texture), reinterpret_cast<ID3D11Texture2D*>(colorPostAlpha.texture));
}

void Raytracing::PostAlpha()
{
	if (!d3d12Interop)
		return;

	if (!setupBuffers)
		CreateFrameGenerationResources();

	auto rendererData = RE::BSGraphics::RendererData::GetSingleton();

	auto context = reinterpret_cast<ID3D11DeviceContext*>(rendererData->context);
	auto dx12SwapChain = DX12SwapChain::GetSingleton();

	context->OMSetRenderTargets(0, nullptr, nullptr);

	{
		auto& colorPreAlpha = rendererData->renderTargets[(uint)RenderTarget::kMain];
		auto& colorPostAlpha = rendererData->renderTargets[(uint)RenderTarget::kMainTemp];

		auto& motionVector = rendererData->renderTargets[(uint)RenderTarget::kMotionVectors];
		auto& depth = rendererData->depthStencilTargets[(uint)DepthStencilTarget::kMain];

		{
			uint32_t dispatchX = (uint32_t)std::ceil(float(dx12SwapChain->swapChainDesc.Width) / 8.0f);
			uint32_t dispatchY = (uint32_t)std::ceil(float(dx12SwapChain->swapChainDesc.Height) / 8.0f);

			ID3D11ShaderResourceView* views[4] = { 
				reinterpret_cast<ID3D11ShaderResourceView*>(colorPreAlpha.srView),
				reinterpret_cast<ID3D11ShaderResourceView*>(colorPostAlpha.srView),
				reinterpret_cast<ID3D11ShaderResourceView*>(motionVector.srView),
				reinterpret_cast<ID3D11ShaderResourceView*>(depth.srViewDepth)
			};

			context->CSSetShaderResources(0, ARRAYSIZE(views), views);

			ID3D11UnorderedAccessView* uavs[2] = { motionVectorBufferShared[dx12SwapChain->frameIndex]->uav.get(), depthBufferShared[dx12SwapChain->frameIndex]->uav.get()};
			context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);

			context->CSSetShader(generateSharedBuffersCS, nullptr, 0);

			context->Dispatch(dispatchX, dispatchY, 1);
		}

		ID3D11ShaderResourceView* views[3] = { nullptr, nullptr, nullptr };
		context->CSSetShaderResources(0, ARRAYSIZE(views), views);

		ID3D11UnorderedAccessView* uavs[2] = { nullptr, nullptr };
		context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);

		ID3D11ComputeShader* shader = nullptr;
		context->CSSetShader(shader, nullptr, 0);
	}
}

void Raytracing::CopyBuffersToSharedResources()
{
	if (!d3d12Interop)
		return;

	if (!setupBuffers)
		CreateFrameGenerationResources();

	auto rendererData = RE::BSGraphics::RendererData::GetSingleton();

	auto context = reinterpret_cast<ID3D11DeviceContext*>(rendererData->context);
	auto dx12SwapChain = DX12SwapChain::GetSingleton();
	
	context->OMSetRenderTargets(0, nullptr, nullptr);

	auto& motionVector = rendererData->renderTargets[(uint)RenderTarget::kMotionVectors];
	context->CopyResource(motionVectorBufferShared[dx12SwapChain->frameIndex]->resource.get(), reinterpret_cast<ID3D11Texture2D*>(motionVector.texture));
		
	{
		auto& depth = rendererData->depthStencilTargets[(uint)DepthStencilTarget::kMain];

		{
			uint32_t dispatchX = (uint32_t)std::ceil(float(dx12SwapChain->swapChainDesc.Width) / 8.0f);
			uint32_t dispatchY = (uint32_t)std::ceil(float(dx12SwapChain->swapChainDesc.Height) / 8.0f);


			ID3D11ShaderResourceView* views[1] = { reinterpret_cast<ID3D11ShaderResourceView*>(depth.srViewDepth) };
			context->CSSetShaderResources(0, ARRAYSIZE(views), views);

			ID3D11UnorderedAccessView* uavs[1] = { depthBufferShared[dx12SwapChain->frameIndex]->uav.get() };
			context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);

			context->CSSetShader(copyDepthToSharedBufferCS, nullptr, 0);

			context->Dispatch(dispatchX, dispatchY, 1);
		}

		ID3D11ShaderResourceView* views[1] = { nullptr };
		context->CSSetShaderResources(0, ARRAYSIZE(views), views);

		ID3D11UnorderedAccessView* uavs[1] = { nullptr };
		context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);

		ID3D11ComputeShader* shader = nullptr;
		context->CSSetShader(shader, nullptr, 0);
	}	
}

void Raytracing::TimerSleepQPC(int64_t targetQPC)
{
	LARGE_INTEGER currentQPC;
	do {
		QueryPerformanceCounter(&currentQPC);
	} while (currentQPC.QuadPart < targetQPC);
}

void Raytracing::FrameLimiter(bool a_useFrameGeneration)
{
	static LARGE_INTEGER lastFrame = {};

	if (d3d12Interop && settings.frameLimitMode) {

		// Stick within VRR bounds
		double bestRefreshRate = refreshRate - (refreshRate * refreshRate) / 3600.0;

		LARGE_INTEGER qpf;
		QueryPerformanceFrequency(&qpf);

		int64_t targetFrameTicks = int64_t(double(qpf.QuadPart) / (bestRefreshRate * (a_useFrameGeneration ? 0.5 : 1.0)));

		LARGE_INTEGER timeNow;
		QueryPerformanceCounter(&timeNow);
		int64_t delta = timeNow.QuadPart - lastFrame.QuadPart;
		if (delta < targetFrameTicks) {
			TimerSleepQPC(lastFrame.QuadPart + targetFrameTicks);
		}
	}

	QueryPerformanceCounter(&lastFrame);
}

void Raytracing::GameFrameLimiter()
{
	double bestRefreshRate = 60.0f;

	LARGE_INTEGER qpf;
	QueryPerformanceFrequency(&qpf);

	int64_t targetFrameTicks = int64_t(double(qpf.QuadPart) / bestRefreshRate);

	static LARGE_INTEGER lastFrame = {};
	LARGE_INTEGER timeNow;
	QueryPerformanceCounter(&timeNow);
	int64_t delta = timeNow.QuadPart - lastFrame.QuadPart;
	if (delta < targetFrameTicks) {
		TimerSleepQPC(lastFrame.QuadPart + targetFrameTicks);
	}
	QueryPerformanceCounter(&lastFrame);	
}

/*
* Copyright (c) 2022-2023 NVIDIA CORPORATION. All rights reserved
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

double Raytracing::GetRefreshRate(HWND a_window)
{
	HMONITOR monitor = MonitorFromWindow(a_window, MONITOR_DEFAULTTONEAREST);
	MONITORINFOEXW info;
	info.cbSize = sizeof(info);
	if (GetMonitorInfoW(monitor, &info) != 0) {
		// using the CCD get the associated path and display configuration
		UINT32 requiredPaths, requiredModes;
		if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &requiredPaths, &requiredModes) == ERROR_SUCCESS) {
			std::vector<DISPLAYCONFIG_PATH_INFO> paths(requiredPaths);
			std::vector<DISPLAYCONFIG_MODE_INFO> modes2(requiredModes);
			if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &requiredPaths, paths.data(), &requiredModes, modes2.data(), nullptr) == ERROR_SUCCESS) {
				// iterate through all the paths until find the exact source to match
				for (auto& p : paths) {
					DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName;
					sourceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
					sourceName.header.size = sizeof(sourceName);
					sourceName.header.adapterId = p.sourceInfo.adapterId;
					sourceName.header.id = p.sourceInfo.id;
					if (DisplayConfigGetDeviceInfo(&sourceName.header) == ERROR_SUCCESS) {
						// find the matched device which is associated with current device
						// there may be the possibility that display may be duplicated and windows may be one of them in such scenario
						// there may be two callback because source is same target will be different
						// as window is on both the display so either selecting either one is ok
						if (wcscmp(info.szDevice, sourceName.viewGdiDeviceName) == 0) {
							// get the refresh rate
							UINT numerator = p.targetInfo.refreshRate.Numerator;
							UINT denominator = p.targetInfo.refreshRate.Denominator;
							return (double)numerator / (double)denominator;
						}
					}
				}
			}
		}
	}
	logger::error("Failed to retrieve refresh rate from swap chain");
	return 60;
}

void Raytracing::PostDisplay()
{
	if (!d3d12Interop)
		return;

	if (!setupBuffers)
		CreateFrameGenerationResources();
	
	auto rendererData = RE::BSGraphics::RendererData::GetSingleton();

	auto& swapChain = rendererData->renderTargets[(uint)RenderTarget::kFrameBuffer];
	ID3D11Resource* swapChainResource;
	reinterpret_cast<ID3D11RenderTargetView*>(swapChain.rtView)->GetResource(&swapChainResource);
	
	auto dx12SwapChain = DX12SwapChain::GetSingleton();

	reinterpret_cast<ID3D11DeviceContext*>(rendererData->context)->CopyResource(HUDLessBufferShared[dx12SwapChain->frameIndex]->resource.get(), swapChainResource);
}

void Raytracing::PostRenderSetup()
{
	static auto state = RE::BSGraphics::State::GetSingleton();
	static auto renderTargetManager = RE::BSGraphics::RenderTargetManager::GetSingleton();

	auto screenSize = float2(float(state.screenWidth), float(state.screenHeight));
	auto renderSize = float2(screenSize.x * renderTargetManager.dynamicWidthRatio, screenSize.y * renderTargetManager.dynamicHeightRatio);

	float2 jitter;
	jitter.x = -state.offsetX * screenSize.x / 2.0f;
	jitter.y = state.offsetY * screenSize.y / 2.0f;

	creationEngineRaytracing->UpdateJitter(jitter / float2(renderTargetManager.dynamicWidthRatio, renderTargetManager.dynamicHeightRatio));

	creationEngineRaytracing->UpdateCamera();
}

void Raytracing::PreRender()
{
	auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
	auto context = reinterpret_cast<ID3D11DeviceContext*>(rendererData->context);

	// SkyCubeToHemi
	{
		context->CSSetShader(cubeToHemiCS.get(), nullptr, 0);

		auto& reflections = rendererData->cubeMapRenderTargets[0];
		ID3D11ShaderResourceView* srvs[] = {
			reinterpret_cast<ID3D11ShaderResourceView*>(reflections.srView)
		};

		context->CSSetShaderResources(0, ARRAYSIZE(srvs), srvs);

		auto sampler = samplerState.get();
		context->CSSetSamplers(0, 1, &sampler);

		ID3D11UnorderedAccessView* uav = skyHemisphere->uav;
		context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);

		uint dispatch = (uint)std::ceil(skyHemiSize / 8.0f);
		context->Dispatch(dispatch, dispatch, 1);

		uav = nullptr;
		context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
	}
}

void Raytracing::PostRender()
{
	if (ga && settings.captureHotkey != 0) {
		bool isKeyDown = (GetAsyncKeyState(settings.captureHotkey) & 0x8000) != 0;
		if (isKeyDown && !wasCaptureHotkeyDown) {
			capturing = true;
		}
		wasCaptureHotkeyDown = isKeyDown;
	}

	if (capturing) {
		if (captureFrame == 0) {
			logger::info("[Raytracing] Starting PIX capture in PostOpaque...");
			ga->BeginCapture();
		}

		captureFrame++;
	}

	// Wait for D3D11 to finish
	DX::ThrowIfFailed(d3d11Context->Signal(d3d11Fence.get(), ++currentFenceValue));
	DX::ThrowIfFailed(commandQueue->Wait(d3d12Fence.get(), currentFenceValue));

	creationEngineRaytracing->Execute();
	currentFrame = creationEngineRaytracing->PostExecution();

	// Wait for D3D12 to finish
	DX::ThrowIfFailed(commandQueue->Signal(d3d12Fence.get(), ++currentFenceValue));
	DX::ThrowIfFailed(d3d11Context->Wait(d3d11Fence.get(), currentFenceValue));

	if (captureFrame > 1) {
		ga->EndCapture();
		logger::info("[Raytracing] Ended PIX capture in PostOpaque.");
		captureFrame = 0;
		capturing = false;
	}
}

void Raytracing::Reset()
{
	if (!d3d12Interop)
		return;

	if (!setupBuffers)
		CreateFrameGenerationResources();

	auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
	auto context = reinterpret_cast<ID3D11DeviceContext*>(rendererData->context);

	auto dx12SwapChain = DX12SwapChain::GetSingleton();

	FLOAT clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	context->ClearRenderTargetView(HUDLessBufferShared[dx12SwapChain->frameIndex]->rtv.get(), clearColor);
	context->ClearRenderTargetView(depthBufferShared[dx12SwapChain->frameIndex]->rtv.get(), clearColor);
	context->ClearRenderTargetView(motionVectorBufferShared[dx12SwapChain->frameIndex]->rtv.get(), clearColor);
}