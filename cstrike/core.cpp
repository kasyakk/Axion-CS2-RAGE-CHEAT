// used: [win] shgetknownfolderpath
#include <shlobj_core.h>

#include "core.h"

// used: features setup
#include "features.h"
// used: string copy
#include "utilities/crt.h"
// used: mem
#include "utilities/memory.h"
// used: l_print
#include "utilities/log.h"
// used: inputsystem setup/restore
#include "utilities/inputsystem.h"
// used: draw destroy
#include "utilities/draw.h"

// used: interfaces setup/destroy
#include "core/interfaces.h"
// used: sdk setup
#include "core/sdk.h"
// used: config setup & variables
#include "core/variables.h"
// used: hooks setup/destroy
#include "core/hooks.h"
// used: schema setup/dump
#include "core/schema.h"
// used: convar setup
#include "core/convars.h"
// used: menu
#include "core/menu.h"
#include <d3d11.h>
bool CORE::GetWorkingPath(wchar_t* wszDestination)
{
	bool bSuccess = false;
	PWSTR wszPathToDocuments = nullptr;

	// get path to user documents
	if (SUCCEEDED(::SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_CREATE, nullptr, &wszPathToDocuments)))
	{
		CRT::StringCat(CRT::StringCopy(wszDestination, wszPathToDocuments), CS_XOR(L"\\.cs2\\"));
		bSuccess = true;
		// create directory if it doesn't exist
		if (!CreateDirectoryW(wszDestination, nullptr))
		{
			if (::GetLastError() != ERROR_ALREADY_EXISTS)
			{
				L_PRINT(LOG_ERROR) << CS_XOR("failed to create default working directory, because one or more intermediate directories don't exist");
				bSuccess = false;
			}
		}
	}
	::CoTaskMemFree(wszPathToDocuments);

	return bSuccess;
}

static bool Setup(HMODULE hModule)
{
#ifdef CS_LOG_CONSOLE
	if (!L::AttachConsole(CS_XOR(L"cs2 developer-mode")))
	{
		CS_ASSERT(false); // failed to attach console
		return false;
	}
#endif
#ifdef CS_LOG_FILE
	if (!L::OpenFile(CS_XOR(L"cs2.log")))
	{
		CS_ASSERT(false); // failed to open file
		return false;
	}
#endif
	L_PRINT(LOG_NONE) << L::SetColor(LOG_COLOR_FORE_GREEN | LOG_COLOR_FORE_INTENSITY) << CS_XOR("logging system initialization completed");

	// setup game's exported functions
	if (!MEM::Setup())
	{
		CS_ASSERT(false); // failed to setup memory system
		return false;
	}
	L_PRINT(LOG_NONE) << L::SetColor(LOG_COLOR_FORE_GREEN | LOG_COLOR_FORE_INTENSITY) << CS_XOR("memory system initialization completed");

	if (!MATH::Setup())
	{
		CS_ASSERT(false); // failed to setup math system
		return false;
	}
	L_PRINT(LOG_NONE) << L::SetColor(LOG_COLOR_FORE_GREEN | LOG_COLOR_FORE_INTENSITY) << CS_XOR("math system initialization completed");
	// grab game's interfaces
	if (!I::Setup())
	{
		CS_ASSERT(false); // failed to setup interfaces
		return false;
	}
	L_PRINT(LOG_NONE) << L::SetColor(LOG_COLOR_FORE_GREEN | LOG_COLOR_FORE_INTENSITY) << CS_XOR("interfaces initialization completed");

	if (!SDK::Setup())
	{
		CS_ASSERT(false); // failed to setup sdk
		return false;
	}
	L_PRINT(LOG_NONE) << L::SetColor(LOG_COLOR_FORE_GREEN | LOG_COLOR_FORE_INTENSITY) << CS_XOR("sdk initialization completed");

	// setup input system and replace game's window messages processor with our
	if (!IPT::Setup())
	{
		CS_ASSERT(false); // failed to setup input system
		return false;
	}
	L_PRINT(LOG_NONE) << L::SetColor(LOG_COLOR_FORE_GREEN | LOG_COLOR_FORE_INTENSITY) << CS_XOR("input system initialization completed");

	L_PRINT(LOG_NONE) << L::SetColor(LOG_COLOR_FORE_GREEN | LOG_COLOR_FORE_INTENSITY) << CS_XOR("renderer backend initialization completed");

	L_PRINT(LOG_NONE) << L::SetColor(LOG_COLOR_FORE_GREEN | LOG_COLOR_FORE_INTENSITY) << CS_XOR("menu style Iinitialization completed");

	// initialize feature-related stuff
	if (!F::Setup())
	{
		CS_ASSERT(false); // failed to setup features
		return false;
	}
	L_PRINT(LOG_NONE) << L::SetColor(LOG_COLOR_FORE_GREEN | LOG_COLOR_FORE_INTENSITY) << CS_XOR("features initialization completed");

	if (!SCHEMA::Setup(CS_XOR(L"schema.txt")))
	{
		CS_ASSERT(false); // failed to setup schema system
		return false;
	}
	L_PRINT(LOG_NONE) << L::SetColor(LOG_COLOR_FORE_GREEN | LOG_COLOR_FORE_INTENSITY) << CS_XOR("schema system initialization completed");

	if (!CONVAR::Dump(CS_XOR(L"convars.txt")))
	{
		CS_ASSERT(false); // failed to setup convars system
		return false;
	}
	L_PRINT(LOG_NONE) << L::SetColor(LOG_COLOR_FORE_GREEN | LOG_COLOR_FORE_INTENSITY) << CS_XOR("convars dumped completed, output: \"convars.txt\"");

	if (!CONVAR::Setup())
	{
		CS_ASSERT(false); // failed to setup convars system
		return false;
	}
	L_PRINT(LOG_NONE) << L::SetColor(LOG_COLOR_FORE_GREEN | LOG_COLOR_FORE_INTENSITY) << CS_XOR("convars system initialization completed");

	// setup hooks
	if (!H::Setup())
	{
		CS_ASSERT(false); // failed to setup hooks
		return false;
	}

	L_PRINT(LOG_NONE) << CS_XOR("hooks initialization completed");


	L_PRINT(LOG_NONE) << CS_XOR("menu style initialization completed");

	// setup values to save/load cheat variables into/from files and load default configuration
	if (!C::Setup(CS_XOR(CS_CONFIGURATION_DEFAULT_FILE_NAME)))
		// this error is not critical, only show that
		L_PRINT(LOG_WARNING) << CS_XOR("failed to setup and/or load default configuration");
	else
		L_PRINT(LOG_NONE) << L::SetColor(LOG_COLOR_FORE_GREEN | LOG_COLOR_FORE_INTENSITY) << CS_XOR("configuration system initialization completed");

	L_PRINT(LOG_NONE) << L::SetColor(LOG_COLOR_FORE_CYAN | LOG_COLOR_FORE_INTENSITY) << CS_XOR("cs2 initialization completed, version: ") << CS_STRINGIFY(CS_VERSION);
	
	return true;
}

// @todo: some of those may crash while closing process, because we dont have any dependencies from the game modules, it means them can be unloaded and destruct interfaces etc before our module | modify ldrlist?
static void Destroy()
{
	// restore window messages processor to original
	IPT::Destroy();

	// restore hooks
	H::Destroy();

	// destroy renderer backend
	D::Destroy();

#ifdef CS_LOG_CONSOLE
	L::DetachConsole();
#endif
#ifdef CS_LOG_FILE
	L::CloseFile();
#endif
}

DWORD WINAPI PanicThread(LPVOID lpParameter)
{
	// don't let proceed unload until user press specified key
	while (!IPT::IsKeyReleased(C_GET(unsigned int, Vars.nPanicKey)))
		::Sleep(500UL);

	// call detach code and exit this thread
	FreeLibraryAndExitThread(static_cast<HMODULE>(lpParameter), EXIT_SUCCESS);
}

extern "C" BOOL WINAPI _CRT_INIT(HMODULE hModule, DWORD dwReason, LPVOID lpReserved);

BOOL APIENTRY CoreEntryPoint(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
	// process destroy of the cheat before crt calls atexit table
	if (dwReason == DLL_PROCESS_DETACH)
		Destroy();

	if (dwReason == DLL_PROCESS_ATTACH)
	{	// dispatch reason for c-runtime, initialize/destroy static variables, TLS etc
		if (!_CRT_INIT(hModule, dwReason, lpReserved))
			return FALSE;

		CORE::hProcess = MEM::GetModuleBaseHandle(nullptr);

		// basic process check
		if (CORE::hProcess == nullptr)
			return FALSE;

		/*
		 * check did all game modules have been loaded
		 * @note: navsystem.dll is the last loaded module
		 */
		if (MEM::GetModuleBaseHandle(NAVSYSTEM_DLL) == nullptr)
			return FALSE;

		// save our module handle
		CORE::hDll = hModule;

		// check did we perform main initialization successfully
		if (!Setup(hModule))
		{
			// undo the things we've done
			Destroy();
			return FALSE;
		}

		// create panic thread, it isn't critical error if it fails
		HANDLE hThread = CreateThread(nullptr, 0U, &PanicThread, hModule, 0UL, nullptr);
		
		if (hThread != nullptr)
			CloseHandle(hThread);

	}

	return TRUE;
}
