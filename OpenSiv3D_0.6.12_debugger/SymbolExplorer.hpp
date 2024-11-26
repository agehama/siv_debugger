#pragma once
#include <Windows.h>

struct LINE_INFO
{
	String fileName;
	size_t lineNumber;
};

class SymbolExplorer
{
public:

	bool init(const CREATE_PROCESS_DEBUG_INFO* pInfo, HANDLE process);

	void entryFunc(size_t address);

	void dispose();

	Optional<size_t> findAddress(const String& symbolName) const;

	void onDllLoaded(const LOAD_DLL_DEBUG_INFO* pInfo) const;
	void onDllUnloaded(const UNLOAD_DLL_DEBUG_INFO* pInfo) const;

	Optional<size_t> tryGetCallInstructionBytesLength(size_t address) const;

	Optional<size_t> getRetInstructionAddress(HANDLE thread) const;

	Optional<size_t> retInstructionLength(size_t address) const;

	static Optional<LINE_INFO> GetCurrentLineInfo(HANDLE process, HANDLE thread);

	HANDLE getProcess() { return m_process; }

private:

	HANDLE m_process;
};
