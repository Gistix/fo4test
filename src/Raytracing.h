#pragma once

#include "Buffer.h"

#include "SimpleIni.h"

class Raytracing
{
public:
	static Raytracing* GetSingleton()
	{
		static Raytracing singleton;
		return &singleton;
	}

	struct Settings
	{
		bool frameGenerationMode = 1;
		bool frameLimitMode = 1;
	};

	Settings settings;

	bool highFPSPhysicsFixLoaded = false;

	bool d3d12Interop = false;
	double refreshRate = 0.0f;

	bool reticleFix = false;

	Texture2D* HUDLessBufferShared[2];
	Texture2D* depthBufferShared[2];
	Texture2D* motionVectorBufferShared[2];
	
	winrt::com_ptr<ID3D12Resource> HUDLessBufferShared12[2];
	winrt::com_ptr<ID3D12Resource> depthBufferShared12[2];
	winrt::com_ptr<ID3D12Resource> motionVectorBufferShared12[2];

	ID3D11ComputeShader* copyDepthToSharedBufferCS;
	ID3D11ComputeShader* generateSharedBuffersCS;

	bool setupBuffers = false;

	void LoadSettings();

	void PostPostLoad();

	void CreateFrameGenerationResources();
	void PreAlpha();
	void PostAlpha();
	void CopyBuffersToSharedResources();

	static void TimerSleepQPC(int64_t targetQPC);

	void FrameLimiter(bool a_useFrameGeneration);

	void GameFrameLimiter();

	static double GetRefreshRate(HWND a_window);

	void PostDisplay();

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

		struct DrawWorld_Forward
		{
			static void thunk(void* a1)
			{
				func(a1);

				auto rt = Raytracing::GetSingleton();

				if (!rt->reticleFix)
					rt->CopyBuffersToSharedResources();

				rt->reticleFix = false;
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

		struct TES_AttachModel
		{
			static void thunk(RE::TES* oThis, RE::TESObjectREFR* refr, RE::TESObjectCELL* cell, void* queuedTree, bool a5, RE::NiAVObject* a6)
			{
				func(oThis, refr, cell, queuedTree, a5, a6);

				auto* baseObject = refr->GetObjectReference();

				//auto flags = baseObject->GetFormFlags();

				auto type = baseObject->GetFormType();

				auto logName = [&](const char* name) {
					logger::info("[RT] TES::AttachModel - {} - {}", magic_enum::enum_name(type), name);
				};

				if (auto* model = baseObject->As<RE::TESModel>()) {
					logName(model->GetModel());
				}
				else {
					if (auto* actor = refr->As<RE::Actor>()) {
						logName(actor->GetName());
					}
				}

			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		static void Install()
		{
#if defined(FALLOUT_POST_NG)
			stl::detour_thunk<WindowSizeChanged>(REL::ID(2276824));
			stl::write_thunk_call<SetUseDynamicResolutionViewportAsDefaultViewport>(REL::ID(2318322).address() + 0xC5);
			stl::detour_thunk<DrawWorld_Forward>(REL::ID(2318315));
			stl::write_thunk_call<DrawWorld_Reticle>(REL::ID(2318315).address() + 0x53D);

			stl::detour_thunk<TES_AttachModel>(REL::ID(2192085));
#else
			// Fix game initialising twice
			stl::detour_thunk<WindowSizeChanged>(REL::ID(212827));

			// Watch frame presentation
			stl::write_thunk_call<SetUseDynamicResolutionViewportAsDefaultViewport>(REL::ID(587723).address() + 0xE1);

			// Fix reticles on motion vectors and depth
			stl::detour_thunk<DrawWorld_Forward>(REL::ID(656535));
			stl::write_thunk_call<DrawWorld_Reticle>(REL::ID(338205).address() + 0x253);
#endif

			logger::info("[Raytracing] Installed hooks");
		}
	};
};
