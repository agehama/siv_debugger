#pragma once
#include <Windows.h>

class SymbolExplorer
{
public:

	bool init(const CREATE_PROCESS_DEBUG_INFO* pInfo);

	Optional<size_t> findAddress(const String& symbolName) const;

	void onDllLoaded(const LOAD_DLL_DEBUG_INFO* pInfo);
	void onDllUnloaded(const UNLOAD_DLL_DEBUG_INFO* pInfo);

	void dispose();

private:

	HANDLE m_process;
};
