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

	const LINE_INFO& lastLineInfo()
	{
		return m_lastCheckedLineInfo;
	}

private:

	HashTable<HANDLE, LINE_INFO> m_lastLineInfoMap; // スレッドID -> 行情報

	LINE_INFO m_lastCheckedLineInfo;
};
