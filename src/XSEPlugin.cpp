
#include <d3d11.h>

void InitializeLog()
{
#ifndef NDEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		stl::report_and_fail("Failed to find standard logging directory"sv);
	}

	*path /= std::format("{}.log"sv, Plugin::NAME);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

#ifndef NDEBUG
	const auto level = spdlog::level::trace;
#else
	const auto level = spdlog::level::info;
#endif

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
	log->set_level(level);
	log->flush_on(spdlog::level::info);

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("%v"s);
}

#if defined(FALLOUT_POST_NG)
extern "C" DLLEXPORT constinit auto F4SEPlugin_Version = []() noexcept {
	F4SE::PluginVersionData data{};

	data.PluginVersion(Plugin::VERSION);
	data.PluginName(Plugin::NAME.data());
	data.AuthorName("");
	data.UsesAddressLibrary(true);
	data.UsesSigScanning(false);
	data.IsLayoutDependent(true);
	data.HasNoStructUse(false);
	data.CompatibleVersions({ F4SE::RUNTIME_LATEST });

	return data;
}();
#endif

decltype(&D3D11CreateDeviceAndSwapChain) ptrD3D11CreateDeviceAndSwapChain;

typedef HRESULT(WINAPI* D3D11CreateDeviceAndSwapChain_t)(
	IDXGIAdapter* pAdapter,
	D3D_DRIVER_TYPE DriverType,
	HMODULE Software,
	UINT Flags,
	const D3D_FEATURE_LEVEL* pFeatureLevels,
	UINT FeatureLevels,
	UINT SDKVersion,
	const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
	IDXGISwapChain** ppSwapChain,
	ID3D11Device** ppDevice,
	D3D_FEATURE_LEVEL* pFeatureLevel,
	ID3D11DeviceContext** ppImmediateContext);

// Store the original function
D3D11CreateDeviceAndSwapChain_t OriginalD3D11CreateDeviceAndSwapChain = nullptr;

// Hooked function
HRESULT WINAPI HookedD3D11CreateDeviceAndSwapChain(
	IDXGIAdapter* pAdapter,
	D3D_DRIVER_TYPE DriverType,
	HMODULE Software,
	UINT Flags,
	const D3D_FEATURE_LEVEL* pFeatureLevels,
	UINT FeatureLevels,
	UINT SDKVersion,
	const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
	IDXGISwapChain** ppSwapChain,
	ID3D11Device** ppDevice,
	D3D_FEATURE_LEVEL* pFeatureLevel,
	ID3D11DeviceContext** ppImmediateContext)
{
	HRESULT hr = OriginalD3D11CreateDeviceAndSwapChain(
		pAdapter,
		DriverType,
		Software,
		Flags,
		pFeatureLevels,
		FeatureLevels,
		SDKVersion,
		pSwapChainDesc,
		ppSwapChain,
		ppDevice,
		pFeatureLevel,
		ppImmediateContext);

	return hr;
}

void AttachD3D11Hook()
{
	HMODULE hD3D11 = GetModuleHandleA("d3d11.dll");

	// Get the original function address
	OriginalD3D11CreateDeviceAndSwapChain = (D3D11CreateDeviceAndSwapChain_t)GetProcAddress(hD3D11, "D3D11CreateDeviceAndSwapChain");

	// Attach the detour
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)OriginalD3D11CreateDeviceAndSwapChain, HookedD3D11CreateDeviceAndSwapChain);
	DetourTransactionCommit();
}

uint32_t pixelShaderID = 0;

struct BSDFLightShaderMacros_GetPixelShaderID
{
	static uint32_t thunk(uint32_t rawShaderID)
	{
		auto ret = func(rawShaderID);
		pixelShaderID = ret;
		return ret;
	}
	static inline REL::Relocation<decltype(thunk)> func;
};

struct DeferredLight_SunShadow
{
	static void thunk(struct BSRenderPass* , unsigned int , char )
	{
		//func(a1, a2, a3);
	}
	static inline REL::Relocation<decltype(thunk)> func;
};

static void InstallHooks()
{
//	AttachD3D11Hook();

	uintptr_t base = (uintptr_t)GetModuleHandleA(nullptr);

//	stl::write_thunk_call<BSDFLightShaderMacros_GetPixelShaderID>(moduleBase + 0x28C06A0 + 0x1F);

	stl::write_thunk_call<DeferredLight_SunShadow>(base + 0x28529B0 + 0x159D);
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);

	InitializeLog();
	InstallHooks();

	return true;
}
