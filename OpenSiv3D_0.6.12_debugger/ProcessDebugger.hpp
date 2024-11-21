#pragma once
#include <Windows.h>
#include <Siv3D.hpp>
#include "BreakPointAttacher.hpp"
#include "SymbolExplorer.hpp"

enum class ProcessStatus
{
	None, Suspended, Interrupted
};

class ProcessDebugger
{
public:

	bool startDebugSession(const FilePathView exeFilePath);

	void continueDebugSession();

	operator bool() const
	{
		return m_process != NULL;
	}

	void suspend();
	void resume();

private:
	bool dispatchDebugEvent(const DEBUG_EVENT* debugEvent);

	bool onProcessCreated(const CREATE_PROCESS_DEBUG_INFO*);
	bool onThreadCreated(const CREATE_THREAD_DEBUG_INFO*, DWORD threadID);

	bool onException(const EXCEPTION_DEBUG_INFO*, DWORD threadID);
	bool onBreakPoint(const EXCEPTION_DEBUG_INFO* pInfo, DWORD threadID);
	bool onSingleStep(const EXCEPTION_DEBUG_INFO* pInfo);

	bool onProcessExited(const EXIT_PROCESS_DEBUG_INFO*);
	bool onThreadExited(const EXIT_THREAD_DEBUG_INFO*, DWORD threadID);
	bool onOutputDebugString(const OUTPUT_DEBUG_STRING_INFO*);
	bool onRipEvent(const RIP_INFO*);
	bool onDllLoaded(const LOAD_DLL_DEBUG_INFO*);
	bool onDllUnloaded(const UNLOAD_DLL_DEBUG_INFO*);

	HashTable<DWORD, HANDLE> m_threadIDMap;
	HANDLE m_process = NULL;
	HANDLE m_mainThread = NULL;
	DWORD m_processID = 0;
	DWORD m_mainThreadID = 0;
	Optional<DWORD> m_stoppedThreadID;
	ProcessStatus m_processStatus = ProcessStatus::None;

	BreakPointAttacher m_breakPointAttacher;
	SymbolExplorer m_symbolExplorer;
};
