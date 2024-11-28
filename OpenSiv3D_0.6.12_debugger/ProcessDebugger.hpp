#pragma once
#include <Windows.h>
#include <Siv3D.hpp>
#include "BreakPointAttacher.hpp"
#include "StepHandler.hpp"
#include "ProcessHandle.hpp"
#include "ThreadHandle.hpp"

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
		return m_process.getHandle() != NULL;
	}

	void requestDebugBreak();
	void suspend();
	void resume();

	void stepIn();
	void stepOver();
	void stepOut();

	const ProcessHandle& process() const { return m_process; }
	ProcessHandle& process() { return m_process; }

	const ThreadHandle& userThread() const { return m_threadIDMap.at(m_userMainThreadID); }
	ThreadHandle& userThread() { return m_threadIDMap.at(m_userMainThreadID); }

	ProcessStatus status() { return m_processStatus; }

	const String& currentFilename();
	int currentLine();

	DWORD mainThreadID() const { return m_mainThreadID; }
	DWORD userThreadID() const { return m_userMainThreadID; }

private:
	bool dispatchDebugEvent(const DEBUG_EVENT* debugEvent);

	bool onProcessCreated(const CREATE_PROCESS_DEBUG_INFO*);
	bool onThreadCreated(const CREATE_THREAD_DEBUG_INFO*, DWORD threadID);

	// Exception: BreakPoint/SingleStep/Code
	bool onException(const EXCEPTION_DEBUG_INFO*, DWORD threadID);

	// BreakPoint: Init/Code/User/StepOver/StepOut
	bool onBreakPoint(const EXCEPTION_DEBUG_INFO* pInfo, DWORD threadID);

	bool onNormalBreakPoint(const EXCEPTION_DEBUG_INFO* pInfo, DWORD threadID);
	bool onUserBreakPoint(const EXCEPTION_DEBUG_INFO* pInfo, DWORD threadID);
	bool onStepOutBreakPoint(const EXCEPTION_DEBUG_INFO* pInfo, DWORD threadID);

	bool onSingleStep(const EXCEPTION_DEBUG_INFO* pInfo, DWORD threadID);
	bool handleSingleStep(DWORD threadID);

	bool onProcessExited(const EXIT_PROCESS_DEBUG_INFO*);
	bool onThreadExited(const EXIT_THREAD_DEBUG_INFO*, DWORD threadID);
	bool onOutputDebugString(const OUTPUT_DEBUG_STRING_INFO*);
	bool onRipEvent(const RIP_INFO*);
	bool onDllLoaded(const LOAD_DLL_DEBUG_INFO*);
	bool onDllUnloaded(const UNLOAD_DLL_DEBUG_INFO*);

	void handledException(bool handled)
	{
		m_continueStatus = handled ? DBG_CONTINUE : DBG_EXCEPTION_NOT_HANDLED;
	}

	HashTable<DWORD, ThreadHandle> m_threadIDMap;
	ProcessHandle m_process;
	DWORD m_processID = 0;
	DWORD m_mainThreadID = 0;
	DWORD m_userMainThreadID = 0;
	Optional<DWORD> m_stoppedThreadID;
	ProcessStatus m_processStatus = ProcessStatus::None;

	BreakPointAttacher m_breakPointAttacher;
	StepHandler m_stepHandler;

	bool m_alwaysContinue = false;
	DWORD m_continueStatus = DBG_EXCEPTION_NOT_HANDLED;

	bool m_requestBreak = false;
};
