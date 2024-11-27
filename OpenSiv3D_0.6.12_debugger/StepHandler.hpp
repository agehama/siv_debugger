#pragma once
#include <Windows.h>
#include <Siv3D.hpp>
#include "SymbolExplorer.hpp"

class StepHandler
{
public:

	void initializeSingleStepHelper();

	void saveCurrentLineInfo(HANDLE process, HANDLE thread);

	bool isLineChanged(HANDLE process, HANDLE thread);

	const LineInfo& lastLineInfo()
	{
		return m_lastCheckedLineInfo;
	}

private:

	HashTable<HANDLE, LineInfo> m_lastLineInfoMap; // スレッドID -> 行情報

	LineInfo m_lastCheckedLineInfo;
};
