#pragma once
#include <Windows.h>

struct LineInfo
{
	String fileName;
	size_t lineNumber;
};

class ThreadHandle;

struct VariableInfo
{
	size_t address;
	size_t modBase;
	DWORD size;
	DWORD typeID;
	String name;
};

class ProcessHandle
{
public:

	ProcessHandle() = default;

	ProcessHandle(HANDLE process) :m_processHandle(process) {}

	void reset();

	bool init(const CREATE_PROCESS_DEBUG_INFO* pInfo);

	void entryFunc(size_t address);

	void dispose();

	void onDllLoaded(const LOAD_DLL_DEBUG_INFO* pInfo) const;

	void onDllUnloaded(const UNLOAD_DLL_DEBUG_INFO* pInfo) const;

	Optional<size_t> findAddress(const String& symbolName) const;

	Optional<size_t> tryGetCallInstructionBytesLength(size_t address) const;

	Optional<size_t> getRetInstructionAddress(const ThreadHandle& thread) const;

	Optional<size_t> retInstructionLength(size_t address) const;

	Optional<LineInfo> getCurrentLineInfo(const ThreadHandle& thread) const;

	HANDLE getHandle() const { return m_processHandle; }

	bool readMemory(size_t address, size_t size, LPVOID lpBuffer) const;

	template<typename T>
	bool readMemory(size_t address, T& buffer) const
	{
		return readMemory(address, sizeof(T), &buffer);
	}

	bool writeMemory(size_t address, size_t size, LPCVOID lpBuffer) const;

	template<typename T>
	bool writeMemory(size_t address, const T& buffer) const
	{
		return writeMemory(address, sizeof(T), &buffer);
	}

	void fetchGlobalVariables();

	void fetchLocalVariables(const ThreadHandle& thread);

	const String& getDebugString() const
	{
		return m_debugString;
	}

private:

	HANDLE m_processHandle = NULL;
	Array<VariableInfo> m_userGlobalVariables;
	String m_debugString;
};
