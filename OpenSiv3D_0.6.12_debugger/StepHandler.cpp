#include <Windows.h>
#include <DbgHelp.h>
#include "StepHandler.hpp"
#include "RegisterHandler.hpp"

void StepHandler::initializeSingleStepHelper()
{
	m_lastCheckedLineInfo = {};
	m_lastLineInfoMap.clear();
}

void StepHandler::saveCurrentLineInfo(HANDLE process, HANDLE thread)
{
	if (auto opt = SymbolExplorer::GetCurrentLineInfo(process, thread))
	{
		m_lastLineInfoMap[thread] = opt.value();
	}
	else
	{
		m_lastLineInfoMap[thread].fileName = U"";
		m_lastLineInfoMap[thread].lineNumber = 0;
	}
}

bool StepHandler::isLineChanged(HANDLE process, HANDLE thread)
{
	auto lineInfoOpt = SymbolExplorer::GetCurrentLineInfo(process, thread);

	// 行情報の取得に失敗したときは同じ行とみなす
	if (not lineInfoOpt.has_value())
	{
		return false;
	}

	const auto& current = lineInfoOpt.value();
	m_lastCheckedLineInfo = current;

	if (current.lineNumber == m_lastLineInfoMap[thread].lineNumber && current.fileName == m_lastLineInfoMap[thread].fileName)
	{
		return false;
	}

	return true;
}

