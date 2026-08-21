#pragma once

#include "Buffer.h"

#include "SimpleIni.h"

#include "CreationEngineRaytracing.h"
#include "WrappedResource.h"

// Microsoft PIX
#pragma push_macro("NTDDI_VERSION")
#undef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WINBLUE
#include <DXProgrammableCapture.h>
#pragma pop_macro("NTDDI_VERSION")

class Raytracing
{
public:
	static Raytracing* GetSingleton()
	{
		static Raytracing singleton;
		return &singleton;
	}

	bool highFPSPhysicsFixLoaded = false;

	bool d3d12Interop = false;
	double refreshRate = 0.0f;

	bool reticleFix = false;

	winrt::com_ptr<ID3D11Device5> d3d11Device;
	winrt::com_ptr<ID3D11DeviceContext4> d3d11Context;

	winrt::com_ptr<ID3D12Device5> d3d12Device;
	winrt::com_ptr<ID3D12CommandQueue> commandQueue;
	winrt::com_ptr<ID3D12CommandQueue> computeCommandQueue;
	winrt::com_ptr<ID3D12CommandQueue> copyCommandQueue;

	UINT64 currentFenceValue = 0;
	HANDLE fenceEvent = nullptr;

	winrt::com_ptr<ID3D11Fence> d3d11Fence;
	winrt::com_ptr<ID3D12Fence> d3d12Fence;

	winrt::com_ptr<IDXGraphicsAnalysis> ga = nullptr;

	Texture2D* HUDLessBufferShared[2];
	Texture2D* depthBufferShared[2];
	Texture2D* motionVectorBufferShared[2];
	
	winrt::com_ptr<ID3D12Resource> HUDLessBufferShared12[2];
	winrt::com_ptr<ID3D12Resource> depthBufferShared12[2];
	winrt::com_ptr<ID3D12Resource> motionVectorBufferShared12[2];

	ID3D11ComputeShader* copyDepthToSharedBufferCS;
	ID3D11ComputeShader* generateSharedBuffersCS;

	bool setupBuffers = false;

	std::unique_ptr<CreationEngineRaytracing> creationEngineRaytracing = nullptr;

	bool initialized = false;

	bool forcedDisabled = false;

	enum DisableReason
	{
		None,
		UnsupportedGPU,
		OutdatedDrivers,
		MissingPlugin,
		InitFailed,
	} disableReason = DisableReason::None;

	uint32_t currentFrame;

	winrt::com_ptr<ID3D11SamplerState> samplerState = nullptr;

	uint32_t skyHemiSize;

	std::unique_ptr<WrappedResource> skyHemisphere = nullptr;
	winrt::com_ptr<ID3D11ComputeShader> cubeToHemiCS = nullptr;

	winrt::com_ptr<ID3D12Resource> albedoTexture = nullptr;
	std::unique_ptr<WrappedResource> normalRoughnessTexture = nullptr;
	winrt::com_ptr<ID3D12Resource> gnmaoTexture = nullptr;

	std::unique_ptr<WrappedResource> waterFlowMap = nullptr;

	struct Settings {
		bool frameGenerationMode = 1;
		bool frameLimitMode = 1;
		uint32_t captureHotkey = VK_F11;
		CreationEngineRaytracing::Settings cert;
	} settings;

	bool wasCaptureHotkeyDown = false;
	bool capturing = false;
	uint32_t captureFrame;

	void LoadSettings();

	void PostPostLoad();

	void ShareTexture(ID3D11Texture2D* d3d11Texture, ID3D12Resource** d3d12Resource, bool nt = false, uint accessFlags = DXGI_SHARED_RESOURCE_READ) const;

	void SetupResources();

	void CompileShaders();

	void CreateFrameGenerationResources();
	void PreAlpha();
	void PostAlpha();
	void CopyBuffersToSharedResources();

	static void TimerSleepQPC(int64_t targetQPC);

	void FrameLimiter(bool a_useFrameGeneration);

	void GameFrameLimiter();

	static double GetRefreshRate(HWND a_window);

	void InitializePIX();
	void CreateD3D12Device(IDXGIAdapter* a_adapter, ID3D11Device* a_d3d11Device, ID3D11DeviceContext* a_d3d11Context);

	void InitializeCERaytracing(ID3D11Device5* d3d11Device, ID3D12Device5* d3d12Device, ID3D12CommandQueue* commandQueue, ID3D12CommandQueue* computeCommandQueue, ID3D12CommandQueue* copyCommandQueue);

	void PostDisplay();

	void PostRenderSetup();

	void PreRender();
	void PostRender();

	void Reset();

	struct Hooks
	{
		struct WindowSizeChanged
		{
			static void thunk(RE::BSGraphics::Renderer*, unsigned int)
			{
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct SetUseDynamicResolutionViewportAsDefaultViewport
		{
			static void thunk(RE::BSGraphics::RenderTargetManager* This, bool a_true)
			{
				func(This, a_true);
				if (!a_true)
					Raytracing::GetSingleton()->PostDisplay();
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct DrawWorld_MainRenderSetup
		{
			static void thunk()
			{
				func();
				Raytracing::GetSingleton()->PostRenderSetup();				
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct DrawWorld_NotifyDoneRendering
		{
			static void thunk()
			{
				auto rt = Raytracing::GetSingleton();
				rt->PreRender();
				func();
				rt->PostRender();
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct DrawWorld_Reticle
		{
			static void thunk(void* a1)
			{
				auto rt = Raytracing::GetSingleton();
				rt->PreAlpha();
				func(a1);
				rt->reticleFix = true;
				rt->PostAlpha();
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct BSShaderRenderTargets_Create
		{
			static void thunk(void* shaderRenderTargets)
			{
				func(shaderRenderTargets);
				Raytracing::GetSingleton()->SetupResources();
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};
		
		static void Install()
		{
#if defined(FALLOUT_POST_NG)
			stl::detour_thunk<BSShaderRenderTargets_Create>(REL::ID(2318909));

			//stl::detour_thunk<WindowSizeChanged>(REL::ID(2276824));
			//stl::write_thunk_call<SetUseDynamicResolutionViewportAsDefaultViewport>(REL::ID(2318322).address() + 0xC5);
			stl::detour_thunk<DrawWorld_MainRenderSetup>(REL::ID(2318298));
			stl::write_thunk_call<DrawWorld_NotifyDoneRendering>(REL::ID(2228969).address() + 0x11C);
			//stl::write_thunk_call<DrawWorld_Reticle>(REL::ID(2318315).address() + 0x53D);
#else
			// Fix game initialising twice
			/*stl::detour_thunk<WindowSizeChanged>(REL::ID(212827));

			// Watch frame presentation
			stl::write_thunk_call<SetUseDynamicResolutionViewportAsDefaultViewport>(REL::ID(587723).address() + 0xE1);

			// Fix reticles on motion vectors and depth
			stl::detour_thunk<DrawWorld_Forward>(REL::ID(656535));
			stl::write_thunk_call<DrawWorld_Reticle>(REL::ID(338205).address() + 0x253);*/
#endif

			logger::info("[Raytracing] Installed hooks");
		}
	};
};
