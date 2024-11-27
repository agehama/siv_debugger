#include <Windows.h>
#include <DbgHelp.h>
#include "StepHandler.hpp"
#include "ProcessHandle.hpp"
#include "ThreadHandle.hpp"

void StepHandler::initializeSingleStepHelper()
{
	m_lastCheckedLineInfo = {};
	m_lastLineInfoMap.clear();
}

void StepHandler::saveCurrentLineInfo(const ProcessHandle& process, const ThreadHandle& thread)
{
	if (auto opt = process.getCurrentLineInfo(thread))
	{
		m_lastLineInfoMap[thread.getHandle()] = opt.value();
	}
	else
	{
		m_lastLineInfoMap[thread.getHandle()].fileName = U"";
		m_lastLineInfoMap[thread.getHandle()].lineNumber = 0;
	}
}

bool StepHandler::isLineChanged(const ProcessHandle& process, const ThreadHandle& thread)
{
	auto lineInfoOpt = process.getCurrentLineInfo(thread);

	// 行情報の取得に失敗したときは同じ行とみなす
	if (not lineInfoOpt.has_value())
	{
		return false;
	}

	const auto& current = lineInfoOpt.value();
	m_lastCheckedLineInfo = current;

	if (current.lineNumber == m_lastLineInfoMap[thread.getHandle()].lineNumber && current.fileName == m_lastLineInfoMap[thread.getHandle()].fileName)
	{
		return false;
	}

	return true;
}

