// used: [d3d] api
#include <d3d11.h>

#include "interfaces.h"

// used: findpattern, callvirtual, getvfunc...
#include "../utilities/memory.h"

// used: l_print
#include "../utilities/log.h"

// used: iswapchaindx11
#include "../sdk/interfaces/iswapchaindx11.h"
#include "hooks.h"
#include "../sdk/datatypes/resourceutils.h"
#include "../cstrike/sdk/interfaces/events.h"
#include "../sdk/interfaces/imaterialsystem.h"
#pragma region interfaces_get

using InstantiateInterfaceFn_t = void* (*)();

class CInterfaceRegister
{
public:
	InstantiateInterfaceFn_t fnCreate;
	const char* szName;
	CInterfaceRegister* pNext;
};

static const CInterfaceRegister* GetRegisterList(const wchar_t* wszModuleName)
{
	void* hModule = MEM::GetModuleBaseHandle(wszModuleName);
	if (hModule == nullptr)
		return nullptr;

	std::uint8_t* pCreateInterface = reinterpret_cast<std::uint8_t*>(MEM::GetExportAddress(hModule, CS_XOR("CreateInterface")));

	if (pCreateInterface == nullptr)
	{
		L_PRINT(LOG_ERROR) << CS_XOR("failed to get \"CreateInterface\" address");
		return nullptr;
	}

	return *reinterpret_cast<CInterfaceRegister**>(MEM::ResolveRelativeAddress(pCreateInterface, 0x3, 0x7));
}

template <typename T = void*>
T* Capture(const CInterfaceRegister* pModuleRegister, const char* szInterfaceName)
{
	for (const CInterfaceRegister* pRegister = pModuleRegister; pRegister != nullptr; pRegister = pRegister->pNext)
	{
		if (const std::size_t nInterfaceNameLength = CRT::StringLength(szInterfaceName);
			// found needed interface
			CRT::StringCompareN(szInterfaceName, pRegister->szName, nInterfaceNameLength) == 0 &&
			// and we've given full name with hardcoded digits
			(CRT::StringLength(pRegister->szName) == nInterfaceNameLength ||
			// or it contains digits after name
			CRT::StringToInteger<int>(pRegister->szName + nInterfaceNameLength, nullptr, 10) > 0))
		{
			// capture our interface
			void* pInterface = pRegister->fnCreate();

#ifdef _DEBUG
			// log interface address
			L_PRINT(LOG_INFO) << CS_XOR("captured \"") << pRegister->szName << CS_XOR("\" interface at address: ") << L::AddFlags(LOG_MODE_INT_SHOWBASE | LOG_MODE_INT_FORMAT_HEX) << reinterpret_cast<std::uintptr_t>(pInterface);
#else
			L_PRINT(LOG_INFO) << CS_XOR("captured \"") << pRegister->szName << CS_XOR("\" interface");
#endif

			return static_cast<T*>(pInterface);
		}
	}

	L_PRINT(LOG_ERROR) << CS_XOR("failed to find interface \"") << szInterfaceName << CS_XOR("\"");
	return nullptr;
}

#pragma endregion

bool I::Setup()
{
	bool bSuccess = true;

#pragma region interface_game_exported
	const auto pTier0Handle = MEM::GetModuleBaseHandle(TIER0_DLL);
	if (pTier0Handle == nullptr)
		return false;

	MemAlloc = *reinterpret_cast<IMemAlloc**>(MEM::GetExportAddress(pTier0Handle, CS_XOR("g_pMemAlloc")));
	bSuccess &= (MemAlloc != nullptr);

	const auto pSchemaSystemRegisterList = GetRegisterList(SCHEMASYSTEM_DLL);
	if (pSchemaSystemRegisterList == nullptr)
		return false;

	SchemaSystem = Capture<ISchemaSystem>(pSchemaSystemRegisterList, SCHEMA_SYSTEM);
	bSuccess &= (SchemaSystem != nullptr);

	const auto pInputSystemRegisterList = GetRegisterList(INPUTSYSTEM_DLL);
	if (pInputSystemRegisterList == nullptr)
		return false;

	InputSystem = Capture<IInputSystem>(pInputSystemRegisterList, INPUT_SYSTEM_VERSION);
	bSuccess &= (InputSystem != nullptr);

	const auto pEngineRegisterList = GetRegisterList(ENGINE2_DLL);
	if (pEngineRegisterList == nullptr)
		return false;

	GameResourceService = Capture<IGameResourceService>(pEngineRegisterList, GAME_RESOURCE_SERVICE_CLIENT);
	bSuccess &= (GameResourceService != nullptr);

	Engine = Capture<IEngineClient>(pEngineRegisterList, SOURCE2_ENGINE_TO_CLIENT);
	bSuccess &= (Engine != nullptr);

	NetworkClientService = Capture<INetworkClientService>(pEngineRegisterList, NETWORK_CLIENT_SERVICE);
	bSuccess &= (NetworkClientService != nullptr);

	const auto pTier0RegisterList = GetRegisterList(TIER0_DLL);
	if (pTier0RegisterList == nullptr)
		return false;
	Cvar = Capture<IEngineCVar>(pTier0RegisterList, ENGINE_CVAR);
	bSuccess &= (Cvar != nullptr);

	const auto pClientRegister = GetRegisterList(CLIENT_DLL);
	if (pClientRegister == nullptr)
		return false;
	Client = Capture<ISource2Client>(pClientRegister, SOURCE2_CLIENT);
	bSuccess &= (Client != nullptr);

	const auto pLocalizeRegisterList = GetRegisterList(LOCALIZE_DLL);
	if (pLocalizeRegisterList == nullptr)
		return false;
	Localize = Capture<CLocalize>(pLocalizeRegisterList, LOCALIZE);
	bSuccess &= (Localize != nullptr);

	/* material sys */
	const auto pMaterialSystem2Register = GetRegisterList(MATERIAL_SYSTEM2_DLL);
	if (pMaterialSystem2Register == nullptr)
		return false;
	MaterialSystem2 = Capture<material_system_t>(pMaterialSystem2Register, MATERIAL_SYSTEM2);
	bSuccess &= (MaterialSystem2 != nullptr);
	
	const auto pResourceSystemRegisterList = GetRegisterList(RESOURCESYSTEM_DLL);
	if (pResourceSystemRegisterList == nullptr)
		return false;

	ResourceSystem = Capture<IResourceSystem>(pResourceSystemRegisterList, RESOURCE_SYSTEM);
	bSuccess &= (ResourceSystem != nullptr);

	if (ResourceSystem != nullptr)
	{
		ResourceHandleUtils = reinterpret_cast<CResourceHandleUtils*>(ResourceSystem->QueryInterface(RESOURCE_HANDLE_UTILS));
		bSuccess &= (ResourceHandleUtils != nullptr);
	}
	/* //render game sys 
	const auto pRenderSysRegister = GetRegisterList(RENDERSYSTEM_DLL);
	if (pRenderSysRegister == nullptr)
		return false;

	RenderGameSystem = Capture<CRenderGameSystem>(pRenderSysRegister, RENDERSYS_SYSTEM);
	bSuccess &= (RenderGameSystem != nullptr);*/
#pragma endregion

	Trace = *reinterpret_cast<i_trace**>(MEM::GetAbsoluteAddress(MEM::FindPattern(CLIENT_DLL, CS_XOR("4C 8B 3D ? ? ? ? 24 C9 0C 49 66 0F 7F 45 ?")), 0x3));
	bSuccess &= (Trace != nullptr);
	L_PRINT(LOG_INFO) << CS_XOR("captured Trace \"") << CS_XOR("\" interface at address: ") << L::AddFlags(LOG_MODE_INT_SHOWBASE | LOG_MODE_INT_FORMAT_HEX) << reinterpret_cast<std::uintptr_t>(Trace);

	GameEvent = *reinterpret_cast<IGameEventManager2**>(MEM::ResolveRelativeAddress(MEM::GetVFunc<std::uint8_t*>(Client, 14U) + 0x3E, 0x3, 0x7));
	bSuccess &= (GameEvent != nullptr);
	
	// @ida:  #STR: "r_gpu_mem_stats", "-threads", "CTSListBase: Misaligned list\n", "CTSQueue: Misaligned queue\n", "Display GPU memory usage.", "-r_max_device_threads"
	SwapChain = **reinterpret_cast<ISwapChainDx11***>(MEM::ResolveRelativeAddress(MEM::FindPattern(RENDERSYSTEM_DLL, CS_XOR("66 0F 7F 05 ? ? ? ? 66 0F 7F 0D ? ? ? ? 48 89 35")), 0x4, 0x8));
	bSuccess &= (SwapChain != nullptr);

	// grab's d3d11 interfaces for later use
	if (SwapChain != nullptr)
	{ 
		if (FAILED(SwapChain->pDXGISwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&Device)))
		{
			L_PRINT(LOG_ERROR) << CS_XOR("failed to get device from swapchain");
			CS_ASSERT(false);
			return false;
		}
		else
			// we successfully got device, so we can get immediate context
			Device->GetImmediateContext(&DeviceContext);
	}
	bSuccess &= (Device != nullptr && DeviceContext != nullptr);

	Input = *reinterpret_cast<CCSGOInput**>(MEM::ResolveRelativeAddress(MEM::FindPattern(CLIENT_DLL, CS_XOR("48 8B 0D ? ? ? ? E8 ? ? ? ? 8B BE ? ? ? ? 44 8B F0 85 FF 78 04 FF C7 EB 03")), 0x3, 0x7));
	bSuccess &= (Input != nullptr);

	// @ida: STR '%s:  %f tick(%d) curtime(%f) OnSequenceCycleChanged: %s : %d=[%s]'
	GlobalVars = *reinterpret_cast<IGlobalVars**>(MEM::ResolveRelativeAddress(MEM::FindPattern(CLIENT_DLL, CS_XOR("48 89 0D ? ? ? ? 48 89 41")), 0x3, 0x7));
	bSuccess &= (GlobalVars != nullptr);

	return bSuccess;
}

bool I::CreateRenderTarget(IDXGISwapChain* pSwapChain)  {
	SwapChain->pDXGISwapChain = pSwapChain;

	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	if (FAILED(SwapChain->pDXGISwapChain->GetDesc(&swapChainDesc))) {
	L_PRINT(LOG_ERROR) << (CS_XOR("Failed to get swap chain description."));
		return false;
	}

	if (FAILED(SwapChain->pDXGISwapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<PVOID*>(&Device)))) {
		L_PRINT(LOG_ERROR) << (CS_XOR("Failed to get device from swap chain."));
		return false;
	}

	Device->GetImmediateContext(&DeviceContext);

	ID3D11Texture2D* back_buffer;
	if (FAILED(SwapChain->pDXGISwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<PVOID*>(&back_buffer)))) {
		L_PRINT(LOG_ERROR) << (CS_XOR("Failed to get buffer from swap chain."));
		return false;
	}

	D3D11_RENDER_TARGET_VIEW_DESC desc;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;

	if (FAILED(Device->CreateRenderTargetView(back_buffer, &desc, &RenderTargetView))) {
		back_buffer->Release();
		L_PRINT(LOG_ERROR) << (CS_XOR("Failed to create render target view."));
		return false;
	}
	back_buffer->Release();

	return true;
}
void I::DestroyRenderTarget()
{
	if (RenderTargetView != nullptr)
	{
		RenderTargetView->Release();
		RenderTargetView = nullptr;
	}
}
