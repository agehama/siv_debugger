#include "SymbolExplorer.hpp"
#include <DbgHelp.h>
#include <Siv3D.hpp>

bool SymbolExplorer::init(const CREATE_PROCESS_DEBUG_INFO* pInfo)
{
	m_process = pInfo->hProcess;

	if (SymInitialize(m_process, NULL, FALSE))
	{
		DWORD64 moduleAddress = SymLoadModule64(
			m_process,
			pInfo->hFile,
			NULL,
			NULL,
			(DWORD64)pInfo->lpBaseOfImage,
			0
		);

		if (moduleAddress != 0)
		{
			return true;
		}
		else
		{
			Console << U"SymLoadModule64 failed: " << GetLastError();
			return false;
		}
	}
	else
	{
		Console << U"SymInitialize failed: " << GetLastError();
		return false;
	}
}

Optional<size_t> SymbolExplorer::findAddress(const String& symbolName) const
{
	SYMBOL_INFO symbolInfo = {};
	symbolInfo.SizeOfStruct = sizeof(SYMBOL_INFO);
	if (SymFromName(m_process, Unicode::ToUTF8(symbolName).c_str(), &symbolInfo))
	{
		return std::bit_cast<size_t>(symbolInfo.Address);
	}
	return none;
}

void SymbolExplorer::onDllLoaded(const LOAD_DLL_DEBUG_INFO* pInfo)
{
	DWORD64 moduleAddress = SymLoadModule64(
		m_process,
		pInfo->hFile,
		NULL,
		NULL,
		(DWORD64)pInfo->lpBaseOfDll,
		0
	);

	if (moduleAddress == 0)
	{
		Console << U"SymLoadModule64 failed: " << GetLastError();
	}

	CloseHandle(pInfo->hFile);
}

void SymbolExplorer::onDllUnloaded(const UNLOAD_DLL_DEBUG_INFO* pInfo)
{
	SymUnloadModule64(m_process, (DWORD64)pInfo->lpBaseOfDll);
}

void SymbolExplorer::dispose()
{
	SymCleanup(m_process);
	m_process = NULL;
}
