#pragma once
#include <Windows.h>
#include <Siv3D.hpp>
#include "ProcessHandle.hpp"

class ThreadHandle;
class ProcessHandle;

class StepHandler
{
public:

	void initializeSingleStepHelper();

	void saveCurrentLineInfo(const ProcessHandle& process, const ThreadHandle& thread);

	bool isLineChanged(const ProcessHandle& process, const ThreadHandle& thread);

	const LineInfo& lastLineInfo()
	{
		return m_lastCheckedLineInfo;
	}

private:

	HashTable<HANDLE, LineInfo> m_lastLineInfoMap; // スレッドID -> 行情報

	LineInfo m_lastCheckedLineInfo;
};
