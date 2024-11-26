#include "ProcessDebugger.hpp"
#include "RegisterHandler.hpp"

bool ProcessDebugger::startDebugSession(const FilePathView exeFilePath)
{
	if (m_processStatus != ProcessStatus::None)
	{
		Console << U"デバッグが実行中です";
		return false;
	}

	STARTUPINFO si = {};
	si.cb = sizeof(si);

	PROCESS_INFORMATION pi = {};

	if (not CreateProcess(
		Unicode::ToWstring(exeFilePath).c_str(),
		NULL,
		NULL,
		NULL,
		FALSE,
		DEBUG_ONLY_THIS_PROCESS | CREATE_NEW_CONSOLE | CREATE_SUSPENDED,
		//DEBUG_ONLY_THIS_PROCESS | CREATE_NEW_CONSOLE,
		NULL,
		NULL,
		&si,
		&pi
	))
	{
		Console << U"CreateProcessに失敗しました: " << GetLastError();
		return false;
	}

	m_process = pi.hProcess;
	m_processID = pi.dwProcessId;
	m_mainThreadID = pi.dwThreadId;
	m_userMainThreadID = 0;
	m_threadIDMap[m_mainThreadID] = pi.hThread;
	m_processStatus = ProcessStatus::Interrupted;

	m_stoppedThreadID = m_mainThreadID;
	ResumeThread(pi.hThread);
	handledException(true);

	return true;
}

void ProcessDebugger::continueDebugSession()
{
	if (m_processStatus == ProcessStatus::None)
	{
		Console << U"プロセスを開始していません";
		return;
	}

	if (m_processStatus == ProcessStatus::Suspended)
	{
		ResumeThread(m_threadIDMap[m_stoppedThreadID.value()]);
	}
	else
	{
		ContinueDebugEvent(m_processID, m_stoppedThreadID.value(), m_alwaysContinue ? DBG_CONTINUE : m_continueStatus);
		m_alwaysContinue = false;
	}

	DEBUG_EVENT debugEvent;

	while (WaitForDebugEvent(&debugEvent, INFINITE))
	{
		if (dispatchDebugEvent(&debugEvent))
		{
			ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId, m_continueStatus);
		}
		else
		{
			break;
		}
	}
}

void ProcessDebugger::requestDebugBreak()
{
	m_requestBreak = true;

	SuspendThread(m_threadIDMap[m_userMainThreadID]);
	RegisterHandler::setTrapFlag(m_threadIDMap[m_userMainThreadID]);
	m_breakPointAttacher.setBeingSingleInstruction(true);
	ResumeThread(m_threadIDMap[m_userMainThreadID]);
}

void ProcessDebugger::suspend()
{
	SuspendThread(m_threadIDMap[m_userMainThreadID]);
	Print << U"SUSPENDED";
	m_stoppedThreadID = m_userMainThreadID;
}

void ProcessDebugger::resume()
{
	if (m_stoppedThreadID)
	{
		ResumeThread(m_threadIDMap[m_stoppedThreadID.value()]);
		m_stoppedThreadID = none;
	}
}

void ProcessDebugger::stepIn()
{
	if (m_processStatus == ProcessStatus::None)
	{
		Console << U"プロセスを開始していません";
		return;
	}

	//auto& currentThread = m_threadIDMap[m_stoppedThreadID.value()];
	auto& currentThread = m_threadIDMap[m_userMainThreadID];
	m_stepHandler.saveCurrentLineInfo(m_process, currentThread);

	RegisterHandler::setTrapFlag(currentThread);
	m_breakPointAttacher.setBeingSingleInstruction(true);

	//continueDebugSession();
}

void ProcessDebugger::stepOver()
{
	if (m_processStatus == ProcessStatus::None)
	{
		Console << U"プロセスを開始していません";
		return;
	}

	auto& currentThread = m_threadIDMap[m_userMainThreadID];

	auto contextOpt = RegisterHandler::getDebuggeeContext(currentThread);
	if (!contextOpt)
	{
		return;
	}

	const auto& context = contextOpt.value();

	m_stepHandler.saveCurrentLineInfo(m_process, currentThread);

	m_breakPointAttacher.setBeingStepOver(true);

	// CALL命令→CALLの実行後にブレーク
	if (auto callLenhOpt = m_symbolExplorer.tryGetCallInstructionBytesLength(context.Rip))
	{
		m_breakPointAttacher.setStepOverBreakPointAt(m_process, context.Rip + callLenhOpt.value());
		m_breakPointAttacher.setBeingSingleInstruction(false);
	}
	// CALL以外→シングルステップ実行
	else
	{
		RegisterHandler::setTrapFlag(currentThread);
		m_breakPointAttacher.setBeingSingleInstruction(true);
	}
}

void ProcessDebugger::stepOut()
{
	auto& currentThread = m_threadIDMap[m_userMainThreadID];
	m_stepHandler.saveCurrentLineInfo(m_process, currentThread);

	m_breakPointAttacher.setBeingStepOut(true);
	m_breakPointAttacher.setBeingSingleInstruction(false);

	// 現在の関数のret命令にブレークポイントを張る
	if (auto retOpt = m_symbolExplorer.getRetInstructionAddress(currentThread))
	{
		m_breakPointAttacher.setStepOutBreakPointAt(m_process, retOpt.value());
	}
}

const String& ProcessDebugger::currentFilename()
{
	return m_stepHandler.lastLineInfo().fileName;
}

int ProcessDebugger::currentLine()
{
	return m_stepHandler.lastLineInfo().lineNumber;
}

bool ProcessDebugger::dispatchDebugEvent(const DEBUG_EVENT* debugEvent)
{
	m_stoppedThreadID = none;

	switch (debugEvent->dwDebugEventCode)
	{
	case CREATE_PROCESS_DEBUG_EVENT:
		Console << U"CREATE_PROCESS_DEBUG_EVENT";
		return onProcessCreated(&debugEvent->u.CreateProcessInfo);
	case CREATE_THREAD_DEBUG_EVENT:
		Console << U"CREATE_THREAD_DEBUG_EVENT";
		return onThreadCreated(&debugEvent->u.CreateThread, debugEvent->dwThreadId);
	case EXCEPTION_DEBUG_EVENT:
	{
		//Console << U"EXCEPTION_DEBUG_EVENT";
		auto isContinue = onException(&debugEvent->u.Exception, debugEvent->dwThreadId);
		if (not isContinue)
		{
			m_stoppedThreadID = debugEvent->dwThreadId;
		}
		return isContinue;
	}
	case EXIT_PROCESS_DEBUG_EVENT:
		// false
		Console << U"EXIT_PROCESS_DEBUG_EVENT";
		return onProcessExited(&debugEvent->u.ExitProcess);
	case EXIT_THREAD_DEBUG_EVENT:
		Console << U"EXIT_THREAD_DEBUG_EVENT";
		return onThreadExited(&debugEvent->u.ExitThread, debugEvent->dwThreadId);
	case LOAD_DLL_DEBUG_EVENT:
		Console << U"LOAD_DLL_DEBUG_EVENT";
		return onDllLoaded(&debugEvent->u.LoadDll);
	case OUTPUT_DEBUG_STRING_EVENT:
		Console << U"OUTPUT_DEBUG_STRING_EVENT";
		return onOutputDebugString(&debugEvent->u.DebugString);
	case RIP_EVENT:
		//false
		Console << U"RIP_EVENT";
		return onRipEvent(&debugEvent->u.RipInfo);
	case UNLOAD_DLL_DEBUG_EVENT:
		Console << U"UNLOAD_DLL_DEBUG_EVENT";
		return onDllUnloaded(&debugEvent->u.UnloadDll);
	default:
		Console << U"Unknown debug event";
		return true;
	}
}

bool ProcessDebugger::onProcessCreated(const CREATE_PROCESS_DEBUG_INFO* pInfo)
{
	m_breakPointAttacher.initializeBreakPointHelper();
	m_stepHandler.initializeSingleStepHelper();

	if (m_symbolExplorer.init(pInfo, m_process))
	{
		if (auto entryPointOpt = m_symbolExplorer.findAddress(U"Main"))
		{
			if (m_breakPointAttacher.setUserBreakPointAt(m_process, entryPointOpt.value()))
			{
				Console << U"set break point at entry function succeeded: ";
				//printHex(mainAddress, false);
				//std::cout;
			}
			else
			{
				Console << U"set break point at entry function failed";
			}
		}
		else
		{
			Console << U"cannot find entry function";
		}
	}
	else
	{
		Console << U"SymInitialize failed: " << GetLastError();
	}

	CloseHandle(pInfo->hFile);

	return true;
}


bool ProcessDebugger::onThreadCreated(const CREATE_THREAD_DEBUG_INFO* pInfo, DWORD threadID)
{
	m_threadIDMap[threadID] = pInfo->hThread;
	return true;
}

bool ProcessDebugger::onException(const EXCEPTION_DEBUG_INFO* pInfo, DWORD threadID)
{
	switch (pInfo->ExceptionRecord.ExceptionCode)
	{
	case EXCEPTION_BREAKPOINT:
		return onBreakPoint(pInfo, threadID);
	case EXCEPTION_SINGLE_STEP:
		return onSingleStep(pInfo, threadID);
	case EXCEPTION_ACCESS_VIOLATION: Console << U"EXCEPTION_ACCESS_VIOLATION\n"; break;
	case EXCEPTION_DATATYPE_MISALIGNMENT: Console << U"EXCEPTION_DATATYPE_MISALIGNMENT\n"; break;
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: Console << U"EXCEPTION_ARRAY_BOUNDS_EXCEEDED\n"; break;
	case EXCEPTION_FLT_DENORMAL_OPERAND: Console << U"EXCEPTION_FLT_DENORMAL_OPERAND\n"; break;
	case EXCEPTION_FLT_DIVIDE_BY_ZERO: Console << U"EXCEPTION_FLT_DIVIDE_BY_ZERO\n"; break;
	case EXCEPTION_FLT_INEXACT_RESULT: Console << U"EXCEPTION_FLT_INEXACT_RESULT\n"; break;
	case EXCEPTION_FLT_INVALID_OPERATION: Console << U"EXCEPTION_FLT_INVALID_OPERATION\n"; break;
	case EXCEPTION_FLT_OVERFLOW: Console << U"EXCEPTION_FLT_OVERFLOW\n"; break;
	case EXCEPTION_FLT_STACK_CHECK: Console << U"EXCEPTION_FLT_STACK_CHECK\n"; break;
	case EXCEPTION_FLT_UNDERFLOW: Console << U"EXCEPTION_FLT_UNDERFLOW\n"; break;
	case EXCEPTION_INT_DIVIDE_BY_ZERO: Console << U"EXCEPTION_INT_DIVIDE_BY_ZERO\n"; break;
	case EXCEPTION_INT_OVERFLOW: Console << U"EXCEPTION_INT_OVERFLOW\n"; break;
	case EXCEPTION_PRIV_INSTRUCTION: Console << U"EXCEPTION_PRIV_INSTRUCTION\n"; break;
	case EXCEPTION_IN_PAGE_ERROR: Console << U"EXCEPTION_IN_PAGE_ERROR\n"; break;
	case EXCEPTION_ILLEGAL_INSTRUCTION: Console << U"EXCEPTION_ILLEGAL_INSTRUCTION\n"; break;
	case EXCEPTION_NONCONTINUABLE_EXCEPTION: Console << U"EXCEPTION_NONCONTINUABLE_EXCEPTION\n"; break;
	case EXCEPTION_STACK_OVERFLOW: Console << U"EXCEPTION_STACK_OVERFLOW\n"; break;
	case EXCEPTION_INVALID_DISPOSITION: Console << U"EXCEPTION_INVALID_DISPOSITION\n"; break;
	case EXCEPTION_GUARD_PAGE: Console << U"EXCEPTION_GUARD_PAGE\n"; break;
	case EXCEPTION_INVALID_HANDLE: Console << U"EXCEPTION_INVALID_HANDLE\n"; break;
	default:
		break;
	}

	/*std::cout << "An exception was occured at ";
	printHex(std::bit_cast<size_t>(pInfo->ExceptionRecord.ExceptionAddress), false);
	std::cout << "." << std::endl << "Exception code: ";
	printHex(pInfo->ExceptionRecord.ExceptionCode, true);
	std::cout << std::endl;*/

	/*if (pInfo->dwFirstChance)
	{
		Console << U"First chance";
	}
	else
	{
		Console << U"Second chance";
	}*/

	m_processStatus = ProcessStatus::Interrupted;

	return false;
}

bool ProcessDebugger::onBreakPoint(const EXCEPTION_DEBUG_INFO* pInfo, DWORD threadID)
{
	Console << U"onBreakPoint thread: " << threadID;

	const auto breadAddress = std::bit_cast<size_t>(pInfo->ExceptionRecord.ExceptionAddress);
	const auto bpType = m_breakPointAttacher.getBreakPointType(breadAddress);

	switch (bpType)
	{
	case BreakPointType::Init:
		handledException(true);
		return true;

	case BreakPointType::Entry:
	{
		m_userMainThreadID = threadID;
		if (auto contextOpt = RegisterHandler::getDebuggeeContext(m_threadIDMap[threadID]))
		{
			m_symbolExplorer.entryFunc(contextOpt.value().Rip);
		}
		handledException(true);
		return true;
	}

	case BreakPointType::Code:
		return onNormalBreakPoint(pInfo, threadID);

	case BreakPointType::User:
		return onUserBreakPoint(pInfo, threadID);

	case BreakPointType::StepOver:
		m_breakPointAttacher.cancelStepOverBreakPoint(m_process);
		RegisterHandler::backDebuggeeRip(m_threadIDMap[threadID]);
		return handleSingleStep(threadID);

	case BreakPointType::StepOut:
		return onStepOutBreakPoint(pInfo, threadID);

	default: return true;
	}
}

bool ProcessDebugger::onNormalBreakPoint(const EXCEPTION_DEBUG_INFO*, DWORD)
{
	// DebugBreakProcess
	//if (m_requestBreak)
	//{
	//	// 新規スレッドで止まるのでメインスレッドでブレークし直す
	//	m_requestBreak = false;
	//	RegisterHandler::setTrapFlag(m_mainThread);
	//	//m_breakPointAttacher.setBeingSingleInstruction(true);
	//	return true;
	//}

	if (m_breakPointAttacher.isBeingSingleInstruction())
	{
		handledException(true);
		return true;
	}

	if (m_breakPointAttacher.isBeingStepOver())
	{
		m_breakPointAttacher.cancelStepOverBreakPoint(m_process);
		m_breakPointAttacher.setBeingStepOver(false);
	}

	if (m_breakPointAttacher.isBeingStepOut())
	{
		m_breakPointAttacher.cancelStepOutBreakPoint(m_process);
		m_breakPointAttacher.setBeingStepOut(false);
	}

	//Console << U"A break point occured at ";
	//printHex(std::bit_cast<size_t>(pInfo->ExceptionRecord.ExceptionAddress), false);
	//Console << U".";

	m_alwaysContinue = true;

	m_processStatus = ProcessStatus::Interrupted;
	return false;
}

bool ProcessDebugger::onUserBreakPoint(const EXCEPTION_DEBUG_INFO* pInfo, DWORD threadID)
{
	const auto breakAddress = std::bit_cast<size_t>(pInfo->ExceptionRecord.ExceptionAddress);
	// 命令を元に戻す
	if (m_breakPointAttacher.recoverUserBreakPoint(m_process, breakAddress))
	{
		// breakpoint実行後に元の命令が実行されるように1バイト引いて戻す
		RegisterHandler::backDebuggeeRip(m_threadIDMap[threadID]);

		// 次の命令で止めてブレークポイントを復元する
		RegisterHandler::setTrapFlag(m_threadIDMap[threadID]);

		//　再セット用にアドレスを持っておく
		m_breakPointAttacher.saveResetUserBreakPoint(breakAddress);

		return onNormalBreakPoint(pInfo, threadID);
	}

	//通らないはず
	assert(false);
	return false;

	//Console << U"A break point occured at ";
	//printHex(std::bit_cast<size_t>(pInfo->ExceptionRecord.ExceptionAddress), false);
	//Console << U".";

	//m_processStatus = ProcessStatus::Interrupted;
	//return false;
}

bool ProcessDebugger::onStepOutBreakPoint(const EXCEPTION_DEBUG_INFO*, DWORD threadID)
{
	m_breakPointAttacher.cancelStepOutBreakPoint(m_process);

	RegisterHandler::backDebuggeeRip(m_threadIDMap[threadID]);

	CONTEXT context = {};
	GetThreadContext(m_threadIDMap[threadID], &context);

	if (auto retLengthOpt = m_symbolExplorer.retInstructionLength(context.Rip))
	{
		size_t retAddress;
		size_t numOfBytes;
		auto pAddress = reinterpret_cast<LPVOID>(context.Rip);
		ReadProcessMemory(m_process, pAddress, &retAddress, sizeof(size_t), &numOfBytes);

		m_breakPointAttacher.setStepOutBreakPointAt(m_process, retAddress);

		handledException(true);

		return true;
	}

	m_breakPointAttacher.setBeingStepOut(false);

	m_alwaysContinue = true;
	m_processStatus = ProcessStatus::Interrupted;
	return false;
}

bool ProcessDebugger::onSingleStep(const EXCEPTION_DEBUG_INFO*, DWORD threadID)
{
	Console << U"onSingleStep thread: " << threadID;

	if (m_breakPointAttacher.needResetBreakPoint())
	{
		m_breakPointAttacher.resetUserBreakPoint(m_process);
	}

	if (m_breakPointAttacher.isBeingSingleInstruction())
	{
		return handleSingleStep(threadID);
	}

	handledException(true);
	return true;
}

bool ProcessDebugger::handleSingleStep(DWORD threadID)
{
	auto& currentThread = m_threadIDMap[threadID];
	if (not m_stepHandler.isLineChanged(m_process, currentThread))
	{
		if (m_breakPointAttacher.isBeingStepOver())
		{
			if (auto contextOpt = RegisterHandler::getDebuggeeContext(currentThread))
			{
				auto context = contextOpt.value();

				// 関数呼び出しだったら終了後にStepOverブレークポイントを張る
				if (auto callLenOpt = m_symbolExplorer.tryGetCallInstructionBytesLength(context.Rip))
				{
					m_breakPointAttacher.setStepOverBreakPointAt(m_process, context.Rip + callLenOpt.value());
					m_breakPointAttacher.setBeingSingleInstruction(false);
				}
				else
				{
					RegisterHandler::setTrapFlag(currentThread);
					m_breakPointAttacher.setBeingSingleInstruction(true);
				}
			}
		}
		else
		{
			RegisterHandler::setTrapFlag(currentThread);
			m_breakPointAttacher.setBeingSingleInstruction(true);
		}

		handledException(true);
		return true;
	}

	if (m_breakPointAttacher.isBeingStepOver())
	{
		m_breakPointAttacher.setBeingStepOver(false);
	}

	m_alwaysContinue = true;
	m_processStatus = ProcessStatus::Interrupted;
	return false;
}

bool ProcessDebugger::onProcessExited(const EXIT_PROCESS_DEBUG_INFO* pInfo)
{
	Console << U"Debuggee was terminated";
	Console << U"Exit code: " << pInfo->dwExitCode;

	m_symbolExplorer.dispose();

	ContinueDebugEvent(m_processID, m_mainThreadID, DBG_CONTINUE);

	CloseHandle(m_threadIDMap[m_mainThreadID]);
	CloseHandle(m_process);

	m_process = NULL;
	m_processID = 0;
	m_mainThreadID = 0;
	m_userMainThreadID = 0;
	m_processStatus = ProcessStatus::None;
	m_continueStatus = DBG_EXCEPTION_NOT_HANDLED;
	m_alwaysContinue = false;
	m_requestBreak = false;

	m_threadIDMap.clear();
	m_stoppedThreadID = none;

	return false;
}

bool ProcessDebugger::onThreadExited(const EXIT_THREAD_DEBUG_INFO*, DWORD threadID)
{
	if (m_mainThreadID != threadID)
	{
		//auto threadHandle = m_threadIDMap[threadID];
		m_threadIDMap.erase(threadID);
		// クローズすると終了時に例外起きる
		//CloseHandle(threadHandle);
	}
	return true;
}

bool ProcessDebugger::onOutputDebugString(const OUTPUT_DEBUG_STRING_INFO* pInfo)
{
	BYTE* pBuffer = static_cast<BYTE*>(malloc(pInfo->nDebugStringLength));

	SIZE_T bytesRead;
	ReadProcessMemory(
		m_process,
		pInfo->lpDebugStringData,
		pBuffer,
		pInfo->nDebugStringLength,
		&bytesRead
	);

	std::string str(reinterpret_cast<char*>(pBuffer), bytesRead);

	Console << Unicode::FromUTF8(str);

	free(pBuffer);

	return true;
}

bool ProcessDebugger::onRipEvent(const RIP_INFO*)
{
	Console << U"A RIP_EVENT occured";
	m_processStatus = ProcessStatus::Interrupted;
	return false;
}

bool ProcessDebugger::onDllLoaded(const LOAD_DLL_DEBUG_INFO* pInfo)
{
	m_symbolExplorer.onDllLoaded(pInfo);
	return true;
}

bool ProcessDebugger::onDllUnloaded(const UNLOAD_DLL_DEBUG_INFO* pInfo)
{
	m_symbolExplorer.onDllUnloaded(pInfo);
	return true;
}
