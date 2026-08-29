#pragma once

#include "Buffer.h"

#include "SimpleIni.h"

#include "CreationEngineRaytracing.h"
#include "WrappedResource.h"

#include <ngx/nvsdk_ngx_defs_dlssd.h>

// Microsoft PIX
#pragma push_macro("NTDDI_VERSION")
#undef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WINBLUE
#include <DXProgrammableCapture.h>
#pragma pop_macro("NTDDI_VERSION")

#include "renderdoc_app.h"

#include "RE/Bethesda/Events.h"
#include "RE/Bethesda/PlayerCharacter.h"
#include "RE/TESWaterReflections.h"
#include "RE/TESWaterSystem.h"

enum class RenderTarget : uint32_t
{
	kFrameBuffer = 0,
	kRefractionNormal = 1,
	kMainPreAlpha = 2,
	kMain = 3,
	kMainTemp = 4,

	kSSRRaw = 7,
	kSSRBlurred = 8,
	kSSRBlurredExtra = 9,
	kSSRDirection = 10,
	kSSRMask = 11,

	kMainVerticalBlur = 14,
	kMainHorizontalBlur = 15,

	kUI = 17,
	kUITemp = 18,

	kGbufferNormal = 20,
	kGbufferNormalSwap = 21,
	kGbufferAlbedo = 22,
	kGbufferEmissive = 23,
	kGbufferMaterial = 24, // Glossiness, Specular, Backlighting, SSS

	kTAAAccumulation = 26,
	kTAAAccumulationSwap = 27,
	kSSAO = 28,
	kMotionVectors = 29,

	kUIDownscaled = 36,
	kUIDownscaledComposite = 37,

	kMainDepthMips = 39,

	kSSAOTemp = 48,
	kSSAOTemp2 = 49,
	kSSAOTemp3 = 50,

	kUnkMask = 57,
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

enum class DepthStencilTarget : uint32_t
{
	kMainOtherOther = 0,
	kMainOther = 1,
	kMain = 2,
	kMainCopy = 3,
	kMainCopyCopy = 4,

	kShadowMap = 8,

	kCount = 13
};

class Raytracing
{
public:
	~Raytracing();

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

	winrt::com_ptr<ID3D11Fence> d3d11Fence;
	winrt::com_ptr<ID3D12Fence> d3d12Fence;
	uint64_t currentFenceValue = 0;
	uint64_t dlssCommandFenceValue = 0;
	HANDLE fenceEvent = nullptr;

	winrt::com_ptr<IDXGraphicsAnalysis> ga = nullptr;

	Texture2D* HUDLessBufferShared[2];
	Texture2D* depthBufferShared[2];
	Texture2D* motionVectorBufferShared[2];
	
	winrt::com_ptr<ID3D12Resource> HUDLessBufferShared12[2];
	winrt::com_ptr<ID3D12Resource> depthBufferShared12[2];
	winrt::com_ptr<ID3D12Resource> motionVectorBufferShared12[2];

	ID3D11ComputeShader* copyDepthToSharedBufferCS;
	ID3D11ComputeShader* generateSharedBuffersCS;

	winrt::com_ptr<ID3D12CommandAllocator> dlssCommandAllocator = nullptr;
	winrt::com_ptr<ID3D12GraphicsCommandList> dlssCommandList = nullptr;

	bool setupBuffers = false;

	std::unique_ptr<CreationEngineRaytracing> creationEngineRaytracing = nullptr;

	bool initialized = false;
	bool dlssFeatureCreated = false;
	bool dlssHistoryReset = true;
	bool dlssLastEvaluationSucceeded = false;
	uint32_t dlssOutputFrame = 0;

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

	RE::NiPointer<RE::TESWaterReflections> waterReflections = nullptr;

	std::unique_ptr<WrappedResource> waterFlowMap = nullptr;

	// Available when Pathtracing
	std::array<std::unique_ptr<WrappedResource>, CreationEngineRaytracing::MAX_FRAMES_IN_FLIGHT> depthTexture;
	std::array<std::unique_ptr<WrappedResource>, CreationEngineRaytracing::MAX_FRAMES_IN_FLIGHT> motionVectorsTexture;

	// Available for both GI and PT
	std::array<std::unique_ptr<WrappedResource>, CreationEngineRaytracing::MAX_FRAMES_IN_FLIGHT> mainTexture;
	std::array<std::unique_ptr<WrappedResource>, CreationEngineRaytracing::MAX_FRAMES_IN_FLIGHT> diffuseAlbedoTexture;

	winrt::com_ptr<ID3D11ComputeShader> copyPTMainCS = nullptr;
	winrt::com_ptr<ID3D11UnorderedAccessView> mainTempUAV = nullptr;

	winrt::com_ptr<ID3D12Resource> albedoTexture = nullptr;
	std::unique_ptr<WrappedResource> normalRoughnessTexture = nullptr;
	winrt::com_ptr<ID3D12Resource> gnmaoTexture = nullptr;

	float4x4 worldToViewMatrix;
	float4x4 viewToClipMatrix;

	struct Settings {
		bool enabled = true;
		bool frameGenerationMode = 1;
		bool frameLimitMode = 1;
		uint32_t captureHotkey = VK_F11;
		bool enableDebug = 0;
		bool enableD3D12Debug = 0;
		bool enablePIX = 0;
		bool enableRenderDoc = 0;
		NVSDK_NGX_RayReconstruction_Hint_Render_Preset dlssRRPreset = NVSDK_NGX_RayReconstruction_Hint_Render_Preset_D;
		CreationEngineRaytracing::Settings cert;
	} settings;

	bool wasCaptureHotkeyDown = false;
	bool capturingPix = false;
	uint32_t pixCaptureFrame = 0;

	RENDERDOC_API_1_4_0* rdocAPI = nullptr;
	bool capturingRdoc = false;
	uint32_t rdocCaptureFrame = 0;

	void LoadSettings();

	void PostPostLoad();

	void GameLoaded();

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
	void InitializeRenderDoc();
	void CreateD3D12Device(IDXGIAdapter* a_adapter, ID3D11Device* a_d3d11Device, ID3D11DeviceContext* a_d3d11Context);

	void InitializeCERaytracing(ID3D11Device5* d3d11Device, ID3D12Device5* d3d12Device, ID3D12CommandQueue* commandQueue, ID3D12CommandQueue* computeCommandQueue, ID3D12CommandQueue* copyCommandQueue);

	void PostDisplay();

	void PreRenderSetup();
	void PostRenderSetup();

	void PreRender();
	void PostRender();
	void WaitForDLSSCommandList();
	void EvaluateDLSSRR();

	void Reset();

	class BGSActorCellEventHandler : public RE::BSTEventSink<RE::BGSActorCellEvent>
	{
	public:
		virtual RE::BSEventNotifyControl ProcessEvent(const RE::BGSActorCellEvent& a_event, RE::BSTEventSource<RE::BGSActorCellEvent>*) override
		{
			if (a_event.flags.underlying() != static_cast<uint32_t>(RE::BGSActorCellEvent::CellFlag::kEnter))
				return RE::BSEventNotifyControl::kContinue;

			auto* tesWaterSystem = RE::TESWaterSystem::GetSingleton();

			if (tesWaterSystem) {
				auto& waterRefl = Raytracing::GetSingleton()->waterReflections;

				bool attach = true;

				// Fallout 4 seems to always have an TESWaterReflection
				if (!tesWaterSystem->waterReflections.empty()) {
					for (auto& item: tesWaterSystem->waterReflections)
					{
						if (item == waterRefl) {
							attach = false;
							break;
						}
					}
				}

				if (attach)
					tesWaterSystem->waterReflections.push_back(waterRefl);

				tesWaterSystem->Enable();
			}

			return RE::BSEventNotifyControl::kContinue;
		}

		static bool Register()
		{
			static BGSActorCellEventHandler singleton;

			auto* player = RE::PlayerCharacter::GetSingleton();
			static_cast<RE::BSTEventSource<RE::BGSActorCellEvent>*>(player)->RegisterSink(&singleton);
			logger::info("Registered {}", typeid(singleton).name());

			return true;
		}
	};

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

		struct DrawWorld_UpdateWater
		{
			static void thunk()
			{
				auto* tes = RE::TES::GetSingleton();
				if (tes->interiorCell) {
					if (tes->interiorCell->cellFlags.none(RE::TESObjectCELL::Flag::kHasWater))
						tes->interiorCell->cellFlags.set(true, RE::TESObjectCELL::Flag::kHasWater);
				}

				auto rt = Raytracing::GetSingleton();
				rt->waterReflections->flags.set(true, RE::TESWaterReflections::Flags::kDirty);

				func();
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct DrawWorld_MainRenderSetup
		{
			static void thunk()
			{			
				auto rt = Raytracing::GetSingleton();
				rt->PreRenderSetup();
				func();
				rt->PostRenderSetup();
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
			stl::detour_thunk<DrawWorld_UpdateWater>(REL::ID(2318288));
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
